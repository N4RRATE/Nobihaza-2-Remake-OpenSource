#include "combat_content.h"
#include "scene_renderer.h"
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Image.hpp>
#include <iostream>
#include <stdexcept>
void require(bool b,const char* m){if(!b){throw std::runtime_error(m);}}
int main(int argc,char** argv){
    if(argc!=2){return 2;}
    try{
        const auto c=loadCombatContent(argv[1]);auto def=c.maps.at(81);def.objects.clear();
        auto selectedSprites=*def.sprites;std::erase_if(selectedSprites,[&](const auto& pair){return pair.first!=def.player.spriteId;});
        def.sprites=std::make_shared<const SpriteDefinitions>(std::move(selectedSprites));
        ExplorationScene scene(def);SceneRenderer renderer(def);
        sf::RenderTexture target({static_cast<unsigned>(def.width*16),static_cast<unsigned>(def.height*16)});
        sf::Image background;require(background.loadFromFile(def.backgroundPath),"Read source upper layer");
        sf::Image foreground;require(foreground.loadFromFile(def.foregroundPath),"Read foreground");
        int checked=0;
        for(int frame=0;frame<24;++frame){
            target.clear(sf::Color::Transparent);renderer.draw(target,scene,{},false);target.display();auto output=target.getTexture().copyToImage();
            // CN1.00 Map81 (4,7): C lower3000 under non-star upper10130.
            for(unsigned y=7*16;y<8*16;++y){for(unsigned x=4*16;x<5*16;++x){
                const auto top=background.getPixel({x,y});
                if(top.a==255&&foreground.getPixel({x,y}).a==0){require(output.getPixel({x,y})==top,"Animated lower covered static upper");++checked;}
            }}
            scene.advanceClock();
        }
        require(checked>0,"Real layered tile sample checked");std::cout<<"Map81 upper preserved across C animation: "<<checked<<" pixels\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
