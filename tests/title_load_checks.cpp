#include "title_navigation.h"
#include "game_application.h"
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <chrono>
void require(bool b,const char* m){if(!b){throw std::runtime_error(m);}}
InputFrame press(InputAction a){InputFrame f;f.buttons[static_cast<int>(a)]={true,true,false};return f;}
int main(int argc,char** argv){
 if(argc==4){
  auto content=loadCombatContent(argv[1]);const std::filesystem::path root=argv[2];std::filesystem::create_directories(root);
  std::ifstream source(argv[3]);const std::string raw{std::istreambuf_iterator<char>(source),{}};require(!raw.empty(),"Missing representative Load source");const auto saved=decodeSave(content,raw);std::ofstream(root/"slot-01.json")<<raw;
  GameApplication game(content,"title");game.configureSaveDirectory(root);finishTitleOpening(game);require(game.menuSelection()==1&&game.titleLoadEnabled(),"Representative save did not select enabled Load");game.update(press(InputAction::Confirm));require(game.mode()==ApplicationMode::Loading,"Representative source did not open Load");game.update(press(InputAction::Confirm));require(game.hasGame()&&game.mode()==ApplicationMode::Playing,"Representative source failed public Load");
  const auto expected=prepareRestore(content,saved);require(game.mapId()==expected.scene->mapId()&&game.session().exportState()==expected.session->exportState(),"Representative Load changed restored map/session");
  game.returnToTitle();finishTitleOpening(game);game.update(press(InputAction::Confirm));game.update(press(InputAction::Confirm));require(game.hasGame()&&game.session().exportState()==expected.session->exportState(),"Representative repeated Load changed state");
  std::ifstream file(root/"slot-01.json");const std::string after{std::istreambuf_iterator<char>(file),{}};require(after==raw,"Representative Load rewrote source bytes");std::cout<<"Representative public title Load twice PASS kind="<<saved.continuation.kind<<" map="<<game.mapId()<<'\n';return 0;
 }
 if(argc!=3){return 2;}auto c=loadCombatContent(argv[1]);GameApplication title(c,"title");title.configureSaveDirectory(argv[2]);require(!title.hasGame()&&title.mode()==ApplicationMode::Title,"title owns no fake session");
 finishTitleOpening(title);title.update(press(InputAction::Confirm));require(title.mode()==ApplicationMode::Loading,"Load opens fifteen slots");title.update(press(InputAction::Confirm));require(title.hasGame()&&title.mode()==ApplicationMode::Playing&&title.mapId()==16,"valid slot commits candidate");
 require(title.session().saveCount()==8&&title.session().variable(399)==6,"Load resumes saved statistics");const auto old=title.generation();
 auto save=SaveFiles(c,argv[2]).read(1);title.configureRestorePresentation([](const SceneDefinition&,const SavedPresentation&){throw std::runtime_error("missing texture");});
 std::string error;require(!title.restore(save,error)&&title.hasGame()&&title.generation()==old&&title.session().saveCount()==8,"failed preparation preserves old active game");
 auto changed=c;save.scene.positions["m16-e2"]=adjacentTile(save.scene.playerTile,save.scene.direction);
 int normalPreparations=0;GameApplication travel(changed,"title",{},[&](const SceneDefinition&){++normalPreparations;});
 travel.configureRestorePresentation([](const SceneDefinition&,const SavedPresentation&){});require(travel.restore(save,error),"travel Load candidate prepared");
 require(normalPreparations==0,"Load does not publish ordinary transfer resources");
 travel.update(press(InputAction::Confirm));for(int i=0;i<300&&travel.mapId()==16;++i){travel.update({});}
 require(travel.mapId()==15&&normalPreparations==1,"Load retains preparation callback for later normal transfer");
 title.returnToTitle();require(!title.hasGame()&&title.generation()==0&&title.dialogue().view().phase==DialoguePhase::Hidden,"title destroys scene and pending requests");
 require(title.menuSelection()==1&&title.titleLoadEnabled()&&title.titleOpeningTicks()==0,"Return with saves must select Load and restart opening");
 const auto statusRoot=std::filesystem::path(argv[2])/("title-status-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 title.configureSaveDirectory(statusRoot/"absent");finishTitleOpening(title);require(!title.titleLoadEnabled()&&title.menuSelection()==0,"Missing directory should disable Load and select New Game");
 title.update(press(InputAction::Down));title.update(press(InputAction::Confirm));require(title.mode()==ApplicationMode::Title&&!title.quitRequested(),"Disabled Load must reject confirmation");
 auto events=title.takePresentation();require(std::any_of(events.begin(),events.end(),[&](const auto& e){return e.kind==PresentationEvent::Kind::Sound&&e.sound.resource==readSound(c.media.at("ui").at("title").at("sounds").at("buzzer")).resource;}),"Disabled Load lost buzzer feedback");
 std::filesystem::create_directories(statusRoot/"bad");std::ofstream(statusRoot/"bad/slot-01.json")<<"damaged";
 title.configureSaveDirectory(statusRoot/"bad");finishTitleOpening(title);require(title.titleLoadEnabled()&&title.menuSelection()==1,"Corrupt artifact must remain reachable without decoding on title");
 title.update(press(InputAction::Confirm));title.update(press(InputAction::Confirm));require(title.fileMenuView()&&title.fileMenuView()->prompt.find("原文件")!=std::string::npos,"Corrupt slot feedback lost");
 title.update(press(InputAction::Cancel));title.update(press(InputAction::Cancel));require(title.mode()==ApplicationMode::Title&&title.menuSelection()==1&&title.titleOpeningTicks()==8,"Load cancel must retain selection and fully open title");
 for(const auto* suffix:{".backup.json",".pending.json",".transaction.json",".recover.pending.json",".unknown"}){
  const auto folder=statusRoot/suffix;std::filesystem::create_directories(folder);std::ofstream(folder/(std::string("slot-01")+suffix))<<"damaged";
  title.configureSaveDirectory(folder);require(title.titleLoadEnabled()&&title.menuSelection()==1,"Recovery/unknown slot artifact hidden by title");
 }
 std::ofstream(statusRoot/"not-directory")<<"protected";title.configureSaveDirectory(statusRoot/"not-directory");require(title.titleLoadEnabled(),"Invalid directory must retain protected Load diagnostics");
 title.update(press(InputAction::Confirm));require(title.mode()==ApplicationMode::Loading&&title.fileMenuView(),"Invalid-root Load must show diagnostics rather than crash");title.update(press(InputAction::Cancel));
 title.configureSaveDirectory(statusRoot/"empty");title.returnToTitle();finishTitleOpening(title);title.update(press(InputAction::Up));title.update(press(InputAction::Confirm));require(title.quitRequested(),"Exit navigation lost");
 std::cout<<"Title Load checks passed\n";
}
