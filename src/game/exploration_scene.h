#pragma once
#include "character_motion.h"
#include "input_buffer.h"
#include "scene_definition.h"
#include <optional>
#include <set>
#include <string>
#include <vector>

struct CharacterState{
    std::string id;
    int eventId{};
    CharacterMotion motion;
    std::optional<std::string> spriteId;
    CharacterAnimation animation;
    int layer{};
    bool active{true};
    bool through{};
    bool overlapForbidden{};
    int trigger{};
    InteractionDefinition interaction;
    // Fixed-direction event pages keep their graphic row while logical movement turns.
    Direction fixedGraphicFacing{Direction::Down};
    int renderTiePriority{};
    // Source move-route through survives page changes; hidden-page state does not.
    std::optional<bool> routeThrough;
    int transparency{};
    std::optional<int> tilePassageFlags;
    int outsideMapMargin{};
    bool directionLocked{};
    int opacity() const{return 255-32*transparency;}
    Direction graphicFacing() const{return animation.directionFixed()?static_cast<Direction>((static_cast<int>(fixedGraphicFacing)+animation.rotation())%4):motion.facing();}
    void face(Direction direction){motion.face(direction);fixedGraphicFacing=direction;}
    void setAnimation(CharacterAnimation value,Direction facing){value.setPaused(animation.paused());animation=value;fixedGraphicFacing=facing;}
    void setSpeedLevel(int level){motion.setSpeed(1<<(1+level));animation.setSpeedLevel(level); }
};

class ExplorationScene{
public:
    explicit ExplorationScene(const SceneDefinition& definition);
    const CharacterState& player() const;
    const std::vector<CharacterState>& objects() const;
    ControlPermissions controls() const;
    const Dialogue& dialogue() const;
    const MessageDisplay& messageOptions() const;
    std::optional<std::string> investigationTarget() const;
    bool canPlayerMove(Direction direction) const;
    void update(const InputFrame& input);
    void setControls(ControlPermissions controls);
    void addTransientActor(CharacterState actor);
    void setObjectState(const CharacterState& object);
    void setPlayerState(const CharacterState& player);
    bool containsTile(TilePosition tile) const;
    bool withinTileMargin(TilePosition tile,int margin) const;
    bool canMove(const CharacterState& actor,Direction direction) const;
    bool canMoveDiagonal(const CharacterState& actor,Direction vertical,Direction horizontal) const;
    void advanceCharacters();
    std::optional<int> mappedLowerTerrain(TilePosition tile) const;
    void substituteLowerTile(int from,int to);
    const std::optional<LowerTileDefinition>& lowerTiles() const{return lowerTiles_;}
    const std::array<int,144>& lowerTileMapping() const{return lowerTileMapping_;}
    void substituteUpperTile(int from,int to);
    const std::vector<UpperTileReplacementDefinition>& upperTileReplacements() const{return upperTileReplacements_;}
    const std::set<int>& substitutedUpperTiles() const{return substitutedUpperTiles_;}
    void advanceClock(){++frame_;}
    std::uint64_t frame() const{return frame_;}
private:
    friend class WarehouseSaveState;
    struct Investigation{
        MessageRequestId request;
        MessageDisplay restore;
    };
    std::uint64_t frame_{};
    Dialogue dialogue_;
    std::optional<Investigation> investigation_;
    MessageDisplay messageOptions_;
    std::map<std::string,MessageDefinition> messages_;
    TerrainMap terrain_;
    std::optional<LowerTileDefinition> lowerTiles_;
    std::array<int,144> lowerTileMapping_;
    std::vector<UpperTileReplacementDefinition> upperTileReplacements_;
    std::set<int> substitutedUpperTiles_;
    bool loopX_{};int width_{},height_{};
    TilePosition normalize(TilePosition p) const;
    CharacterState player_;
    std::vector<CharacterState> objects_;
    ControlPermissions controls_;
    std::set<std::string> graphicIds_;
};
