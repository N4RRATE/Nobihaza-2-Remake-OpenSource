#pragma once

#include "character_animation.h"
#include "dialogue.h"
#include <filesystem>
#include <string>
#include <map>
#include <vector>
#include <optional>
#include <array>
#include <cstdint>
#include <memory>

struct SpriteFrameDefinition{
    std::string direction;
    int column{};
    int left{};
    int top{};
    int width{};
    int height{};
};

struct SpriteDefinition{
    std::filesystem::path texturePath;
    int textureWidth{};
    int textureHeight{};
    int originX{};
    int originY{};
    // Validated frame tables are immutable across maps and legacy content snapshots.
    std::shared_ptr<const std::vector<SpriteFrameDefinition>> frames=std::make_shared<const std::vector<SpriteFrameDefinition>>();
};

using SpriteDefinitions=std::map<std::string,SpriteDefinition>;

struct PlayerEntryDefinition{
    int tileX{};
    int tileY{};
    std::string direction;
    std::string spriteId;
    int initialColumn{};
    int moveSpeed{};
    int subunitsPerTile{};
    int subunitsPerUpdate{};
};

enum class InteractionKind{ None, Message, Deferred };
enum class InteractionGeometry{ None, SameTile, AdjacentFacing };
struct InteractionDefinition{
    InteractionKind kind{InteractionKind::None};
    InteractionGeometry geometry{InteractionGeometry::None};
    std::string messageId;
};
struct SceneObjectDefinition{
    std::string id;
    int sourceEventId{};
    int tileX{};
    int tileY{};
    std::optional<std::string> spriteId;
    std::string direction;
    int initialColumn{};
    int layer{};
    bool active{};
    bool through{};
    bool overlapForbidden{};
    int moveSpeed{};
    int animationType{};
    int trigger{};
    InteractionDefinition interaction;
    int renderTiePriority{}; // Presentation only, after layer and position.
    int transparency{};
    std::optional<int> tilePassageFlags;
    int outsideMapMargin{};
};

struct ScreenToneDefinition{
    int redPercent{100};
    int greenPercent{100};
    int bluePercent{100};
};

struct ControlPermissions{
    bool ordinaryMovement{};
    bool ordinaryAction{};
    bool customMenu{};
    bool combat{};
    bool defaultShortcutMenu{};
};

struct AnimatedTileDefinition{
    int id{},width{},height{},frameTicks{};
    bool foreground{};
    std::filesystem::path file;
    std::vector<std::array<int,4>> frames;
    std::vector<int> order;
    std::vector<std::array<int,2>> tiles;
    std::size_t frameAt(std::uint64_t tick) const{return order.at((tick/frameTicks)%order.size());}
};
struct PanoramaDefinition{
    std::string image;
    bool loopX{},loopY{},autoX{},autoY{};
    int speedX{},speedY{}; // Original RPG Maker setting, not pixels/update.
};
struct PanoramaState{
    PanoramaDefinition definition;
    std::array<std::int64_t,2> phase{}; // 1/32 pixel units, advanced by Scene logic only.
};
struct UpperTileReplacementDefinition{
    int fromIndex{},toIndex{};
    std::filesystem::path image,underlay;
    std::vector<std::filesystem::path> cellUnderlays;
    std::vector<std::array<int,2>> tiles;
    std::vector<int> passageMasks;
};
struct LowerTileCell{std::array<int,2> tile;int index{},upperPassage{};};
struct LowerTileDefinition{
    std::vector<std::array<int,2>> allowed;
    std::vector<int> passageFlags,terrainIds;
    std::vector<LowerTileCell> cells;
    std::map<int,std::filesystem::path> images;
};
struct SceneDefinition{
    std::optional<LowerTileDefinition> lowerTiles;
    std::vector<UpperTileReplacementDefinition> upperTileReplacements;
    std::optional<PanoramaDefinition> defaultPanorama;
    std::vector<AnimatedTileDefinition> animatedTiles;
    bool loopX{};
    int width{};
    int height{};
    int tileSize{};
    std::vector<int> passageMasks;
    ControlPermissions controls;

    std::filesystem::path backgroundPath;
    std::filesystem::path foregroundPath;

    std::filesystem::path fontPath;
    ScreenToneDefinition tone;
    std::map<std::string,MessageDefinition> messages;
    WalkingAnimationDefinition walkingAnimation;
    PlayerEntryDefinition player;
    std::vector<SceneObjectDefinition> objects;
    // Each content generation freezes one directory; extensions derive a private copy.
    std::shared_ptr<const SpriteDefinitions> sprites=std::make_shared<const SpriteDefinitions>();
};

SceneDefinition loadSceneDefinition(const std::filesystem::path& scenePath);
