#pragma once
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Text.hpp>
#include <algorithm>
#include <cmath>

// Pixel coordinates; the caller keeps the original logical advance and clipping.
inline void drawTextCell(sf::RenderTarget& target,const sf::Font& font,char32_t character,
                         sf::Vector2f position,float cellWidth,float scale,sf::Color color){
    if(character==U' '||character==U'　'){return;}
    const auto size=unsigned(std::max(1.f,std::round(12*scale)));
    const float outline=std::max(1.f,scale/3.f);
    sf::Text glyph(font,sf::String(character),size);
    glyph.setFillColor(color);glyph.setOutlineColor(sf::Color::Black);glyph.setOutlineThickness(outline);
    const auto bounds=glyph.getLocalBounds();
    if(bounds.size.x<=0){return;}
    // Fit all ink, including bearings/outline, without changing height or raster size.
    const float available=std::max(1.f,cellWidth-1.f);
    const float horizontal=std::min(1.f,available/bounds.size.x);
    glyph.setScale({horizontal,1.f});
    const float baseline=-float(size)-font.getGlyph(U'国',size,false,outline).bounds.position.y;
    glyph.setPosition({std::round(position.x)+(available-bounds.size.x*horizontal)/2-bounds.position.x*horizontal,
                       std::round(position.y+baseline)});
    target.draw(glyph);
}
