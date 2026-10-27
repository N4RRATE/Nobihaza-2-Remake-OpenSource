#include "combat_presentation.h"
// Explicit visual fixtures. They do not constitute natural-route or player-feel acceptance.
int main(int argc,char** argv){
 if(argc!=3){return 2;}auto c=loadCombatContent(argv[1]);auto e=c.entries.at("defeated-claimed");
 e.pages.clear();e.positions.clear();e.through.clear();e.directions.clear();
 const std::string role=argv[2];
 if(role=="hall"){e.map=15;e.playerTile={10,6};e.direction=Direction::Up;e.session.flags[58]=false;}
 else if(role=="ladder"){e.map=12;e.playerTile={30,8};e.direction=Direction::Right;}
 else if(role=="pictures"){e.map=14;e.playerTile={21,14};e.direction=Direction::Up;}
 else if(role=="music"){e.map=9;e.playerTile={4,7};e.direction=Direction::Up;e.session.flags[55]=false;}
 else if(role=="key-choice"){e.map=13;e.playerTile={23,15};e.direction=Direction::Up;e.session.inventory[60]=1;e.session.flags[59]=true;}
 else if(role=="password"){e.map=14;e.playerTile=adjacentTile(c.objects.at("m14-e10").tile,Direction::Down);e.direction=Direction::Up;e.session.variables[388]=4321;}
 else if(role=="reports"){e.map=18;e.playerTile=adjacentTile(c.objects.at("m18-e11").tile,Direction::Down);e.direction=Direction::Up;}
 else{return 2;}
 if(role=="key-choice"||role=="password"||role=="reports"){for(const auto& [id,enemy]:c.enemies){if(!enemy.story){e.session.flags[enemy.deadFlag]=true;}}}
 c.entries["F3-35-visual-fixture"]=e;return runCombatGame(c,"F3-35-visual-fixture",15000);
}
