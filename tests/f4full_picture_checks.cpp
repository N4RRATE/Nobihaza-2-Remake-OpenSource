#include "combat_presentation.h"
#include "full_content.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv){try{
    if(argc!=3){return 2;}auto content=loadCombatContent(argv[1]);
    // Reviewed real media, explicit development entry; production map gate stays closed.
    content.pendingMapTickets.erase(52);
    auto entry=content.entries.at("defeated-claimed");entry.map=52;entry.playerTile={8,12};
    entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();entry.session.flags[156]=false;
    content.entries["picture-domain"]=entry;
    auto reference=content.cue("m52-e2-p1-c31");
    reference["position"]={{"mode","literal"},{"xy",{168,88}}};
    content.setMediaDefinition("cues","picture-domain-reference",reference);
    sf::Font font;if(!font.openFromFile(content.maps.at(52).fontPath)){throw std::runtime_error("Missing real font");}
    GameApplication game(content,"picture-domain");
    sf::RenderTexture target({320,240});
    const auto output=std::filesystem::path(argv[2]);std::filesystem::create_directories(output);
    const auto render=[&](const std::string& cue,const std::string& actor,const std::string& name){
        CombatPresentation presentation(content,font);
        presentation.update({{PresentationEvent::Kind::Cue,cue,{},GridPosition{20,12},0,actor}});
        target.setView(sf::View(sf::FloatRect{{160,120},{320,240}}));target.clear(sf::Color::Transparent);
        presentation.draw(target,game);target.display();auto result=target.getTexture().copyToImage();
        if(!result.saveToFile(output/name)){throw std::runtime_error("Cannot save picture evidence");}return result;
    };
    // World foot(328,208), camera origin(160,120): original screen result(168,88).
    const auto actual=render("m52-e2-p1-c31","m52-e4","actual.png");
    const auto expected=render("picture-domain-reference",{},"expected.png");
    bool visible=false;for(std::size_t n=3;n<320*240*4;n+=4){visible=visible||expected.getPixelsPtr()[n]>0;}
    if(!visible){throw std::runtime_error("Real source picture must contain visible pixels");}
    if(!std::equal(actual.getPixelsPtr(),actual.getPixelsPtr()+320*240*4,expected.getPixelsPtr())){
        throw std::runtime_error("Source screen-variable picture must project its resolved world anchor through the camera");
    }
    CombatPresentation unfixed(content,font);
    unfixed.update({{PresentationEvent::Kind::Cue,"m52-e2-p1-c31",{},GridPosition{20,12},0,"m52-e4",GridPosition{168,88}}});
    target.setView(sf::View(sf::FloatRect{{176,120},{320,240}}));target.clear(sf::Color::Transparent);
    unfixed.draw(target,game);target.display();auto scrolled=target.getTexture().copyToImage();
    if(!scrolled.saveToFile(output/"unfixed-after-camera.png")){throw std::runtime_error("Cannot save camera evidence");}
    if(!std::equal(scrolled.getPixelsPtr(),scrolled.getPixelsPtr()+320*240*4,expected.getPixelsPtr())){
        throw std::runtime_error("Unfixed variable picture must keep command-time screen pixels while camera moves and no Move executes");
    }
    {auto cueVariant=content.cue("picture-domain-reference");cueVariant["position"]["xy"]={184,104};content.setMediaDefinition("cues","picture-domain-reference",cueVariant);}
    const auto movedExpected=render("picture-domain-reference",{},"move-expected.png");
    unfixed.update({{PresentationEvent::Kind::Cue,"m52-e2-p1-c85",{},GridPosition{21,13},0,"m52-e4",GridPosition{184,104}}});
    target.setView(sf::View(sf::FloatRect{{176,120},{320,240}}));target.clear(sf::Color::Transparent);
    unfixed.draw(target,game);target.display();auto moved=target.getTexture().copyToImage();
    if(!std::equal(moved.getPixelsPtr(),moved.getPixelsPtr()+320*240*4,movedExpected.getPixelsPtr())){
        throw std::runtime_error("Unfixed Move must consume its new command sample without camera resampling");
    }
    {auto cueVariant=content.cue("picture-domain-reference");cueVariant["position"]["xy"]={152,88};content.setMediaDefinition("cues","picture-domain-reference",cueVariant);}
    const auto fixedExpected=render("picture-domain-reference",{},"fixed-expected.png");
    unfixed.update({{PresentationEvent::Kind::Cue,"m52-e2-p2-c9",{},GridPosition{20,12},0,"m52-e4",GridPosition{168,88}}});
    target.setView(sf::View(sf::FloatRect{{176,120},{320,240}}));target.clear(sf::Color::Transparent);
    unfixed.draw(target,game);target.display();auto fixed=target.getTexture().copyToImage();
    if(!std::equal(fixed.getPixelsPtr(),fixed.getPixelsPtr()+320*240*4,fixedExpected.getPixelsPtr())){
        throw std::runtime_error("Map-fixed variable picture must follow the camera rather than pinning its screen sample");
    }
    if(content.catalog.contains("crocodile")){
        // CN1.00 M109 E3P1 C12 / E5P2 C2: same channel, different closed/open images.
        const auto event=[](const std::string& id){PresentationEvent e{PresentationEvent::Kind::Cue,id};e.actor="player";e.resolvedPicture=ResolvedPicture{{159,80},0,0};return e;};
        const auto capture=[&](CombatPresentation& p,const char* name){target.clear(sf::Color::Transparent);p.draw(target,game);target.display();auto image=target.getTexture().copyToImage();if(!image.saveToFile(output/name)){throw std::runtime_error("Cannot save mouth evidence");}return image;};
        CombatPresentation mouth(content,font);mouth.update({event("m109-e3-p1-c12")});mouth.update({event("m109-e5-p2-c2")});auto opened=capture(mouth,"crocodile-open-actual.png");
        CombatPresentation reference(content,font);reference.update({event("m109-e5-p2-c2")});auto expectedMouth=capture(reference,"crocodile-open-expected.png");
        if(!std::equal(opened.getPixelsPtr(),opened.getPixelsPtr()+320*240*4,expectedMouth.getPixelsPtr())){throw std::runtime_error("Resolved show-picture must replace closed mouth image on the same channel");}
    }
    if(content.maps.at(109).defaultPanorama){
        const auto require=[](bool value,const char* message){if(!value){throw std::runtime_error(message);}};
        const auto makeEntry=[&](int map,TilePosition tile){
            auto e=content.entries.at("post-pickup");e.map=map;e.playerTile=tile;e.direction=Direction::Up;e.sprite=content.maps.at(map).player.spriteId;
            e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();initializeFullSession(content,e.session);
            e.session.flags[1]=e.session.flags[18]=e.session.flags[13]=true;return e;
        };
        content.entries["panorama-boss"]=makeEntry(109,{9,8});
        content.entries["panorama-after"]=makeEntry(110,{9,26});
        content.entries["panorama-top"]=makeEntry(109,{9,0});
        const auto& bossDefault=*content.maps.at(109).defaultPanorama;
        const auto& afterDefault=*content.maps.at(110).defaultPanorama;
        require(!content.maps.at(109).loopX&&bossDefault.loopY&&bossDefault.autoY&&bossDefault.speedY==-5,"Map109 background loop is independent of topology");
        require(!afterDefault.loopX&&!afterDefault.loopY&&!afterDefault.autoX&&!afterDefault.autoY,"Map110 original yuka is static");
        GameApplication backgroundGame(content,"panorama-boss");CombatPresentation background(content,font);background.update(backgroundGame.takePresentation());
        const auto source=background.images().at(bossDefault.image).copyToImage();
        const auto captureBackground=[&](CombatPresentation& p,const GameApplication& app){
            const auto c=app.cameraCenter();target.setView(sf::View({static_cast<float>(c.x),static_cast<float>(c.y)},{320,240}));
            target.clear(sf::Color::Transparent);p.drawBackground(target,app);target.display();return target.getTexture().copyToImage();
        };
        const auto checkPixels=[&](const sf::Image& actual,int firstX,int firstY,const char* message){
            for(unsigned y=0;y<240;++y){for(unsigned x=0;x<320;++x){
                if(actual.getPixel({x,y})!=source.getPixel({(x+firstX)%320,(y+firstY)%480})){throw std::runtime_error(message);}
            }}
        };
        // CN1.00 M109 fields31-38; pinned Player GetY/Update. Camera top16
        // contributes8px. raw-5 then advances one source row per game tick.
        require(backgroundGame.world().frame()==0&&backgroundGame.cameraCenter().y==136,"Source sample starts at known camera and game clock");
        checkPixels(captureBackground(background,backgroundGame),0,8,"Default yuka must be visible before any panorama event");
        for(int n=0;n<32;++n){backgroundGame.update({});background.update(backgroundGame.takePresentation());}
        auto rolling=captureBackground(background,backgroundGame);checkPixels(rolling,0,40,"raw-5 must scroll1px/tick, not5px/tick");
        require(rolling.saveToFile(output/"panorama109-tick32.png"),"Cannot save default background evidence");
        InputFrame cancel;cancel.buttons[static_cast<int>(InputAction::Cancel)].pressed=true;
        backgroundGame.update(cancel);background.update(backgroundGame.takePresentation());require(backgroundGame.mode()==ApplicationMode::Equipment,"Actual boss menu opens");
        const auto pausedTick=backgroundGame.world().frame();const auto paused=captureBackground(background,backgroundGame);
        for(int n=0;n<60;++n){backgroundGame.update({});background.update(backgroundGame.takePresentation());}
        require(backgroundGame.world().frame()==pausedTick,"Menu freezes background game clock");
        const auto pausedAgain=captureBackground(background,backgroundGame);require(std::equal(paused.getPixelsPtr(),paused.getPixelsPtr()+320*240*4,pausedAgain.getPixelsPtr()),"Menu must freeze actual background pixels");
        backgroundGame.update(cancel);background.update(backgroundGame.takePresentation());backgroundGame.update({});background.update(backgroundGame.takePresentation());
        require(backgroundGame.world().frame()>pausedTick,"Menu resumes background clock");
        // Existing finite event panorama overrides default, then scene lifecycle clears it.
        const auto legacy=std::find_if(content.cues.begin(),content.cues.end(),[](const auto& row){return row.second->at("kind")=="panorama";});
        require(legacy!=content.cues.end(),"Real legacy panorama cue exists");
        background.update({{PresentationEvent::Kind::Cue,legacy->first}});
        sf::Sprite eventImage(background.images().at(legacy->second->at("image").get<std::string>()));eventImage.setPosition(target.getView().getCenter()-sf::Vector2f{160,120});
        target.clear(sf::Color::Transparent);target.draw(eventImage);target.display();const auto legacyExpected=target.getTexture().copyToImage();
        auto overridden=captureBackground(background,backgroundGame);require(std::equal(overridden.getPixelsPtr(),overridden.getPixelsPtr()+320*240*4,legacyExpected.getPixelsPtr()),"Legacy event panorama must override map default without changed placement");
        backgroundGame.restartDevelopmentEntry("panorama-after");background.update(backgroundGame.takePresentation());
        auto still=captureBackground(background,backgroundGame);checkPixels(still,0,240,"Scene reset clears event override and uses static110 default");
        for(int n=0;n<32;++n){backgroundGame.update({});background.update(backgroundGame.takePresentation());}
        checkPixels(captureBackground(background,backgroundGame),0,240,"Static110 must not inherit109 automatic scroll");
        require(still.saveToFile(output/"panorama110-static.png"),"Cannot save static background evidence");
        backgroundGame.restartDevelopmentEntry("picture-domain");background.update(backgroundGame.takePresentation());
        const auto cleared=captureBackground(background,backgroundGame);require(std::all_of(cleared.getPixelsPtr(),cleared.getPixelsPtr()+320*240*4,[](auto byte){return byte==0;}),"Map without default must clear previous background");
        // Representative raw positive slow setting wraps before integer pixel sampling.
        content.maps.at(109).defaultPanorama->speedY=1;GameApplication slow(content,"panorama-top");CombatPresentation slowPresentation(content,font);slowPresentation.update(slow.takePresentation());
        slow.update({});slowPresentation.update(slow.takePresentation());checkPixels(captureBackground(slowPresentation,slow),0,479,"Positive speed1 wraps2/32px before source integer sampling");
        content.maps.at(109).defaultPanorama->speedY=0;content.maps.at(109).defaultPanorama->loopX=true;content.maps.at(109).defaultPanorama->autoX=true;content.maps.at(109).defaultPanorama->speedX=-4;
        GameApplication horizontal(content,"panorama-boss");CombatPresentation horizontalPresentation(content,font);horizontalPresentation.update(horizontal.takePresentation());
        horizontal.update({});horizontal.update({});horizontalPresentation.update(horizontal.takePresentation());checkPixels(captureBackground(horizontalPresentation,horizontal),1,8,"Horizontal changed setting reuses parallax rule at half pixel/tick");
        std::cout<<"Default109/110 panorama, source speed, menu pause, event precedence, scene cleanup and axis variation passed\n";
    }
    std::cout<<"Source picture camera projection and resolved mouth image replacement passed; not natural gameplay acceptance\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
