#include "combat_scene.h"
#include <iostream>
#include <stdexcept>
void require(bool x,const char* s){if(!x){throw std::runtime_error(s);}}
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);
 for(const auto& id:{"m27-e9","m30-e9","m44-e2"}){
  std::cout<<id<<std::endl;const auto& def=c.enemies.at(id);auto e=c.entries.at("defeated-claimed");e.map=def.map;e.playerTile={1,1};e.pages.clear();e.positions.clear();e.through.clear();e.directions.clear();if(!def.rules.at("activation_fact").is_null()){e.session.flags[def.rules.at("activation_fact").get<int>()]=true;}e.session.variables[35]=0;e.session.mainWeapon=0;e.session.baseAttack=4;e.session.flags[47]=false;
  GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,[](int lo,int hi){return lo==0&&hi==10?2:hi;});scene.update({});scene.takePresentation();
  auto strike=[&](int receiver){const auto p=findCharacter(scene.world(),id).motion.position();std::set<std::string> seen;auto contact=scene.resolveAttack({{(p.x+.5)*16,(p.y+.5)*16},{0,0},AttackKind::Handgun,receiver,0,false},seen);require(contact.processed==1,"Real source target receives attack");return scene.takePresentation();};
  const auto empty=strike(4);require(std::none_of(empty.begin(),empty.end(),[&](const auto& v){return v.kind==PresentationEvent::Kind::Effect&&v.actor==id;}),"First cross-slot P4 has blood type zero, so no blood effect");
  strike(2);const auto retained=strike(4);require(std::any_of(retained.begin(),retained.end(),[&](const auto& v){return v.id=="enemy-slot-18-blood-1"&&v.actor==id;}),"P4 reads own previous small blood while writing another slot");
 }
 std::cout<<"Three source blood-slot anomalies and retained scene state passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
