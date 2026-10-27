#include "combat_scene.h"
#include "enemy_controller.h"
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* message){if(!ok){throw std::runtime_error(message);}}
InputFrame press(InputAction a){InputFrame f;f.buttons[static_cast<int>(a)].pressed=true;return f;}
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}const auto c=loadCombatContent(argv[1]);
 for(int item:{12,27}){
  auto e=c.entries.at("defeated-claimed");e.map=28;e.playerTile={5,5};e.direction=Direction::Right;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.mainWeapon=e.session.auxiliaryWeapon=0;e.session.inventory[item]=1;e.session.magazines[item]=item==12?13:7;
  GameSession s(c.items,e.session,c.facts);require(s.equip(item,item==12?1:2),"Both firearms have executable bindings");CombatScene room(c,s,e);room.update(press(item==12?InputAction::Weapon:InputAction::AuxiliaryWeapon));for(int n=0;n<100;++n){room.update({});}require(room.raised(),"Firearm raises");
  auto input=press(InputAction::Attack);input.buttons[static_cast<int>(InputAction::Attack)].held=true;room.update(input);input.buttons[static_cast<int>(InputAction::Attack)].pressed=false;
  for(int n=0;n<250;++n){room.update(input);}require(item==12?s.magazine(item)<12:s.magazine(item)==6,"SG repeats held fire; LC9 requires a new press");
 }
 for(auto [distance,expected]:{std::pair{0,32},std::pair{6,27},std::pair{10,14},std::pair{12,8}}){require(rollWeaponDamage(AttackKind::Sg,35,3,distance,[](int,int){return 5;}).amount==expected,"SG source distance bands");}
 for(auto [distance,expected]:{std::pair{0,20},std::pair{7,20},std::pair{8,16},std::pair{12,8}}){require(rollWeaponDamage(AttackKind::Lc9,23,3,distance,[](int,int){return 5;}).amount==expected,"LC9 source distance bands");}
 {
  auto e=c.entries.at("defeated-claimed");e.map=32;e.playerTile={3,12};e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.mainWeapon=12;e.session.variables[35]=1;e.session.flags[88]=true;e.session.flags[119]=false;
  GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e,[](int lo,int hi){return lo==0&&hi==10?2:lo;});auto dog=makeEnemyBehavior(scene,c.enemies.at("m32-e2"));dog->activateState(1);
  auto p=findCharacter(scene.world(),dog->id()).motion.position();auto hit=dog->receiveAttack({{(p.x+.5)*16,(p.y+.5)*16},{0,0},AttackKind::Sg,2,0,false});require(hit.processed&&hit.endRequested,"Canine synchronous CE11 forwards SG end request");
 }
 std::cout<<"SG/LC9 input and source damage passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
