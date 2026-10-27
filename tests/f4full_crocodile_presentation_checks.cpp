#include "combat_presentation.h"
#include "full_content.h"
#include "scene_renderer.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {
void need(bool ok,const std::string& message){if(!ok){throw std::runtime_error(message);}}
InputFrame key(InputAction action=InputAction::Confirm){InputFrame f;f.buttons[static_cast<int>(action)]={true,true,false};return f;}
CombatEntry entry(const CombatContent& c,int map,TilePosition tile){
    auto e=c.entries.at("post-pickup");e.map=map;e.playerTile=tile;e.direction=Direction::Up;e.sprite=c.maps.at(map).player.spriteId;
    e.positions.clear();e.pages.clear();e.through.clear();e.directions.clear();initializeFullSession(c,e.session);
    e.session.flags[1]=e.session.flags[18]=e.session.flags[13]=true;return e;
}
sf::Image render(sf::RenderTexture& target,CombatPresentation& p,const GameApplication& game,bool world){
    const auto camera=game.cameraCenter();target.setView(sf::View({float(camera.x),float(camera.y)},{320,240}));target.clear(sf::Color::Transparent);
    if(world){p.drawBackground(target,game);SceneRenderer layer(game.definition());layer.draw(target,game.world(),p.characterFlashes());}
    p.draw(target,game);target.display();return target.getTexture().copyToImage();
}
bool same(const sf::Image& a,const sf::Image& b){return std::equal(a.getPixelsPtr(),a.getPixelsPtr()+320*240*4,b.getPixelsPtr());}
}
int main(int argc,char** argv){try{
    need(argc==4,"usage package output intro|gas");auto c=loadCombatContent(argv[1]);const auto output=std::filesystem::path(argv[2]);std::filesystem::create_directories(output);
    sf::Font font;need(font.openFromFile(c.maps.at(108).fontPath),"Source font loads");sf::RenderTexture surface({320,240});
    if(std::string(argv[3])=="intro"){
        auto e=entry(c,108,{28,10});e.direction=Direction::Left;e.session.flags[322]=true;c.entries["intro-render"]=e;
        auto reference=c;for(const auto& [id,definition]:reference.cues){auto cue=*definition;
            if(id.starts_with("m108-e5")&&cue.at("kind")=="show_picture"){cue["fixed_to_map"]=false;reference.setMediaDefinition("cues",id,cue);}
        }
        GameApplication game(c,"intro-render",[](int lo,int){return lo;});CombatPresentation actual(c,font),expected(reference,font);
        actual.update(game.takePresentation());expected.update({});bool seen=false;
        // Use the real NPC contact input and finite source intro, not manually emitted pictures.
        InputFrame contact;contact.movement=Direction::Left;game.update(contact);
        auto events=game.takePresentation();actual.update(events);expected.update(events);
        for(int n=0;n<60000&&game.mapId()==108;++n){
            game.update(key());events=game.takePresentation();actual.update(events);expected.update(events);
            if(std::any_of(events.begin(),events.end(),[](const auto& x){return x.id=="m108-e5-p1-c227";})){
                seen=true;const auto frame=render(surface,actual,game,false),wanted=render(surface,expected,game,false);
                need(frame.saveToFile(output/"intro-actual.png")&&wanted.saveToFile(output/"intro-expected.png"),"Save real rendered pixels");
                need(render(surface,actual,game,true).saveToFile(output/"intro-scene.png"),"Save actual scene");
                need(same(frame,wanted),"M108 E5P1 C227 crocodile must be visible at source screen center(160,70) with nonzero camera origin");
                const auto shown=*std::find_if(events.begin(),events.end(),[](const auto& x){return x.id=="m108-e5-p1-c227";});
                CombatPresentation fixed(c,font);fixed.update({shown});
                auto shifted=reference;{auto cueVariant=shifted.cue(shown.id);cueVariant["position"]["xy"]={144,62};shifted.setMediaDefinition("cues",shown.id,cueVariant);}CombatPresentation moved(shifted,font);moved.update({shown});
                const auto camera=game.cameraCenter();surface.setView(sf::View({float(camera.x+16),float(camera.y+8)},{320,240}));surface.clear(sf::Color::Transparent);
                fixed.draw(surface,game);surface.display();auto scrolled=surface.getTexture().copyToImage();surface.clear(sf::Color::Transparent);moved.draw(surface,game);surface.display();
                need(same(scrolled,surface.getTexture().copyToImage()),"Literal map-fixed picture follows later camera delta without double projection");
            }
        }
        need(seen&&game.mapId()==109&&game.session().flag(323)&&game.session().variable(89)==0,"Actual intro appears and initializes109 without victory");
        std::cout<<"Natural NPC intro rendered at227 and entered109 without victory\n";
    }else{
        CombatPresentation presentation(c,font);
        for(const auto [weapon,x]:{std::pair{11,9},std::pair{11,6},std::pair{14,6}}){
            auto e=entry(c,109,{x,15});e.direction=Direction::Down;e.session.inventory[weapon]=1;e.session.mainWeapon=weapon;e.session.magazines[weapon]=5;
            const auto random=[x](int lo,int hi){return lo==6&&hi==13?x:lo;};
            c.entries["gas-render"]=e;GameApplication rendered(c,"gas-render",random);GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e,random);
            presentation.update({{PresentationEvent::Kind::ResetGame}});presentation.update(scene.takePresentation());
            bool blast=false,damage=false,blood=false,gasBlood=false,retired=false;int boomSounds=0;std::set<std::string> blastPictures;
            // The facade supplies the same actor/camera samples; this explicit fixture changes only boss work fields.
            auto tick=[&](InputFrame f){scene.update(f);if(!retired){rendered.update(f);rendered.takePresentation();}const auto events=scene.takePresentation();
                for(const auto& event:events){if(event.id.starts_with("m109-e6-p10")){blast=true;blastPictures.insert(event.id);std::cout<<"weapon"<<weapon<<" blast cue "<<event.id<<'\n'<<std::flush;}}
                for(const auto& event:events){
                    if(event.kind==PresentationEvent::Kind::Sound&&event.sound.resource==c.cue("m109-e6-p10-c1").at("sound").at("resource").get<std::string>()){++boomSounds;}
                    if(event.kind==PresentationEvent::Kind::Effect&&event.id.starts_with("enemy-slot-18-blood-")){blood|=event.actor=="player";gasBlood|=event.actor=="m109-e6"&&event.id=="enemy-slot-18-blood-3";}
                }
                presentation.update(events);render(surface,presentation,rendered,false);return events;
            };
            tick(key(InputAction::Weapon));for(int i=0;i<359;++i){tick({});}need(scene.raised()&&state.flag(29),"Real raise and source gas spawn");
            const auto gas=scene.sampleActorScreen("m109-e6");state.setVariable(84,int(gas.y)-70);state.setVariable(91,10);state.setVariable(117,1000);
            need(findCharacter(rendered.world(),"m109-e6").motion.tile()==findCharacter(scene.world(),"m109-e6").motion.tile()&&rendered.cameraCenter().x==scene.cameraCenter().x&&rendered.cameraCenter().y==scene.cameraCenter().y,"Rendered actor samples match actual weapon scene");
            const int before=state.magazine(weapon);
            const int damageBefore=state.variable(89);tick(key(InputAction::Attack));
            for(int i=0;i<180;++i){tick({});if(state.variable(89)>0){damage=true;}if(blast&&i==6){need(render(surface,presentation,rendered,false).saveToFile(output/("gas-weapon"+std::to_string(weapon)+".png")),"Save rendered explosion");}}
            std::cout<<"weapon"<<weapon<<" magazine="<<state.magazine(weapon)<<" blast="<<blast<<" gasBlood="<<gasBlood<<" sounds="<<boomSounds<<" damage="<<state.variable(89)-damageBefore<<'\n'<<std::flush;
            if(x==9){need(blood&&!blast&&state.magazine(weapon)==before-1,"Aligned handgun hits body, renders source blood and ends its bullet before gas");}
            else{
                need(state.magazine(weapon)==before-1&&blast&&damage&&gasBlood&&boomSounds==2&&state.variable(89)==damageBefore+80,"Real weapon ignites gas, emits two sounds/source gas blood and adds exact source blast damage80");
                need(blastPictures.contains("m109-e6-p10-c49")&&blastPictures.contains("m109-e6-p10-c55")&&!state.flag(29),"All source explosion frames and cleanup finish");
            }
            scene.deactivate();retired=true;const int prior=state.variable(89);const bool gasFlag=state.flag(29);for(int i=0;i<600;++i){tick({});}need(state.variable(89)==prior&&state.flag(29)==gasFlag,"Retired scene cannot regenerate gas or damage");
            std::cout<<"Real weapon"<<weapon<<" x"<<x<<(x==9?" normal source blood rendered":" gas explosion rendered, boss damaged and cleaned")<<"; retired\n";
        }
        auto invalid=c;invalid.media["effects"].erase("enemy-slot-18-blood-3");bool rejected=false;
        try{installCrocodileContent(invalid,invalid.full->at("runtime").at("evacuation"));}catch(const std::exception&){rejected=true;}
        need(rejected,"Missing source blood effect remains rejected before gameplay");
    }
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
