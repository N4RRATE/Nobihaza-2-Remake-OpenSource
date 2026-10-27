#include "combat_presentation.h"
#include "scene_renderer.h"
#include "simulation_clock.h"
#include "combat_input.h"
#include <iostream>
#include <chrono>
#include <cstdlib>
int runCombatGame(CombatContent content,const std::string& entry,int frameLimit,const std::filesystem::path& saveDirectory){
    const auto started=std::chrono::steady_clock::now();
    const auto profile=[&](const char* phase){if(std::getenv("NOBIHAZA_STARTUP_PROFILE")){std::cerr<<"startup."<<phase<<"_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()<<'\n';}};
    auto spriteTextures=std::make_shared<SceneSpriteTextures>();
    std::unique_ptr<SceneRenderer> preparedRenderer;
    GameApplication scene(std::move(content),entry,{},[&](const SceneDefinition& definition){auto candidate=std::make_unique<SceneRenderer>(definition,spriteTextures);preparedRenderer=std::move(candidate);});
    profile("application_ready");
    if(!saveDirectory.empty()){scene.configureSaveDirectory(saveDirectory);}
    sf::Font font;if(!font.openFromFile(scene.content().maps.begin()->second.fontPath)){throw std::runtime_error("Cannot load combat font: "+scene.content().maps.begin()->second.fontPath.string());}
    sf::RenderWindow window(sf::VideoMode({960,720}),"Nobihaza 2 Remake - "+entry);window.setFramerateLimit(60);window.setKeyRepeatEnabled(false);
    auto renderer=std::move(preparedRenderer);std::uint64_t generation=scene.generation();
    auto presentation=std::make_unique<CombatPresentation>(scene.content(),font);std::unique_ptr<CombatPresentation> preparedPresentation;
    auto dialogue=std::make_unique<DialogueRenderer>(font);dialogue->setMedia(presentation->images(),presentation->palette(),&presentation->systemImage());
    std::unique_ptr<DialogueRenderer> preparedDialogue;
    scene.configureRestorePresentation([&](const SceneDefinition& definition,const SavedPresentation& state){
        auto newRenderer=std::make_unique<SceneRenderer>(definition,spriteTextures);auto newPresentation=std::make_unique<CombatPresentation>(scene.content(),font);newPresentation->prepareRestoredState(state);
        auto newDialogue=std::make_unique<DialogueRenderer>(font);newDialogue->setMedia(newPresentation->images(),newPresentation->palette(),&newPresentation->systemImage());
        // Prepare every borrowed resource before committing any candidate.
        preparedRenderer=std::move(newRenderer);preparedPresentation=std::move(newPresentation);preparedDialogue=std::move(newDialogue);
    });
    auto adopt=[&]{if(generation!=scene.generation()){
        if(scene.hasGame()){renderer=std::move(preparedRenderer);if(preparedPresentation){presentation=std::move(preparedPresentation);dialogue=std::move(preparedDialogue);presentation->activateRestoredState();}}
        else{renderer.reset();preparedRenderer.reset();preparedPresentation.reset();preparedDialogue.reset();}
        generation=scene.generation();
    }};
    CombatInput input;input.setFocused(window.hasFocus());SimulationClock simulation;sf::Clock clock;int frames=0;bool firstFrame=true;
    presentation->update(scene.takePresentation());scene.setPresentationState(presentation->saveState());
    profile("media_ready");
    while(window.isOpen()){
        while(const auto event=window.pollEvent()){if(event->is<sf::Event::Closed>()){window.close();}else{input.handle(*event);}}
        if(!window.isOpen()){break;}const auto advance=simulation.advance(clock.restart().asSeconds());
        if(advance.droppedSeconds>0){
            std::cerr<<"[simulation] discarded "<<advance.droppedSeconds<<" seconds after a long stall\n";
        }
        for(int i=0;i<advance.steps;++i){if(const auto ticks=presentation->musicTicks()){scene.reportMusicTicks(ticks->first,ticks->second);}if(const auto firstPass=presentation->musicFirstPass()){scene.reportMusicFirstPass(*firstPass);}const bool directBefore=scene.directSaving(),playingBefore=scene.mode()==ApplicationMode::Playing;const auto generationBefore=scene.generation();scene.update(input.consume());const bool loaded=!playingBefore&&scene.generation()!=generationBefore;adopt();
            try{presentation->update(scene.takePresentation(),!loaded&&!directBefore&&(playingBefore||!scene.directSaving()));}catch(const std::exception& e){
                if(!scene.openingActive()){throw;}scene.failOpening(e.what());adopt();presentation->update(scene.takePresentation());
            }
            scene.setPresentationState(presentation->saveState());}
        if(scene.quitRequested()){window.close();break;}
        const auto size=window.getSize();if(size.x==0||size.y==0){continue;}auto view=makeSceneView(size,{320,240});const auto center=scene.cameraCenter();view.setCenter({static_cast<float>(center.x)+presentation->shakeOffset(),static_cast<float>(center.y)});
        window.setView(view);window.clear(sf::Color::Black);
        if(scene.hasGame()){presentation->drawBackground(window,scene);renderer->draw(window,scene.world(),presentation->characterFlashes(),scene.playerVisible(),presentation->saturation());}
        presentation->draw(window,scene);dialogue->draw(window,scene.dialogue().view());dialogue->draw(window,scene.inputPromptView());window.display();
        if(firstFrame){profile("first_frame");firstFrame=false;}
        if(frameLimit>0&&++frames>=frameLimit){window.close();}
    }return 0;
}
