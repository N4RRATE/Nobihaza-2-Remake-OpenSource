#include "g5_control_driver.h"
#include "g4_truth_driver.h"
#include "g2_natural_driver.h"
#include "full_content.h"
#include "content_hash.h"
#include "scene_presentation.h"
#include <fstream>
#include <set>
#include "g4_dora_render.h"
namespace {
std::string read(const std::filesystem::path& p){std::ifstream in(p);return {std::istreambuf_iterator<char>(in),{}};}
using g4truth::enter;
using g4truth::winDora;
using g4truth::winCash;
void rules(const CombatContent& c,const std::string& raw,const std::filesystem::path& root){
 for(int map:{193,194}){need(c.maps.contains(map)&&c.maps.at(map).width==160&&c.maps.at(map).height==15,"Original truth long map absent");}
 need(c.catalog.at("flows").at("m190-e8-p2").at("runtime_enabled").get<bool>(),"Truth choice consumer absent");
 const auto saved=decodeSave(c,raw);need(saved.contentCompatibility=="post-f5-g4-normal-3"&&saved.session.worldValues.at("tower.truth-stage")==0&&!saved.session.worldFlags.at("tower.truth-completed"),"Exact legacy chain did not initialize truth facts");
 need(decodeSave(c,encodeSave(c,saved))==saved,"Truth identity save roundtrip lost state");SaveFiles files(c,root/"identity");need(files.write(1,saved).outcome==WriteOutcome::Committed&&files.read(1)==saved,"Truth native save persistence failed");
 auto restore=prepareRestore(c,files.read(1));need(restore.scene->mapId()==151&&restore.session->hp()==46&&restore.session->saveCount()==60,"Truth restore changed real disk");
 for(int value:{-1,1,2,3,4,5,6}){auto bad=saved;bad.session.worldValues.at("tower.truth-stage")=value;bool rejected=false;try{encodeSave(c,bad);}catch(const std::exception&){rejected=true;}need(rejected,"Invalid truth stage accepted");}
 for(int stage:{0,5}){auto later=saved;later.session.worldFlags.at("tower.dora-defeated")=true;later.session.worldFlags.at("tower.truth-completed")=true;later.session.worldValues.at("tower.truth-stage")=stage;need(decodeSave(c,encodeSave(c,later))==later,"Completed branch lost at known legal save node");need(files.write(stage==0?2:3,later).outcome==WriteOutcome::Committed&&files.read(stage==0?2:3)==later,"Completed branch disk roundtrip lost identity");}
 auto bad=saved;bad.session.worldFlags.at("tower.truth-completed")=true;bool rejected=false;try{encodeSave(c,bad);}catch(const std::exception&){rejected=true;}need(rejected,"Truth completion before victory accepted");
 need(c.cue("m190-e12-p2-c161").at("effect_mode")==0,"Source nonrotating picture changed");
 std::cout<<"03 strict loader/save identity and rejection PASS\n";
}
void saveRejections(const CombatContent& c,const std::string& raw,const std::filesystem::path& root){
 const auto saved=decodeSave(c,raw);GameApplication active(c,"title",safe);std::string error;need(active.restore(saved,error),"G4 active restore failed");const auto generation=active.generation();const auto live=active.session().exportState();
 std::ofstream(root/"slot-01.json")<<raw;SaveFiles files(c,root);auto unknown=Json::parse(raw);unknown["content"]="unknown-g4-content";std::ofstream(root/"slot-02.json")<<unknown.dump();std::ofstream(root/"slot-03.json")<<"{broken";
 for(int slot:{2,3}){bool rejected=false;try{files.read(slot);}catch(const std::exception&){rejected=true;}need(rejected&&active.generation()==generation&&active.session().exportState()==live&&contentSha256(read(root/"slot-01.json"))==contentSha256(raw),"Rejected G4 disk changed original disk or active game");}
 auto invalid=saved;invalid.scene.playerTile={-1,-1};need(!active.restore(invalid,error)&&!error.empty()&&active.generation()==generation&&active.session().exportState()==live&&contentSha256(read(root/"slot-01.json"))==contentSha256(raw),"Rejected G4 restore partially replaced active game/disk");
 std::ofstream(root/"result.json")<<Json{{"unknown_identity_rejected",true},{"malformed_json_rejected",true},{"invalid_candidate_rejected",true},{"active_generation_unchanged",true},{"active_session_unchanged",true},{"original_disk_sha256",contentSha256(read(root/"slot-01.json"))}}.dump(2)<<'\n';std::cout<<"04 current G4 unknown/corrupt disk and invalid restore preserve disk/active game PASS\n";
}
void natural(const CombatContent& c,const std::string& raw,const std::filesystem::path& root,int choice,bool cashVictory=false,bool cycle=false){
 std::filesystem::create_directories(root);std::ofstream(root/"slot-01.json")<<raw;
 bool rejectCash=choice==4;int prepareAttempts=0;SessionEntry failedState;GameApplication* running{};
 auto generator=std::make_shared<std::mt19937>(19202026);G2NaturalDriver r(c,root,"public-inputs.jsonl",[generator](int lo,int hi){return std::uniform_int_distribution<int>(lo,hi)(*generator);},[&](const SceneDefinition& d){if(rejectCash&&std::any_of(d.objects.begin(),d.objects.end(),[](const auto& o){return o.id=="m191-e2";})){++prepareAttempts;if(prepareAttempts==1){failedState=running->session().exportState();}need(running->mapId()==190&&running->session().exportState()==failedState,"Failed Cash preparation partially published or repeated capture");throw std::runtime_error("03 injected Cash renderer preparation failure");}SceneRenderer ready(d);});running=&r.game;r.game.configureSaveDirectory(root);DoraRender render(r.game,c,root/"frames");
 Json cues=Json::array(),changes=Json::array();bool checking=false;int previousMap=0,previousStage=-1;std::set<int> stages;int show19=0,show0=0;bool tailVisible=false;int moves=0;int listenPlacement=0;int captures=0;SessionEntry atCapture;std::vector<std::string> notices;
 r.observe=[&](const PresentationEvent& event){render.pending.push_back(event);if(checking&&event.kind==PresentationEvent::Kind::Cue){cues.push_back({{"step",r.ticks},{"map",r.game.mapId()},{"id",event.id},{"transition",event.transition}});if(event.id=="truth-transfer194-show"){need(event.transition==19,"Native194 show failed to consume slot19");++show19;}if(event.id=="m190-e12-p2-c7"){need(findCharacter(r.game.world(),"m190-e12").motion.tile()==TilePosition{14,17},"Rebuilt listen actor did not apply source C6 placement");++listenPlacement;}if(event.id=="m194-e25-p1-c6"){need(event.transition==0,"193 transfer tail did not restore show slot0");++show0;}if(event.id.starts_with("m190-e12-p2-c")&&c.cue(event.id).at("kind")=="move_picture"){++moves;need(event.sampledScreenPosition.has_value(),"Lost actor-sampled picture anchor");}}if(event.kind==PresentationEvent::Kind::Notice){notices.push_back(event.id);}};
 r.afterTick=[&]{render.tick();if(!checking||!r.game.hasGame()){return;}auto& g=r.game;const auto& s=g.session();need(g.mode()==ApplicationMode::Playing,"Truth exposed free menu/save");need(!g.takeSaveRequest(),"Truth exposed ordinary save");
  if(prepareAttempts>=120){rejectCash=false;}
  if(prepareAttempts>0&&rejectCash){need(g.mapId()==190&&s.flag(504)&&s.flag(506)&&s.flag(586)&&!s.flag(1)&&!s.flag(18)&&contentSha256(read(root/"slot-01.json"))==contentSha256(raw),"Rejected Cash preparation changed facts/disk or freed controls");}
  if(moves==2||moves==8||moves==14){render.frame("spin-moves-"+std::to_string(moves));}
  const int map=g.mapId(),stage=s.variable(627);stages.insert(stage);
  if(stage!=previousStage||map!=previousMap){changes.push_back({{"step",r.ticks},{"map",map},{"stage",stage},{"visible",g.playerVisible()},{"1",s.flag(1)},{"18",s.flag(18)},{"foreground",int(g.foreground())},{"camera",{g.cameraCenter().x,g.cameraCenter().y}}});previousMap=map;previousStage=stage;render.frame("map-"+std::to_string(map)+"-stage-"+std::to_string(stage));}
  if(map==190&&s.flag(506)&&stage==5){tailVisible|=g.playerVisible();}
  if(s.variable(283)==220&&s.flag(586)&&captures==0){captures=1;atCapture=s.exportState();}
  if(map!=191){need(!s.flag(1)&&!s.flag(18),"Truth transfer released controls before consumer");}
 };
 winDora(r,cycle);const auto doraWon=r.game.session().exportState();checking=true;
 for(int n=0;n<20000&&!r.game.inputPromptView().cursor;++n){r.tick(n%8==0?key():InputFrame{});}need(r.game.inputPromptView().cursor.has_value(),"Source choice absent");
 need(findCharacter(r.game.world(),"m190-e12").motion.tile()==TilePosition{14,17},"Original rejection actor route did not reach14,17");render.frame("choice");const auto oldIdentity=PromptIdentity{r.game.generation(),r.game.inputPromptView().request};
 if(choice==1||choice==3){r.press(InputAction::Down);}InputFrame selected=key(choice>=2?InputAction::Cancel:InputAction::Confirm);if(choice==3){selected.buttons[int(InputAction::Confirm)]={true,true};}r.tick(selected);r.tick();
 for(int n=0;n<30000&&(r.game.mapId()!=191||r.game.foreground()!=ForegroundKind::None);++n){r.tick(n%8==0?key():InputFrame{});}
 auto& g=r.game;const auto& s=g.session();need(g.mapId()==191&&g.foreground()==ForegroundKind::None&&s.flag(504)&&s.flag(506)&&s.flag(586)&&s.flag(1)&&s.flag(18)&&s.variable(283)==220,"Truth did not enter actual operable Cash");need(s.variable(627)==(choice==0?5:0),"Cancel/listen branch identity lost");need(captures==1&&(cycle?(atCapture.hp==doraWon.hp&&atCapture.inventory==doraWon.inventory):(atCapture.hp!=46&&atCapture.inventory.at(5)<decodeSave(c,raw).session.inventory.at(5))),"220 reused old219 HP/items instead of actual victory consumption");
 need(moves==14,"Seven paired picture tracks not executed exactly once");
 std::vector<int> firstTicks,secondTicks;for(const auto& row:cues){const auto id=row.at("id").get<std::string>();for(int n:{166,172,178,182,189,195,201}){if(id=="m190-e12-p2-c"+std::to_string(n)){firstTicks.push_back(row.at("step"));}if(id=="m190-e12-p2-c"+std::to_string(n+1)){secondTicks.push_back(row.at("step"));}}}need(firstTicks==secondTicks&&firstTicks.size()==7,"Paired nonwait/wait picture moves serialized");for(int n=1;n<7;++n){need(firstTicks[n]-firstTicks[n-1]==(n==4?24:48),"Picture pair wait differs from source tenths");}
 if(choice==4){need(prepareAttempts==120&&!rejectCash&&contentSha256(read(root/"slot-01.json"))==contentSha256(raw),"Cash retry did not preserve exact old disk and checkpoint");}if(choice==0){need(stages==std::set<int>({0,1,2,3,4,5})&&show19==1&&show0==1&&tailVisible&&listenPlacement==1,"Long maps stage/tail/visibility absent");}else{need(stages==std::set<int>({0})&&show19==0,"Reject/Cancel entered long truth branch");}
 render.frame("natural-M191");const int retry=s.variable(630);const auto live=s.exportState();std::set<int> cashModes;bool via220=false;
 if(!cycle){for(int n=0;n<18000&&g.session().hp()>0;++n){r.tick();cashModes.insert(g.session().variable(91));}need(g.session().hp()==0&&cashModes.size()>1,"Natural M191 lacks actual damaging boss capability");
 for(int n=0;n<5000&&!g.inputPromptView().cursor;++n){r.tick(n%8==0?key():InputFrame{});}need(g.inputPromptView().cursor.has_value(),"Natural Cash Continue prompt absent");r.tick(key());
 for(int n=0;n<10000;++n){r.tick(n%8==0?key():InputFrame{});via220|=g.mapId()==220;if(via220&&g.mapId()==191&&g.foreground()==ForegroundKind::None){break;}}
 const auto normalized=[](auto values){std::erase_if(values,[](const auto& row){return row.second==0;});return values;};
 auto expectedInventory=normalized(live.inventory);
 // CN1.00 CE101 stores unequipped stock; CE102 clears1..210 and CE140
 // re-equips four slots from that stock. PF10 area3 §01:55: N -> max(N-1,0).
 for(int id=207;id<=210;++id){expectedInventory.erase(id);}
 for(int id:{live.mainWeapon,live.auxiliaryWeapon,live.armor,live.accessory}){if(id&&expectedInventory[id]>0){--expectedInventory[id];}}
 expectedInventory=normalized(expectedInventory);
 std::ofstream(root/"continue-comparison.json")<<Json{{"live_hp",live.hp},{"restored_hp",g.session().hp()},{"live_inventory",live.inventory},{"restored_inventory",g.session().exportState().inventory},{"expected_CE140_inventory",expectedInventory},{"live_magazines",live.magazines},{"restored_magazines",g.session().exportState().magazines},{"retry_before",retry},{"retry_after",g.session().variable(630)},{"504",g.session().flag(504)},{"506",g.session().flag(506)}}.dump(2)<<'\n';
 need(via220&&g.mapId()==191&&g.session().hp()==live.hp&&normalized(g.session().exportState().inventory)==expectedInventory&&g.session().exportState().magazines==live.magazines&&g.session().variable(630)==retry+1&&g.session().saveCount()==(cycle?live.saveCount:60)&&g.session().flag(504)&&g.session().flag(506),"Natural220 restore rolled back consumption/statistics or replayed Dora");}
 std::ofstream(root/"result.json")<<Json{{"choice",choice},{"steps",r.ticks},{"maps",r.maps},{"stages",stages},{"changes",changes},{"cues",cues},{"picture_moves",moves},{"listen_actor_placement",listenPlacement},{"captured_hp",atCapture.hp},{"captured_inventory",atCapture.inventory},{"magazines",atCapture.magazines},{"save_count",s.saveCount()},{"retry_count",s.variable(630)},{"via220",via220},{"reused_existing_Continue_checks",cycle},{"cash_modes",cashModes},{"frames",render.frames},{"old_choice_identity",{{"scene",oldIdentity.scene},{"request",oldIdentity.request}}},{"prepare_rejections",prepareAttempts},{"old_disk_sha256",contentSha256(read(root/"slot-01.json"))},{"ordinary_save_absent",true},{"injected_victory",false}}.dump(2)<<'\n';
 if(cashVictory){checking=false;winCash(r,render,root,cycle);if(cycle){r.settle();need(g.mapId()==195&&g.session().flag(508)&&g.session().variable(635)==1,"Cycle actual G5 aftermath absent");const int count=g.session().saveCount();g5natural::supplies(r,true);g5natural::saving(r,2,1);g5natural::saving(r,2,2);g5natural::saving(r,2);const auto disk=decodeSave(c,read(root/"slot-02.json"));need(disk.scene.map==196&&disk.session.saveCount==count+1&&disk.session.worldValues.at(c.facts.variables.at(635))==1&&read(root/"slot-01.json")==raw,"Cycle control save lost ancestry or rewrote source");std::ofstream(root/"cycle-control-result.json")<<Json{{"runtime_injection",false},{"steps",r.ticks},{"source_sha256",contentSha256(raw)},{"output_sha256",contentSha256(read(root/"slot-02.json"))},{"native",disk.session.saveCount},{"V635",1},{"map",196},{"No_and_cancel",true}}.dump(2)<<'\n';}}
 std::cout<<"03 natural choice="<<choice<<(cycle?" truth/mainline PASS steps=":" Cash/220 PASS steps=")<<r.ticks<<'\n';
}
}
int main(int argc,char** argv){try{std::cout.setf(std::ios::unitbuf);need(argc==5,"Truth package real-save root rules|save-rejections|0|1|2|3|4|all|win-listen|win-reject");auto c=loadCombatContent(argv[1]);auto raw=read(argv[2]);const bool cycle=std::string(argv[4])=="cycle-win-reject";if(cycle){need(c.compatibility=="post-f5-g7-normal-7"&&decodeSave(c,raw).session.worldValues.at(c.facts.variables.at(635))==1,"Cycle truth requires actual completed-cycle source");}else{need(contentSha256(raw)=="f87bb3d63d67803dc33916916dface8096d7ad09ef86cf3270570c93627edb62","Changed actual G3 disk");}std::filesystem::path root=argv[3];std::filesystem::create_directories(root);if(cycle){natural(c,raw,root,1,true,true);}else if(std::string(argv[4])=="rules"){rules(c,raw,root);}else if(std::string(argv[4])=="save-rejections"){saveRejections(c,raw,root);}else if(std::string(argv[4])=="all"){rules(c,raw,root/"rules");for(int choice:{0,1,2,3,4}){natural(c,raw,root/std::to_string(choice),choice);}}else if(std::string(argv[4])=="win-listen"||std::string(argv[4])=="win-reject"){natural(c,raw,root,std::string(argv[4])=="win-listen"?0:1,true);}else{natural(c,raw,root,std::stoi(argv[4]));}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
