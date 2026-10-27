#include "combat_input.h"
#include "dialogue_renderer.h"
#include "combat_scene.h"
#include "scene_renderer.h"
#include <SFML/Graphics.hpp>
#include <iostream>
// Explicit visual fixture: input presentation and its real scene boundary, no natural story grants.
int main(int argc,char** argv){
    if(argc!=2){return 2;}const auto content=loadCombatContent(argv[1]);
    GameSession session(content.items,content.entries.at("natural").session);
    CombatScene scene(content,session,"natural");auto request=scene.requestInput(NumberPrompt{4,0});
    sf::Font font;if(!font.openFromFile(scene.definition().fontPath)){return 3;}
    sf::RenderWindow window(sf::VideoMode({960,720}),"Nobihaza - 32 explicit prompt fixture");window.setFramerateLimit(60);window.setKeyRepeatEnabled(false);
    SceneRenderer renderer(scene.definition());DialogueRenderer dialogue(font);CombatInput input;input.setFocused(window.hasFocus());
    bool choice=false;int frames=0;
    while(window.isOpen()&&++frames<7200){
        while(const auto event=window.pollEvent()){
            if(event->is<sf::Event::Closed>()){window.close();}else{input.handle(*event);}
        }
        if(!window.isOpen()){break;}scene.update(input.consume());
        if(auto result=scene.takeInputResult(request)){
            if(!choice){
                std::cout<<"number="<<std::get<NumberSubmitted>(result->result).value<<std::endl;
                const auto label=[](const std::string& text){return MessageLine{text,{{MessageTokenKind::Text,decodeMessageText(text),0}}};};
                request=scene.requestInput(ChoicePrompt{{label("保留"),label("丢弃")},2});choice=true;
            }else{std::cout<<"choice="<<std::get<ChoiceSelected>(result->result).index<<std::endl;window.close();continue;}
        }
        window.setView(makeSceneView(window.getSize(),{320,240}));window.clear(sf::Color::Black);
        renderer.draw(window,scene.world());dialogue.draw(window,scene.inputPromptView());window.display();
    }
}
