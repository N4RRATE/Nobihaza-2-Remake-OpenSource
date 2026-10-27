#include "title_navigation.h"
#include "game_application.h"
#include <iostream>
#include <stdexcept>
#include <queue>
#include <map>
#include "scene_presentation.h"
void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
InputFrame press(InputAction a){InputFrame i;i.buttons[static_cast<int>(a)].pressed=true;return i;}
std::optional<Direction> nextMove(const GameApplication& a,TilePosition goal){
 auto actor=a.world().player();const auto start=actor.motion.tile();std::queue<TilePosition> q;q.push(start);std::map<std::pair<int,int>,std::optional<Direction>> seen;seen[{start.x,start.y}]={};
 while(!q.empty()){auto at=q.front();q.pop();if(at==goal)return seen.at({at.x,at.y});
 for(int d=0;d<4;++d){const auto dir=static_cast<Direction>(d);actor.motion=CharacterMotion(at,dir,32);if(!a.world().canMove(actor,dir))continue;auto t=adjacentTile(at,dir);auto key=std::pair{t.x,t.y};if(seen.contains(key))continue;seen[key]=seen.at({at.x,at.y}).value_or(dir);q.push(t);}}
 throw std::runtime_error("unreachable conversation position");
}
void approach(GameApplication& a,const std::string& id){
 std::cerr<<"approach "<<id<<" from "<<a.world().player().motion.tile().x<<","<<a.world().player().motion.tile().y<<"\n";const auto actor=findCharacter(a.world(),id);TilePosition target{};Direction facing{};bool found=false;
 for(int d=0;d<4&&!found;++d){facing=static_cast<Direction>(d);target=actor.layer==1?adjacentTile(actor.motion.tile(),static_cast<Direction>((d+2)%4)):actor.motion.tile();try{nextMove(a,target);found=true;}catch(const std::exception&){} }
 require(found,"NPC physically approachable");
 for(int n=0;n<10000&&(a.world().player().motion.moving()||a.world().player().motion.tile()!=target);++n){InputFrame i;if(!a.world().player().motion.moving())i.movement=nextMove(a,target);a.update(i);a.takePresentation();}
 require(a.world().player().motion.tile()==target,"Reached NPC through real collision");if(a.world().player().motion.facing()!=facing){InputFrame i;i.movement=facing;a.update(i);}for(int n=0;n<50&&a.world().player().motion.moving();++n)a.update({});
}
void finishTalk(GameApplication& a){for(int n=0;n<15000&&a.foreground()!=ForegroundKind::None;++n){a.update(n%11==0?press(InputAction::Confirm):InputFrame{});a.takePresentation();}require(a.foreground()==ForegroundKind::None,"Conversation completes");}
void talk(GameApplication& a,const std::string& id){approach(a,id);a.update(press(InputAction::Confirm));finishTalk(a);}
int main(int argc,char**argv){try{
 if(argc!=4)return 2;auto c=loadCombatContent(argv[1]);const std::filesystem::path dir=argv[2];std::string mode=argv[3];
 for(bool gun:{false,true}){
  const auto slot=gun?2:1;
  if(mode=="write"){
   // Start at the already-verified vent exit with lawful prerequisite facts.
   // NORMAL attributes and original enemy simulation remain unchanged.
   auto e=c.entries.at("defeated-claimed");e.map=23;e.playerTile={3,8};e.direction=Direction::Down;
   e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();
   for(int f:{58,59,60,66,67,68,71,72})e.session.flags[f]=true;e.session.inventory[62]=1;
   c.entries["64-vent-exit-fixture"]=e;GameApplication a(c,"64-vent-exit-fixture",[](int low,int){return low;});a.configureSaveDirectory(dir);finishTitleOpening(a);
   a.update(press(InputAction::Confirm));for(int n=0;n<1000&&a.mapId()!=24;++n){a.update({});a.takePresentation();}
   for(int n=0;n<20000&&!a.session().flag(73);++n){a.update(n%11==0?press(InputAction::Confirm):InputFrame{});a.takePresentation();}
   require(a.mapId()==24&&a.session().flag(73),"Actual vent exit completes meeting-room entry");
   for(int id:{2,6,10,5,4,3,7,8})talk(a,"m24-e"+std::to_string(id));
   if(gun)talk(a,"m24-e11");
   const int ammo=a.session().count(32);talk(a,"m24-e2");
   require(a.mapId()==12&&a.session().flag(84)&&a.session().count(32)==ammo+1,"Actual conference and return complete once");
   require(a.session().count(14)==int(gun)&&a.session().flag(74)==gun,"Skipping optional shotgun does not block conference");
   for(auto [id,map]:std::initializer_list<std::pair<const char*,int>>{{"m12-e8",13},{"m13-e4",15},{"m15-e6",16}}){
    talk(a,id);require(a.mapId()==map,"Actual old-area transfer reaches next map");
   }
   require(a.session().knowsSkill(3)&&!a.session().knowsSkill(4),"Real return to safe room restores mixing");
   approach(a,"m16-e5");a.update(press(InputAction::Confirm));require(a.mode()==ApplicationMode::Saving,"Natural return arrives at crow");
   for(int n=0;n<200;++n)a.update({});a.update(press(InputAction::Confirm));if(gun)a.update(press(InputAction::Down));a.update(press(InputAction::Confirm));
   require(a.mode()==ApplicationMode::Playing&&a.session().flag(84),"Story progress saved at original legal point");
  }else{
   GameApplication a(c,"title");a.configureSaveDirectory(dir);finishTitleOpening(a);a.update(press(InputAction::Confirm));if(gun)a.update(press(InputAction::Down));a.update(press(InputAction::Confirm));
   require(a.hasGame()&&a.mapId()==16&&a.session().flag(84)&&a.session().count(14)==int(gun),"Separate process resumes completed story and optional choice");
   const auto boxes=a.session().count(32);for(int n=0;n<120;++n)a.update({});require(a.foreground()==ForegroundKind::None&&a.session().count(32)==boxes,"Load never repeats story reward");
  }
 }
 std::cout<<"F4 story "<<mode<<" passed, optional gun both branches\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
