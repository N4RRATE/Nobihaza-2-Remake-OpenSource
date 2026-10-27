#pragma once
#include "enemy_controller.h"
#include "enemy_host.h"
#include "combat_content.h"
// Six original source profiles share finite action ownership; no event interpreter.
class ChimeraBehavior final:public EnemyController{
public:
 ChimeraBehavior(CombatScene&,const EnemyDefinition&);
 const std::string& id()const override{return definition_.id;}
 void update(const InputFrame&)override;
 void updateParallel()override;
 bool hasParallelMapBehavior()const override{return true;}
 void updateDeathPresentation()override{}
 void retire()override;
 AttackContact receiveAttack(const AttackSample&)override;
 EnemyMode mode()const override;
 int sourceState()const override;
 int accumulatedDamage()const override;
 bool holdsPlayer()const override{return host_.holdsPlayer();}
 bool beginGrab()override;
 bool activateState(int)override;
 void collisionYield(Direction)override;
private:
 EnemyDefinition definition_;EnemyHost host_;
 int page_{},phase_{},wait_{},retreatTicks_{},managerPage_{};
 bool retired_{},pendingGrab_{},ending_{};
 std::optional<RoutePlayer> autonomy_;
 const nlohmann::json& rules()const{return definition_.rules;}
 int value(const char*)const;void set(const char*,int);
 void change(int);void finish();void applyPage(int);
 void sampleBody();void manager();void decide();void action(const InputFrame&);
 void pose(int);void playerPose(int);void release(bool);
 void play(int,int);bool timeline(const std::string&);
 bool grabRange()const;void claw(bool);
};
