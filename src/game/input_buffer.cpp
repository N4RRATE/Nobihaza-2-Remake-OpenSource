#include "input_buffer.h"
#include <algorithm>
#include <limits>
#include <utility>

void InputBuffer::press(InputAction action){
    if(!focused_){ return; }
    auto& button = buttons_.at(static_cast<std::size_t>(action));
    if(!button.held){ button.held = true; button.pressed = true; }
}
void InputBuffer::release(InputAction action){
    auto& button = buttons_.at(static_cast<std::size_t>(action));
    if(button.held){ button.held = false; button.released = true; }
    age_.at(static_cast<std::size_t>(action)) = 0;
}
void InputBuffer::setFocused(bool focused){
    focused_ = focused;
    if(!focused){ buttons_ = {}; age_ = {}; }

}
InputFrame InputBuffer::consume(){
    InputFrame result;
    result.buttons = buttons_;
    for(std::size_t i = 0; i < buttons_.size(); ++i){
        if(buttons_[i].held){
            if(age_[i] < std::numeric_limits<unsigned int>::max()){ ++age_[i]; }
        }else{ age_[i] = 0; }
        buttons_[i].pressed = false;
        buttons_[i].released = false;
    }
    const auto held = [&](InputAction action){ const auto& b=result.buttons[static_cast<std::size_t>(action)];return b.held||b.pressed; };
    if((held(InputAction::Up) && held(InputAction::Down)) ||
       (held(InputAction::Left) && held(InputAction::Right))){ return result; }
    unsigned int youngest = std::numeric_limits<unsigned int>::max();
    for(const auto& [direction,action] : {std::pair{Direction::Down,InputAction::Down},
            {Direction::Left,InputAction::Left}, {Direction::Right,InputAction::Right},
            {Direction::Up,InputAction::Up}}){
        const auto index = static_cast<std::size_t>(action);
        if((result.buttons[index].held || result.buttons[index].pressed) && (!result.movement || age_[index] < youngest)){
            youngest = age_[index]; result.movement = direction;
        }
    }
    return result;
}
