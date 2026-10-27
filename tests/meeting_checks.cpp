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
int main(int argc,char**argv){try{if(argc!=2)return 2;auto c=loadCombatContent(argv[1]);
 require(!c.pendingMapTickets.contains(24),"Meeting room must be runtime accessible after Map23");
 auto e=c.entries.at("defeated-claimed");e.map=24;e.playerTile={13,5};e.direction=Direction::Down;e.session.flags[73]=false;for(int flag:{66,67,68,71,72})e.session.flags[flag]=true;e.session.inventory[62]=1;c.entries["meeting-fixture"]=e;
 auto passage=e;passage.map=23;passage.playerTile={3,8};passage.direction=Direction::Down;c.entries["passage-fixture"]=passage;
 GameApplication a(c,"passage-fixture");a.update(press(InputAction::Confirm));
 for(int n=0;n<1000&&a.mapId()!=24;++n)a.update({});require(a.mapId()==24&&a.world().player().motion.tile()==TilePosition{13,5},"Actual Map23 exit reaches original meeting entry");
 for(int n=0;n<20000&&!a.session().flag(73);++n){a.update(n%11==0?press(InputAction::Confirm):InputFrame{});a.takePresentation();}
 require(a.session().flag(73)&&a.session().flag(1)&&a.session().flag(18)&&!a.session().flag(19),"Complete entry restores source control and marks S73");
 for(auto [id,tile]:std::initializer_list<std::pair<const char*,TilePosition>>{{"m24-e3",{11,7}},{"m24-e10",{10,5}},{"m24-e2",{11,6}},{"m24-e5",{14,5}}})require(findCharacter(a.world(),id).motion.tile()==tile,"Entry routes finish at original actor locations");
 require(a.world().player().motion.facing()==Direction::Left,"Entry leaves player facing left");
 // Optional gun has its own commit before award; it is not a meeting requirement.
 require(a.session().count(14)==0&&!a.session().flag(74),"Entry does not silently award shotgun");
 const int pouch=a.session().count(51),red=a.session().count(4);
 talk(a,"m24-e2");require(a.session().flag(80),"First Daimon conversation only marks its own flag");
 talk(a,"m24-e2");require(!a.session().flag(82),"Missing other conversations does not start meeting");
 for(int id:{6,10,5,4,3,7,8}){talk(a,"m24-e"+std::to_string(id));}
 for(const auto& flag:c.expansion.meeting.at("talk_flags"))require(a.session().flag(flag),"Every independent conversation completes");
 require(a.session().count(51)==pouch+1&&a.session().count(4)==red+1&&a.session().knowsSkill(3),"Actual talks grant pouch herb and skill");
 for(int id:{6,10,5,4,3,7,8})talk(a,"m24-e"+std::to_string(id));
 require(a.session().count(51)==pouch+1&&a.session().count(4)==red+1,"Repeat pages never repeat awards");
 require(a.mapId()==24&&a.foreground()==ForegroundKind::None&&!a.session().flag(74),"All flags do not auto-start or grant optional gun");
 if(c.expansion.meetingStory.is_null()){
 approach(a,"m24-e2");a.takePresentation();a.update(press(InputAction::Confirm));bool handoff=false;for(const auto& event:a.takePresentation())handoff|=event.kind==PresentationEvent::Kind::Notice;
 require(handoff,"Reinvestigation after all flags requests future meeting");
 }
 approach(a,"m24-e11");a.update(press(InputAction::Confirm));require(a.session().flag(74)&&!a.session().count(14),"Gun flag commits before pickup message and inventory award");finishTalk(a);
 require(a.session().count(14)==1&&a.session().magazine(14)==5,"Optional gun keeps original magazine seed");a.update(press(InputAction::Confirm));finishTalk(a);require(a.session().count(14)==1,"Gun cannot be claimed twice");
 // Domain fixture at legitimate save contexts; M24 itself never allows saving.
 for(int map:{16,22}){
  auto safe=e;safe.map=map;const std::string crow=map==16?"m16-e5":"m22-e2";safe.playerTile=adjacentTile(c.objects.at(crow).tile,Direction::Down);safe.direction=Direction::Up;
  GameSession durable(c.items,a.session().exportState(),c.facts);if(!c.expansion.meetingStory.is_null())durable.setFlag(84,true);CombatScene save(c,durable,safe);
  auto snapshot=captureSave(c,save,{save.generation(),crow,""},{});auto restored=prepareRestore(c,decodeSave(c,encodeSave(c,snapshot)));
  for(const auto& flag:c.expansion.meeting.at("talk_flags"))require(restored.session->flag(flag),"Independent talk facts persist");
  require(restored.session->flag(74)&&restored.session->count(14)==1&&restored.session->count(51)==pouch+1,"Optional pickup and reward survive value snapshot");
  auto invalid=snapshot;invalid.session.worldFlags[c.facts.switches.at(80)]=false;bool rejected=false;try{encodeSave(c,invalid);}catch(const std::exception&){rejected=true;}require(rejected,"Partial meeting cannot pretend to reach a safe point");
 }
 // Shared presentation: same-actor route replacement, parallel actors and barrier.
 auto fixture=e;fixture.session.flags[73]=true;GameSession shared(c.items,fixture.session,c.facts);CombatScene scene(c,shared,fixture);ScenePresentation player(scene);
 player.route("m24-e10-p2-c69","m24-e10");player.route("m24-e10-p2-c80","m24-e10");player.route("m24-e10-p2-c80","m24-e10");
 require(!player.executeCue("m24-e10-p2-c81","m24-e10"),"Barrier waits for all forced actors");
 for(int n=0;n<1000&&!player.executeCue("m24-e10-p2-c81","m24-e10");++n)scene.update({});
 require(player.executeCue("m24-e10-p2-c81","m24-e10"),"Parallel actors eventually finish barrier");
 require(findCharacter(scene.world(),"m24-e3").motion.tile()==TilePosition{11,7}&&findCharacter(scene.world(),"m24-e10").motion.tile()==TilePosition{10,5},"Shared executor keeps actor positions authoritative");
 bool missing=false;try{player.route("m24-e10-p2-c80","missing-actor");}catch(const std::exception&){missing=true;}
 // C80 targets this event; absence is an immediate binding error.
 require(missing,"Self-target route rejects missing actor at submission");
 // Preflight catches a late invalid actor before the entry's first control commit.
 auto broken=c;broken.routes.at("m24-e10-p2-c143").target=9999;GameSession unchanged(broken.items,e.session,broken.facts);CombatScene invalid(broken,unchanged,e);
 bool rejected=false;try{invalid.update({});}catch(const std::exception&){rejected=true;}
 require(rejected&&!unchanged.flag(73)&&unchanged.flag(1)==e.session.flags.at(1)&&unchanged.flag(18)==e.session.flags.at(18),"Missing later actor rejects entire entry before partial state mutation");
 auto oldGeneration=scene.generation();scene.deactivate();scene.update(press(InputAction::Confirm));require(scene.generation()==oldGeneration&&!shared.flag(75),"Deactivated flow cannot consume new input");
 std::cout<<"Meeting checks passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
