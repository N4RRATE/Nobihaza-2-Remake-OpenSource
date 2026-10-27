#include "title_navigation.h"
#include "game_application.h"
#include "full_content.h"
#include <fstream>
#include <iostream>
#include <queue>
#include <stdexcept>
namespace{
void need(bool ok,const std::string& message){if(!ok){throw std::runtime_error(message);}}
InputFrame key(InputAction action=InputAction::Confirm){InputFrame i;i.buttons[static_cast<int>(action)]={true,true,false};return i;}
void prompt(GameApplication& g){for(int n=0;n<3000&&!g.inputPromptView().cursor;++n){g.update(n%13==0?key():InputFrame{});}need(g.inputPromptView().cursor.has_value(),"Opening prompt missing");}
void choose(GameApplication& g,int index){for(int n=0;n<6&&g.inputPromptView().cursor->line!=index;++n){g.update(key(InputAction::Down));}need(g.inputPromptView().cursor->line==index,"Difficulty option disabled");g.update(key());g.update({});}
std::string bytes(const std::filesystem::path& p){std::ifstream in(p);need(bool(in),"Missing saved sample");return {std::istreambuf_iterator<char>(in),{}};}
SaveSnapshot controlled(const CombatContent& c,const std::string& difficulty){
 SessionEntry e;initializeFullSession(c,e);e.difficulty=difficulty;e.saveCount=7;e.variables[399]=5;e.variables[382]=3;e.skills={7,8,9};
 GameSession s(c.items,e,c.facts);if(difficulty=="easy"){for(int id:{130,132,137,242,382,397,405,418,538,536}){s.setFlag(id,true);}}
 s.setFlag(1,true);s.setFlag(18,true);CombatEntry entry;entry.map=16;entry.playerTile={10,21};entry.direction=Direction::Up;entry.sprite=c.maps.at(16).player.spriteId;
 CombatScene scene(c,s,entry);return captureSave(c,scene,{scene.generation(),"m16-e5","BSAA 1F入口"},{});
}
std::optional<Direction> path(const GameApplication& g,TilePosition goal){
 auto actor=g.world().player();const auto start=actor.motion.tile();std::queue<TilePosition> queue;queue.push(start);std::map<std::pair<int,int>,std::optional<Direction>> seen;seen[{start.x,start.y}]={};
 while(!queue.empty()){const auto at=queue.front();queue.pop();if(at==goal){return seen.at({at.x,at.y});}for(int d=0;d<4;++d){const auto dir=static_cast<Direction>(d);actor.motion=CharacterMotion(at,dir,32);if(!g.world().canMove(actor,dir)){continue;}const auto next=adjacentTile(at,dir);const auto key=std::pair{next.x,next.y};if(seen.contains(key)){continue;}seen[key]=seen.at({at.x,at.y}).value_or(dir);queue.push(next);}}
 throw std::runtime_error("No lawful path to opening door");
}
void frozenWorld(GameApplication& g){
 const auto state=g.session().exportState();const auto tile=g.world().player().motion.tile();g.takePresentation();InputFrame input=key();input.movement=Direction::Right;for(int n=0;n<30;++n){g.update(input);}
 need(g.session().exportState()==state&&g.world().player().motion.tile()==tile&&!g.world().player().motion.moving()&&!g.takeSaveRequest(),"Unsupported difficulty advanced the world after Load/handoff");
 const auto events=g.takePresentation();need(std::any_of(events.begin(),events.end(),[](const auto& e){return e.kind==PresentationEvent::Kind::Notice;}),"Unsupported world has no boundary feedback");g.update(key(InputAction::Cancel));need(!g.hasGame()&&g.mode()==ApplicationMode::Title,"Unsupported world cannot return to title");
}
void checkOpening(const CombatContent& c,int selection,bool skip,bool finish=false){
 const std::string difficulty=std::array<std::string,3>{"easy","normal","hard"}.at(selection);
 GameApplication g(c,"title");std::string error;need(g.newGame(error),"New Game rejected: "+error);prompt(g);
 g.update(key(InputAction::Cancel));g.update({});need(g.inputPromptView().cursor.has_value(),"Difficulty Cancel must be disabled");
 choose(g,0);prompt(g);g.update(key(InputAction::Cancel));g.update({});prompt(g);
 need(g.session().difficulty()=="normal"&&!g.session().flag(130),"Cancel changed difficulty/preset");
 choose(g,2);prompt(g);choose(g,0);prompt(g);need(g.session().difficulty()=="normal"&&!g.session().flag(130),"No changed difficulty/preset");
 choose(g,selection);prompt(g);choose(g,1);prompt(g);
 need(g.session().difficulty()==difficulty&&g.session().difficultyLevel()==selection+1&&g.session().variable(280)==selection+1&&g.session().exportState().level==1,"Confirmation identity/Actor19/V280 differs");
 for(int id:{130,132,137,242,382,397,405,418,538,536}){need(g.session().flag(id)==(selection==0),"EASY initialization differs");}
 need(g.session().variable(382)==0&&g.session().variable(394)==0,"Presets forged statistics");
 if(skip){g.update(key(InputAction::Cancel));g.update({});}else{choose(g,0);}
 bool transport=false;
 for(int n=0;n<70000&&g.openingStage()!="door-approach-common";++n){transport|=g.mapId()==87;g.update(n%13==0?key():InputFrame{});need(g.hasGame(),"Late opening failed");}
 need(g.mapId()==4&&g.openingStage()=="door-approach-common"&&transport==!skip,"Skip/full original branch failed");
 need(g.session().difficulty()==difficulty&&g.session().magazine(15)==7&&g.session().magazine(11)==15&&g.session().inParty(1),"Opening seeds/identity changed");
 const auto state=g.session().exportState();for(int n=0;n<12;++n){g.update({});}need(g.session().difficulty()==difficulty&&g.session().variable(382)==0,"Repeated ticks reapplied initialization");
 if(finish){
  const auto door=c.objects.at("m4-e2").tile;const TilePosition goal{door.x,door.y+1};
  for(int n=0;n<10000&&g.openingActive();++n){InputFrame input;if(g.openingStage()=="door-approach-common"){
   if(!g.world().player().motion.moving()){if(g.world().player().motion.tile()!=goal){input.movement=path(g,goal);}else if(g.world().player().motion.facing()!=Direction::Up){input.movement=Direction::Up;}else{input=key();}}
  }else if(n%13==0){input=key();}g.update(input);}
  need(!g.openingActive()&&g.session().flag(51),"Opening did not complete its real collapse handoff");frozenWorld(g);
 }
 std::cout<<"opening "<<difficulty<<" skip="<<skip<<" PASS\n";
}
void checks(const CombatContent& c,const std::filesystem::path& out,bool opening=true){
 if(opening){for(int difficulty=0;difficulty<3;++difficulty){for(bool skip:{true,false}){checkOpening(c,difficulty,skip);}}}
 std::filesystem::create_directories(out);
 for(const auto& difficulty:{"easy","normal","hard"}){
  std::cout<<"controlled "<<difficulty<<"\n";const auto s=controlled(c,difficulty);const auto encoded=encodeSave(c,s);std::ofstream(out/(std::string(difficulty)+".json"))<<encoded;
  auto restored=prepareRestore(c,decodeSave(c,encoded));need(restored.session->difficulty()==difficulty&&restored.session->variable(280)==(std::string(difficulty)=="easy"?1:std::string(difficulty)=="hard"?3:2)&&restored.session->saveCount()==8&&restored.session->variable(399)==6&&restored.session->variable(382)==3,"Controlled restore lost difficulty/statistics");
  auto bad=nlohmann::json::parse(encoded);bad["session"]["difficulty"]="unknown";bool rejected=false;try{decodeSave(c,bad.dump());}catch(const std::exception&){rejected=true;}need(rejected,"Unknown difficulty accepted");
  if(std::string(difficulty)!="normal"){
   auto forbidden=s;forbidden.continuation.kind="development_f5_recovery";rejected=false;try{validateSave(c,forbidden);}catch(const std::exception&){rejected=true;}need(rejected,"E/H gained development recovery");
   forbidden=s;forbidden.continuation.kind="chapter_handoff_after_file";rejected=false;try{validateSave(c,forbidden);}catch(const std::exception&){rejected=true;}need(rejected,"E/H gained Chapter before region delivery");
  }
 }
 auto normal=controlled(c,"normal");auto old=*c.legacySource;auto oldNormal=controlled(old,"normal");const auto adapted=decodeSave(c,encodeSave(old,oldNormal));need(adapted.session.difficulty=="normal"&&adapted.session.variables.at(382)==3&&adapted.session.magazines.at(15)==oldNormal.session.magazines.at(15),"Legacy adaptation rewrote identity/statistics/magazines");
 auto forged=nlohmann::json::parse(encodeSave(old,oldNormal));forged["session"]["difficulty"]="easy";bool rejected=false;try{decodeSave(c,forged.dump());}catch(const std::exception&){rejected=true;}need(rejected,"Forged EASY legacy accepted");
 GameApplication game(c,"title");std::string error;need(game.restore(normal,error),"Normal sample restore failed");const auto before=game.session().exportState();const auto generation=game.generation();
 game.configureRestorePresentation([](const SceneDefinition&,const SavedPresentation&){throw std::runtime_error("bad media");});need(!game.newGame(error)&&game.generation()==generation&&game.session().exportState()==before,"Pre-publication failure destroyed current game");
 bool late=false;GameApplication failed(c,"title",{},[&](const SceneDefinition& d){if(late&&d.backgroundPath==c.maps.at(4).backgroundPath){throw std::runtime_error("late media failure");}});failed.configureRestorePresentation([](const SceneDefinition&,const SavedPresentation&){});need(failed.newGame(error),"Late failure setup rejected");late=true;prompt(failed);choose(failed,2);prompt(failed);choose(failed,1);prompt(failed);choose(failed,1);for(int n=0;n<1000&&failed.hasGame();++n){failed.update(n%13==0?key():InputFrame{});}need(!failed.hasGame()&&failed.mode()==ApplicationMode::Title&&!failed.takeSaveRequest(),"Late opening failure retained old game/request");
 std::cout<<"Controlled three-difficulty save/legacy/kind/failure checks PASS\n";
}
void legacy(const CombatContent& c,const std::filesystem::path& list){
 std::ifstream input(list);nlohmann::json files;input>>files;
 for(const auto& file:files){std::cout<<"checking legacy "<<file<<"\n";const auto raw=bytes(file.get<std::string>());const auto old=decodeSave(*c.legacySource,raw);const auto adapted=decodeSave(c,raw);auto expected=old.session;initializeFullSession(c,expected);
  need(adapted.session==expected&&adapted.session.difficulty=="normal"&&adapted.continuation==old.continuation,"Legacy adapted state was rewritten");
  if(adapted.continuation.kind!="ordinary"||adapted.continuation.point!="m16-e5"){
   auto forbidden=adapted;forbidden.session.difficulty="hard";bool blocked=false;try{validateSave(c,forbidden);}catch(const std::exception&){blocked=true;}need(blocked,"HARD inherited an unsupported kind/point");
  }
  auto restored=prepareRestore(c,adapted);need(restored.session->difficulty()=="normal"&&!restored.scene->openingActive(),"Legacy Load restarted difficulty selection");
  auto bad=nlohmann::json::parse(raw);bad["session"]["difficulty"]="hard";bool rejected=false;try{decodeSave(c,bad.dump());}catch(const std::exception&){rejected=true;}need(rejected,"Legacy HARD forgery accepted");
  std::cout<<"legacy "<<nlohmann::json::parse(raw).at("content")<<" "<<adapted.continuation.kind<<" count="<<adapted.session.saveCount<<" PASS\n";
 }
}
void worldBoundary(const CombatContent& c){
 for(const auto& name:{"easy","hard"}){GameApplication g(c,"title");std::string error;need(g.restore(controlled(c,name),error),"World boundary restore setup failed");frozenWorld(g);}
 checkOpening(c,0,true,true);checkOpening(c,2,false,true);
 const auto movable=[](PreparedRestore& r){for(int i=0;i<4;++i){const auto direction=static_cast<Direction>(i);if(r.scene->world().canMove(r.scene->world().player(),direction)){InputFrame input;input.movement=direction;r.scene->update(input);need(r.scene->world().player().motion.moving(),"Supported world input was rejected");return;}}throw std::runtime_error("World variation has no walkable direction");};
 auto normal=prepareRestore(c,controlled(c,"normal"));movable(normal);
 auto expanded=c;expanded.catalog["difficulty"]["support"]["world"]["hard"]=nlohmann::json::array({16});auto allowed=prepareRestore(expanded,controlled(expanded,"hard"));movable(allowed);
 std::cout<<"Unsupported world public handoff/Load boundaries and data-scoped world variation PASS\n";
}

void feedback(const CombatContent& c){
 GameApplication g(c,"title");std::string error;need(g.newGame(error),"Feedback setup failed");prompt(g);choose(g,1);prompt(g);g.takePresentation();g.update(key(InputAction::Down));
 auto events=g.takePresentation();const auto cursor=readSound(c.media.at("ui").at("title").at("sounds").at("cursor"));
 need(std::any_of(events.begin(),events.end(),[&](const auto& e){return e.kind==PresentationEvent::Kind::Sound&&e.sound==cursor;}),"Opening choice cursor sound missing");
 g.update(key(InputAction::Cancel));events=g.takePresentation();const auto cancel=readSound(c.media.at("ui").at("title").at("sounds").at("cancel"));
 need(std::any_of(events.begin(),events.end(),[&](const auto& e){return e.kind==PresentationEvent::Kind::Sound&&e.sound==cancel;}),"Opening choice cancel sound missing");std::cout<<"Opening cursor/cancel cues PASS\n";
}
void independent(const CombatContent& c,const std::filesystem::path& out,const std::string& difficulty){
 const auto slot=out/difficulty/"slot-01.json";const auto raw=bytes(slot);GameApplication g(c,"title");g.configureSaveDirectory(slot.parent_path());finishTitleOpening(g);g.update(key());need(g.mode()==ApplicationMode::Loading,"Independent title Load unavailable");g.update(key());
 need(g.hasGame()&&g.mode()==ApplicationMode::Playing&&g.session().difficulty()==difficulty&&g.session().saveCount()==8&&g.session().variable(399)==6&&g.session().variable(382)==3&&g.session().exportState().level==1&&!g.openingActive(),"Independent public ordinary Load changed initialized state");
 const auto before=g.session().exportState();g.returnToTitle();finishTitleOpening(g);g.update(key());g.update(key());need(g.hasGame()&&g.session().exportState()==before&&bytes(slot)==raw,"Repeated public Load changed state/disk");if(difficulty!="normal"){frozenWorld(g);}std::cout<<"Independent "<<difficulty<<" public ordinary Load PASS\n";

}
}
int main(int argc,char** argv){try{std::cout.setf(std::ios::unitbuf);need(argc>=3,"Expected package output [load difficulty]");auto c=loadCombatContent(argv[1]);std::cout<<"Loaded G8 initialization content\n";if(argc==5){independent(c,argv[2],argv[4]);}else if(argc==4&&std::string(argv[3])=="legacy"){legacy(c,argv[2]);}else if(argc==4&&std::string(argv[3])=="world"){worldBoundary(c);}else if(argc==4&&std::string(argv[3])=="feedback"){feedback(c);}else{checks(c,argv[2],argc!=4);}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
