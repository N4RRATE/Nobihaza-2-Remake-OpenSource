#include "game_application.h"
#include <queue>
#include <map>
#include <iostream>
#include <stdexcept>
using Point=std::pair<int,int>;
InputFrame press(){InputFrame f;f.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;return f;}
std::optional<Direction> nextStep(const GameApplication& game,TilePosition target){
 const auto& w=game.world();const auto start=w.player().motion.tile();if(start==target){return {};}
 std::queue<TilePosition> q;q.push(start);std::map<Point,Direction> first;first[{start.x,start.y}]=Direction::Up;
 while(!q.empty()){
  auto pos=q.front();q.pop();auto actor=w.player();actor.motion=CharacterMotion(pos,Direction::Down,32);
  for(int i=0;i<4;++i){auto d=static_cast<Direction>(i);auto next=adjacentTile(pos,d);Point key{next.x,next.y};
   if(first.contains(key)||!w.canMove(actor,d)){continue;}
   first[key]=pos==start?d:first.at({pos.x,pos.y});if(next==target){return first[key];}q.push(next);
  }
 }
 return {};
}
int main(int argc,char** argv){
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);auto entry=c.entries.at("defeated-claimed");
 entry.map=6;entry.playerTile={20,3};entry.direction=Direction::Up;entry.pages.clear();entry.positions.clear();entry.through.clear();entry.directions.clear();
 // Path automation fixture only: extra health buys deterministic navigation time. All real AI/collision/attacks remain enabled.
 entry.session.hp=10000;entry.session.maxHp=10000;c.entries["route-navigation-fixture"]=entry;GameApplication game(c,"route-navigation-fixture",[](int lo,int){return lo;});
 bool observedEnemyMove=false;int ticks=0;auto tick=[&](InputFrame f){game.update(f);++ticks;for(const auto& o:game.world().objects()){if(c.enemies.contains(o.id)&&!c.enemies.at(o.id).story&&o.active&&o.motion.tile()!=c.objects.at(o.id).tile){observedEnemyMove=true;}}if(game.mode()!=ApplicationMode::Playing){throw std::runtime_error("Unexpected application mode in reachable route");}};
 auto settle=[&](){for(int n=0;n<4000;++n){tick(n%12==0?press():InputFrame{});if(game.foreground()==ForegroundKind::None&&game.mapId()!=6&&!game.world().player().motion.moving()){return;}}throw std::runtime_error("Foreground did not finish");};
 settle();
 for(const auto* id:{"m9-e6-p2","m10-e10-p2","m11-e4-p2","m10-e9-p2","m12-e8-p2","m13-e2-p2","m14-e2-p2","m13-e4-p2","m15-e6-p2"}){
  const auto& transfer=c.transitions.at(id);const auto& object=c.objects.at(transfer.at("object").get<std::string>());
  if(game.mapId()!=object.map){throw std::runtime_error(std::string("Wrong map before ")+id);}
  const auto* page=c.activeVariant(object,game.session());bool arrived=false;
  for(int attempt=0;attempt<16000&&game.mapId()==object.map;++attempt){
   if(game.foreground()!=ForegroundKind::None){tick(attempt%12==0?press():InputFrame{});continue;}
   if(game.world().player().motion.moving()){tick({});continue;}
   const auto from=game.world().player().motion.tile();
   if(page->definition.trigger==0&&page->definition.layer==1){
    std::optional<Direction> toward;
    for(int i=0;i<4;++i){if(adjacentTile(from,static_cast<Direction>(i))==object.tile){toward=static_cast<Direction>(i);}}
    if(toward){InputFrame turn;turn.movement=*toward;tick(turn);tick(press());continue;}
   }
   std::optional<Direction> move;
   if(page->definition.layer==1){for(int i=0;i<4&&!move;++i){move=nextStep(game,adjacentTile(object.tile,static_cast<Direction>(i)));}}
   else{move=nextStep(game,object.tile);}
   InputFrame f;f.movement=move;tick(f);
  }
  arrived=game.mapId()==transfer.at("destination").at("map").get<int>();if(!arrived){throw std::runtime_error(std::string("Unreachable route: ")+id);}
  settle();std::cout<<id<<" -> "<<game.mapId()<<" HP="<<game.session().hp()<<"\n";
 }

 // Continue the same actual-map route through ticket44. Extra HP is a navigation fixture only.
 auto action=[&](const std::string& id,int destination){
  const auto& object=c.objects.at(id);const int origin=game.mapId();
  for(int attempt=0;attempt<16000;++attempt){
   if(game.mapId()!=origin){settle();return;}
   if(game.foreground()!=ForegroundKind::None){settle();return;}
   if(game.world().player().motion.moving()){tick({});continue;}
   const auto* page=c.activeVariant(object,game.session());const auto from=game.world().player().motion.tile();
   for(int i=0;i<4;++i){auto d=static_cast<Direction>(i);if(adjacentTile(from,d)==object.tile&&page->definition.layer==1){InputFrame turn;turn.movement=d;tick(turn);tick(press());if(game.foreground()!=ForegroundKind::None){settle();return;}}}
   if(page->definition.layer!=1&&from==object.tile){tick(press());settle();return;}
   std::optional<Direction> move;
   if(page->definition.layer==1){for(int i=0;i<4&&!move;++i){move=nextStep(game,adjacentTile(object.tile,static_cast<Direction>(i)));}}
   else{move=nextStep(game,object.tile);}InputFrame f;f.movement=move;tick(f);
  }
  throw std::runtime_error("44 navigation unreachable "+id+" target "+std::to_string(destination));
 };
 for(const auto& [id,destination]:std::vector<std::pair<std::string,int>>{{"m16-e4",17},{"m17-e7",17},{"m17-e2",16},{"m16-e2",15},{"m15-e3",13},{"m13-e7",13},{"m13-e7",18},{"m18-e11",18},{"m18-e8",13},{"m13-e4",15},{"m15-e6",16}}){
  action(id,destination);if(game.mapId()!=destination){throw std::runtime_error("44 route wrong destination after "+id);}std::cout<<id<<" -> "<<game.mapId()<<"\n";
 }
 if(!game.session().flag(59)||!game.session().flag(60)||game.session().count(60)!=0){throw std::runtime_error("Natural key pickup/discard/unlock failed");}
 if(!game.session().flag(55)||!game.session().flag(58)||!game.session().flag(18)||!game.playerVisible()){throw std::runtime_error("Hall intro failed to restore session/control");}
 if(!observedEnemyMove){throw std::runtime_error("Route did not exercise enabled enemy movement");}
 std::cout<<"Actual-map route including open side rooms completed in "<<ticks<<" updates; health fixture, not user playtest\n";
}
