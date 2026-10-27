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
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);auto entry=c.entries.at("defeated-claimed");
 entry.map=18;entry.playerTile={17,6};entry.direction=Direction::Right;entry.pages.clear();entry.positions.clear();entry.through.clear();entry.directions.clear();entry.session.flags[56]=true;
 // Navigation fixture: extended health only. Original enemy movement, grabs and collisions remain active.
 entry.session.hp=10000;entry.session.maxHp=10000;c.entries["f4-route-fixture"]=entry;GameApplication game(c,"f4-route-fixture",[](int low,int){return low;});
 bool moved=false,grabbed=false;int ticks=0;
 auto tick=[&](InputFrame f){if(game.session().flag(10)){f.buttons[static_cast<int>(InputAction::Left)].pressed=true;grabbed=true;}game.update(f);++ticks;
  for(const auto& object:game.world().objects()){if(c.expansion.enemies.contains(object.id)&&object.active&&object.motion.tile()!=c.objects.at(object.id).tile){moved=true;}}
  if(game.mode()!=ApplicationMode::Playing){throw std::runtime_error("Unexpected route application mode");}};
 auto settle=[&](){for(int i=0;i<4000;++i){if(game.foreground()==ForegroundKind::None&&!game.world().player().motion.moving()){return;}tick(i%12==0?press():InputFrame{});}throw std::runtime_error("Route foreground stuck");};
 auto interact=[&](const std::string& id,int destination){
  const auto& object=c.objects.at(id);const auto origin=game.mapId();bool submitted=false;
  for(int attempt=0;attempt<18000;++attempt){
   if(game.mapId()!=origin){settle();break;}
   if(game.foreground()!=ForegroundKind::None){settle();if(submitted){break;}continue;}
   if(game.world().player().motion.moving()){tick({});continue;}
   const auto* page=c.activeVariant(object,game.session());const auto from=game.world().player().motion.tile();
   bool adjacent=false;for(int i=0;i<4;++i){auto d=static_cast<Direction>(i);if(adjacentTile(from,d)==object.tile&&page->definition.layer==1){InputFrame turn;turn.movement=d;tick(turn);tick(press());submitted=true;adjacent=true;break;}}
   if(adjacent){if(game.foreground()!=ForegroundKind::None){settle();break;}continue;}
   if(page->definition.layer!=1&&from==object.tile){tick(press());submitted=true;settle();break;}
   std::optional<Direction> move;
   if(page->definition.layer==1){for(int i=0;i<4&&!move;++i){move=nextStep(game,adjacentTile(object.tile,static_cast<Direction>(i)));}}
   else{move=nextStep(game,object.tile);}InputFrame f;f.movement=move;tick(f);
  }
  if(game.mapId()!=destination){throw std::runtime_error("Wrong destination after "+id+" got "+std::to_string(game.mapId()));}
  std::cout<<id<<" -> "<<game.mapId()<<" hp="<<game.session().hp()<<"\n";
 };
 for(const auto& [id,map]:std::vector<std::pair<std::string,int>>{{"m18-e9",19},{"m19-e15",20},{"m20-e2",21},{"m21-e9",22},{"m22-e4",22},{"m22-e3",21},{"m21-e11",21},{"m21-e11",10},{"m10-e11",21},{"m21-e9",22}}){interact(id,map);}
 if(!game.session().flag(57)||!game.session().flag(61)||!game.session().flag(66)||game.session().count(61)!=1||!moved){throw std::runtime_error("Natural route facts/key/enemy movement missing");}
 std::cout<<"F4 navigation fixture passed "<<ticks<<" ticks, grabbed="<<grabbed<<"; not NORMAL player acceptance\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
