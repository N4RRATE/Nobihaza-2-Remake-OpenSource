#include "combat_presentation.h"
#include "scene_renderer.h"
#include <iostream>
// Model-input visual fixture, not a check of physical Shift+mouse delivery.
int main(int argc,char** argv){try{
 if(argc!=3){return 2;}auto c=loadCombatContent(argv[1]);auto e=c.entries.at("defeated-claimed");const bool grenade=std::string(argv[2])=="grenade";
 e.map=28;e.playerTile={10,8};e.direction=Direction::Left;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.auxiliaryWeapon=grenade?29:27;e.session.inventory[29]=5;e.session.magazines[27]=7;c.entries["aux-visual"]=e;
 sf::Font font;if(!font.openFromFile(c.maps.begin()->second.fontPath)){return 3;}
 sf::RenderWindow window(sf::VideoMode({960,720}),std::string("81 auxiliary visual - ")+argv[2]);window.setFramerateLimit(60);
 std::unique_ptr<SceneRenderer> renderer;GameApplication app(c,"aux-visual",{},[&](const SceneDefinition& d){renderer=std::make_unique<SceneRenderer>(d);});CombatPresentation media(c,font);
 DialogueRenderer dialogue(font);dialogue.setMedia(media.images(),media.palette());int tick=0;
 while(window.isOpen()){
  while(auto event=window.pollEvent()){if(event->is<sf::Event::Closed>()){window.close();}}
  if(!window.isOpen()){break;}InputFrame input;
  if(tick==1){input.buttons[static_cast<int>(InputAction::AuxiliaryWeapon)].pressed=true;}
  if(tick==180){input.buttons[static_cast<int>(InputAction::Attack)].pressed=true;}
  app.update(input);media.update(app.takePresentation());++tick;
  if(tick==500){const bool committed=grenade?app.session().count(29)==4:app.session().magazine(27)==6;if(!committed){throw std::runtime_error("Auxiliary input did not commit exactly once");}std::cout<<"Visual fixture action committed once\n"<<std::flush;}
  auto view=makeSceneView(window.getSize(),{320,240});auto center=app.cameraCenter();view.setCenter({float(center.x)+media.shakeOffset(),float(center.y)});window.setView(view);window.clear();renderer->draw(window,app.world(),media.characterFlashes(),app.playerVisible(),media.saturation());media.draw(window,app);window.display();
 }
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
