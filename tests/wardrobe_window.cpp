#include "combat_presentation.h"
#include <iostream>
// Only positions a keyed test actor at the wardrobe. No natural rewards or saves.
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);auto e=c.entries.at("defeated-claimed");
 e.map=11;e.playerTile={6,6};e.direction=Direction::Up;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();
 e.session.inventory[61]=1;e.session.flags[66]=true;e.session.flags[104]=true; // Already-defeated room, for deliberate UI inspection.
 c.entries["57-wardrobe-window-fixture"]=e;
 return runCombatGame(c,"57-wardrobe-window-fixture",0,std::filesystem::temp_directory_path()/"nobihaza-57-window-saves");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
