#pragma once
#include "enemy_controller.h"
#include "enemy_host.h"
#include <unordered_set>
// Source-parametrized finite Hunter stages; world owns the only actor position.
class HunterBehavior final:public EnemyController{
public:
 HunterBehavior(CombatScene&,const EnemyDefinition&);
 const std::string& id()const override{return definition_.id;}
 void update(const InputFrame&)override;
 void updateParallel()override;
 bool hasParallelMapBehavior()const override{return rules().value("parallel_map_behavior",false);}
 void updateDeathPresentation()override{}
 void retire()override;
 AttackContact receiveAttack(const AttackSample&)override;
 EnemyMode mode()const override;
 int sourceState()const override;
 int accumulatedDamage()const override;
 bool holdsPlayer()const override{return false;}
 bool beginGrab()override{return false;}
 void collisionYield(Direction)override{}
 bool activateState(int)override;
private:
 EnemyDefinition definition_;EnemyHost host_;
 std::optional<RoutePlayer> autonomy_;
 int page_{},phase_{},wait_{},arcStep_{},height_{10},endingPage_{};
 bool retired_{},pictureShown_{};
 std::unordered_set<std::uint64_t> processed_;
 std::optional<AttackSample> delayed_;
 const nlohmann::json& rules()const{return definition_.rules;}
 int value(const char*)const;
 void set(const char*,int);
 void change(int);
 void applyPage(int);
 void cancel();
 void sampleBody();
 bool play(int,int);
 bool sequence(int,std::initializer_list<int>);
 void picture(int,int);
 int forwardDistance(int)const;
 bool arcHit()const;
 bool fatalHit()const;
 void decide();
 void action();
 void strike(bool jump);
 void jump(bool big);
 AttackContact damage(const AttackSample&,bool retreat);
 void ending();
};
