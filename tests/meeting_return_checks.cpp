#include "save_snapshot.h"
#include <iostream>
#include <queue>
#include <set>
void need(bool v,const char* reason){if(!v)throw std::runtime_error(reason);}
InputFrame confirm(){InputFrame i;i.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;return i;}
int main(int argc,char**argv){try{
 if(argc!=2)return 2;auto c=loadCombatContent(argv[1]);auto e=c.entries.at("defeated-claimed");e.map=12;e.playerTile={19,17};e.direction=Direction::Right;
 e.session.inventory[62]=1;
 for(int f:{66,67,68,71,72,58,73,75,76,77,78,79,80,81,82})e.session.flags[f]=true;e.session.flags[1]=false;e.session.flags[18]=false;e.session.flags[13]=true;e.session.skills.insert(4);
 GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e);const int ammo=state.count(32);bool rewardBeforeCompletion=false;
 for(int n=0;n<20000&&(!state.flag(84)||scene.foreground()!=ForegroundKind::None);++n){
  scene.update(n%11==0?confirm():InputFrame{});scene.takePresentation();
  if(state.count(32)==ammo+1&&!state.flag(84))rewardBeforeCompletion=true;
 }
 need(state.flag(84)&&rewardBeforeCompletion&&state.count(32)==ammo+1,"Return must grant one box before committing S84");
 need(state.flag(1)&&state.flag(18)&&!state.flag(13)&&scene.foreground()==ForegroundKind::None,"Return restores controls only after final show");
 need(!findCharacter(scene.world(),"m12-e20").spriteId&&!findCharacter(scene.world(),"m12-e21").spriteId,"S84 hides returning actors");
 for(int n=0;n<400;++n)scene.update(confirm());need(state.count(32)==ammo+1,"Completed return never repeats reward");
 for(int map:{16,22}){
  auto at=e;at.map=map;at.positions.clear();const std::string crow=map==16?"m16-e5":"m22-e2";at.playerTile=adjacentTile(c.objects.at(crow).tile,Direction::Down);at.direction=Direction::Up;
  // Use the actual map transfer, including any conditional parallel entry event.
  GameSession safe(c.items,state.exportState(),c.facts);auto approach=e;approach.map=map==16?15:21;
  const auto& door=c.objects.at(map==16?"m15-e6":"m21-e9");approach.playerTile=adjacentTile(door.tile,Direction::Down);approach.direction=Direction::Up;
  CombatScene entering(c,safe,approach);entering.update(confirm());
  for(int n=0;n<2000&&(entering.mapId()!=map||entering.foreground()!=ForegroundKind::None);++n){entering.update({});entering.takePresentation();}
  if(entering.mapId()!=map||!safe.knowsSkill(3)||safe.knowsSkill(4))std::cerr<<"Safe entry target="<<map<<" actual="<<entering.mapId()<<" foreground="<<static_cast<int>(entering.foreground())<<" skill3="<<safe.knowsSkill(3)<<" skill4="<<safe.knowsSkill(4)<<"\n";
  need(entering.mapId()==map&&safe.knowsSkill(3)&&!safe.knowsSkill(4),"Actual safe-room entry restores mixing before save");
  CombatScene save(c,safe,at);
  auto snapshot=captureSave(c,save,{save.generation(),crow,""},{});auto restored=prepareRestore(c,decodeSave(c,encodeSave(c,snapshot)));
  need(restored.session->flag(84)&&restored.session->count(32)==ammo+1,"Completion and exact box count survive both save contexts");
  CombatScene revisit(c,*restored.session,e);for(int n=0;n<60;++n)revisit.update({});need(revisit.foreground()==ForegroundKind::None&&restored.session->count(32)==ammo+1,"Load/revisit does not replay return");
 }
 // Revisit both original blocked meeting doors; control returns without rewards.
 for(const std::string id:{"m12-e3","m12-e4"}){
  auto door=e;door.playerTile=adjacentTile(c.objects.at(id).tile,Direction::Down);door.direction=Direction::Up;
  CombatScene refusal(c,state,door);refusal.update(confirm());need(refusal.foreground()==ForegroundKind::Content,"Original meeting refusal is playable");
  for(int n=0;n<2000&&refusal.foreground()!=ForegroundKind::None;++n){refusal.update(n%11==0?confirm():InputFrame{});refusal.takePresentation();}
  need(refusal.foreground()==ForegroundKind::None&&state.count(32)==ammo+1&&state.flag(1)&&state.flag(18),"Refusal restores controls without duplicate award");
 }
 // Real collision connectivity from the old-area entrance to each reachable exit.
 for(const std::string id:{"m16-e15","m16-e16","m16-e17"}){
  auto at=e;at.map=16;at.playerTile={21,27};at.direction=Direction::Up;GameSession branch(c.items,state.exportState(),c.facts);CombatScene travel(c,branch,at);
  const auto target=c.objects.at(id).tile;const auto goal=adjacentTile(target,id=="m16-e17"?Direction::Down:Direction::Right);
  auto actor=travel.world().player();std::queue<TilePosition> queue;std::set<std::pair<int,int>> seen;queue.push(at.playerTile);seen.insert({at.playerTile.x,at.playerTile.y});bool reachable=false;
  while(!queue.empty()){
   const auto tile=queue.front();queue.pop();if(tile==goal){reachable=true;break;}
   for(int d=0;d<4;++d){const auto direction=static_cast<Direction>(d);actor.motion=CharacterMotion(tile,direction,32);if(!travel.world().canMove(actor,direction))continue;auto next=adjacentTile(tile,direction);if(seen.insert({next.x,next.y}).second)queue.push(next);}
  }
  need(reachable,"S84 exit must be physically reachable from real Map16 entry");
  at.playerTile=goal;at.direction=id=="m16-e17"?Direction::Up:Direction::Left;CombatScene edge(c,branch,at);const auto before=branch.exportState();edge.update(confirm());
  need(edge.exitRequest().has_value()&&edge.mapId()==16&&before==branch.exportState(),"Unimplemented branch requests destination without CE55 or state mutation");
  need(edge.exitRequest()->map==(id=="m16-e17"?39:26),"Boundary destination follows source");edge.dismissBoundary();
 }
 std::cout<<"Meeting return checks passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
