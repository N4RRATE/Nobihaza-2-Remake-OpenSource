#include "combat_presentation.h"
#include "title_navigation.h"
#include "scene_renderer.h"
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <stdexcept>
InputFrame press(InputAction a){InputFrame f;f.buttons[static_cast<int>(a)]={true,true,false};return f;}
int main(int argc,char** argv){
 if(argc!=3){return 2;}auto content=loadCombatContent(argv[1]);const auto output=std::filesystem::path(argv[2]);std::filesystem::create_directories(output);
 sf::Font font;if(!font.openFromFile(content.maps.begin()->second.fontPath)){throw std::runtime_error("font");}
 sf::RenderTexture target({960,720});
 auto run=[&](CombatContent& data,const std::string& entry,const std::string& prefix,bool die){
  GameApplication scene(data,entry,[](int lo,int){return lo;});CombatPresentation presentation(data,font);DialogueRenderer dialogue(font);dialogue.setMedia(presentation.images(),presentation.palette());
  auto renderer=std::make_unique<SceneRenderer>(scene.definition());auto generation=scene.generation();
  auto tick=[&](InputFrame frame){scene.update(frame);presentation.update(scene.takePresentation());if(generation!=scene.generation()){renderer=scene.hasGame()?std::make_unique<SceneRenderer>(scene.definition()):nullptr;generation=scene.generation();}};
  auto capture=[&](const std::string& name){target.setView(makeSceneView({960,720},{320,240}));target.clear(sf::Color::Black);if(scene.hasGame()){renderer->draw(target,scene.world(),presentation.characterFlashes());}presentation.draw(target,scene);dialogue.draw(target,scene.dialogue().view());target.display();if(!target.getTexture().copyToImage().saveToFile(output/(prefix+name+".png"))){throw std::runtime_error("capture");}};
  tick({});capture("entry");
  if(die){bool wave=false;for(int i=0;i<6000&&scene.mode()!=ApplicationMode::Title;++i){tick({});if(scene.foreground()==ForegroundKind::Death&&i%60==0&&!wave){capture("death");wave=true;}}if(scene.mode()!=ApplicationMode::Title){throw std::runtime_error("no title");}finishTitleOpening(scene);capture("title");while(scene.menuSelection()!=2){scene.update(press(InputAction::Down));}scene.update(press(InputAction::Confirm));if(!scene.quitRequested()){throw std::runtime_error("title exit intent");}
   scene.restartDevelopmentEntry("post-pickup");presentation.update(scene.takePresentation());
   const auto render=[&](const GameApplication& app,CombatPresentation& view){
    SceneRenderer layer(app.definition());target.setView(makeSceneView({960,720},{320,240}));target.clear(sf::Color::Black);
    layer.draw(target,app.world(),view.characterFlashes());view.draw(target,app);target.display();return target.getTexture().copyToImage();
   };
   const auto restarted=render(scene,presentation);
   GameApplication clean(data,"post-pickup",[](int lo,int){return lo;});CombatPresentation cleanView(data,font);cleanView.update(clean.takePresentation());
   const auto expected=render(clean,cleanView);
   if(!std::equal(restarted.getPixelsPtr(),restarted.getPixelsPtr()+960*720*4,expected.getPixelsPtr())){throw std::runtime_error("old death presentation leaked into replacement game");}
   return;}
  tick(press(InputAction::Cancel));capture("menu");tick(press(InputAction::Down));capture("description");tick(press(InputAction::Confirm));capture("equipped");tick(press(InputAction::Cancel));
  InputFrame shift;shift.buttons[static_cast<int>(InputAction::Weapon)].held=true;for(int i=0;i<90;++i){tick(shift);}capture("aim");
  InputFrame fire=shift;fire.buttons[static_cast<int>(InputAction::Attack)]={true,true,false};tick(fire);capture("muzzle");
 };
 auto frozen=content;frozen.entries.at("post-pickup").session.flags[52]=false;run(frozen,"post-pickup","weapon-",false);
 auto lethal=content;lethal.entries.at("post-pickup").session.hp=1;run(lethal,"post-pickup","failure-",true);
 std::cout<<"presentation frames rendered; playback is not listening/feel acceptance\n";
}
