#include "game_application.h"
#include <iostream>
#include <stdexcept>
void require(bool b,const char* why){if(!b){throw std::runtime_error(why);}}
InputFrame key(InputAction a){InputFrame f;f.buttons[static_cast<int>(a)]={true,true,false};return f;}
void settle(GameApplication& scene,int ticks){for(int i=0;i<ticks;++i){scene.update({});}}
void walk(GameApplication& s,Direction d,int tiles=1){for(int j=0;j<tiles;++j){InputFrame f;f.movement=d;for(int i=0;i<8;++i){s.update(f);}}}
void messages(GameApplication& s,int limit=2200){for(int i=0;i<limit&&s.foreground()!=ForegroundKind::None;++i){s.update(i%30==29?key(InputAction::Confirm):InputFrame{});}}
int main(int argc,char** argv){
 if(argc!=2){return 2;}auto data=loadCombatContent(argv[1]);auto rng=[](int lo,int){return lo;};
 GameApplication s(data,"natural",rng);walk(s,Direction::Down);walk(s,Direction::Left);s.update(key(InputAction::Confirm));messages(s);
 require(s.session().flag(52),"natural pickup completes");s.update(key(InputAction::Cancel));s.update(key(InputAction::Down));s.update(key(InputAction::Confirm));s.update(key(InputAction::Cancel));
 require(s.session().equipment().mainWeapon==11,"natural equipment");
 for(int i=0;i<10;++i){s.update(key(InputAction::Weapon));}InputFrame release;release.buttons[static_cast<int>(InputAction::Weapon)].released=true;s.update(release);settle(s,10);
 for(int i=0;i<2200&&!s.session().flag(101)&&s.session().hp()>0;++i){
  InputFrame f;if(s.foreground()==ForegroundKind::None){f.buttons[static_cast<int>(InputAction::Attack)].held=true;f.buttons[static_cast<int>(InputAction::Attack)].pressed=i%30==0;}else if(i%30==0){f=key(InputAction::Confirm);}s.update(f);
 }
 require(s.session().flag(101),"natural battle reaches stable corpse");require(s.session().variable(398)>=1,"corpse type saved");require(!s.session().flag(54),"kill does not claim supply");require(!s.session().flag(55),"first kill must not claim Map9 initial visit");
 std::cout<<"natural chain HP="<<s.session().hp()<<" rounds="<<s.session().magazine(11)<<" corpse="<<s.session().variable(396)<<","<<s.session().variable(397)<<"\n";
 GameApplication supply(data,"defeated-unclaimed",rng);
 auto target=TilePosition{supply.session().variable(396),supply.session().variable(397)};
 for(int guard=0;guard<20&&supply.world().player().motion.tile()!=target;++guard){auto p=supply.world().player().motion.tile();walk(supply,p.x!=target.x?(p.x<target.x?Direction::Right:Direction::Left):(p.y<target.y?Direction::Down:Direction::Up));}
 supply.update(key(InputAction::Confirm));messages(supply);require(supply.session().flag(54)&&supply.session().count(24)==1&&supply.session().count(31)==30,"supply awards once");supply.update(key(InputAction::Confirm));messages(supply);require(supply.session().count(31)==30,"no repeat supply");
 auto low=data;low.entries.at("post-pickup").session.hp=1;GameApplication death(low,"post-pickup",rng);
 bool observedDead=false;for(int i=0;i<6000&&death.mode()!=ApplicationMode::Title;++i){death.update({});if(death.hasGame()&&death.session().hp()==0){observedDead=true;}}
 require(death.mode()==ApplicationMode::Title&&!death.hasGame()&&observedDead,"lethal enemy leads to title");
 GameApplication travel(data,"natural",rng);walk(travel,Direction::Down);walk(travel,Direction::Left,2);walk(travel,Direction::Up,2);walk(travel,Direction::Right);settle(travel,100);
 require(travel.definition().width==20,"natural soldier blocks the only exit approach");
 GameApplication escape(data,"post-pickup",rng);walk(escape,Direction::Up);walk(escape,Direction::Left,3);walk(escape,Direction::Up);settle(escape,100);
 require(escape.definition().width==25,"escape while enemy alive loads corridor");
 require(!escape.session().flag(101)&&escape.session().flag(52),"escape does not kill or forget pickup");
 InputFrame up;up.movement=Direction::Down;escape.update(up);settle(escape,8);escape.update(key(InputAction::Confirm));settle(escape,100);
 require(escape.definition().width==20,"actual return to prison");
 auto reward=data;auto& rewardEntry=reward.entries.at("defeated-unclaimed");rewardEntry.session.variables[396]=8;rewardEntry.session.variables[397]=8;
 reward.interactions["supply"]["award"]["24"]=2;reward.interactions["supply"]["award"]["31"]=7;
 GameApplication movedSupply(reward,"defeated-unclaimed",rng);settle(movedSupply,1);
 bool visible=false,hidden=false,visibleAgain=false;
 for(int n=0;n<200;++n){movedSupply.update({});const auto id=findCharacter(movedSupply.world(),"m4-e12").spriteId;
  if(id=="supply-5"){if(hidden){visibleAgain=true;}visible=true;}if(visible&&id=="invisible-alias-0"){hidden=true;}}
 require(visible&&hidden&&visibleAgain,"supply completes visible-hidden-visible cycle");
 walk(movedSupply,Direction::Left,2);walk(movedSupply,Direction::Down,2);movedSupply.update(key(InputAction::Confirm));const auto sparkle=findCharacter(movedSupply.world(),"m4-e12");settle(movedSupply,40);require(findCharacter(movedSupply.world(),"m4-e12").spriteId==sparkle.spriteId&&findCharacter(movedSupply.world(),"m4-e12").motion.facing()==sparkle.motion.facing(),"message pauses autonomous route");messages(movedSupply);
 require(movedSupply.session().count(24)==2&&movedSupply.session().count(31)==7,"moved corpse and changed reward quantities");
 auto persistent=data;auto& saved=persistent.entries.at("defeated-claimed");saved.session.variables[35]=7;saved.session.variables[41]=3;saved.session.magazines[11]=13;saved.session.conditions={1000,1000,0,0};saved.session.flags[13]=true;
 GameApplication returnTrip(persistent,"defeated-claimed",rng);walk(returnTrip,Direction::Up);walk(returnTrip,Direction::Left,3);walk(returnTrip,Direction::Up);settle(returnTrip,100);
 require(returnTrip.definition().width==25,"dead-enemy exit works");InputFrame down;down.movement=Direction::Down;returnTrip.update(down);settle(returnTrip,8);returnTrip.update(key(InputAction::Confirm));settle(returnTrip,100);
 require(returnTrip.definition().width==20&&returnTrip.world().player().motion.tile()==TilePosition{7,5},"round trip reaches actual prison landing");
 require(returnTrip.session().flag(101)&&returnTrip.session().flag(54)&&returnTrip.session().magazine(11)==13,"corpse claim and rounds survive scene reconstruction");
 require(returnTrip.session().conditions().bleedingTicks>0&&returnTrip.session().conditions().bleedingTicks<1000&&returnTrip.session().conditions().invulnerableTicks>0,"cross-map conditions keep remaining progress");
 require(returnTrip.session().variable(35)==7&&returnTrip.session().variable(41)==3,"designated statistics survive room reset");
 auto corridor=data;auto& corridorEntry=corridor.entries.at("post-pickup");corridorEntry.map=6;corridorEntry.playerTile={20,4};corridorEntry.direction=Direction::Up;corridorEntry.pages.clear();corridorEntry.positions.clear();corridorEntry.through.clear();corridorEntry.directions.clear();
 GameApplication boundary(corridor,"post-pickup",rng);walk(boundary,Direction::Up);settle(boundary,3);require(boundary.mode()==ApplicationMode::Boundary&&boundary.exitRequest()->map==9,"Map9 returns a real handoff request without fake map");boundary.update(key(InputAction::Cancel));require(boundary.mode()==ApplicationMode::Playing&&!boundary.exitRequest(),"boundary cancellation returns to corridor");
 corridorEntry.session.flags[52]=false;GameApplication refusal(corridor,"post-pickup",rng);walk(refusal,Direction::Up);settle(refusal,3);messages(refusal);require(refusal.mode()==ApplicationMode::Playing&&refusal.world().player().motion.tile()==TilePosition{20,4},"explicit no-gun corridor fixture refuses and steps back");
 std::cout<<"combat integration checks passed\n";
}
