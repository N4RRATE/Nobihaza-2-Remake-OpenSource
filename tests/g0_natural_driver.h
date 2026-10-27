#pragma once
#include "game_application.h"
#include "enemy_host.h"
#include <iostream>
#include <fstream>
#include <queue>
#include <cmath>
#include <stdexcept>
namespace g0natural{
inline void need(bool ok,const std::string& why){if(!ok){throw std::runtime_error(why);}}
inline InputFrame key(InputAction a=InputAction::Confirm){InputFrame f;f.buttons[static_cast<int>(a)]={true,true,false};return f;}
inline std::string read(const char* p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
inline int safe(int lo,int hi){if(lo==0&&hi==10){return 5;}if(lo==0&&hi==15){return 9;}return hi;}
inline std::optional<Direction> next(const ExplorationScene& w,TilePosition goal){
 auto start=w.player().motion.tile();std::queue<TilePosition> q;q.push(start);std::map<std::pair<int,int>,std::optional<Direction>> first;first[{start.x,start.y}]={};
 while(!q.empty()){auto at=q.front();q.pop();if(at==goal){return first.at({at.x,at.y});}for(auto d:{Direction::Up,Direction::Right,Direction::Down,Direction::Left}){auto p=w.player();p.motion=CharacterMotion(at,d,p.motion.speed());auto t=adjacentTile(at,d);auto cell=std::pair{t.x,t.y};if(first.contains(cell)||!w.canMove(p,d)){continue;}first[cell]=first.at({at.x,at.y}).value_or(d);q.push(t);}}return {};
}

template<class Driver>
inline void driveVictory(Driver& scene,const GameSession& s,const CombatContent& c,const std::function<void(const InputFrame&)>& record={},const std::function<void()>& prepareBattleTick={},const std::function<int(const std::string&)>& extraCueCount={}){
 std::map<std::string,int> shown;int ticks=0;
 const auto advance=[&](InputFrame in=InputFrame{}){if(record){record(in);}scene.update(in);++ticks;for(const auto& v:scene.takePresentation()){if(v.kind==PresentationEvent::Kind::Sound&&v.sound.resource==readSound(c.cue("m120-e13-p18-c4").at("sound")).resource){++shown["m120-e13-p18-c4"];}if(v.kind==PresentationEvent::Kind::Cue){++shown[v.id];}}need(s.hp()>0,"Natural battle died hp0 damage="+std::to_string(s.variable(89))+" ticks="+std::to_string(ticks)+" raised="+std::to_string(scene.raised())+" S45="+std::to_string(s.flag(45))+" S148="+std::to_string(s.flag(148)));};
 const auto walk=[&](TilePosition goal){int map=scene.mapId();for(int n=0;n<50000;++n){if(scene.mapId()!=map||s.flag(349)){return;}if(scene.foreground()!=ForegroundKind::None||scene.dialogue().active()){advance(n%8==0?key():InputFrame{});continue;}if(scene.world().player().motion.moving()){advance();continue;}if(scene.world().player().motion.tile()==goal){return;}InputFrame f;f.movement=next(scene.world(),goal);need(f.movement.has_value(),"No natural route");advance(f);}throw std::runtime_error("Natural walk did not settle");};
 walk({11,21});InputFrame leave;leave.movement=Direction::Down;advance(leave);for(int n=0;n<1000&&scene.mapId()==122;++n){advance();}need(scene.mapId()==120,"Natural save never left crow");walk({36,50});walk(c.objects.at("m120-e20").tile);for(int n=0;n<20000&&!s.flag(349);++n){advance(n%8==0?key():InputFrame{});}for(int n=0;n<40;++n){advance();}need(s.flag(349)&&!s.flag(350),"Natural intro did not enter battle");shown.clear();const auto before=s.exportState();int shots=0;
 if(s.difficulty()=="hard"){
  const auto origin=findCharacter(scene.world(),"m120-e13").motion.tile();const TilePosition firingLine{origin.x,origin.y+2};
  for(int n=0;n<2000&&(scene.world().player().motion.moving()||scene.world().player().motion.tile()!=firingLine);++n){InputFrame in;if(!scene.world().player().motion.moving()){in.movement=next(scene.world(),firingLine);need(in.movement.has_value(),"Actual source firing line is unreachable");}advance(in);}
  need(scene.world().player().motion.tile()==firingLine,"Public firing-line walk did not settle");
 }
 for(int n=0;n<18000&&scene.mapId()==120&&!s.flag(350);++n){
  if(prepareBattleTick){prepareBattleTick();}if(scene.mapId()!=120||s.flag(350)){break;}
  InputFrame input;const bool auxiliary=s.difficulty()=="hard"&&s.equipped(1)==11&&s.equipped(2)==29;const auto posture=auxiliary?InputAction::AuxiliaryWeapon:InputAction::Weapon;
  if(n%500==0){std::cout<<"Battle tick="<<n<<" HP="<<s.hp()<<" damage="<<s.variable(89)<<" raised="<<scene.raised()<<" S17="<<s.flag(17)<<" stock5="<<s.count(5)<<" green="<<s.count(2)<<" S1/18/45/148="<<s.flag(1)<<s.flag(18)<<s.flag(45)<<s.flag(148)<<" weapon="<<s.equipped(1)<<" magazine="<<s.magazine(s.equipped(1))<<" shots="<<shots<<" player="<<scene.world().player().motion.tile().x<<","<<scene.world().player().motion.tile().y<<" moving="<<scene.world().player().motion.moving()<<" enemy="<<findCharacter(scene.world(),"m120-e13").motion.tile().x<<","<<findCharacter(scene.world(),"m120-e13").motion.tile().y<<" body="<<s.variable(83)<<","<<s.variable(84)<<" camera="<<scene.cameraCenter().x<<","<<scene.cameraCenter().y<<"\n";}
  if(s.flag(45)){if(n%8==0){input=key(InputAction::Up);}}
  else if(scene.foreground()!=ForegroundKind::None||scene.dialogue().active()){if(n%8==0){input=key();}}
  else if(!scene.raised()){input=key(posture);}
  else{
   auto player=scene.world().player().motion.tile();const auto camera=scene.cameraCenter();const TilePosition enemy{int(std::round((s.variable(83)+camera.x-160-8)/16)),int(std::round((s.variable(84)+camera.y-120-8)/16))};const int dx=enemy.x-player.x,dy=enemy.y-player.y;Direction facing=std::abs(dx)>std::abs(dy)?(dx>0?Direction::Right:Direction::Left):(dy>0?Direction::Down:Direction::Up);
   if(!scene.world().player().motion.moving()){
    const auto actor=findCharacter(scene.world(),"m120-e13").motion.tile();
    const bool evade=s.difficulty()=="hard"&&s.flag(37);
    if(evade){
     int best=std::abs(actor.x-player.x)+std::abs(actor.y-player.y);
     if(best<5){for(Direction direction:{Direction::Down,Direction::Left,Direction::Right,Direction::Up}){const auto to=adjacentTile(player,direction);const int distance=std::abs(actor.x-to.x)+std::abs(actor.y-to.y);if(distance>best&&scene.world().canMove(scene.world().player(),direction)){best=distance;input.movement=direction;}}}
    }else if(dx&&dy){
     for(int offset:{0,1,-1,2,-2}){input.movement=next(scene.world(),{enemy.x,player.y+offset});if(input.movement){break;}}
     if(!input.movement){input.movement=next(scene.world(),{player.x,enemy.y});}
    }
    if(!input.movement&&!evade){
     if(scene.world().player().motion.facing()!=facing){input=key(posture);input.movement=facing;}
     else if(!auxiliary&&s.equipped(1)==17&&s.flag(37)){} // Original burn rejects another flame hit until S37 clears.
     else if(!auxiliary&&s.equipped(1)==11&&s.difficulty()=="hard"){
      input.buttons[int(posture)].held=true;
      if(scene.aiming()&&n%80==0){input.buttons[int(InputAction::Attack)]={true,true,false};++shots;}
     }
     else if(!auxiliary&&s.equipped(1)==19){input.buttons[int(InputAction::Attack)].held=true;if(n%80==0){input.buttons[int(InputAction::Attack)].pressed=true;++shots;}}
     else if(n%80==0){input=key(InputAction::Attack);++shots;}
    }
   }
  }
  advance(input);
 }

 need(s.flag(350),"Natural battle did not win");
 const auto ammo=s.exportState().magazines;
 for(int n=0;n<6000&&(scene.mapId()!=122||s.variable(89)!=0);++n){advance(n%8==0?key():InputFrame{});}
 need(scene.mapId()==122&&scene.foreground()==ForegroundKind::Content&&s.exportState().magazines==ammo,"Natural victory did not reach occupied M122 with real ammunition");
 need(shown["m120-e13-p18-c4"]+(extraCueCount?extraCueCount("m120-e13-p18-c4"):0)==1&&shown["m120-e13-p18-c40"]+(extraCueCount?extraCueCount("m120-e13-p18-c40"):0)==1,"Natural victory body/corpse cues differ");
 for(const auto& pair:c.catalog.at("g0").at("reading_pairs")){int a=pair[0],b=pair[1],oldA=before.inventory.contains(a)?before.inventory.at(a):0,oldB=before.inventory.contains(b)?before.inventory.at(b):0;need(s.count(a)==std::min(99,oldA+(oldB>0))&&s.count(b)==std::max(0,oldB-1),"Natural victory CE167 mismatch");}
 std::cout<<"Natural03 -> E20/E19 -> actual weapon battle -> occupied M122: ticks="<<ticks<<" shotRequests="<<shots<<" weapon="<<s.equipped(1)<<" magazine="<<s.magazine(s.equipped(1))<<"\n";
}

inline PreparedRestore reachVictory(const CombatContent& c,const SaveSnapshot& saved){
 auto ready=prepareRestore(c,saved);auto& s=*ready.session;CombatEntry restored=c.entries.at("post-pickup");restored.map=saved.scene.map;restored.playerTile=saved.scene.playerTile;restored.direction=saved.scene.direction;restored.sprite=saved.scene.sprite;restored.positions=saved.scene.positions;restored.directions=saved.scene.directions;restored.session=s.exportState();ready.scene=std::make_unique<CombatScene>(c,s,restored,safe);auto& scene=*ready.scene;need(s.equip(21),"Natural save lacks its original DE");driveVictory(scene,s,c);
 return ready;
}
}
