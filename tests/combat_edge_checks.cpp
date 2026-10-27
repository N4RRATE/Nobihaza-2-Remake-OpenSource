#include "game_application.h"
#include "enemy_controller.h"
#include <iostream>
#include <stdexcept>
void require(bool b,const char* why){if(!b){throw std::runtime_error(why);}}
InputFrame press(InputAction a){InputFrame f;f.buttons[static_cast<int>(a)]={true,true,false};return f;}
InputFrame held(InputAction a){InputFrame f;f.buttons[static_cast<int>(a)].held=true;return f;}
InputFrame release(InputAction a){InputFrame f;f.buttons[static_cast<int>(a)].released=true;return f;}
void ticks(GameApplication& s,int n,InputFrame f={}){for(int i=0;i<n;++i){s.update(f);}}
void raise(GameApplication& s){ticks(s,45,held(InputAction::Weapon));s.update(release(InputAction::Weapon));ticks(s,5);require(s.raised(),"initial raise survives release");}
int main(int argc,char** argv){
 if(argc!=2){return 2;}auto data=loadCombatContent(argv[1]);auto& entry=data.entries.at("post-pickup");
 entry.session.flags[52]=false;entry.session.mainWeapon=11;entry.session.inventory.erase(11);
 auto live=data;live.entries.at("post-pickup").session.flags[52]=true;GameApplication menuPause(live,"post-pickup");menuPause.update(press(InputAction::Cancel));const auto enemyBefore=findCharacter(menuPause.world(),"m4-e4").motion.position();ticks(menuPause,600);const auto enemyAfter=findCharacter(menuPause.world(),"m4-e4").motion.position();require(menuPause.session().hp()==100&&enemyBefore.x==enemyAfter.x&&enemyBefore.y==enemyAfter.y,"equipment freezes map simulation");
 GameApplication combined(data,"post-pickup");InputFrame combo=held(InputAction::AuxiliaryWeapon);combo.buttons[static_cast<int>(InputAction::Attack)].held=true;ticks(combined,40,combo);require(!combined.raised()&&combined.session().magazine(11)==15,"auxiliary request never falls back to main weapon");
 GameApplication aim(data,"post-pickup");raise(aim);ticks(aim,120,held(InputAction::Weapon));require(aim.aiming(),"sustained hold grants aim");
 InputFrame turn=held(InputAction::Weapon);turn.movement=Direction::Up;ticks(aim,3,turn);require(!aim.aiming(),"turning removes aim");
 ticks(aim,120,held(InputAction::Weapon));require(aim.aiming(),"can re-aim after turning");aim.update(release(InputAction::Weapon));require(!aim.aiming(),"release clears aim");
 GameApplication empty(data,"post-pickup");raise(empty);const auto rounds=empty.session().magazine(11);
 empty.update(press(InputAction::Cancel));for(int i=0;i<9;++i){empty.update(held(InputAction::Cancel));}empty.update(release(InputAction::Cancel));ticks(empty,3);require(empty.session().magazine(11)==rounds,"full-magazine reload no operation");
 auto ammo=data;ammo.entries.at("post-pickup").session.magazines[11]=0;ammo.entries.at("post-pickup").session.inventory[32]=1;
 GameApplication autoReload(ammo,"post-pickup");raise(autoReload);autoReload.update(press(InputAction::Attack));require(autoReload.session().magazine(11)==15&&autoReload.session().count(31)==15,"empty press commits reload only");ticks(autoReload,100);require(autoReload.session().magazine(11)==15,"reload completion does not shoot");
 auto hurt=ammo;auto& h=hurt.entries.at("post-pickup");h.session.flags[52]=true;h.positions["m4-e4"]={10,7};h.directions["m4-e4"]=Direction::Up;
 GameApplication interrupted(hurt,"post-pickup",[](int lo,int hi){return hi==2?2:lo;});ticks(interrupted,10,held(InputAction::Weapon));interrupted.update(release(InputAction::Weapon));interrupted.update(press(InputAction::Attack));
 ticks(interrupted,220);require(interrupted.session().magazine(11)==15,"grab cannot refund or spend committed reload");require(interrupted.session().hp()<100,"grab causes actual damage");
 // Actual grab -> release -> recovery must relinquish the forced player sprite.
 GameApplication recovery(hurt,"post-pickup",[](int lo,int hi){return hi==2?2:lo;});
 bool enteredRecovery=false;
 for(int i=0;i<500;++i){recovery.update({});if(recovery.session().conditions().recoveryTicks>0){enteredRecovery=true;break;}}
 require(enteredRecovery,"actual grab reaches push-away recovery");
 const auto pushOrigin=findCharacter(recovery.world(),"m4-e4").motion.position();
 ticks(recovery,6);
 const auto pushed=findCharacter(recovery.world(),"m4-e4");
 require(pushed.motion.position().x!=pushOrigin.x||pushed.motion.position().y!=pushOrigin.y,"push-away must retreat with player recovery, without an extra stationary wait");
 require(pushed.spriteId=="soldier-0"&&pushed.layer==2,"retreat uses P15 sprite and layer, not ordinary bite recovery");
 while(recovery.session().conditions().recoveryTicks>0){recovery.update({});}
 ticks(recovery,2);
 require(recovery.world().player().spriteId==recovery.definition().player.spriteId,"push-away sprite must end with recovery");
 // P13/P14 C83 still belongs to the held pose; four approach directions
 // exercise both grab layers and all retreat routes through scene updates.
 for(auto facing:{Direction::Up,Direction::Right,Direction::Down,Direction::Left}){
  auto directional=live;auto& start=directional.entries.at("post-pickup");
  start.playerTile={10,8};start.positions["m4-e4"]=adjacentTile(start.playerTile,static_cast<Direction>((static_cast<int>(facing)+2)%4));start.directions["m4-e4"]=facing;
  GameApplication s(directional,"post-pickup",[](int lo,int hi){return hi==2?2:lo;});
  for(int i=0;i<500&&s.session().hp()>40;++i){s.update({});}
  require(s.session().hp()==40,"six actual grab bites precede natural release");
  ticks(s,45);
  require(s.session().conditions().recoveryTicks==0&&s.world().player().spriteId=="grab-7","C83 wait keeps grab pose, not push pose");
  for(int i=0;i<20&&s.session().conditions().recoveryTicks==0;++i){s.update({});}
  require(s.session().conditions().recoveryTicks>0,"natural release follows C83 wait");
  ticks(s,6);auto e=findCharacter(s.world(),"m4-e4");
  require(e.motion.position().x!=10||e.motion.position().y!=8,"all four retreat directions separate the actors");
  require(e.motion.facing()==facing,"retreat preserves facing");
 }
 // Data-owned presentation may change independently of actor identity/room.
 auto alternate=live;
 for(auto& v:alternate.objects.at("m4-e4").variants){if(v.page==15){v.definition.spriteId="soldier-1";v.definition.moveSpeed=4;}}
 GameApplication alternatePose(alternate,"post-pickup",[](int lo,int hi){return hi==2?2:lo;});
 for(int i=0;i<600&&alternatePose.session().conditions().recoveryTicks==0;++i){alternatePose.update({});}
 ticks(alternatePose,6);
 require(findCharacter(alternatePose.world(),"m4-e4").spriteId=="soldier-1"&&findCharacter(alternatePose.world(),"m4-e4").motion.speed()==32,"retreat reads content page presentation and speed");
 auto wall=live;auto& wallEntry=wall.entries.at("post-pickup");wallEntry.playerTile={10,8};wallEntry.positions["m4-e4"]={10,9};wallEntry.directions["m4-e4"]=Direction::Up;
 wall.maps.at(4).passageMasks[10*wall.maps.at(4).width+10]=0;
 GameApplication blockedRetreat(wall,"post-pickup",[](int lo,int hi){return hi==2?2:lo;});
 for(int i=0;i<600&&blockedRetreat.session().conditions().recoveryTicks==0;++i){blockedRetreat.update({});}
 require(blockedRetreat.session().conditions().recoveryTicks>0,"wall fixture reaches actual release");
 ticks(blockedRetreat,60);
 require(findCharacter(blockedRetreat.world(),"m4-e4").motion.tile()==TilePosition{10,9},"retreat may be short at a wall; never teleports through terrain");
 // Receiver boundary fixture: a separate caller delivers a hit while held.
 // This is not claimed as a naturally reachable player shot during a grab.
 for(bool lethal:{false,true}){
  auto receiverData=hurt;receiverData.entries.at("post-pickup").playerTile={10,8};receiverData.entries.at("post-pickup").positions["m4-e4"]={10,9};if(lethal){receiverData.entries.at("post-pickup").session.baseAttack=1000;}
  GameSession receiverSession(receiverData.items,receiverData.entries.at("post-pickup").session);
  CombatScene receiverScene(receiverData,receiverSession,"post-pickup",[](int lo,int hi){return hi==2?2:lo;});
  for(int i=0;i<300&&!receiverScene.enemyStates().at(0).holdsPlayer;++i){receiverScene.update({});}
  require(receiverScene.enemyStates().at(0).holdsPlayer,"receiver fixture reaches grab via public update");
  receiverScene.takePresentation();
  const auto position=findCharacter(receiverScene.world(),"m4-e4").motion.position();
  std::set<std::string> hitTargets;
  require(receiverScene.resolveShot({(position.x+.5)*16,(position.y+1)*16-8},lethal,0,true,hitTargets),"hit reaches grabbed enemy receiver");
  require(!receiverScene.enemyStates().at(0).holdsPlayer&&!receiverScene.session().flag(10)&&receiverScene.session().conditions().recoveryTicks==0,"interrupt exits grab without natural push-away recovery");
  for(const auto& event:receiverScene.takePresentation()){require(event.id!="m4-e4-p15-c2","interrupt must not flash the retreat page");}
  if(!lethal){require(findCharacter(receiverScene.world(),"m4-e4").layer==1&&findCharacter(receiverScene.world(),"m4-e4").motion.speed()==8,"nonlethal interruption restores ordinary enemy presentation");}
  else{receiverScene.update({});require(receiverScene.foreground()==ForegroundKind::EnemyEnd,"lethal grab interruption hands off to ending");}
 }
 // Synthetic overlap: a new foreground route outlives an existing recovery timer.
 auto overlap=hurt;auto& overlapping=overlap.entries.at("post-pickup");
 overlapping.session.hp=1;overlapping.session.conditions.recoveryTicks=120;
 GameApplication takeover(overlap,"post-pickup",[](int lo,int){return lo;});
 for(int i=0;i<100&&takeover.foreground()!=ForegroundKind::Death;++i){takeover.update({});}
 require(takeover.foreground()==ForegroundKind::Death,"death takes ownership during recovery fixture");
 while(takeover.session().conditions().recoveryTicks>0){takeover.update({});}
 ticks(takeover,2);
 require(takeover.world().player().spriteId=="player-6","recovery must not overwrite a newer death presentation");
 auto varied=data;auto node=varied.objects.extract("m4-e4");node.key()="enemy-variant";node.mapped().id="enemy-variant";for(auto& v:node.mapped().variants){v.definition.id="enemy-variant";}varied.objects.insert(std::move(node));varied.rules["enemy"]["id"]="enemy-variant";auto enemyNode=varied.enemies.extract("m4-e4");enemyNode.key()="enemy-variant";enemyNode.mapped().id="enemy-variant";varied.enemies.insert(std::move(enemyNode));
 for(auto& [name,e]:varied.entries){for(auto* values:{&e.pages}){auto n=values->extract("m4-e4");if(n){n.key()="enemy-variant";values->insert(std::move(n));}}auto p=e.positions.extract("m4-e4");if(p){p.key()="enemy-variant";e.positions.insert(std::move(p));}auto t=e.through.extract("m4-e4");if(t){t.key()="enemy-variant";e.through.insert(std::move(t));}auto d=e.directions.extract("m4-e4");if(d){d.key()="enemy-variant";e.directions.insert(std::move(d));}}
 GameApplication variant(varied,"post-pickup");ticks(variant,10);require(findCharacter(variant.world(),"enemy-variant").id=="enemy-variant","enemy identity is content-owned");
 std::cout<<"combat edge checks passed\n";
}
