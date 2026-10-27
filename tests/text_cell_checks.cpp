#include "dialogue_renderer.h"
#include "combat_presentation.h"
#include "content_hash.h"
#include "scene_renderer.h"
#include <SFML/Graphics.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void need(bool ok,const std::string& message){if(!ok){throw std::runtime_error(message);}}
struct Ink{int left=100000,right=-1,top=100000,bottom=-1,pixels=0;};
Ink ink(const sf::Image& image){Ink result;for(unsigned y=0;y<image.getSize().y;++y){for(unsigned x=0;x<image.getSize().x;++x){if(image.getPixel({x,y}).a){result.left=std::min(result.left,int(x));result.right=std::max(result.right,int(x));result.top=std::min(result.top,int(y));result.bottom=std::max(result.bottom,int(y));++result.pixels;}}}return result;}
}
int main(int argc,char** argv){try{
    need(argc==3||argc==4,"text_cell_checks source-sample.json output [--native]");const std::filesystem::path output=argv[2];std::filesystem::create_directories(output);
    nlohmann::json sample;{std::ifstream input(argv[1]);input>>sample;}
    const std::filesystem::path package=sample.at("package");
    for(const auto& [file,sha]:sample.at("files").items()){std::ifstream input(package/file,std::ios::binary);need(bool(input)&&contentSha256(readContentBytes(input))==sha.get<std::string>(),"Source sample changed: "+file);}
    sf::Font font;need(font.openFromFile(package/sample.at("font").get<std::string>()),"Actual font missing");DialogueRenderer renderer(font);
    // Existing five-image source fixture reaches the real title caller without loading every map/audio.
    CombatContent content;content.directory=package;content.media=sample.at("title_media");
    GameApplication game(content,"title");CombatPresentation menu(content,font);for(int n=0;n<43;++n){game.update({});}menu.update(game.takePresentation(),false);
    const auto originalTerms=content.media.at("terms");
    auto palette=menu.palette();renderer.setMedia(menu.images(),palette);


    for(const auto size:{sf::Vector2u{960,720},sf::Vector2u{1000,600},sf::Vector2u{400,360}}){
        sf::RenderTexture target(size);const auto view=makeSceneView(size,{320,240});target.setView(view);
        const auto viewport=target.getViewport(view);const float scale=float(viewport.size.y)/240;
        DialogueView text;text.phase=DialoguePhase::AwaitingInput;text.display={true,2,true,false};text.showPause=false;
        for(char32_t c:U"WMNifj()国中09，。"){
            if(c==0){continue;}text.lines={{c}};target.clear(sf::Color::Transparent);renderer.draw(target,text);target.display();const auto image=target.getTexture().copyToImage();const auto bounds=ink(image);
            const int x=int(std::round(viewport.position.x+8*scale));const int width=int(std::round((c<256?6:12)*scale));
            need(bounds.pixels>0,"Empty glyph "+std::to_string(unsigned(c)));
            if(c==U'W'){need(image.saveToFile(output/("W-"+std::to_string(size.x)+".png")),"Capture");}
            need(bounds.left>=x&&bounds.right<x+width,"Visible glyph including outline crosses its original cell: U+"+std::to_string(unsigned(c))+" bounds="+std::to_string(bounds.left)+".."+std::to_string(bounds.right)+" cell="+std::to_string(x)+".."+std::to_string(x+width-1));
            if(c==U'W'){need(bounds.bottom-bounds.top>=int(7*scale),"Width fitting shrank the font height");}
        }
        // Numeric input retains its separate 12px advance; blanks retain half/full logical cells.
        text.cursor=DialogueCursor{0,3,1,true};text.lines={U"    "};target.clear(sf::Color::Transparent);renderer.draw(target,text);target.display();const auto emptyNumeric=target.getTexture().copyToImage();
        text.lines={U"0389"};target.clear(sf::Color::Transparent);renderer.draw(target,text);target.display();
        const auto numeric=target.getTexture().copyToImage();
        need(numeric.saveToFile(output/("number-"+std::to_string(size.x)+".png")),"Numeric capture");
        for(int digit=0;digit<4;++digit){bool visible=false;const int left=int(std::round(viewport.position.x+(20+12*digit)*scale));
            for(int y=int(viewport.position.y+170*scale);y<int(viewport.position.y+186*scale);++y){for(int x=left;x<int(left+6*scale);++x){const sf::Vector2u point{unsigned(x),unsigned(y)};visible|=numeric.getPixel(point)!=emptyNumeric.getPixel(point);}}need(visible,"Numeric place lost its glyph");}
        text.cursor.reset();text.lines={U"  W　M Nifj 中文09，。 ",U"NEW GAME   LOAD  GAME   EXIT"};
        text.colors={std::vector<int>(text.lines[0].size(),4),std::vector<int>(text.lines[1].size(),0)};
        text.exfontImage="g9-message-exfont";text.exfontIndices={std::vector<int>(text.lines[0].size(),-1),std::vector<int>(text.lines[1].size(),-1)};text.exfontIndices[0].back()=32;
        target.clear(sf::Color::Transparent);renderer.draw(target,text);target.display();
        need(target.getTexture().copyToImage().saveToFile(output/("mixed-"+std::to_string(size.x)+".png")),"Mixed capture");
        // Difference from an actual blank title isolates visible text from source background/window/cursor.
        for(auto& term:content.media["terms"]){term["display"]=std::string(term.at("display").get<std::string>().size(),' ');}
        target.clear();menu.draw(target,game);target.display();const auto blank=target.getTexture().copyToImage();
        content.media["terms"]["115"]["display"]="  W        ";target.clear();menu.draw(target,game);target.display();const auto letter=target.getTexture().copyToImage();
        Ink menuInk;for(unsigned y=0;y<size.y;++y){for(unsigned x=0;x<size.x;++x){if(letter.getPixel({x,y})!=blank.getPixel({x,y})){menuInk.left=std::min(menuInk.left,int(x));menuInk.right=std::max(menuInk.right,int(x));++menuInk.pixels;}}}
        const int first=target.mapCoordsToPixel({127,172},view).x+int(std::round(12*scale));
        need(menuInk.pixels>0&&menuInk.left>=first&&menuInk.right<first+int(std::round(6*scale)),"Actual menu title W crosses its original slot or lost leading spaces: ink="+std::to_string(menuInk.left)+".."+std::to_string(menuInk.right)+" expected="+std::to_string(first)+".."+std::to_string(first+int(std::round(6*scale))-1));
        content.media["terms"]=originalTerms;target.clear();menu.draw(target,game);target.display();
        need(target.getTexture().copyToImage().saveToFile(output/("title-"+std::to_string(size.x)+".png")),"Title capture");
        if(argc==4){
            sf::RenderWindow window(sf::VideoMode(size),"Nobi 08 isolated text cells");
            for(int scene=0;scene<2;++scene){
                for(int frame=0;frame<4;++frame){while(window.pollEvent()){}window.setView(makeSceneView(window.getSize(),{320,240}));window.clear(sf::Color(18,39,61));if(scene==0){menu.draw(window,game);}else{renderer.draw(window,text);}window.display();}
                sf::Texture screen;need(screen.resize(window.getSize()),"Native texture");screen.update(window);
                need(screen.copyToImage().saveToFile(output/(std::string(scene==0?"native-title-":"native-dialogue-")+std::to_string(size.x)+".png")),"Native capture");
            }
            window.close();
        }
    }
    std::cout<<"Actual DialogueRenderer/Menu title outlined ink fits original cells; spaces, half/full mix, colors, ExFont and numeric spacing at three offscreen/native sizes PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
