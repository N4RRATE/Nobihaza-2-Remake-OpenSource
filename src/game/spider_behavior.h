#pragma once
#include "enemy_controller.h"
#include "enemy_host.h"

// CN spider pages own finite actions; the World remains the sole position owner.
class SpiderBehavior final:public EnemyController{
public:
    SpiderBehavior(CombatScene&,const EnemyDefinition&);
    const std::string& id() const override{return definition_.id;}
    void update(const InputFrame&) override;
    void updateParallel() override;
    bool hasParallelMapBehavior() const override{return true;}
    void updateDeathPresentation() override{}
    bool activateState(int) override;
    bool setScriptedDamage(int) override;
    AttackContact receiveAttack(const AttackSample&) override;
    EnemyMode mode() const override;
    int sourceState() const override{return state_;}
    int accumulatedDamage() const override{return damage_;}
    bool holdsPlayer() const override{return false;}
    bool beginGrab() override{return false;}
    void collisionYield(Direction) override;
private:
    EnemyDefinition definition_;
    EnemyHost host_,acidHost_;
    int state_{},damage_{},phase_{},wait_{},jumpFrame_{},contacts_{};
    int acidPhase_{},acidWait_{},walkingTicks_{},settleTicks_{};
    bool active_{},ending_{},corpse_{},damageRequestedEnd_{};
    Direction sampledDirection_{Direction::Down};
    std::string shownBody_;
    GridPosition lastPosition_{},deathSample_{};
    TilePosition sampledTile_{};
    std::vector<PresentationEvent> pendingBody_;
    std::string pendingBodyRole_;
    std::optional<RoutePlayer> movement_;
    std::vector<std::string> cues_;
    std::size_t cueCursor_{};
    const nlohmann::json& profile() const;
    const ObjectVariant& page(int) const;
    void applyPage(int);
    void change(int);
    void end();
    void move();
    int frontDistance() const;
    void sequence(const nlohmann::json&);
    bool advanceSequence();
    bool presentCue(const std::string&);
    void advanceBody();
    void advanceAcid();
    void advanceSwoop();
    void hitPlayer();
    int calculateDamage(const AttackSample&);
};
