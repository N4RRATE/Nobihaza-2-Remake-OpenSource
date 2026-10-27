#pragma once
#include <string>
#include <cstddef>
#include "combat_content.h"
#include "input_buffer.h"
class CombatScene;
// A bounded story graph owns sequence progress; shared scene services execute its presentation.
class OpeningController{
public:
    explicit OpeningController(CombatScene&);
    void update(const InputFrame&);
    bool finished() const{return finished_;}
    const std::string& stage() const{return stage_;}
private:
    void enter(const std::string&);
    void execute(const nlohmann::json&);
    SoundDefinition decisionSound_;
    CombatScene& scene_;
    std::string stage_,subsequence_,self_;
    nlohmann::json operations_,choice_;
    std::size_t cursor_{};
    bool finished_{};bool environment_{};int environmentWait_{};std::size_t environmentNext_{};
};
