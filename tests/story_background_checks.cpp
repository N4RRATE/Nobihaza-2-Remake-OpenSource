#include "combat_presentation.h"
#include "scene_renderer.h"
#include "full_content.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void need(bool ok,const char* message){if(!ok){throw std::runtime_error(message);}}
}
int mansionWindow(CombatContent content,const std::filesystem::path& package,const std::filesystem::path& output){
    auto entry=content.entries.at("post-pickup");entry.map=154;entry.playerTile={24,8};entry.direction=Direction::Right;
    entry.positions.clear();entry.pages.clear();entry.directions.clear();entry.through.clear();
    for(int id:{437,438}){entry.session.worldFlags[content.facts.switches.at(id)]=true;}
    for(int id:{439,440}){entry.session.worldFlags[content.facts.switches.at(id)]=false;}
    content.entries["mansion-window"]=entry;
    std::unique_ptr<SceneRenderer> renderer,prepared;auto shared=std::make_shared<SceneSpriteTextures>();
    GameApplication game(std::move(content),"mansion-window",{},[&](const SceneDefinition& d){prepared=std::make_unique<SceneRenderer>(d,shared);});
    renderer=std::move(prepared);auto generation=game.generation();
    sf::Font font;need(font.openFromFile(game.definition().fontPath),"Missing mansion font");
    CombatPresentation presentation(game.content(),font);sf::RenderTexture surface({320,240});
    nlohmann::json frames=nlohmann::json::array();bool window=false,pulled=false,briefed=false;int ticks=0;
    const auto frame=[&](bool background){
        const auto c=game.cameraCenter();surface.setView(sf::View({float(c.x),float(c.y)},{320,240}));surface.clear(sf::Color::Black);
        if(background){presentation.drawBackground(surface,game);}renderer->draw(surface,game.world(),{},game.playerVisible());surface.display();return surface.getTexture().copyToImage();
    };
    auto consume=[&]{if(generation!=game.generation()){renderer=std::move(prepared);generation=game.generation();}
        auto events=game.takePresentation();
        std::erase_if(events,[&](const auto& e){return e.kind==PresentationEvent::Kind::Music||e.kind==PresentationEvent::Kind::Sound||
            (e.kind==PresentationEvent::Kind::Cue&&game.content().cue(e.id).at("kind")=="sound");});
        presentation.update(events);
        if(game.mapId()==154){
            const auto& motion=game.world().player().motion;const auto tile=motion.tile();
            const bool stationary=!window&&tile==TilePosition{62,6}&&!motion.moving();
            const bool pulling=window&&!pulled&&tile.y==6&&tile.x>62;
            if(stationary||pulling){
                const auto missing=frame(false),restored=frame(true);int changed=0;
                for(unsigned y=0;y<240;++y){for(unsigned x=0;x<320;++x){changed+=missing.getPixel({x,y})!=restored.getPixel({x,y});}}
                const auto name=stationary?"stationary":"pulled";
                need(restored.saveToFile(output/(std::string(name)+"-restored.png")),"Cannot save escape frame");
                need(missing.saveToFile(output/(std::string(name)+"-missing.png")),"Cannot save missing escape frame");
                std::cout<<name<<" tick="<<ticks<<" pixels="<<changed<<" player="<<game.world().player().motion.tile().x<<" camera="<<game.cameraCenter().x<<'\n';
                need(changed>1000,"M154 stationary window is black: source panorama does not reach actual escape pixels");
                const auto& def=*game.definition().defaultPanorama;const auto source=presentation.images().at(def.image).copyToImage();
                need(!def.loopX&&!def.loopY&&!def.autoX&&!def.autoY&&source.getSize()==sf::Vector2u{320,270},"Unexpected source escape panorama geometry");
                for(unsigned y=0;y<240;++y){for(unsigned x=0;x<320;++x){if(missing.getPixel({x,y})!=restored.getPixel({x,y})){need(restored.getPixel({x,y})==source.getPixel({x,y}),"Window pixels differ from opaque original panorama");}}}
                presentation.draw(surface,game);surface.display();const auto complete=surface.getTexture().copyToImage();
                need(complete.saveToFile(output/(std::string(name)+"-complete.png")),"Cannot save complete escape presentation");
                int visible=0;for(unsigned y=0;y<96;++y){for(unsigned x=0;x<320;++x){visible+=complete.getPixel({x,y})!=sf::Color::Black;}}
                need(visible>1000,"Actual presentation hides the window behind a fade/layer");
                frames.push_back({{"stage",name},{"tick",ticks},{"pixels",changed},{"camera_x",game.cameraCenter().x},{"player_x",game.world().player().motion.tile().x}});
                if(stationary){window=true;}else{pulled=true;}
            }
        }
    };
    consume();
    for(;ticks<12000&&!briefed;++ticks){InputFrame input;if(ticks%3==0){input.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;}
        game.update(input);consume();briefed=game.mapId()==151&&game.session().flag(440)&&game.foreground()==ForegroundKind::None;}
    std::cout<<"final map="<<game.mapId()<<" foreground="<<int(game.foreground())<<" player="<<game.world().player().motion.tile().x<<","<<game.world().player().motion.tile().y<<" dialogue="<<int(game.dialogue().view().phase)<<'\n';
    need(window&&pulled&&briefed&&!game.session().flag(437),"Escape did not retain source pull/transfer/briefing continuation");
    need(!game.definition().defaultPanorama&&!game.panorama(),"M154 panorama leaked into monitor room");
    surface.clear(sf::Color::Black);presentation.drawBackground(surface,game);surface.display();const auto cleared=surface.getTexture().copyToImage();
    for(unsigned y=0;y<240;++y){for(unsigned x=0;x<320;++x){need(cleared.getPixel({x,y})==sf::Color::Black,"Retired escape background survives scene change");}}
    for(int map:{193,200,207,208,210}){need(game.content().maps.at(map).defaultPanorama.has_value(),"Escape fix lost a prior story background");}
    std::ifstream input(package);const auto definition=nlohmann::json::parse(input);
    for(const char* variant:{"projection","source"}){auto bad=definition;if(std::string(variant)=="projection"){bad["projection"]["default_panoramas"]["154"]["speed_x"]=1;}else{bad["source"]["sha256"]="forged";}
        bool rejected=false;try{loadCombatContentDefinition(package,bad);}catch(const std::exception&){rejected=true;}need(rejected,"Unreviewed escape visual revision accepted");}
    bool rejected=false;try{loadCombatContentDefinition(output/"absent"/"g8-late.json",definition);}catch(const std::exception&){rejected=true;}need(rejected,"Missing escape panorama accepted");
    std::ofstream(output/"frames.json")<<nlohmann::json{{"frames",frames},{"ticks",ticks},{"after_map",game.mapId()},{"briefed",briefed},{"no_background_leak",true}}.dump(2)<<'\n';
    std::cout<<"M154 actual source route/window pixels/pull/M151 full briefing/retirement/old backgrounds/rejection PASS\n";return 0;
}
int main(int argc,char** argv){try{
    std::cout.setf(std::ios::unitbuf);
    need(argc>=3&&argc<=4,"package output [profile|remaining|mansion]");
    auto content=loadCombatContent(argv[1]);
    const std::filesystem::path output=argv[2];std::filesystem::create_directories(output);
    if(argc>3&&std::string(argv[3])=="mansion"){return mansionWindow(std::move(content),argv[1],output);}
    if(argc>3&&std::string(argv[3])=="profile"){
        auto shared=std::make_shared<SceneSpriteTextures>();
        for(int map:{193,194,190,200,207,208}){
            const auto start=std::chrono::steady_clock::now();SceneRenderer renderer(content.maps.at(map),shared);
            const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            std::cout<<"prepare M"<<map<<" sprites="<<content.maps.at(map).sprites->size()<<" ms="<<ms<<std::endl;
        }
        return 0;
    }
    for(int map:{193,200,207,208,210}){need(content.maps.at(map).defaultPanorama.has_value(),"Source default panorama missing: ruins/cockpit/ship/truth rendered black");}
    need(!content.maps.at(190).defaultPanorama&&!content.maps.at(194).defaultPanorama,"Unchanged opaque maps gained backgrounds");
    const bool remaining=argc>3&&std::string(argv[3])=="remaining";
    auto shared=std::make_shared<SceneSpriteTextures>();
    if(!remaining){
    for(int map:{193,194,190,200,207,208}){
        const auto start=std::chrono::steady_clock::now();SceneRenderer renderer(content.maps.at(map),shared);
        std::cout<<"prepare M"<<map<<" sprites="<<content.maps.at(map).sprites->size()<<" ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<'\n';
    }
    {
        auto cache=std::make_shared<SceneSpriteTextures>();const auto& sprite=content.maps.at(193).sprites->begin()->second;
        const auto* first=&cache->load(sprite,{100,100,100});
        need(&cache->load(sprite,{100,100,100})==first,"Same path/tone must reuse immutable texture");
        sf::Image original;need(original.loadFromFile(sprite.texturePath),"Original cached sprite");const auto neutral=first->copyToImage();
        need(std::equal(original.getPixelsPtr(),original.getPixelsPtr()+sprite.textureWidth*sprite.textureHeight*4,neutral.getPixelsPtr()),"Neutral tone changed source pixels");
        const auto* dark=&cache->load(sprite,{50,50,50});need(dark!=first,"Tone variants share wrong pixels");
        auto bad=sprite;bad.texturePath="absent-story-sprite.png";bool rejected=false;
        try{cache->load(bad,{100,100,100});}catch(const std::exception&){rejected=true;}
        need(rejected&&(&cache->load(sprite,{100,100,100})==first),"Failed candidate changed cached good resource");
        bad=sprite;++bad.textureWidth;rejected=false;
        try{cache->load(bad,{100,100,100});}catch(const std::exception&){rejected=true;}
        need(rejected,"Cache bypassed expected dimensions");
    }
    }
    for(int map:{193,200,207,208,210,190}){
        auto e=content.entries.at("post-pickup");e.map=map;e.playerTile=map==193?TilePosition{90,7}:map==200?TilePosition{14,12}:map==190?TilePosition{14,25}:map==210?TilePosition{12,9}:TilePosition{18,7};
        e.positions.clear();e.pages.clear();e.directions.clear();e.through.clear();
        content.entries["background-"+std::to_string(map)]=e;
        if(map==207){e.session.worldFlags[content.facts.switches.at(580)]=true;e.session.worldFlags[content.facts.switches.at(582)]=false;content.entries["background-207-dynamic"]=e;}
    }
    std::unique_ptr<SceneRenderer> renderer;
    GameApplication game(std::move(content),remaining?"background-193":"background-200",{},[&](const SceneDefinition& d){renderer=std::make_unique<SceneRenderer>(d,shared);});
    sf::Font font;need(font.openFromFile(game.definition().fontPath),"Missing story font");CombatPresentation presentation(game.content(),font);sf::RenderTexture surface({320,240});
    for(int map:remaining?std::vector<int>{193,190}:std::vector<int>{200,207,208,210,193,190}){
        if(game.mapId()!=map){game.restartDevelopmentEntry("background-"+std::to_string(map));}
        auto events=game.takePresentation();std::erase_if(events,[](const auto& e){return e.kind==PresentationEvent::Kind::Music;});presentation.update(events,false);
        const auto center=game.cameraCenter();surface.setView(sf::View({float(center.x),float(center.y)},{320,240}));
        const auto frame=[&](bool background){surface.clear(sf::Color::Black);if(background){presentation.drawBackground(surface,game);}renderer->draw(surface,game.world(),{},game.playerVisible());surface.display();return surface.getTexture().copyToImage();};
        const auto absent=frame(false),restored=frame(true);int changed=0;
        for(unsigned y=0;y<240;++y){for(unsigned x=0;x<320;++x){changed+=absent.getPixel({x,y})!=restored.getPixel({x,y});}}
        need(map==190?changed==0:changed>500,"Default panorama fails to reach transparent world pixels");
        need(restored.saveToFile(output/("M"+std::to_string(map)+"-restored.png")),"Cannot save restored background");
        if(map!=190){need(absent.saveToFile(output/("M"+std::to_string(map)+"-missing.png")),"Cannot save missing background reference");}
        std::cout<<"M"<<map<<" background changed world pixels="<<changed<<'\n';
    }
    game.restartDevelopmentEntry("background-207-dynamic");
    for(int n=0;n<10&&!game.panorama();++n){game.update({});}
    need(game.panorama().has_value(),"Actual second cockpit page did not set dynamic space panorama");
    const auto center=game.cameraCenter();surface.setView(sf::View({float(center.x),float(center.y)},{320,240}));
    surface.clear();renderer->draw(surface,game.world(),{},game.playerVisible());surface.display();const auto blackWindows=surface.getTexture().copyToImage();
    surface.clear();presentation.drawBackground(surface,game);renderer->draw(surface,game.world(),{},game.playerVisible());surface.display();const auto space=surface.getTexture().copyToImage();int windowPixels=0;
    for(unsigned y=0;y<240;++y){for(unsigned x=0;x<320;++x){windowPixels+=blackWindows.getPixel({x,y})!=space.getPixel({x,y});}}
    need(windowPixels>500,"Actual cockpit window covers dynamic space panorama");need(space.saveToFile(output/"M207-dynamic-restored.png"),"Cannot save actual cockpit space");
    std::cout<<"Actual M207 P2 dynamic space through world pixels="<<windowPixels<<'\n';
    std::ifstream input(argv[1]);const auto definition=nlohmann::json::parse(input);
    for(const char* variant:{"projection","source"}){
        auto bad=definition;if(std::string(variant)=="projection"){bad["projection"]["default_panoramas"]["200"]["speed_x"]=1;}else{bad["source"]["sha256"]="forged";}
        bool rejected=false;try{loadCombatContentDefinition(argv[1],bad);}catch(const std::exception&){rejected=true;}need(rejected,"Unreviewed visual revision accepted");
    }
    bool missing=false;try{loadCombatContentDefinition(output/"missing-package"/"g7.json",definition);}catch(const std::exception&){missing=true;}need(missing,"Missing panorama images accepted");
    std::cout<<(remaining?"Remaining M193 / M207 dynamic / opaque map / revision rejection PASS; prior cache and M200/207/208/210 checks reused\n":"Source default panoramas / opaque-map variation / cache tone-path-dimensions / failed candidate / revision rejection PASS\n");
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
