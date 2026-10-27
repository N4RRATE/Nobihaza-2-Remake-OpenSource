#include "combat_rules.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
bool auxiliaryDamage(AttackKind kind){return kind==AttackKind::Knife||kind==AttackKind::Lc9||kind==AttackKind::Grenade;}
bool grenadeLauncher(AttackKind kind){return kind==AttackKind::GlNormal||kind==AttackKind::GlFlame||kind==AttackKind::GlAcid;}
int biteDamage(int attack,int defense,int randomAdjustment){return std::max(1,attack-defense/2+randomAdjustment);}
int handgunDamage(int attack,int defense,int advances,int selector,int adjustment){
    if(advances>=12){attack/=2;}else if(advances>=8){attack-=4;}
    int damage=attack-defense;
    if(selector<=1){damage-=adjustment;}else if(selector>=8){damage+=adjustment;}
    return std::max(1,damage);
}
bool criticalHit(int hits,int roll){return hits>0&&hits%3==0&&roll-hits<=7;}
bool hitOverlap(GridPosition a,GridPosition b,int attackHalf,int targetHalf){return std::abs(a.x-b.x)<=attackHalf+targetHalf&&std::abs(a.y-b.y)<=attackHalf+targetHalf;}

WeaponDamage rollWeaponDamage(AttackKind kind,int attack,int defense,int advances,const std::function<int(int,int)>& random){
    // CE11 weapon adjustment precedes defense and the fluctuation random call.
    const bool shotgun=kind==AttackKind::Shotgun||kind==AttackKind::SemiAutoShotgun;
    if(kind==AttackKind::GlAcid){defense=0;}
    if(shotgun&&advances<=2){attack+=kind==AttackKind::SemiAutoShotgun?random(25,35):random(20,40);}
    if(kind==AttackKind::Sg){if(advances>=12){attack/=3;}else if(advances>=10){attack/=2;}else if(advances>=6){attack-=5;}}
    const int selector=random(0,10);
    const int adjustment=selector<=1?random(1,5):selector>=8?random(5,10):0;
    return {handgunDamage(attack,defense,kind==AttackKind::Handgun||kind==AttackKind::Lc9?advances:0,selector,adjustment),(shotgun&&advances>2)||kind==AttackKind::Sg||kind==AttackKind::Lc9};
}
int mainCriticalThreshold(AttackKind kind){return kind==AttackKind::Saa?12:kind==AttackKind::HandgunHp?10:(kind==AttackKind::Ar||kind==AttackKind::InfiniteAr)?0:grenadeLauncher(kind)?6:(kind==AttackKind::Shotgun||kind==AttackKind::SemiAutoShotgun)?4:kind==AttackKind::Magnum||kind==AttackKind::De||kind==AttackKind::Sg||kind==AttackKind::Rocket?5:7;}
void validateAttackSample(const AttackSample& sample){
    switch(sample.kind){
    case AttackKind::InfiniteAr:case AttackKind::Saa:case AttackKind::Rocket:case AttackKind::HandgunHp:case AttackKind::Handgun:case AttackKind::Knife:case AttackKind::Magnum:case AttackKind::De:
    case AttackKind::Shotgun:case AttackKind::SemiAutoShotgun:case AttackKind::Grenade:case AttackKind::Sg:case AttackKind::Lc9:
    case AttackKind::Ar:case AttackKind::GlNormal:case AttackKind::GlFlame:case AttackKind::GlAcid:break;
    default:throw std::invalid_argument("Unsupported weapon damage kind");
    }
    switch(sample.phase){
    case AttackPhase::Impact:return;
    case AttackPhase::Flight:if(grenadeLauncher(sample.kind)||sample.kind==AttackKind::Rocket){return;}break;
    case AttackPhase::Burning:if(sample.kind==AttackKind::GlFlame){return;}break;
    case AttackPhase::ResolvedContact:break; // Only an already resolved source callback may bypass geometry.
    }
    throw std::invalid_argument("Unsupported weapon attack phase");
}
