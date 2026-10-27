#include "g2_natural_driver.h"
#include "full_content.h"
namespace {
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}need(rejected,"G3 invalid state accepted");}
void saveRisk(const CombatContent& c,const SaveSnapshot& saved){
 auto legal=saved;GameSession s(c.items,saved.session,c.facts);s.captureContinueCheckpoint(c.magazineSeedBindings,218,{9,7});
 for(bool won:{false,true}){s.setFlag(449,won);s.setFlag(450,won);s.setFlag(586,false);legal.session=s.exportState();validateSave(c,legal);auto raw=nlohmann::json::parse(encodeSave(c,legal));
  auto bad=legal;bad.session.worldFlags.at(c.facts.switches.at(586))=true;rejects([&]{validateSave(c,bad);});
  for(auto& row:raw.at("session").at("world_flags")){if(row.at(0)==c.facts.switches.at(586)){row.at(1)=true;}}
  rejects([&]{decodeSave(c,raw.dump());});
 }
 std::cout<<"G3 ordinary active checkpoint rejection and retained backup passed\n";
}
void conditionRisk(const CombatContent& c,const SessionEntry& base){
 // CN1.00 CE141 C9–16: state2 poison, then state3 bleeding. CE140 C39–46 restores one.
 for(int history:{0,2,3}){for(bool poison:{false,true}){for(bool bleed:{false,true}){
  auto e=base;e.commonEffectsKnown=true;e.conditions.poisoned=poison;e.conditions.bleedingActive=bleed;e.conditions.bleedingTicks=bleed?360000:0;
  GameSession s(c.items,e,c.facts);s.setVariable(272,history);s.captureContinueCheckpoint(c.magazineSeedBindings,218,{9,7});const int expected=bleed?3:poison?2:history;
  need(s.variable(272)==expected,"Continue condition capture contradicts original state IDs");auto retry=s.prepareContinueRestore(c.magazineSeedBindings,218,{9,7});
  need(retry.conditions().poisoned==(expected==2)&&retry.conditions().bleedingActive==(expected==3),"Continue condition restore contradicts original state IDs");
 }}}
 std::cout<<"G3 original poison/bleeding/history cross cases passed\n";
}
CombatEntry fixture(const CombatContent& c,const SessionEntry& state,int map=165){auto e=c.entries.at("post-pickup");e.map=map;e.playerTile={9,12};e.direction=Direction::Up;e.session=state;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();return e;}
void finishNatural(G2NaturalDriver& run){
 auto& game=run.game;const int stock=game.session().count(20),rounds=game.session().magazine(20);need(!game.session().flag(586),"Victory left checkpoint active");
 run.actObject("m164-e3");run.receptionBack();run.monitor(2);
 need(!game.session().flag(499)&&game.session().count(20)==stock,"Unclaimed reward changed inventory");
 run.reception();run.actObject("m159-e5");need(game.mapId()==164&&game.session().flag(450),"Victory reentry repeated boss");run.actObject("m164-e12");
 need(game.session().flag(499)&&game.session().count(20)==stock+1&&game.session().magazine(20)==rounds,"Reward failed or refilled existing magazine");
 run.actObject("m164-e12");need(game.session().count(20)==stock+1,"Reward duplicated on repeat");run.equip(20);run.tick(key(InputAction::Weapon));for(int n=0;n<100;++n){run.tick();}
 need(game.raised()&&rounds>0,"Natural magnum raise/ammo absent");const auto other=game.session().magazine(11);run.tick(key(InputAction::Attack));for(int n=0;n<150;++n){run.tick();}
 need(game.session().magazine(20)==rounds-1&&game.session().magazine(11)==other,"Natural magnum fire changed another magazine");
 const auto sound=game.content().cue("f3-ce75-c2").at("sound").at("resource").get<std::string>();need(std::find(run.sounds.begin(),run.sounds.end(),sound)!=run.sounds.end(),"Natural magnum source sound absent");
 run.lower();run.actObject("m164-e3");run.receptionBack();run.monitor(3);
 need(game.mapId()==151&&game.session().flag(449)&&game.session().flag(450)&&game.session().flag(499)&&!game.session().flag(586),"Claimed ordinary save lost G3 facts");
 std::cout<<"G3 unclaimed/claimed natural Save and actual I20 equip/fire ticks="<<run.ticks<<" HP="<<game.session().hp()<<" I20="<<game.session().magazine(20)<<'\n';
}
void loaded(G2NaturalDriver& run,const SaveSnapshot& saved){
 auto& game=run.game;const GameSession expected(game.content().items,saved.session,game.content().facts);need(game.mapId()==151,"Independent G3 Load missed ordinary point");
 for(int fact:{449,450,499,586}){need(game.session().flag(fact)==expected.flag(fact),"Independent G3 Load changed victory/reward/checkpoint");}
 need(game.session().hp()==expected.hp()&&game.session().equipment().mainWeapon==expected.equipped(1)&&game.session().exportState().magazines==saved.session.magazines&&game.session().exportState().itemUses==saved.session.itemUses,"Independent G3 Load changed typed inventory usage or magazine");
 need(game.session().variable(283)==218&&game.session().variable(630)==expected.variable(630),"Independent G3 Load lost retained Continue history");
 const int stock=game.session().count(20),rounds=game.session().magazine(20);run.reception();run.actObject("m159-e5");need(game.mapId()==164&&game.session().flag(450)&&game.foreground()==ForegroundKind::None,"Independent G3 Load replayed boss/aftermath");
 if(game.session().flag(499)){run.actObject("m164-e12");need(game.session().count(20)==stock&&game.session().magazine(20)==rounds,"Independent G3 Load duplicated/refilled reward");}
 run.actObject("m164-e3");run.receptionBack();run.monitor(2);std::cout<<"G3 independent ordinary Load/reentry/Save passed reward="<<expected.flag(499)<<" I20="<<rounds<<" HP="<<expected.hp()<<'\n';
}
void partVariation(const CombatContent& c,const SessionEntry& base){
 for(int branch=0;branch<3;++branch){auto e=fixture(c,base);GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,safe);
  for(int n=0;n<3000&&(n==0||scene.foreground()!=ForegroundKind::None||!s.flag(441));++n){scene.update(n%8==0?key():InputFrame{});scene.takePresentation();}
  need(s.flag(586)&&scene.foreground()==ForegroundKind::None,"Part variation intro absent");
  s.setVariable(83,160);s.setVariable(84,120);s.setVariable(97,160);s.setVariable(98,120);s.setVariable(89,0);s.setVariable(103,branch==0?799:10);s.setVariable(90,branch==1?2:0);s.setVariable(104,4);s.setVariable(105,branch==2?0:1);s.setVariable(35,0);s.setFlag(47,branch==1);
  std::set<std::string> ids;const auto hit=scene.resolveAttack(AttackSample{{160,120},{5,5},branch==2?AttackKind::GlFlame:AttackKind::Handgun,branch==2?6:3,0,false,300},ids);
  if(branch==0){need(hit.processed==1&&hit.damage>0&&s.variable(103)>=800,"Open body at799 rejected eligible damage");}
  if(branch==1){need(hit.processed==2&&ids.size()==2&&s.variable(35)==1&&!s.flag(47)&&!s.flag(12)&&s.variable(89)>s.variable(103),"Ordered overlapping receivers reused precision or wrong defense");auto before=s.exportState();need(scene.resolveAttack(AttackSample{{160,120},{5,5},AttackKind::Handgun,3,0,false,300},ids).processed==0&&s.exportState()==before,"One attack repeated a part");std::set<std::string> invalid;need(scene.resolveAttack(AttackSample{{160,120},{5,5},AttackKind::Handgun,7,0,false,301},invalid).processed==0&&invalid.empty(),"Unsupported receiver was processed");need(s.exportState()==before,"Rejected receiver altered part state");}
  if(branch==2){need(hit.processed==1&&hit.damage==0&&s.variable(35)==0&&s.flag(38),"Closed flame called CE11 or failed to ignite");for(int n=0;n<7;++n){scene.update({});scene.takePresentation();}need(s.variable(103)==14,"Closed flame lost common source +4 damage");}
 }
 std::cout<<"G3 open799/overlap/precision/dedup/invalid/closed common burn passed\n";
}
void rules(const CombatContent& c,const SessionEntry& base){
 for(int spare:{0,1,2,99}){
  auto e=base;e.inventory[13]=spare;e.mainWeapon=13;e.magazines[13]=3;e.itemUses[8]=2;e.inventory[8]=2;GameSession s(c.items,e,c.facts);s.setVariable(382,19);s.setVariable(630,3);s.setVariable(272,2);s.captureContinueCheckpoint(c.magazineSeedBindings,218,{9,7});need(s.variable(274)==3&&s.variable(291)==3&&s.variable(413)==spare,"Typed Continue capture incorrect");auto backup=s.exportState();s.consumeRound();s.loseHp(1000);s.setVariable(382,20);auto candidate=s.prepareContinueRestore(c.magazineSeedBindings,218,{9,7});need(s.hp()==0&&s.variable(630)==3,"Prepare partially committed");s.commitContinueRestore(candidate);need(s.count(13)==std::max(0,spare-1)&&s.equipped(1)==13&&s.magazine(13)==3&&s.hp()==e.hp&&s.variable(630)==4&&s.variable(382)==20&&s.saveCount()==e.saveCount,"Continue restore differs from source contract");need(!s.exportState().itemUses.contains(8),"Continue retained usage");need(s.conditions().poisoned&&!s.conditions().bleedingActive,"Healthy capture cleared condition history");auto again=s.prepareContinueRestore(c.magazineSeedBindings,218,{9,7});s.commitContinueRestore(again);need(s.count(13)==std::max(0,spare-1)&&s.variable(630)==5&&s.variable(413)==backup.worldValues.at(c.facts.variables.at(413)),"Repeated Continue consumed the backup");
  for(int field:{283,284,274,405}){auto b=s;auto before=b.exportState();b.setVariable(field,9999);auto damaged=b.exportState();rejects([&]{b.prepareContinueRestore(c.magazineSeedBindings,218,{9,7});});need(b.exportState()==damaged,"Rejected Continue changed live state");}
 }
 {GameSession s(c.items,base,c.facts);s.setVariable(405,1);rejects([&]{s.validateContinueCheckpoint(c.magazineSeedBindings,218,{9,7},false);});}
 {GameSession s(c.items,base,c.facts);s.setFlag(20,true);auto before=s.exportState();rejects([&]{s.captureContinueCheckpoint(c.magazineSeedBindings,218,{9,7});});need(s.exportState()==before,"Rejected capture changed live state");}
 auto e=fixture(c,base);GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,safe);int ticks=0;
 for(;ticks<3000&&(ticks==0||scene.foreground()!=ForegroundKind::None||!s.flag(441));++ticks){scene.update(ticks%8==0?key():InputFrame{});scene.takePresentation();}
 need(s.flag(441)&&s.flag(586)&&s.variable(104)==4&&s.variable(90)==2,"Boss intro/checkpoint did not complete");
 const auto shoot=[&](bool eye,AttackKind kind,int page,int damage,int open){s.setVariable(eye?89:103,damage);s.setVariable(105,open);s.setVariable(eye?83:97,160);s.setVariable(eye?84:98,120);s.setVariable(eye?90:104,eye?2:4);s.setVariable(eye?104:90,0);std::set<std::string> targets;return scene.resolveAttack(AttackSample{{160,120},{5,5},kind,page,0,false,100,AttackPhase::Impact},targets);};
 auto closed=shoot(false,AttackKind::GlFlame,6,799,0);std::cout<<"closed processed="<<closed.processed<<" damage="<<closed.damage<<" v103="<<s.variable(103)<<" feedback="<<s.variable(106)<<" hit="<<s.flag(24)<<" burn="<<s.flag(38)<<"\n";need(closed.processed==1&&closed.damage==0&&s.variable(103)==799&&s.variable(106)==123&&s.flag(24)&&s.flag(38),"Closed shell flame contract failed");
 auto threshold=shoot(false,AttackKind::GlNormal,6,800,1);need(threshold.processed==0&&s.variable(103)==800,"Body threshold accepted another receiver");
 std::cout<<"before eye\n";auto eye=shoot(true,AttackKind::Handgun,3,99,0);need(eye.processed==1&&eye.endRequested&&s.variable(92)==112&&s.flag(22),"Eye receiver feedback/end failed");
 std::cout<<"after eye\n";auto eyeEnd=shoot(true,AttackKind::Handgun,3,100,0);need(eyeEnd.processed==0,"Eye threshold accepted receiver");
 scene.deactivate();auto before=s.exportState();for(int i=0;i<200;++i){scene.update(key());}std::set<std::string> ids;scene.resolveAttack(AttackSample{{160,120},{5,5},AttackKind::GlNormal,6,0,false,200},ids);need(s.exportState()==before,"Retired boss changed current session");
 for(int branch=0;branch<3;++branch){
  std::cout<<"Death branch="<<branch<<"\n";auto entry=fixture(c,base);GameSession state(c.items,entry.session,c.facts);CombatScene battle(c,state,entry,safe);
  for(int n=0;n<3000&&(n==0||battle.foreground()!=ForegroundKind::None||!state.flag(441));++n){battle.update(n%8==0?key():InputFrame{});battle.takePresentation();}
  std::cout<<"death intro complete\n";const auto hp=state.hp(),rounds=state.magazine(state.equipped(1)),counter=state.variable(630);state.loseHp(1000);
  for(int n=0;n<3000&&!battle.inputPromptView().cursor;++n){battle.update(n%8==0?key():InputFrame{});battle.takePresentation();}
  std::cout<<"death prompt complete\n";need(battle.inputPromptView().cursor.has_value()&&state.hp()==0,"Dead Continue prompt absent");
  if(branch==2){battle.configurePresentation([](const SceneDefinition& d){if(d.width==20){throw std::runtime_error("injected resource failure");}});}
  if(branch==1){battle.update(key(InputAction::Down));battle.update({});}
  std::cout<<"death choose\n";const auto before=state.exportState();battle.update(key());
  for(int n=0;n<3000;++n){battle.update(n%8==0?key():InputFrame{});battle.takePresentation();if(branch==0&&battle.mapId()==165&&state.hp()>0&&battle.foreground()==ForegroundKind::None){break;}if(branch==2&&!battle.transferError().empty()){break;}if(branch==1&&battle.takeApplicationRequest()==ApplicationMode::Title){break;}}
  if(branch==0){need(battle.mapId()==165&&state.hp()==hp&&state.magazine(state.equipped(1))==rounds&&state.variable(630)==counter+1&&state.flag(586),"Death Yes did not restore and retry");}
  if(branch==1){need(state.hp()==0&&state.variable(630)==counter,"Death No restored live state");}
  if(branch==2){need(!battle.transferError().empty()&&state.exportState()==before,"Continue resource failure partially committed");}
 }
 std::cout<<"G3 typed Continue/backup rejection/receiver/retirement passed intro="<<ticks<<'\n';
}
}
int main(int argc,char**argv){try{std::cout.setf(std::ios::unitbuf);need(argc==5,"G3 package source isolated-root rules|natural|load");auto c=loadCombatContent(argv[1]);auto raw=read(argv[2]);auto saved=decodeSave(c,raw);need(c.compatibility=="post-f5-g3-normal-1","Wrong G3 identity");auto root=std::filesystem::path(argv[3]);std::filesystem::create_directories(root);std::string mode=argv[4];if(mode=="save-risk"){saveRisk(c,saved);return 0;}if(mode=="condition-risk"){conditionRisk(c,saved.session);return 0;}if(mode=="parts"){partVariation(c,saved.session);return 0;}if(mode=="rules"){saveRisk(c,saved);conditionRisk(c,saved.session);rules(c,saved.session);return 0;}G2NaturalDriver run(c,root,"public-inputs-"+mode+".jsonl");run.game.configureSaveDirectory(root);std::ofstream(root/"slot-01.json")<<raw;run.load();if(mode=="load"){loaded(run,saved);return 0;}run.reception();run.equip(21);run.actObject("m159-e5");for(int n=0;n<6000&&run.game.mapId()==164;++n){InputFrame in;if(n%500==0){auto p=run.game.world().player().motion.tile();std::cout<<"ENTRY n="<<n<<" xy="<<p.x<<","<<p.y<<" moving="<<run.game.world().player().motion.moving()<<" fg="<<int(run.game.foreground())<<" mode="<<int(run.game.mode())<<" controls="<<run.game.session().flag(1)<<run.game.session().flag(18)<<"\n";}if(run.game.foreground()!=ForegroundKind::None||run.game.dialogue().active()){if(n%8==0){in=key();}}else if(!run.game.world().player().motion.moving()){in.movement=next(run.game.world(),{19,9});}run.tick(in);}run.settle();need(run.game.mapId()==165&&run.game.session().flag(586),"Natural M164 intro did not reach real battle");std::cout<<"G3 natural entry ticks="<<run.ticks<<" HP="<<run.game.session().hp()<<'\n';
int fired=0;for(int n=0;n<60000&&!run.game.session().flag(449);++n){auto& g=run.game;const auto& st=g.session();InputFrame in;int weapon=st.equipped(1),rounds=st.magazine(weapon);if(n%500==0){auto t=g.world().player().motion.tile();std::cout<<"FIGHT n="<<n<<" HP="<<st.hp()<<" ammo="<<rounds<<" xy="<<t.x<<","<<t.y<<" eye="<<st.variable(83)<<","<<st.variable(84)<<" dmg="<<st.variable(89)<<","<<st.variable(103)<<" state="<<st.variable(91)<<","<<st.variable(201)<<" open="<<st.variable(105)<<" reg="<<st.variable(90)<<" control="<<st.flag(1)<<st.flag(18)<<" recovery="<<st.flag(45)<<" facing="<<int(g.world().player().motion.facing())<<" raised="<<g.raised()<<" fg="<<int(g.foreground())<<" mode="<<int(g.mode())<<"\n";}
if(g.inputPromptView().cursor){in=key();}else if(g.dialogue().active()||g.foreground()!=ForegroundKind::None){if(n%8==0){in=key();}}
else if(st.hp()<=45&&st.count(5)>0&&st.flag(1)&&!st.flag(45)){run.lower();run.item(5);continue;}else if(st.flag(45)){if(n%2==0){in=key(n%4?InputAction::Right:InputAction::Up);}}else if(!g.raised()){in=key(InputAction::Weapon);}else{const bool body=st.variable(105)==1;const bool valid=body||st.variable(90)>0;int x=body?160:st.variable(83);auto tile=g.world().player().motion.tile();int column=std::clamp((x-8)/16,1,18);if(tile.x!=column){in.movement=tile.x<column?Direction::Right:Direction::Left;}else if(g.world().player().motion.facing()!=Direction::Up){in.buttons[int(InputAction::Weapon)].held=true;in.movement=Direction::Up;}else if(valid&&n%12==0){in=key(InputAction::Attack);}}
run.tick(in);fired+=std::max(0,rounds-run.game.session().magazine(weapon));need(run.game.mode()==ApplicationMode::Playing,"Natural boss returned Title");}
need(run.game.session().flag(449),"Natural boss did not defeat");run.settle();need(run.game.mapId()==164&&run.game.session().flag(450),"Natural boss aftermath did not finish");std::cout<<"G3 natural win fired="<<fired<<" ticks="<<run.ticks<<" HP="<<run.game.session().hp()<<'\n';finishNatural(run);return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
