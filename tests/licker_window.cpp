#include "combat_presentation.h"
#include <iostream>
int main(int argc,char** argv){try{
    if(argc!=2){return 2;}auto content=loadCombatContent(argv[1]);content.pendingMapTickets.erase(19);
    auto entry=content.entries.at("defeated-claimed");entry.map=19;entry.playerTile={20,16};entry.direction=Direction::Up;
    entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();
    entry.session.flags[57]=true;entry.session.flags[105]=false;entry.session.hp=100;entry.session.maxHp=100;
    entry.session.baseDefense=999;entry.directions["m19-e2"]=Direction::Down;
    content.entries["52-licker-window-fixture"]=entry;
    return runCombatGame(content,"52-licker-window-fixture",0,std::filesystem::temp_directory_path()/"nobihaza-52-window-saves");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
