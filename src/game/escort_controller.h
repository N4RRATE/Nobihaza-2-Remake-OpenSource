#pragma once
#include "combat_scene.h"
// Costume motion belongs to the current Scene, independently of combat party/health.
class EscortController{
public:
    EscortController(CombatScene&,const nlohmann::json&);
    void update();
    int value(const std::string& field) const;
    void assign(const std::string& field,int value);
private:
    void picture(const std::string&,GridPosition);
    CombatScene& scene_;
    const nlohmann::json& profile_;
    int enabled_{},frame_{},lastFacing_{},walkPhase_{},movementKey_{},page_{};
    std::string facing_;
};
void installEscortContent(CombatContent&,const nlohmann::json&);
