#include "game_application.h"
#include "enemy_controller.h"
#include <iostream>
#include <stdexcept>
void require(bool b,const char* m){if(!b){throw std::runtime_error(m);}}
InputFrame press(InputAction a){InputFrame f;f.buttons[static_cast<int>(a)]={true,true,false};return f;}
void ticks(GameApplication& g,int n){while(n--){g.update({});}}
int main(int argc,char** argv){
 if(argc!=2){return 2;}
 try{
  auto content=loadCombatContent(argv[1]);auto& entry=content.entries.at("post-pickup");
  entry.session.flags[52]=false;entry.session.flags[1]=true;entry.session.flags[18]=true;entry.session.flags[47]=true;
  GameSession state(content.items,entry.session,content.facts);state.grant(21,1);
  require(state.equip(21),"Audited DE action is executable after ticket53");
  require(state.magazine(21)==8&&state.consumeRound()&&state.magazine(21)==7,"DE owns an eight-round magazine");
  state.grant(21,1);state.equip(0);require(state.equip(21)&&state.magazine(21)==7,"Repeat grants and equipment cycling never refill DE");
  entry.session.mainWeapon=21;entry.positions["m4-e4"]={15,10};
  GameApplication game(content,"post-pickup");game.update(press(InputAction::Weapon));require(!game.raised(),"Raise request is not a completed posture before its waits");ticks(game,100);require(game.raised(),"DE raise completes through normal input");
  game.update(press(InputAction::Attack));require(game.session().magazine(21)==7&&game.session().magazine(20)==6,"DE shot does not spend magnum ammunition");ticks(game,200);
  require(!game.session().flag(47),"An actual missed bullet clears precision when it ends");
  auto interaction=content;auto& inspectEntry=interaction.entries.at("post-pickup");inspectEntry.playerTile={8,6};inspectEntry.direction=Direction::Up;inspectEntry.positions["m4-e4"]={8,5};
  GameSession inspectState(interaction.items,inspectEntry.session,interaction.facts);CombatScene inspectScene(interaction,inspectState,"post-pickup");
  auto simultaneous=press(InputAction::Weapon);simultaneous.buttons[static_cast<int>(InputAction::Confirm)]={true,true,false};inspectScene.update(simultaneous);
  require(inspectScene.foreground()==ForegroundKind::None,"Player forced raise route blocks simultaneous investigation");
  auto timed=content;timed.actionProfiles["magnum-raise"]["cue_order"]={"f3-ce73-c5"};
  GameApplication waitOne(timed,"post-pickup");waitOne.update(press(InputAction::Weapon));ticks(waitOne,5);require(!waitOne.raised(),"Wait1 has not completed at five logic updates");waitOne.update({});require(waitOne.raised(),"Wait1 completes at six logic updates, without an extra implementation frame");
  timed.actionProfiles["magnum-raise"]["cue_order"]={"f3-ce73-c8"};GameApplication waitZero(timed,"post-pickup");waitZero.update(press(InputAction::Weapon));require(!waitZero.raised(),"Wait0 yields the starting update");waitZero.update({});require(waitZero.raised(),"Wait0 resumes at the next logic update");
  auto empty=content;empty.entries.at("post-pickup").session.magazines[21]=0;
  GameApplication click(empty,"post-pickup");click.update(press(InputAction::Weapon));ticks(click,100);click.update(press(InputAction::Attack));ticks(click,160);
  require(click.session().magazine(21)==0&&click.session().flag(47),"Empty click creates no bullet-ending precision clear");
  empty.entries.at("post-pickup").session.inventory[43]=1;
  GameApplication reload(empty,"post-pickup");reload.update(press(InputAction::Weapon));ticks(reload,100);reload.update(press(InputAction::Attack));
  require(reload.session().magazine(21)==8&&reload.session().count(42)==22&&reload.session().count(43)==0,"Empty DE with box commits reload once before animation");ticks(reload,200);
  require(reload.session().magazine(21)==8&&reload.session().flag(47),"Automatic reload does not append an unrequested shot");
  auto corridor=content;auto& lane=corridor.entries.at("post-pickup");lane.map=10;lane.playerTile={5,19};lane.direction=Direction::Up;
  lane.pages.clear();lane.positions.clear();lane.through.clear();lane.directions.clear();
  lane.positions["m10-e3"]={5,18};lane.positions["m10-e5"]={5,16};lane.pages["m10-e3"]=8;lane.pages["m10-e5"]=8;
  GameSession laneSession(corridor.items,lane.session,corridor.facts);CombatScene laneScene(corridor,laneSession,"post-pickup",[](int lo,int){return lo;});
  laneScene.update(press(InputAction::Weapon));laneSession.setFlag(18,false);for(int n=0;n<100;++n){laneScene.update({});}
  laneSession.setFlag(18,true);laneScene.update(press(InputAction::Attack));laneSession.setFlag(18,false);
  bool firstHit=false,secondHit=false,preservedBetween=false;
  for(int n=0;n<45;++n){laneScene.update({});const auto targets=laneScene.enemyStates();
   firstHit|=targets[0].damage>=150;secondHit|=targets[1].damage>=150;
   preservedBetween|=firstHit&&!secondHit&&laneSession.flag(47);
  }
  require(firstHit&&secondHit&&preservedBetween,"DE bullet propagates from one P5 zombie to a later target and retains precision until actual bullet end");
  require(!laneSession.flag(47)&&laneSession.variable(35)==0,"Bullet end clears precision without inventing CE11 hits for zombie direct kills");
  auto replacement=corridor;replacement.actionProfiles["de-fire"]["cue_order"]={"f4-ce75-c88"};
  auto& replacementEntry=replacement.entries.at("post-pickup");replacementEntry.positions["m10-e3"]={5,12};replacementEntry.positions["m10-e5"]={5,10};
  GameSession replaceState(replacement.items,replacementEntry.session,replacement.facts);CombatScene replaceScene(replacement,replaceState,"post-pickup");
  replaceScene.update(press(InputAction::Weapon));replaceState.setFlag(18,false);for(int n=0;n<100;++n){replaceScene.update({});}
  replaceState.setFlag(18,true);replaceScene.update(press(InputAction::Attack));replaceState.setFlag(18,false);for(int n=0;n<3;++n){replaceScene.update({});}
  require(replaceState.flag(47),"Old flying DE preserves precision before ending");replaceState.setFlag(18,true);replaceScene.update(press(InputAction::Attack));replaceState.setFlag(18,false);
  require(replaceState.magazine(21)==6&&!replaceState.flag(47),"Replacing an active bullet spends one round and executes old ending before new flight");
  replaceScene.deactivate();for(int n=0;n<100;++n){replaceScene.update({});}require(replaceState.magazine(21)==6,"Cancelled delayed new flight never replays the ammo commit");
  auto magnum=corridor;auto& magEntry=magnum.entries.at("post-pickup");magEntry.session.mainWeapon=20;
  magEntry.positions["m10-e3"]={5,12};magEntry.positions["m10-e5"]={5,10};
  magnum.actionProfiles["magnum-fire"]["cue_order"]={"f3-ce73-c3","f4-ce75-c88"};
  GameSession magState(magnum.items,magEntry.session,magnum.facts);CombatScene magScene(magnum,magState,"post-pickup");
  magScene.update(press(InputAction::Weapon));magState.setFlag(18,false);for(int n=0;n<100;++n){magScene.update({});}
  magState.setFlag(18,true);magScene.update(press(InputAction::Attack));magState.setFlag(18,false);for(int n=0;n<4;++n){magScene.update({});}
  const auto oldPose=magScene.world().player().spriteId;
  magnum.actionProfiles["magnum-fire"]["cue_order"]={"f3-ce73-c6","f4-ce75-c88"};
  magState.setFlag(18,true);magScene.update(press(InputAction::Attack));magState.setFlag(18,false);
  require(magState.magazine(20)==4&&magScene.world().player().spriteId==oldPose,"I20 replacement commits ammo but yields before the new recoil pose");
  magScene.update({});require(magScene.world().player().spriteId!=oldPose,"I20 replacement resumes recoil after Wait0");
  auto partial=entry.session;partial.magazines[21]=6;partial.inventory[42]=1;partial.inventory[43]=1;
  GameSession partialSession(content.items,partial,content.facts);CombatScene reloadScene(content,partialSession,entry);
  reloadScene.update(press(InputAction::Weapon));for(int n=0;n<100;++n){reloadScene.update({});}
  auto held=press(InputAction::Cancel);held.buttons[static_cast<int>(InputAction::Cancel)].pressed=false;
  reloadScene.update(press(InputAction::Cancel));for(int n=0;n<9;++n){reloadScene.update(held);}reloadScene.update({});for(int n=0;n<3;++n){reloadScene.update({});}
  require(partialSession.magazine(21)==8&&partialSession.count(42)==29&&partialSession.count(43)==0,"Partial DE reload combines loose round and box using its own capacity");
  reloadScene.deactivate();for(int n=0;n<200;++n){reloadScene.update({});}
  require(partialSession.magazine(21)==8&&partialSession.count(42)==29,"Cancelling reload presentation cannot refund submitted ammunition");
  std::cout<<"DE checks passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
