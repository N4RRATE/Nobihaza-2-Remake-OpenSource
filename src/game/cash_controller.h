#pragma once
#include "combat_rules.h"
#include "input_buffer.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <map>
#include <optional>
#include <set>
class CombatScene;
// Scene-local continuations of the reviewed fixed pages. Session owns all source facts.
class CashController{
public:
 CashController(CombatScene&,const nlohmann::json&);
 std::optional<std::string> update(const InputFrame&);
 void retire();
 InputFrame recoveryInput(const InputFrame&);
 AttackResolution receive(const AttackSample&,std::set<std::string>&);
private:
 enum class Step{Continue,Called,Yield,Done};
 struct Frame{int page{},next{};};
 struct Work{std::vector<Frame> stack;int wait{},page{};};
 void call(Work&,int);bool run(Work&,bool pauseMessages=true);Step advance(Work&,Frame&);
 bool play(const std::string&,const std::string&,Work&);
 std::string actorName(int,const std::string&) const;
 int characterValue(int,int,const std::string&) const;
 void locate(int,int,int,const std::string&);int terrain(int,int) const;
 void beginPlayerBlood(int);void advancePlayerBlood();
 void common5();void common6();void common11();void common12();void common55();void sourceFlag(int,bool);
 bool live() const;
 CombatScene& scene_;nlohmann::json profile_;std::uint64_t generation_{};
 std::map<std::string,Work> parallel_;Work foregroundWork_;
 bool receiverEnd_{},retired_{},winning_{},winTransfer_{},finished_{},recoveryWaiting_{};
 int playerBloodType_{},playerBloodTicks_{};
 std::optional<AttackSample> receiver_;
};
