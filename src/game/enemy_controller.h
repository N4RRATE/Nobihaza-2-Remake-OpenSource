#pragma once
#include "combat_rules.h"
#include "input_buffer.h"
#include <memory>
#include <optional>
class CombatScene;
struct EnemyDefinition;
struct EnemyBlueprint;
enum class EnemyMode{Dormant,Pursuit,Approach,Windup,Cooldown,HitRecovery,Evade,PendingGrab,Grab,Retreat,Ending,Corpse,Rush,Fallen,Arrival};
class EnemyController{
public:
    virtual ~EnemyController()=default;
    virtual const std::string& id() const=0;
    virtual void update(const InputFrame& input)=0;
    virtual void updateDeathPresentation()=0;
    virtual void updateParallel(){}
    virtual void retire(){}
    virtual bool hasParallelMapBehavior() const{return false;}
    virtual bool setScriptedDamage(int){return false;}
    virtual bool activateFromEncounter(){return false;}
    virtual bool activateState(int state){return state==2&&activateFromEncounter();}
    virtual AttackContact receiveAttack(const AttackSample& sample)=0;
    virtual EnemyMode mode() const=0;
    virtual int sourceState() const{return -1;}
    virtual int accumulatedDamage() const=0;
    virtual bool holdsPlayer() const=0;
    virtual bool beginGrab()=0;
    virtual void collisionYield(Direction facing)=0;
};
std::unique_ptr<EnemyController> makeEnemyBehavior(CombatScene& scene,const EnemyDefinition& definition);
std::unique_ptr<EnemyController> makeEnemyBehavior(CombatScene& scene,const EnemyBlueprint& definition);
