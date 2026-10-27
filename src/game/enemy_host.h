#pragma once
#include "route_player.h"
#include "combat_rules.h"
class CombatScene;
// A behavior can affect its actor, combat domain and owned routes. It cannot
// replace a scene, navigate menus, transfer maps or retain another position.
class EnemyHost{
public:
    EnemyHost(CombatScene& scene,std::string actor);
    const std::string& id() const{return id_;}
    const CombatContent& content() const;
    GameSession& session() const;
    int map() const;
    CharacterState actor() const;
    CharacterState player() const;
    GridPosition actorScreenFoot() const;
    GridPosition sampleActorScreen(const std::string&) const;
    GridPosition sourcePlayerBody() const;
    void neuralGasHit(const std::string& flash);
    std::optional<Direction> sourceDirection() const;
    void sampleSourceDirection();
    bool pictureCommandsAllowed() const;
    GridPosition screenPicturePosition(GridPosition sampledScreen) const;
    GridPosition pictureWorkspacePosition(int commonEvent) const;
    TilePosition targetTile(const std::string& actor) const;
    void setActor(const CharacterState& actor);
    void setPlayer(const CharacterState& player);
    int random(int low,int high) const;
    AttackKind damageKind(AttackKind launched) const;
    AttackKind equippedDamageKind(bool auxiliary) const;
    void startBurn();
    bool burning() const;
    int advanceBurn(std::optional<GridPosition> sampledBody=std::nullopt);
    void advanceBlood(const nlohmann::json& blood,int selected);
    int terrain(TilePosition tile) const;
    void claimAutonomy();
    bool advance(RoutePlayer& route,const std::function<void(int,bool)>& setSwitch={});
    bool playerRoutePending() const;
    bool hasTerrainAtDistance(int distance) const;
    void fail(const std::string& flow);
    void startRoute(const std::string& cue,bool player=false);
    void route(RouteDefinition definition);
    void cancelRoutes(bool player=false);
    bool routePending(const std::string& source={}) const;
    void sound(const std::string& cue);
    void cue(const std::string& cue,bool actor=true);
    void present(PresentationEvent event);
    int bloodType(int writeSlot,int readSlot,int value);
    void damagePlayer(int amount);
    bool receiveSourceDamage(int attack);
    AttackContact receivePartDamage(const AttackSample&,int defense);
    bool requestPlayerReaction(int receiverPage);
    bool requestTargetReaction(const std::string& target);
    bool requestSourceReaction(int receiverPage);
    bool requestSpiderReaction(Direction sampledDirection);
    void cancelWeapon();
    void sourceJumpHit(const std::string& playerRoute);
    void sourceFatalHit();
    void requestAttackEnd(std::uint64_t id);
    bool holdsPlayer() const;
    void requestGrab();
    void releaseGrab();
    void yieldBehind();
    void playerPose(int pose);
    void cleanupPlayer();
    void startEscapeRecovery();
    bool dialogueActive() const;
    Direction sourceActorFacing(const std::string&) const;
    int receiverAt(TilePosition) const;
    void collisionReceiver(TilePosition);
    void eraseOwnedPictures(int first,int last);
    void storyEnding(int variant);
    bool completeEncounter(const std::string& flow);
    void reappearanceCue(const std::string& flow,const std::string& cue);
    void placeReappearanceObject(const std::string& object,TilePosition tile);
private:
    const nlohmann::json* burnRules() const;
    CombatScene& scene_;
    std::string id_;
    bool recoveryPermit_{};
    bool applyPlayerReaction(int receiverPage);
};
