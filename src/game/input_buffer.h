#pragma once
#include "world_geometry.h"
#include <array>
#include <cstddef>
#include <optional>

enum class InputAction{ Up, Right, Down, Left, Confirm, Cancel, Weapon, Attack, AuxiliaryWeapon, Count };
inline constexpr std::size_t inputActionCount = static_cast<std::size_t>(InputAction::Count);
struct ButtonState{
    bool held{};
    bool pressed{};
    bool released{};
};
struct InputFrame{
    std::array<ButtonState,inputActionCount> buttons{};
    std::optional<Direction> movement;
};

class InputBuffer{
public:
    void press(InputAction action);
    void release(InputAction action);
    void setFocused(bool focused);
    InputFrame consume();
private:
    bool focused_{true};
    std::array<ButtonState,inputActionCount> buttons_{};
    std::array<unsigned int,inputActionCount> age_{};
};
