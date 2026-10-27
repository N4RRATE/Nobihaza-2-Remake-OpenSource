#include "combat_presentation.h"
#include "text_cell.h"
#include <algorithm>
#include <cmath>

sf::Color CombatPresentation::uiColor(int index) const{
    return palette_.getPixel({unsigned(index%10*16+8),unsigned(index/10*16+55)});
}
void CombatPresentation::drawUiWindow(sf::RenderTarget& target,sf::FloatRect rect) const{
    windowSkins_.at(systemImage_).drawWindow(target,rect);
}
void CombatPresentation::drawUiCursor(sf::RenderTarget& target,sf::FloatRect rect) const{
    windowSkins_.at(systemImage_).drawCursor(target,rect,uiFrame_);
}
void CombatPresentation::drawUiLine(sf::RenderTarget& target,const std::string& value,sf::FloatRect rect,int color,bool scroll,bool gradient) const{
    const auto saved=target.getView();const auto size=target.getSize();const auto viewport=target.getViewport(saved);
    const float scale=viewport.size.y/saved.getSize().y;
    const auto point=target.mapCoordsToPixel(rect.position);
    sf::View pixels(sf::FloatRect({0,0},{float(size.x),float(size.y)}));
    const float left=std::clamp(float(point.x),0.f,float(size.x)),top=std::clamp(float(point.y),0.f,float(size.y));
    const float right=std::clamp(point.x+rect.size.x*scale,left,float(size.x)),bottom=std::clamp(point.y+rect.size.y*scale,top,float(size.y));
    pixels.setScissor({{left/size.x,top/size.y},{(right-left)/size.x,(bottom-top)/size.y}});target.setView(pixels);
    const auto chars=decodeUiText(value);float width=0;for(char32_t c:chars){width+=messageCharacterWidth(c)*6.f;}
    if(gradient&&width>0){
        // Cache only this viewport's title glyph masks; resizing never keeps old raster scales.
        if(titleTextScale_!=scale||titleTextSystem_!=systemImage_){titleText_.clear();titleTextScale_=scale;titleTextSystem_=systemImage_;}
        const auto key=std::pair{value,color};auto found=titleText_.find(key);
        if(found==titleText_.end()){
            sf::RenderTexture mask({unsigned(std::ceil(width*scale)),unsigned(std::ceil(16*scale))});mask.clear(sf::Color::Transparent);float x=0;
            for(char32_t c:chars){drawTextCell(mask,font_,c,{x,2*scale},messageCharacterWidth(c)*6.f*scale,scale,sf::Color::White);x+=messageCharacterWidth(c)*6.f*scale;}
            mask.display();auto image=mask.getTexture().copyToImage();
            for(int row=0;row<16;++row){const auto shade=palette_.getPixel({unsigned(color%10*16+8),unsigned(color/10*16+48+row)});
                for(unsigned y=unsigned(std::round(row*scale));y<std::min(image.getSize().y,unsigned(std::round((row+1)*scale)));++y){for(unsigned x=0;x<image.getSize().x;++x){auto p=image.getPixel({x,y});p.r=p.r*shade.r/255;p.g=p.g*shade.g/255;p.b=p.b*shade.b/255;image.setPixel({x,y},p);}}
            }
            sf::Texture texture;if(!texture.loadFromImage(image)){throw std::runtime_error("Cannot prepare original title text");}found=titleText_.emplace(key,std::move(texture)).first;
        }
        sf::Sprite text(found->second);text.setPosition({float(point.x),float(point.y)});target.draw(text);target.setView(saved);return;
    }
    float offset=0;
    if(scroll&&width>rect.size.x){const float distance=width-rect.size.x;const unsigned period=120+unsigned(distance*2);const auto frame=uiFrame_%period;offset=std::clamp((float(frame)-60)/2,0.f,distance);}
    float x=point.x-offset*scale;
    for(char32_t c:chars){
        drawTextCell(target,font_,c,{x,point.y+2*scale},messageCharacterWidth(c)*6.f*scale,scale,uiColor(color));
        x+=messageCharacterWidth(c)*6.f*scale;
    }
    target.setView(saved);
}
void CombatPresentation::drawFormalMenu(sf::RenderTarget& target,const GameApplication& game){
    sf::RectangleShape background({320,240});background.setFillColor(sf::Color::Black);target.draw(background);
    const auto v=game.menuView();const auto& state=game.session();
    const auto term=[&](int id)->std::string{return content_.media.at("terms").at(std::to_string(id)).at("display");};
    const auto line=[&](const std::string& text,float x,float y,float width,int color=0,bool scroll=false){drawUiLine(target,text,{{x,y},{width,16}},color,scroll);};
    const auto panel=[&](float x,float y,float w,float h){drawUiWindow(target,{{x,y},{w,h}});};
    const auto actor=[&](float x,float y,bool targetActor=false){
        const auto& face=content_.catalog.at("menu").at("actor_face");const int index=face.at("index");
        sf::Sprite sprite(images_.at(face.at("image").get<std::string>()));sprite.setTextureRect({{index%4*48,index/4*48},{48,48}});sprite.setPosition({x,y});target.draw(sprite);
        const float tx=x+56;line(content_.media.at("ui").at("menu").at("actor_name"),tx,y,80);
        std::string title;for(const auto& effect:content_.catalog.at("menu").at("effect_titles")){if(state.flag(effect.at("flag"))){title=effect.at("text");}}
        line(title,tx+88,y,72);
        line(term(123),tx,y+16,48,1);line(std::to_string(content_.media.at("actors").at("1").at("level").get<int>()),tx+36,y+16,24);
        const auto& conditions=state.conditions();line(conditions.bleedingActive?"出血":conditions.poisoned?"中毒":term(126),tx+48,y+16,64,conditions.bleedingActive||conditions.poisoned?4:0);
        if(targetActor){line(term(124)+" "+std::to_string(state.hp())+"/"+std::to_string(state.maxHp()),tx,y+32,112);line(term(125)+" "+std::to_string(state.tp())+"/"+std::to_string(state.maxTp()),tx,y+48,112);}
        else{line(term(124)+" "+std::to_string(state.hp())+"/"+std::to_string(state.maxHp()),tx,y+32,88);line(term(125)+" "+std::to_string(state.tp())+"/"+std::to_string(state.maxTp()),tx+88,y+32,72);}
    };
    if(v.page==MenuPage::Root){
        panel(0,0,88,96);panel(88,0,232,240);panel(0,208,88,32);actor(96,8);
        for(std::size_t i=0;i<v.rows.size();++i){if(int(i)==v.selection){drawUiCursor(target,{{4,8+float(i)*16},{80,16}});}line(v.rows[i].text,8,8+i*16,72,v.rows[i].enabled?0:3);}
        const auto weapon=std::to_string(state.equipped(1));const bool infinite=content_.catalog.at("weapon_bindings").contains(weapon)&&content_.catalog.at("weapon_bindings").at(weapon).value("infinite",false);
        line((infinite?std::string("∞"):std::to_string(state.magazine(state.equipped(1))))+" "+term(95),8,216,72);return;
    }
    if(v.page==MenuPage::TitleConfirm){
        const float width=std::clamp(float(decodeUiText(v.title).size())*12+16,96.f,320.f);panel((320-width)/2,72,width,32);line(v.title,(320-width)/2+8,80,width-16);
        panel(124,120,72,48);drawUiCursor(target,{{128,128+float(v.selection)*16},{64,16}});for(int i=0;i<2;++i){line(v.rows[i].text,132,128+16*i,56);}return;
    }
    if(v.page==MenuPage::Notice){panel(0,64,320,112);auto chars=decodeUiText(v.notice);for(int i=0;i<5&&!chars.empty();++i){sf::String chunk(chars.substr(0,25));auto bytes=chunk.toUtf8();line(std::string(bytes.begin(),bytes.end()),8,72+16*i,304);chars.erase(0,std::min<std::size_t>(25,chars.size()));}return;}
    if(v.page==MenuPage::Target){panel(0,0,136,32);line(v.title,8,8,120);panel(0,32,136,32);line("× "+std::to_string(state.count(v.rows.at(0).id)),8,40,120);panel(136,0,184,240);drawUiCursor(target,{{140,8},{176,80}});actor(144,8,true);return;}
    panel(0,0,320,32);line(v.description,8,8,304,0,true);
    if(v.currentStats){
        panel(0,32,124,96);panel(124,32,196,96);panel(0,128,320,112);
        line(content_.media.at("ui").at("menu").at("actor_name"),8,40,108);
        const auto a=*v.currentStats,b=v.previewStats.value_or(a);const int before[]{a.attack,a.defense,a.spirit,a.agility},after[]{b.attack,b.defense,b.spirit,b.agility};
        for(int i=0;i<4;++i){line(term(132+i),8,56+i*16,60,1);line(std::to_string(before[i]),68,56+i*16,v.previewStats?20:48);if(v.previewStats){line("→",88,56+i*16,12,1);line(std::to_string(after[i]),100,56+i*16,20,after[i]>before[i]?2:after[i]<before[i]?4:0);}}
        for(std::size_t i=0;i<v.equipmentSlots.size();++i){const float y=40+i*16;if(v.page==MenuPage::Slots&&int(i)==v.selection){drawUiCursor(target,{{128,y},{188,16}});}const int slot=v.equipmentSlots[i].id;line(term(135+slot),132,y,60,1);const int item=state.equipped(slot);line(item?content_.items.at(item).name:"",192,y,120);}
        if(v.page==MenuPage::Slots){return;}
    }
    const bool equipment=v.page==MenuPage::Equipment,skills=v.page==MenuPage::Skills;
    const int top=equipment?128:skills?64:32;
    if(!equipment){panel(0,top,320,240-top);}
    if(skills){panel(0,32,320,32);line(content_.media.at("ui").at("menu").at("actor_name"),8,40,112);line(term(124)+" "+std::to_string(state.hp())+"/"+std::to_string(state.maxHp()),120,40,96);line(term(125)+" "+std::to_string(state.tp())+"/"+std::to_string(state.maxTp()),224,40,88);}
    const int columns=v.columns,visibleRows=(240-top-16)/16,selectedRow=v.selection/columns;
    const int first=std::max(0,selectedRow-visibleRows+1)*columns;const float cell=320.f/columns;
    for(int i=first;i<std::min(int(v.rows.size()),first+visibleRows*columns);++i){const auto& row=v.rows[i];const float x=i%columns*cell,y=top+8+(i-first)/columns*16;
        if(i==v.selection){drawUiCursor(target,{{x+4,y},{cell-8,16}});}const int color=row.enabled?0:3;
        if((v.page==MenuPage::Items||equipment)&&row.id){line(content_.items.at(row.id).name,x+8,y,cell-48,color);line("× "+std::to_string(state.count(row.id)),x+cell-40,y,32,color);}
        else{line(row.text,x+8,y,cell-16,color);}
    }
    if(v.rows.empty()){line("－",8,top+8,304,3);}
    const auto arrow=[&](bool down,float y){windowSkins_.at(systemImage_).drawScrollArrow(target,{152,y},down);};
    if(first>0){arrow(false,float(top));}if(first+visibleRows*columns<int(v.rows.size())){arrow(true,232);}
}
