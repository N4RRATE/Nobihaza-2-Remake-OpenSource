#include "route_player.h"
#include "combat_scene.h"
#include <stdexcept>
#include <iostream>
#include <tuple>
void require(bool b,const char* why){if(!b){throw std::runtime_error(why);}}
int main(int argc,char** argv){
 if(argc!=2){return 2;}const auto data=loadCombatContent(argv[1]);const auto& entry=data.entries.at("natural");GameSession session(data.items,entry.session);auto def=data.makeScene(entry.map,entry.playerTile,Direction::Up,session,&entry);ExplorationScene world(def);
 RouteDefinition route;route.frequency=8;route.steps={{"lock_direction"},{"down"}};RoutePlayer player(route,"player");std::vector<PresentationEvent> events;
 player.tick(world,session,events,[](int lo,int){return lo;});
 require(world.player().motion.facing()==Direction::Up,"locked route preserves facing while moving down");
 for(int i=0;i<8;++i){world.advanceCharacters();}require(world.player().motion.tile()==TilePosition{8,6},"locked route moved");
 // Through diagonal route: one continuous step, locked direction, no axis teleport.
 auto actor=world.player();actor.through=true;actor.motion=CharacterMotion({8,6},Direction::Up,32);world.setPlayerState(actor);
 RouteDefinition diagonal;diagonal.frequency=8;diagonal.steps={{"lock_direction"},{"up_right"}};
 RoutePlayer climb(diagonal,"player");climb.tick(world,session,events,[](int lo,int){return lo;});world.advanceCharacters();
 require(world.player().motion.tile()==TilePosition{9,5}&&world.player().motion.position().x>8&&world.player().motion.position().x<9,"diagonal destination reserves while both axes interpolate");
 require(world.player().motion.facing()==Direction::Up,"locked diagonal preserves sprite facing");
 for(int n=0;n<8;++n){world.advanceCharacters();}require(!world.player().motion.moving(),"diagonal has one tile's duration");
 // CN M190 E12P2 C53's jump body contains diagonal translations. The scanner
 // accumulates both axes once per command and preserves the locked graphic row.
 for(const auto& [kind,dx,dy]:std::initializer_list<std::tuple<const char*,int,int>>{{"up_left",-1,-1},{"up_right",1,-1},{"down_left",-1,1},{"down_right",1,1}}){
  auto varied=def;varied.width=7;varied.height=5;varied.passageMasks.assign(35,15);varied.objects.clear();varied.player.tileX=3;varied.player.tileY=2;
  ExplorationScene jumping(varied);auto a=jumping.player();a.through=true;a.motion=CharacterMotion({3,2},Direction::Left,32);jumping.setPlayerState(a);
  RouteDefinition jump;jump.frequency=8;jump.steps={{"lock_direction"},{"begin_jump"},{kind},{kind},{"end_jump"},{"switch_on",{},{},91}};
  RoutePlayer route(jump,"player");session.setFlag(91,false);route.tick(jumping,session,events,[](int lo,int){return lo;});
  require(jumping.player().motion.tile()==TilePosition{3+2*dx,2+2*dy}&&jumping.player().motion.jumping()&&!session.flag(91),"Diagonal jump accumulates both axes without releasing its tail");
  for(int n=0;n<200;++n){jumping.advanceCharacters();route.tick(jumping,session,events,[](int lo,int){return lo;});}
  require(jumping.player().motion.facing()==Direction::Left&&session.flag(91),"Diagonal jump preserves locked facing and executes tail only after landing");
 }
 // Shared fixed-facing semantics: logical forward follows the move direction,
 // while fixed animation types retain their page's graphic row.
 for(int type=0;type<=4;++type){
  auto varied=def;auto object=varied.objects.front();object.id="graphic-fixture";object.tileX=10;object.tileY=10;
  object.direction="left";object.initialColumn=1;object.animationType=type;object.moveSpeed=2;object.through=true;object.active=true;
  varied.objects={object};ExplorationScene moving(varied);
  RouteDefinition steps;steps.frequency=8;steps.steps={{"right"},{"forward"}};RoutePlayer movement(steps,object.id);
  for(int tick=0;tick<100;++tick){
   movement.tick(moving,session,events,[](int lo,int){return lo;});moving.advanceCharacters();
   const auto actor=findCharacter(moving,object.id);
   require(actor.graphicFacing()==(type>=2?Direction::Left:Direction::Right),"page-fixed graphic row is independent from logical route direction");
   if(type==4){require(actor.animation.column()==1,"fixed graphic keeps column during movement");}
  }
  RouteDefinition turn;turn.frequency=8;turn.steps={{"lock_direction"},{"face_up"}};RoutePlayer explicitTurn(turn,object.id);
  explicitTurn.tick(moving,session,events,[](int lo,int){return lo;});
  require(findCharacter(moving,object.id).graphicFacing()==Direction::Up,"explicit route turn updates even fixed graphic rows (door and soldier animation)");
  require(findCharacter(moving,object.id).motion.tile()==TilePosition{12,10},"forward uses logical right direction even with fixed graphic");
 }
 // Animation pause is independent from the page's movement/fixed/continuous type.
 for(int type=0;type<=5;++type){
  auto varied=def;auto object=varied.objects.front();object.id="pause-fixture";object.tileX=10;object.tileY=10;
  object.direction="left";object.initialColumn=0;object.animationType=type;object.moveSpeed=2;object.through=true;object.active=true;
  varied.objects={object};ExplorationScene toggling(varied);
  const auto command=[&](std::initializer_list<RouteStep> steps){RouteDefinition d;d.frequency=8;d.steps=steps;RoutePlayer r(d,object.id);r.tick(toggling,session,events,[](int lo,int){return lo;});};
  command({{"stop_animation"},{"face_up"},{"right"}});
  require(findCharacter(toggling,object.id).graphicFacing()==(type>=2?Direction::Up:Direction::Right),"Animation stop does not invent a facing lock, explicit turns retain fixed rows");
  for(int tick=0;tick<40;++tick){toggling.advanceCharacters();if(type<4){require(findCharacter(toggling,object.id).animation.column()==1,"Paused ordinary and continuous animations stay centered even while moving");}}
  if(type==4){require(findCharacter(toggling,object.id).animation.column()==0,"Pause preserves an explicitly fixed graphic column");}
  const auto rotation=findCharacter(toggling,object.id).animation.rotation();
  command({{"start_animation"}});
  bool rotated=false;int changes=0,previous=findCharacter(toggling,object.id).animation.column();
  for(int tick=0;tick<120;++tick){toggling.advanceCharacters();const int column=findCharacter(toggling,object.id).animation.column();changes+=column!=previous;previous=column;rotated|=findCharacter(toggling,object.id).animation.rotation()!=rotation;}
  require((changes>0)==(type==1||type==3),"Resume retains the source animation type: ordinary idle stops and continuous actors keep stepping");
  if(type==5){require(rotated,"Spin retains its own rotation while resumed");}
  command({{"left"}});
  int movingChanges=0;previous=findCharacter(toggling,object.id).animation.column();
  for(int tick=0;tick<32;++tick){toggling.advanceCharacters();const int column=findCharacter(toggling,object.id).animation.column();movingChanges+=column!=previous;previous=column;}
  if(type<4){require(movingChanges>0,"Resume permits ordinary walking animation again");}
 }
 // A continuous actor jumping must center for the jump and resume only on landing.
 {
  ExplorationScene jumping(def);auto a=jumping.player();a.through=true;a.animation=CharacterAnimation(animationForSpeed(2),0,1);jumping.setPlayerState(a);
  RouteDefinition d;d.frequency=8;d.steps={{"begin_jump"},{"left"},{"end_jump"}};RoutePlayer jump(d,"player");jump.tick(jumping,session,events,[](int lo,int){return lo;});
  while(jumping.player().motion.jumping()){jumping.advanceCharacters();require(jumping.player().animation.column()==1,"Jump temporarily centers a continuous animation");}
  bool stepped=false;for(int tick=0;tick<40;++tick){jumping.advanceCharacters();stepped|=jumping.player().animation.column()!=1;}
  require(stepped,"Jump does not permanently pause continuous animation");
 }
 // Page-driven pose refresh preserves route pause; a newly created scene owns fresh state.
 {
  ExplorationScene changed(def);auto a=changed.objects().front();a.animation.setPaused(true);
  a.setAnimation(CharacterAnimation(animationForSpeed(2),0,1),Direction::Left);changed.setObjectState(a);
  for(int tick=0;tick<40;++tick){changed.advanceCharacters();}
  require(findCharacter(changed,a.id).animation.paused()&&findCharacter(changed,a.id).animation.column()==1,"Changing a page pose does not discard its actor's route pause");
  ExplorationScene next(def);require(!findCharacter(next,a.id).animation.paused(),"A new scene does not inherit another scene's animation pause");
 }
 // Continuous repeat across a different loop width: no extra stopped update,
 // and an immediate-only route stays bounded (one sound per tick).
 {
  auto sample=def;sample.width=3;sample.height=2;sample.loopX=true;sample.passageMasks.assign(6,15);sample.objects.clear();sample.player.tileX=1;sample.player.tileY=1;sample.player.subunitsPerUpdate=64;
  ExplorationScene loop(sample);auto p=loop.player();p.through=true;loop.setPlayerState(p);
  RouteDefinition repeating;repeating.frequency=8;repeating.repeat=true;repeating.steps={{"left"}};RoutePlayer movement(repeating,"player");
  for(int n=0;n<100;++n){const auto before=loop.player().motion.position().x;movement.tick(loop,session,events,[](int lo,int){return lo;});loop.advanceCharacters();require(loop.player().motion.position().x==before-.25,"loop route moves continuously across tile and wrap boundaries");}
  RouteDefinition immediate;immediate.frequency=8;immediate.repeat=true;immediate.steps={{"sound"}};RoutePlayer sounds(immediate,"player");
  for(int n=0;n<5;++n){const auto count=events.size();sounds.tick(loop,session,events,[](int lo,int){return lo;});require(events.size()==count+1,"all-immediate repeat is bounded to one pass per tick");}
 }
 {
  ExplorationScene jumping(def);auto actor=jumping.player();actor.through=true;jumping.setPlayerState(actor);
  RouteDefinition jump;jump.frequency=8;jump.steps={{"begin_jump"},{"left"},{"through_off"},{"end_jump"},{"switch_on",{},{},91}};
  RoutePlayer route(jump,"player");session.setFlag(91,false);route.tick(jumping,session,events,[](int lo,int){return lo;});
  require(jumping.player().through&&!session.flag(91),"Jump scan skips through toggle and defers post-jump switch");
  for(int tick=0;tick<200;++tick){jumping.advanceCharacters();route.tick(jumping,session,events,[](int lo,int){return lo;});}
  require(jumping.player().through&&session.flag(91)&&jumping.player().motion.tile()==TilePosition{7,5},"Post-jump switch executes after landing without replaying skipped toggle");
 }
 {
  ExplorationScene through(def);
  for(bool enabled:{true,false}){RouteDefinition toggle;toggle.frequency=8;toggle.steps={{enabled?"through_on":"through_off"}};RoutePlayer route(toggle,"player");route.tick(through,session,events,[](int lo,int){return lo;});require(through.player().through==enabled&&through.player().routeThrough==enabled,"Explicit through on/off stores route state independently of page defaults");}
 }
 // Guard corpse routes may jump at a map edge; source skip-blocked controls continuation.
 for(bool skip:{false,true}){
  ExplorationScene edge(def);auto actor=edge.player();actor.motion=CharacterMotion({0,0},Direction::Up,32);edge.setPlayerState(actor);
  RouteDefinition jump;jump.frequency=8;jump.skipBlocked=skip;jump.steps={{"begin_jump"},{"up"},{"up"},{"end_jump"},{"switch_on",{},{},92}};
  RoutePlayer route(jump,"player");session.setFlag(92,false);route.tick(edge,session,events,[](int lo,int){return lo;});
  require(edge.player().motion.tile()==TilePosition{0,0}&&!edge.player().motion.moving()&&session.flag(92)==skip,"Out-of-map jump waits or skips without committing an invalid World position");
 }
 {
  ExplorationScene jumping(def);auto actor=jumping.player();actor.motion=CharacterMotion({0,0},Direction::Right,32);actor.through=false;jumping.setPlayerState(actor);
  auto blocker=jumping.objects().front();blocker.motion=CharacterMotion({1,0},Direction::Up,32);blocker.active=true;blocker.layer=1;blocker.through=false;jumping.setObjectState(blocker);
  RouteDefinition jump;jump.frequency=8;jump.steps={{"begin_jump"},{"right"},{"right"},{"end_jump"}};RoutePlayer route(jump,"player");route.tick(jumping,session,events,[](int lo,int){return lo;});
  for(int tick=0;tick<200;++tick){jumping.advanceCharacters();route.tick(jumping,session,events,[](int lo,int){return lo;});}
  require(jumping.player().motion.tile()==TilePosition{2,0},"Legal source jump crosses occupied intermediate tiles rather than using ordinary step collision");
 }
 // CN M73 E3 P3 jumps one to four forward tiles, preserving the launch facing.
 for(const auto facing:{Direction::Up,Direction::Right,Direction::Down,Direction::Left}){
  ExplorationScene jumping(def);auto actor=jumping.objects().front();const TilePosition origin{10,10};actor.motion=CharacterMotion(origin,facing,32);actor.through=true;jumping.setObjectState(actor);
  RouteDefinition jump;jump.frequency=8;jump.steps={{"begin_jump"},{"forward"},{"forward"},{"forward"},{"end_jump"}};
  RoutePlayer route(jump,actor.id);route.tick(jumping,session,events,[](int lo,int){return lo;});
  auto destination=origin;for(int i=0;i<3;++i){destination=adjacentTile(destination,facing);}
  for(int i=0;i<200;++i){jumping.advanceCharacters();route.tick(jumping,session,events,[](int lo,int){return lo;});}
  require(findCharacter(jumping,actor.id).motion.tile()==destination&&findCharacter(jumping,actor.id).motion.facing()==facing,"Jump forward scans the launch direction for every body step in all four directions");
 }
 // Two away steps are scanned from the actor's original position, not the accumulating jump offset.
 for(const auto origin:{TilePosition{10,5},TilePosition{8,8}}){
  ExplorationScene jumping(def);auto actor=jumping.objects().front();actor.motion=CharacterMotion(origin,Direction::Up,32);actor.through=true;jumping.setObjectState(actor);
  RouteDefinition jump;jump.frequency=8;jump.steps={{"begin_jump"},{"away_player"},{"away_player"},{"end_jump"}};
  RoutePlayer route(jump,actor.id);route.tick(jumping,session,events,[](int lo,int){return lo;});
  const auto destination=origin.x==10?TilePosition{12,5}:TilePosition{8,10};
  for(int tick=0;tick<200;++tick){jumping.advanceCharacters();route.tick(jumping,session,events,[](int lo,int){return lo;});}
  require(findCharacter(jumping,actor.id).motion.tile()==destination,"Jump away uses player position on both axes");
 }
 // Underground source turns use the shared finite route reader and executor.
 for(const auto facing:{Direction::Up,Direction::Right,Direction::Down,Direction::Left}){
  ExplorationScene turning(def);auto a=turning.objects().front();a.motion=CharacterMotion({10,5},facing,32);a.setAnimation(CharacterAnimation(animationForSpeed(2),1,2),facing);turning.setObjectState(a);
  const auto source=readRouteDefinition(nlohmann::json{{"target",1},{"frequency",8},{"repeat",false},{"skip_blocked",false},{"steps",nlohmann::json::array({{{"kind","lock_direction"}},{{"kind","turn_around"}}})}});
  RoutePlayer turn(source,a.id);require(turn.tick(turning,session,events,[](int lo,int){return lo;}),"Immediate opposite turn completes without inventing movement");
  const auto expected=facing==Direction::Up?Direction::Down:facing==Direction::Right?Direction::Left:facing==Direction::Down?Direction::Up:Direction::Right;
  require(findCharacter(turning,a.id).graphicFacing()==expected&&findCharacter(turning,a.id).motion.tile()==TilePosition{10,5},"Opposite turn updates the fixed graphic row and preserves position");
  const auto away=readRouteDefinition(nlohmann::json{{"frequency",8},{"repeat",false},{"skip_blocked",false},{"steps",nlohmann::json::array({{{"kind","face_away"}}})}});
  RoutePlayer face(away,a.id);face.tick(turning,session,events,[](int lo,int){return lo;});
  require(findCharacter(turning,a.id).graphicFacing()==Direction::Right,"Source face-away turns from the current player rather than stale page direction");
 }
 {
  ExplorationScene fading(def);const auto id=fading.objects().front().id;
  const auto command=[&](const char* kind){return readRouteDefinition(nlohmann::json{{"frequency",8},{"repeat",false},{"skip_blocked",false},{"steps",nlohmann::json::array({{{"kind",kind}}})}});};
  for(int step=0;step<10;++step){RoutePlayer down(command("opacity_down"),id);down.tick(fading,session,events,[](int lo,int){return lo;});
   if(step==0){require(findCharacter(fading,id).opacity()==223,"Source first transparency increment gives223 rather than an arbitrary percentage");}}
  require(findCharacter(fading,id).opacity()==31,"Eight-level source transparency saturates at31 rather than hiding the actor");
  require(fading.player().opacity()==255,"A different actor's route does not change player transparency");
  for(int step=0;step<10;++step){RoutePlayer up(command("opacity_up"),id);up.tick(fading,session,events,[](int lo,int){return lo;});}
  require(findCharacter(fading,id).opacity()==255,"Repeated opposite commands restore opacity and saturate at255");
 }
 {
  auto source=def;source.objects.front().transparency=3;const auto id=source.objects.front().id;
  ExplorationScene initial(source);require(findCharacter(initial,id).opacity()==159,"A translucent source page initializes its own three transparency levels");
  RouteDefinition down;down.frequency=8;down.steps={{"opacity_down"}};RoutePlayer fade(down,id);fade.tick(initial,session,events,[](int lo,int){return lo;});
  require(findCharacter(initial,id).opacity()==127,"A route changes only the active actor state, not the shared page definition");
  ExplorationScene recreated(source);require(findCharacter(recreated,id).opacity()==159&&recreated.player().opacity()==255,"A new scene rebuilds source transparency independently");
  auto invalid=recreated.player();invalid.transparency=8;bool rejected=false;try{recreated.setPlayerState(invalid);}catch(const std::exception&){rejected=true;}
  require(rejected&&recreated.player().opacity()==255,"Illegal player transparency cannot overwrite valid state");
 }
 if(!data.catalog.is_null()){
  auto c=data;auto e=c.entries.at("natural");const std::string id="m4-e1";
  auto first=c.objects.at(id).variants.front();first.page=1;first.flags.clear();first.variableMinimum.reset();first.inventoryItem.reset();first.actor.reset();first.timerMaximumSeconds.reset();first.definition.transparency=3;
  auto second=first;second.page=2;second.flags[60]=true;second.definition.transparency=0;c.objects.at(id).variants={first,second};e.pages.erase(id);
  GameSession s(c.items,e.session,c.facts);s.setFlag(60,false);CombatScene room(c,s,e);
  require(findCharacter(room.world(),id).opacity()==159,"Initial active page uses the reviewed translucent default");
  s.setFlag(60,true);room.update({});require(findCharacter(room.world(),id).opacity()==255,"A newly effective opaque event page resets earlier route transparency");
 }
 std::cout<<"route checks passed\n";
}
