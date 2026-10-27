#pragma once
#include "input_buffer.h"
#include <SFML/Window/Event.hpp>

// Physical bindings stay outside gameplay. A right-button press selects a slot
// until release, so releasing Shift cannot turn an auxiliary request into a gun.
class CombatInput{
public:
    void handle(const sf::Event& event);
    void setFocused(bool focused);
    InputFrame consume();
private:
    InputBuffer buffer_;
    bool focused_{true};
    bool leftShift_{};
    bool rightShift_{};
    std::optional<InputAction> rightButton_;
};
