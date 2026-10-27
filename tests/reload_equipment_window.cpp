#include "combat_presentation.h"
#include <iostream>
// Isolated equipment fixture; no natural grant and no formal save directory.
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);auto e=c.entries.at("defeated-claimed");
 e.session.mainWeapon=21;e.session.magazines[21]=0;e.session.inventory[43]=3;e.session.inventory[51]=1;
 e.session.flags[1]=true;e.session.flags[18]=true;e.playerTile={10,8};e.direction=Direction::Left;
 c.entries["54-reload-window-fixture"]=e;return runCombatGame(c,"54-reload-window-fixture",0,std::filesystem::temp_directory_path()/"nobihaza-54-window-saves");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
