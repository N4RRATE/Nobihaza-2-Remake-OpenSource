#include "combat_presentation.h"
// Native presentation fixture: a reviewed after-battle snapshot placed at the left corridor exit.
// This bypasses the earlier playable path; it is not a natural-route acceptance test.
int main(int argc,char** argv){
    if(argc!=2){return 2;}auto content=loadCombatContent(argv[1]);
    auto entry=content.entries.at("defeated-claimed");entry.map=6;entry.playerTile={20,3};entry.direction=Direction::Up;
    entry.pages.clear();entry.positions.clear();entry.through.clear();entry.directions.clear();
    content.entries["F3-33-transfer-fixture"]=entry;
    return runCombatGame(content,"F3-33-transfer-fixture",3600);
}
