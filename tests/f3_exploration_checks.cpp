#include "game_application.h"
#include <iostream>
#include <stdexcept>
void require(bool b,const char* m){if(!b){throw std::runtime_error(m);}}
InputFrame confirm(){InputFrame f;f.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;return f;}
void settle(CombatScene& s,int count=2400){for(int n=0;n<count;++n){s.update(n%12==0?confirm():InputFrame{});}}
CombatEntry room(CombatContent& c,int map,TilePosition tile,Direction facing){
 auto e=c.entries.at("defeated-claimed");e.map=map;e.playerTile=tile;e.direction=facing;
 e.pages.clear();e.positions.clear();e.through.clear();e.directions.clear();return e;
}
int main(int argc,char** argv){
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);
 auto repeat=room(c,6,{2,7},Direction::Up);c.entries["repeat"]=repeat;
 GameSession r(c.items,repeat.session);CombatScene repeated(c,r,"repeat");repeated.update(confirm());
 require(r.count(20)==1&&r.count(41)==2&&r.count(5)==5,"same-tile repeat grants all three original items");
 for(int n=0;n<120;++n){repeated.update({});repeated.update(confirm());}require(r.count(5)==99&&r.count(41)==99,"repeat grant obeys inventory cap");
 auto pickup=room(c,14,{4,15},Direction::Up);c.entries["pickup"]=pickup;
 GameSession p(c.items,pickup.session);CombatScene collected(c,p,"pickup");const int before=p.count(31);collected.update(confirm());
 require(p.flag(62)&&p.count(31)==before,"claim hides object before message completes, award not yet committed");
 settle(collected);require(p.count(31)==before+15,"message completion commits one award");settle(collected,200);require(p.count(31)==before+15,"claimed page prevents repeat award");
 for(int side=0;side<2;++side){
  auto intro=room(c,9,{4+side,7},Direction::Up);intro.session.flags[55]=false;c.entries["intro"]=intro;
  GameSession state(c.items,intro.session);CombatScene scene(c,state,"intro");for(int n=0;n<2400&&!state.flag(55);++n){scene.update(n%12==0?confirm():InputFrame{});}
  require(state.flag(55)&&state.flag(1)&&state.flag(18)&&!state.flag(19),"first-visit sequence restores control and commits once");
  require(scene.world().player().motion.tile()==TilePosition{9,4},"right entrance skips blocked first route step; both reach source corpse");
 }
 auto save=room(c,16,{10,21},Direction::Up);c.entries["save"]=save;
 GameSession s(c.items,save.session);CombatScene saving(c,s,"save");saving.update(confirm());
 auto request=saving.takeSaveRequest();require(request&&request->scene==saving.generation()&&s.variable(399)==0,"legal save request carries identity without writing fake save");
 // Actual ladder: source through routes restore passage, helpers occupy the top perimeter.
 for(int facing=0;facing<4;++facing){
  auto model=c;const auto d=static_cast<Direction>(facing);const TilePosition ladder{31,8};
  auto e=room(model,12,adjacentTile(ladder,static_cast<Direction>((facing+2)%4)),d);model.entries["ladder"]=e;
  GameSession state(model.items,e.session);CombatScene climbing(model,state,"ladder");climbing.update(confirm());
  for(int n=0;n<1000&&climbing.foreground()!=ForegroundKind::None;++n){climbing.update({});}
  require(findCharacter(climbing.world(),"m12-e9").graphicFacing()==Direction::Left&&findCharacter(climbing.world(),"m12-e9").animation.column()==1,"climbing page keeps ladder graphic");
  require(state.flag(69)&&!state.flag(19)&&state.flag(1)&&state.flag(18),"climb restores stable-top control without permanent combat lock");
  require(climbing.world().player().motion.tile()==TilePosition{31,7}&&!climbing.world().player().through,"all four actual through routes end one tile above ladder");
  require(!climbing.world().canPlayerMove(Direction::Right),"source helper blocks ordinary walk off ladder");
 }
 // Changed position and ID, clear test terrain: verify all four exit routes and shared source target binding.
 for(int direction=0;direction<4;++direction){
  auto model=c;auto ladder=model.objects.at("m12-e9");model.objects.erase(ladder.id);ladder.id="movable-fixture";ladder.tile={20,15};
  for(auto& v:ladder.variants){v.definition.id=ladder.id;v.definition.tileX=20;v.definition.tileY=15;}
  model.objects.emplace(ladder.id,ladder);model.catalog["ladders"][0]["object"]=ladder.id;
  auto& map=model.maps.at(12);std::fill(map.passageMasks.begin(),map.passageMasks.end(),15);std::fill(model.terrainIds.at(12).begin(),model.terrainIds.at(12).end(),1);
  // Remove unrelated Map12 object collisions only for this isolated variation.
  for(auto& [id,object]:model.objects){if(object.map==12&&id!=ladder.id){for(auto& page:object.variants){page.definition.through=true;}}}
  const auto pushDirection=static_cast<Direction>(direction);
  auto pushEntry=room(model,12,adjacentTile({20,15},static_cast<Direction>((direction+2)%4)),pushDirection);
  model.entries["varied-push"]=pushEntry;GameSession pushState(model.items,pushEntry.session);CombatScene variedPush(model,pushState,"varied-push");
  InputFrame moving;moving.movement=pushDirection;variedPush.update(moving);
  for(int tick=0;tick<300;++tick){
   const auto object=findCharacter(variedPush.world(),ladder.id);
   require(object.graphicFacing()==Direction::Left&&object.animation.column()==1,"four-way push preserves fixed frame with changed ID/location");
   variedPush.update({});
  }
  require(findCharacter(variedPush.world(),ladder.id).motion.tile()==adjacentTile({20,15},pushDirection),"varied push advances in requested direction");
  require(variedPush.world().player().graphicFacing()==pushDirection,"normal player graphic follows pushing direction");
  auto e=room(model,12,{20,16},Direction::Up);model.entries["ladder"]=e;GameSession state(model.items,e.session);CombatScene scene(model,state,"ladder");scene.update(confirm());
  for(int n=0;n<1000&&scene.foreground()!=ForegroundKind::None;++n){scene.update({});}
  InputFrame turn;turn.movement=static_cast<Direction>(direction);scene.update(turn);scene.update(confirm());
  for(int n=0;n<1000&&scene.foreground()!=ForegroundKind::None;++n){scene.update({});}
  const TilePosition expected=direction==0?TilePosition{20,14}:adjacentTile({20,15},static_cast<Direction>(direction));
  require(!state.flag(69)&&state.flag(18)&&state.flag(1)&&scene.world().player().motion.tile()==expected&&!scene.world().player().through,"directional helper restores control and source exit landing after ID/location variation");
 }
 auto push=room(c,12,{30,8},Direction::Right);c.entries["push"]=push;GameSession pushingState(c.items,push.session);CombatScene pushing(c,pushingState,"push");
 const auto ladderBefore=findCharacter(pushing.world(),"m12-e9");
 InputFrame up;up.movement=Direction::Right;pushing.update(up);
 for(int n=0;n<300&&pushing.foreground()!=ForegroundKind::None;++n){
  const auto ladderNow=findCharacter(pushing.world(),"m12-e9");
  require(ladderNow.spriteId==ladderBefore.spriteId&&ladderNow.graphicFacing()==Direction::Left&&ladderNow.animation.column()==1,"pushing ladder must retain original graphic row and column, not turn into herb");
  pushing.update({});
 }
 require(findCharacter(pushing.world(),"m12-e9").motion.tile()==TilePosition{32,8}&&pushing.world().player().motion.tile()==TilePosition{31,8},"actual push advances ladder and player separately");
 GameSession simultaneousState(c.items,push.session);CombatScene simultaneous(c,simultaneousState,"push");InputFrame combined=up;combined.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;
 simultaneous.update(combined);for(int n=0;n<300&&simultaneous.foreground()!=ForegroundKind::None;++n){simultaneous.update({});}
 require(!simultaneousState.flag(69)&&findCharacter(simultaneous.world(),"m12-e9").motion.tile()==TilePosition{32,8}&&simultaneous.world().player().motion.tile()==TilePosition{31,8},"movement-owned push consumes same-frame investigate edge");
 auto blocked=c;blocked.terrainIds.at(12)[8*40+32]=3;GameSession blockedState(blocked.items,push.session);CombatScene blockedPush(blocked,blockedState,"push");blockedPush.update(up);
 require(findCharacter(blockedPush.world(),"m12-e9").motion.tile()==TilePosition{31,8}&&blockedState.flag(1),"terrain3 refuses push and restores control");
 auto interrupted=room(c,9,{4,7},Direction::Up);interrupted.session.flags[55]=false;c.entries["interrupted"]=interrupted;
 GameSession dying(c.items,interrupted.session);CombatScene cancelled(c,dying,"interrupted");for(int n=0;n<500&&cancelled.dialogue().view().phase==DialoguePhase::Hidden;++n){cancelled.update({});}
 dying.loseHp(dying.hp());cancelled.update({});require(cancelled.foreground()==ForegroundKind::Death&&cancelled.dialogue().view().phase==DialoguePhase::Hidden&&cancelled.playerVisible(),"death invalidates old exploration message and visibility owner");
 auto weather=room(c,10,{3,10},Direction::Right);c.entries["weather"]=weather;GameSession storm(c.items,weather.session);CombatScene lightning(c,storm,"weather");
 std::map<std::string,int> at;for(int n=0;n<310;++n){lightning.update({});for(const auto& event:lightning.takePresentation()){if(event.kind==PresentationEvent::Kind::Cue){at.try_emplace(event.id,n);}}}
 require(at.at("m10-e2-p2-c3")==300&&at.at("m10-e2-p2-c5")==301&&at.at("m10-e2-p2-c8")==302&&at.at("m10-e2-p2-c10")==303,"Wait0 yields exactly one logic update in environment flow");
 std::cout<<"F3 exploration checks passed\n";
}
