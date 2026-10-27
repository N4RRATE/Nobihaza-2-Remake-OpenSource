#pragma once
#include "enemy_controller.h"
#include "enemy_host.h"
#include <array>
#include <unordered_set>
// Scene-local M120 actions and three independent picture projectiles. Actor motion lives in world.
class HangarBehavior final:public EnemyController{
public:
    HangarBehavior(CombatScene&,const EnemyDefinition&);
    const std::string& id() const override{return definition_.id;}
    void update(const InputFrame&) override;
    void updateParallel() override;
    bool hasParallelMapBehavior() const override{return true;}
    void updateDeathPresentation() override{}
    void retire() override{if(!cleaned_){cancel(true);}mode_=EnemyMode::Dormant;}
    AttackContact receiveAttack(const AttackSample&) override;
    EnemyMode mode() const override{return mode_;}
    int sourceState() const override{return action_;}
    int accumulatedDamage() const override{return damage_;}
    bool holdsPlayer() const override{return false;}
    bool beginGrab() override{return false;}
    void collisionYield(Direction) override{}
private:
    struct Gas{bool active{},picturePending{};GridPosition screen{};GridPosition playerTile{};GridPosition velocity{};GridPosition cameraDrift{};std::array<int,2> driftWait{};int age{},scale{};};
    CombatScene& scene_;
    EnemyDefinition definition_;
    EnemyHost host_;
    EnemyMode mode_{EnemyMode::Dormant};
    int actor32Hp_{800};
    int damage_{},action_{},wait_{},arcStep_{},arcHeight_{};
    std::size_t cursor_{};
    std::unordered_set<std::uint64_t> processedAttacks_;
    bool submitted_{},cleaned_{true};
    std::array<Gas,3> gases_;
    std::optional<RoutePlayer> autonomy_;
    int autonomyPage_{};
    int bodyPage_{};
    int passiveGasWait_{};
    Direction bodyDirection_{Direction::Down};
    TilePosition lastBodyTile_{};
    bool bodyShown_{};
    bool valid() const;
    void cancel(bool clearBody);
    void cancelAction();
    void beginAction(int);
    void advanceAction();
    void advanceAutonomy();
    void strike(const nlohmann::json&);
    void releaseGases();
    void advancePassiveGases();
    void emitGases(const std::array<bool,3>&,const std::string& flash);
    void advanceGases();
    void stopGas(std::size_t);
    void advanceBody();
    void sampleBody();
    void picture(const std::string&,GridPosition);
    void cue(const std::string&);
    int level() const;
};
