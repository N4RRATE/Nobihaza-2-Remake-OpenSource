#include "game_application.h"
#include "projectile_policy.h"
#include <iostream>
#include <stdexcept>
namespace{
void require(bool b,const char* m){if(!b){throw std::runtime_error(m);}}
InputFrame press(InputAction a){InputFrame f;f.buttons[static_cast<int>(a)]={true,true,false};return f;}
void ticks(CombatScene& s,int n){while(n--){s.update({});}}
}
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);auto e=c.entries.at("post-pickup");
 e.map=22;e.playerTile={10,5};e.direction=Direction::Up;e.positions.clear();e.pages.clear();e.directions.clear();e.through.clear();
 e.session.flags[52]=false;e.session.flags[1]=true;e.session.flags[18]=true;
 GameSession state(c.items,e.session,c.facts);state.grant(14,1);require(state.equip(14),"Prepared B shotgun becomes executable in59");
 require(state.magazine(14)==5&&state.consumeRound()&&state.magazine(14)==4,"Shotgun has its independent five-round magazine");
 state.grant(14,1);state.equip(0);require(state.equip(14)&&state.magazine(14)==4,"Repeated pickup and re-equipment never refill shotgun");
 CombatScene scene(c,state,e);scene.update(press(InputAction::Weapon));ticks(scene,100);require(scene.raised(),"Actual shotgun raise completes");
 scene.update(press(InputAction::Attack));require(state.magazine(14)==3&&state.magazine(21)==8,"Actual shot commits one shotgun round only");ticks(scene,180);
 auto l=e;l.map=19;l.playerTile={20,18};l.session.mainWeapon=14;l.session.flags[57]=true;l.session.flags[105]=false;l.session.flags[47]=false;l.positions["m19-e2"]={20,15};
 for(int distance:{2,3}){
  std::vector<std::pair<int,int>> rolls;
  GameSession hurt(c.items,l.session,c.facts);CombatScene target(c,hurt,l,[&](int lo,int hi){rolls.emplace_back(lo,hi);return lo==0&&hi==10?2:lo;});target.update({});rolls.clear();
  const auto p=findCharacter(target.world(),"m19-e2").motion.position();std::set<std::string> hit;
  const auto result=target.resolveAttack(AttackSample{{(p.x+.5)*16,(p.y+.5)*16},{15,5},AttackKind::Shotgun,distance==2?4:3,distance,false},hit);
  require(result.damage==(distance==2?51:31),"Shotgun adds20..40 only at V42<=2, with no handgun distance reduction");
  require(result.endRequested==(distance>2),"Only CE11 far shotgun requests S16, not every contact");
  if(distance==2){require(rolls==std::vector<std::pair<int,int>>{{20,40},{0,10}},"Near bonus random is read before damage fluctuation");}
 }
 auto lane=e;lane.map=10;lane.playerTile={5,19};lane.session.mainWeapon=14;lane.session.flags[47]=false;lane.session.variables[35]=0;
 lane.positions["m10-e3"]={5,18};lane.positions["m10-e5"]={6,16};lane.pages["m10-e3"]=8;lane.pages["m10-e5"]=8;
 GameSession flightState(c.items,lane.session,c.facts);CombatScene flight(c,flightState,lane,[](int lo,int hi){return lo==0&&hi==10?2:lo;});
 flight.update(press(InputAction::Weapon));flightState.setFlag(18,false);ticks(flight,100);flightState.setFlag(18,true);flight.update(press(InputAction::Attack));flightState.setFlag(18,false);ticks(flight,20);
 require(flight.enemyStates()[0].damage>=150&&flight.enemyStates()[1].damage>0,"Muzzle direct kill does not prevent wide flight hitting a different offset target");
 for(bool far:{false,true}){
  auto trail=lane;trail.positions["m10-e3"]={5,far?14:15};trail.positions["m10-e5"]={5,12};
  GameSession progress(c.items,trail.session,c.facts);CombatScene shot(c,progress,trail,[](int lo,int hi){return lo==0&&hi==10?2:lo;});
  shot.update(press(InputAction::Weapon));progress.setFlag(18,false);ticks(shot,100);progress.setFlag(18,true);shot.update(press(InputAction::Attack));progress.setFlag(18,false);ticks(shot,20);
  require(shot.enemyStates()[0].damage>0,"The first target receives the shot");
  require((shot.enemyStates()[1].damage==0)==far,"V42=2 contact propagates; V42>=3 CE11 response ends before farther targets");
 }
 for(int roll:{7,8}){
  auto entry=l;entry.session.variables[35]=2;
  GameSession critical(c.items,entry.session,c.facts);CombatScene target(c,critical,entry,[&](int lo,int hi){return hi==100?roll:lo==0&&hi==10?2:lo;});target.update({});
  const auto p=findCharacter(target.world(),"m19-e2").motion.position();std::set<std::string> hit;
  const auto result=target.resolveAttack(AttackSample{{(p.x+.5)*16,(p.y+.5)*16},{15,5},AttackKind::Shotgun,4,2,false},hit);
  require(result.damage==(roll==7?102:51),"Shotgun critical threshold is roll minus hit count <=4");
  if(roll==7){bool sound=false;for(const auto& ev:target.takePresentation()){sound|=ev.kind==PresentationEvent::Kind::Sound;}require(sound,"Licker P4 critical still emits its unconditional hit sound");}
 }
 // Literal expectations from CE41 four directions and fixed CE43 sample table.
 const auto& rule=c.expansion.attacks.at(14);
 const std::array<GridPosition,4> muzzle={GridPosition{168,160},{176,168},{168,184},{160,168}};
 for(int dir=0;dir<4;++dir){const auto sample=sampleProjectile(rule,{10,10},static_cast<Direction>(dir),true,0);require(sample.center.x==muzzle[dir].x&&sample.center.y==muzzle[dir].y&&sample.receiverPage==5,"Four-direction muzzle coordinates preserve the different downward offset");}
 const auto vertical=sampleProjectile(rule,{10,8},Direction::Up,false,2);
 require(vertical.center.x==168&&vertical.center.y==112&&vertical.halfPixels==std::array<int,2>{15,5}&&vertical.receiverPage==4,"Flight samples an independent half-step ahead of its terrain cursor");
 const auto horizontal=sampleProjectile(rule,{13,10},Direction::Right,false,3);
 require(horizontal.center.x==240&&horizontal.center.y==168&&horizontal.halfPixels==std::array<int,2>{5,15}&&horizontal.receiverPage==3,"Horizontal flight swaps geometry axes at far response");
 for(int rounds:{0,3,5}){
  auto entry=e;entry.session.mainWeapon=14;entry.session.magazines[14]=rounds;entry.session.inventory[33]=1;entry.session.inventory[34]=1;
  GameSession ammo(c.items,entry.session,c.facts);CombatScene reload(c,ammo,entry);reload.update(press(InputAction::Weapon));ticks(reload,100);
  auto held=press(InputAction::Cancel);held.buttons[static_cast<int>(InputAction::Cancel)].pressed=false;reload.update(press(InputAction::Cancel));for(int n=0;n<9;++n){reload.update(held);}ticks(reload,4);
  require(ammo.magazine(14)==5&&ammo.count(33)==(rounds==5?1:rounds==3?29:26)&&ammo.count(34)==(rounds==5?1:0),"Shotgun partial and full reload consume its own ammunition at start");
  reload.deactivate();ticks(reload,120);require(ammo.magazine(14)==5,"Reload cancellation cannot refund or reapply committed ammo");
 }
 auto empty=e;empty.session.mainWeapon=14;empty.session.magazines[14]=0;empty.session.inventory[34]=1;empty.session.flags[47]=true;
 GameSession refill(c.items,empty.session,c.facts);CombatScene automatic(c,refill,empty);automatic.update(press(InputAction::Weapon));ticks(automatic,100);automatic.update(press(InputAction::Attack));ticks(automatic,200);
 require(refill.magazine(14)==5&&refill.count(33)==25&&refill.flag(47),"Empty shot loads one box without appending a shot or clearing precision");
 auto savedEntry=e;savedEntry.session=state.exportState();GameSession saved(c.items,savedEntry.session,c.facts);CombatScene saveScene(c,saved,savedEntry);
 const auto stored=captureSave(c,saveScene,{saveScene.generation(),"m22-e2",""},{});auto restored=prepareRestore(c,decodeSave(c,encodeSave(c,stored)));
 require(restored.session->equipment().mainWeapon==14&&restored.session->magazine(14)==3&&restored.session->magazine(21)==8,"Independent shotgun magazine and equipment survive lawful save restoration");
 auto open=c;open.terrainIds.at(19).assign(open.terrainIds.at(19).size(),0);
 auto clear=l;clear.playerTile={15,18};clear.direction=Direction::Right;clear.session.flags[105]=true;clear.session.flags[47]=true;
 for(bool wall:{false,true}){
  auto map=open;if(wall){map.terrainIds.at(19)[18*map.maps.at(19).width+17]=3;}
  GameSession aim(map.items,clear.session,map.facts);CombatScene emptyFlight(map,aim,clear);emptyFlight.update(press(InputAction::Weapon));ticks(emptyFlight,100);emptyFlight.update(press(InputAction::Attack));
  const int beforeEnd=wall?3:8;ticks(emptyFlight,beforeEnd);require(aim.flag(47),"Shot remains alive until the terrain cursor or eighth flight call ends it");
  emptyFlight.update({});require(!aim.flag(47),"Wall uses tile cursor; open trajectory ends after eight sampled calls");
 }
 std::cout<<"Shotgun checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
