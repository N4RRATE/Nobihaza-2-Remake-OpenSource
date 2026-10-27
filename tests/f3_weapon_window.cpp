#include "combat_presentation.h"
// Explicit visual fixture: unrelated to New Game or natural inventory acquisition.
int main(int argc,char** argv){
 if(argc!=3){return 2;}auto c=loadCombatContent(argv[1]);auto e=c.entries.at("defeated-claimed");
 e.session.mainWeapon=std::string(argv[2])=="magnum"?20:11;e.session.auxiliaryWeapon=24;
 const std::string weapon=argv[2];
 if(weapon=="sg"||weapon=="lc9"||weapon=="grenade"){
  e.map=28;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.mainWeapon=weapon=="sg"?12:11;e.session.auxiliaryWeapon=weapon=="grenade"?29:27;
  e.session.inventory[12]=1;e.session.inventory[27]=1;e.session.inventory[29]=5;e.session.inventory[31]=50;e.session.magazines[12]=13;e.session.magazines[27]=7;
 }
 e.playerTile={10,8};e.direction=Direction::Left;e.session.flags[1]=true;e.session.flags[18]=true;
 c.entries["F3-38-weapon-fixture"]=e;return runCombatGame(c,"F3-38-weapon-fixture",18000);
}
