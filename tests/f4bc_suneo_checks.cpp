#include "combat_scene.h"
#include "enemy_controller.h"
#include <iostream>
#include <stdexcept>
void require(bool b,const char* m){if(!b){throw std::runtime_error(m);}}
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}const auto c=loadCombatContent(argv[1]);require(c.enemies.contains("m95-e12"),"Suneo requires actual rescue enemy");
 for(bool timely:{false,true}){
  auto e=c.entries.at("defeated-claimed");e.map=95;e.playerTile={3,12};e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.flags[316]=true;e.session.flags[318]=true;e.session.inventory[91]=1;
  GameSession state(c.items,e.session,c.facts);CombatScene room(c,state,e,[](int low,int){return low;});room.update({});
  require(room.enemyStates().size()==1,"Suneo instance is independent");
  if(timely){const auto p=findCharacter(room.world(),"m95-e12").motion.position();std::set<std::string> seen;require(room.resolveAttack({{(p.x+.5)*16,(p.y+.5)*16},{0,0},AttackKind::Magnum,5,0,false},seen).processed==1,"Actual weapon ends rescue enemy");}
  InputFrame z;z.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;
  bool endingWithSimulationDisabled=false,picture10Shown=false,picture10Erased=false;int clearStart=-1,clearEnd=-1;
  room.takePresentation();
  for(int n=0;n<30000&&!state.flag(274);++n){
   room.update(n%12==0?z:InputFrame{});
   endingWithSimulationDisabled|=state.flag(213)&&!state.flag(18);
   for(const auto& event:room.takePresentation()){
    if(event.kind!=PresentationEvent::Kind::Cue){continue;}
    const auto& cue=c.cue(event.id);const std::string kind=cue.at("kind");
    if(kind=="show_picture"&&cue.at("picture_id")==10){picture10Shown=true;}
    if(kind=="erase_picture"&&cue.at("picture_id")==10){picture10Erased=true;}
    if(event.id=="f4bc-ce145-c1"){clearStart=n;}
    if(event.id=="f4bc-ce145-c2"){clearEnd=n;}
   }
  }
  require(state.flag(274)&&state.flag(213)&&state.flag(timely?338:337)&&!state.flag(timely?337:338),"Suneo outcome commits only its result");
  require(state.count(1)==e.session.inventory[1]+1&&state.count(5)==e.session.inventory[5]+1&&state.count(6)==e.session.inventory[6]+3,"Suneo has original medicine rewards");
  require(endingWithSimulationDisabled,"Rescue enemy ending commits while foreground has disabled S18");
  require(clearStart>=0&&clearEnd-clearStart>=12,"CE145 moves picture49 for two tenths before erasing it");
  require(picture10Shown==!timely&&!picture10Erased,"Original intervention picture10 is not incorrectly erased by CE145 targeting49");
  for(int n=0;n<120;++n){room.update({});}require(state.flag(1)&&state.flag(18)&&!state.flag(13),"Cooperation restores control");
  const auto count=state.count(6);for(int n=0;n<120;++n){room.update(z);}require(state.count(6)==count,"Outcome does not repeat reward");
 }
 // CN1.00 M95 E2/E3/E4 P3 overrides the doors during S318;
 // S274 selects P4 and restores each original transfer without a parallel lock flag.
 for(int event:{2,3,4}){
  const std::string id="m95-e"+std::to_string(event);const auto& transfer=c.transitions.at(id+"-p4");
  auto e=c.entries.at("defeated-claimed");e.map=95;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();
  e.playerTile=event==2?TilePosition{12,11}:TilePosition{event==3?6:7,5};e.direction=event==2?Direction::Down:Direction::Up;e.session.flags[318]=true;
  GameSession sealed(c.items,e.session,c.facts);CombatScene closed(c,sealed,e);InputFrame doorInput;if(event==2){doorInput.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;}else{doorInput.movement=Direction::Up;}closed.update(doorInput);
  for(int n=0;n<40;++n){closed.update({});}
  require(c.activeVariant(c.objects.at(id),sealed)->page==3&&closed.mapId()==95&&closed.foreground()==ForegroundKind::None,"Every crisis door remains closed on its actual high page");
  e.session.flags[274]=true;e.session.flags[337]=true;e.session.flags[213]=true;GameSession cooperation(c.items,e.session,c.facts);CombatScene open(c,cooperation,e);open.update(doorInput);
  for(int n=0;n<240&&open.mapId()==95;++n){open.update({});}
  require(c.activeVariant(c.objects.at(id),cooperation)->page==4&&open.mapId()==transfer.at("destination").at("map"),"Cooperation reopens each original exit through the public scene input");
 }
 std::cout<<"Suneo rescue checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
