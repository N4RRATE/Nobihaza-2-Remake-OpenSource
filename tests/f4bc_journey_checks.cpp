#include "title_navigation.h"
#include "game_application.h"
#include <queue>
#include <iostream>
#include <fstream>
#include <stdexcept>
using Point=std::pair<int,int>;
void need(bool b,const std::string& why){if(!b){throw std::runtime_error(why);}}
InputFrame z(){InputFrame f;f.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;return f;}
class Journey{
public:
 const CombatContent& c;GameSession state;CombatScene room;int ticks{};
 Journey(const CombatContent& content,const CombatEntry& e):c(content),state(c.items,e.session,c.facts),room(c,state,e,[](int low,int){return low;}){}
 void tick(InputFrame f={}){if(state.flag(10)){f.buttons[static_cast<int>(InputAction::Left)].pressed=true;}room.update(f);++ticks;need(state.hp()>0,"Navigation fixture died");}
 void settle(){for(int n=0;n<20000;++n){if(room.foreground()==ForegroundKind::None&&!room.world().player().motion.moving()){return;}tick(n%12==0?z():InputFrame{});}throw std::runtime_error("Foreground stuck");}
 std::optional<Direction> next(TilePosition target){
  const auto& w=room.world();const auto start=w.player().motion.tile();if(start==target){return {};}
  std::queue<TilePosition> q;q.push(start);std::map<Point,Direction> first;first[{start.x,start.y}]=Direction::Up;
  while(!q.empty()){auto pos=q.front();q.pop();auto actor=w.player();actor.motion=CharacterMotion(pos,Direction::Down,32);
   for(int n=0;n<4;++n){auto d=static_cast<Direction>(n);auto dest=adjacentTile(pos,d);Point key{dest.x,dest.y};if(first.contains(key)||!w.canMove(actor,d)){continue;}first[key]=pos==start?d:first.at({pos.x,pos.y});if(dest==target){return first[key];}q.push(dest);}}
  return {};
 }
 void walk(TilePosition target){const int map=room.mapId();for(int n=0;n<20000;++n){settle();need(room.mapId()==map,"Walk unexpectedly transferred");if(room.world().player().motion.tile()==target){return;}InputFrame f;f.movement=next(target);tick(f);}throw std::runtime_error("Walk blocked map="+std::to_string(map));}
 void clearBlock(){
  for(auto d:{Direction::Up,Direction::Right,Direction::Down,Direction::Left}){
   for(int n=0;n<90;++n){InputFrame f;f.movement=d;f.buttons[static_cast<int>(InputAction::Weapon)].held=true;tick(f);}tick();
   InputFrame f;f.buttons[static_cast<int>(state.magazine(21)>0?InputAction::Attack:InputAction::Cancel)].pressed=true;tick(f);for(int n=0;n<140;++n){tick();}
  }
  if(room.raised()){for(int n=0;n<8;++n){InputFrame f;f.buttons[static_cast<int>(InputAction::Weapon)].held=true;tick(f);}for(int n=0;n<60;++n){tick();}}
 }
 void interact(const std::string& id){const auto& object=c.objects.at(id);const int origin=room.mapId();need(object.map==origin,"Wrong interaction map");
  for(int n=0;n<20000;++n){settle();if(room.mapId()!=origin){return;}const auto* page=c.activeVariant(object,state);need(page,"No active interaction page");auto from=room.world().player().motion.tile();
   if(page->definition.layer==1){for(int i=0;i<4;++i){auto d=static_cast<Direction>(i);if(adjacentTile(from,d)==object.tile){InputFrame turn;turn.movement=d;tick(turn);tick(z());settle();std::cout<<id<<" -> "<<room.mapId()<<std::endl;return;}}}
   else if(from==object.tile){tick(z());settle();return;}
   std::optional<Direction> direction;if(page->definition.layer==1){for(int i=0;i<4&&!direction;++i){direction=next(adjacentTile(object.tile,static_cast<Direction>(i)));}}else{direction=next(object.tile);}if(n%600==599){clearBlock();continue;}InputFrame f;f.movement=direction;tick(f);
  }throw std::runtime_error("Interaction blocked: "+id+" at "+std::to_string(room.world().player().motion.tile().x)+","+std::to_string(room.world().player().motion.tile().y)+" flags1/18="+std::to_string(state.flag(1))+std::to_string(state.flag(18)));
 }
 void travel(int target){for(int hop=0;hop<40&&room.mapId()!=target;++hop){
  const int start=room.mapId();std::queue<int> q;q.push(start);std::map<int,std::string> first;first[start]="";
  while(!q.empty()&&!first.contains(target)){const int map=q.front();q.pop();for(const auto& [id,t]:c.transitions.items()){const auto& object=c.objects.at(t.at("object").get<std::string>());if(object.map!=map||!t.value("runtime_enabled",true)){continue;}const auto* page=c.activeVariant(object,state);if(!page||page->page!=t.at("page")){continue;}const int destination=t.at("destination").at("map");if(first.contains(destination)){continue;}first[destination]=map==start?object.id:first.at(map);q.push(destination);}}
  need(first.contains(target),"No active graph route to "+std::to_string(target));interact(first.at(target));need(room.mapId()!=start,"Transfer refused "+first.at(target));
 }need(room.mapId()==target,"Travel hop limit");}
};
int main(int argc,char** argv){try{
 if(argc!=4){return 2;}const auto c=loadCombatContent(argv[1]);const std::filesystem::path root=argv[2];const std::string mode=argv[3];
 if(mode=="write"){
  auto e=c.entries.at("defeated-claimed");e.map=50;e.playerTile={5,5};e.direction=Direction::Up;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();
  // Navigation endurance fixture only; original collision, AI and all story commits remain active.
  for(int f:{58,66,67,68,71,72,61,57,73,75,76,77,78,79,80,81,82,84,87,88,93}){e.session.flags[f]=true;}e.session.inventory[62]=1;e.session.skills.insert(3);e.session.skills.erase(4);e.session.hp=e.session.maxHp=999;e.session.mainWeapon=21;e.session.magazines[21]=8;e.session.inventory[42]=99;Journey j(c,e);j.interact("m50-e3");need(j.state.count(87)==1&&j.state.flag(316),"Tooth must be obtained by original choice");
  j.travel(96);j.walk({7,7});j.tick();auto tool=j.room.requestToolUse();need(tool&&j.room.useTool(*tool),"Natural tooth request");j.settle();need(j.state.flag(318)&&j.state.count(91)==1&&!j.state.flag(274),"Gem commits before rescue");
  j.travel(95);for(int n=0;n<30000&&!j.state.flag(274);++n){j.tick(n%12==0?z():InputFrame{});}j.settle();need(j.state.flag(337)&&j.state.flag(213)&&j.state.count(6)==3,"Unassisted natural arrival triggers source intervention");
  j.travel(34);j.travel(27);j.travel(26);j.travel(16);j.travel(12);j.travel(25);j.interact("m25-e13");j.interact("m25-e13");need(j.state.count(12)==0&&j.state.count(27)==0,"Intervention return must not grant timely weapons");
  j.travel(16);j.travel(26);j.travel(27);j.travel(34);j.travel(37);j.walk({10,5});j.tick();tool=j.room.requestToolUse();need(tool&&j.room.useTool(*tool),"Returned gemstone reaches blue eye consumer");j.settle();need(j.state.flag(314)&&j.state.count(91)==0&&j.state.count(92)==0,"Blue eye committed once");
  j.travel(34);j.travel(27);j.travel(26);j.travel(16);j.walk({10,21});InputFrame up;up.movement=Direction::Up;j.tick(up);j.tick(z());auto request=j.room.takeSaveRequest();need(request.has_value(),"Actual crow dispatches legal save request");
  auto snapshot=captureSave(c,j.room,*request,{});SaveFiles files(c,root);need(files.write(1,snapshot).outcome==WriteOutcome::Committed,"Journey save transaction committed");std::cout<<"Blue route public input chain saved; endurance fixture "<<j.ticks<<" ticks\n";
  // Solved-puzzle precondition isolates its real key -> crisis -> return consumers.
  e.map=49;e.playerTile={10,5};e.direction=Direction::Up;e.session.inventory[68]=1;
  for(int f:{145,141,142,143}){e.session.flags[f]=true;}
  Journey black(c,e);black.interact("m49-e3");need(black.state.flag(144)&&black.state.count(70)==1,"Actual revealed puzzle grants key");
  black.travel(63);need(!black.state.flag(274),"Crisis is still pending on arrival");black.travel(34);need(!black.state.flag(274),"Actual exit leaves Gian rescue pending");black.travel(27);black.travel(26);black.travel(16);black.walk({10,21});black.tick(up);black.tick(z());auto pending=black.room.takeSaveRequest();need(pending.has_value(),"Pending rescue reaches real crow");need(files.write(2,captureSave(c,black.room,*pending,{})).outcome==WriteOutcome::Committed,"Unfinished Gian request can save after actual escape");
  black.travel(26);black.travel(27);black.travel(34);black.travel(63);
  for(int n=0;n<30000&&!black.state.flag(274);++n){black.tick(n%12==0?z():InputFrame{});}black.settle();need(black.state.flag(272)&&black.state.count(29)==3,"Reentered crisis resolves actual intervention and reward");
  black.travel(34);black.travel(27);black.travel(26);black.travel(16);black.travel(12);black.travel(25);black.interact("m25-e13");black.interact("m25-e13");need(black.state.count(12)==0,"Intervened Gian return does not grant SG");black.travel(16);black.walk({10,21});black.tick(up);black.tick(z());auto complete=black.room.takeSaveRequest();need(complete.has_value(),"Completed black route reaches crow");need(files.write(3,captureSave(c,black.room,*complete,{})).outcome==WriteOutcome::Committed,"Completed black route saves");std::cout<<"Black key, escape, reentry and return chain saved; endurance fixture "<<black.ticks<<" ticks\n";

 }else if(mode=="load"){
  GameApplication game(c,"title");game.configureSaveDirectory(root);finishTitleOpening(game);game.update(z());need(game.mode()==ApplicationMode::Loading,"Fresh process title Load");game.update(z());need(game.mode()==ApplicationMode::Playing&&game.mapId()==16,"Load returns real crow map");
  need(game.session().flag(316)&&game.session().flag(318)&&game.session().flag(337)&&game.session().flag(274)&&game.session().flag(314),"Continuous route facts survive process exit");need(game.session().count(91)==0&&game.session().count(92)==0&&game.session().count(6)==3,"Load does not replay rescue reward or gem");
  const auto before=game.session().exportState();std::string error;const auto save=SaveFiles(c,root).read(1);need(game.restore(save,error)&&game.session().exportState()==before,"Repeated Load deterministic");need(game.newGame(error)&&!game.session().flag(274)&&!game.session().flag(314)&&game.session().count(6)==0,"Fresh game clears previous route");
  InputFrame down;down.buttons[static_cast<int>(InputAction::Down)].pressed=true;
  for(int slot:{2,3}){GameApplication other(c,"title");other.configureSaveDirectory(root);finishTitleOpening(other);other.update(z());for(int n=1;n<slot;++n){other.update(down);}other.update(z());need(other.mode()==ApplicationMode::Playing&&other.mapId()==16,"Black route title Load");need(other.session().flag(144)&&other.session().count(70)==1&&other.session().flag(274)==(slot==3)&&other.session().flag(272)==(slot==3)&&other.session().count(29)==(slot==3?3:0),"Pending or completed black crisis retains exact state");}
  std::cout<<"Cross-process three route states, Load and new game passed\n";
 }else{return 2;}
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
