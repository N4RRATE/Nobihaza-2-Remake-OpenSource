#pragma once
#include "enemy_controller.h"
#include "enemy_host.h"
class PursuerBehavior final:public EnemyController{
public:
    PursuerBehavior(CombatScene& scene,const EnemyDefinition& definition);
    const std::string& id() const override{return definition_.id;}
    void update(const InputFrame&) override;
    void updateParallel() override;
    bool hasParallelMapBehavior() const override{return true;}
    void updateDeathPresentation() override{}
    AttackContact receiveAttack(const AttackSample& sample) override;
    EnemyMode mode() const override{return mode_;}
    int sourceState() const override{return attackChoice_;}
    int accumulatedDamage() const override{return damage_;}
    bool holdsPlayer() const override{return false;}
    bool beginGrab() override{return false;}
    void collisionYield(Direction) override{}
private:
    EnemyDefinition definition_;
    EnemyHost host_;
    EnemyMode mode_{EnemyMode::Dormant};
    int damage_{};
    bool completionSubmitted_{};
    bool reappearing_{};
    int wait_{};
    TilePosition reappearTile_{};
    bool specialReappearance_{};
    int reappearPhase_{};
    std::size_t reappearSequence_{};
    std::string reappearFlow_;
    std::size_t reappearFlowStep_{};
    int attackChoice_{};
    int attackTicks_{};
    bool slashHorizontal_{};
    // E2 reads the same source counter206 that E4 action stages write.
    int bodyFrameCounter_{};
    enum class BodyPhase{Sample,Setup,IdleTile,Hide,Select,Visible};
    BodyPhase bodyPhase_{BodyPhase::Sample};
    int bodyPage_{};
    bool bodyStarting_{};
    std::size_t bodyCueIndex_{};
    GridPosition bodyPictureSample_{};
    std::string bodyDirection_;
    std::string idleShownDirection_;
    std::string bodyFrame_;
    std::string bodyVisible_;
    bool bodyWalking_{};
    TilePosition pictureTile_{};
    int walkPulse_{};
    std::optional<RoutePlayer> autonomousRoute_;
    int autonomousPage_{};
    bool present() const;
    void startAttack(int choice);
    void advanceAttack();
    void applyPage(const std::string& role);
    void applyPage(int page);
    void advanceAutonomy();
    void advanceProximityEntry(const nlohmann::json& mode);
    void advanceBlood();
    void samplePictureWorkspace(int commonEvent,const nlohmann::json& offset);
    void presentPictureCue(const std::string& cue,int commonEvent);
    bool playerInside(const nlohmann::json& bounds) const;
    bool factsOff(const nlohmann::json& facts) const;
    void presentCue(const std::string& cue);
    void validateBodyManager() const;
    void advanceBody();
    void sampleBody();
    bool presentBodyCue(const std::string& cue);
    bool advanceBurn();
    bool advanceReappearance(const nlohmann::json& profile);
    bool advanceReappearanceFlow();
    void accumulateDamage(int amount);
};
