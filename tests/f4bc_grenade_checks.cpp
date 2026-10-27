#include "combat_scene.h"
#include "enemy_controller.h"
#include "enemy_host.h"
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* why){if(!ok){throw std::runtime_error(why);}}
InputFrame press(InputAction action){InputFrame input;input.buttons[static_cast<int>(action)].pressed=true;return input;}
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);auto e=c.entries.at("defeated-claimed");e.map=28;e.playerTile={5,5};e.direction=Direction::Right;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.mainWeapon=0;e.session.auxiliaryWeapon=0;e.session.inventory[29]=1;
 GameSession s(c.items,e.session,c.facts);require(s.equip(29,2),"Grenade is a usable auxiliary weapon");e.session=s.exportState();CombatScene room(c,s,e);room.update(press(InputAction::AuxiliaryWeapon));for(int n=0;n<90;++n){room.update({});}require(room.raised(),"Grenade raises using original animation");
 room.update(press(InputAction::Attack));require(s.equipped(2)==29,"Windup does not spend grenade at button press");
 bool exploded=false;for(int n=0;n<220;++n){room.update({});for(const auto& event:room.takePresentation()){if(event.id=="f4bc-ce7-c833"){exploded=true;}}}
 require(s.count(29)==0&&s.equipped(2)==0,"Last grenade commits once and unequips");require(exploded,"Last grenade survives unequip and reaches explosion");require(!room.raised(),"Last throw restores walking posture");
 for(const auto& id:{"m27-e3","m32-e2"}){
  const auto& d=c.enemies.at(id);auto target=e;target.map=d.map;target.playerTile={3,12};target.session=s.exportState();target.session.hp=100;target.session.flags[93]=true;target.session.flags[88]=true;target.session.variables[35]=9;
  GameSession state(c.items,target.session,c.facts);state.setFlag(93,true);state.setFlag(88,true);state.setFlag(d.deadFlag,false);CombatScene arena(c,state,target);arena.update({});std::unique_ptr<EnemyController> dog;if(std::string(id)=="m32-e2"){dog=makeEnemyBehavior(arena,d);dog->activateState(1);}const auto pos=findCharacter(arena.world(),id).motion.position();std::set<std::string> resolved;
  const AttackSample sample{{(pos.x+.5)*16,(pos.y+.5)*16},{0,0},AttackKind::Grenade,6,0,false};
  const auto result=dog?dog->receiveAttack(sample):[&]{auto r=arena.resolveAttack(sample,resolved);return AttackContact{r.contacts>0,r.processed>0,r.damage,r.endRequested};}();
  require(result.processed&&result.damage==2000&&state.variable(35)==9,"Grenade P6 writes2000 without ordinary damage statistics");
  for(int n=0;n<600;++n){if(dog){dog->update({});}else{arena.update({});}}require(state.flag(d.deadFlag),"Species ending commits original dead fact");
 }

 for(bool afterCommit:{false,true}){
  auto start=e;start.session.auxiliaryWeapon=29;start.session.inventory[29]=0;
  GameSession state(c.items,start.session,c.facts);CombatScene arena(c,state,start);
  arena.update(press(InputAction::AuxiliaryWeapon));for(int n=0;n<90;++n){arena.update({});}
  arena.update(press(InputAction::Attack));
  if(afterCommit){for(int n=0;n<200&&state.equipped(2)==29;++n){arena.update({});}require(state.equipped(2)==0,"Reached original consumption point");}
  EnemyHost host(arena,"player");host.cancelWeapon();bool blast=false;
  for(int n=0;n<240;++n){arena.update({});for(const auto& event:arena.takePresentation()){blast|=event.id=="f4bc-ce7-c833";}}
  require(blast==afterCommit,"Grab cancellation preserves only already committed grenade");require(state.equipped(2)==(afterCommit?0:29),"Cancelled windup does not spend item");
 }
 for(int offset:{38,39}){
  auto start=e;start.map=27;start.session.flags[93]=true;start.session.flags[111]=false;
  GameSession state(c.items,start.session,c.facts);CombatScene arena(c,state,start);
  const auto pos=findCharacter(arena.world(),"m27-e3").motion.position();std::set<std::string> seen;
  const AttackSample sample{{(pos.x+.5)*16+offset,(pos.y+.5)*16},{30,30},AttackKind::Grenade,6,0,false};
  arena.resolveAttack(sample,seen);require(seen.contains("m27-e3")== (offset==38),"Explosion uses inclusive 30+8 bounds");
  require(arena.resolveAttack(sample,seen).processed==0,"Same explosion never processes a target twice");
 }
 // CE11 must read the current auxiliary slot, including an empty final grenade slot.
 for(bool finalItem:{false,true}){
  auto target=c.entries.at("defeated-claimed");target.map=19;target.playerTile={3,3};target.pages.clear();target.positions.clear();target.directions.clear();target.through.clear();target.session.mainWeapon=0;target.session.auxiliaryWeapon=0;target.session.inventory[29]=1;target.session.flags[57]=true;target.session.flags[105]=false;target.session.baseAgility=1;target.session.flags[47]=true;target.session.variables[35]=0;
  GameSession state(c.items,target.session,c.facts);require(state.equip(29,2),"Equip test grenade");CombatScene arena(c,state,target,[](int lo,int hi){return lo==0&&hi==10?2:lo;});arena.update(press(InputAction::AuxiliaryWeapon));for(int n=0;n<100;++n){arena.update({});}
  if(finalItem){state.consumeEquippedItem(2);}
  auto pos=findCharacter(arena.world(),"m19-e2").motion.position();std::set<std::string> resolved;
  const auto hit=arena.resolveAttack({{(pos.x+.5)*16,(pos.y+.5)*16},{30,30},AttackKind::Grenade,6,0,false},resolved);
  require(hit.processed==1&&hit.damage==(finalItem?1:106),"Licker P6 uses current agility after last-item removal");require(state.flag(47)&&state.variable(35)==1,"Auxiliary CE11 leaves precision intact");
 }
 // Legacy targets keep their existing hit cues and their separate body endings.
 for(const auto& id:{"m10-e3","m10-e5","m11-e2","m13-e9","m17-e3","m17-e5"}){
  const auto& d=c.enemies.at(id);auto target=e;target.map=d.map;target.playerTile={1,1};GameSession state(c.items,target.session,c.facts);state.setFlag(d.deadFlag,false);CombatScene arena(c,state,target,[](int lo,int hi){return lo==0&&hi==10?2:lo;});
  auto p=findCharacter(arena.world(),id).motion.position();std::set<std::string> seen;
  require(arena.resolveAttack({{(p.x+.5)*16,(p.y+.5)*16},{0,0},AttackKind::Handgun,2,0,false},seen).processed>0,"Old ordinary hit keeps full cue definitions");
  seen.clear();arena.resolveAttack({{(p.x+.5)*16,(p.y+.5)*16},{0,0},AttackKind::Grenade,6,0,false},seen);
  for(int tick=0;tick<600;++tick){arena.update({});}require(state.flag(d.deadFlag),"Old zombie body transition completes");
 }
 {
  auto target=c.entries.at("defeated-claimed");target.pages.clear();target.positions.clear();target.directions.clear();target.through.clear();target.session.variables[89]=0;
  GameSession state(c.items,target.session,c.facts);state.setFlag(101,false);state.setFlag(55,true);state.setFlag(52,true);state.setFlag(1,true);state.setFlag(18,true);CombatScene arena(c,state,target);
  auto p=findCharacter(arena.world(),"m4-e4").motion.position();std::set<std::string> seen;arena.takePresentation();
  arena.resolveAttack({{(p.x+.5)*16,(p.y+.5)*16},{0,0},AttackKind::Grenade,6,0,false},seen);
  bool stopped=false;for(int tick=0;tick<160;++tick){arena.update({});for(const auto& v:arena.takePresentation()){stopped|=v.kind==PresentationEvent::Kind::StopMusic;}}
  require(state.flag(101)&&state.variable(398)==3,"Soldier body transitions to original third outcome");require(findCharacter(arena.world(),"m4-e4").layer==0&&arena.enemyStates().front().mode==EnemyMode::Corpse,"Story body completes its corpse page and controller");require(state.flag(1)&&state.flag(18)&&!stopped,"S55 body outcome preserves controls and music fade");
 }
 {
  auto varied=c;std::fill(varied.maps.at(27).passageMasks.begin(),varied.maps.at(27).passageMasks.end(),15);
  auto target=e;target.map=27;target.playerTile={5,5};target.positions["m27-e3"]={5,6};target.directions["m27-e3"]=Direction::Up;target.session.hp=target.session.maxHp=1000;
  GameSession state(varied.items,target.session,varied.facts);state.setFlag(93,true);for(const auto& [id,d]:varied.enemies){if(d.map==27){state.setFlag(d.deadFlag,id!="m27-e3");}}
  CombatScene arena(varied,state,target,[](int lo,int hi){return lo==1&&hi==2?2:lo;});bool held=false;
  for(int tick=0;tick<1000&&!held;++tick){arena.update({});held=state.flag(10);}require(held,"Real approach reaches player grab");
  const auto p=findCharacter(arena.world(),"m27-e3").motion.position();std::set<std::string> seen;arena.resolveAttack({{(p.x+.5)*16,(p.y+.5)*16},{30,30},AttackKind::Grenade,6,0,false},seen);
  for(int tick=0;tick<300;++tick){arena.update({});}require(!state.flag(10)&&state.flag(1),"Grenade ending releases existing grab ownership");
  const auto before=arena.world().player().motion.tile();InputFrame move;move.movement=Direction::Right;for(int tick=0;tick<30;++tick){arena.update(move);}require(arena.world().player().motion.tile()!=before,"Player movement resumes after grabbing enemy is destroyed");
 }
 std::cout<<"Grenade windup, final item and explosion passed\n";
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
