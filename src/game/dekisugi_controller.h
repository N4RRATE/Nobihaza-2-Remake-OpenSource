#pragma once
#include "combat_rules.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <set>
#include <optional>
class CombatScene;
// Source-specific scheduler; compiled pages contain explicit operations, never event arrays.
class DekisugiController{
public:
    DekisugiController(CombatScene&,const nlohmann::json&);
    std::optional<std::string> update();
    bool owns(const std::string&) const;
    AttackResolution receive(const AttackSample&,std::set<std::string>&);
private:
    enum class Step{Continue,Called,Yield,Done};
    struct Frame{int page{},next{};};
    struct Work{std::vector<Frame> stack;int wait{};};
    void call(Work&,int page);
    bool run(Work&);
    Step advance(Work&,Frame&);
    bool play(const std::string&,const std::string&,Work&);
    int calledPage(int event,int page) const;
    void common6();void common9();void common12();void common141();
    CombatScene& scene_;
    nlohmann::json profile_;
    std::uint64_t generation_{};
    Work main_,body_;
    bool started_{},introDone_{},winning_{},finished_{},winTransfer_{};
};
