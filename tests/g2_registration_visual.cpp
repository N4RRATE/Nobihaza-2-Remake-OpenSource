#include "combat_presentation.h"
#include "g1_piano_driver.h"
#include "scene_renderer.h"
#include <queue>
// Explicit presentation fixture at the source reception; never writes a natural save.
int main(int argc,char** argv){try{
    need(argc==4,"usage G2-06 package source-save isolated-pixel-output");
    auto c=loadCombatContent(argv[1]);const auto saved=decodeSave(c,read(argv[2]));
    auto e=c.entries.at("post-pickup");e.map=159;e.session=saved.session;
    e.playerTile=c.objects.at("m159-e3").tile;e.direction=Direction::Up;
    e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();
    c.entries["registration-render"]=e;e.map=164;e.playerTile={18,9};e.direction=Direction::Right;e.session.inventory[101]=1;c.entries["g3-boundary-render"]=e;GameApplication game(c,"registration-render",safe);
    sf::Font font;need(font.openFromFile(game.definition().fontPath),"Original font absent");
    CombatPresentation presentation(c,font);sf::RenderTexture surface({320,240});
    const auto output=std::filesystem::path(argv[3]);std::filesystem::create_directories(output);
    const auto tick=[&](InputFrame input=InputFrame{}){game.update(input);presentation.update(game.takePresentation());};
    const auto press=[&](InputAction action=InputAction::Confirm){tick(key(action));tick();};
    const auto capture=[&](const std::string& name){
        const auto camera=game.cameraCenter();surface.setView(sf::View({float(camera.x),float(camera.y)},{320,240}));surface.clear(sf::Color::Black);
        if(game.hasGame()){SceneRenderer renderer(game.definition());renderer.draw(surface,game.world(),presentation.characterFlashes());}presentation.draw(surface,game);surface.display();
        need(surface.getTexture().copyToImage().saveToFile(output/(name+".png")),"Cannot save actual rendered keyboard");
    };
    press();for(int n=0;n<3000&&!game.inputPromptView().cursor;++n){tick(n%8==0?key():InputFrame{});}
    need(game.inputPromptView().cursor.has_value(),"Reception choice absent");press(InputAction::Down);press();
    for(int n=0;n<3000&&game.foreground()!=ForegroundKind::Registration;++n){tick(n%8==0?key():InputFrame{});}
    need(game.foreground()==ForegroundKind::Registration,"Keyboard absent");for(int n=0;n<32;++n){tick();}
    press(InputAction::Cancel);need(game.mode()==ApplicationMode::Playing&&game.foreground()==ForegroundKind::Registration,"Physical cancel leaked into root menu");
    capture("keyboard-initial");int selected=200;
    const auto go=[&](int target){
        std::queue<int> todo;todo.push(selected);std::map<int,std::vector<InputAction>> paths{{selected,{}}};
        while(!todo.empty()&&!paths.contains(target)){
            const int from=todo.front();todo.pop();
            for(const auto& [direction,action]:std::initializer_list<std::pair<const char*,InputAction>>{{"up",InputAction::Up},{"right",InputAction::Right},{"left",InputAction::Left},{"down",InputAction::Down}}){
                const int next=c.catalog.at("registration").at("neighbors").at(std::to_string(from)).at(direction);
                if(!paths.contains(next)){paths[next]=paths.at(from);paths[next].push_back(action);todo.push(next);}
            }
        }
        need(paths.contains(target),"Key unreachable");for(auto action:paths.at(target)){press(action);}selected=target;
    };
    for(int keyId:{102,301,302,102,208,208,200}){go(keyId);press();}
    go(209);capture("keyboard-username-enter");press();
    for(int keyId:{304,2,1,1,9,5,3,8}){go(keyId);press();}
    go(209);capture("keyboard-password-enter");
    need(game.session().count(101)==0,"Visual fixture awarded before submission");
    game.returnToTitle();presentation.update(game.takePresentation());capture("keyboard-retired");
    game.restartDevelopmentEntry("g3-boundary-render");presentation.update(game.takePresentation());InputFrame contact;contact.movement=Direction::Right;tick(contact);for(int n=0;n<60;++n){tick();}
    need(game.foreground()==ForegroundKind::Content&&game.world().player().motion.tile()==TilePosition{19,9}&&!game.session().flag(449)&&game.session().count(20)==0,"G3 visual handoff crossed or fabricated progress");capture("g3-handoff");
    std::cout<<"Actual original images/font/16 glyph slots/independent Enter cursor/retirement rendered without a visible window\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
