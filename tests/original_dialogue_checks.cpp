#include "local_text_fixtures.h"
#include "combat_content.h"
#include "content_hash.h"
#include "dialogue_renderer.h"
#include "input_prompt.h"
#include "scene_renderer.h"
#include <SFML/Graphics.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void need(bool value,const char* message){if(!value){throw std::runtime_error(message);}}
void reveal(Dialogue& dialogue){for(int i=0;i<1024&&dialogue.view().phase!=DialoguePhase::AwaitingInput;++i){dialogue.update(false,false);}need(dialogue.view().phase==DialoguePhase::AwaitingInput,"Dialogue reveal");}
InputFrame press(InputAction action){InputFrame input;input.buttons[static_cast<std::size_t>(action)].pressed=true;return input;}
bool same(const sf::Image& a,const sf::Image& b){return a.getSize()==b.getSize()&&std::equal(a.getPixelsPtr(),a.getPixelsPtr()+a.getSize().x*a.getSize().y*4,b.getPixelsPtr());}
}
int main(int argc,char** argv){try{
    need(argc==3||argc==4,"original_dialogue_checks source-sample.json isolated-output [--native]");
    const std::filesystem::path output=argv[2];std::filesystem::create_directories(output);
    nlohmann::json sample;{std::ifstream input(argv[1]);input>>sample;}
    const std::filesystem::path package=sample.at("package");
    for(const auto& [file,sha]:sample.at("files").items()){
        std::ifstream input(package/file,std::ios::binary);need(bool(input)&&contentSha256(readContentBytes(input))==sha.get<std::string>(),"Changed source sample");
    }
    sf::Font font;need(font.openFromFile(package/sample.at("font").get<std::string>()),"Actual font missing");
    std::map<std::string,sf::Texture> images;
    for(const auto& [id,image]:sample.at("images").items()){need(images[id].loadFromFile(package/image.at("file").get<std::string>()),"Actual image missing");}
    std::string identity="menu-system";auto palette=images.at(identity).copyToImage();
    DialogueRenderer renderer(font);renderer.setMedia(images,palette,&identity);
    const auto source=palette;const sf::Color background(18,39,61),key(255,156,0);
    auto definition=readCombatMessage(sample.at("messages").at("m61-e2-p2-c36")).message;
    Dialogue dialogue;dialogue.request(definition);reveal(dialogue);
    // CN1.00 M61 E2 P2 C36: actual four-line text/face/controls, not a skin-only scene.
    std::vector<std::pair<std::string,DialogueView>> cases;
    for(int position=0;position<3;++position){auto view=dialogue.view();view.display.position=position;view.display.transparent=false;view.animationFrame=0;cases.emplace_back("dialogue-"+std::to_string(position),view);}
    auto right=cases.back().second;right.faceRight=true;right.faceFlipped=true;cases.emplace_back("right-flipped",right);
    auto transparent=right;transparent.display.transparent=true;cases.emplace_back("transparent",transparent);
    ChoicePrompt question;question.question=dialogue.view();question.display={false,2,true,false};
    // Two source question lines plus two options exercise the real four-line composition.
    question.question->lines.resize(2);question.question->colors.resize(2);if(!question.question->exfontIndices.empty()){question.question->exfontIndices.resize(2);}
    question.options={{{},{{MessageTokenKind::Text,decodeMessageText(localSourceText("original_dialogue_checks.0")),0}}},{{},{{MessageTokenKind::Text,decodeMessageText(localSourceText("original_dialogue_checks.1")),0}}}};question.cancelType=5;
    InputPrompt choice;auto choiceId=choice.request(question,1);need(!choice.view().showPause,"Question pause leaked into a choice");
    cases.emplace_back("choice",choice.view());
    need(choice.update(press(InputAction::Down))==PromptFeedback::Cursor,"Choice navigation changed");
    need(choice.view().cursor->line==3&&choice.view().cursor->firstChoiceLine==2,"Choice row/indent changed");
    need(choice.update(press(InputAction::Cancel))==PromptFeedback::Cancel,"Choice cancellation changed");
    need(std::holds_alternative<ChoiceCancelled>(choice.takeCompletion(choiceId)->result),"Choice result changed");
    InputPrompt number;auto numberId=number.request(NumberPrompt{4,389},2);cases.emplace_back("number-0389",number.view());
    MessageDefinition opening;opening.faceImage="face-actor1";opening.faceIndex=1;
    opening.lines={{{},{{MessageTokenKind::InstantLine,{},0},{MessageTokenKind::Color,{},4},{MessageTokenKind::Text,decodeMessageText(localSourceText("original_dialogue_checks.2")),0},{MessageTokenKind::Color,{},0}}},
                   {{},{{MessageTokenKind::InstantOff,{},0},{MessageTokenKind::Text,decodeMessageText(localSourceText("original_dialogue_checks.3")),0}}}};
    Dialogue videoState;videoState.request(opening);reveal(videoState);cases.emplace_back("video-245s-m4-e2-p3-c17",videoState.view());
    need(number.update(press(InputAction::Up))==PromptFeedback::Cursor&&number.view().lines.front()==decodeMessageText(localSourceText("original_dialogue_checks.4")),"Numeric rollover changed");
    number.update(press(InputAction::Left));need(number.view().cursor->column==2,"Numeric place changed");
    need(number.update(press(InputAction::Confirm))==PromptFeedback::Decision&&std::get<NumberSubmitted>(number.takeCompletion(numberId)->result).value==380,"Numeric submission changed");
    const sf::Vector2u sizes[]={{960,720},{1000,600},{400,360}};
    for(const auto size:sizes){
        sf::RenderTexture surface(size);auto world=makeSceneView(size,{320,240});world.setCenter({620,520});surface.setView(world);
        const auto viewport=surface.getViewport(world);const float scale=viewport.size.y/240.f;
        const auto pixel=[&](const sf::Image& image,float x,float y){return image.getPixel({unsigned(viewport.position.x+(x+.5f)*scale),unsigned(viewport.position.y+(y+.5f)*scale)});};
        for(const auto& [name,view]:cases){
            surface.clear(background);renderer.draw(surface,view);need(surface.getView().getCenter()==world.getCenter(),"Dialogue moved camera");surface.display();auto actual=surface.getTexture().copyToImage();
            need(actual.saveToFile(output/(name+"-"+std::to_string(size.x)+".png")),"Image export");
            const float top=80.f*view.display.position;
            if(!view.display.transparent){
                // Corners and repeated top edge come directly from fixed Player window.cpp L231-240/L221-223.
                for(unsigned x=0;x<320;++x){const unsigned sx=x<8?32+x:x>=312?56+x-312:40+(x-8)%16;const auto expected=source.getPixel({sx,0});
                    if(expected!=key){need(pixel(actual,float(x),top)==expected,"Original frame corner/tile mismatch");}}
                need(pixel(actual,200,top+60)==source.getPixel({20,24}),"Original opaque background was not stretched");
            }else{need(pixel(actual,2,top+2)==background,"Transparent window retained a frame/background");}
            if(!view.faceImage.empty()){
                const auto face=images.at(view.faceImage).copyToImage();unsigned checked=0;
                for(unsigned y=0;y<48;++y){for(unsigned x=0;x<48;++x){const auto expected=face.getPixel({unsigned(view.faceIndex%4*48)+(view.faceFlipped?47-x:x),unsigned(view.faceIndex/4*48)+y});
                    if(expected.a==255){need(pixel(actual,(view.faceRight?256.f:16.f)+x,top+16+y)==expected,"Original face margin/flip mismatch");++checked;}}}need(checked>100,"Empty face oracle");
            }
        }
        auto blank=cases[2].second;blank.lines.clear();blank.colors.clear();blank.exfontIndices.clear();blank.faceImage.clear();blank.showPause=false;
        surface.clear(background);renderer.draw(surface,blank);surface.display();const auto initial=surface.getTexture().copyToImage();
        identity="title-system";palette=images.at(identity).copyToImage();surface.clear(background);renderer.draw(surface,blank);surface.display();need(!same(initial,surface.getTexture().copyToImage()),"Live system identity switch retained old mask");
        identity="menu-system";palette=images.at(identity).copyToImage();surface.clear(background);renderer.draw(surface,blank);surface.display();need(same(initial,surface.getTexture().copyToImage()),"System restoration retained changed skin");
        // The arrow is masked with original palette index zero, never a solid orange block.
        auto pause=blank;pause.showPause=true;pause.animationFrame=0;surface.clear(background);renderer.draw(surface,pause);surface.display();const auto shown=surface.getTexture().copyToImage();
        pause.animationFrame=20;surface.clear(background);renderer.draw(surface,pause);surface.display();need(same(initial,surface.getTexture().copyToImage())&&!same(initial,shown),"Continue arrow does not blink/hide independently");
        auto cursor=blank;cursor.cursor=DialogueCursor{0,0,1};cursor.animationFrame=0;surface.clear(background);renderer.draw(surface,cursor);surface.display();const auto cursor1=surface.getTexture().copyToImage();
        cursor.animationFrame=11;surface.clear(background);renderer.draw(surface,cursor);surface.display();need(!same(cursor1,surface.getTexture().copyToImage()),"Original two cursor phases do not differ");
        // Source's Choice rect is 300 pixels wide even with a one-character label.
        bool edge=false;for(unsigned y=0;y<8;++y){auto expected=source.getPixel({95,y});if(expected!=key){need(pixel(cursor1,309,168+y)==expected,"Choice frame shrank to label width");edge=true;}}need(edge,"Empty cursor edge oracle");
        auto numeric=blank;numeric.cursor=DialogueCursor{0,3,1,true};surface.clear(background);renderer.draw(surface,numeric);surface.display();const auto digit=surface.getTexture().copyToImage();
        for(unsigned y=0;y<8;++y){const auto expected=source.getPixel({95,y});if(expected!=key){need(pixel(digit,65,168+y)==expected,"Numeric cursor does not retain14px width/12px spacing");}}
        need(pixel(digit,66,168)==pixel(initial,66,168),"Numeric cursor reached the next digit");
        auto invalid=images;need(invalid["bad"].loadFromImage(sf::Image({12,12},sf::Color::White)),"Bad fixture");std::string bad="bad";bool rejected=false;
        try{renderer.setMedia(invalid,palette,&bad);}catch(const std::runtime_error&){rejected=true;}need(rejected,"Invalid System accepted");
        surface.clear(background);renderer.draw(surface,blank);surface.display();need(same(initial,surface.getTexture().copyToImage()),"Failed binding destroyed old presentation");
    }
    // Renderer calls are repeatable and do not advance input or control-code timing.
    MessageDefinition paused;paused.lines={{{},{{MessageTokenKind::Text,decodeMessageText(localSourceText("original_dialogue_checks.5")),0},{MessageTokenKind::WaitInput,{},0},{MessageTokenKind::Text,decodeMessageText(localSourceText("original_dialogue_checks.6")),0}}}};
    Dialogue control;control.request(paused);for(int i=0;i<40&&!control.view().showPause;++i){control.update(false,false);}need(control.view().phase==DialoguePhase::Typing&&control.view().showPause,"\\! pause missing");
    control.update(true,false);need(!control.view().showPause,"\\! confirmation did not release indicator");reveal(control);need(control.view().lines.front()==decodeMessageText(localSourceText("original_dialogue_checks.7")),"Control text changed");
    control.update(true,false);for(int i=0;i<6;++i){control.update(false,false);}need(control.view().phase==DialoguePhase::Cooldown,"Closing timing changed");control.update(false,false);need(control.takeCompletion().has_value(),"Completion timing changed");
    // A second consumer size and the supported tiled background mode are atlas rules, not dialogue assumptions.
    OriginalWindowSkin skin(source,key);sf::RenderTexture other({320,240});other.clear(background);
    skin.drawWindow(other,{{23,33},{111,39}},1,false);other.display();const auto tiled=other.getTexture().copyToImage();
    need(tiled.getPixel({63,50})==source.getPixel({8,17}),"Tiled background stretched on another rectangle size");
    need(tiled.getPixel({23,33})==source.getPixel({32,0}),"Reusable window lost its source corner");
    need(tiled.getPixel({22,33})==background&&tiled.getPixel({134,33})==background,"Reusable window escaped its rectangle");
    if(argc==4){
        sf::RenderWindow window(sf::VideoMode(sizes[0]),"Nobi 05 isolated original dialogue");window.setFramerateLimit(30);
        for(unsigned stage=0;stage<3&&window.isOpen();++stage){window.setSize(sizes[stage]);
            for(int frame=0;frame<45&&window.isOpen();++frame){while(const auto event=window.pollEvent()){if(event->is<sf::Event::Closed>()){window.close();}}
                window.setView(makeSceneView(window.getSize(),{320,240}));window.clear(background);renderer.draw(window,cases[stage==1?5:stage==2?6:0].second);
                if(frame==30){sf::Texture capture;need(capture.resize(window.getSize()),"Native capture allocation");capture.update(window);need(capture.copyToImage().saveToFile(output/("native-"+std::to_string(stage)+".png")),"Native capture");}
                window.display();
            }
        }window.close();
    }
    std::cout<<"Original Dialogue/Choice/Number: source skin, face margins, transparent/three positions, live identity/rebind failure, pause/cursor phases, existing controls PASS\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
