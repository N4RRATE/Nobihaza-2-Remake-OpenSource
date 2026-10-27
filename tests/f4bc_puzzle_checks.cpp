#include "combat_scene.h"
#include "save_snapshot.h"
#include "enemy_host.h"
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* why){if(!ok){throw std::runtime_error(why);}}
CombatEntry entry(const CombatContent& c,int map,TilePosition p,Direction d){auto e=c.entries.at("defeated-claimed");e.map=map;e.playerTile=p;e.direction=d;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();return e;}
void settle(CombatScene& room){for(int n=0;n<3000;++n){room.update({});if(room.foreground()==ForegroundKind::None&&!room.world().player().motion.moving()){return;}}throw std::runtime_error("Puzzle flow stuck");}
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);
 auto e=entry(c,63,{5,6},Direction::Right);GameSession state(c.items,e.session,c.facts);CombatScene room(c,state,e);InputFrame right;right.movement=Direction::Right;room.update(right);settle(room);
 require(findCharacter(room.world(),"m63-e2").motion.tile()==TilePosition{7,6},"Contact pushes stool and player together");require(room.world().player().motion.tile()==TilePosition{6,6},"Stool moves actual collision state");
 auto solved=entry(c,49,{10,14},Direction::Up);solved.session.flags[141]=true;
 for(const auto& [id,xy]:c.branch.at("episodes").at("elements").at("solution").items()){solved.positions[id]={xy[0],xy[1]};}
 GameSession solvedState(c.items,solved.session,c.facts);CombatScene puzzle(c,solvedState,solved);for(int n=0;n<1000;++n){puzzle.update({});}
 require(solvedState.flag(142)&&solvedState.flag(143),"Four real positions resolve puzzle and reveal pedestal");
 auto climb=entry(c,63,{5,6},Direction::Right);GameSession climbed(c.items,climb.session,c.facts);CombatScene stairs(c,climbed,climb);InputFrame z;z.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;
 stairs.update(z);settle(stairs);require(climbed.flag(69)&&stairs.world().player().motion.tile()==TilePosition{6,5},"Decision trigger climbs diagonally, no Shift needed");
 InputFrame left;left.movement=Direction::Left;stairs.update(left);settle(stairs);stairs.update(z);settle(stairs);
 require(!climbed.flag(69)&&stairs.world().player().motion.tile()==TilePosition{5,6},"Facing left and Z descends to stool left side");
 require(stairs.world().player().motion.speed()==32&&!stairs.world().player().through,"Descent restores source speed and collision");
 auto wrong=entry(c,63,{6,7},Direction::Up);GameSession notClimbed(c.items,wrong.session,c.facts);CombatScene wrongRoom(c,notClimbed,wrong);wrongRoom.update(z);settle(wrongRoom);require(!notClimbed.flag(69)&&findCharacter(wrongRoom.world(),"m63-e2").motion.tile()==TilePosition{6,6},"Wrong-side decision does not fall through to pushing");
 auto revisit=entry(c,49,{10,14},Direction::Up);revisit.session=solvedState.exportState();GameSession restored(c.items,revisit.session,c.facts);CombatScene rebuilt(c,restored,revisit);rebuilt.update({});for(const auto& [id,xy]:c.branch.at("episodes").at("elements").at("solution").items()){require(findCharacter(rebuilt.world(),id).motion.tile()==TilePosition{xy[0],xy[1]},"Solved room rebuilds the actual solution");}
 auto high=entry(c,63,{2,8},Direction::Right);high.positions["m63-e2"]={3,8};GameSession highState(c.items,high.session,c.facts);CombatScene highRoom(c,highState,high);highRoom.update(z);settle(highRoom);
 require(highRoom.world().player().motion.tile()==TilePosition{3,7},"Moved stool provides exact switch elevation");InputFrame faceRight;faceRight.movement=Direction::Right;highRoom.update(faceRight);highRoom.update(z);
 for(int n=0;n<2000&&!highState.flag(197);++n){highRoom.update(n%12==0?z:InputFrame{});}require(highState.flag(197),"Actual question choice opens upper switch");settle(highRoom);
 auto unsolved=entry(c,49,{10,14},Direction::Up);unsolved.session.flags[141]=true;GameSession unsolvedState(c.items,unsolved.session,c.facts);CombatScene reset(c,unsolvedState,unsolved);for(int n=0;n<20;++n){reset.update({});}require(!unsolvedState.flag(142)&&!unsolvedState.flag(143),"Fresh wrong layout never solves");require(findCharacter(reset.world(),"m49-e2").motion.tile()==TilePosition{7,7},"Unsolved return rebuilds initial positions");
 auto key=entry(c,49,{10,5},Direction::Up);key.session=solvedState.exportState();const auto& enabled=c;
 for(bool cooperation:{false,true}){
  key.session.flags[274]=cooperation;GameSession inventory(enabled.items,key.session,enabled.facts);CombatScene take(enabled,inventory,key);take.update(z);for(int n=0;n<3000&&!inventory.flag(144);++n){take.update(n%12==0?z:InputFrame{});}require(inventory.flag(144)&&inventory.count(70)==1,"Full explicit fixture awards key and commits request for either cooperation state");take.update(z);for(int n=0;n<60;++n){take.update(z);}require(inventory.count(70)==1,"Reinvestigation does not duplicate key");
 }
 // The runtime conditions use content identities and relative positions, not a built-in stool ID.
 auto changed=c;auto object=changed.objects.extract("m63-e2");object.key()="shifted-stool";object.mapped().id="shifted-stool";for(auto& page:object.mapped().variants){page.definition.id="shifted-stool";}changed.objects.insert(std::move(object));
 for(auto& [id,flow]:changed.catalog["flows"].items()){
  if(flow.at("object")=="m63-e2"){flow["object"]="shifted-stool";}
  for(const char* condition:{"destination","player_relative"}){if(flow.contains(condition)&&flow[condition]["object"]=="m63-e2"){flow[condition]["object"]="shifted-stool";}}
 }
 auto varied=entry(changed,63,{3,6},Direction::Right);varied.positions["shifted-stool"]={4,6};GameSession variedState(changed.items,varied.session,changed.facts);CombatScene variedRoom(changed,variedState,varied);variedRoom.update(z);settle(variedRoom);require(variedState.flag(69)&&variedRoom.world().player().motion.tile()==TilePosition{4,5},"Changed stable ID and position preserve climb");
 for(int auxiliary:{27,29}){
  auto armed=entry(c,63,{5,6},Direction::Right);armed.session.auxiliaryWeapon=auxiliary;GameSession armedState(c.items,armed.session,c.facts);CombatScene armedRoom(c,armedState,armed);InputFrame raise;raise.buttons[static_cast<int>(InputAction::AuxiliaryWeapon)].pressed=true;armedRoom.update(raise);for(int n=0;n<100;++n){armedRoom.update({});}require(armedRoom.raised(),"Fixture auxiliary raised");armedRoom.update(z);armedRoom.update(right);settle(armedRoom);require(!armedState.flag(69)&&findCharacter(armedRoom.world(),"m63-e2").motion.tile()==TilePosition{6,6},"S2 includes LC9/grenade and refuses stool");
 }
 for(bool blocked:{false,true}){
  auto variant=c;auto start=entry(variant,49,{7,8},Direction::Up);start.session.flags[141]=true;if(blocked){variant.maps.at(49).passageMasks.at(6*variant.maps.at(49).width+7)=0;}
  GameSession pushing(variant.items,start.session,variant.facts);CombatScene pushRoom(variant,pushing,start);pushRoom.takePresentation();InputFrame up;up.movement=Direction::Up;pushRoom.update(up);settle(pushRoom);
  const auto expected=blocked?TilePosition{7,7}:TilePosition{7,6};require(findCharacter(pushRoom.world(),"m49-e2").motion.tile()==expected,"Statue collision keeps or moves real actor");bool sound=false;const auto audio=variant.cue("m49-e2-p2-c36").at("sound").at("resource").get<std::string>();for(const auto& event:pushRoom.takePresentation()){sound|=event.kind==PresentationEvent::Kind::Sound&&event.sound.resource==audio;}
  require(sound!=blocked,"Stone sound is conditional on player displacement");
 }
 for(bool eventsContinue:{false,true}){
  auto waiting=entry(c,28,{5,5},Direction::Right);GameSession state(c.items,waiting.session,c.facts);CombatScene waitingRoom(c,state,waiting);EnemyHost actor(waitingRoom,"player");RouteDefinition route;route.frequency=8;route.steps={{"right"}};actor.route(route);
  ChoicePrompt choice;choice.options={MessageLine{"确认",{{MessageTokenKind::Text,U"确认",0}}}};choice.display.continueEvents=eventsContinue;auto request=waitingRoom.requestInput(choice);InputFrame leftInput;leftInput.movement=Direction::Left;for(int n=0;n<90;++n){waitingRoom.update(leftInput);}
  require(waitingRoom.world().player().motion.tile()==TilePosition{6,5},"Choice keeps forced route advancing while player movement stays locked");require(waitingRoom.cancelInput(request),"Waiting choice remains owned by scene");
 }
 auto saveEntry=entry(c,16,{10,21},Direction::Up);GameSession saving(c.items,saveEntry.session,c.facts);CombatScene saveRoom(c,saving,saveEntry);auto snapshot=captureSave(c,saveRoom,{saveRoom.generation(),"m16-e5","BSAA 1F入口"},{});
 for(int flag:{142,143,144,147}){
  auto bad=snapshot;bad.session.worldFlags[c.facts.switches.at(flag)]=true;bool rejected=false;try{encodeSave(c,bad);}catch(const std::exception&){rejected=true;}require(rejected,"Contradictory puzzle or key fact cannot cross save boundary");
 }
 snapshot.session.inventory[68]=1;snapshot.session.inventory[70]=1;for(int flag:{145,141,142,143,144,197}){snapshot.session.worldFlags[c.facts.switches.at(flag)]=true;}
 require(prepareRestore(c,decodeSave(c,encodeSave(c,snapshot))).session->count(70)==1,"Complete key and puzzle facts restore together");
 snapshot.session.worldFlags[c.facts.switches.at(147)]=true;snapshot.session.inventory.erase(70);require(prepareRestore(c,decodeSave(c,encodeSave(c,snapshot))).session->flag(147),"Later consumer may discard unlocked key");
 std::cout<<"Stool and elements puzzle checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
