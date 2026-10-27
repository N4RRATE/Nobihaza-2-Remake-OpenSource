#pragma once
#include "dialogue.h"
#include "original_window_skin.h"
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Image.hpp>
#include <map>

// Font is owned by Game and must outlive this renderer. Drawing never advances dialogue.
class DialogueRenderer{
public:
    explicit DialogueRenderer(const sf::Font& font);
    void draw(sf::RenderTarget& target,const DialogueView& view) const;
    // Without an identity, legacy callers bind a fixed palette/skin snapshot.
    // Live presentation callers borrow all three values and rebind after replacement.
    void setMedia(const std::map<std::string,sf::Texture>& images,const sf::Image& palette,const std::string* systemImage=nullptr);
private:
    const sf::Font& font_;
    const std::map<std::string,sf::Texture>* images_{};
    const sf::Image* palette_{};
    const std::string* systemImage_{};
    std::map<std::string,OriginalWindowSkin> skins_;
    std::optional<OriginalWindowSkin> legacySkin_;
};
