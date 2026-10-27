#include "combat_presentation.h"
#include <iostream>
// Reviewed ladder position fixture. Z performs real climbing, then X opens items.
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);auto e=c.entries.at("defeated-claimed");
 e.map=12;e.playerTile={28,8};e.direction=Direction::Up;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();
 e.positions["m12-e9"]={28,7};e.session.flags[66]=e.session.flags[67]=e.session.flags[68]=true;e.session.inventory[62]=1;
 c.entries["58-tool-window-fixture"]=e;
 return runCombatGame(c,"58-tool-window-fixture",0,std::filesystem::temp_directory_path()/"nobihaza-58-window-saves");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
