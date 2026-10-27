#pragma once
#include "combat_rules.h"
#include "input_buffer.h"
#include <nlohmann/json.hpp>
#include <map>
#include <optional>
#include <set>
#include <vector>
class CombatScene;
// Fixed source pages own continuations; Session owns facts and World owns positions.
class FinalBossController{
public:
 struct Victory{std::uint64_t scene{};std::string object;int page{};};
 FinalBossController(CombatScene&,const nlohmann::json&);
 std::optional<Victory> update(const InputFrame&);
 AttackResolution receive(const AttackSample&,std::set<std::string>&);
 InputFrame recoveryInput(const InputFrame&);
 bool airEquipped() const;
 void updateFreeControl(const InputFrame&);
    void updateVictoryPresentation();
 void retire();
private:
 enum class Step{Continue,Called,Yield,Done};
 struct Frame{int page{},next{};};
 struct Work{std::vector<Frame> stack;int wait{},page{};};
 void call(Work&,int);bool run(Work&,bool pauseMessages=true);Step advance(Work&,Frame&);
 bool play(const std::string&,const std::string&,Work&);
 std::string actorName(int,const std::string&) const;
 int characterValue(int,int,const std::string&) const;
 void locate(int,int,int,const std::string&);
 void common5();void common6();void common11();void common12();
 void sourceFlag(int,bool);void storyEquip(int);
 void beginAir(bool);void advanceAir();void endAir();void airContact();void updateAirInput(const InputFrame&);
 void beginPlayerBlood(int);void advancePlayerBlood();
 bool live() const;
 void cancelRoutes();
 CombatScene& scene_;nlohmann::json profile_;std::uint64_t generation_{};
    std::map<std::string,Work> parallel_;Work foreground_,airAction_,airExplosion_,airReceiver_;
 std::set<std::string> erased_,airResolved_;
 std::optional<AttackSample> receiver_;
 bool retired_{},finished_{},receiverEnd_{},recoveryWaiting_{},airActive_{},airStartYield_{},airExploded_{};
 bool decisionSampled_{},decisionAttack_{};std::uint64_t airId_{};
 int playerBloodType_{},playerBloodTicks_{};
};
