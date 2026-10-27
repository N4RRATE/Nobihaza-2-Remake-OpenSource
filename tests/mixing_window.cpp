#include "combat_presentation.h"
#include <iostream>
// Explicit local fixture; natural skill teaching remains ticket61.
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);auto e=c.entries.at("defeated-claimed");
 e.session.skills={3};e.session.inventory[2]=2;e.session.inventory[4]=2;e.session.inventory[5]=0;
 c.entries["60-mixing-window-fixture"]=e;
 return runCombatGame(c,"60-mixing-window-fixture",0,std::filesystem::temp_directory_path()/"nobihaza-60-window-saves");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
