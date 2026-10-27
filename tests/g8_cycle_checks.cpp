#include "title_navigation.h"
#include "g1_piano_driver.h"
#include "g4_dora_render.h"
#include "content_hash.h"
#include <climits>

namespace {
std::string bytes(const std::filesystem::path& path){std::ifstream in(path);need(bool(in),"Missing cycle source "+path.string());return {std::istreambuf_iterator<char>(in),{}};}
int cycleCount(const CombatContent& c,const SessionEntry& s){return s.worldValues.at(c.facts.variables.at(635));}
int nextCycle(const CombatContent& c,const SessionEntry& s){return std::min(cycleCount(c,s)+1,c.catalog.at("cycle").at("counter_max").get<int>());}
void toClearChoice(GameApplication& g){
 std::optional<MusicPlaybackIdentity> music;
 for(int n=0;n<500&&!g.inputPromptView().cursor;++n){g.update({});for(const auto& e:g.takePresentation()){if(e.musicPlayback){music=e.musicPlayback;}}if(music&&g.mapId()==215){need(g.reportMusicTicks(*music,0),"Clear clock identity rejected");}}
 need(g.mapId()==216&&g.inputPromptView().cursor,"Clear M216 choice missing");
}
void prompt(GameApplication& g){for(int n=0;n<3000&&!g.inputPromptView().cursor;++n){g.update(n%13==0?key():InputFrame{});}need(g.inputPromptView().cursor.has_value(),"Difficulty prompt missing");}
void choose(GameApplication& g,int row){for(int n=0;n<6&&g.inputPromptView().cursor->line!=row;++n){g.update(key(InputAction::Down));}need(g.inputPromptView().cursor->line==row,"Unavailable difficulty choice");g.update(key());g.update({});}
void seedPreserved(const CombatContent& c,const GameApplication& g,const SessionEntry& old){
 const auto s=g.session().exportState();
 need(g.openingActive()&&g.mapId()==2&&g.world().player().motion.tile()==TilePosition{9,7},"Cycle did not publish source M2");
 need(g.session().variable(635)==nextCycle(c,old)&&s.saveCount==old.saveCount&&g.session().variable(399)==0,"Cycle/native/statistic counts differ");
 need(s.hp==old.hp&&s.maxHp==old.maxHp&&s.tp==old.tp&&s.maxTp==old.maxTp&&s.level==old.level&&s.baseAttack==old.baseAttack&&s.baseDefense==old.baseDefense&&s.baseAgility==old.baseAgility&&s.baseSpirit==old.baseSpirit,"Actor1 pure values changed");
 auto skills=old.skills;skills.erase(3);skills.erase(4);need(s.skills==skills,"Cycle skill retention differs");
 for(int slot=1;slot<=5;++slot){need(g.session().equipped(slot)==0,"Old equipment retained");}
 for(int id=1;id<=600;++id){need(!g.session().flag(id),"Old source switch retained");}
 for(int id=1;id<=634;++id){need(g.session().variable(id)==(id==13?1:0),"Old source variable retained: "+std::to_string(id));}
 for(int id=1;id<=210;++id){int expected=0;for(const auto& reward:c.catalog.at("cycle").at("rewards")){if(reward.at("item")==id&&old.worldFlags.at(c.facts.switches.at(reward.at("flag")))){expected=1;}}need(g.session().count(id)==expected,"Cycle inventory/reward differs");}
}
void controlled(const CombatContent& c,const SaveSnapshot& real,const std::filesystem::path& out){
 Json results=Json::array();std::string error;
 for(const auto& sourceDifficulty:{"easy","hard","normal"}){
  // These legal Clear value variations isolate the entry contract; natural sources run separately.
  auto source=real;source.session.difficulty=sourceDifficulty;
  for(const auto& id:c.catalog.at("difficulty").at("easy_preseed")){source.session.worldFlags[c.facts.switches.at(id.get<int>())]=std::string(sourceDifficulty)=="easy";}
  source.session.hp=source.session.maxHp=117;source.session.tp=source.session.maxTp=35;source.session.baseDefense=7;source.session.skills.insert(7);source.session.skills.insert(4);
  validateSave(c,source);GameApplication g(c,"title");need(g.restore(source,error),"Controlled Clear restore: "+error);toClearChoice(g);const auto old=g.session().exportState();const auto generation=g.generation();int attempts=0;
  g.configureRestorePresentation([&](const SceneDefinition&,const SavedPresentation&){++attempts;throw std::runtime_error("controlled missing opening media");});g.update(key());for(int n=0;n<12&&!g.postClearRequest();++n){g.update({});}
  need(g.postClearRequest().has_value(),"Retained start request missing");const auto request=*g.postClearRequest();
  need(attempts==1&&g.generation()==generation&&g.session().exportState()==old,"Failed preparation changed Clear or source gate rejected registered difficulty: "+std::string(sourceDifficulty));
  for(int n=0;n<20;++n){g.update({});}need(attempts==1,"Failed preparation retried without input");
  need(!g.startNewCycle({request.scene+1,0},error)&&!g.startNewCycle({request.scene,1},error)&&g.session().exportState()==old,"Stale/wrong request accepted");
  g.configureRestorePresentation({});g.update(key());seedPreserved(c,g,old);need(!g.startNewCycle(request,error),"Retired Clear request accepted");
  prompt(g);g.update(key(InputAction::Cancel));g.update({});need(g.inputPromptView().cursor.has_value(),"Difficulty Cancel escaped selection");
  choose(g,0);prompt(g);g.update(key(InputAction::Cancel));g.update({});prompt(g);need(g.session().difficulty()=="normal"&&g.session().variable(635)==nextCycle(c,old),"Confirmation Cancel committed difficulty/cycle");
  choose(g,2);prompt(g);choose(g,0);prompt(g);need(g.session().difficulty()=="normal"&&g.session().variable(635)==nextCycle(c,old),"Confirmation No committed difficulty/cycle");
  const int target=std::string(sourceDifficulty)=="normal"?0:std::string(sourceDifficulty)=="easy"?2:1;choose(g,target);prompt(g);choose(g,1);prompt(g);
  const auto selected=std::array<std::string,3>{"easy","normal","hard"}.at(target);need(g.session().difficulty()==selected&&g.session().variable(280)==target+1&&g.session().variable(635)==nextCycle(c,old)&&g.session().saveCount()==old.saveCount,"Cross-difficulty confirmation changed retained domains");
  for(const auto& id:c.catalog.at("difficulty").at("easy_preseed")){need(g.session().flag(id.get<int>())==(target==0),"Old EASY preset leaked into next difficulty");}
  GameApplication bonus(c,"title");need(bonus.restore(source,error),"Bonus source rejected");toClearChoice(bonus);const auto bonusBefore=bonus.session().exportState();bonus.update(key(InputAction::Cancel));for(int n=0;n<20&&bonus.mapId()!=217;++n){bonus.update({});}need(bonus.mapId()==217&&bonus.session().variable(635)==cycleCount(c,bonusBefore)&&bonus.session().saveCount()==bonusBefore.saveCount&&bonus.session().difficulty()==sourceDifficulty,"Cancel bonus path changed cycle/count/difficulty");
  results.push_back({{"source",sourceDifficulty},{"target",selected},{"controlled_source",true},{"candidate_failure_preserves",true},{"retry_once",true},{"cancel_no_reselect",true}});
 }
 GameApplication current(c,"title");need(current.restore(real,error),"Rejection baseline failed");const auto before=current.session().exportState();const auto generation=current.generation();
 for(int variant=0;variant<7;++variant){auto bad=real;if(variant==0){bad.session.difficulty="unknown";}if(variant==1){bad.session.worldFlags[c.facts.switches.at(582)]=false;}if(variant==2){bad.session.worldValues[c.facts.variables.at(635)]=INT_MAX;}if(variant==3){bad.session.skills.insert(999);}if(variant==4){bad.session.baseDefense=1000;}if(variant==5){bad.continuation.kind="ordinary";}if(variant==6){bad.contentCompatibility="unknown";}need(!current.restore(bad,error)&&current.generation()==generation&&current.session().exportState()==before,"Corrupt/unknown Clear replaced current game");}
 auto restricted=c;restricted.catalog["difficulty"]["support"]["clear"]=Json::array({"normal"});auto unsupported=real;unsupported.session.difficulty="hard";GameApplication notDelivered(restricted,"title");need(!notDelivered.restore(unsupported,error)&&!notDelivered.hasGame(),"Undelivered Clear difficulty accepted");
 bool late=false;GameApplication failure(c,"title",safe,[&](const SceneDefinition& d){if(late&&d.backgroundPath==c.maps.at(4).backgroundPath){throw std::runtime_error("controlled late opening media failure");}});failure.configureRestorePresentation([](const SceneDefinition&,const SavedPresentation&){});need(failure.restore(real,error),"Late failure source rejected");toClearChoice(failure);failure.update(key());for(int n=0;n<12&&!failure.openingActive();++n){failure.update({});}need(failure.openingActive(),"Late failure cycle absent");late=true;prompt(failure);choose(failure,2);prompt(failure);choose(failure,1);prompt(failure);choose(failure,1);for(int n=0;n<1000&&failure.hasGame();++n){failure.update(n%13==0?key():InputFrame{});}need(!failure.hasGame()&&failure.mode()==ApplicationMode::Title&&!failure.takeSaveRequest(),"Late failure retained a half-initialized game or write request");
 std::ofstream(out/"controlled-result.json")<<Json{{"combinations",results},{"natural_Clear_evidence",false},{"bad_sources_preserve",true},{"late_failure_safe_title",true}}.dump(2)<<'\n';
 std::cout<<"G8 controlled cross-difficulty Clear candidates/confirmation/rejection/failure PASS\n";
}
struct CycleDriver:PianoDriver{
 DoraRender view;
 CycleDriver(const CombatContent& c,const std::filesystem::path& out):PianoDriver(c,out),view(game,c,out/"frames"){observe=[&](const PresentationEvent& e){view.pending.push_back(e);};afterTick=[&]{view.tick();if(auto time=view.presentation->musicTicks()){game.reportMusicTicks(time->first,time->second);}};}
 void opening(int target,bool skip){
  const auto awaitPrompt=[&]{for(int n=0;n<3000&&!game.inputPromptView().cursor;++n){tick(n%13==0?key():InputFrame{});}need(game.inputPromptView().cursor.has_value(),"Natural difficulty prompt missing");};
  const auto select=[&](int row){for(int n=0;n<6&&game.inputPromptView().cursor->line!=row;++n){tick(key(InputAction::Down));}need(game.inputPromptView().cursor->line==row,"Natural difficulty unavailable");tick(key());tick();};
  awaitPrompt();select(target);awaitPrompt();select(1);
  bool transport=false,skipArrival=false,fullArrival=false;for(int n=0;n<90000&&game.openingActive();++n){InputFrame in;const auto stage=game.openingStage();transport|=game.mapId()==87;skipArrival|=game.mapId()==4&&game.world().player().motion.tile()==TilePosition{10,8};fullArrival|=game.mapId()==4&&game.world().player().motion.tile()==TilePosition{7,4};
   if(game.inputPromptView().cursor){const int row=skip?1:0;in=key(game.inputPromptView().cursor->line==row?InputAction::Confirm:InputAction::Down);}
   else if(stage=="door-approach-common"&&!game.world().player().motion.moving()){const auto door=game.content().objects.at("m4-e2").tile;const TilePosition goal{door.x,door.y+1};if(game.world().player().motion.tile()!=goal){in.movement=next(game.world(),goal);need(in.movement.has_value(),"Opening door path missing");}else if(game.world().player().motion.facing()!=Direction::Up){in.movement=Direction::Up;}else{in=key();}}
   else if(n%13==0){in=key();}tick(in);need(game.hasGame(),"Natural new cycle opening failed");
  }need(!game.openingActive()&&game.mapId()==4&&game.session().flag(51)&&transport==!skip&&(skip?skipArrival:fullArrival),"Source skip/full opening did not finish");
 }
 void firstBattle(){
  int spent=0;for(int n=0;n<12000&&!game.session().flag(101);++n){InputFrame in;const auto p=game.world().player().motion.tile(),t=findCharacter(game.world(),"m4-e4").motion.tile();const int rounds=game.session().magazine(game.session().equipped(1));
   if(game.dialogue().active()||game.foreground()!=ForegroundKind::None){if(n%8==0){in=key();}}else if(!game.raised()){in=key(InputAction::Weapon);}else if(!game.enemyStates().empty()&&game.enemyStates().front().damage>=100){}else if((p.x!=t.x&&p.y!=t.y)||std::abs(p.x-t.x)+std::abs(p.y-t.y)>4){for(auto side:{Direction::Down,Direction::Up,Direction::Right,Direction::Left}){auto step=next(game.world(),adjacentTile(t,side));if(step){in.movement=step;break;}}}else{const int dx=t.x-p.x,dy=t.y-p.y;const auto face=std::abs(dx)>std::abs(dy)?(dx>0?Direction::Right:Direction::Left):(dy>0?Direction::Down:Direction::Up);if(game.world().player().motion.facing()!=face){in.buttons[int(InputAction::Weapon)].held=true;in.movement=face;}else if(n%12==0){in=key(InputAction::Attack);}}
   tick(in);spent+=std::max(0,rounds-game.session().magazine(game.session().equipped(1)));need(game.session().hp()>0,"Natural reward battle died");
  }need(game.session().flag(101)&&spent>0,"Natural first encounter spent no reward/weapon rounds");for(int n=0;n<3&&game.raised();++n){lower();}
 }
};
void natural(const CombatContent& c,const std::filesystem::path& sourcePath,const std::filesystem::path& out,int target,bool skip){
 const auto raw=bytes(sourcePath);const auto source=decodeSave(c,raw);const auto slots=out/"slots";std::filesystem::create_directories(slots);std::filesystem::copy_file(sourcePath,slots/"slot-02.json",std::filesystem::copy_options::overwrite_existing);
 CycleDriver r(c,out);r.game.configureSaveDirectory(slots);finishTitleOpening(r.game);r.game.returnToTitle();finishTitleOpening(r.game);r.tick(key());r.tick(key(InputAction::Down));r.tick(key());need(r.game.mapId()==215,"Independent public Clear Load rejected");for(int n=0;n<500&&!r.game.inputPromptView().cursor;++n){r.tick();}need(r.game.mapId()==216&&r.game.inputPromptView().cursor,"PCM Clear M216 absent");const auto old=r.game.session().exportState();r.view.frame("source-clear");
 r.tick(key());for(int n=0;n<12&&!r.game.openingActive();++n){r.tick();}seedPreserved(c,r.game,old);r.opening(target,skip);const auto selected=std::array<std::string,3>{"easy","normal","hard"}.at(target);
 need(r.game.session().difficulty()==selected&&r.game.session().magazine(15)==7&&r.game.session().magazine(27)==7&&r.game.session().magazine(56)==6&&r.game.session().variable(635)==nextCycle(c,old),"New difficulty/initial magazines/cycle differ");
 const bool reward=r.game.session().count(56)>0;for(const auto& item:c.catalog.at("cycle").at("rewards")){need(r.game.session().count(item.at("item"))==int(old.worldFlags.at(c.facts.switches.at(item.at("flag")))),"Natural reward retained incorrectly");}
 r.actObject("m4-e10");r.equip(reward?56:11);r.firstBattle();r.actObject("m4-e4");for(const auto* id:{"m4-e2","m6-e13","m9-e6","m10-e9","m12-e8","m13-e4","m15-e6"}){r.actObject(id);need(r.game.session().hp()>0,"Natural first-save route died");}need(r.game.mapId()==16,"First legal BSAA save not reached");r.actObject("m16-e5");need(r.game.mode()==ApplicationMode::Saving,"Natural ordinary save missing");for(int n=0;n<200;++n){r.tick();}r.tick(key());need(r.game.fileMenuView().has_value(),"Save slots missing");r.tick(key(InputAction::Down));r.tick(key(InputAction::Down));r.tick(key());if(auto v=r.game.fileMenuView();v&&v->confirmation){if(v->answer){r.tick(key(InputAction::Up));}r.tick(key());}
 need(r.game.mode()==ApplicationMode::Playing&&r.game.session().saveCount()==old.saveCount+1&&r.game.session().variable(399)==1,"First ordinary native/statistic count differs");const auto saved=SaveFiles(c,slots).read(3);need(saved.session.difficulty==selected&&saved.session.saveCount==old.saveCount+1&&saved.continuation.statisticBefore==0&&cycleCount(c,saved.session)==nextCycle(c,old),"Ordinary disk lost cross-difficulty seed");need(bytes(slots/"slot-02.json")==raw&&bytes(sourcePath)==raw,"Original Clear bytes changed");r.view.frame("ordinary-return");
 std::ofstream(out/"natural-result.json")<<Json{{"source_sha256",contentSha256(raw)},{"source_difficulty",source.session.difficulty},{"target_difficulty",selected},{"source_kind",source.continuation.kind},{"true_source_provenance_required",true},{"public_Clear_Load",true},{"public_opening",true},{"skip",skip},{"reward_SAA_used",reward},{"source_count",old.saveCount},{"saved_count",saved.session.saveCount},{"cycle",nextCycle(c,old)},{"statistic",1},{"maps",r.maps},{"steps",r.ticks},{"source_unchanged",true},{"manual_play",false}}.dump(2)<<'\n';
 std::cout<<"G8 public Clear->"<<selected<<" cycle/reward/ordinary Save PASS steps="<<r.ticks<<'\n';
}
void independent(const CombatContent& c,const std::filesystem::path& slot,const std::filesystem::path& out){
 const auto saved=decodeSave(c,bytes(slot));CycleDriver r(c,out);r.game.configureSaveDirectory(slot.parent_path());finishTitleOpening(r.game);r.load(3);const auto state=r.game.session().exportState();need(state.difficulty==saved.session.difficulty&&state.saveCount==saved.session.saveCount&&state.inventory==saved.session.inventory&&state.magazines==saved.session.magazines&&cycleCount(c,state)==cycleCount(c,saved.session)&&r.game.session().variable(399)==saved.continuation.statisticBefore+1,"Independent public Load differs from saved new cycle");std::string error;const auto before=r.game.session().exportState();need(!r.game.startNewCycle({r.game.generation(),0},error)&&r.game.session().exportState()==before,"Ordinary disk guessed Clear");r.view.frame("independent-ordinary-load");std::ofstream(out/"load-result.json")<<Json{{"difficulty",r.game.session().difficulty()},{"save_count",r.game.session().saveCount()},{"cycle",r.game.session().variable(635)},{"statistic",r.game.session().variable(399)},{"public_Load",true},{"ordinary_not_Clear",true}}.dump(2)<<'\n';
}
}
int main(int argc,char** argv){try{std::cout.setf(std::ios::unitbuf);need(argc==5||argc==7,"Expected package output checks|load|natural source [difficulty-index skip]");const auto c=loadCombatContent(argv[1]);const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);const std::string mode=argv[3];if(mode=="checks"){controlled(c,decodeSave(c,bytes(argv[4])),out);}else if(mode=="load"){independent(c,argv[4],out);}else{need(mode=="natural"&&argc==7,"Unknown cycle mode");natural(c,argv[4],out,std::stoi(argv[5]),std::stoi(argv[6])!=0);}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
