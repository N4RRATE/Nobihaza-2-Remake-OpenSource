#include "game.h"

#include <SFML/Graphics.hpp>
#include <stdexcept>
#include <iostream>
#include <optional>

namespace{
    std::optional<InputAction> gameKey(sf::Keyboard::Scancode key){
        switch(key){
        case sf::Keyboard::Scancode::Z: return InputAction::Confirm;
        case sf::Keyboard::Scancode::X: return InputAction::Cancel;
        case sf::Keyboard::Scancode::W: return InputAction::Up;
        case sf::Keyboard::Scancode::D: return InputAction::Right;
        case sf::Keyboard::Scancode::S: return InputAction::Down;
        case sf::Keyboard::Scancode::A: return InputAction::Left;
        default: return std::nullopt;
        }
    }
}

Game::Game(const SceneDefinition& scene)
    : scene_(scene), world_(scene_),
      window_(sf::VideoMode({960, 720}), "Nobihaza 2 Remake"),
      renderer_(scene_), dialogueRenderer_(font_){
    if(!font_.openFromFile(scene_.fontPath)){
        throw std::runtime_error("/font/file: cannot load " + scene_.fontPath.string());
    }
    window_.setFramerateLimit(60);
    window_.setKeyRepeatEnabled(false);
    input_.setFocused(window_.hasFocus());
    updateView();
}

void Game::updateView(){
    const auto windowSize = window_.getSize();
    if(windowSize.x == 0 || windowSize.y == 0){
        return;
    }

    window_.setView(makeSceneView(windowSize,
        {static_cast<float>(scene_.width * scene_.tileSize),
         static_cast<float>(scene_.height * scene_.tileSize)}));
}

int Game::run(){
    sf::Clock frameClock;
    while(window_.isOpen()){
        while(const auto event = window_.pollEvent()){
            if(event->is<sf::Event::Closed>()){
                window_.close();
            }else if(event->is<sf::Event::Resized>()){
                updateView();
            }else if(event->is<sf::Event::FocusLost>()){
                input_.setFocused(false);
            }else if(event->is<sf::Event::FocusGained>()){
                input_.setFocused(true);
            }else if(const auto* key = event->getIf<sf::Event::KeyPressed>()){
                if(const auto action = gameKey(key->scancode)){ input_.press(*action); }
            }else if(const auto* key = event->getIf<sf::Event::KeyReleased>()){
                if(const auto action = gameKey(key->scancode)){ input_.release(*action); }
            }
        }

        if(!window_.isOpen()){
            break;
        }

        const auto advance = simulation_.advance(frameClock.restart().asMicroseconds() / 1000000.0);
        if(advance.droppedSeconds > 0){
            std::cerr << "[simulation] discarded " << advance.droppedSeconds
                      << " seconds after a long stall\n";
        }
        for(int step = 0; step < advance.steps; ++step){ world_.update(input_.consume()); }

        window_.clear(sf::Color::Black);
        renderer_.draw(window_,world_);
        dialogueRenderer_.draw(window_,world_.dialogue().view());
        window_.display();
    }

    return 0;
}
