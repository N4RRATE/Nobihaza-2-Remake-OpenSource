#pragma once
#include "input_buffer.h"
#include "combat_rules.h"
#include "expanded_content.h"
#include "character_motion.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <optional>
#include <cstdint>
#include <vector>
#include <string>
#include <set>
class CombatScene;
enum class ActionCancel{ Grabbed, Death, Transition, Foreground };
class WeaponAction{
public:
    virtual ~WeaponAction()=default;
    virtual void start()=0;
    virtual bool tick()=0;
    virtual void cancel(ActionCancel reason)=0;
    virtual nlohmann::json saveState() const=0;
    virtual void restoreState(const nlohmann::json&)=0;
};
struct AttackState{
    GridPosition position;
    Direction direction;
    int advances{};
    int movementUpdates{};
    bool muzzle{true};
    std::uint64_t generation{};
    bool startYield{true};
    std::set<std::string> resolved;
    AttackKind kind{AttackKind::Handgun};
    std::optional<AttackDefinition> definition;
    std::uint64_t id{};
    bool through{};
    int definitionWeapon{}; // Keep the fired definition when equipment changes during flight.
};
class WeaponController{
public:
    explicit WeaponController(CombatScene& scene);
    void update(const InputFrame& input);
    void advanceAttack();
    void cancel(ActionCancel reason,bool preserveAimingAid=false);
    void requestAttackEnd(std::uint64_t id);
    nlohmann::json captureWarehouseState() const;
    void restoreWarehouseState(const nlohmann::json&);
    bool raised() const;
    bool auxiliaryRaised() const{return raised_&&auxiliary_;}
    bool aiming() const;
    bool busy() const;
private:
    void posture(bool raise);
    const nlohmann::json& binding() const;
    const nlohmann::json& mapProfile(int weapon) const;
    const nlohmann::json& profile(const std::string& name) const;
    bool auxiliary_{};
    bool knifeWindup_{};
    bool grenadeFinish_{};
    bool launcherReloadPending_{};
    struct GrenadeFlight{CharacterMotion motion;int steps{},age{},height{8},explosionAge{-1};};
    std::optional<GrenadeFlight> grenade_;
    void strikeKnife();
    bool infiniteAmmunition() const;
    void advanceLauncher();
    void explodeLauncher();
    int emitLauncherCue(const std::string& cue);
    std::unique_ptr<WeaponAction> launcherPresentation_;
    int launcherPresentationWeapon_{};
    GridPosition launcherPresentationPosition_;
    struct LauncherFlight{CharacterMotion motion;int weapon{},steps{},stage{-1},wait{};};
    std::optional<LauncherFlight> launcher_;
    void fire();
    void commitGrenade();
    void advanceGrenade();
    void reload();
    void endAttack(bool preserveExplosion=false,bool preserveAimingAid=false);
    void clearAim(bool preserveAimingAid=false);
    void begin(const std::vector<std::string>& sequence,bool start=true);
    friend class WarehouseSaveState;
    std::uint64_t nextAttackId_{1},requestedEnd_{};
    CombatScene& scene_;
    bool raised_{};
    bool aimed_{};
    bool aimPending_{};
    int shiftCount_{};
    int aimCount_{};
    int cancelCount_{};
    int controlWait_{};
    std::optional<InputFrame> controlSample_;
    bool turned_{};
    bool initialRaise_{};
    std::unique_ptr<WeaponAction> action_;
    std::optional<AttackState> attack_,pendingAttack_;
    std::vector<std::string> pendingFireCues_;
    int pendingFireWait_{};
    std::optional<bool> postureTarget_;
};
