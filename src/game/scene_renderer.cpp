#include "scene_renderer.h"

#include <SFML/Graphics/Image.hpp>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <tuple>

namespace{
    std::uint8_t toneChannel(std::uint8_t value, int percent){
        if(percent == 100){
            return value;
        }
        // Static entry only: RGB 0..100%, saturation 100%.
        // Pinned compatibility semantics are documented in learning/production/12-completion.md.
        const int level = percent * 128 / 100;
        return static_cast<std::uint8_t>(2 * level * value / 255);
    }

    void loadSceneTexture(sf::Texture& texture, const std::filesystem::path& path,
                          sf::Vector2u expectedSize, const ScreenToneDefinition& tone,
                          const std::string& field){
        sf::Image image;
        if(!image.loadFromFile(path)){
            throw std::runtime_error(field + ": cannot load " + path.string());
        }
        if(image.getSize() != expectedSize){
            throw std::runtime_error(field + ": image size does not match definition: " + path.string());
        }
        // Modify a runtime copy only. Source PNGs and UI font are never tinted.
        if(tone.redPercent!=100||tone.greenPercent!=100||tone.bluePercent!=100){
            for(unsigned int y = 0; y < expectedSize.y; ++y){
                for(unsigned int x = 0; x < expectedSize.x; ++x){
                    auto pixel = image.getPixel({x, y});
                    pixel.r = toneChannel(pixel.r, tone.redPercent);
                    pixel.g = toneChannel(pixel.g, tone.greenPercent);
                    pixel.b = toneChannel(pixel.b, tone.bluePercent);
                    image.setPixel({x, y}, pixel);
                }
            }
        }
        if(!texture.loadFromImage(image)){
            throw std::runtime_error(field + ": cannot upload texture: " + path.string());
        }
        texture.setSmooth(false);
    }

    const SpriteFrameDefinition& findFrame(const SpriteDefinition& graphic,
                                           const std::string& direction, int column,
                                           const std::string& id){
        for(const auto& frame : *graphic.frames){
            if(frame.direction == direction && frame.column == column){
                return frame;
            }
        }
        throw std::runtime_error("/sprites/" + id + "/frames: missing direction/column");
    }
}

sf::View makeSceneView(sf::Vector2u targetSize, sf::Vector2f sceneSize){
    if(targetSize.x == 0 || targetSize.y == 0 || sceneSize.x <= 0.f || sceneSize.y <= 0.f){
        throw std::runtime_error("Cannot create scene view with zero or negative dimensions");
    }
    const float scale = std::min(targetSize.x / sceneSize.x, targetSize.y / sceneSize.y);
    const float width = sceneSize.x * scale / targetSize.x;
    const float height = sceneSize.y * scale / targetSize.y;
    sf::View view(sf::FloatRect({0.f, 0.f}, sceneSize));
    view.setViewport(sf::FloatRect({(1.f - width) / 2.f, (1.f - height) / 2.f}, {width, height}));
    return view;
}

const sf::Texture& SceneSpriteTextures::load(const SpriteDefinition& graphic,const ScreenToneDefinition& tone){
    const Key key{graphic.texturePath,graphic.textureWidth,graphic.textureHeight,tone.redPercent,tone.greenPercent,tone.bluePercent};
    if(const auto found=textures_.find(key);found!=textures_.end()){return found->second;}
    sf::Texture candidate;
    loadSceneTexture(candidate,graphic.texturePath,{static_cast<unsigned>(graphic.textureWidth),static_cast<unsigned>(graphic.textureHeight)},tone,"/sprites");
    return textures_.emplace(key,std::move(candidate)).first->second;
}

SceneRenderer::SceneRenderer(const SceneDefinition& scene,std::shared_ptr<SceneSpriteTextures> sprites)
    : spriteTextures_(std::move(sprites)),backgroundSprite_(backgroundTexture_),
      foregroundSprite_(foregroundTexture_),
      graphics_(scene.sprites), tileSize_(scene.tileSize),loopX_(scene.loopX),mapWidth_(scene.width*scene.tileSize){
    constexpr auto fragment=R"(uniform sampler2D texture;uniform float saturation;
void main(){vec4 p=texture2D(texture,gl_TexCoord[0].xy);float lum=floor(dot(floor(p.rgb*255.0+0.5),vec3(19595.0,38470.0,7471.0))/65536.0)/255.0;
 gl_FragColor=gl_Color*vec4(clamp(mix(vec3(lum),p.rgb,saturation),0.0,1.0),p.a);})";
    if(!sf::Shader::isAvailable()||!saturationShader_.loadFromMemory(fragment,sf::Shader::Type::Fragment)){throw std::runtime_error("Cannot prepare scene color shader");}
    saturationShader_.setUniform("texture",sf::Shader::CurrentTexture);
    const sf::Vector2u mapSize{
        static_cast<unsigned int>(scene.width * scene.tileSize),
        static_cast<unsigned int>(scene.height * scene.tileSize)
    };
    loadSceneTexture(backgroundTexture_, scene.backgroundPath, mapSize, scene.tone, "/map/background");
    loadSceneTexture(foregroundTexture_, scene.foregroundPath, mapSize, scene.tone, "/map/foreground");
    backgroundSprite_.setTexture(backgroundTexture_, true);
    foregroundSprite_.setTexture(foregroundTexture_, true);

    for(const auto& row:scene.upperTileReplacements){
        if(!row.cellUnderlays.empty()&&row.cellUnderlays.size()!=row.tiles.size()){throw std::runtime_error("Invalid upper replacement cell underlays");}
        for(std::size_t cell=0;cell<(row.cellUnderlays.empty()?std::size_t{1}:row.tiles.size());++cell){
            sf::Image image,upper;const auto& underlay=row.cellUnderlays.empty()?row.underlay:row.cellUnderlays.at(cell);
            if(!image.loadFromFile(underlay)||!upper.loadFromFile(row.image)||image.getSize()!=sf::Vector2u{16,16}||upper.getSize()!=sf::Vector2u{16,16}){throw std::runtime_error("Invalid upper replacement pixels");}
            if(!image.copy(upper,{0,0},sf::IntRect{},true)){throw std::runtime_error("Cannot compose replacement pixels");}
            for(unsigned y=0;y<16;++y){for(unsigned x=0;x<16;++x){auto p=image.getPixel({x,y});p.r=toneChannel(p.r,scene.tone.redPercent);p.g=toneChannel(p.g,scene.tone.greenPercent);p.b=toneChannel(p.b,scene.tone.bluePercent);image.setPixel({x,y},p);}}
            if(!replacementTextures_[{row.fromIndex,cell}].loadFromImage(image)){throw std::runtime_error("Cannot prepare replacement tile");}
        }
    }
    if(scene.lowerTiles){for(const auto& [index,file]:scene.lowerTiles->images){loadSceneTexture(lowerReplacementTextures_[index],file,{16,16},scene.tone,"lower replacement tile");}}
    animatedTiles_=scene.animatedTiles;
    for(const auto& tile:animatedTiles_){loadSceneTexture(tileTextures_[tile.id],tile.file,{static_cast<unsigned>(tile.width),static_cast<unsigned>(tile.height)},scene.tone,"animated tile");}
    for(const auto& [id, graphic] : *scene.sprites){
        try{characterTextures_.emplace(id,&spriteTextures_->load(graphic,scene.tone));}
        catch(const std::exception& e){throw std::runtime_error("/sprites/"+id+"/file: "+e.what());}
    }

}

void SceneRenderer::draw(sf::RenderTarget& target, const ExplorationScene& scene,const std::map<std::string,sf::Color>& flashes,bool showPlayer,float saturation) const{
    const float gray=std::floor(saturation*128.f/100.f);saturationShader_.setUniform("saturation",gray>128?(1024+(gray-128)*16)/1024:gray/128);
    sf::RenderStates states;states.shader=&saturationShader_;
    std::vector<CharacterSprite> characters;
    const auto addCharacter = [&](const CharacterState& character){
        if(!character.active || !character.spriteId){ return; }
        const auto& id = *character.spriteId;
        const auto& graphic = graphics_->at(id);
        const auto& frame = findFrame(graphic, std::string(directionName(character.graphicFacing())),
                                     character.animation.column(),id);
        sf::Sprite sprite(*characterTextures_.at(id));
        sprite.setTextureRect(sf::IntRect({frame.left,frame.top},{frame.width,frame.height}));
        sprite.setColor(sf::Color(255,255,255,static_cast<std::uint8_t>(character.opacity())));
        sprite.setOrigin({static_cast<float>(graphic.originX),static_cast<float>(graphic.originY)});
        const auto position = character.motion.position();
        double footX = (position.x + .5) * tileSize_;
        if(loopX_){footX+=std::round((target.getView().getCenter().x-footX)/mapWidth_)*mapWidth_;}
        const double footY = (position.y + 1) * tileSize_;
        sprite.setPosition({static_cast<float>(footX),static_cast<float>(footY-character.motion.jumpHeight())});
        const int copies=loopX_?static_cast<int>(std::ceil(target.getView().getSize().x/mapWidth_))+1:0;
        for(int offset=-copies;offset<=copies;++offset){auto copy=sprite;copy.move({static_cast<float>(offset*mapWidth_),0});
            characters.push_back({copy,character.layer,footY,character.eventId==0?65535.0:footX+offset*mapWidth_,character.renderTiePriority,character.eventId,flashes.contains(character.id)?std::optional(flashes.at(character.id)):std::nullopt});
        }
    };
    for(const auto& object : scene.objects()){ addCharacter(object); }
    if(showPlayer){addCharacter(scene.player());}
    std::sort(characters.begin(),characters.end(),[](const auto& left,const auto& right){
        return std::tie(left.layer,left.footY,left.sortX,left.tiePriority,left.eventId) <
               std::tie(right.layer,right.footY,right.sortX,right.tiePriority,right.eventId);
    });
    const auto drawCharacter=[&](const CharacterSprite& c){
        target.draw(c.sprite,states);
        if(c.flash){auto overlay=c.sprite;auto color=*c.flash;color.a=static_cast<std::uint8_t>(color.a*overlay.getColor().a/255);overlay.setColor(color);target.draw(overlay,sf::BlendAdd);}
    };
    const auto drawLayer=[&](const sf::Sprite& sprite){
        if(!loopX_){target.draw(sprite,states);return;}
        const int base=static_cast<int>(std::floor(target.getView().getCenter().x/mapWidth_));
        const int copies=static_cast<int>(std::ceil(target.getView().getSize().x/mapWidth_))+1;
        for(int i=base-copies;i<=base+copies;++i){auto copy=sprite;copy.move({static_cast<float>(i*mapWidth_),0});target.draw(copy,states);}
    };
    const auto drawAnimated=[&](bool foreground){
        for(const auto& tile:animatedTiles_){
            if(tile.foreground!=foreground){continue;}
            const auto& r=tile.frames.at(tile.frameAt(scene.frame()));sf::Sprite sprite(tileTextures_.at(tile.id));
            sprite.setTextureRect(sf::IntRect({r[0],r[1]},{r[2],r[3]}));
            for(const auto& xy:tile.tiles){sprite.setPosition({static_cast<float>(xy[0]*tileSize_),static_cast<float>(xy[1]*tileSize_)});drawLayer(sprite);}
        }
    };
    drawAnimated(false);drawLayer(backgroundSprite_);
    if(scene.lowerTiles()){for(const auto& cell:scene.lowerTiles()->cells){const int mapped=scene.lowerTileMapping()[cell.index];if(mapped==cell.index){continue;}sf::Sprite sprite(lowerReplacementTextures_.at(mapped));sprite.setPosition({float(cell.tile[0]*tileSize_),float(cell.tile[1]*tileSize_)});drawLayer(sprite);}}
    for(const auto& row:scene.upperTileReplacements()){
        if(!scene.substitutedUpperTiles().contains(row.fromIndex)){continue;}
        for(std::size_t cell=0;cell<row.tiles.size();++cell){const auto& tile=row.tiles.at(cell);sf::Sprite sprite(replacementTextures_.at({row.fromIndex,row.cellUnderlays.empty()?0:cell}));sprite.setPosition({static_cast<float>(tile[0]*tileSize_),static_cast<float>(tile[1]*tileSize_)});drawLayer(sprite);}
    }
    for(const auto& character : characters){
        if(character.layer < 2){ drawCharacter(character); }
    }
    drawAnimated(true);drawLayer(foregroundSprite_);
    for(const auto& character : characters){
        if(character.layer == 2){ drawCharacter(character); }
    }
}
