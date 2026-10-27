#include "combat_scene.h"
#include "save_snapshot.h"
#include "enemy_controller.h"
#include "game_application.h"
#include "enemy_host.h"
#include <iostream>
#include <stdexcept>
void require(bool value,const char* message){if(!value){throw std::runtime_error(message);}}
CombatEntry fixture(const CombatContent& c,TilePosition player){auto e=c.entries.at("defeated-claimed");e.map=63;e.playerTile=player;e.direction=Direction::Up;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.flags[144]=true;return e;}
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}const auto c=loadCombatContent(argv[1]);require(c.enemies.contains("m63-e12"),"Rescue enemy must be a real combat participant");
 auto e=fixture(c,{3,12});GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e);require(scene.enemyStates().size()==1,"Gian crisis owns one independent enemy");
 InputFrame z;z.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;
 for(int n=0;n<30000&&!state.flag(274);++n){scene.update(n%12==0?z:InputFrame{});}
 require(state.flag(272)&&!state.flag(273)&&state.flag(213)&&state.flag(274),"Enemy reaching NPC drives BSAA intervention and cooperation");
 require(state.count(29)==3&&state.count(33)==7,"Intervention grants source consumables once");
 for(int n=0;n<120;++n){scene.update({});}
 require(state.flag(1)&&state.flag(18)&&!state.flag(13),"Full outcome restores player control");
 for(int receiver:{4,5,6}){
  auto f=fixture(c,{3,12});GameSession rescued(c.items,f.session,c.facts);CombatScene fight(c,rescued,f,[](int low,int){return low;});fight.update({});
  auto enemy=findCharacter(fight.world(),"m63-e12").motion.position();AttackSample hit{{(enemy.x+.5)*16,(enemy.y+.5)*16},{0,0},AttackKind::Handgun,receiver,0,false};
  if(receiver==4){rescued.setVariable(35,1);rescued.setFlag(47,false);hit.kind=AttackKind::Knife;}
  std::set<std::string> targets;
  for(int n=0;n<150&&fight.enemyStates().front().damage<100;++n){targets.clear();fight.resolveAttack(hit,targets);}
  require(fight.enemyStates().front().damage>=100,"Attack interface reaches rescue ending");
  fight.update({});const auto& animation=c.media.at("animations").at("1");const int wait=receiver==4?30:receiver==5?60+static_cast<int>(animation.at("frames").size())*animation.at("playback").at("logic_updates_per_frame").get<int>():42;
  for(int n=0;n<wait-1;++n){fight.update({});}if(rescued.flag(273)){throw std::runtime_error("Rescue premature outcome receiver="+std::to_string(receiver)+" damage="+std::to_string(fight.enemyStates().front().damage)+" S12="+std::to_string(rescued.flag(12))+" expected="+std::to_string(wait));}fight.update({});require(rescued.flag(273),"Death page commits timely result at its own delay");
  for(int n=0;n<30000&&!rescued.flag(274);++n){fight.update(n%12==0?z:InputFrame{});}
  require(!rescued.flag(272)&&rescued.flag(273)&&rescued.flag(213)&&rescued.flag(274),"Normal/head/body termination wins timely rescue");
  require(rescued.count(29)==3&&rescued.count(33)==7,"Timely branch grants same consumables");
 }
 {
  auto varied=c;auto start=fixture(varied,{3,12});start.positions["m63-e11"]={11,8};
  GameSession moving(varied.items,start.session,varied.facts);CombatScene room(varied,moving,start);for(int n=0;n<20;++n){room.update({});}
  require(findCharacter(room.world(),"m63-e12").motion.tile().y>5,"NPC pursuit reads real changed world position");
 }
 {
  auto test=c;test.entries["rescue-test"]=fixture(test,{3,12});GameApplication app(std::move(test),"rescue-test");InputFrame x;x.buttons[static_cast<int>(InputAction::Cancel)].pressed=true;app.update(x);require(app.mode()==ApplicationMode::Equipment,"Crisis allows original menu");const auto before=findCharacter(app.world(),"m63-e12").motion.position();
  for(int n=0;n<600;++n){app.update({});}require((findCharacter(app.world(),"m63-e12").motion.position().x==before.x&&findCharacter(app.world(),"m63-e12").motion.position().y==before.y)&&!app.session().flag(272),"Application menu pauses crisis rather than advancing a global countdown");app.update(x);require(app.mode()==ApplicationMode::Playing,"Closing menu resumes scene");
 }
 {
  auto start=fixture(c,{3,12});GameSession pending(c.items,start.session,c.facts);CombatScene first(c,pending,start);for(int n=0;n<25;++n){first.update({});}first.deactivate();auto returned=fixture(c,{3,12});returned.session=pending.exportState();CombatScene second(c,pending,returned);require(second.generation()!=first.generation()&&second.enemyStates().front().damage==0&&findCharacter(second.world(),"m63-e12").motion.tile()==TilePosition{11,5},"Unfinished crisis rebuilds source position and temporary damage on return");require(pending.flag(144)&&!pending.flag(274),"Returning preserves request without inventing completion");
 }
 {
  auto saved=fixture(c,{10,21});saved.map=16;saved.session.inventory[68]=1;saved.session.inventory[70]=1;for(int flag:{145,141,142,143}){saved.session.flags[flag]=true;}
  GameSession inventory(c.items,saved.session,c.facts);CombatScene saveRoom(c,inventory,saved);auto snapshot=captureSave(c,saveRoom,{saveRoom.generation(),"m16-e5","BSAA 1F入口"},{});
  require(prepareRestore(c,decodeSave(c,encodeSave(c,snapshot))).session->flag(144),"Pending Gian request can persist at legal crow");
  for(int flag:{272,273,213,274,318,336,339}){auto bad=snapshot;bad.session.worldFlags[c.facts.switches.at(flag)]=true;bool rejected=false;try{encodeSave(c,bad);}catch(const std::exception&){rejected=true;}require(rejected,"Isolated rescue or reward fact is rejected at save boundary");}
  snapshot.session.worldFlags[c.facts.switches.at(272)]=true;snapshot.session.worldFlags[c.facts.switches.at(213)]=true;snapshot.session.worldFlags[c.facts.switches.at(274)]=true;require(prepareRestore(c,decodeSave(c,encodeSave(c,snapshot))).session->flag(272),"Complete intervention state round trips without regrant");
 }
 {
  auto start=fixture(c,{13,5});start.session.flags[13]=true;start.directions["m63-e12"]=Direction::Right;GameSession guarded(c.items,start.session,c.facts);CombatScene room(c,guarded,start);room.update({});EnemyHost enemy(room,"m63-e12");RouteDefinition route;route.frequency=8;route.steps={{"right"}};enemy.route(route);room.update({});
  require(findCharacter(room.world(),"m63-e12").motion.moving()&&room.enemyStates().front().mode==EnemyMode::Approach,"Moving enemy acquires front player even during S13 invulnerability");
 }
 {
  auto variant=c;auto start=fixture(variant,{14,9});start.positions["m63-e11"]={11,5};start.directions["m63-e12"]=Direction::Up;variant.terrainIds.at(63).at(4*variant.maps.at(63).width+11)=3;
  GameSession inventory(variant.items,start.session,variant.facts);CombatScene room(variant,inventory,start);room.update({});EnemyHost host(room,"m63-e12");auto actor=host.actor();actor.face(Direction::Up);host.setActor(actor);for(int n=0;n<4;++n){room.update({});}
  require(findCharacter(room.world(),"m63-e12").motion.tile()==TilePosition{12,5},"Source terrain route follows player side when NPC axes submit no replacement");
 }
 std::cout<<"Gian rescue checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
