#include "combat_input.h"
#include "game_application.h"
#include "simulation_clock.h"
#include <iostream>
#include <stdexcept>
namespace{
    void require(bool condition,const char* message){if(!condition){throw std::runtime_error(message);}}
    const ButtonState& button(const InputFrame& input,InputAction action){return input.buttons.at(static_cast<int>(action));}
    sf::Event down(sf::Keyboard::Scancode key){sf::Event::KeyPressed event;event.scancode=key;return event;}
    sf::Event up(sf::Keyboard::Scancode key){sf::Event::KeyReleased event;event.scancode=key;return event;}
    sf::Event mouseDown(sf::Mouse::Button button){return sf::Event::MouseButtonPressed{button,{20,20}};}
    sf::Event mouseUp(sf::Mouse::Button button){return sf::Event::MouseButtonReleased{button,{20,20}};}
}
int main(int argc,char** argv){
    if(argc!=2){return 2;}
    CombatInput input;
    input.handle(mouseDown(sf::Mouse::Button::Left));
    auto frame=input.consume();
    require(button(frame,InputAction::Attack).pressed&&!button(frame,InputAction::Confirm).pressed,"left mouse is attack, not confirmation");
    input.handle(mouseUp(sf::Mouse::Button::Left));input.consume();
    input.handle(down(sf::Keyboard::Scancode::Z));frame=input.consume();
    require(button(frame,InputAction::Confirm).pressed&&!button(frame,InputAction::Attack).held,"Z only confirms");
    input.handle(up(sf::Keyboard::Scancode::Z));input.consume();
    input.handle(down(sf::Keyboard::Scancode::LShift));input.handle(down(sf::Keyboard::Scancode::RShift));
    input.handle(up(sf::Keyboard::Scancode::LShift));input.handle(mouseDown(sf::Mouse::Button::Right));frame=input.consume();
    require(button(frame,InputAction::AuxiliaryWeapon).pressed&&!button(frame,InputAction::Weapon).held,"either Shift plus right selects auxiliary");
    input.handle(up(sf::Keyboard::Scancode::RShift));frame=input.consume();
    require(button(frame,InputAction::AuxiliaryWeapon).held&&!button(frame,InputAction::Weapon).held,"selected slot survives modifier release");
    input.handle(mouseUp(sf::Mouse::Button::Right));input.consume();
    input.handle(mouseDown(sf::Mouse::Button::Right));frame=input.consume();
    require(button(frame,InputAction::Weapon).pressed,"plain right selects main weapon");
    input.handle(sf::Event::FocusLost{});input.handle(mouseDown(sf::Mouse::Button::Left));input.handle(sf::Event::FocusGained{});frame=input.consume();
    require(!button(frame,InputAction::Weapon).held&&!button(frame,InputAction::Attack).held&&!button(frame,InputAction::AuxiliaryWeapon).held,"focus loss clears mouse and modifier states");

    CombatInput paced;SimulationClock clock;
    paced.handle(down(sf::Keyboard::Scancode::Z));paced.handle(up(sf::Keyboard::Scancode::Z));
    paced.handle(mouseDown(sf::Mouse::Button::Left));paced.handle(mouseUp(sf::Mouse::Button::Left));
    paced.handle(down(sf::Keyboard::Scancode::W));
    require(clock.advance(.001).steps==0,"render-only frame leaves pending physical edges untouched");
    const auto catchup=clock.advance(.25);int confirmations=0,attacks=0;
    require(catchup.steps==8&&catchup.droppedSeconds>0,"catch-up keeps eight-step and accumulated-time caps");
    for(int i=0;i<catchup.steps;++i){
        const auto step=paced.consume();
        confirmations+=button(step,InputAction::Confirm).pressed;attacks+=button(step,InputAction::Attack).pressed;
        require(step.movement==Direction::Up,"held movement survives every catch-up step");
    }
    require(confirmations==1&&attacks==1,"zero-step taps reach one catch-up step, never eight");
    paced.handle(down(sf::Keyboard::Scancode::W));
    require(!button(paced.consume(),InputAction::Up).pressed,"repeated physical key press does not fabricate an edge");
    paced.handle(down(sf::Keyboard::Scancode::Z));paced.handle(down(sf::Keyboard::Scancode::LShift));
    paced.handle(mouseDown(sf::Mouse::Button::Right));paced.handle(sf::Event::FocusLost{});
    paced.handle(down(sf::Keyboard::Scancode::D));paced.handle(mouseDown(sf::Mouse::Button::Left));
    paced.handle(sf::Event::FocusGained{});frame=paced.consume();
    require(!frame.movement,"focus loss clears movement before refocus");
    for(const auto& state:frame.buttons){require(!state.held&&!state.pressed&&!state.released,"refocus replays no held state or edge");}
    paced.handle(mouseDown(sf::Mouse::Button::Right));frame=paced.consume();
    require(button(frame,InputAction::Weapon).pressed&&!button(frame,InputAction::AuxiliaryWeapon).held,"refocus forgets the old modifier and mouse slot");

    auto data=loadCombatContent(argv[1]);auto& entry=data.entries.at("post-pickup");
    entry.session.flags[52]=false;entry.session.mainWeapon=11;entry.session.inventory.erase(11);
    GameApplication scene(data,"post-pickup");
    input.handle(mouseDown(sf::Mouse::Button::Right));
    for(int i=0;i<45;++i){scene.update(input.consume());}
    input.handle(mouseUp(sf::Mouse::Button::Right));for(int i=0;i<5;++i){scene.update(input.consume());}
    require(scene.raised(),"right click raises, release keeps posture");
    input.handle(mouseDown(sf::Mouse::Button::Left));input.handle(mouseUp(sf::Mouse::Button::Left));
    SimulationClock attackClock;require(attackClock.advance(.001).steps==0,"attack may arrive before a logic step");
    const auto attackSteps=attackClock.advance(.2);require(attackSteps.steps==8,"attack scene receives eight catch-up steps");
    for(int i=0;i<attackSteps.steps;++i){scene.update(input.consume());}
    for(int i=0;i<100;++i){scene.update(input.consume());}
    require(scene.session().magazine(11)==14,"physical left click fires one shot through scene");
    input.handle(mouseDown(sf::Mouse::Button::Right));for(int i=0;i<10;++i){scene.update(input.consume());}
    input.handle(mouseUp(sf::Mouse::Button::Right));for(int i=0;i<45;++i){scene.update(input.consume());}
    require(!scene.raised(),"second short right click lowers");
    scene.takePresentation();
    input.handle(down(sf::Keyboard::Scancode::LShift));input.handle(mouseDown(sf::Mouse::Button::Right));
    scene.update(input.consume());
    const auto feedback=scene.takePresentation();
    bool rejected=false;for(const auto& event:feedback){if(event.kind==PresentationEvent::Kind::Notice){rejected=true;}}
    require(rejected&&!scene.raised()&&scene.session().magazine(11)==14,"unimplemented auxiliary has explicit feedback and never raises main gun");
    std::cout<<"combat mouse bindings and scene checks passed\n";
}
