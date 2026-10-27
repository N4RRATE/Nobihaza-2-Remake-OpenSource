#include "combat_scene.h"
#include "save_snapshot.h"
#include <iostream>
#include <stdexcept>
void require(bool b,const char* m){if(!b){throw std::runtime_error(m);}}
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}const auto c=loadCombatContent(argv[1]);
 auto e=c.entries.at("defeated-claimed");e.map=37;e.playerTile={10,5};e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.inventory[91]=1;e.session.flags[316]=e.session.flags[318]=e.session.flags[274]=e.session.flags[213]=e.session.flags[338]=true;
 GameSession state(c.items,e.session,c.facts);CombatScene room(c,state,e);room.update({});require(state.count(91)==0&&state.count(92)==1,"Blue gemstone activates only at original statue position");auto request=room.requestToolUse();require(request.has_value(),"Blue eye tool can be requested");require(room.useTool(*request),"Blue eye uses shared tool request");require(!room.useTool(*request),"Repeated request cannot consume twice");
 for(int n=0;n<120;++n){room.update({});}require(state.flag(314)&&state.count(92)==0,"Blue eye consumes one gemstone and commits permanent flag");require(!room.requestToolUse(),"Completed blue eye cannot grant another use");
 for(TilePosition tile: {TilePosition{10,6},TilePosition{9,5},TilePosition{11,5}}){auto other=e;other.playerTile=tile;other.session.inventory.erase(91);other.session.inventory[92]=1;GameSession back(c.items,other.session,c.facts);CombatScene exit(c,back,other);exit.update({});require(back.count(92)==0&&back.count(91)==1&&!exit.requestToolUse(),"Every original exit normalizes gem without use");}
 auto old=e;GameSession staleState(c.items,old.session,c.facts);CombatScene stale(c,staleState,old);stale.update({});auto oldRequest=stale.requestToolUse();stale.deactivate();require(oldRequest&&!stale.useTool(*oldRequest)&&!staleState.flag(314),"Scene invalidation rejects old gem result");
 require(c.catalog.at("flows").at("tooth-use").at("runtime_enabled"),"Natural tooth trigger opens after both rescue consumers exist");
 // Both orders retain the later key/request fact but skip the second cry and rescue.
 for(bool gianFirst:{false,true}){
  auto later=c.entries.at("defeated-claimed");later.map=gianFirst?96:49;later.playerTile=gianFirst?TilePosition{7,7}:TilePosition{10,5};later.direction=Direction::Up;
  later.pages.clear();later.positions.clear();later.directions.clear();later.through.clear();later.session.flags[274]=later.session.flags[213]=true;
  if(gianFirst){later.session.flags[273]=later.session.flags[144]=later.session.flags[316]=true;later.session.inventory[87]=1;}
  else{later.session.flags[338]=later.session.flags[318]=later.session.flags[316]=true;later.session.inventory[91]=1;for(int flag:{145,141,142,143}){later.session.flags[flag]=true;}}
  GameSession crossed(c.items,later.session,c.facts);CombatScene acquire(c,crossed,later);acquire.update({});acquire.takePresentation();
  InputFrame z;z.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;
  if(gianFirst){auto use=acquire.requestToolUse();require(use&&acquire.useTool(*use),"Completed Gian route can naturally use the later tooth");}else{acquire.update(z);}
  const std::string cry=c.cue(gianFirst?"m96-e30-p1-c21":"m49-e3-p5-c19").at("sound").at("resource");bool heardCry=false;
  const int requestFlag=gianFirst?318:144;
  for(int n=0;n<6000&&!crossed.flag(requestFlag);++n){acquire.update(n%12==0?z:InputFrame{});for(const auto& event:acquire.takePresentation()){heardCry|=event.kind==PresentationEvent::Kind::Sound&&event.sound.resource==cry;}}
  require(crossed.flag(requestFlag)&&crossed.count(gianFirst?91:70)==1&&!heardCry,"Cooperation skips the other cry but still grants its item and request flag");
  auto visit=later;visit.map=gianFirst?95:63;visit.playerTile={3,12};visit.session=crossed.exportState();const auto before=visit.session.inventory;
  CombatScene revisit(c,crossed,visit);for(int n=0;n<180;++n){revisit.update(z);}
  require(!crossed.flag(gianFirst?337:272)&&!crossed.flag(gianFirst?338:273)&&crossed.exportState().inventory==before,"Shared cooperation prevents a second result or rescue reward on the other route");
 }
 {
  auto saved=e;saved.map=16;saved.playerTile={10,21};saved.direction=Direction::Up;saved.session=state.exportState();GameSession inventory(c.items,saved.session,c.facts);CombatScene crow(c,inventory,saved);
  const auto snapshot=captureSave(c,crow,{crow.generation(),"m16-e5","BSAA 1F入口"},{});
  require(prepareRestore(c,decodeSave(c,encodeSave(c,snapshot))).session->flag(314),"Installed blue eye survives the legal crow save boundary");
  for(int item:{91,92}){auto invalid=snapshot;invalid.session.inventory[item]=1;bool rejected=false;try{encodeSave(c,invalid);}catch(const std::exception&){rejected=true;}require(rejected,"Completed eye cannot restore a second inactive or active gemstone");}
 }
 std::cout<<"Blue eye and return checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
