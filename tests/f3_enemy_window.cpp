#include "combat_presentation.h"
// Native dual-enemy fixture, explicitly separate from the natural route acceptance.
int main(int argc,char** argv){
    if(argc!=2){return 2;}auto content=loadCombatContent(argv[1]);
    auto entry=content.entries.at("post-pickup");entry.map=10;entry.playerTile={5,20};entry.direction=Direction::Up;
    entry.pages.clear();entry.positions.clear();entry.through.clear();entry.directions.clear();
    entry.positions["m10-e3"]={5,18};entry.positions["m10-e5"]={6,17};
    entry.pages["m10-e3"]=8;entry.pages["m10-e5"]=8;entry.through["m10-e3"]=false;entry.through["m10-e5"]=false;
    entry.directions["m10-e3"]=Direction::Down;entry.directions["m10-e5"]=Direction::Down;
    entry.session.mainWeapon=11;entry.session.inventory.erase(11);entry.session.hp=500;entry.session.maxHp=500;
    content.entries["F3-34-dual-enemy-fixture"]=entry;
    return runCombatGame(content,"F3-34-dual-enemy-fixture",10800);
}
