#include "acid_projectile.h"
#include "full_content.h"
#include <cmath>
AcidProjectile::AcidProjectile(CombatScene& s,std::string id,nlohmann::json r):host_(s,std::move(id)),rules_(std::move(r)){host_.claimAutonomy();}
void AcidProjectile::stop(){host_.session().setFlag(rules_.at("activation_fact"),false);flying_=false;route_.reset();}
void AcidProjectile::update(){
 if(retired_){return;}auto& s=host_.session();const int fact=rules_.at("activation_fact"),slot=rules_.at("slot"),base=81+(slot-1)*14;
 if(!s.flag(fact)){flying_=false;route_.reset();return;}
 if(!flying_){
  auto a=host_.actor();a.motion=CharacterMotion({s.variable(base),s.variable(base+1)},host_.sourceActorFacing(rules_.at("entity")),a.motion.speed());a.active=true;host_.setActor(a);
  host_.sound(rules_.at("launch_sound"));host_.cue(rules_.at("direction_routes").at(std::string(directionName(a.motion.facing()))));flying_=true;launchWait_=1;
  route_.emplace(host_.content().routes.at(rules_.at("flight_route").get<std::string>()),host_.id());return;
 }
 if(launchWait_>0){--launchWait_;return;}
 if(route_){host_.advance(*route_,[&](int id,bool enabled){if(id!=fact){throw std::logic_error("Unknown acid route switch");}s.setFlag(id,enabled);});}
 const auto acid=host_.sampleActorScreen(host_.id()),player=host_.sourcePlayerBody();
 if(std::abs(acid.x-player.x)<=6&&std::abs(acid.y-8-player.y)<=6){
  host_.cue(rules_.at("corrosion_animation"));host_.cue(rules_.at("corrosion_flash"));const auto* selected=difficultyControllerRules(host_.content(),s,"access_keys");host_.receiveSourceDamage(selected?selected->at("acid_spirit"):host_.content().catalog.at("access_keys").at("acid_spirit"));
  if(host_.random(0,10)>=6){s.applyCorrosion();}stop();return;
 }
 const auto a=host_.actor();const auto front=adjacentTile(a.motion.tile(),a.motion.facing());if(host_.terrain(front)==3){stop();return;}
 const int receiver=host_.receiverAt(front);if(receiver!=0&&receiver!=1&&receiver!=4){stop();}
}
