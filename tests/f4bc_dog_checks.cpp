#include "combat_scene.h"
#include "enemy_controller.h"
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* why){if(!ok){throw std::runtime_error(why);}}
CombatEntry fixture(const CombatContent& c,int map){auto e=c.entries.at("defeated-claimed");e.map=map;e.playerTile={3,12};e.direction=Direction::Up;e.pages.clear();e.positions.clear();e.through.clear();e.directions.clear();e.session.flags[88]=e.session.flags[89]=e.session.flags[91]=true;e.session.mainWeapon=11;return e;}
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}const auto c=loadCombatContent(argv[1]);
 for(const auto& id:{"m31-e6","m31-e8","m31-e10","m31-e16","m32-e2"}){require(c.enemies.contains(id),"Five real canine instances registered");const auto& d=c.enemies.at(id);auto e=fixture(c,d.map);GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,[](int lo,int){return lo;});auto dog=makeEnemyBehavior(scene,d);dog->activateState(1);const auto actor=findCharacter(scene.world(),id);auto result=dog->receiveAttack({GridPosition{(actor.motion.position().x+.5)*16,(actor.motion.position().y+.5)*16},{1,1},AttackKind::Magnum,5,0,false});require(result.processed&&dog->accumulatedDamage()==150,"P5 direct death does not call CE11");for(int t=0;t<500;++t){dog->update({});}require(s.flag(d.deadFlag)&&dog->accumulatedDamage()==1600,"Critical canine corpse uses1600 not2100");}
 auto e=fixture(c,32);e.session.flags[119]=true;GameSession dead(c.items,e.session,c.facts);CombatScene storage(c,dead,e);require(storage.enemyStates().front().mode==EnemyMode::Corpse,"Shared death prevents storage respawn");e.map=31;CombatScene dining(c,dead,e);for(const auto& d:dining.enemyStates()){if(d.id=="m31-e16"){require(d.mode==EnemyMode::Corpse,"Shared S119 prevents dining respawn");}}
 for(const auto& id:{"m32-e2","m31-e16","m31-e6"}){
  const auto& d=c.enemies.at(id);auto e=fixture(c,d.map);e.session.mainWeapon=0;e.session.baseAttack=20;e.session.variables[35]=0;
  GameSession session(c.items,e.session,c.facts);session.setVariable(49,9);CombatScene scene(c,session,e,[](int lo,int hi){return lo==0&&hi==10?2:lo;});auto dog=makeEnemyBehavior(scene,d);dog->activateState(1);
  auto p=findCharacter(scene.world(),id).motion.position();const AttackSample hit{{(p.x+.5)*16,(p.y+.5)*16},{0,0},AttackKind::Handgun,4,0,false};
  require(dog->receiveAttack(hit).processed&&dog->accumulatedDamage()==0,"Blow damage waits for state page");dog->update({});
  std::cout<<id<<" attack="<<session.attack()<<" damage="<<dog->accumulatedDamage()<<" defense="<<session.variable(49)<<std::endl;
  require(dog->accumulatedDamage()==(std::string(id)=="m32-e2"?16:20)&&session.variable(49)==5,"Per-instance retained V49 order");
  auto again=hit;again.receiverPage=2;dog->receiveAttack(again);dog->update({});require(dog->sourceState()==10,"Second hit resets state11 to10");
 }
 auto varied=c;std::fill(varied.maps.at(32).passageMasks.begin(),varied.maps.at(32).passageMasks.end(),15);
 auto grabEntry=fixture(varied,32);grabEntry.playerTile={5,5};grabEntry.positions["m32-e2"]={5,7};grabEntry.directions["m32-e2"]=Direction::Up;grabEntry.session.hp=grabEntry.session.maxHp=1000;
 GameSession grabbed(varied.items,grabEntry.session,varied.facts);CombatScene grapple(varied,grabbed,grabEntry,[](int,int hi){return hi;});bool held=false,released=false;int before=1000;
 for(int i=0;i<1200;++i){InputFrame in;if(grabbed.flag(10)){held=true;in.buttons[static_cast<int>(InputAction::Left)].pressed=true;}grapple.update(in);if(held&&!grabbed.flag(10)&&grabbed.conditions().recoveryTicks==0){released=true;break;}}
 require(held&&released&&grabbed.hp()<before,"Natural canine approach grabs, bites before escape, and recovers");
 require(grapple.world().player().spriteId!=varied.cue("f4bc-ce9-c18").at("sprite").get<std::string>(),"Grab pose is cleared after recovery");
 auto arrival=fixture(c,31);arrival.playerTile={14,17};arrival.session.flags[89]=arrival.session.flags[90]=arrival.session.flags[91]=false;arrival.session.hp=arrival.session.maxHp=1000;
 GameSession arrivals(c.items,arrival.session,c.facts);CombatScene windows(c,arrivals,arrival);for(int n=0;n<600;++n){windows.update({});}
 require(arrivals.flag(89)&&arrivals.flag(90)&&arrivals.flag(91),"Parallel window entry commits all source stages");
 require(!findCharacter(windows.world(),"m31-e18").spriteId,"Route switch hides visual proxy when real shared dog activates");
 std::cout<<"Five dogs, retained defense, shared death and natural grab recovery passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
