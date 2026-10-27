#include "g2_natural_driver.h"
#include "full_content.h"
#include "content_hash.h"
#include "scene_presentation.h"
#include <fstream>
#include <set>
#include "g4_dora_render.h"
namespace{
std::string read(const std::filesystem::path& p){std::ifstream in(p);return {std::istreambuf_iterator<char>(in),{}};}
CombatEntry entry(const CombatContent& c,const SessionEntry& s,int map,TilePosition tile){auto e=c.entries.at("post-pickup");e.map=map;e.playerTile=tile;e.positions.clear();e.directions.clear();e.pages.clear();e.through.clear();e.session=s;initializeFullSession(c,e.session);e.session.flags[1]=true;e.session.flags[18]=true;return e;}
void feedback(const CombatContent& c,const SessionEntry& saved,const std::filesystem::path& root){
 for(bool auxiliary:{false,true}){
  auto e=entry(c,saved,192,{14,22});GameSession s(c.items,e.session,c.facts);s.setFlag(13,true);s.setFlag(507,true);CombatScene scene(c,s,e,safe);
  scene.update(key(auxiliary?InputAction::AuxiliaryWeapon:InputAction::Weapon));for(int n=0;n<80&&!scene.raised();++n){scene.update({});scene.takePresentation();}need(scene.raised(),"Dora legal posture failed");
  const auto inventory=s.exportState();scene.takePresentation();scene.update(key(InputAction::Attack));auto events=scene.takePresentation();std::vector<std::string> cues;for(const auto& ev:events){if(ev.kind==PresentationEvent::Kind::Cue&&ev.id.starts_with("ce39-c")){cues.push_back(ev.id);}}
  need(cues.size()==2&&s.flag(225)&&s.exportState().inventory==inventory.inventory&&s.exportState().magazines==inventory.magazines&&s.variable(89)==0,"Dora replacement committed ammunition/failed feedback");
  const int selected=s.variable(44);s.setFlag(17,true);for(int n=0;n<50;++n){auto in=key(InputAction::Attack);scene.update(in);scene.takePresentation();}need(s.variable(44)==selected&&s.flag(225)&&!s.flag(17),"225 redrew/restarted feedback or lost source17 cleanup");
  for(int n=0;n<75;++n){scene.update({});scene.takePresentation();}need(!s.flag(225),"E17 Wait20 failed cleanup");
  if(!auxiliary){while(s.consumeRound()){}const auto empty=s.exportState();scene.update(key(InputAction::Attack));scene.takePresentation();need(s.flag(225)&&s.exportState().inventory==empty.inventory&&s.magazine(s.equipped(1))==0,"Dora empty main auto-reloaded");for(int n=0;n<12;++n){auto hold=key(InputAction::Cancel);hold.buttons[int(InputAction::Cancel)].pressed=false;scene.update(hold);scene.takePresentation();}for(int n=0;n<180;++n){scene.update({});scene.takePresentation();}need(s.magazine(s.equipped(1))>0,"Independent manual reload blocked by507");}
 }
 auto copy=c;auto e=entry(copy,saved,192,{14,22});e.session.flags[13]=true;e.session.flags[507]=true;e.session.flags[225]=true;copy.entries["dora-menu-check"]=e;GameApplication game(copy,"dora-menu-check",safe);game.update(key(InputAction::Cancel));need(game.mode()==ApplicationMode::Equipment,"507 blocked ordinary menu");const auto paused=game.session().exportState();for(int n=0;n<240;++n){game.update({});game.takePresentation();}need(game.session().flag(225)&&game.session().exportState()==paused,"Native menu consumed feedback Wait20");game.update(key(InputAction::Cancel));for(int n=0;n<150;++n){game.update({});game.takePresentation();}need(!game.session().flag(225),"Feedback cleanup did not resume after menu");
 auto rendered=c;auto re=entry(rendered,saved,192,{14,22});re.session.flags[13]=true;re.session.flags[507]=true;rendered.entries["dora-feedback-render"]=re;GameApplication shown(rendered,"dora-feedback-render",safe);DoraRender pixels(shown,rendered,root/"frames");
 const auto tick=[&](const InputFrame& input){shown.update(input);pixels.pending=shown.takePresentation();pixels.tick();};tick(key(InputAction::Weapon));for(int n=0;n<80;++n){tick({});}tick(key(InputAction::Attack));need(shown.session().flag(225),"Rendered feedback absent");pixels.frame("feedback45-46");for(int n=0;n<125;++n){tick({});}need(!shown.session().flag(225),"Rendered feedback did not erase");pixels.frame("feedback-erased");
 std::cout<<"02 eligible main/aux/225/Wait20/empty-ammo PASS\n";
}
void missiles(const CombatContent& c,const SessionEntry& saved){
 for(int event:{5,6,9,11}){for(bool protectedPlayer:{false,true}){for(bool qte:{false,true}){for(bool inside:{false,true}){
  auto e=entry(c,saved,192,{14,22});e.positions["m192-e"+std::to_string(event)]={inside?14:4,22};GameSession s(c.items,e.session,c.facts);s.setFlag(21,true);s.setVariable(91,11);s.setFlag(13,protectedPlayer);s.setFlag(10,qte);const int variable=event==5?119:event==6?133:event==9?147:161;s.setVariable(variable,1);CombatScene scene(c,s,e,safe);const int hp=s.hp();scene.update({});scene.takePresentation();
  need((s.variable(30)==10)==(inside&&!protectedPlayer&&qte),"Missile interruption gate violated");need((s.hp()<hp)==(inside&&!protectedPlayer),"Missile spatial/protection damage gate violated");
 }}}}
 auto d=c.maps.at(192);d.width=7;d.height=5;d.passageMasks.assign(35,15);d.objects.clear();d.player.tileX=2;d.player.tileY=2;ExplorationScene world(d);CharacterState flight{"edge-flight",99,CharacterMotion({3,-10},Direction::Down,128)};flight.layer=2;flight.through=true;flight.outsideMapMargin=10;world.addTransientActor(flight);GameSession s(c.items,saved,c.facts);RouteDefinition definition;definition.frequency=8;definition.skipBlocked=false;definition.outsideMapMargin=10;for(int n=0;n<13;++n){definition.steps.push_back({"down"});}RoutePlayer route(definition,"edge-flight");std::vector<PresentationEvent> events;for(int n=0;n<100;++n){route.tick(world,s,events,safe);world.advanceCharacters();}need(findCharacter(world,"edge-flight").motion.tile()==TilePosition{3,3},"Bounded alternate-map flight failed");
 bool rejected=false;auto altered=flight;altered.outsideMapMargin=11;try{world.setObjectState(altered);}catch(const std::runtime_error&){rejected=true;}need(rejected,"Runtime changed immutable flight margin");rejected=false;CharacterState ordinary{"ordinary-outside",100,CharacterMotion({3,-1},Direction::Down,128)};try{world.addTransientActor(ordinary);}catch(const std::runtime_error&){rejected=true;}need(rejected,"Ordinary actor admitted offscreen");
 std::cout<<"02 four missiles 32 spatial/protection/QTE gates and alternate-map offscreen entry PASS\n";
}
void actionChaining(const CombatContent& c,const SessionEntry& saved){
 // CN1.00 E2P11 C45–50 / P13 C90–95: selecting a successor replaces the
 // parallel page before the old tail can clear S21 (Player 0de2a9a RefreshPage).
 for(int mode:{1,3}){
  auto e=entry(c,saved,192,{14,22});e.positions["m192-e2"]={14,23};e.directions["m192-e2"]=Direction::Up;
  GameSession s(c.items,e.session,c.facts);s.setFlag(13,true);s.setFlag(21,true);s.setVariable(91,mode);
  CombatScene scene(c,s,e,[](int lo,int hi){if(lo==0&&hi==20){return 0;}if((lo==3||lo==4)&&hi==5){return 4;}return hi;});
  bool jump=false;
  for(int n=0;n<800&&!jump;++n){scene.update({});for(const auto& ev:scene.takePresentation()){jump|=ev.id=="m192-e2-p14-c7";}}
  need(jump,"Selected Dora jump was erased by previous action tail: "+std::to_string(mode));
 }
 // Once P9 enables its selected page, later distance branches cannot replace it.
 auto e=entry(c,saved,192,{14,22});e.positions["m192-e2"]={14,23};e.directions["m192-e2"]=Direction::Up;
 GameSession s(c.items,e.session,c.facts);s.setFlag(13,true);
 CombatScene scene(c,s,e,[](int lo,int hi){if(lo==0&&hi==500){return 0;}if(lo==3&&hi==5){return 4;}return lo;});
 scene.update({});scene.takePresentation();need(s.flag(21)&&s.variable(91)==4,"P9 continued after selecting its jump page");
 std::cout<<"02 air/missile jump chaining and P9 page replacement PASS\n";
}
void actions(const CombatContent& c,const SessionEntry& saved){
 for(int mode:{1,2,3,4,5,6}){for(bool protectedPlayer:{false,true}){
  auto e=entry(c,saved,192,{14,22});e.positions["m192-e2"]={14,23};e.directions["m192-e2"]=Direction::Up;GameSession s(c.items,e.session,c.facts);s.setFlag(21,true);s.setFlag(13,protectedPlayer);s.setVariable(91,mode);CombatScene scene(c,s,e,safe);const int hp=s.hp();bool cues=false;bool complete=false;
  for(int n=0;n<2500&&s.hp()>0;++n){scene.update({});for(const auto& ev:scene.takePresentation()){cues|=ev.kind==PresentationEvent::Kind::Sound||(ev.kind==PresentationEvent::Kind::Cue&&ev.id.starts_with("m192-"));}if((mode==4&&s.variable(91)==11)||!s.flag(21)||(mode==6&&s.variable(91)==4)){complete=true;break;}}
  need(cues&&complete,"Source boss action did not complete: "+std::to_string(mode)+" HP="+std::to_string(s.hp())+" 21="+std::to_string(s.flag(21))+" 91="+std::to_string(s.variable(91)));if(protectedPlayer){need(s.hp()==hp,"Protected boss action applied damage");}if(mode==2||mode==5){need((s.hp()<hp)==!protectedPlayer,"Facing melee action damage gate failed");}if(mode==4&&!protectedPlayer){need(s.hp()<hp&&s.flag(45),"Jump inner contact failed");}
 }}
 // Confirm only binds the highest legal window page and adjacent facing geometry.
 for(int mode:{0,11,12}){auto e=entry(c,saved,192,{14,22});e.direction=Direction::Up;e.positions["m192-e2"]={14,21};GameSession s(c.items,e.session,c.facts);s.setFlag(13,true);s.setFlag(21,true);s.setVariable(91,mode);CombatScene scene(c,s,e,safe);scene.update(key());need((s.variable(91)>=100)==(mode==11),"Confirm bypassed source window");}
 for(bool protectedPlayer:{false,true}){for(bool inside:{false,true}){auto e=entry(c,saved,192,{14,22});e.positions["m192-e3"]={inside?14:20,inside?22:16};GameSession s(c.items,e.session,c.facts);s.setFlag(21,true);s.setVariable(91,11);s.setFlag(13,protectedPlayer);s.setFlag(10,true);s.setVariable(105,1);s.setVariable(30,0);CombatScene scene(c,s,e,safe);const int hp=s.hp();bool explosion=false;for(int n=0;n<100;++n){scene.update({});for(const auto& event:scene.takePresentation()){explosion|=event.id=="m192-e3-p3-c8";}}need(explosion&&s.variable(105)==0&&s.variable(30)==0&&(s.hp()<hp)==(inside&&!protectedPlayer),"Air flight-end explosion protection/range/count gate failed");}}
 for(int mode:{1,2,4,5}){
  auto e=entry(c,saved,192,{mode==4?14:20,mode==4?22:16});e.positions["m192-e2"]={14,23};e.directions["m192-e2"]=Direction::Up;
  GameSession s(c.items,e.session,c.facts);s.setFlag(21,true);s.setVariable(91,mode);CombatScene scene(c,s,e,safe);const int hp=s.hp();bool completed=false;
  for(int n=0;n<600;++n){
   InputFrame in;if(mode==4){in.movement=next(scene.world(),{20,16});}scene.update(in);scene.takePresentation();
   if((mode==4&&s.variable(91)==11)||!s.flag(21)){completed=true;break;}
  }
  need(completed&&s.hp()==hp,"Source action dodge failed: "+std::to_string(mode));
  // The jump's contact is committed before mode11. Waiting motionless afterwards
  // permits the next source selection; it is not part of this jump's dodge.
  if(mode==4){need(s.variable(91)==11,"Jump dodge did not reach legal window");}
  if(mode==1){s.setFlag(21,true);s.setVariable(91,11);for(int n=0;n<180;++n){scene.update({});scene.takePresentation();}need(s.hp()==hp,"Air projectile tail hit distant player");}
  std::cout<<"02 dodge mode="<<mode<<" HP="<<s.hp()<<" source-mode="<<s.variable(91)<<" PASS\n";
 }
 std::cout<<"02 all six actions/protection/facing contact/window rejection/air end/dodge PASS\n";
}
void continueFailures(const CombatContent& c,const SessionEntry& saved){
 for(bool reject:{false,true}){auto e=entry(c,saved,192,{14,22});GameSession s(c.items,e.session,c.facts);s.captureContinueCheckpoint(c.magazineSeedBindings,219,{14,22});CombatScene scene(c,s,e,safe);s.loseHp(1000);for(int n=0;n<3000&&!scene.inputPromptView().cursor;++n){scene.update(n%8==0?key():InputFrame{});scene.takePresentation();}need(scene.inputPromptView().cursor.has_value(),"02 failure Continue absent");if(reject){scene.configurePresentation([](const SceneDefinition& d){if(d.width==20){throw std::runtime_error("02 injected resource failure");}});}else{scene.update(key(InputAction::Down));scene.update({});}const auto before=s.exportState();const int retryCount=s.variable(630);scene.update(key());bool title=false;for(int n=0;n<3000;++n){scene.update(n%8==0?key():InputFrame{});scene.takePresentation();title|=scene.takeApplicationRequest()==ApplicationMode::Title;if(title||!scene.transferError().empty()){break;}}if(reject){need(!scene.transferError().empty()&&s.exportState()==before,"Failed219 prepare partially committed");}else{need(title&&s.hp()==0&&s.variable(630)==retryCount,"CE38 No failed terminal title");}}
 std::cout<<"02 death No/title and failed219 prepare preservation PASS\n";
}
void rules(const CombatContent& c,const SessionEntry& saved,const std::filesystem::path& root){
 actionChaining(c,saved);feedback(c,saved,root);missiles(c,saved);actions(c,saved);continueFailures(c,saved);
 need(c.maps.contains(192)&&!c.catalog.at("pending_maps").contains("192")&&c.catalog.at("g4").at("delivered").get<int>()>=2,"Real battlefield not installed");
 need(c.catalog.at("continue").at("map")==219&&c.catalog.at("continue_entries").contains("218"),"Retry contract lost");
 for(int mode:{11,12,100,101}){auto e=entry(c,saved,192,{14,22});GameSession s(c.items,e.session,c.facts);s.setFlag(21,true);s.setVariable(91,mode);const auto* p=c.activeVariant(c.objects.at("m192-e2"),s);need(p->page==(mode==11?17:mode==12?18:20),"Dora highest source page wrong");}
 for(int progress:{0,20,40,60,90}){
  auto e=entry(c,saved,192,{14,22});GameSession s(c.items,e.session,c.facts);s.setVariable(89,progress);s.setFlag(21,true);s.setVariable(91,101);s.setVariable(29,progress>=90?18:progress>=40?15:10);
  CombatScene scene(c,s,e,safe);scene.update({});s.setVariable(30,10);bool started=false;bool dialogueObserved=false;for(int n=0;n<1000&&s.variable(89)==progress;++n){scene.update(n%8==0?key():InputFrame{});scene.takePresentation();started|=scene.foreground()==ForegroundKind::Boss;if(scene.dialogue().active()){dialogueObserved=true;need(s.variable(89)==progress,"Reward preceded completed dialogue");}}
  need(started&&dialogueObserved&&s.variable(89)==progress+(progress>=60?30:20),"Source QTE success/reward failed");
  if(progress==90){need(!s.flag(10)&&s.variable(29)==18&&s.flag(21)&&!s.flag(1),"Highest EndEventProcessing ran ordinary cleanup");}else{for(int n=0;n<1000&&scene.foreground()!=ForegroundKind::None;++n){scene.update(n%8==0?key():InputFrame{});scene.takePresentation();}need(scene.foreground()==ForegroundKind::None&&scene.playerVisible()&&!s.flag(10)&&s.variable(29)==0&&!s.flag(21),"QTE ordinary tail failed");}
 }
 auto e=entry(c,saved,192,{14,22});GameSession s(c.items,e.session,c.facts);s.setFlag(21,true);s.setVariable(91,100);CombatScene scene(c,s,e,safe);
 for(int n=0;n<1000&&s.flag(21);++n){scene.update({});scene.takePresentation();}
 need(s.variable(89)==0&&!s.flag(10)&&scene.playerVisible()&&s.variable(29)==0,"Timeout granted progress/failed cleanup");
 const auto before=s.exportState();const auto generation=scene.generation();scene.deactivate();for(int n=0;n<200;++n){scene.update(key());}need(s.exportState()==before&&!scene.restoreMessageDisplay(generation,{}),"Retired Dora wrote state");
 std::cout<<"02 focused source pages/success tiers/timeout/retirement PASS\n";
}
void enter(G2NaturalDriver& r){r.load();r.reception();r.actObject("m159-e6");r.walk({14,30});InputFrame in;in.movement=Direction::Up;r.tick(in);for(int n=0;n<30000&&(r.game.mapId()!=192||r.game.foreground()!=ForegroundKind::None);++n){r.tick(n%8==0?key():InputFrame{});}need(r.game.mapId()==192&&r.game.session().flag(507)&&r.game.session().flag(1)&&r.game.session().flag(18),"Actual intro did not unlock Dora");}
void natural(const CombatContent& c,const std::string& raw,const std::filesystem::path& root){
 std::ofstream(root/"slot-01.json")<<raw;auto generator=std::make_shared<std::mt19937>(19202026);G2NaturalDriver r(c,root,"public-inputs.jsonl",[generator](int low,int high){return std::uniform_int_distribution<int>(low,high)(*generator);});r.game.configureSaveDirectory(root);DoraRender render(r.game,c,root/"frames");
 r.observe=[&](const PresentationEvent& event){render.pending.push_back(event);};
 r.afterTick=[&]{render.tick();if(!r.game.hasGame()){return;}if(r.game.mapId()==192){const auto& st=r.game.session();const int mode=st.variable(91);if(r.game.dialogue().active()){render.frame("persuasion-"+std::to_string(st.variable(89)));if(r.game.dialogue().view().phase==DialoguePhase::AwaitingInput){render.frame("dialogue-"+std::to_string(st.variable(89)));}}else if(mode==1||mode==3||mode==4||mode==5||mode==6||mode==11||mode==100){render.frame("mode-"+std::to_string(mode));}for(int event:{3,5,6,9,11}){const auto& actor=findCharacter(r.game.world(),"m192-e"+std::to_string(event));if(actor.active&&actor.spriteId&&actor.motion.tile().y>=15&&actor.motion.tile().y<=26){render.frame("projectile-"+std::to_string(event));}}if(st.hp()==0){render.frame("natural-death");}}else if(r.game.mapId()==219){render.frame("continue219");}else if(r.game.mapId()==190&&r.game.session().flag(504)){render.frame("victory-pending03");}};
 enter(r);
 const auto original=r.game.session().exportState();const int beforeContinue=r.game.session().variable(630);
 for(int n=0;n<10000&&r.game.session().hp()>0;++n){r.tick();}need(r.game.session().hp()==0,"Natural death did not occur");
 for(int n=0;n<5000&&!r.game.inputPromptView().cursor;++n){r.tick(n%8==0?key():InputFrame{});}need(r.game.inputPromptView().cursor.has_value(),"Natural CE38 Continue absent");r.tick(key());
 bool via219=false;for(int n=0;n<10000;++n){r.tick(n%8==0?key():InputFrame{});via219|=r.game.mapId()==219;if(r.game.mapId()==192&&r.game.foreground()==ForegroundKind::None){break;}}
 need(via219&&r.game.mapId()==192&&r.game.session().hp()==46&&r.game.session().variable(630)==beforeContinue+1&&r.game.session().saveCount()==60&&r.game.session().magazine(20)==2&&r.game.session().magazine(21)==5&&r.game.session().count(111)==1&&!r.game.session().flag(504)&&r.game.session().flag(507),"Natural219 restore lost actual backup");
 std::cout<<"02 natural death/Continue219 actual HP46/card/ammo/count60 PASS step="<<r.ticks<<'\n';
 std::set<int> modes;int caught=0;int lastProgress=0;int peakProgress=0;std::vector<int> milestones;TilePosition fleeTarget{8,25};int corner=0;
 for(int n=0;n<80000&&r.game.mapId()==192;++n){
  auto& g=r.game;const auto& s=g.session();const int mode=s.variable(91);modes.insert(mode);if(n%50==0){std::cout<<"02 n="<<n<<" HP="<<s.hp()<<" progress="<<s.variable(89)<<" mode="<<mode<<" 29="<<s.variable(29)<<" fg="<<int(g.foreground())<<" p="<<g.world().player().motion.tile().x<<","<<g.world().player().motion.tile().y<<" boss="<<findCharacter(g.world(),"m192-e2").motion.tile().x<<","<<findCharacter(g.world(),"m192-e2").motion.tile().y<<'\n';}
  need(s.hp()>0,"02 natural battle died");if(s.hp()<70&&s.count(5)>0&&g.foreground()==ForegroundKind::None&&!s.flag(45)&&s.flag(1)){r.item(5);continue;}InputFrame in;
  if(g.dialogue().active()){if(n%8==0){in=key();}}
  else if(s.flag(10)||s.flag(45)){if(n%3==0){in=key(InputAction::Down);}else if(n%3==1){in=key(InputAction::Up);}}
  else if(g.foreground()==ForegroundKind::None){
   auto player=g.world().player();const auto boss=findCharacter(g.world(),"m192-e2").motion.tile();
   if(mode==11){const auto face=player.motion.facing();if(adjacentTile(player.motion.tile(),face)==boss){in=key();++caught;}else if(std::abs(player.motion.tile().x-boss.x)+std::abs(player.motion.tile().y-boss.y)==1){in.movement=faceToward(player.motion.tile(),boss);}else{for(Direction side:{Direction::Down,Direction::Up,Direction::Right,Direction::Left}){const auto tile=adjacentTile(boss,side);const auto step=next(g.world(),tile);if(step){in.movement=step;break;}}}}
   else if(mode==4||mode==12){const auto tile=player.motion.tile();const auto step=next(g.world(),{std::clamp(tile.x+(tile.x<14?-3:3),7,21),std::clamp(tile.y+(tile.y<21?-3:3),15,26)});if(step){in.movement=step;}}
   else if(mode!=11){const auto tile=player.motion.tile();const std::array<TilePosition,4> corners{{{8,25},{20,25},{20,16},{8,16}}};if(std::abs(tile.x-fleeTarget.x)+std::abs(tile.y-fleeTarget.y)<2){corner=(corner+1)%4;fleeTarget=corners[corner];}const auto step=next(g.world(),fleeTarget);if(step){in.movement=step;}}
  }
  r.tick(in);if(g.session().variable(89)!=lastProgress){std::cout<<"02 persuasion="<<g.session().variable(89)<<" step="<<r.ticks<<'\n';lastProgress=g.session().variable(89);peakProgress=std::max(peakProgress,lastProgress);milestones.push_back(lastProgress);}
 }
 need(r.game.mapId()==190&&r.game.session().flag(504)&&!r.game.session().flag(586)&&r.game.foreground()==ForegroundKind::Content,"Natural complete Dora did not commit locked win");const auto committed=r.game.session().exportState();for(int n=0;n<60;++n){r.tick(key());}need(r.game.mapId()==190&&r.game.foreground()==ForegroundKind::Content&&r.game.session().exportState()==committed,"Repeated win input changed locked handoff");
 std::ofstream(root/"result.json")<<Json{{"steps",r.ticks},{"hp",r.game.session().hp()},{"progress",lastProgress},{"peak_progress",peakProgress},{"milestones",milestones},{"504",r.game.session().flag(504)},{"586",r.game.session().flag(586)},{"foreground",int(r.game.foreground())},{"magazine20",r.game.session().magazine(20)},{"magazine21",r.game.session().magazine(21)},{"retry_count",r.game.session().variable(630)},{"save_count",r.game.session().saveCount()},{"frames",render.frames},{"modes",modes},{"caught_inputs",caught},{"map",r.game.mapId()}}.dump(2)<<'\n';
 std::cout<<"02 natural complete battle PASS steps="<<r.ticks<<" HP="<<r.game.session().hp()<<'\n';
}
}
int main(int argc,char** argv){try{std::cout.setf(std::ios::unitbuf);need(argc==5,"Dora package real-save root rules|natural");auto c=loadCombatContent(argv[1]);const auto raw=read(argv[2]);need(contentSha256(raw)=="f87bb3d63d67803dc33916916dface8096d7ad09ef86cf3270570c93627edb62","Changed actual G3 disk");const auto saved=decodeSave(c,raw);std::filesystem::path root=argv[3];std::filesystem::create_directories(root);if(std::string(argv[4])=="rules"){need(decodeSave(c,encodeSave(c,saved))==saved,"normal-2 identity lost save roundtrip");SaveFiles files(c,root/"new-identity");need(files.write(1,saved).outcome==WriteOutcome::Committed&&files.read(1)==saved,"normal-2 native save persistence failed");auto restored=prepareRestore(c,files.read(1));need(restored.scene->mapId()==151&&restored.session->hp()==46&&restored.session->saveCount()==60,"normal-2 restore changed actual progress");rules(c,saved.session,root);}else{natural(c,raw,root);}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
