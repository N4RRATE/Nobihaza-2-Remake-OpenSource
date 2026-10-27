#pragma once
#include "enemy_controller.h"
#include "enemy_host.h"
class CerberusBehavior final:public EnemyController{
public:
    CerberusBehavior(CombatScene&,const EnemyDefinition&);
    const std::string& id() const override{return definition_.id;}
    void update(const InputFrame&) override;
    void updateDeathPresentation() override;
    void updateParallel() override;
    bool activateState(int state) override;
    AttackContact receiveAttack(const AttackSample&) override;
    AttackContact receiveCompanion(int page,int damage);
    EnemyMode mode() const override;
    int sourceState() const override{return definition_.rules.contains("encounter_receivers")?(state_==1?0:state_==10?100:state_==11?110:state_):state_;}
    bool attacking() const{return attack_;}
    int accumulatedDamage() const override{return damage_;}
    bool holdsPlayer() const override{return host_.holdsPlayer();}
    bool beginGrab() override;
    void collisionYield(Direction) override;
private:
    EnemyDefinition definition_;
    EnemyHost host_;
    int state_{},damage_{},wait_{},phase_{},escape_{},cycles_{},feedingWait_{120},feedingPhase_{-1};
    bool pendingGrenadeEnding_{};
    bool active_{},attack_{},pendingGrab_{},ending_{},corpse_{},criticalEnding_{};
    std::optional<RoutePlayer> movement_;
    std::optional<AttackSample> pendingHit_;
    bool damageRequestedEnd_{};
    std::vector<std::string> sequence_;
    std::size_t nextCue_{};
    const nlohmann::json& rules() const;
    const nlohmann::json& role(const std::string&) const;
    const ObjectVariant& page(const std::string&) const;
    std::string cue(const std::string&,int command) const;
    void apply(const std::string&);
    void change(int state);
    void move();
    int frontDistance() const;
    void sequence(const std::string&,int first=0);
    bool advanceSequence();
    void sampleBodyCache();
    void clearBodyCache();
    std::optional<int> companionDamage_;
    int calculate(const AttackSample&,bool retainDefense,bool bonus);
    void end();
    void bite();
    void release(bool interrupted);
};
