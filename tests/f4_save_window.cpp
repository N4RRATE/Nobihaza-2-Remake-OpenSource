// Isolated Map22 fixture; no player-accessible arbitrary save ability.
#include "combat_presentation.h"
#include <iostream>
int main(int argc,char** argv){try{
 if(argc!=4){return 2;}auto content=loadCombatContent(argv[1]);std::string entry=argv[3];
 if(entry=="save"){
  auto e=content.entries.at("defeated-claimed");e.map=22;e.playerTile={10,5};e.direction=Direction::Up;
  e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.flags[57]=true;
  content.entries["56-window-fixture"]=e;entry="56-window-fixture";
 }
 return runCombatGame(content,entry,0,argv[2]);
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
