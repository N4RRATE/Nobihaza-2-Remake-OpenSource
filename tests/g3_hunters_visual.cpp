#include "combat_presentation.h"
#include "g1_piano_driver.h"
#include "scene_renderer.h"
// Explicit offscreen engineering views, not natural route or user acceptance.
int main(int argc,char** argv){try{
 need(argc==4,"06 package real58 isolated-render-output");auto c=loadCombatContent(argv[1]);const auto saved=decodeSave(c,read(argv[2]));const auto root=std::filesystem::path(argv[3]);std::filesystem::create_directories(root);
 for(const auto& name:{"b1-hidden","b1-three-active","b1-dead","card-room"}){
  auto e=c.entries.at("post-pickup");e.map=std::string(name)=="card-room"?187:185;e.playerTile=e.map==187?TilePosition{9,8}:TilePosition{19,10};e.direction=Direction::Up;e.session=saved.session;e.positions.clear();e.directions.clear();e.pages.clear();e.through.clear();e.session.flags[1]=e.session.flags[18]=e.session.flags[13]=true;
  e.session.worldFlags[c.facts.switches.at(462)]=std::string(name)!="b1-hidden";if(std::string(name)=="b1-dead"){for(int fact:{403,404,405}){e.session.worldFlags[c.facts.switches.at(fact)]=true;}}
  c.entries["render06"]=e;GameApplication game(c,"render06",safe);sf::Font font;need(font.openFromFile(game.definition().fontPath),"Missing06 font");CombatPresentation present(c,font);sf::RenderTexture surface({320,240});
  for(int n=0;n<2;++n){game.update({});auto events=game.takePresentation();std::erase_if(events,[](const auto& v){return v.kind==PresentationEvent::Kind::Music||v.kind==PresentationEvent::Kind::Sound;});present.update(events);}
  if(e.map==185){for(const auto& id:{"m185-e2","m185-e10","m185-e14"}){const auto a=findCharacter(game.world(),id);need(a.spriteId.has_value()==(std::string(name)!="b1-hidden")&&a.active==(std::string(name)!="b1-dead"),"06 actual render actor selection wrong");}}
  const auto xy=game.cameraCenter();surface.setView(sf::View({float(xy.x),float(xy.y)},{320,240}));surface.clear(sf::Color::Black);SceneRenderer renderer(game.definition());renderer.draw(surface,game.world(),present.characterFlashes());present.draw(surface,game);surface.display();need(surface.getTexture().copyToImage().saveToFile(root/(std::string(name)+".png")),"06 frame export failed");
 }
 std::cout<<"06 actual34x20 B1 hidden/three-active/dead and20x15 zombie/card offscreen render PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
