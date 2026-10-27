#include "combat_presentation.h"
#include <iostream>
// Isolated entry fixture; actual Map23 transfer is checked separately.
int main(int argc,char** argv){try{
 if(argc!=2)return 2;auto c=loadCombatContent(argv[1]);auto e=c.entries.at("defeated-claimed");
 e.map=24;e.playerTile={13,5};e.direction=Direction::Down;e.session.flags[73]=false;
 c.entries["61-meeting-window-fixture"]=e;
 return runCombatGame(c,"61-meeting-window-fixture",0,std::filesystem::temp_directory_path()/"nobihaza-61-window-saves");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
