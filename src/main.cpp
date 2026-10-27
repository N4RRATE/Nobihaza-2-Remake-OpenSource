#include "RoomMap.h"

#include <SFML/Graphics.hpp>
#include <exception>
#include <iostream>
#include <string>

sf::Vector2f tileAnchor(TilePosition tile, int tileSize){
    return {
        static_cast<float>(tile.x * tileSize) + tileSize / 2.f,
        static_cast<float>((tile.y + 1) * tileSize)
    };
}

bool isFacingTarget(TilePosition playerTile, TilePosition facing, TilePosition targetTile){
    const TilePosition frontTile{
        playerTile.x + facing.x,
        playerTile.y + facing.y
    };

    return frontTile.x == targetTile.x
        && frontTile.y == targetTile.y;
}

int main(){
    try{
        const RoomMap room("private/generated/first-room/room.json");
        const sf::Vector2u roomPixels{
            static_cast<unsigned int>(room.width() * room.tileSize()),
            static_cast<unsigned int>(room.height() * room.tileSize())
        };
        const sf::Vector2f gameSize{
            static_cast<float>(roomPixels.x),
            static_cast<float>(roomPixels.y)
        };
        sf::RenderWindow window(sf::VideoMode({960,720}),"Nobihaza 2 Remake");

        window.setFramerateLimit(60);

        window.setKeyRepeatEnabled(false);

        sf::View gameView;
        gameView.setSize(gameSize);
        gameView.setCenter({gameSize.x / 2.f, gameSize.y / 2.f});
        window.setView(gameView);

        sf::Texture backgroundTexture;

        if (!backgroundTexture.loadFromFile(
                "private/generated/first-room/room-background.png"))
        {
            std::cerr << "Failed to load: private/generated/first-room/room-background.png\n";
            return 1;
        }

        if(backgroundTexture.getSize() != roomPixels){
            std::cerr << "background image size does not match room.json grid/pixel_size\n";
            return 1;
        }

        sf::Sprite backgroundSprite(backgroundTexture);

        sf::Texture foregroundTexture;

        if (!foregroundTexture.loadFromFile(
                "private/generated/first-room/room-foreground.png"))
        {
            std::cerr << "Failed to load: private/generated/first-room/room-foreground.png\n";
            return 1;
        }

        if(foregroundTexture.getSize() != roomPixels){
            std::cerr << "foreground image size does not match room.json grid/pixel_size\n";
            return 1;
        }

        sf::Sprite foregroundSprite(foregroundTexture);

        sf::Texture playerTexture;

        if (!playerTexture.loadFromFile(
                "private/generated/first-room/nobita.png"))
        {
            std::cerr << "Failed to load: private/generated/first-room/nobita.png\n";
            return 1;
        }

        sf::Sprite playerSprite(playerTexture);

        const sf::IntRect idleFrame({24, 0}, {24, 32});
        playerSprite.setTextureRect(idleFrame);

        playerSprite.setOrigin({12.f, 32.f});
        TilePosition playerTile = room.playerSpawn();
        TilePosition targetTile = playerTile;
        TilePosition facing{0, -1};
        playerSprite.setPosition(tileAnchor(playerTile, room.tileSize()));

        sf::Texture soldierTexture;

        if (!soldierTexture.loadFromFile(
                "private/generated/first-room/soldier.png"))
        {
            std::cerr << "Failed to load: private/generated/first-room/soldier.png\n";
            return 1;
        }

        sf::Sprite soldierSprite(soldierTexture);
        soldierSprite.setOrigin({12.f, 32.f});
        soldierSprite.setPosition(tileAnchor(room.soldierTile(), room.tileSize()));

        bool messageOpen = false;

        sf::RectangleShape messageBox({304.f, 56.f});
        messageBox.setPosition({8.f, 176.f});
        messageBox.setFillColor(sf::Color(20, 20, 30, 240));
        messageBox.setOutlineColor(sf::Color::White);
        messageBox.setOutlineThickness(1.f);
        sf::Font messageFont;

        if(!messageFont.openFromFile(
            "private/generated/first-room/NotoSansCJKsc-Regular.otf")){
            std::cerr << "Failed to load: private/generated/first-room/NotoSansCJKsc-Regular.otf\n";
            return 1;
        }

        const std::string dialogue = room.interactionText();
        const sf::String displayText =
            sf::String::fromUtf8(dialogue.begin(), dialogue.end());
        sf::Text messageText(messageFont, displayText, 56);
        messageText.setScale({0.25f, 0.25f});
        messageText.setPosition({20.f, 188.f});
        messageText.setFillColor(sf::Color::White);

        bool upHeld = false;
        bool downHeld = false;
        bool leftHeld = false;
        bool rightHeld = false;

        bool isMoving = false;
        sf::Vector2f moveDirection{0.f, 0.f};
        sf::Vector2f targetPosition = playerSprite.getPosition();
        float remainingDistance = 0.f;

        const float moveSpeed = 64.f;
        sf::Clock frameClock;

        while(window.isOpen()){

            float deltaTime = frameClock.restart().asSeconds();

            if(deltaTime > 0.05f){
                deltaTime = 0.05f;
            }

            while(const auto event = window.pollEvent()){
                if(event->is<sf::Event::Closed>()){
                    window.close();
                }

                if(event->is<sf::Event::Resized>()){
                    const auto size = window.getSize();

                    if(size.x == 0 || size.y == 0){
                        continue;
                    }

                    const float gameRatio = gameSize.x / gameSize.y;
                    const float windowRatio =
                        static_cast<float>(size.x) / static_cast<float>(size.y);

                    sf::FloatRect viewport({0.f, 0.f}, {1.f, 1.f});

                    if(windowRatio > gameRatio){
                        viewport.size.x = gameRatio / windowRatio;
                        viewport.position.x = (1.f - viewport.size.x) / 2.f;
                    }else{
                        viewport.size.y = windowRatio / gameRatio;
                        viewport.position.y = (1.f - viewport.size.y) / 2.f;
                    }

                    gameView.setViewport(viewport);
                    window.setView(gameView);
                }

                if(const auto key = event->getIf<sf::Event::KeyPressed>()){
                    if(key->code == sf::Keyboard::Key::Z && window.hasFocus()){
                        const bool canOpen = !isMoving
                            && isFacingTarget(playerTile, facing, room.soldierTile());

                        if(messageOpen || canOpen){
                            messageOpen = !messageOpen;

                            upHeld = false;
                            downHeld = false;
                            leftHeld = false;
                            rightHeld = false;
                        }
                    }else if(!messageOpen){
                        if(key->code == sf::Keyboard::Key::W){
                            upHeld = true;
                        }else if(key->code == sf::Keyboard::Key::S){
                            downHeld = true;
                        }else if(key->code == sf::Keyboard::Key::A){
                            leftHeld = true;
                        }else if(key->code == sf::Keyboard::Key::D){
                            rightHeld = true;
                        }
                    }
                }

                if(const auto key = event->getIf<sf::Event::KeyReleased>()){
                    if(key->code == sf::Keyboard::Key::W){
                        upHeld = false;
                    }else if(key->code == sf::Keyboard::Key::S){
                        downHeld = false;
                    }else if(key->code == sf::Keyboard::Key::A){
                        leftHeld = false;
                    }else if(key->code == sf::Keyboard::Key::D){
                        rightHeld = false;
                    }
                }

                if(event->is<sf::Event::FocusLost>()){
                    upHeld = false;
                    downHeld = false;
                    leftHeld = false;
                    rightHeld = false;
                }
            }
            
            if(!window.isOpen()){
                break;
            }

            if(window.hasFocus() && !messageOpen){
                auto position = playerSprite.getPosition();

                if(!isMoving){
                    moveDirection = {0.f, 0.f};
                    int frameRow = 0;

                    if(upHeld){
                        moveDirection = {0.f, -1.f};
                        frameRow = 0;
                    }else if(downHeld){
                        moveDirection = {0.f, 1.f};
                        frameRow = 2;
                    }else if(leftHeld){
                        moveDirection = {-1.f, 0.f};
                        frameRow = 3;
                    }else if(rightHeld){
                        moveDirection = {1.f, 0.f};
                        frameRow = 1;
                    }

                    if(moveDirection.x != 0.f || moveDirection.y != 0.f){

                        facing = {static_cast<int>(moveDirection.x),static_cast<int>(moveDirection.y)};

                        const TilePosition nextTile{
                            playerTile.x + static_cast<int>(moveDirection.x),
                            playerTile.y + static_cast<int>(moveDirection.y)
                        };

                        // 即使被挡住，也允许转身面向目标。
                        playerSprite.setTextureRect(
                            sf::IntRect({24, frameRow * 32}, {24, 32}));

                        if(room.canMove(playerTile, nextTile)){
                            targetTile = nextTile;
                            targetPosition = tileAnchor(targetTile, room.tileSize());
                            remainingDistance = static_cast<float>(room.tileSize());
                            isMoving = true;
                        }
                    }
                }

                if(isMoving){
                    float distance = moveSpeed * deltaTime;

                    if(distance > remainingDistance){
                        distance = remainingDistance;
                    }

                    position.x += moveDirection.x * distance;
                    position.y += moveDirection.y * distance;
                    remainingDistance -= distance;

                    if(remainingDistance <= 0.f){
                        position = targetPosition;
                        playerTile = targetTile;
                        isMoving = false;
                    }

                    playerSprite.setPosition(position);
                }
            }

            window.clear(sf::Color::Black);

            window.draw(backgroundSprite);
            if(playerSprite.getPosition().y < soldierSprite.getPosition().y){
                window.draw(playerSprite);
                window.draw(soldierSprite);
            }else{
                window.draw(soldierSprite);
                window.draw(playerSprite);
            }
            window.draw(foregroundSprite);
            if(messageOpen){
                window.draw(messageBox);
                window.draw(messageText);
            }
            window.display();
        }

        return 0;
    }catch(const std::exception& error){
        std::cerr << error.what() << '\n';
        return 1;
    }
}
