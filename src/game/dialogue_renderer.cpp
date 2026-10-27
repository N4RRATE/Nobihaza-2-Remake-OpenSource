#include "dialogue_renderer.h"
#include "text_cell.h"
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

DialogueRenderer::DialogueRenderer(const sf::Font& font):font_(font){}
void DialogueRenderer::setMedia(const std::map<std::string,sf::Texture>& images,const sf::Image& palette,const std::string* systemImage){
    // CN1.00's eight System files use this palette[0]; no used nonzero index shares its RGB.
    // A new source palette must supply its own audited key instead of extending this assumption.
    constexpr sf::Color transparentColor(255,156,0);
    std::map<std::string,OriginalWindowSkin> prepared;std::optional<OriginalWindowSkin> legacy;
    if(systemImage){
        if(!images.contains(*systemImage)){throw std::runtime_error("Missing System atlas: "+*systemImage);}
        for(const auto& [id,image]:images){if(image.getSize()==sf::Vector2u{160,80}){prepared.try_emplace(id,image.copyToImage(),transparentColor);}}
        if(!prepared.contains(*systemImage)){throw std::runtime_error("Invalid System atlas dimensions: "+*systemImage);}
    }else{legacy.emplace(palette,transparentColor);}
    images_=&images;palette_=&palette;systemImage_=systemImage;skins_=std::move(prepared);legacySkin_=std::move(legacy);
}
void DialogueRenderer::draw(sf::RenderTarget& target,const DialogueView& view) const{
    if(view.phase == DialoguePhase::Hidden || view.phase == DialoguePhase::Cooldown){return;}
    const OriginalWindowSkin* skin=legacySkin_?&*legacySkin_:nullptr;
    if(systemImage_){
        const auto found=skins_.find(*systemImage_);
        if(found==skins_.end()){throw std::runtime_error("Missing prepared System atlas: "+*systemImage_);}
        skin=&found->second;
    }
    const sf::Texture* exfont = nullptr;
    if(!view.exfontImage.empty()){
        if(!images_ || !images_->contains(view.exfontImage)){throw std::runtime_error("Missing ExFont atlas: "+view.exfontImage);}
        exfont=&images_->at(view.exfontImage);
        if(exfont->getSize()!=sf::Vector2u{156,48}){throw std::runtime_error("Invalid ExFont atlas dimensions: "+view.exfontImage);}
    }
    if(!view.exfontIndices.empty()){
        if(view.exfontIndices.size()>view.lines.size()){throw std::runtime_error("Invalid ExFont display lines");}
        for(std::size_t line=0;line<view.exfontIndices.size();++line){
            if(view.exfontIndices[line].size()!=view.lines[line].size()){throw std::runtime_error("Invalid ExFont display glyphs");}
            for(int index:view.exfontIndices[line]){
                if(index < -1 || index >= 52 || (index >= 0 && !exfont)){throw std::runtime_error("Invalid ExFont display index or atlas");}
            }
        }
    }
    const auto worldView = target.getView();
    const auto viewport = target.getViewport(worldView);
    const float scale = static_cast<float>(viewport.size.y) / worldView.getSize().y;
    if(scale <= 0){return;}
    const float boxTop = (worldView.getSize().y - 80.f) * view.display.position / 2.f;
    const float left = static_cast<float>(viewport.position.x);
    const float top = static_cast<float>(viewport.position.y) + boxTop * scale;
    const auto targetSize=target.getSize();
    target.setView(sf::View(sf::FloatRect({0.f,0.f},{float(targetSize.x),float(targetSize.y)})));
    if(!view.display.transparent){
        if(skin){skin->drawWindow(target,{{left,top},{320.f*scale,80.f*scale}},scale,true,view.visibleHeight*scale);}
        else{
            // The retained first_room experiment has no System media binding.
            sf::RectangleShape panel({static_cast<float>(viewport.size.x),80.f * scale});
            panel.setPosition({left,top});panel.setFillColor(sf::Color(0,0,0,220));target.draw(panel);
        }
    }
    if(view.phase==DialoguePhase::Opening||view.phase==DialoguePhase::Closing){target.setView(worldView);return;}
    if(!view.faceImage.empty()&&images_){
        sf::Sprite face(images_->at(view.faceImage));
        face.setTextureRect({{(view.faceIndex%4)*48,(view.faceIndex/4)*48},{48,48}});
        face.setPosition({left+(view.faceRight?256.f:16.f)*scale,top+16.f*scale});
        face.setScale({scale,scale});if(view.faceFlipped){face.setOrigin({48,0});face.setScale({-scale,scale});}target.draw(face);
    }
    if(view.cursor){
        const bool numeric=view.cursor->numeric;const float faceInset=view.faceImage.empty()?0.f:72.f;
        const float x=numeric?16.f+view.cursor->column*12.f:10.f+(view.faceRight?0.f:faceInset);
        const float width=numeric?14.f:300.f-faceInset;
        const sf::FloatRect rect({left+x*scale,top+(8.f+view.cursor->line*16.f)*scale},{width*scale,16.f*scale});
        if(skin){skin->drawCursor(target,rect,view.animationFrame,scale);}
        else{sf::RectangleShape cursor(rect.size);cursor.setPosition(rect.position);cursor.setFillColor(sf::Color(72,104,164,180));target.draw(cursor);}
    }
    if(skin&&view.showPause&&!view.cursor){skin->drawPause(target,{left+152.f*scale,top+72.f*scale},view.animationFrame,scale);}
    auto textView = target.getView();
    // Match the original 320x80 window's 304x64 contents bitmap, including transparent pages.
    textView.setScissor({{(left+8.f*scale)/targetSize.x,(top+8.f*scale)/targetSize.y},
                         {304.f*scale/targetSize.x,64.f*scale/targetSize.y}});
    target.setView(textView);
    for(std::size_t line=0;line<view.lines.size();++line){
        const bool numeric=view.cursor&&view.cursor->numeric;
        const bool choice=view.cursor&&!numeric&&int(line)>=view.cursor->firstChoiceLine;
        float x = left + ((numeric?20.f:(view.faceImage.empty()||view.faceRight?8.f:80.f))+(choice?12.f:0.f)) * scale;
        const float y = top + (10.f + 16.f * line) * scale;
        std::size_t glyphIndex=0;
        for(auto c : view.lines[line]){
            const int index=line<view.exfontIndices.size()?view.exfontIndices[line][glyphIndex]:-1;
            const int color=line<view.colors.size()&&glyphIndex<view.colors[line].size()?view.colors[line][glyphIndex]:0;
            const auto fill=palette_?palette_->getPixel({static_cast<unsigned>((color%10)*16+8),static_cast<unsigned>((color/10)*16+55)}):sf::Color::White;
            if(index>=0){
                sf::Sprite glyph(*exfont);glyph.setTextureRect({{index%13*12,index/13*12},{12,12}});
                glyph.setScale({scale,scale});glyph.setPosition({std::round(x)+scale,std::round(y)+scale});
                glyph.setColor(sf::Color::Black);target.draw(glyph);
                glyph.setPosition({std::round(x),std::round(y)});glyph.setColor(fill);target.draw(glyph);
            }else{
                drawTextCell(target,font_,c,{x,y},messageCharacterWidth(c)*6.f*scale,scale,fill);
            }
            ++glyphIndex;
            x += (numeric?12.f:(index>=0?2:messageCharacterWidth(c))*6.f) * scale;
        }
    }
    target.setView(worldView);
}
