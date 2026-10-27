#pragma once
#include "enemy_controller.h"
#include "enemy_host.h"
#include "input_buffer.h"
#include "route_player.h"
#include <optional>


class ZombieBehavior final: public EnemyController{
public:
    ZombieBehavior(CombatScene& scene,const EnemyDefinition& definition);
    const std::string& id() const override{return id_;}
    void update(const InputFrame& input) override;
    void updateDeathPresentation() override;
    void updateParallel() override;
    bool setScriptedDamage(int damage) override;
    bool activateState(int state) override;
    AttackContact receiveAttack(const AttackSample& sample) override;
    EnemyMode mode() const override;
    int accumulatedDamage() const override;
    int sourceState() const override;
    bool holdsPlayer() const override;
    bool beginGrab() override;
    void collisionYield(Direction facing) override;
private:
    void recoverAway();
    void resumePursuit();
    void updateEnding();
    void beginDamageEnding(bool critical);
    void beginRescueEnding();
    void startRescueEndingPage();
    void updateRescueEnding();
    void updateArrival();
    bool advancePresentation(const nlohmann::json& cues,std::size_t& cursor,int& wait);
    EnemyDefinition definition_;
    const nlohmann::json& rules() const;
    bool present() const;
    std::string sprite(const std::string& role) const;
    std::string cue(const std::string& role) const;
    void beginAttack(bool deferRoute=false);
    void releaseGrab();
    void interruptGrab();
    void applyPresentationPage(const std::string& role);
    void bite();
    void setSprite(const std::string& sprite,int layer=1);
    EnemyHost host_;
    std::string id_;
    EnemyMode mode_{EnemyMode::Dormant};
    std::optional<RoutePlayer> pursuit_;
    int wait_{};
    int phase_{};
    std::size_t arrivalCue_{};
    std::size_t dormantCue_{};
    int dormantWait_{};
    int attackChoice_{};
    int attackPage_{};
    int bites_{};
    int escapeInputs_{};
    int retreatSteps_{};
    int damage_{};
    int feedingWait_{120};
    int feedingStage_{};
    bool headEnding_{};
    bool bodyEnding_{};
    bool grabbedPursuit_{};
    bool fallenOnce_{},grounded_{},fallenState_{};
    std::optional<RoutePlayer> variantRoute_;
    void applyPage(int page,bool preserveFacing=false);
    void startRush();
    bool rushInRange() const;
    void advancePursuit(int page,std::optional<RoutePlayer>& route);
    void updateRush();
};
