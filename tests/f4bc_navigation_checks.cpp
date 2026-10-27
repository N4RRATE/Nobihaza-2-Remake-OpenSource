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
 if(argc!=2&&argc!=3){return 2;}const bool wardrobe=argc==3;auto c=loadCombatContent(argv[1]);auto entry=c.entries.at("defeated-claimed");
 entry.map=16;entry.playerTile={3,26};entry.direction=Direction::Left;entry.pages.clear();entry.positions.clear();entry.through.clear();entry.directions.clear();entry.session.flags[56]=true;entry.session.flags[84]=true;
 // Navigation fixture: extended health only. Original enemy movement, grabs and collisions remain active.
 if(wardrobe){entry.map=28;entry.playerTile={10,4};entry.direction=Direction::Left;entry.session.mainWeapon=21;entry.session.magazines[21]=8;}
 entry.session.hp=10000;entry.session.maxHp=10000;c.entries["f4-route-fixture"]=entry;GameApplication game(c,"f4-route-fixture",[](int low,int){return low;});
 bool moved=false,grabbed=false;int ticks=0;
 auto tick=[&](InputFrame f){if(game.session().flag(10)){f.buttons[static_cast<int>(InputAction::Left)].pressed=true;grabbed=true;}game.update(f);++ticks;
  for(const auto& object:game.world().objects()){if((c.expansion.enemies.contains(object.id)||c.enemies.contains(object.id))&&object.active&&object.motion.tile()!=c.objects.at(object.id).tile){moved=true;}}
  if(game.mode()!=ApplicationMode::Playing){throw std::runtime_error("Unexpected route mode "+std::to_string(static_cast<int>(game.mode()))+" map="+std::to_string(game.mapId())+" tile="+std::to_string(game.world().player().motion.tile().x)+","+std::to_string(game.world().player().motion.tile().y));}};
 auto settle=[&](){for(int i=0;i<4000;++i){if(game.foreground()==ForegroundKind::None&&!game.world().player().motion.moving()){return;}tick(i%12==0?press():InputFrame{});}throw std::runtime_error("Route foreground stuck");};
 auto interact=[&](const std::string& id,int destination,std::optional<Direction> facing={}){
  const auto& object=c.objects.at(id);const auto origin=game.mapId();bool submitted=false;
  for(int attempt=0;attempt<18000;++attempt){
   if(game.mapId()!=origin){settle();break;}
   if(game.foreground()!=ForegroundKind::None){settle();if(submitted){break;}continue;}
   if(game.world().player().motion.moving()){tick({});continue;}
   const auto* page=c.activeVariant(object,game.session());const auto from=game.world().player().motion.tile();
   bool adjacent=false;for(int i=0;i<4;++i){auto d=static_cast<Direction>(i);if(adjacentTile(from,d)==object.tile&&page->definition.layer==1&&(!facing||d==*facing)){InputFrame turn;turn.movement=d;tick(turn);tick(press());submitted=true;adjacent=true;break;}}
   if(adjacent){if(game.foreground()!=ForegroundKind::None){settle();break;}continue;}
   if(page->definition.layer!=1&&from==object.tile){tick(press());submitted=true;settle();break;}
   std::optional<Direction> move;
   if(page->definition.layer==1){if(facing){move=nextStep(game,adjacentTile(object.tile,static_cast<Direction>((static_cast<int>(*facing)+2)%4)));}else{for(int i=0;i<4&&!move;++i){move=nextStep(game,adjacentTile(object.tile,static_cast<Direction>(i)));}}}
   else{move=nextStep(game,object.tile);}InputFrame f;f.movement=move;tick(f);
  }
  if(game.mapId()!=destination){throw std::runtime_error("Wrong destination after "+id+" got "+std::to_string(game.mapId()));}
  if(destination==30){
   for(int n=0;n<2000&&game.world().player().motion.tile()!=TilePosition{14,17};++n){
    if(game.foreground()!=ForegroundKind::None){settle();continue;}InputFrame f;
    if(!game.world().player().motion.moving()){f.movement=nextStep(game,{14,17});}tick(f);
   }
  }
  std::cout<<id<<" -> "<<game.mapId()<<" hp="<<game.session().hp()<<"\n";
 };
 if(wardrobe){
  InputFrame push;push.movement=Direction::Left;tick(push);settle();if(!game.session().flag(97)){throw std::runtime_error("Wardrobe did not open");}
  interact("m28-e4",43);interact("m43-e3",44);interact("m44-e12",44,Direction::Up);
  if(game.session().count(68)!=1){throw std::runtime_error("Natural staff-room pickup missing");}
  // Armed navigation fixture: clear the blocking real enemies through normal input.
  for(auto direction:{Direction::Right,Direction::Up,Direction::Down}){
   for(int n=0;n<90;++n){InputFrame aim;aim.movement=direction;aim.buttons[static_cast<int>(InputAction::Weapon)].held=true;tick(aim);}
   tick({});InputFrame shot;shot.buttons[static_cast<int>(InputAction::Attack)].pressed=true;tick(shot);
   for(int n=0;n<140;++n){tick({});}
  }
  if(game.raised()){for(int n=0;n<8;++n){InputFrame lower;lower.buttons[static_cast<int>(InputAction::Weapon)].held=true;tick(lower);}for(int n=0;n<60;++n){tick({});}}
  interact("m44-e14",92);interact("m92-e2",44);interact("m44-e10",43);interact("m43-e2",28);
 }else for(const auto& [id,map]:std::vector<std::pair<std::string,int>>{{"m16-e15",26},{"m26-e8",27},{"m27-e11",28},{"m28-e2",27},{"m27-e13",29},{"m29-e3",30},{"m30-e2",29},{"m29-e2",27},{"m27-e2",26},{"m26-e5",16}}){interact(id,map);}
 if(!moved){throw std::runtime_error("Route bypassed active enemy movement");}
 std::cout<<"F4-B/C initial navigation fixture passed "<<ticks<<" ticks, grabbed="<<grabbed<<"; not NORMAL player acceptance\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
