#pragma once
#include "combat_scene.h"
// Encounter-local timelines; World owns actors and Session owns the source work fields.
class CrocodileController{
public:
    CrocodileController(CombatScene&,const nlohmann::json&);
    void update();
    AttackResolution receive(const AttackSample&,std::set<std::string>&);
    void movementSpeed();
private:
    void enter(int mode);
    void updateBody();
    void updateFeet();
    void updateGas();
    void hitPlayer();
    void updateBurn();
    void picture(const std::string&,GridPosition);
    void emit(const std::string&,int,int);
    AttackContact hit(int slot,int page,const AttackSample* sample=nullptr);
    GridPosition center(int slot) const;
    GridPosition attackScreen(GridPosition) const;
    bool inFront(int range) const;
    CombatScene& scene_;
    nlohmann::json profile_;
    int mode_{-1},bodyWait_{},iterations_{},gasWait_{},gasFrame_{-1},gasTick_{},hitWait_{};
    bool finished_{};

};
