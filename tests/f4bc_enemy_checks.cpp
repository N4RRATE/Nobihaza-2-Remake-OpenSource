#include "combat_scene.h"
#include "enemy_controller.h"
#include "enemy_host.h"
#include <iostream>
#include <stdexcept>
void require(bool b,const char* m){if(!b){throw std::runtime_error(m);}}
AttackResolution hit(CombatScene& scene,const std::string& id,int receiver,AttackKind kind=AttackKind::Handgun){auto p=findCharacter(scene.world(),id).motion.position();std::set<std::string> seen;return scene.resolveAttack({{(p.x+.5)*16,(p.y+.5)*16},{0,0},kind,receiver,0,false},seen);}
EnemyView state(const CombatScene& s,const std::string& id){for(const auto& e:s.enemyStates()){if(e.id==id){return e;}}throw std::runtime_error("Missing enemy instance");}
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);
 for(const auto& [id,definition]:c.enemies){if(definition.rules.contains("rescue")||!definition.rules.contains("activation_fact")||definition.rules.value("behavior",std::string{})=="cerberus"){continue;}
  std::cout<<"instance "<<id<<std::endl;auto e=c.entries.at("defeated-claimed");e.map=definition.map;e.playerTile={1,1};e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.mainWeapon=0;e.session.baseAttack=4;e.session.flags[47]=false;
  if(!definition.rules.at("activation_fact").is_null()){e.session.flags[definition.rules.at("activation_fact").get<int>()]=true;}
  GameSession session(c.items,e.session,c.facts);CombatScene scene(c,session,e,[](int,int hi){return hi;});scene.update({});
  require(state(scene,id).mode==EnemyMode::Pursuit,"Each real new zombie activates independently");
  require(hit(scene,id,2).processed==1,"Each instance resolves original normal hit");
  auto events=scene.takePresentation();const auto blood=definition.rules.at("runtime_profile").at("blood_effects").at("1").get<std::string>();
  require(std::any_of(events.begin(),events.end(),[&](const auto& e){return e.id==blood&&e.actor==id;}),"Blood slot belongs to actual source enemy");
  require(hit(scene,id,5,AttackKind::De).processed==1,"Direct ending uses same public attack entry");
  const int headPage=definition.rules.at("runtime_profile").at("head_page");
  const auto& pages=c.objects.at(id).variants;const auto original=std::find_if(pages.begin(),pages.end(),[&](const auto& p){return p.page==headPage;});
  const auto ending=findCharacter(scene.world(),id);require(ending.layer==original->definition.layer,"Head ending uses original page layer");
  for(int i=0;i<400;++i){scene.update({});}require(session.flag(definition.deadFlag),"Death commits this enemy persistent fact");
 }
 for(const auto& id:{"m27-e3","m38-e2","m38-e5","m40-e3","m40-e7","m81-e6","m96-e6"}){
  const auto& d=c.enemies.at(id);const int threshold=d.rules.at("runtime_profile").at("fall").at("damage_threshold");
  auto e=c.entries.at("defeated-claimed");e.map=d.map;e.playerTile={1,1};e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.mainWeapon=0;e.session.baseAttack=threshold+3;e.session.flags[47]=false;
  if(!d.rules.at("activation_fact").is_null()){e.session.flags[d.rules.at("activation_fact").get<int>()]=true;}
  GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,[](int low,int high){return low==0&&high==10?2:high;});scene.update({});require(hit(scene,id,2).damage==threshold,"Original50/60 damage sample");scene.update({});require(state(scene,id).mode==EnemyMode::Fallen,"Original instance threshold enters down state");
  const int before=s.variable(35);hit(scene,id,3);require(s.variable(35)==before,"Main weapon cannot damage grounded receiver3");
  s.setFlag(4,true);hit(scene,id,3,AttackKind::Knife);
  const bool interrupted=id==std::string("m38-e2")||id==std::string("m38-e5")||id==std::string("m40-e7")||id==std::string("m81-e6")||id==std::string("m96-e6");
  require((state(scene,id).mode==EnemyMode::HitRecovery)==interrupted,"Grounded receiver3 respects per-instance interruption");
 }
 // Free-grid variation isolates the original approach/lost-target transition.
 auto varied=c;std::fill(varied.maps.at(27).passageMasks.begin(),varied.maps.at(27).passageMasks.end(),15);
 auto entry=varied.entries.at("defeated-claimed");entry.map=27;entry.playerTile={5,4};entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();entry.positions["m27-e3"]={5,5};entry.directions["m27-e3"]=Direction::Up;entry.session.flags[93]=true;
 GameSession moving(varied.items,entry.session,varied.facts);CombatScene approach(varied,moving,entry,[](int low,int){return low;});approach.update({});approach.update({});
 require(state(approach,"m27-e3").mode==EnemyMode::Approach&&findCharacter(approach.world(),"m27-e3").motion.speed()==16,"Original approach page raises speed to3");
 bool resumed=false;for(int n=0;n<100&&!resumed;++n){InputFrame f;f.movement=Direction::Left;approach.update(f);if(state(approach,"m27-e3").mode==EnemyMode::Pursuit){resumed=true;require(findCharacter(approach.world(),"m27-e3").motion.speed()==8,"Lost target restores original pursuit speed2");}}
 require(resumed,"Player can leave the attack approach");
 // CN1.00 M80E2P9 writes the attack state before C83-162 terrain checks;
 // C90 must still replace an existing route before next page's approach cue.
 for(bool acquire:{false,true}){
  auto variant=c;std::fill(variant.maps.at(80).passageMasks.begin(),variant.maps.at(80).passageMasks.end(),15);
  std::fill(variant.terrainIds.at(80).begin(),variant.terrainIds.at(80).end(),0);
  variant.terrainIds.at(80).at(4*variant.maps.at(80).width+5)=3;
  auto start=variant.entries.at("defeated-claimed");start.map=80;start.playerTile=acquire?TilePosition{6,4}:TilePosition{8,4};
  start.pages.clear();start.positions.clear();start.directions.clear();start.through.clear();start.positions["m80-e2"]={5,5};start.directions["m80-e2"]=Direction::Up;start.session.flags[13]=true;
  GameSession inventory(variant.items,start.session,variant.facts);CombatScene room(variant,inventory,start,[](int low,int){return low;});room.update({});
  EnemyHost host(room,"m80-e2");RouteDefinition previous;previous.frequency=8;previous.steps={{"wait"},{"left"}};host.route(previous);
  require(host.routePending(),"Fixture installs an unfinished route through the public host");room.update({});
  require(host.routePending("m80-e2-p9-c90"),"Terrain avoidance replaces an existing route even when acquiring an invulnerable player");
  require(state(room,"m80-e2").mode==(acquire?EnemyMode::Approach:EnemyMode::Pursuit),"Terrain avoidance preserves this update's acquisition decision");
  if(acquire){room.update({});require(host.routePending("m80-e2-p10-c1"),"Approach route begins on the following page update");}
 }
 std::cout<<"Branch zombie instances and down-state variants passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
