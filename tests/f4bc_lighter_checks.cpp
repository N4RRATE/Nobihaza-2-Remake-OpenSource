#include "combat_scene.h"
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* why){if(!ok){throw std::runtime_error(why);}}
CombatEntry fixture(const CombatContent& c,int map,TilePosition tile,Direction direction){auto e=c.entries.at("defeated-claimed");e.map=map;e.playerTile=tile;e.direction=direction;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();return e;}
InputFrame confirm(){InputFrame f;f.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;return f;}
void finish(CombatScene& scene){for(int i=0;i<4000;++i){scene.update(i%12==0?confirm():InputFrame{});if(scene.foreground()==ForegroundKind::None){return;}}throw std::runtime_error("Flow did not finish");}
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}const auto c=loadCombatContent(argv[1]);
 for(auto direction:{Direction::Left,Direction::Right}){
  const auto origin=c.objects.at("m28-e3").tile;const auto start=adjacentTile(origin,direction==Direction::Left?Direction::Right:Direction::Left);
  auto e=fixture(c,28,start,direction);GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);
  InputFrame push;push.movement=direction;push.buttons[static_cast<int>(direction==Direction::Left?InputAction::Left:InputAction::Right)].held=true;room.update(push);finish(room);
  require(s.flag(direction==Direction::Left?97:96),"Wardrobe contact moves in facing direction");
  require(s.flag(1)&&s.flag(18)&&!s.flag(13),"Wardrobe restores control after parallel routes");
  require(findCharacter(room.world(),"m28-e4").motion.tile()==TilePosition{9,3},"Door upper piece returns through page change");
  auto back=fixture(c,28,{9,4},Direction::Up);back.session=s.exportState();GameSession returned(c.items,back.session,c.facts);CombatScene revisit(c,returned,back);
  require(!findCharacter(revisit.world(),"m28-e3").spriteId&&findCharacter(revisit.world(),"m28-e4").motion.tile()==TilePosition{9,3},"Revisit uses side replacement pages and original door location");
 }
 for(const auto posture:{InputAction::Weapon,InputAction::AuxiliaryWeapon}){
  auto raisedEntry=fixture(c,28,{10,4},Direction::Left);raisedEntry.session.mainWeapon=11;raisedEntry.session.auxiliaryWeapon=24;
  GameSession raised(c.items,raisedEntry.session,c.facts);CombatScene blocked(c,raised,raisedEntry);InputFrame raise;raise.buttons[static_cast<int>(posture)].pressed=true;blocked.update(raise);for(int n=0;n<60;++n){blocked.update({});}
  require(blocked.raised(),"Fixture raises real weapon posture");InputFrame push;push.movement=Direction::Left;blocked.update(push);require(!raised.flag(96)&&!raised.flag(97)&&blocked.foreground()==ForegroundKind::None,"Raised main or auxiliary rejects wardrobe");
 }
 auto wrongWardrobe=fixture(c,28,{9,5},Direction::Up);GameSession wrongWardrobeSession(c.items,wrongWardrobe.session,c.facts);CombatScene wrongWardrobeRoom(c,wrongWardrobeSession,wrongWardrobe);InputFrame upward;upward.movement=Direction::Up;wrongWardrobeRoom.update(upward);require(wrongWardrobeRoom.foreground()==ForegroundKind::None&&!wrongWardrobeSession.flag(96)&&!wrongWardrobeSession.flag(97),"Vertical wardrobe push is refused");
 auto e=fixture(c,44,{7,8},Direction::Up);e.session.flags[137]=e.session.flags[138]=e.session.flags[139]=true;GameSession s(c.items,e.session,c.facts);CombatScene pickup(c,s,e);pickup.update(confirm());finish(pickup);
 require(s.count(68)==1&&s.flag(145),"Lighter grants once from source facing");pickup.update(confirm());finish(pickup);require(s.count(68)==1,"Repeat pickup does not duplicate");
 auto ignition=fixture(c,49,{10,6},Direction::Up);ignition.session=s.exportState();GameSession carried(c.items,ignition.session,c.facts);CombatScene fire(c,carried,ignition);fire.update({});
 require(carried.count(69)==1&&carried.count(68)==0,"Lighter activates at source tile");auto request=fire.requestToolUse();require(request&&fire.useTool(*request),"Menu request starts ignition");finish(fire);
 require(carried.flag(141)&&carried.count(68)==1&&carried.count(69)==0,"Ignition retains total lighter and commits flame");require(!fire.useTool(*request),"Completed request cannot replay");
 auto wrong=fixture(c,44,{6,7},Direction::Right);GameSession wrongSession(c.items,wrong.session,c.facts);CombatScene wrongRoom(c,wrongSession,wrong);wrongRoom.update(confirm());finish(wrongRoom);require(wrongSession.count(68)==0,"Wrong facing cannot collect lighter");
 auto moved=c;moved.catalog["context_tools"][1]["use_tile"]={11,6};auto movedEntry=ignition;movedEntry.playerTile={11,6};GameSession movedSession(moved.items,movedEntry.session,moved.facts);CombatScene movedFire(moved,movedSession,movedEntry);movedFire.update({});auto stale=movedFire.requestToolUse();require(stale.has_value(),"Tool area follows definition variation");movedFire.deactivate();require(!movedFire.useTool(*stale),"Inactive scene cannot ignite");
 for(const auto tile:std::vector<TilePosition>{{10,7},{9,6},{11,6},{8,6}}){
  auto exit=fixture(c,49,tile,Direction::Up);exit.session.inventory[69]=1;exit.session.flags[145]=true;GameSession inv(c.items,exit.session,c.facts);CombatScene area(c,inv,exit);area.update({});const bool border=tile!=TilePosition{8,6};require(inv.count(68)==(border?1:0)&&inv.count(69)==(border?0:1),"Only original return cells normalize lighter");
 }
 for(bool lit:{false,true}){
  auto hint=fixture(c,49,{10,6},Direction::Up);hint.session.flags[141]=lit;GameSession inv(c.items,hint.session,c.facts);CombatScene room(c,inv,hint);room.update(confirm());const std::u32string expected=lit?U"烛灯被点燃了。":U"上面放着一个烛灯。";bool saw=false;
  for(int n=0;n<1000;++n){for(const auto& line:room.dialogue().view().lines){if(line.find(expected)!=std::u32string::npos){saw=true;}}if(saw){break;}room.update({});}require(saw,"Both original candle descriptions open a message");finish(room);
 }
 std::cout<<"Wardrobe and lighter checks passed\n";
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
