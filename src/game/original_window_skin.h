#pragma once
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <algorithm>
#include <stdexcept>

// RPG2000 System atlas. Rectangle coordinates follow the target's current view;
// scale converts its logical border pixels to those coordinates. No UI state lives here.
class OriginalWindowSkin {
public:
    OriginalWindowSkin(const sf::Image& image, sf::Color transparentColor) {
        if(image.getSize()!=sf::Vector2u{160,80}){throw std::runtime_error("Invalid System atlas dimensions");}
        auto foreground=image;foreground.createMaskFromColor(transparentColor);
        if(!background_.loadFromImage(image)||!foreground_.loadFromImage(foreground)){
            throw std::runtime_error("Cannot prepare System window skin");
        }
    }
    void drawWindow(sf::RenderTarget& target,sf::FloatRect rect,float scale=1.f,bool stretch=true,float visibleHeight=-1.f) const {
        if(rect.size.x<16*scale||rect.size.y<16*scale||scale<=0){throw std::runtime_error("Invalid System window rectangle");}
        const float shown=visibleHeight<0?rect.size.y:std::clamp(visibleHeight,0.f,rect.size.y);
        if(shown==0){return;}
        const auto saved=target.getView();auto clipped=saved;
        // Opening/closing reveals the original full-size background from its centre.
        const auto size=target.getSize();const auto a=target.mapCoordsToPixel({rect.position.x,rect.position.y+(rect.size.y-shown)/2});
        const auto b=target.mapCoordsToPixel({rect.position.x+rect.size.x,rect.position.y+(rect.size.y+shown)/2});
        clipped.setScissor({{float(a.x)/size.x,float(a.y)/size.y},{float(b.x-a.x)/size.x,float(b.y-a.y)/size.y}});target.setView(clipped);
        if(stretch){blit(target,background_,{{0,0},{32,32}},rect);}
        else{tile(target,background_,{{0,0},{32,32}},rect,scale);}
        target.setView(saved);
        rect.position.y+=(rect.size.y-shown)/2;rect.size.y=shown;
        if(shown<16*scale){
            const int half=int(shown/(2*scale));const float h=half*scale;
            if(half==0){return;}
            for(bool bottom:{false,true}){
                const int y=bottom?32-half:0;const float dy=rect.position.y+(bottom?shown-h:0);
                blit(target,foreground_,{{32,y},{8,half}},{{rect.position.x,dy},{8*scale,h}});
                tile(target,foreground_,{{40,y},{16,half}},{{rect.position.x+8*scale,dy},{rect.size.x-16*scale,h}},scale);
                blit(target,foreground_,{{56,y},{8,half}},{{rect.position.x+rect.size.x-8*scale,dy},{8*scale,h}});
            }
            return;
        }
        nineSlice(target,32,rect,scale,false);
    }
    void drawCursor(sf::RenderTarget& target,sf::FloatRect rect,unsigned frame,float scale=1.f) const {
        if(rect.size.x<=4*scale||rect.size.y<=4*scale||scale<=0){return;}
        nineSlice(target,frame%21<=10?64:96,rect,scale,true);
    }
    void drawScrollArrow(sf::RenderTarget& target,sf::Vector2f position,bool down,float scale=1.f) const {
        blit(target,foreground_,{{40,down?16:8},{16,8}},{position,{16*scale,8*scale}});
    }
    void drawPause(sf::RenderTarget& target,sf::Vector2f position,unsigned frame,float scale=1.f) const {
        if(frame%40<20){blit(target,foreground_,{{40,16},{16,8}},{position,{16*scale,8*scale}});}
    }
private:
    static void blit(sf::RenderTarget& target,const sf::Texture& image,sf::IntRect source,sf::FloatRect destination){
        if(destination.size.x<=0||destination.size.y<=0){return;}
        sf::Sprite sprite(image);sprite.setTextureRect(source);sprite.setPosition(destination.position);
        sprite.setScale({destination.size.x/source.size.x,destination.size.y/source.size.y});target.draw(sprite);
    }
    static void tile(sf::RenderTarget& target,const sf::Texture& image,sf::IntRect source,sf::FloatRect destination,float scale){
        for(float y=0;y<destination.size.y;y+=source.size.y*scale){for(float x=0;x<destination.size.x;x+=source.size.x*scale){
            auto part=source;part.size.x=std::min(source.size.x,int((destination.size.x-x)/scale+.001f));
            part.size.y=std::min(source.size.y,int((destination.size.y-y)/scale+.001f));
            if(part.size.x>0&&part.size.y>0){blit(target,image,part,{destination.position+sf::Vector2f{x,y},{part.size.x*scale,part.size.y*scale}});}
        }}
    }
    void nineSlice(sf::RenderTarget& target,int sourceX,sf::FloatRect rect,float scale,bool fill) const {
        const float b=8*scale,w=rect.size.x,h=rect.size.y;const auto p=rect.position;
        tile(target,foreground_,{{sourceX+8,0},{16,8}},{p+sf::Vector2f{b,0},{w-2*b,b}},scale);
        tile(target,foreground_,{{sourceX+8,24},{16,8}},{p+sf::Vector2f{b,h-b},{w-2*b,b}},scale);
        tile(target,foreground_,{{sourceX,8},{8,16}},{p+sf::Vector2f{0,b},{b,h-2*b}},scale);
        tile(target,foreground_,{{sourceX+24,8},{8,16}},{p+sf::Vector2f{w-b,b},{b,h-2*b}},scale);
        // Narrow numeric cursors overlap their corner tiles in the original draw order.
        blit(target,foreground_,{{sourceX,0},{8,8}},{p,{b,b}});
        blit(target,foreground_,{{sourceX+24,0},{8,8}},{p+sf::Vector2f{w-b,0},{b,b}});
        blit(target,foreground_,{{sourceX,24},{8,8}},{p+sf::Vector2f{0,h-b},{b,b}});
        blit(target,foreground_,{{sourceX+24,24},{8,8}},{p+sf::Vector2f{w-b,h-b},{b,b}});
        if(fill){tile(target,foreground_,{{sourceX+8,8},{16,16}},{p+sf::Vector2f{b,b},{w-2*b,h-2*b}},scale);}
    }
    sf::Texture background_,foreground_;
};
