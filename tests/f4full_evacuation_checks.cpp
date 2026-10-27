#include "game_application.h"
#include "full_content.h"
#include "scene_presentation.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <queue>
#include <stdexcept>
namespace{
void need(bool v,const std::string& why){if(!v){throw std::runtime_error(why);}}
InputFrame key(InputAction a=InputAction::Confirm){InputFrame f;f.buttons[static_cast<int>(a)].pressed=true;return f;}
CombatEntry entry(const CombatContent& c,int map,TilePosition tile){auto e=c.entries.at("post-pickup");e.map=map;e.playerTile=tile;e.direction=Direction::Up;e.sprite=c.maps.at(map).player.spriteId;e.pages.clear();e.positions.clear();e.through.clear();e.directions.clear();initializeFullSession(c,e.session);e.session.flags[1]=e.session.flags[18]=true;e.session.flags[321]=true;return e;}
void tick(CombatScene& s,int n){while(n--){s.update({});}}
void settle(CombatScene& s){for(int i=0;i<60000;++i){s.update(key());if(s.foreground()==ForegroundKind::None&&!s.dialogue().active()&&!s.world().player().motion.moving()){return;}}throw std::runtime_error("Did not settle map"+std::to_string(s.mapId()));}
using Cell=std::pair<int,int>;
bool path(const CombatScene& s,TilePosition target,std::vector<Direction>& result){auto origin=s.world().player().motion.tile();std::queue<Cell> q;std::map<Cell,std::pair<Cell,Direction>> previous;q.push({origin.x,origin.y});previous[{origin.x,origin.y}]={{origin.x,origin.y},Direction::Up};while(!q.empty()){auto at=q.front();q.pop();if(at==Cell{target.x,target.y}){for(auto x=at;x!=Cell{origin.x,origin.y};x=previous.at(x).first){result.push_back(previous.at(x).second);}std::reverse(result.begin(),result.end());return true;}for(auto d:{Direction::Up,Direction::Right,Direction::Down,Direction::Left}){auto p=s.world().player();p.through=false;p.motion=CharacterMotion({at.first,at.second},d,p.motion.speed());auto next=adjacentTile(p.motion.tile(),d);Cell cell{next.x,next.y};if(!previous.contains(cell)&&s.world().canMove(p,d)){previous[cell]={at,d};q.push(cell);}}}return false;}
void walk(CombatScene& s,TilePosition target){
 const int map=s.mapId();
 for(int moves=0;moves<2000&&s.world().player().motion.tile()!=target;++moves){
  std::vector<Direction> route;if(!path(s,target,route)){s.update({});continue;}
  InputFrame f;f.movement=route.front();s.update(f);
  for(int n=0;n<512&&s.world().player().motion.moving();++n){s.update({});}
  if(s.foreground()!=ForegroundKind::None){settle(s);}if(s.mapId()!=map){return;}
  // M119's source NPC routes move while the player walks; plan again around their current occupancy.
 }
 need(s.world().player().motion.tile()==target,"Natural input reaches tile on map"+std::to_string(s.mapId())+" expected"+std::to_string(target.x)+","+std::to_string(target.y));
}
void action(const CombatContent& c,CombatScene& s,const std::string& id,bool settleResult=true){auto tile=c.objects.at(id).tile;if(findCharacter(s.world(),id).layer!=1){const int map=s.mapId();walk(s,tile);if(s.mapId()!=map){return;}s.update(key());if(settleResult){settle(s);}return;}for(auto d:{Direction::Down,Direction::Left,Direction::Right,Direction::Up}){std::vector<Direction> route;auto from=adjacentTile(tile,d);if(path(s,from,route)){const int map=s.mapId();walk(s,from);if(s.mapId()!=map){return;}InputFrame f;f.movement=d==Direction::Down?Direction::Up:d==Direction::Up?Direction::Down:d==Direction::Left?Direction::Right:Direction::Left;s.update(f);while(s.world().player().motion.moving()){s.update({});}if(s.mapId()!=map){return;}s.update(key());if(settleResult){settle(s);}return;}}throw std::runtime_error("No action path: "+id);}
}
int main(int argc,char** argv){try{
need(argc==2,"usage evacuation package");auto c=loadCombatContent(argv[1]);need(c.catalog.contains("escort"),"105 complete escort capability loaded");
auto start=entry(c,116,{15,12});start.session.flags[341]=true;start.session.variables[269]=10;start.session.variables[267]=2;
GameSession state(c.items,start.session,c.facts);CombatScene scene(c,state,start);std::vector<PresentationEvent> history;std::set<int> visited;int falls=0;
for(int n=0;n<60000;++n){scene.update(key());visited.insert(scene.mapId());auto events=scene.takePresentation();for(const auto& event:events){if(event.id=="m116-e27-p1-c72"){++falls;}history.push_back(event);}if(scene.mapId()==119&&scene.foreground()==ForegroundKind::None&&!scene.dialogue().active()){break;}}
need(scene.mapId()==119&&visited.contains(118),"Actual source score/story reaches116->118->119");need(state.variable(394)==40,"C1-8 score commits exactly once before105 tail");need(falls==2,"Both original E27 fall invocations execute fully");
need(std::any_of(history.begin(),history.end(),[](const auto& e){return e.id=="m118-e21-p1-c61";}),"Original118 route/fade executes");need(state.flag(342)&&state.flag(343)&&state.flag(344)&&state.flag(1)&&state.flag(18)&&!state.flag(13),"Original scene facts and controls restored at119");
need(state.inParty(1)&&!state.inParty(3)&&!scene.companionHp(),"Escort never re-adds encounter-local Actor3");
std::cout<<"119 tail verified; checking E4\n"<<std::flush;action(c,scene,"m119-e4");need(scene.mapId()==121&&scene.world().player().motion.tile()==TilePosition{11,28},"Natural119 E4 source flow lands121 at11,28");
std::cout<<"E4 landing verified; walking final exit\n"<<std::flush;action(c,scene,"m121-e2",false);need(scene.takeApplicationRequest()==ApplicationMode::Boundary&&scene.exitRequest()&&scene.exitRequest()->completed,"Actual final exit submits visible completion intent");
need(scene.mapId()==121&&state.flag(1)&&state.flag(18)&&!state.flag(13)&&scene.playerVisible(),"Completion intercepted before first lock/control/black command");auto events=scene.takePresentation();need(std::none_of(events.begin(),events.end(),[](const auto& e){return e.id.starts_with("m121-e2-p1-c");}),"No source121 departure presentation begins");need(!scene.takeSaveRequest(),"Boundary never requests automatic save");
const auto at=scene.world().player().motion.tile();scene.dismissBoundary();for(int n=0;n<3;++n){scene.update(key());need(!scene.takeApplicationRequest()&&!scene.exitRequest(),"Confirm after staying is suppressed inside final action region");}need(scene.world().player().motion.tile()==at,"Staying keeps same Scene");
// Real second119 entrance, with an independent existing Scene instance.
{std::cout<<"Final intercept verified; checking E5\n"<<std::flush;auto second=entry(c,119,{31,23});GameSession s(c.items,second.session,c.facts);CombatScene room(c,s,second);action(c,room,"m119-e5");need(room.mapId()==121&&room.world().player().motion.tile()==TilePosition{11,27},"Original E5 landing remains independent of E4");auto stale=room.requestInput(NumberPrompt{});need(room.cancelInput(stale),"Old prompt cancelled before final action");action(c,room,"m121-e2",false);need(!room.takeInputResult(stale)&&!room.cancelInput(stale),"Final intercept invalidates old prompt identity");auto snapshot=s.exportState();room.deactivate();need(!room.takeApplicationRequest()&&!room.exitRequest(),"Retired Scene drops pending boundary request");tick(room,60);need(s.exportState()==snapshot,"Retired Scene cannot commit old departure");}
// Application keys have the same advertised meaning, and retirement reaches true Title.
{auto application=c;auto boundary=entry(c,121,adjacentTile(c.objects.at("m121-e2").tile,Direction::Down));application.entries["completion-fixture"]=boundary;GameApplication app(application,"completion-fixture");app.update(key());need(app.mode()==ApplicationMode::Boundary&&app.exitRequest()->completed,"Real Application receives final boundary");app.update(key());need(app.mode()==ApplicationMode::Playing&&app.hasGame()&&app.mapId()==121,"Z stays in active Scene");app.update(key());need(app.mode()==ApplicationMode::Playing,"Held/repeated Z never reopens completion");InputFrame move;move.movement=Direction::Down;app.update(move);for(int n=0;n<40;++n){app.update({});}move.movement=Direction::Up;app.update(move);for(int n=0;n<40;++n){app.update({});}app.update(key());need(app.mode()==ApplicationMode::Boundary,"Leaving action area rearms final exit");app.update(key(InputAction::Cancel));need(app.mode()==ApplicationMode::Title&&!app.hasGame()&&!app.exitRequest(),"X destroys prior Scene and returns to real Title");}
// Tile replacements apply to World geometry and remain local across Scene lifetime.
{auto room=c.maps.at(116);room.width+=2;room.height+=1;room.passageMasks.assign(room.width*room.height,15);room.player.tileX=13;room.player.tileY=6;ExplorationScene world(room);world.substituteUpperTile(66,56);need(world.substitutedUpperTiles().contains(66)&&world.canPlayerMove(Direction::Up),"Changed map width uses cell coordinates rather than hidden stride");world.substituteUpperTile(66,56);need(world.substitutedUpperTiles().size()==1,"Repeated replacement remains idempotent");world.substituteUpperTile(60,50);auto actor=world.player();actor.motion=CharacterMotion({13,5},Direction::Up,actor.motion.speed());world.setPlayerState(actor);need(!world.canPlayerMove(Direction::Up),"Original upper destination retains blocked passage");ExplorationScene rebuilt(room);need(rebuilt.substitutedUpperTiles().empty(),"Scene reconstruction discards presentation replacement state");bool rejected=false;try{world.substituteUpperTile(66,999);}catch(const std::exception&){rejected=true;}need(rejected,"Unknown substitution rejected rather than guessed");}
std::cout<<"105 actual116->118->119->121, two fall calls/door landings, pre-lock completion/Z/X/suppression/lifetime and changed-size tile rules passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
