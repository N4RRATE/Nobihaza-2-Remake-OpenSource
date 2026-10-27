// Authored by Codex for toolchain verification, not learner/game implementation.
#include <SFML/Audio/Listener.hpp>
#include <SFML/Graphics.hpp>
#include <array>
#include <iostream>
#include <span>
#include <string_view>

static_assert(__cplusplus >= 202002L, "This project requires C++20");

int main(int argc, char* argv[])
{
    const bool smokeMode = argc > 1 && std::string_view(argv[1]) == "--smoke";
    const std::array<int, 2> dimensions{640, 360};
    const std::span<const int> size(dimensions);
    std::cout << "C++=" << __cplusplus << "; SFML=" << SFML_VERSION_MAJOR << '.'
              << SFML_VERSION_MINOR << '.' << SFML_VERSION_PATCH
              << "; audio volume=" << sf::Listener::getGlobalVolume() << std::endl;

    sf::RenderWindow window(sf::VideoMode({static_cast<unsigned>(size[0]),
                                          static_cast<unsigned>(size[1])}),
                            "Nobihaza - Toolchain Probe");
    window.setFramerateLimit(60);
    sf::CircleShape marker(40.f);
    marker.setFillColor(sf::Color(90, 200, 160));
    marker.setPosition({280.f, 140.f});

    int frameCount = 0;
    while (window.isOpen())
    {
        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();
            if (const auto* key = event->getIf<sf::Event::KeyPressed>();
                key && key->code == sf::Keyboard::Key::Escape)
                window.close();
        }
        if (!window.isOpen())
            break;
        ++frameCount; // BREAKPOINT: inspect frameCount, step, then continue.
        window.clear(sf::Color(24, 30, 42));
        window.draw(marker);
        window.display();
        if (smokeMode && frameCount >= 120)
            window.close();
    }
    std::cout << "Closed after " << frameCount << " frames" << std::endl;
}
