#include "title_navigation.h"
#include "game_application.h"
#include <iostream>
#include <stdexcept>
#include <queue>
#include <map>
void require(bool b,const char* m){if(!b){throw std::runtime_error(m);}}
InputFrame key(InputAction a){InputFrame i;i.buttons[static_cast<int>(a)]={true,true,false};return i;}
std::optional<Direction> path(const GameApplication& app,TilePosition goal){
 const auto& world=app.world();auto actor=world.player();const auto start=actor.motion.tile();
 std::queue<TilePosition> q;q.push(start);std::map<std::pair<int,int>,std::optional<Direction>> seen;seen[{start.x,start.y}]={};
 while(!q.empty()){auto at=q.front();q.pop();if(at==goal){return seen.at({at.x,at.y});}
 for(int d=0;d<4;++d){auto dir=static_cast<Direction>(d);actor.motion=CharacterMotion(at,dir,32);if(!world.canMove(actor,dir)){continue;}auto next=adjacentTile(at,dir);auto k=std::pair{next.x,next.y};if(seen.contains(k)){continue;}seen[k]=seen.at({at.x,at.y}).value_or(dir);q.push(next);}}
 throw std::runtime_error("No path to opening door");
}
int main(int argc,char** argv){
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);GameApplication app(c,"title");finishTitleOpening(app);if(app.menuSelection()==1){InputFrame up;up.buttons[static_cast<int>(InputAction::Up)].pressed=true;app.update(up);}app.update(key(InputAction::Confirm));
 require(app.hasGame()&&app.mapId()==2,"New Game starts a fresh Map2 session, not natural combat fixture");
 require(!app.session().count(11)&&!app.session().flag(52)&&app.session().saveCount()==0,"new game no inventory/story leak");
 for(bool skip:{true,false}){
  GameApplication game(c,"title");finishTitleOpening(game);if(game.menuSelection()==1){InputFrame up;up.buttons[static_cast<int>(InputAction::Up)].pressed=true;game.update(up);}game.update(key(InputAction::Confirm));bool arrived=false;bool transport=false;int loopTicks=0;int transportPauses=0;
  for(int tick=0;tick<80000&&game.openingActive();++tick){
   InputFrame input;const auto stage=game.openingStage();transport|=game.mapId()==87;if(game.mapId()==87){++loopTicks;}
   const auto& prompt=game.inputPromptView();
   if(prompt.cursor){int desired=stage=="opening-choice"?(skip?1:0):1;input=key(prompt.cursor->line==desired?InputAction::Confirm:InputAction::Down);}
   else if(stage=="door-approach-common"){
    arrived=true;for(const auto& a:game.world().objects()){if(a.id=="m4-e8"&&!game.session().flag(46)){require(a.motion.speed()==4,"page change updates real E8 speed in full and skip");}}const auto door=c.objects.at("m4-e2").tile;const TilePosition goal{door.x,door.y+1};const auto& player=game.world().player();
    if(!player.motion.moving()){
      if(player.motion.tile()==goal){if(player.motion.facing()!=Direction::Up){input.movement=Direction::Up;}else{input=key(InputAction::Confirm);}}
      else{input.movement=path(game,goal);}
    }
   }else if(tick%13==0){input=key(InputAction::Confirm);}
   auto priorPlayer=game.world().player().motion.position();
   game.update(input);
   if(!skip&&game.mapId()==87&&loopTicks>=500&&loopTicks<520){
     if(game.world().player().motion.position().x==priorPlayer.x){++transportPauses;}
     const auto front=findCharacter(game.world(),"m87-e1");
     for(const auto& a:game.world().objects()){
       if(a.eventId>18){continue;}
       const auto expected=c.objects.at(a.id).tile.x-c.objects.at(front.id).tile.x;
       require(std::abs(a.motion.position().x-front.motion.position().x-expected)<1e-9,
               "moving vehicle pieces must remain joined regardless of string key ordering");
     }
   }

   if(game.mode()==ApplicationMode::Title){for(const auto& e:game.takePresentation()){if(e.kind==PresentationEvent::Kind::Notice){std::cerr<<e.id<<"\n";}}throw std::runtime_error("opening failed to title");}
  }
  std::cerr<<"branch "<<skip<<" stage "<<game.openingStage()<<" active "<<game.openingActive()<<" pos "<<game.world().player().motion.tile().x<<","<<game.world().player().motion.tile().y<<"\n";
  require(arrived&&!game.openingActive()&&game.mapId()==4&&game.session().flag(51),"opening naturally reaches collapse handoff");
  const auto soldier=findCharacter(game.world(),"m4-e4");require(soldier.active&&soldier.motion.tile()==TilePosition{7,5}&&soldier.spriteId=="corpse-6","collapse hands off visible dormant soldier at source relocation");
  require(transport==!skip&&(!transport||loopTicks>1000),"full branch transports over looping map; skip bypasses");
  require(game.session().count(180)==1&&game.session().count(11)==0&&game.session().magazine(11)==15&&game.session().magazine(20)==6&&game.session().equipment().armor==45,"opening commits normal equipment and seeds without weapons");
  if(c.items.contains(14)){require(game.session().magazine(14)==5&&game.session().magazine(21)==8&&!game.session().count(14)&&!game.session().count(21),"F4 opening seeds unowned weapons at original branch commands");}
  require(game.session().knowsSkill(7)&&game.session().knowsSkill(8)&&game.session().knowsSkill(9)&&!game.session().flag(1)&&!game.session().flag(18),"handoff skills and permissions");
  require(transportPauses==0,"continuous frequency-8 transport must not pause at every tile/repeat boundary");
  std::string error;require(game.newGame(error)&&game.mapId()==2&&!game.session().flag(51)&&!game.session().count(180)&&game.session().exportState().party.empty(),"new game from old progress has no leak");
 }
 Dialogue controls;MessageDefinition message;message.lines={MessageLine{"",{{MessageTokenKind::Text,U"前",0},{MessageTokenKind::WaitInput,{},0},{MessageTokenKind::Text,U"后",0},{MessageTokenKind::LiteralSlash,{},0},{MessageTokenKind::AutoClose,{},0}}}};
 controls.request(message);for(int i=0;i<100;++i){controls.update(false,false);}require(controls.active()&&controls.view().lines[0]==U"前","inline input wait blocks later glyphs");
 controls.update(true,false);for(int i=0;i<100;++i){controls.update(false,false);}require(!controls.active()&&controls.takeCompletion().has_value(),"inline wait resumes then auto-closes");
 CharacterMotion jump({3,4},Direction::Left,32);require(jump.beginJump({0,0}),"in-place jump starts");int jumpTicks=0;double maximum=0;while(jump.moving()){jump.advance();maximum=std::max(maximum,jump.jumpHeight());++jumpTicks;}
 require(jumpTicks==11&&maximum==16&&jump.tile()==TilePosition{3,4}&&jump.facing()==Direction::Left,"source zero jump timing height facing");
 TerrainMap wrapped(3,2,{15,15,15,0,0,0});require(wrapped.canTraverse({0,0},Direction::Left,true)&&!wrapped.canTraverse({0,0},Direction::Left),"horizontal loop respects map setting");require(!wrapped.canTraverse({0,1},Direction::Left,true),"loop edge still checks tile passage");
 std::string preparationError;GameApplication failed(c,"title");failed.configureRestorePresentation([](const SceneDefinition&,const SavedPresentation&){throw std::runtime_error("damaged media");});
 require(!failed.newGame(preparationError)&&!failed.hasGame()&&failed.mode()==ApplicationMode::Title,"failed New Game candidate leaves clean title");
 std::cout<<"Opening checks passed\n";
}
