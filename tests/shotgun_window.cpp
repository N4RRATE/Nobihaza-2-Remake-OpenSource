#include "combat_presentation.h"
#include <iostream>
// Explicit weapon/media fixture; the natural pickup remains the optional cabinet.
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);auto e=c.entries.at("defeated-claimed");
 e.session.mainWeapon=14;e.session.magazines[14]=5;e.session.inventory[34]=1;e.session.inventory[51]=1;
 e.session.flags[1]=true;e.session.flags[18]=true;e.playerTile={10,8};e.direction=Direction::Left;
 c.entries["59-shotgun-window-fixture"]=e;
 return runCombatGame(c,"59-shotgun-window-fixture",0,std::filesystem::temp_directory_path()/"nobihaza-59-window-saves");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
