#pragma once
#include <string>
#include <nlohmann/json_fwd.hpp>
struct CombatContent;
// Lower reviewed branch definitions into the existing scene catalog.
void loadBranchExploration(CombatContent& content);

void loadBranchWeapons(CombatContent&);

void loadBranchPuzzles(CombatContent&);

void loadBranchRescues(CombatContent&);

void loadBranchReturns(CombatContent& content);
void loadBranchBlue(CombatContent& content);
void validateBranchEnemyProfile(const CombatContent&,const std::string&,const nlohmann::json&);
