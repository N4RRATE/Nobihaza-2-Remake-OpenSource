#include "combat_presentation.h"
#include <iostream>
// Isolated conference fixture; natural eight-talk gating has separate checks.
int main(int argc,char** argv){try{
 if(argc<2||argc>3)return 2;auto c=loadCombatContent(argv[1]);auto e=c.entries.at("defeated-claimed");
 e.map=24;e.playerTile={11,5};e.direction=Direction::Down;e.session.skills.insert(3);
 for(int flag:{73,75,76,77,78,79,80,81,82})e.session.flags[flag]=true;
 e.positions["m24-e2"]={11,6};e.positions["m24-e3"]={11,7};e.positions["m24-e10"]={10,5};e.positions["m24-e5"]={14,5};
 if(argc==3){e.map=12;e.playerTile={19,17};e.direction=Direction::Right;e.positions.clear();e.session.skills.erase(3);e.session.skills.insert(4);e.session.flags[1]=false;e.session.flags[18]=false;e.session.flags[13]=true;}
 c.entries["62-conference-window-fixture"]=e;
 return runCombatGame(c,"62-conference-window-fixture",0,std::filesystem::temp_directory_path()/"nobihaza-62-window-saves");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
