#include "game_application.h"
#include <stdexcept>
#include <iostream>
void require(bool b,const char* m){if(!b){throw std::runtime_error(m);}}
InputFrame press(InputAction a){InputFrame f;f.buttons[static_cast<int>(a)]={true,true,false};return f;}
int main(int argc,char** argv){
 if(argc!=3){return 2;}auto c=loadCombatContent(argv[1]);auto& e=c.entries.at("defeated-claimed");e.map=16;e.playerTile={10,21};e.direction=Direction::Up;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.saveCount=7;e.session.variables[399]=5;e.session.quickshotTicks=800;
 GameApplication game(c,"defeated-claimed");game.configureSaveDirectory(argv[2]);game.update(press(InputAction::Confirm));require(game.mode()==ApplicationMode::Saving,"crow requests actual save flow");
 for(int i=0;i<200;++i){game.update({});}game.update(press(InputAction::Down));game.update(press(InputAction::Confirm));require(game.mode()==ApplicationMode::Playing&&game.session().saveCount()==7&&game.session().variable(399)==5,"original no leaves counters unchanged");
 game.update(press(InputAction::Confirm));for(int i=0;i<200;++i){game.update({});}game.update(press(InputAction::Confirm));require(game.fileMenuView().has_value(),"original yes opens slots");game.update(press(InputAction::Cancel));require(game.mode()==ApplicationMode::Playing&&game.session().saveCount()==7,"slot cancel unchanged");
 game.update(press(InputAction::Confirm));for(int i=0;i<200;++i){game.update({});}game.update(press(InputAction::Confirm));game.update(press(InputAction::Confirm));require(game.mode()==ApplicationMode::Playing&&game.session().saveCount()==8&&game.session().variable(399)==6,"committed save resumes exactly once");
 SaveFiles files(c,argv[2]);auto disk=files.read(1);require(disk.session.saveCount==8&&disk.session.variables.at(399)==5&&game.session().quickshotTicks()==disk.session.quickshotTicks,"disk holds original continuation and UI pauses effect");
 GameApplication failed(c,"defeated-claimed");failed.configureSaveDirectory(std::filesystem::path(argv[2])/"failed",[](std::string_view stage){if(stage=="begin"){throw std::runtime_error("disk error");}});failed.update(press(InputAction::Confirm));for(int i=0;i<200;++i){failed.update({});}failed.update(press(InputAction::Confirm));failed.update(press(InputAction::Confirm));failed.update(press(InputAction::Cancel));failed.update(press(InputAction::Cancel));
 require(failed.mode()==ApplicationMode::Playing&&failed.session().saveCount()==7&&failed.session().variable(399)==5&&failed.session().flag(1)&&failed.session().flag(18),"write error returns without successful-save count");
 require(decodeUiText("C:\\Users\\save") == U"C:\\Users\\save","plain UI errors preserve backslashes");
 GameApplication interrupted(c,"defeated-claimed");interrupted.configureSaveDirectory(std::filesystem::path(argv[2])/"interrupted",[](std::string_view stage){if(stage=="after-replace"){throw std::runtime_error("ambiguous publish");}});
 interrupted.update(press(InputAction::Confirm));for(int i=0;i<200;++i){interrupted.update({});}interrupted.update(press(InputAction::Confirm));interrupted.update(press(InputAction::Confirm));
 interrupted.update(press(InputAction::Cancel));interrupted.update(press(InputAction::Confirm));interrupted.update(press(InputAction::Confirm));interrupted.update(press(InputAction::Up));interrupted.update(press(InputAction::Confirm));
 require(interrupted.mode()==ApplicationMode::Playing&&interrupted.session().saveCount()==8&&interrupted.session().variable(399)==6,"explicit recovery of current committed candidate resumes current save");
 require(game.world().player().motion.tile()==TilePosition{10,21}&&!game.raised(),"return consumes input edge");
 InputBuffer held;held.press(InputAction::Confirm);game.update(held.consume());
 require(game.mode()==ApplicationMode::Saving,"fresh confirmation after save can request save again");
 for(int i=0;i<200;++i){game.update(held.consume());}
 held.release(InputAction::Confirm);game.update(held.consume());held.press(InputAction::Confirm);game.update(held.consume());
 require(game.fileMenuView().has_value(),"fresh Yes opens save slots");
 held.release(InputAction::Confirm);held.press(InputAction::Down);game.update(held.consume());held.release(InputAction::Down);game.update(held.consume());held.press(InputAction::Confirm);game.update(held.consume());
 require(game.mode()==ApplicationMode::Playing&&game.session().saveCount()==9,"slot confirmation commits one save");
 for(int i=0;i<8;++i){game.update(held.consume());}
 require(game.mode()==ApplicationMode::Playing&&game.foreground()==ForegroundKind::None&&game.session().saveCount()==9,"held closing Z does not reopen save or repeat its commit in catch-up steps");
 held.release(InputAction::Confirm);game.update(held.consume());held.press(InputAction::Cancel);game.update(held.consume());
 require(game.mode()==ApplicationMode::Equipment,"menu opens at the same save object");
 held.release(InputAction::Cancel);game.update(held.consume());held.press(InputAction::Confirm);held.press(InputAction::Cancel);game.update(held.consume());
 require(game.mode()==ApplicationMode::Playing,"menu close consumes simultaneous Z and X");
 for(int i=0;i<8;++i){game.update(held.consume());}
 require(game.mode()==ApplicationMode::Playing&&game.foreground()==ForegroundKind::None&&game.session().saveCount()==9,"held menu Z cannot investigate the save object after closing");
 held.release(InputAction::Confirm);held.release(InputAction::Cancel);game.update(held.consume());held.press(InputAction::Confirm);game.update(held.consume());
 require(game.mode()==ApplicationMode::Saving,"new press after menu close still investigates normally");
 std::cout<<"Save flow checks passed\n";
}
