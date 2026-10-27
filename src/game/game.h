#pragma once

#include "scene_definition.h"
#include "scene_renderer.h"
#include "exploration_scene.h"
#include "input_buffer.h"
#include "simulation_clock.h"
#include "dialogue_renderer.h"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Font.hpp>

class Game{
public:
    Game(const SceneDefinition& scene);
    int run();

private:
    void updateView();

    SceneDefinition scene_;
    ExplorationScene world_;
    InputBuffer input_;
    SimulationClock simulation_;
    sf::Font font_;
    sf::RenderWindow window_;
    SceneRenderer renderer_;
    DialogueRenderer dialogueRenderer_;
};
