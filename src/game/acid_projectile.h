#pragma once
#include "enemy_host.h"
// Source map projectile owns only its World actor and route, independently of its launcher.
class AcidProjectile{
public:
 AcidProjectile(CombatScene&,std::string,nlohmann::json);
 void update();void retire(){retired_=true;route_.reset();}
private:
 EnemyHost host_;nlohmann::json rules_;bool flying_{},retired_{};int launchWait_{};
 std::optional<RoutePlayer> route_;
 void stop();
};
