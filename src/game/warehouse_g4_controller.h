#pragma once
#include "combat_rules.h"
#include "input_buffer.h"
#include "input_prompt.h"
#include <nlohmann/json.hpp>
#include <map>
#include <optional>
#include <set>
#include <vector>
class CombatScene;
// Only the fixed, reviewed M204 pages; World and Session retain the source facts.
class WarehouseG4Controller {
public:
 WarehouseG4Controller(CombatScene&,const nlohmann::json&);
 void update(const InputFrame&);
 bool interact();
 AttackResolution receive(const AttackSample&,std::set<std::string>&);
 InputFrame recoveryInput(const InputFrame&);
 InputFrame promptInput(const InputFrame&) const;
 void retire();
private:
    friend class WarehouseSaveState;
 enum class Step {Continue,Called,Yield,Done};
 struct Frame {int page{},next{};};
 struct Work {std::vector<Frame> stack;int wait{},page{};std::string actor;bool refresh{};};
 bool valid() const;
 int selectedPage(const std::string&) const;
 void schedule(const std::string&);
 void call(Work&,int);
 bool run(Work&);
 Step advance(Work&,Frame&);
 bool play(const std::string&,const std::string&,Work&);
 std::string actorName(int,const std::string&) const;
 int characterValue(int,int,const std::string&) const;
 void locate(int,int,int,const std::string&);
 int terrain(int,int) const;
 void sourceFlag(int,bool);
 void common5();void common6();void common11();void common12();void common38();
 bool choice(Work&);
 void systemGraphic(const std::string&);
 void advancePlayerBlood();void beginPlayerBlood(int);
 CombatScene& scene_;nlohmann::json profile_;std::uint64_t generation_{};
 std::map<std::string,Work> parallel_;Work foreground_;
 std::optional<AttackSample> receiver_;std::optional<PromptIdentity> choice_;
 bool choiceCreated_{},retired_{},failureCommitted_{},recoveryWaiting_{},receiverEnd_{};
 int choiceIndex_{},playerBloodType_{},playerBloodTicks_{};
};
