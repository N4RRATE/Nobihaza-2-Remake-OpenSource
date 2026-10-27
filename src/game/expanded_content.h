#pragma once
#include "item_definition.h"
#include <map>
#include <string>
#include <array>
#include <optional>
#include <nlohmann/json.hpp>
enum class EnemyBehaviorKind{Licker,RunningZombie,FallingZombie,SoldierZombie};
struct EnemyBlueprint{
    std::string id;
    int map{},event{},sourceOrder{},sourceSlot{},deadFlag{},attack{},defense{},terminationDamage{};
    std::optional<int> activationFlag;
    EnemyBehaviorKind behavior{};
    std::array<int,2> hitboxHalfPixels{};
    nlohmann::json rules;
};
enum class ProjectileStrategy{MagnumFlight,ShotgunDistance};
struct AttackDefinition{
    ProjectileStrategy strategy{};
    bool muzzleTest{},endOnTarget{};
    std::array<int,2> halfPixels{};
    int forwardOffset{},responsePage{},wallTerrain{};
    std::array<int,4> muzzleForward{},responseByAdvance{};
    std::array<int,2> muzzleHalf{},verticalHalf{},horizontalHalf{};
    int endOnHitFromAdvance{},sampleLeadPixels{},flightCalls{};
};
// Incremental consumers validate and lower their finite operations before enabling them.
// Raw original command arrays are never stored or executed here.
struct ExpandedContent{
    std::map<std::string,EnemyBlueprint> enemies;
    std::map<int,AttackDefinition> attacks;
    nlohmann::json behaviorDefinitions,flows,encounters,entryPlacements,environment,bloodLoop;
    nlohmann::json reloadEquipment,savePoints,failureFlows,mapMusic;
    nlohmann::json stagedFlows,tool,mixing,meeting,meetingStory;
    std::optional<MixingRecipe> mixingRecipe;
};
