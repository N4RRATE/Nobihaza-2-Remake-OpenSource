#include "title_navigation.h"
#include "g1_piano_driver.h"
#include "g4_dora_render.h"
#include "content_hash.h"
#include <iostream>
#include <fstream>
namespace{
std::string bytes(const std::filesystem::path&p){std::ifstream f(p);need(bool(f),"Missing "+p.string());return {std::istreambuf_iterator<char>(f),{}};}
void toChoice(GameApplication&g){std::optional<MusicPlaybackIdentity> music;for(int n=0;n<400&&!g.inputPromptView().cursor;++n){g.update({});for(const auto&e:g.takePresentation()){if(e.musicPlayback){music=e.musicPlayback;}}if(music&&g.mapId()==215){need(g.reportMusicTicks(*music,0),"Clear music identity rejected");}}need(g.mapId()==216&&g.inputPromptView().cursor,"Clear C7/216 not reached");}
void selectStart(GameApplication&g){g.update(key());for(int n=0;n<8&&!g.postClearRequest();++n){g.update({});}need(g.postClearRequest().has_value(),"Retained start request absent");}
void focused(const CombatContent&c,const SaveSnapshot&source,const std::filesystem::path&out){
 std::string error;GameApplication g(c,"title");need(!g.startNewCycle({g.generation(),0},error)&&!g.hasGame(),"Title fabricated Clear");
 need(g.restore(source,error),"True old Clear rejected");toChoice(g);const auto before=g.session().exportState();const auto generation=g.generation();int attempts=0;
 g.configureRestorePresentation([&](const SceneDefinition&,const SavedPresentation&){++attempts;throw std::runtime_error("missing opening media");});selectStart(g);const auto request=*g.postClearRequest();
 need(attempts==1&&g.generation()==generation&&g.session().exportState()==before,"Failed preparation changed Clear");for(int n=0;n<20;++n){g.update({});}need(attempts==1,"Failed request retried without input");
 auto stale=request;++stale.scene;need(!g.startNewCycle(stale,error)&&g.session().exportState()==before,"Stale generation accepted");need(!g.startNewCycle({request.scene,1},error),"Bonus incremented cycle");
 g.configureRestorePresentation({});g.update(key());need(g.openingActive()&&g.mapId()==2&&g.world().player().motion.tile()==TilePosition{9,7},"Retry did not enter source M2 C7");
 need(g.session().variable(635)==before.worldValues.at(c.facts.variables.at(635))+1&&g.session().saveCount()==before.saveCount&&g.session().variable(399)==0,"Cycle counters not retained/reset");
 need(!g.startNewCycle(request,error),"Old request reused on new session");for(int n=0;n<20;++n){g.update(key());}need(g.session().variable(635)==1,"Repeated input added cycles");
 const auto& seed=g.session().exportState();need(seed.baseDefense==before.baseDefense&&g.session().equipped(3)==0&&g.session().count(49)==0,"I49 effective bonus inherited");need(!seed.skills.contains(3)&&!seed.skills.contains(4),"CE102/M2 skills retained");
 for(int id=1;id<=600;++id){need(!g.session().flag(id),"Old source switch retained before initialization");}for(int id=1;id<=634;++id){need(id==13||g.session().variable(id)==0,"Old source variable retained before initialization: "+std::to_string(id));}
 for(const auto&r:c.catalog.at("cycle").at("rewards")){need(g.session().count(r.at("item"))==int(before.worldFlags.at(c.facts.switches.at(r.at("flag")))),"Natural reward count differs");}
 bool failedMap=false;int stagedMaps=0;GameApplication maps(c,"title",safe,[&](const SceneDefinition&d){++stagedMaps;if(failedMap&&d.backgroundPath==c.maps.at(87).backgroundPath){throw std::runtime_error("missing full-branch map");}});
 maps.configureRestorePresentation([](const SceneDefinition&,const SavedPresentation&){});need(maps.restore(source,error),"Map preflight source rejected");toChoice(maps);const auto oldMapState=maps.session().exportState();failedMap=true;selectStart(maps);need(stagedMaps>=2&&maps.mapId()==216&&maps.session().exportState()==oldMapState,"Later opening branch failed after destroying Clear");failedMap=false;maps.update(key());need(maps.mapId()==2&&maps.session().variable(635)==1,"Branch preparation retry not published exactly once");
 GameApplication bonus(c,"title");need(bonus.restore(source,error),"Bonus source rejected");toChoice(bonus);const auto baseline=bonus.session().exportState();bonus.update(key(InputAction::Cancel));for(int n=0;n<10&&!bonus.postClearRequest();++n){bonus.update({});}need(bonus.postClearRequest()&&bonus.postClearRequest()->selection==1&&bonus.session().exportState()==baseline,"Cancel did not request bonus without increment");
 for(int variant=0;variant<2;++variant){auto s=source;s.session.maxHp=s.session.hp=variant?149:117;s.session.maxTp=s.session.tp=variant?43:35;s.session.baseAttack=variant?19:7;s.session.baseDefense=variant?27:4;s.session.baseAgility=variant?14:5;s.session.baseSpirit=variant?12:6;s.session.skills.insert(7);s.session.skills.insert(8);s.session.skills.insert(9);s.session.skills.insert(4);s.session.worldValues[c.facts.variables.at(635)]=variant?999999:4;for(int id=601;id<=605;++id){s.session.worldFlags[c.facts.switches.at(id)]=true;}
  // Legal pure-value variations validate the same disk contract; they are not natural grant evidence.
  validateSave(c,s);GameApplication v(c,"title");need(v.restore(s,error),"Legal Actor1 variation rejected");toChoice(v);v.update(key());for(int n=0;n<10&&!v.openingActive();++n){v.update({});}need(v.openingActive(),"Variation cycle absent");const auto e=v.session().exportState();need(e.hp==s.session.hp&&e.tp==s.session.tp&&e.baseAttack==s.session.baseAttack&&e.baseDefense==s.session.baseDefense&&e.baseAgility==s.session.baseAgility&&e.baseSpirit==s.session.baseSpirit&&e.level==s.session.level,"Legal Actor1 pure values reset to fresh defaults");need(e.skills==std::set<int>({7,8,9})&&v.session().variable(635)==(variant?999999:5),"Skills/counter clamp differs");for(int i=55;i<=59;++i){need(v.session().count(i)==1,"Complete reward variations failed");}
 }
 for(auto name:{"unknown-skill","attribute","missing-unlock","false-victory","old-ordinary-unlock","counter-overflow","counter-int-max"}){auto bad=source;if(std::string(name)=="unknown-skill"){bad.session.skills.insert(999);}else if(std::string(name)=="attribute"){bad.session.baseDefense=1000;}else if(std::string(name)=="missing-unlock"){bad.session.worldFlags.erase(c.facts.switches.at(605));}else if(std::string(name)=="false-victory"){bad.session.worldFlags[c.facts.switches.at(582)]=false;}else if(std::string(name)=="counter-overflow"||std::string(name)=="counter-int-max"){bad.session.worldValues[c.facts.variables.at(635)]=std::string(name)=="counter-overflow"?1000000:INT_MAX;}else{bad.contentCompatibility=c.legacySource->compatibility;bad.continuation.kind="ordinary";}
  GameApplication v(c,"title");need(!v.restore(bad,error)&&!v.hasGame(),"Corrupt/unknown source accepted");
 }
 std::ofstream(out/"focused.json")<<Json{{"source_kind","real_Clear"},{"candidate_failure_preserves",true},{"retry_once",true},{"stale_rejected",true},{"counter_clamp",999999},{"Actor1_variations","legal_source_only"},{"ordinary_no_guess",true}}.dump(2)<<'\n';
 std::cout<<"G7 real Clear candidate/retry/old request/Actor1 pure values/reset/rewards/rejections PASS\n";
}
struct CycleDriver:PianoDriver{
 DoraRender view;
 CycleDriver(const CombatContent&c,const std::filesystem::path&out):PianoDriver(c,out),view(game,c,out/"frames"){
  observe=[&](const PresentationEvent&e){view.pending.push_back(e);};afterTick=[&]{view.tick();if(auto t=view.presentation->musicTicks()){game.reportMusicTicks(t->first,t->second);}};
 }
 void clearLoad(const std::filesystem::path&slot){const auto isolated=view.root.parent_path()/"slots";std::filesystem::create_directories(isolated);std::filesystem::copy_file(slot,isolated/"slot-02.json",std::filesystem::copy_options::overwrite_existing);game.configureSaveDirectory(isolated);finishTitleOpening(game);game.returnToTitle();finishTitleOpening(game);tick(key());for(int n=1;n<2;++n){tick(key(InputAction::Down));}tick(key());need(game.mapId()==215,"Public independent Clear Load rejected");for(int n=0;n<400&&!game.inputPromptView().cursor;++n){tick();}need(game.mapId()==216&&game.inputPromptView().cursor,"Actual PCM Clear C7 rejected");}
 void firstBattle(){int spent=0;
  for(int n=0;n<12000&&!game.session().flag(101);++n){const auto p=game.world().player().motion.tile(),t=findCharacter(game.world(),"m4-e4").motion.tile();InputFrame in;const int rounds=game.session().magazine(11);
   if(game.dialogue().active()||game.foreground()!=ForegroundKind::None){if(n%8==0){in=key();}}
   else if(!game.raised()){in=key(InputAction::Weapon);}
   else if(!game.enemyStates().empty()&&game.enemyStates().front().damage>=100){}
   else if((p.x!=t.x&&p.y!=t.y)||std::abs(p.x-t.x)+std::abs(p.y-t.y)>4){for(auto side:{Direction::Down,Direction::Up,Direction::Right,Direction::Left}){auto step=next(game.world(),adjacentTile(t,side));if(step){in.movement=step;break;}}}
   else{const int dx=t.x-p.x,dy=t.y-p.y;auto face=std::abs(dx)>std::abs(dy)?(dx>0?Direction::Right:Direction::Left):(dy>0?Direction::Down:Direction::Up);if(game.world().player().motion.facing()!=face){in.buttons[int(InputAction::Weapon)].held=true;in.movement=face;}else if(n%12==0){in=key(InputAction::Attack);}}
   tick(in);spent+=std::max(0,rounds-game.session().magazine(11));need(game.session().hp()>0,"Natural first encounter died");
  }
  need(game.session().flag(101)&&spent>0,"Natural first soldier kill absent");for(int n=0;n<3&&game.raised();++n){lower();}std::cout<<"G7 natural first encounter rounds="<<spent<<" HP="<<game.session().hp()<<" PASS\n";
 }
 void opening(bool skip){bool transport=false;bool skipArrival=false,fullArrival=false;int confirmations=0;
  for(int n=0;n<90000&&game.openingActive();++n){InputFrame in;const auto stage=game.openingStage();transport|=game.mapId()==87;const auto tile=game.world().player().motion.tile();skipArrival|=game.mapId()==4&&tile==TilePosition{10,8};fullArrival|=game.mapId()==4&&tile==TilePosition{7,4};
   if(game.inputPromptView().cursor){int row=stage=="opening-choice"?(skip?1:0):1;in=key(game.inputPromptView().cursor->line==row?InputAction::Confirm:InputAction::Down);++confirmations;}
   else if(stage=="door-approach-common"){if(!game.world().player().motion.moving()){if(tile==adjacentTile(game.content().objects.at("m4-e2").tile,Direction::Down)){if(game.world().player().motion.facing()!=Direction::Up){in.movement=Direction::Up;}else{in=key();}}else{in.movement=next(game.world(),adjacentTile(game.content().objects.at("m4-e2").tile,Direction::Down));need(in.movement.has_value(),"Door path absent");}}}
   else if(n%13==0){in=key();}tick(in);
   need(game.mode()==ApplicationMode::Playing,"Real cycle opening left game");
  }
  need(!game.openingActive()&&game.mapId()==4&&game.session().flag(51)&&transport==!skip&&(skip?skipArrival:fullArrival),"Original two arrival branches differ");
  need(game.session().count(180)==1&&game.session().equipped(3)==45&&game.session().magazine(56)==6&&game.session().magazine(27)==7&&game.session().variable(635)==1,"Opening/reward magazines not reset at source writes");view.frame(skip?"skip-opening-handoff":"full-opening-handoff");
  std::cout<<"G7 actual cycle opening skip="<<skip<<" transport="<<transport<<" steps="<<ticks<<" prompts="<<confirmations<<" PASS\n";
 }
};
void natural(const CombatContent&c,const std::filesystem::path&slot,const std::filesystem::path&out,bool skip){
 CycleDriver r(c,out);r.clearLoad(slot);const auto before=r.game.session().exportState();r.view.frame("real-clear216");r.tick(key());for(int n=0;n<10&&!r.game.openingActive();++n){r.tick();}need(r.game.openingActive(),"Real public start did not publish");need(r.game.session().saveCount()==before.saveCount&&r.game.session().variable(399)==0,"Initial N/0 violated");r.opening(skip);
 std::ofstream(out/"natural-result.json")<<Json{{"source_sha256",contentSha256(bytes(slot))},{"source_count",before.saveCount},{"cycle",r.game.session().variable(635)},{"statistic",r.game.session().variable(399)},{"skip",skip},{"steps",r.ticks},{"maps",r.maps},{"real_Clear_Load",true}}.dump(2)<<'\n';
 if(!skip){return;}
 // First legal ordinary Save consumer is the original BSAA entrance crow.
 r.actObject("m4-e10");r.equip(11);r.firstBattle();r.actObject("m4-e4");
 for(const auto* id:{"m4-e2","m6-e13","m9-e6","m10-e9","m12-e8","m13-e4","m15-e6"}){r.actObject(id);need(r.game.session().hp()>0,"Natural first-save route died");}
 need(r.game.mapId()==16,"Natural BSAA entrance absent");r.actObject("m16-e5");need(r.game.mode()==ApplicationMode::Saving,"Natural ordinary Save request absent");
 for(int n=0;n<200;++n){r.tick();}r.tick(key());need(r.game.fileMenuView().has_value(),"Ordinary slots absent");
 // Use isolated slot3; the source Clear slot2 remains byte-for-byte unchanged.
 for(int n=1;n<3;++n){r.tick(key(InputAction::Down));}r.tick(key());if(auto v=r.game.fileMenuView();v&&v->confirmation){if(v->answer){r.tick(key(InputAction::Up));}r.tick(key());}
 need(r.game.mode()==ApplicationMode::Playing&&r.game.session().saveCount()==before.saveCount+1&&r.game.session().variable(399)==1,"Ordinary return N+1/1 differs");
 auto saved=SaveFiles(c,out/"slots").read(3);need(saved.session.saveCount==before.saveCount+1&&saved.continuation.statisticBefore==0&&saved.session.worldValues.at(c.facts.variables.at(635))==1,"Ordinary disk lost cycle/native count");
 std::ofstream(out/"ordinary-result.json")<<Json{{"source_count",before.saveCount},{"disk_count",saved.session.saveCount},{"disk_statistic",saved.continuation.statisticBefore},{"return_statistic",r.game.session().variable(399)},{"cycle",r.game.session().variable(635)},{"steps",r.ticks},{"maps",r.maps},{"ordinary_point",saved.continuation.point}}.dump(2)<<'\n';
 r.view.frame("new-cycle-ordinary-return");std::cout<<"G7 natural Clear Load->cycle->opening->BSAA ordinary N+1/1 Save PASS\n";
}
void clearBounds(const CombatContent&c,const SaveSnapshot&source,const std::filesystem::path&out){
 std::string error;GameApplication app(c,"title");need(app.restore(source,error),"Clear bounds valid source rejected");const auto baseline=app.session().exportState();const auto generation=app.generation();
 for(int invalid:{-1,1000000,INT_MAX}){auto bad=source;bad.session.worldValues[c.facts.variables.at(635)]=invalid;need(!app.restore(bad,error)&&app.session().exportState()==baseline&&app.generation()==generation,"Clear out-of-range candidate destroyed current Clear");}
 auto cap=source;cap.session.worldValues[c.facts.variables.at(635)]=999999;need(app.restore(cap,error),"Source cap should remain legal");
 auto signedSource=source;signedSource.session.magazines[27]=-999999;validateSave(c,signedSource);GameApplication signedCycle(c,"title");need(signedCycle.restore(signedSource,error),"Legal signed Clear source rejected");toChoice(signedCycle);signedCycle.update(key());
 for(int n=0;n<6000&&signedCycle.mapId()!=4;++n){InputFrame in;if(signedCycle.inputPromptView().cursor){in=key(signedCycle.inputPromptView().cursor->line==1?InputAction::Confirm:InputAction::Down);}else if(n%13==0){in=key();}signedCycle.update(in);}
 need(signedCycle.mapId()==4&&signedCycle.session().magazine(27)==7&&signedCycle.session().magazine(56)==6&&signedCycle.session().variable(635)==1&&signedCycle.session().saveCount()==source.session.saveCount,"Original new-cycle seeds carried negative LC9 or lost native count");
 std::ofstream(out/"signed-cycle.json")<<Json{{"source_kind","legal_Clear_value_variation"},{"source_LC9",-999999},{"new_LC9",7},{"new_SAA",6},{"cycle",1},{"native_count",signedCycle.session().saveCount()},{"natural_signed_source",false}}.dump(2)<<'\n';
 std::ofstream(out/"clear-bounds.json")<<Json{{"actual_restore_entry",true},{"rejected",{-1,1000000,INT_MAX}},{"max_accepted",999999},{"failure_preserves_current_Clear",true}}.dump(2)<<'\n';std::cout<<"G7 Clear Restore rejects negative/1000000/INT_MAX, preserves current, accepts999999 PASS\n";
}
void independent(const CombatContent&c,const std::filesystem::path&slot,const std::filesystem::path&out){
 CycleDriver r(c,out);r.game.configureSaveDirectory(slot.parent_path());finishTitleOpening(r.game);r.game.returnToTitle();finishTitleOpening(r.game);r.tick(key());r.tick(key(InputAction::Down));r.tick(key(InputAction::Down));r.tick(key());
 need(r.game.mode()==ApplicationMode::Playing&&r.game.mapId()==16&&r.game.session().variable(635)==1&&r.game.session().variable(399)==1,"Independent ordinary public Load lost cycle/statistic");
 const auto s=decodeSave(c,bytes(slot));
 for(int invalid:{-1,1000000,INT_MAX}){auto bad=s;bad.session.worldValues[c.facts.variables.at(635)]=invalid;bool rejected=false;try{validateSave(c,bad);}catch(const std::exception&){rejected=true;}need(rejected,"Ordinary cycle counter outside source range accepted");}
 auto noCycle=s;noCycle.session.worldValues[c.facts.variables.at(635)]=0;bool rejected=false;try{validateSave(c,noCycle);}catch(const std::exception&){rejected=true;}need(rejected,"Ordinary unlocked rewards guessed without cycle");
 need(r.game.session().saveCount()==s.session.saveCount&&r.game.session().count(56)==1&&r.game.session().magazine(56)==6,"Independent ordinary Load lost reward/native count");
 std::string error;need(!r.game.startNewCycle({r.game.generation(),0},error),"Ordinary disk guessed Clear");const auto before=r.game.session().exportState();need(r.game.restore(s,error)&&r.game.session().exportState()==before,"Repeated ordinary Load reapplied counts");r.tick();r.view.frame("independent-ordinary-load");
 std::ofstream(out/"load-result.json")<<Json{{"independent_public_Load",true},{"ordinary_point",s.continuation.point},{"save_count",r.game.session().saveCount()},{"statistic",r.game.session().variable(399)},{"cycle",r.game.session().variable(635)},{"SAA_count",r.game.session().count(56)},{"SAA_magazine",r.game.session().magazine(56)}}.dump(2)<<'\n';
 std::cout<<"G7 independent ordinary public Load/cycle/reward/N+1/V399=1 PASS\n";
}
}
int main(int argc,char**argv){try{std::cout.setf(std::ios::unitbuf);need(argc==5,"Expected package output checks|skip|full source Clear");auto c=loadCombatContent(argv[1]);std::filesystem::path out=argv[2];std::filesystem::create_directories(out);const auto source=decodeSave(c,bytes(argv[4]));if(std::string(argv[3])=="bounds"){clearBounds(c,source,out);}else if(std::string(argv[3])=="load"){independent(c,argv[4],out);}else if(std::string(argv[3])=="checks"){focused(c,source,out);}else{natural(c,argv[4],out,std::string(argv[3])=="skip");}return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
