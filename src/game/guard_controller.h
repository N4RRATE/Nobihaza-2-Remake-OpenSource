#pragma once
#include "combat_scene.h"
#include <array>
// Encounter-local coordination; World remains the only position owner.
class CerberusBehavior;
class GuardController{
public:
    explicit GuardController(CombatScene& scene,const nlohmann::json& definition);
    ~GuardController();
    bool owns(const std::string& id) const;
    bool grantGrab(const std::string& id);
    void update(const InputFrame& input);
    AttackResolution receive(const AttackSample&,std::set<std::string>& resolved);
    std::vector<EnemyView> views() const;
    int hp() const{return hp_;}
    void restore(){hp_=profile_.at("attributes").at("hp");}
private:
    enum class NpcMode { Neutral=0, Random, ApproachPlayer, LeavePlayer, Perimeter, Fire,
        LongFire, Reload, ApproachTarget, Combo, Evade, Knife, Retract, ClosePlayer,
        Kick, KickRecovery, Rescued=20, GrabRecovery=21 };
    enum class SlotMode { Roam=0, Pursuit, Bite, GrabPrepare, PlayerFront, PlayerBack,
        Retreat, CompanionFront, CompanionBack, Stun=10, Knockback=11, Feeding=100 };
    enum class Regeneration { Alive, Dying, Fading, Hidden };
    struct CueTrack {std::vector<std::string> cues;std::size_t cursor{};int wait{};std::array<int,2> offset{};std::array<int,2> workspace{};};
    struct RoleProgress{std::string role;std::size_t cursor{};int wait{};std::optional<GridPosition> sampledScreen{};};
    struct Slot{int number{},wait{},bites{},escape{},fade{};Regeneration regeneration{Regeneration::Alive};bool holdingPlayer{},holdingNpc{};std::string receiverRoute;RoleProgress presentation;};
    const nlohmann::json& row(const Slot&) const;
    int value(const Slot&,const char*) const;
    void set(Slot&,const char*,int);
    void sampleBodies();
    void stepActor(const std::string&,Direction,bool backwards=false);
    void applyRole(const std::string&,const nlohmann::json&);
    bool advanceRole(const std::string&,const nlohmann::json&,RoleProgress&,const std::function<void(int)>& commit={});
    void updateSlot(Slot&,const InputFrame&);
    void regenerate(Slot&);
    void release(Slot&,bool rescued=false);
    void beginGrab(Slot&,bool companion);
    void hurtNpc(int attack);
    AttackContact hit(Slot&,int page,int damage,const AttackSample* sample=nullptr);
    bool select(TilePosition probe,int radius,bool evade=false);
    void updateNpc();
    void melee(const std::string&,int command);
    void shoot();
    void updateProjectile();
    void updateAttachedGrenade();
    void updateSupport();
    void updateCoverFire();
    void presentNpcPulses();
    void advanceTrack(CueTrack&);
    void showVoice(int page);
    int outgoingDamage(const Slot&);
    void beginNpcDeath();
    CombatScene& scene_;
    nlohmann::json profile_;
    std::array<Slot,8> slots_;
    int weapon_{},hp_{},npcMode_{-1},npcWait_{},npcRecovery_{},supportWait_{},grenadeWait_{-1},bulletDistance_{};
    RoleProgress npcPresentation_,deathPresentation_,grenadePresentation_,coverPresentation_;
    CueTrack muzzle_,blood_,voice_;
    std::size_t meleeContact_{};
    bool bulletActive_{},finished_{},dying_{};
    int bloodType_{1};
    int recoveryStage_{};
    bool coverSelected_{};
    std::unique_ptr<CerberusBehavior> dog_;
};
int guardSourceVariable(const std::string& field);
void installFirstGuardContent(CombatContent&,const nlohmann::json&);
void installSecondGuardContent(CombatContent&,const nlohmann::json&);
