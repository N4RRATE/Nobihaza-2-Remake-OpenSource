#include "game_application.h"
#include "enemy_controller.h"
#include <stdexcept>
#include <iostream>
void require(bool b,const char* m){if(!b){throw std::runtime_error(m);}}
InputFrame press(InputAction a){InputFrame f;f.buttons[static_cast<int>(a)]={true,true,false};return f;}
int main(int argc,char** argv){
 if(argc!=2){return 2;}auto content=loadCombatContent(argv[1]);
 auto& entry=content.entries.at("post-pickup");entry.session.auxiliaryWeapon=24;entry.session.mainWeapon=20;entry.session.flags[47]=true;
 entry.positions["m4-e4"]={15,10};entry.session.flags[52]=false;entry.session.flags[1]=true;entry.session.flags[18]=true;
 GameApplication knife(content,"post-pickup");knife.update(press(InputAction::AuxiliaryWeapon));
 for(int n=0;n<100;++n){knife.update({});}
 require(knife.raised(),"equipped auxiliary can be raised");
 const auto standing=knife.world().player().spriteId;knife.update(press(InputAction::Attack));
 bool struck=false;InputFrame held;held.buttons[static_cast<int>(InputAction::Attack)].held=true;
 for(int n=0;n<160;++n){knife.update(held);struck|=knife.world().player().spriteId!=standing;}
 require(struck&&knife.world().player().spriteId==standing,"one press completes one knife strike while held");
 require(knife.session().magazine(20)==6&&knife.session().tp()==30&&knife.session().flag(47),"knife uses no ammunition TP or precision");
 InputFrame auxHold;auxHold.buttons[static_cast<int>(InputAction::Weapon)].held=true;
 for(int n=0;n<8;++n){knife.update(auxHold);}for(int n=0;n<50;++n){knife.update({});}
 require(!knife.raised(),"auxiliary short re-press lowers knife");
 GameApplication magnum(content,"post-pickup");magnum.update(press(InputAction::Weapon));
 for(int n=0;n<100;++n){magnum.update({});}
 magnum.update(press(InputAction::Attack));require(magnum.session().magazine(20)==5&&magnum.session().magazine(11)==15,"magnum has independent magazine");
 auto handgun=content;handgun.entries.at("post-pickup").session.mainWeapon=11;GameApplication combinedAim(handgun,"post-pickup");
 combinedAim.update(press(InputAction::Weapon));for(int n=0;n<100;++n){combinedAim.update({});}
 InputFrame combinedHold;combinedHold.buttons[static_cast<int>(InputAction::AuxiliaryWeapon)].held=true;
 for(int n=0;n<180;++n){combinedAim.update(combinedHold);}
 require(combinedAim.aiming(),"already raised handgun uses the same right-button hold across modifier changes");
 auto dual=content;auto& d=dual.entries.at("post-pickup");d.map=10;d.playerTile={8,19};d.direction=Direction::Up;
 d.pages.clear();d.positions.clear();d.through.clear();d.directions.clear();
 d.positions["m10-e3"]={8,18};d.positions["m10-e5"]={8,18};d.pages["m10-e3"]=8;d.pages["m10-e5"]=8;d.through["m10-e3"]=true;d.through["m10-e5"]=true;d.directions["m10-e3"]=Direction::Down;d.directions["m10-e5"]=Direction::Down;
 GameSession ks(dual.items,d.session);CombatScene knives(dual,ks,"post-pickup",[](int lo,int){return lo;});
 std::set<std::string> ids;
 require(!knives.resolveShot({145,296},false,0,false,ids,AttackKind::Knife)&&ks.variable(35)==0,"knife nine pixels outside does not increment hits");
 require(knives.resolveShot({144,296},false,0,false,ids,AttackKind::Knife)&&ids.size()==2,"knife inclusive eight-pixel boundary visits both targets");
 require(knives.enemyStates()[0].damage==11&&knives.enemyStates()[1].damage==11&&ks.variable(35)==2&&ks.flag(47),"independent knife damage uses effective agility without spending precision");
 require(!knives.resolveShot({136,296},false,0,false,ids,AttackKind::Knife),"one attack cannot hit a target twice");
 GameSession ms(dual.items,d.session);CombatScene bullets(dual,ms,"post-pickup");ids.clear();
 require(bullets.resolveShot({149,296},false,0,false,ids,AttackKind::Magnum)&&ids.size()==2,"magnum inclusive thirteen-pixel flight reaches both targets");
 require(ms.variable(35)==0&&ms.flag(47)&&bullets.enemyStates()[0].damage==150&&bullets.enemyStates()[1].damage==150,"magnum P5 bypasses CE11 stats and precision");
 auto gun=dual;gun.entries.at("post-pickup").session.mainWeapon=11;GameSession gs(gun.items,gun.entries.at("post-pickup").session);CombatScene precision(gun,gs,"post-pickup");ids.clear();
 require(precision.resolveShot({136,296},true,0,false,ids)&&!gs.flag(47),"actual handgun hit consumes precision");
 require(precision.enemyStates()[0].damage==150&&precision.enemyStates()[1].damage<150,"precision only affects first eligible target");
 std::cout<<"F3 weapon checks passed\n";
}
