#include "title_navigation.h"
#include "combat_presentation.h"
#include "scene_renderer.h"
#include "opening_driver.h"
#include <fstream>
#include <iostream>
int main(int argc,char** argv){try{
 if(argc<3){return 2;}auto content=loadCombatContent(argv[1]);bool skip=std::string(argv[2])=="skip";const std::string mode=argv[2];const bool visual=mode=="visual"||mode.starts_with("inspect-");
 if(std::string(argv[2])=="broken"){
  const std::string resource=content.cue("m2-e1-p1-c145").at("sound").at("resource");content.media["audio"][resource]["file"]="broken.mp3";
 }
 sf::Font font;if(!font.openFromFile(content.maps.begin()->second.fontPath)){return 3;}
 sf::RenderWindow window(sf::VideoMode({960,720}),"43 opening window - "+std::string(argv[2])+(visual?" - transport/escort 1x":" - accelerated 4x"));window.setFramerateLimit(60);
 std::unique_ptr<SceneRenderer> stagedRenderer,renderer;
 GameApplication app(content,"title",{},[&](const SceneDefinition& d){stagedRenderer=std::make_unique<SceneRenderer>(d);});
 auto presentation=std::make_unique<CombatPresentation>(app.content(),font);std::unique_ptr<CombatPresentation> stagedPresentation;
 app.configureRestorePresentation([&](const SceneDefinition& d,const SavedPresentation& s){auto r=std::make_unique<SceneRenderer>(d);auto p=std::make_unique<CombatPresentation>(app.content(),font);p->prepareRestoredState(s);stagedRenderer=std::move(r);stagedPresentation=std::move(p);});
 DialogueRenderer dialogue(font);dialogue.setMedia(presentation->images(),presentation->palette());std::uint64_t generation=0;
 auto adopt=[&]{if(generation!=app.generation()){if(app.hasGame()){renderer=std::move(stagedRenderer);if(stagedPresentation){presentation=std::move(stagedPresentation);dialogue.setMedia(presentation->images(),presentation->palette());}}else{renderer.reset();stagedRenderer.reset();stagedPresentation.reset();}generation=app.generation();}};
 finishTitleOpening(app);if(app.menuSelection()==1){app.update(openingKey(InputAction::Up));}app.update(openingKey(InputAction::Confirm));int tick=0,frames=0;std::string previous;bool finished=false;
 if(mode.starts_with("inspect-")){
  const std::string goal=mode=="inspect-escort"?"escort":"transport";
  while(app.openingStage()!=goal&&tick<80000){app.update(openingInput(app,false,tick++));adopt();presentation->update(app.takePresentation());}
  if(app.openingStage()!=goal){throw std::runtime_error("Cannot reach inspected opening stage");}
 }

 while(window.isOpen()&&frames++<18000){while(auto e=window.pollEvent()){if(e->is<sf::Event::Closed>()){window.close();}}if(!window.isOpen()){break;}
  for(int n=0;n<(visual&&app.mapId()!=2?1:4);++n){if(app.openingActive()&&!(mode=="inspect-escort"&&app.openingStage()=="monologue-explosion")){app.update(openingInput(app,skip,tick++));}adopt();
   try{presentation->update(app.takePresentation());}catch(const std::exception& e){if(!app.openingActive()){throw;}app.failOpening(e.what());adopt();presentation->update(app.takePresentation());std::cerr<<"presentation failure returned to empty title: "<<e.what()<<"\n";}
   if(app.openingStage()!=previous){previous=app.openingStage();std::cerr<<"stage "<<previous<<"\n";}
   if(!app.openingActive()&&!finished){finished=true;std::cerr<<"finished mode "<<int(app.mode())<<" map "<<app.mapId()<<"\n";}
  }
  auto view=makeSceneView(window.getSize(),{320,240});auto center=app.cameraCenter();view.setCenter({float(center.x)+presentation->shakeOffset(),float(center.y)});window.setView(view);window.clear();
  if(app.hasGame()){renderer->draw(window,app.world(),presentation->characterFlashes(),app.playerVisible(),presentation->saturation());}presentation->draw(window,app);dialogue.draw(window,app.dialogue().view());dialogue.draw(window,app.inputPromptView());window.display();
 }
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
