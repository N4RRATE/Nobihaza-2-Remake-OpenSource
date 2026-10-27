#pragma once

#include "scene_definition.h"
#include "exploration_scene.h"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

// Shared by the window and render checks; zero-sized windows keep their previous view.
sf::View makeSceneView(sf::Vector2u targetSize, sf::Vector2f sceneSize);

// The game shares immutable character textures across prepared scene candidates.
class SceneSpriteTextures{
public:
    const sf::Texture& load(const SpriteDefinition&,const ScreenToneDefinition&);
private:
    using Key=std::tuple<std::filesystem::path,int,int,int,int,int>;
    std::map<Key,sf::Texture> textures_;
};

class SceneRenderer{
public:
    explicit SceneRenderer(const SceneDefinition& scene,std::shared_ptr<SceneSpriteTextures> sprites=std::make_shared<SceneSpriteTextures>());
    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;
    SceneRenderer(SceneRenderer&&) = delete;
    SceneRenderer& operator=(SceneRenderer&&) = delete;
    void draw(sf::RenderTarget& target, const ExplorationScene& scene, const std::map<std::string,sf::Color>& flashes={},bool showPlayer=true,float saturation=100.f) const;

private:
    struct CharacterSprite{
        sf::Sprite sprite;
        int layer{};
        double footY{};
        double sortX{};
        int tiePriority{};
        int eventId{};
        std::optional<sf::Color> flash;
    };

    std::shared_ptr<SceneSpriteTextures> spriteTextures_;
    std::map<int,sf::Texture> tileTextures_;
    std::map<std::pair<int,std::size_t>,sf::Texture> replacementTextures_;
    std::map<int,sf::Texture> lowerReplacementTextures_;
    std::vector<AnimatedTileDefinition> animatedTiles_;
    sf::Texture backgroundTexture_;
    sf::Sprite backgroundSprite_;
    sf::Texture foregroundTexture_;
    sf::Sprite foregroundSprite_;

    // Map nodes have stable addresses. Sprites are destroyed before their textures.
    std::map<std::string, const sf::Texture*> characterTextures_;
    std::shared_ptr<const SpriteDefinitions> graphics_;
    mutable sf::Shader saturationShader_;
    int tileSize_{};bool loopX_{};int mapWidth_{};
};
