#include "combat_content.h"
#include "combat_presentation.h"
#include "content_hash.h"
#include "dialogue_renderer.h"
#include "input_prompt.h"
#include "scene_camera.h"
#include "scene_renderer.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <set>
#ifdef __APPLE__
#include <dlfcn.h>
#endif

namespace {
void need(bool ok,const std::string& message){if(!ok){throw std::runtime_error(message);}}
bool same(const sf::Image& a,const sf::Image& b){return a.getSize()==b.getSize()&&std::equal(a.getPixelsPtr(),a.getPixelsPtr()+a.getSize().x*a.getSize().y*4,b.getPixelsPtr());}
void reveal(Dialogue& d){for(int n=0;n<1000&&d.view().phase!=DialoguePhase::AwaitingInput;++n){d.update(false,false);}need(d.view().phase==DialoguePhase::AwaitingInput,"Native message reveal did not finish");}
double backingScale(const sf::Window& window){
#ifdef __APPLE__
    const auto selector=reinterpret_cast<void*(*)(const char*)>(dlsym(RTLD_DEFAULT,"sel_registerName"));
    const auto send=reinterpret_cast<void*(*)(void*,void*)>(dlsym(RTLD_DEFAULT,"objc_msgSend"));
    const auto number=reinterpret_cast<double(*)(void*,void*)>(dlsym(RTLD_DEFAULT,"objc_msgSend"));
    need(selector&&send&&number,"Cannot inspect this window's native backing scale");
    return number(send(window.getNativeHandle(),selector("screen")),selector("backingScaleFactor"));
#else
    (void)window;return 0;
#endif
}
void titleCheck(const nlohmann::json& sample,const std::filesystem::path& root,const sf::Font& font,const std::filesystem::path& output){
    CombatContent content;content.directory=root;content.media=sample.at("title_media");content.catalog["opening"]=nlohmann::json::object();
    // This five-image fixture intentionally contains no audio; navigation feedback is checked by title_load_checks.
    for(auto& sound:content.media["ui"]["title"]["sounds"]){sound["resource"]="";}
    GameApplication game(content,"title");game.configureSaveDirectory(output/"absent-slots");CombatPresentation presentation(game.content(),font);presentation.update(game.takePresentation(),false);
    const auto source=presentation.images().at("title").copyToImage(),skin=presentation.images().at("title-system").copyToImage();
    const auto key=[](InputAction action){InputFrame f;f.buttons[static_cast<int>(action)].pressed=true;return f;};
    const auto tick=[&](InputFrame input=InputFrame{}){game.update(input);presentation.update(game.takePresentation(),false);};
    nlohmann::json evidence=nlohmann::json::array(),costs=nlohmann::json::object();
    sf::RenderWindow window(sf::VideoMode({960,720}),"Nobi 09 isolated original title");window.setFramerateLimit(60);
    const auto capture=[&](const std::string& name){
        double first=0,repeated=0;for(int n=0;n<4;++n){while(window.pollEvent()){}window.setView(makeSceneView(window.getSize(),{320,240}));window.clear(sf::Color::Black);const auto started=std::chrono::steady_clock::now();presentation.draw(window,game);const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();if(n==0){first=ms;}else{repeated+=ms;}window.display();}
        costs[name]={{"first_draw_ms",first},{"subsequent_draw_mean_ms",repeated/3}};
        sf::Texture image;need(image.resize(window.getSize()),"Title native capture allocation");image.update(window);const auto pixels=image.copyToImage();
        need(pixels.saveToFile(output/(name+".png")),"Title native capture export");return pixels;
    };
    for(const auto size:{sf::Vector2u{960,720},sf::Vector2u{1000,600},sf::Vector2u{400,360}}){
        window.setSize(size);game.returnToTitle();presentation.update(game.takePresentation(),false);
        const auto view=makeSceneView(size,{320,240});const auto viewport=window.getViewport(view);const float scale=viewport.size.y/240.f;
        const auto point=[&](float x,float y){return sf::Vector2u{unsigned(viewport.position.x+x*scale),unsigned(viewport.position.y+y*scale)};};
        for(int frame=0;frame<=43;++frame){
            if(frame){tick();}if(frame!=0&&frame!=17&&frame!=35&&frame!=36&&frame!=42&&frame!=43){continue;}
            const auto image=capture("title-"+std::to_string(size.x)+"-open"+std::to_string(frame));
            // The title art is original320x240, centred/scaled only by the viewport.
            for(const sf::Vector2u p:std::initializer_list<sf::Vector2u>{{80,80},{150,60},{300,20},{270,224}}){need(frame<35||image.getPixel(point(p.x+.5f,p.y+.5f))==source.getPixel(p),"Title art changed outside source command window");}
            if(frame==0||frame==17){const auto actual=image.getPixel(point(150.5f,60.5f)),full=source.getPixel({150,60});
                need(std::abs(int(actual.r)-int(full.r)*frame/35)<=1&&std::abs(int(actual.g)-int(full.g)*frame/35)<=1&&std::abs(int(actual.b)-int(full.b)*frame/35)<=1,"Title did not fade source art from black over35updates");}
            if(frame<43){
                for(int y=156;y<204;++y){for(int x=123;x<197;++x){need(image.getPixel(point(x+.5f,y+.5f))==sf::Color::Black,"Title contents/cursor appeared before eight-update opening finished");}}
            }else{
                // Independent CN1.00 Terms: longest LOAD__GAME=11half cells =>82wide;
                // Scene_Title anchor212, border8, row16 =>text(127+leading spaces,156+row16).
                int firstInk=10000,lastInk=-1,rampPixels=0;std::set<unsigned> colors;
                for(int y=156;y<172;++y){for(int x=135;x<193;++x){const auto p=image.getPixel(point(x+.5f,y+.5f));if(p!=sf::Color::Black){firstInk=std::min(firstInk,x);lastInk=std::max(lastInk,x);}
                    const auto expected=skin.getPixel({8,unsigned(48+y-156)});if(p==expected&&p!=sf::Color::Black){++rampPixels;colors.insert(p.toInteger());}
                }}
                need(firstInk+.5f>=139-1/scale&&firstInk<145&&lastInk<187+1/scale,"Title lost source leading spaces or82px centred placement: "+std::to_string(firstInk)+".."+std::to_string(lastInk));need(rampPixels>5&&colors.size()>1,"Title omitted original System vertical colour ramp");
                int disabled=0;for(int y=172;y<188;++y){for(int x=133;x<193;++x){if(image.getPixel(point(x+.5f,y+.5f))==skin.getPixel({56,unsigned(48+y-172)})){++disabled;}}}need(disabled>5,"Empty-title Load does not use disabled palette3");
                bool cursorMatch=false;for(unsigned atlasX:{64u,96u}){bool match=true;int ink=0;for(unsigned y=0;y<8;++y){for(unsigned x=0;x<8;++x){const auto p=skin.getPixel({atlasX+x,y});if(p!=sf::Color(255,156,0)&&p!=sf::Color::Black){++ink;match&=image.getPixel(point(123+x+.5f,156+y+.5f))==p;}}}cursorMatch|=match&&ink>0;}need(cursorMatch,"Title cursor does not use source atlas at123,156");
                evidence.push_back({{"size",{size.x,size.y}},{"viewport",{viewport.position.x,viewport.position.y,viewport.size.x,viewport.size.y}},{"first_text_ink",firstInk},{"ramp_pixels",rampPixels},{"ramp_colors",colors.size()},{"native_backing_scale",backingScale(window)},{"source_geometry_oracle",true}});
            }
        }
    }
    std::filesystem::create_directories(output/"corrupt-slots");std::ofstream(output/"corrupt-slots/slot-01.json")<<"corrupt";game.configureSaveDirectory(output/"corrupt-slots");game.returnToTitle();presentation.update(game.takePresentation(),false);for(int n=0;n<43;++n){tick();}
    need(game.titleLoadEnabled()&&game.menuSelection()==1,"Present corrupt slot must select enabled Load");capture("title-with-corrupt-slot");tick(key(InputAction::Confirm));tick(key(InputAction::Confirm));need(game.fileMenuView()&&game.fileMenuView()->prompt.find("原文件")!=std::string::npos,"Bad-slot protected feedback absent");capture("corrupt-feedback");tick(key(InputAction::Cancel));tick(key(InputAction::Cancel));need(game.mode()==ApplicationMode::Title&&game.titleOpeningTicks()==8&&game.menuSelection()==1,"Load cancel reopened or changed title selection");capture("title-load-cancel");
    sf::RenderTexture target({960,720});target.setView(makeSceneView(target.getSize(),{320,240}));target.clear();presentation.draw(target,game);target.display();const auto started=std::chrono::steady_clock::now();for(int n=0;n<30;++n){target.clear();presentation.draw(target,game);target.display();}const double milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()/30;need(milliseconds<16.7,"Title UI rendering alone exceeds a60Hz frame");
    std::ofstream(output/"observations.json")<<nlohmann::json{{"title",evidence},{"draw_costs",costs},{"mean_draw_ms",milliseconds},{"windows",true},{"opening_updates",8}}.dump(2)<<'\n';window.close();
    std::cout<<"Original-source title geometry, untouched art, leading spaces, palette ramp, disabled/enabled Load, eight-update reveal and native3sizes PASS; mean draw "<<milliseconds<<"ms\n";
}

}
int main(int argc,char** argv){try{
    need(argc==3||argc==4,"g9_native_window_checks actual-small-source.json isolated-output [--system-restore]");
    const std::filesystem::path output=argv[2];std::filesystem::create_directories(output);
    nlohmann::json sample;{std::ifstream in(argv[1]);need(bool(in),"Missing native source sample");in>>sample;}
    const std::filesystem::path root=sample.at("package");
    for(const auto& [file,sha]:sample.at("files").items()){
        std::ifstream in(root/file,std::ios::binary);need(bool(in)&&contentSha256(readContentBytes(in))==sha.get<std::string>(),"Changed native source resource: "+file);
    }
    sf::Font font;need(font.openFromFile(root/sample.at("font").get<std::string>()),"Missing actual font");
    std::map<std::string,sf::Texture> images;
    for(const auto& [id,row]:sample.at("images").items()){need(images[id].loadFromFile(root/row.at("file").get<std::string>()),"Cannot upload native sample: "+id);}
    const std::string systemImage="menu-system";const auto palette=images.at(systemImage).copyToImage();DialogueRenderer text(font);text.setMedia(images,palette,&systemImage);
    CombatContent menuContent;menuContent.directory=root;menuContent.media=sample.at("title_media");
    GameApplication menuGame(std::move(menuContent),"title");CombatPresentation menu(menuGame.content(),font);for(int n=0;n<43;++n){menuGame.update({});}menu.update(menuGame.takePresentation(),false);
    if(argc==4&&std::string(argv[3])=="--title"){titleCheck(sample,root,font,output);return 0;}
    if(argc==4){
        // The same SavedPresentation/candidate/rebind path used by direct Load; no files or gameplay are edited.
        CombatContent content;content.directory=root;content.media=sample.at("title_media");content.catalog["warehouse_direct"]={{"map",204}};
        auto active=std::make_unique<CombatPresentation>(content,font);DialogueRenderer restored(font);
        restored.setMedia(active->images(),active->palette(),&active->systemImage());
        active->update({{PresentationEvent::Kind::SystemGraphic,"title-system"}},false);
        const auto saved=active->saveState();need(saved.warehouseVisual.at("system")=="title-system","Save omitted changed System identity");
        Dialogue dialogue;auto message=readCombatMessage(sample.at("messages").at("m61-e2-p2-c36")).message;message.display.transparent=false;dialogue.request(message);reveal(dialogue);
        sf::RenderTexture surface({960,720});surface.setView(makeSceneView(surface.getSize(),{320,240}));
        const auto frame=[&]{surface.clear(sf::Color(18,39,61));restored.draw(surface,dialogue.view());surface.display();return surface.getTexture().copyToImage();};
        const auto before=frame();auto invalid=saved;invalid.warehouseVisual["system"]="absent";bool rejected=false;
        try{CombatPresentation candidate(content,font);candidate.prepareRestoredState(invalid);}catch(const std::runtime_error&){rejected=true;}
        need(rejected&&same(before,frame()),"Failed restoration disturbed active System/renderer");
        const auto badFile=std::filesystem::absolute(output/"bad-system.png");need(sf::Image({12,12},sf::Color::White).saveToFile(badFile),"Bad mask fixture");
        auto badContent=content;badContent.media["images"]["bad"]={{"file",badFile.string()},{"size_px",{12,12}}};
        auto badMaskState=saved;badMaskState.warehouseVisual["system"]="bad";rejected=false;
        try{auto staged=std::make_unique<CombatPresentation>(badContent,font);staged->prepareRestoredState(badMaskState);auto stagedText=std::make_unique<DialogueRenderer>(font);stagedText->setMedia(staged->images(),staged->palette(),&staged->systemImage());}
        catch(const std::runtime_error&){rejected=true;}
        need(rejected&&same(before,frame()),"Mask preparation failure disturbed active Presentation");
        auto candidate=std::make_unique<CombatPresentation>(content,font);candidate->prepareRestoredState(saved);active=std::move(candidate);
        restored.setMedia(active->images(),active->palette(),&active->systemImage());
        need(active->systemImage()=="title-system"&&same(before,frame()),"Restored Presentation did not rebind current skin/palette");
        active->update({{PresentationEvent::Kind::SystemGraphic,"menu-system"}},false);need(!same(before,frame()),"Restored Presentation kept old borrowed identity");
        need(frame().saveToFile(output/"restored-system.png"),"System restore image export");
        std::cout<<"Actual SavedPresentation System switch, invalid candidate preservation and replacement rebind PASS\n";return 0;
    }
    auto m61=readCombatMessage(sample.at("messages").at("m61-e2-p2-c36")).message;
    auto reading=readCombatMessage(sample.at("messages").at("f4ev-ce159-c30")).message;
    // Explicit patterned map fixture exercises world projection independently of UI pixels.
    sf::Image background({512,320},sf::Color(22,45,62)),foreground({512,320},sf::Color::Transparent);
    for(unsigned y=0;y<320;++y){for(unsigned x=0;x<512;++x){background.setPixel({x,y},{std::uint8_t(20+(x/16)%4*25),std::uint8_t(35+(y/16)%4*25),60});}}
    need(background.saveToFile(output/"map.png")&&foreground.saveToFile(output/"upper.png"),"Cannot prepare small map fixture");
    SceneDefinition definition;definition.width=32;definition.height=20;definition.tileSize=16;definition.passageMasks.assign(32*20,15);
    definition.backgroundPath=output/"map.png";definition.foregroundPath=output/"upper.png";definition.tone={100,100,100};
    definition.player={10,8,"down","",1,4,256,32};definition.loopX=true;
    ExplorationScene world(definition);SceneRenderer map(definition);
    SceneCamera camera;camera.reset({15,9},32,20,true);const auto origin=camera.center();
    camera.pan(Direction::Right,3,6);for(int n=0;n<6;++n){camera.tick({15,9});}camera.restore(6);for(int n=0;n<20&&camera.moving();++n){camera.tick({15,9});}
    need(camera.center().x==origin.x&&camera.center().y==origin.y,"Native map camera restoration failed");
    struct Stage{sf::Vector2u size;const char* name;};
    const Stage stages[]={{{960,720},"baseline"},{{1000,600},"wide-choice"},{{400,360},"small-reading"},{{960,720},"restored"},{{960,720},"number"},{{1000,600},"title-menu"}};
    std::ofstream(output/"stage")<<0;std::ofstream(output/"events.jsonl",std::ios::trunc);
    sf::RenderWindow window(sf::VideoMode(stages[0].size),"Nobi G9-02 isolated presentation");window.setFramerateLimit(30);window.setKeyRepeatEnabled(false);
    sf::Image first;int current=-1;int stable{};Dialogue dialogue;InputPrompt prompt;nlohmann::json evidence=nlohmann::json::array();
    const auto begin=std::chrono::steady_clock::now();
    while(window.isOpen()){
        while(const auto event=window.pollEvent()){
            if(event->is<sf::Event::Closed>()){window.close();break;}
            if(const auto* resized=event->getIf<sf::Event::Resized>()){std::ofstream log(output/"events.jsonl",std::ios::app);log<<nlohmann::json{{"event","resized"},{"pixels",{resized->size.x,resized->size.y}}}.dump()<<'\n';}
        }
        if(!window.isOpen()){break;}
        int requested=current<0?0:current;{std::ifstream control(output/"stage");control>>requested;}
        if(requested==6){window.close();break;}need(requested>=0&&requested<6,"Invalid native check stage");
        if(requested!=current){
            prompt.invalidate(current+1);current=requested;stable=0;window.setSize(stages[current].size);dialogue.invalidate();
            if(current==2){dialogue.request(reading);reveal(dialogue);}
            else if(current==4){prompt.request(NumberPrompt{4,389},current+1);}
            else if(current!=5){
                auto message=m61;if(current==1){message.faceRight=true;message.faceFlipped=true;message.display.position=0;}
                dialogue.request(message);reveal(dialogue);
                if(current==1){ChoicePrompt choice;choice.question=dialogue.view();choice.display=dialogue.view().display;
                    choice.options={{"继续",{{MessageTokenKind::Text,U"继续",0}}},{"返回",{{MessageTokenKind::Text,U"返回",0}}}};prompt.request(choice,current+1);}
            }
        }
        const auto size=window.getSize();if(size.x==0||size.y==0){continue;}
        auto view=makeSceneView(size,{320,240});view.setCenter({float(origin.x)+512,float(origin.y)});window.setView(view);window.clear(sf::Color::Black);
        if(current==5){menu.draw(window,menuGame);}else{map.draw(window,world,{},false);text.draw(window,prompt.active()?prompt.view():dialogue.view());}
        need(window.getView().getCenter()==view.getCenter(),"Native UI changed world camera");
        if(++stable==4){
            need(size==stages[current].size,"Native resize did not reach requested client pixels");
            sf::Texture capture;need(capture.resize(size),"Native capture allocation");capture.update(window);const auto image=capture.copyToImage();
            const auto& message=prompt.active()?prompt.view():dialogue.view();const auto viewport=window.getViewport(view);const float scale=viewport.size.y/240.f;
            if(!message.faceImage.empty()){
                const auto face=images.at(message.faceImage).copyToImage();int opaque=0;
                for(unsigned y=0;y<48;++y){for(unsigned x=0;x<48;++x){const unsigned sourceX=(message.faceIndex%4)*48+(message.faceFlipped?47-x:x),sourceY=(message.faceIndex/4)*48+y;const auto pixel=face.getPixel({sourceX,sourceY});
                    if(pixel.a==255){++opaque;const unsigned px=viewport.position.x+unsigned(((message.faceRight?256:16)+x)*scale+scale/2),py=viewport.position.y+unsigned((80*message.display.position+16+y)*scale+scale/2);
                        need(image.getPixel({px,py})==pixel,"Native face did not keep its48x48 logical size/position after resize");}
                }}need(opaque>100,"Actual native face fixture has no independent opaque pixels");
            }
            if(current==2){need(!prompt.active()&&message.lines.size()==4,"Reading fixture retained an old choice prompt");}
            if(current==5){
                // Fixed Player Window_Command/Scene_Title: 11 half cells +16 border =82px,
                // centered at119 and ending at212; source cursor corner starts at123,156.
                // Read the original atlas directly; no production UI helper supplies this oracle.
                const auto skin=images.at("title-system").copyToImage();int ink=0;
                for(unsigned y=0;y<8;++y){for(unsigned x=0;x<8;++x){const auto expected=skin.getPixel({64+x,y});
                    if(expected!=sf::Color(255,156,0)&&expected!=sf::Color::Black){++ink;
                        const sf::Vector2u p{unsigned(viewport.position.x+(123+x+.5f)*scale),unsigned(viewport.position.y+(156+y+.5f)*scale)};
                        need(image.getPixel(p)==expected,"Native title cursor differs from actual System corner at source-derived position");}
                }}need(ink>0,"Title cursor source oracle is empty");
            }
            need(image.saveToFile(output/(std::string(stages[current].name)+".png")),"Cannot save native window framebuffer");
            if(current==0){first=image;}if(current==3){need(same(first,image),"Native resize/shrink/restore changed final pixels");}
            const double backing=backingScale(window);
            evidence.push_back({{"stage",current},{"name",stages[current].name},{"pixels",{size.x,size.y}},{"viewport",{viewport.position.x,viewport.position.y,viewport.size.x,viewport.size.y}},{"native_screen_backing_scale",backing},{"restored_pixel_identity",current==3},{"face_size_oracle",!message.faceImage.empty()},{"menu_pixel_oracle",current==5},{"real_window",true}});
            std::ofstream(output/"observations.json")<<evidence.dump(2)<<'\n';
            std::cout<<"NATIVE_STAGE_READY "<<current<<' '<<stages[current].name<<" pixels="<<size.x<<'x'<<size.y<<" backing="<<backing<<std::endl;
        }
        window.display();
        need(std::chrono::steady_clock::now()-begin<std::chrono::minutes(15),"Native check timed out");
    }
    need(evidence.size()==6,"Native inspection stopped before all stages");std::cout<<"Native resize/shrink/restore pixel identity, face/menu pixel oracles and actual backing-scale observation PASS; no cross-DPI/Windows claim\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
