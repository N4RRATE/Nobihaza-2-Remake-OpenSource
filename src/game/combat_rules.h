#pragma once
#include "world_geometry.h"
#include <array>
#include <cstdint>
#include <functional>
enum class AttackKind{Handgun,Knife,Magnum,De,Shotgun,Grenade,Sg,Lc9,Ar,GlNormal,GlFlame,GlAcid,SemiAutoShotgun,HandgunHp,Rocket,Saa,InfiniteAr};
enum class AttackPhase{Impact,Flight,Burning,ResolvedContact};
bool grenadeLauncher(AttackKind);
bool auxiliaryDamage(AttackKind);
// CN CE11/12: integers truncate at each original arithmetic operation.
int biteDamage(int attack,int defense,int randomAdjustment);
int handgunDamage(int attack,int defense,int advances,int selector,int adjustment);
bool criticalHit(int qualifiedHits,int roll);
struct WeaponDamage{int amount{};bool endRequested{};};
WeaponDamage rollWeaponDamage(AttackKind,int attack,int defense,int advances,const std::function<int(int,int)>& random);
int mainCriticalThreshold(AttackKind);
bool hitOverlap(GridPosition attack,GridPosition target,int attackHalf,int targetHalf);

// A collision may be acknowledged without dealing damage (for example a
// ceiling licker). Projectile propagation is a separate caller policy.
struct AttackContact{bool contact{},processed{};int damage{};bool endRequested{};};
struct AttackResolution{int contacts{},processed{},damage{};bool endRequested{};};

// Geometry and receiver are selected by the attack before source-ordered targets.
struct AttackSample{
    GridPosition center;
    std::array<int,2> halfPixels{};
    AttackKind kind{AttackKind::Handgun};
    int receiverPage{2};
    int advances{};
    bool aimed{};
    std::uint64_t attackId{};
    AttackPhase phase{AttackPhase::Impact};
};
void validateAttackSample(const AttackSample&);
