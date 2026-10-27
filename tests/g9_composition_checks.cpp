#include "combat_presentation.h"
#include "content_hash.h"
#include "scene_renderer.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void need(bool ok,const std::string& reason){if(!ok){throw std::runtime_error(reason);}}
bool same(const sf::Image& a,const sf::Image& b){return a.getSize()==b.getSize()&&std::equal(a.getPixelsPtr(),a.getPixelsPtr()+a.getSize().x*a.getSize().y*4,b.getPixelsPtr());}
InputFrame press(InputAction action){InputFrame input;input.buttons[static_cast<int>(action)].pressed=true;return input;}
void reveal(Dialogue& d){for(int n=0;n<1000&&d.view().phase!=DialoguePhase::AwaitingInput;++n){d.update(false,false);}need(d.view().phase==DialoguePhase::AwaitingInput,"Composed message reveal did not finish");}
}
int main(int argc,char** argv){try{
    need(argc==3,"g9_composition_checks frozen-package isolated-output");std::cout.setf(std::ios::unitbuf);
    const std::filesystem::path package=argv[1],output=argv[2];std::filesystem::create_directories(output);
    {std::ifstream in(package,std::ios::binary);need(bool(in)&&contentSha256(readContentBytes(in))=="79564718bb09133bda39383f6d432ed5ed611963df3a6477da6c718e4271b7ca","Changed reviewed G8-04 media candidate");}
    auto content=loadCombatContent(package);const auto base=content.entries.at("post-pickup");
    for(const std::string difficulty:{"easy","normal","hard"}){
        auto e=base;e.map=16;e.playerTile={10,9};e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.difficulty=difficulty;
        e.session.flags[1]=true;e.session.flags[18]=true;e.session.flags[10]=false;e.session.flags[45]=false;
        content.entries["g9-ui-"+difficulty]=e;
    }
    auto shared=std::make_shared<SceneSpriteTextures>();
    GameApplication game(std::move(content),"g9-ui-easy");const auto& c=game.content();
    sf::Font font;need(font.openFromFile(game.definition().fontPath),"Missing composed font");CombatPresentation presentation(c,font);
    DialogueRenderer text(font);text.setMedia(presentation.images(),presentation.palette());SceneRenderer renderer(game.definition(),shared);
    const auto reset=[&]{presentation.update({{PresentationEvent::Kind::ResetGame},{PresentationEvent::Kind::SystemGraphic,c.media.at("ui").at("menu").at("system_image")}},false);};
    const auto step=[&](InputAction action){game.update(press(action));auto events=game.takePresentation();std::erase_if(events,[](const auto& e){return e.kind==PresentationEvent::Kind::Music||e.kind==PresentationEvent::Kind::Sound;});presentation.update(events,false);};
    nlohmann::json report={{"candidate",package.string()},{"kind","controlled presentation; no natural story arrival"},{"difficulties",nlohmann::json::array()},{"checks",nlohmann::json::array()}};
    for(const std::string difficulty:{"easy","normal","hard"}){
        game.restartDevelopmentEntry("g9-ui-"+difficulty);game.takePresentation();reset();
        need(game.session().difficulty()==difficulty,"UI fixture lost selected difficulty");
        step(InputAction::Cancel);need(game.mode()==ApplicationMode::Equipment&&game.formalMenu(),"Actual formal menu did not open");
        for(const auto size:{sf::Vector2u{960,720},sf::Vector2u{1000,600},sf::Vector2u{400,360}}){
            sf::RenderTexture target(size);auto view=makeSceneView(size,{320,240});view.setCenter({700,400});target.setView(view);target.clear(sf::Color::Black);presentation.draw(target,game);
            need(target.getView().getCenter()==view.getCenter(),"Formal menu leaked camera changes");target.display();
            const auto image=target.getTexture().copyToImage();const auto viewport=target.getViewport(view);int visible=0;
            for(unsigned y=0;y<size.y;++y){for(unsigned x=0;x<size.x;++x){if(image.getPixel({x,y})!=sf::Color::Black){++visible;need(int(x)>=viewport.position.x&&int(x)<viewport.position.x+viewport.size.x&&int(y)>=viewport.position.y&&int(y)<viewport.position.y+viewport.size.y,"Formal menu escaped letterbox");}}}
            need(visible>1000,"Composed Chinese formal menu is blank");need(image.saveToFile(output/(difficulty+"-menu-"+std::to_string(size.x)+"x"+std::to_string(size.y)+".png")),"Menu capture failed");
        }
        step(InputAction::Confirm);need(game.menuView().page==MenuPage::Items,"Actual item menu did not open");step(InputAction::Cancel);step(InputAction::Cancel);need(game.mode()==ApplicationMode::Playing,"Actual menu cancel did not restore playing");
        // Actual token definitions from the frozen composed candidate, including its blank source Terms.
        need(c.media.at("terms").at("140").at("display")=="","Blank source term changed");
        const auto variable=std::find_if(c.messages.begin(),c.messages.end(),[](const auto& row){for(const auto& line:row.second.message.lines){for(const auto& token:line.tokens){if(token.kind==MessageTokenKind::Variable&&token.value==389){return true;}}}return false;});
        need(variable!=c.messages.end(),"Actual G1 service message definition is missing");const auto& dynamic=*variable;
        for(int value:{3,2,1,0}){
            auto state=base.session;state.difficulty=difficulty;state.variables[389]=value;if(c.facts.variables.contains(389)){state.worldValues[c.facts.variables.at(389)]=value;}
            GameSession sample(c.items,state,c.facts);const auto message=resolveCombatMessage(dynamic.second,sample);bool found=false;
            for(const auto& line:message.lines){for(const auto& token:line.tokens){need(token.kind!=MessageTokenKind::Variable,"Unresolved service number reached renderer");found|=token.kind==MessageTokenKind::Text&&token.text==decodeMessageText(std::to_string(value));}}
            need(found,"Actual service variable rendering lost 3/2/1/0");
        }
        Dialogue reading;auto source=c.messages.at("f4ev-ce159-c30").message;reading.request(source);reveal(reading);need(reading.view().lines.size()==4,"Actual CE159 page lost source lines");
        const auto blank=std::find_if(c.messages.begin(),c.messages.end(),[](const auto& row){return row.second.message.lines.size()>1&&std::any_of(row.second.message.lines.begin(),row.second.message.lines.end(),[](const auto& line){return line.raw.empty();});});
        need(blank!=c.messages.end(),"Actual source blank-line example missing");Dialogue emptyLine;emptyLine.request(resolveCombatMessage(blank->second,game.session()));reveal(emptyLine);
        for(std::size_t line=0;line<blank->second.message.lines.size();++line){if(blank->second.message.lines[line].raw.empty()){need(emptyLine.view().lines.at(line).empty(),"Original blank source row collapsed or acquired text");}}
        Dialogue m61;m61.request(c.messages.at("m61-e2-p2-c36").message);reveal(m61);
        InputPrompt choice;ChoicePrompt options;options.question=m61.view();options.display=m61.view().display;options.options={{"继续",{{MessageTokenKind::Text,U"继续",0}}},{"返回",{{MessageTokenKind::Text,U"返回",0}}}};choice.request(options,game.generation());
        for(const auto size:{sf::Vector2u{960,720},sf::Vector2u{1000,600},sf::Vector2u{400,360}}){
            sf::RenderTexture target(size);auto view=makeSceneView(size,{320,240});const auto center=game.cameraCenter();view.setCenter({float(center.x),float(center.y)});target.setView(view);target.clear(sf::Color::Black);
            presentation.drawBackground(target,game);renderer.draw(target,game.world(),{},false);text.draw(target,size.x==400?reading.view():size.x==1000?emptyLine.view():choice.view());target.display();
            need(target.getTexture().copyToImage().saveToFile(output/(difficulty+"-text-"+std::to_string(size.x)+"x"+std::to_string(size.y)+".png")),"Composed text capture failed");
        }
        report["difficulties"].push_back({{"difficulty",difficulty},{"menu","actual root/items/cancel"},{"text","actual CE159, original ExFont M61+Choice, source blank Terms/rows, actual V389 3/2/1/0 resolution"},{"blank_source_message",blank->first}});
        std::cout<<"COMPOSED_UI "<<difficulty<<" menu/text/face/choice/blank/variable PASS\n";
    }
    sf::RenderTexture surface({320,240});const auto pictureFrame=[&](float cameraX){auto view=makeSceneView({320,240},{320,240});view.setCenter({cameraX,120});surface.setView(view);surface.clear(sf::Color::Black);presentation.draw(surface,game);surface.display();return surface.getTexture().copyToImage();};
    // The original long credits are a292x2880 picture, not newly typeset or wrapped text.
    const auto& credits=c.cue("m211-e1-p1-c10");const auto& creditsTexture=presentation.images().at(credits.at("image").get<std::string>());
    need(creditsTexture.getSize()==sf::Vector2u{292,2880},"Original long credits image dimensions changed");int creditsInk=0;
    const auto creditsSource=creditsTexture.copyToImage();
    for(int centerY:{1440,0,-1200}){
        reset();PresentationEvent event{PresentationEvent::Kind::Cue,"m211-e1-p1-c10"};event.resolvedPicture=ResolvedPicture{{160,double(centerY)},0,0};presentation.update({event},false);
        const auto actual=pictureFrame(160);const int top=centerY-1440;
        for(unsigned y=0;y<240;++y){for(unsigned x=14;x<306;++x){const int sourceY=int(y)-top;if(sourceY<0||sourceY>=2880){continue;}const auto expected=creditsSource.getPixel({x-14,unsigned(sourceY)});
            if(expected.a==255){need(actual.getPixel({x,y})==expected,"Long source credits cropped or anchored incorrectly");creditsInk+=expected!=sf::Color::Black;}
        }}
        need(actual.saveToFile(output/("credits-"+std::to_string(centerY)+".png")),"Long credits capture failed");
    }
    need(creditsInk>100,"Original long subtitle/credits lost its visible source pixels");
    // Independent translation oracle: actual map-fixed source picture shifts by exactly camera delta; screen picture stays put.
    for(const std::string id:{"g1-ce160-c13","m208-e1-p1-c2"}){
        reset();PresentationEvent event{PresentationEvent::Kind::Cue,id};event.pictureMapOrigin=GridPosition{0,0};presentation.update({event},false);for(int n=0;n<120;++n){presentation.update({});}
        const auto a=pictureFrame(160),b=pictureFrame(176);int lit=0;
        if(id=="g1-ce160-c13"){need(same(a,b),"Screen-fixed source picture moved with camera");}
        else{for(unsigned y=0;y<240;++y){for(unsigned x=16;x<320;++x){lit+=a.getPixel({x,y})!=sf::Color::Black;need(a.getPixel({x,y})==b.getPixel({x-16,y}),"Map-fixed source picture did not follow exact16px camera delta");}}need(lit>100,"Map-fixed source image invisible");}
        need(a.saveToFile(output/(id+"-anchor.png")),"Anchor capture failed");
        presentation.update({{PresentationEvent::Kind::SceneChanged}},false);const auto clear=pictureFrame(160);for(unsigned y=0;y<240;++y){for(unsigned x=0;x<320;++x){need(clear.getPixel({x,y})==sf::Color::Black,"Scene change retained old picture pixels");}}
    }
    reset();presentation.update({{PresentationEvent::Kind::Cue,"m177-e3-p1-c13"}},false);bool shaken=false;
    for(int n=0;n<18;++n){shaken|=presentation.shakeOffset()!=0;presentation.update({});}need(shaken&&presentation.shakeOffset()==0,"Actual M177 shake did not stop at source18ticks");
    const auto worldFrame=[&]{auto view=makeSceneView({320,240},{320,240});const auto center=game.cameraCenter();view.setCenter({float(center.x)+presentation.shakeOffset(),float(center.y)});surface.setView(view);surface.clear(sf::Color::Black);renderer.draw(surface,game.world(),{},false);presentation.draw(surface,game);surface.display();return surface.getTexture().copyToImage();};
    reset();const auto before=worldFrame();
    for(const auto pair:{std::pair{"m177-e3-p1-c14","m177-e3-p1-c16"},std::pair{"m177-e3-p1-c25","m177-e3-p1-c27"}}){
        for(const auto id:{pair.first,pair.second}){
            const auto& cue=c.cue(id);PresentationEvent event{PresentationEvent::Kind::Cue,id};event.transition=cue.at("effective_transition_id");event.duration=20;presentation.update({event},false);
            for(int n=0;n<20;++n){worldFrame();presentation.update({});}
        }
        need(same(before,worldFrame()),"Source scroll erase/show left residual pixels");
    }
    need(worldFrame().saveToFile(output/"M177-transition-restored.png"),"Transition evidence capture failed");
    report["checks"]={"screen/map source picture exact projection and scene cleanup","M177 source shake18ticks then0","M177 source scroll9/10 finite20tick erase/show restores exact pixels","controlled snapshots only; no natural story flags committed"};
    std::ofstream(output/"composition.json")<<report.dump(2)<<'\n';std::cout<<"G9 composed three-difficulty presentation PASS; offscreen/control fixtures, native/feel/Windows separate\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
