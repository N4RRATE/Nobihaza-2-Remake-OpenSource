#pragma once
#include "enemy_controller.h"
#include "expanded_content.h"
#include "enemy_host.h"
class LickerBehavior final: public EnemyController{
public:
    LickerBehavior(CombatScene& scene,const EnemyBlueprint& definition);
    const std::string& id() const override{return definition_.id;}
    void update(const InputFrame& input) override;
    void updateDeathPresentation() override{}
    void updateParallel() override;
    void retire() override;
    bool hasParallelMapBehavior() const override{return rules().value("source_cache",false);}
    bool activateFromEncounter() override;
    bool activateState(int state) override;
    AttackContact receiveAttack(const AttackSample& sample) override;
    EnemyMode mode() const override;
    int sourceState() const override{return state_==State::Landing&&rules().value("source_cache",false)?10:static_cast<int>(state_); }
    int accumulatedDamage() const override{return damage_;}
    bool holdsPlayer() const override{return host_.holdsPlayer();}
    bool beginGrab() override;
    void collisionYield(Direction) override{}
private:
    enum class State{Wander,Stalk,Pursuit,Claw,Pounce,FindWall,ToWall,Ceiling,TongueGrab,Drop,Down=100,Landing=101};
    enum class Action{None,Tongue,TongueSample,TongueRecover,PouncePrepare,PounceRecover,Claw,ClawWindup,ClawRecover,Climb,ClimbRoute,CeilingPrepare,CeilingMiss,Grab,GrabLoop,GrabEscape,GrabEscapeRoute,GrabFatal,GrabFatalRoute,Drop,DropRoute,DropWait,Down,Hit,Ending};
    EnemyBlueprint definition_;
    EnemyHost host_;
    State state_{State::Wander};
    Action action_{Action::None};
    bool active_{},dead_{},pendingGrab_{},attackActive_{},retired_{};
    GridPosition body_{};
    TilePosition stalkOrigin_{},captured_{};
    int tongueDistance_{},pouncePreparationPage_{11},footstepTicks_{},inputCooldown_{};
    int damage_{},phase_{},wait_{},scanDistance_{1},escape_{},cycles_{};
    std::optional<RoutePlayer> autonomous_;
    const nlohmann::json& rules() const;
    const ObjectVariant& page(const std::string& role) const;
    void applyPage(const std::string& role);
    void change(State next);
    void begin(Action action);
    bool play(int page,int command);
    bool playCue(const std::string& id);
    std::string cueId(int page,int command) const;
    bool sequence(const std::vector<int>& commands,int page);
    void move();
    void decide();
    void advanceAction(const InputFrame& input);
    int forwardDistance(int minimum,int maximum) const;
    bool frontRow(bool randomDown=false);
    void hurt(int attack,int animation=0);
    void release(bool interrupted);
    void sampleBody();
    int terrain(TilePosition tile) const;
    int actionAttack(const std::string& role) const;
    bool cueCondition(const nlohmann::json& condition) const;
};
