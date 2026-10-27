#include "combat_input.h"
namespace{
    std::optional<InputAction> keyboardAction(sf::Keyboard::Scancode key){
        switch(key){
        case sf::Keyboard::Scancode::W:return InputAction::Up;
        case sf::Keyboard::Scancode::D:return InputAction::Right;
        case sf::Keyboard::Scancode::S:return InputAction::Down;
        case sf::Keyboard::Scancode::A:return InputAction::Left;
        case sf::Keyboard::Scancode::Z:return InputAction::Confirm;
        case sf::Keyboard::Scancode::X:return InputAction::Cancel;
        default:return {};
        }
    }
}
void CombatInput::setFocused(bool focused){
    focused_=focused;buffer_.setFocused(focused);
    if(!focused){leftShift_=rightShift_=false;rightButton_.reset();}
}
InputFrame CombatInput::consume(){return buffer_.consume();}
void CombatInput::handle(const sf::Event& event){
    if(event.is<sf::Event::FocusLost>()){setFocused(false);return;}
    if(event.is<sf::Event::FocusGained>()){setFocused(true);return;}
    if(!focused_){return;}
    if(const auto* key=event.getIf<sf::Event::KeyPressed>()){
        if(key->scancode==sf::Keyboard::Scancode::LShift){leftShift_=true;}
        if(key->scancode==sf::Keyboard::Scancode::RShift){rightShift_=true;}
        if(const auto action=keyboardAction(key->scancode)){buffer_.press(*action);}
    }else if(const auto* key=event.getIf<sf::Event::KeyReleased>()){
        if(key->scancode==sf::Keyboard::Scancode::LShift){leftShift_=false;}
        if(key->scancode==sf::Keyboard::Scancode::RShift){rightShift_=false;}
        if(const auto action=keyboardAction(key->scancode)){buffer_.release(*action);}
    }else if(const auto* mouse=event.getIf<sf::Event::MouseButtonPressed>()){
        if(mouse->button==sf::Mouse::Button::Left){buffer_.press(InputAction::Attack);}
        if(mouse->button==sf::Mouse::Button::Right&&!rightButton_){
            rightButton_=(leftShift_||rightShift_)?InputAction::AuxiliaryWeapon:InputAction::Weapon;
            buffer_.press(*rightButton_);
        }
    }else if(const auto* mouse=event.getIf<sf::Event::MouseButtonReleased>()){
        if(mouse->button==sf::Mouse::Button::Left){buffer_.release(InputAction::Attack);}
        if(mouse->button==sf::Mouse::Button::Right&&rightButton_){
            buffer_.release(*rightButton_);rightButton_.reset();
        }
    }
}
