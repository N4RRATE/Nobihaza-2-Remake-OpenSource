#include "title_navigation.h"
#include "game_application.h"
#include <iostream>
#include <stdexcept>
void require(bool value,const char* text){if(!value){throw std::runtime_error(text);}}
InputFrame key(InputAction a=InputAction::Confirm){InputFrame f;f.buttons[static_cast<int>(a)]={true,true,false};return f;}
void question(GameApplication& game){game.update(key());require(game.mode()==ApplicationMode::Saving,"Map22 crow begins actual save flow");for(int n=0;n<200;++n){game.update({});}}
int main(int argc,char** argv){try{
 if(argc!=4){return 2;}auto c=loadCombatContent(argv[1]);const std::filesystem::path directory=argv[2];const std::string mode=argv[3];
 if(mode=="write"){
  auto e=c.entries.at("defeated-claimed");e.map=22;e.playerTile={10,5};e.direction=Direction::Up;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.saveCount=7;e.session.variables[399]=5;e.session.quickshotTicks=800;e.session.flags[57]=true;
  c.entries["save56-fixture"]=e;
  GameApplication cancelled(c,"save56-fixture");cancelled.configureSaveDirectory(directory);finishTitleOpening(cancelled);question(cancelled);cancelled.update(key(InputAction::Down));cancelled.update(key());
  require(cancelled.mode()==ApplicationMode::Playing&&cancelled.session().saveCount()==7&&cancelled.session().variable(399)==5,"Refuse save does not change statistics");
  question(cancelled);cancelled.update(key());cancelled.update(key(InputAction::Cancel));require(cancelled.mode()==ApplicationMode::Playing&&cancelled.session().saveCount()==7,"Cancel slots leaves counts unchanged");
  for(int slot=1;slot<=2;++slot){
   auto& state=c.entries.at("save56-fixture").session;state.flags[66]=slot==2;state.inventory[61]=slot==2?1:0;state.magazines[21]=slot==2?3:8;
   GameApplication game(c,"save56-fixture");game.configureSaveDirectory(directory);finishTitleOpening(game);
   SavedPresentation media;media.music=readSound(c.expansion.mapMusic.at("22").at("sound"));game.setPresentationState(media);
   question(game);game.update(key());require(game.fileMenuView().has_value(),"Yes opens reviewed slot UI");if(slot==2){game.update(key(InputAction::Down));}game.update(key());
   require(game.mode()==ApplicationMode::Playing&&game.session().saveCount()==8&&game.session().variable(399)==6,"Current save resumes 8/6 once");
   const auto disk=SaveFiles(c,directory).read(slot);require(disk.session.saveCount==8&&disk.session.variables.at(399)==5&&disk.scene.map==22,"Disk captures Map22 at8/5");
   auto contradictory=disk;contradictory.session.inventory[61]=slot==1?1:0;bool denied=false;
   try{encodeSave(c,contradictory);}catch(const std::exception&){denied=true;}
   require(denied,"Wardrobe key and acquisition fact must agree before unlock");
   if(slot==2){for(int retained:{0,1}){
    // A has no unlock page yet. Explicit future-definition fixture, never granted in natural A.
    auto future=c;future.facts.switches[67]="fixture.wardrobe-unlocked";
    auto unlocked=disk;unlocked.session.worldFlags[future.facts.switches.at(67)]=true;unlocked.session.inventory[61]=retained;
    require(decodeSave(future,encodeSave(future,unlocked)).session.inventory.at(61)==retained,"Future legal unlock may retain or discard the key");
   }}
   require(disk.presentation.music.resource==media.music.resource&&game.session().quickshotTicks()==disk.session.quickshotTicks,"Save preserves actual music and pauses quickshot");
  }
  GameApplication failure(c,"save56-fixture");failure.configureSaveDirectory(directory/"failure",[](std::string_view stage){if(stage=="begin"){throw std::runtime_error("injected disk failure");}});question(failure);failure.update(key());failure.update(key());failure.update(key(InputAction::Cancel));failure.update(key(InputAction::Cancel));
  require(failure.mode()==ApplicationMode::Playing&&failure.session().saveCount()==7&&failure.session().variable(399)==5&&failure.session().flag(1)&&failure.session().flag(18),"Failure restores controls without counting success");
  auto closed=e;closed.session.flags[1]=false;closed.session.flags[10]=true;GameSession locked(c.items,closed.session,c.facts);CombatScene scene(c,locked,closed);bool rejected=false;
  try{captureSave(c,scene,{scene.generation(),"m22-e2",""},{});}catch(const std::exception&){rejected=true;}require(rejected,"Positive HP without live control cannot save");
 }else if(mode=="load"){
  for(int slot=1;slot<=2;++slot){
   GameApplication game(c,"title");game.configureSaveDirectory(directory);finishTitleOpening(game);game.update(key());require(game.mode()==ApplicationMode::Loading,"Fresh process enters actual title Load");if(slot==2){game.update(key(InputAction::Down));}game.update(key());
   require(game.hasGame()&&game.mapId()==22&&game.mode()==ApplicationMode::Playing,"Title commits Map22 candidate");
   require(game.session().saveCount()==8&&game.session().variable(399)==6&&game.session().count(61)==(slot==2?1:0)&&game.session().magazine(21)==(slot==2?3:8),"Independent slot progress and continuation restored");
   require(game.session().quickshotTicks()==900,"Load rebuilds original quickshot wait without charging TP");
   const auto snapshot=SaveFiles(c,directory).read(slot);std::string error;require(game.restore(snapshot,error)&&game.session().variable(399)==6,"Repeated Load does not add statistic again");
   const auto generation=game.generation();game.configureRestorePresentation([](const SceneDefinition&,const SavedPresentation&){throw std::runtime_error("missing texture");});
   require(!game.restore(snapshot,error)&&game.generation()==generation&&game.mapId()==22,"Failed preparation preserves previous candidate");
  }
 }else{return 2;}
 std::cout<<"Map22 "<<mode<<" checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
