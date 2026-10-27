#include "game_application.h"
#include <iostream>
#include <type_traits>
static_assert(!std::is_move_constructible_v<GameApplication>);
#include <stdexcept>
void require(bool value,const char* message){if(!value){throw std::runtime_error(message);}}
int main(int argc,char** argv){
    if(argc!=2){return 2;}
    GameApplication game(loadCombatContent(argv[1]),"natural",[](int low,int){return low;});
    const auto original=game.generation();
    game.takePresentation();
    for(int n=0;n<3;++n){
        game.restartDevelopmentEntry("post-pickup");
        const auto events=game.takePresentation();
        require(!events.empty()&&events.front().kind==PresentationEvent::Kind::ResetGame,"new game clears consumed presentation before startup cues");
        require(game.generation()!=original,"scene identities survive destruction and recreation");
        require(game.session().count(11)==1&&game.session().equipment().mainWeapon==0,"fresh fixture remains unequipped");
        InputFrame menu;menu.buttons[static_cast<int>(InputAction::Cancel)].pressed=true;game.update(menu);
        require(game.mode()==ApplicationMode::Equipment,"application accepts menu request");
        const auto before=game.world().player().motion.position();
        for(int i=0;i<90;++i){game.update({});}
        require(game.world().player().motion.position().x==before.x&&game.world().player().motion.position().y==before.y,"menu owns pause");
        game.restartDevelopmentEntry("natural");
        require(game.mode()==ApplicationMode::Playing&&!game.dialogue().active(),"new instance drops old menu and message");
        require(game.session().count(11)==0,"new developer session does not leak inventory");
    }
    const auto generation=game.generation();
    try{game.restartDevelopmentEntry("missing");throw std::logic_error("missing fixture accepted");}
    catch(const std::out_of_range&){}
    require(game.generation()==generation,"failed candidate preserves active game");
    const auto content=loadCombatContent(argv[1]);GameSession borrowed(content.items,content.entries.at("post-pickup").session);
    CombatScene consumer(content,borrowed,"post-pickup",[](int lo,int){return lo;});
    auto request=consumer.requestInput(NumberPrompt{4,0});const int hp=borrowed.hp();
    InputFrame confirm;confirm.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;
    for(int i=0;i<120;++i){consumer.update({});}require(borrowed.hp()==hp,"modal prompt suspends map simulation");
    consumer.update(confirm);auto result=consumer.takeInputResult(request);require(result&&std::get<NumberSubmitted>(result->result).value==0,"scene returns typed prompt result");
    require(!consumer.takeInputResult(request),"scene cannot consume result twice");
    request=consumer.requestInput(NumberPrompt{4,12});require(consumer.cancelInput(request),"owning flow can cancel request");
    require(!consumer.takeInputResult(request),"cancelled result never reaches gameplay");
    for(bool dieAfterConfirm:{false,true}){
        GameSession state(content.items,content.entries.at("post-pickup").session);
        CombatScene scene(content,state,"post-pickup",[](int lo,int){return lo;});
        const auto id=scene.requestInput(NumberPrompt{4,12});
        if(dieAfterConfirm){scene.update(confirm);}
        state.loseHp(100);
        if(!dieAfterConfirm){scene.update(confirm);}
        require(!scene.takeInputResult(id),"death invalidates pending and completed prompt results");
        scene.update({});require(scene.foreground()==ForegroundKind::Death,"death takes precedence over modal prompt");
    }
    std::cout<<"application lifecycle checks passed\n";
}
