#include "title_navigation.h"
#include "combat_presentation.h"
#include "scene_renderer.h"
#include "opening_driver.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
InputFrame confirm(){InputFrame f;f.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;return f;}
int main(int argc,char** argv){
 if(argc!=3){return 2;}auto c=loadCombatContent(argv[1]);const std::filesystem::path output=argv[2];std::filesystem::create_directories(output);
 sf::Font font;if(!font.openFromFile(c.maps.at(4).fontPath)){throw std::runtime_error("font");}sf::RenderTexture target({960,720});
 // Real Map12 push: compare rendered pixels with the original ladder translated
 // to its destination. Other actors are identical and the player is hidden.
 {
  auto e=c.entries.at("defeated-claimed");e.pages.clear();e.positions.clear();e.through.clear();e.directions.clear();
  e.map=12;e.playerTile={30,8};e.direction=Direction::Right;c.entries["ladder-push"]=e;
  GameApplication game(c,"ladder-push");const auto original=findCharacter(game.world(),"m12-e9");
  InputFrame push;push.movement=Direction::Right;game.update(push);
  for(int tick=0;tick<300;++tick){game.update({});}
  const auto moved=findCharacter(game.world(),original.id);
  if(moved.motion.tile()!=TilePosition{32,8}){throw std::runtime_error("ladder render check did not push");}
  ExplorationScene expectedWorld(game.definition());
  for(const auto& actor:game.world().objects()){expectedWorld.setObjectState(actor);}
  auto expected=original;
  expected.motion=CharacterMotion(moved.motion.tile(),original.motion.facing(),original.motion.speed());
  expectedWorld.setObjectState(expected);
  SceneRenderer renderer(game.definition());
  auto view=makeSceneView({960,720},{320,240});view.setCenter({31*16.f,8*16.f});target.setView(view);
  const auto capture=[&](const ExplorationScene& world){target.clear();renderer.draw(target,world,{},false);target.display();return target.getTexture().copyToImage();};
  const auto actual=capture(game.world());const auto wanted=capture(expectedWorld);
  if(!actual.saveToFile(output/"ladder-push-actual.png")||!wanted.saveToFile(output/"ladder-push-expected.png")){throw std::runtime_error("ladder capture");}
  const auto bytes=actual.getSize().x*actual.getSize().y*4;
  if(!std::equal(actual.getPixelsPtr(),actual.getPixelsPtr()+bytes,wanted.getPixelsPtr())){throw std::runtime_error("pushing must render the original ladder frame at its new position");}
 }
 // Run the real full opening to the closed-door monologue. The last escort
 // remains in the model; the closed door must cover it, as in video 143.5s.
 {
  GameApplication game(c,"title");finishTitleOpening(game);if(game.menuSelection()==1){InputFrame up;up.buttons[static_cast<int>(InputAction::Up)].pressed=true;game.update(up);}game.update(confirm());
  for(int tick=0;tick<80000&&game.openingStage()!="monologue-explosion";++tick){game.update(openingInput(game,false,tick));}
  if(game.openingStage()!="monologue-explosion"){throw std::runtime_error("escort did not finish");}
  const auto soldier=findCharacter(game.world(),"m4-e13");
  if(soldier.motion.tile()!=TilePosition{7,4}||!soldier.spriteId){throw std::runtime_error("escort source state changed");}
  ExplorationScene expected(game.definition());expected.setPlayerState(game.world().player());for(const auto& a:game.world().objects()){expected.setObjectState(a);}auto hidden=soldier;hidden.spriteId.reset();expected.setObjectState(hidden);
  SceneRenderer renderer(game.definition());target.setView(makeSceneView({960,720},{320,240}));
  auto capture=[&](const ExplorationScene& world){target.clear();renderer.draw(target,world);target.display();return target.getTexture().copyToImage();};
  const auto actual=capture(game.world()),covered=capture(expected);
  if(!actual.saveToFile(output/"escort-closed-actual.png")||!covered.saveToFile(output/"escort-closed-covered.png")){throw std::runtime_error("escort capture");}
  if(!std::equal(actual.getPixelsPtr(),actual.getPixelsPtr()+960*720*4,covered.getPixelsPtr())){throw std::runtime_error("closed door must cover the last escort before control returns");}
  // Same presentation rule with unrelated IDs/position; no Map4 lookup in renderer.
  auto varied=game.definition();varied.objects.clear();
  for(const std::string id:{"m4-e13","m4-e2"}){
   auto source=std::find_if(game.definition().objects.begin(),game.definition().objects.end(),[&](const auto& a){return a.id==id;});
   auto object=*source;object.id="varied-"+id;object.sourceEventId+=100;object.tileX=10;object.tileY=6;object.direction="down";object.initialColumn=1;varied.objects.push_back(object);
  }
  ExplorationScene variant(varied),without(varied);auto removed=without.objects().front();removed.spriteId.reset();without.setObjectState(removed);
  SceneRenderer variedRenderer(varied);
  const auto drawVariant=[&](const ExplorationScene& world){target.clear();variedRenderer.draw(target,world,{},false);target.display();return target.getTexture().copyToImage();};
  const auto a=drawVariant(variant),b=drawVariant(without);
  if(!std::equal(a.getPixelsPtr(),a.getPixelsPtr()+960*720*4,b.getPixelsPtr())){throw std::runtime_error("same-depth presentation rule must survive object relocation and identity changes");}

 }
 // Presentation boundary: original rain has long, fading pixel streaks,
 // not the previous bright eight-pixel uniformly repeating dashes.
 {
  GameApplication game(c,"natural");CombatPresentation rain(c,font);
  rain.update({{PresentationEvent::Kind::Cue,"m87-e19-p1-c2",{},{},0}});
  for(int n=0;n<120;++n){rain.update({});}
  sf::RenderTexture sample({320,240});sample.setView(makeSceneView({320,240},{320,240}));sample.clear(sf::Color::Black);rain.draw(sample,game);sample.display();
  const auto image=sample.getTexture().copyToImage();if(!image.saveToFile(output/"rain-alone.png")){throw std::runtime_error("rain capture");}
  std::vector<bool> seen(320*240);int longest=0,lit=0,bright=0;
  for(int y=0;y<240;++y){for(int x=0;x<320;++x){
   const auto color=image.getPixel({unsigned(x),unsigned(y)});if(color.b){++lit;if(color.b>90){++bright;}}
   if(seen[y*320+x]||color.b==0){continue;}std::vector<std::pair<int,int>> queue{{x,y}};seen[y*320+x]=true;int low=y,high=y;
   for(std::size_t at=0;at<queue.size();++at){auto [px,py]=queue[at];low=std::min(low,py);high=std::max(high,py);
    for(int dy=-1;dy<=1;++dy){for(int dx=-1;dx<=1;++dx){int nx=px+dx,ny=py+dy;if(nx<0||nx>=320||ny<0||ny>=240||seen[ny*320+nx]||image.getPixel({unsigned(nx),unsigned(ny)}).b==0){continue;}seen[ny*320+nx]=true;queue.emplace_back(nx,ny);}}
   }longest=std::max(longest,high-low+1);
  }}
  if(longest<24||lit==0||bright>lit/20){throw std::runtime_error("rain must use long low-opacity fading streaks");}
  rain.update({{PresentationEvent::Kind::ResetGame,{},{},{},0}});sample.clear(sf::Color::Black);rain.draw(sample,game);sample.display();
  const auto clear=sample.getTexture().copyToImage();for(unsigned y=0;y<240;++y){for(unsigned x=0;x<320;++x){if(clear.getPixel({x,y})!=sf::Color::Black){throw std::runtime_error("reset must clear old rain");}}}
 }
 for(const std::string role:{"intro","hall","ladder","pictures","weather"}){
  auto e=c.entries.at("defeated-claimed");e.pages.clear();e.positions.clear();e.through.clear();e.directions.clear();
  if(role=="intro"){e.map=9;e.playerTile={4,7};e.direction=Direction::Up;e.session.flags[55]=false;}
  if(role=="hall"){e.map=15;e.playerTile={10,6};e.direction=Direction::Up;e.session.flags[58]=false;}
  if(role=="ladder"){e.map=12;e.playerTile={30,8};e.direction=Direction::Right;}
  if(role=="pictures"){e.map=14;e.playerTile={21,14};e.direction=Direction::Up;}
  if(role=="weather"){e.map=10;e.playerTile={3,10};e.direction=Direction::Right;}
  c.entries["presentation-fixture"]=e;GameApplication game(c,"presentation-fixture",[](int lo,int){return lo;});CombatPresentation presentation(c,font);
  auto renderer=std::make_unique<SceneRenderer>(game.definition());auto generation=game.generation();DialogueRenderer dialogue(font);dialogue.setMedia(presentation.images(),presentation.palette());
  for(int tick=0;tick<1500;++tick){
   game.update(tick==0||(role=="intro"&&tick%12==0)?confirm():InputFrame{});presentation.update(game.takePresentation());
   if(game.generation()!=generation){renderer=std::make_unique<SceneRenderer>(game.definition());generation=game.generation();}
   if(tick!=0&&tick!=90&&tick!=302&&tick!=600&&tick!=1499){continue;}
   auto view=makeSceneView({960,720},{320,240});const auto center=game.cameraCenter();view.setCenter({static_cast<float>(center.x),static_cast<float>(center.y)});target.setView(view);target.clear(sf::Color::Black);
   presentation.drawBackground(target);renderer->draw(target,game.world(),presentation.characterFlashes(),game.playerVisible());presentation.draw(target,game);dialogue.draw(target,game.dialogue().view());target.display();
   if(!target.getTexture().copyToImage().saveToFile(output/(role+"-"+std::to_string(tick)+".png"))){throw std::runtime_error("capture");}
  }
  if(role=="hall"&&(!game.session().flag(58)||!game.playerVisible())){throw std::runtime_error("hall presentation did not complete");}
 }
 std::cout<<"F3 presentation data rendered at five stages each; no claim of listening or native UI acceptance\n";
}
