#include "final_boss_controller.h"
#include "combat_scene.h"
#include "full_content.h"
#include "enemy_host.h"
#include "scene_presentation.h"
#include "weapon_controller.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
FinalBossController::FinalBossController(CombatScene& scene,const nlohmann::json& profile):scene_(scene),profile_(profile),generation_(scene.generation_){
 if(scene_.map_!=profile_.at("map")||profile_.at("actor_attributes").at("6")!=17||profile_.at("actor_attributes").at("7")!=38){throw std::logic_error("Final boss requires reviewed NORMAL content");}
 if(const auto* patch=difficultyControllerRules(scene.content_,scene.session_,"final_boss")){profile_.merge_patch(*patch);}
}
bool FinalBossController::live() const{return !retired_&&!finished_&&scene_.active_&&scene_.generation_==generation_&&scene_.session_.hp()>0&&scene_.foreground_!=ForegroundKind::Death&&scene_.foreground_!=ForegroundKind::Failure;}
void FinalBossController::updateFreeControl(const InputFrame& input){
 if(!live()){return;}
 if(!scene_.session_.flag(18)){scene_.movePlayer(input);return;}
 if(input.buttons[int(InputAction::Cancel)].pressed&&scene_.session_.flag(1)){scene_.session_.organizeAmmo();scene_.requestedMode_=ApplicationMode::Equipment;return;}
 if(!input.buttons[int(InputAction::Weapon)].held){scene_.movePlayer(input);}
}
bool FinalBossController::airEquipped() const{return scene_.session_.equipped(1)==23;}
std::string FinalBossController::actorName(int event,const std::string& self) const{
 if(event==10001){return "player";}if(event==10005){return self;}
 for(const auto& actor:scene_.world_->objects()){if(actor.eventId==event){return actor.id;}}
 throw std::logic_error("Unbound final character");
}
int FinalBossController::characterValue(int event,int field,const std::string& self) const{
 const auto id=actorName(event,self);const auto actor=findCharacter(*scene_.world_,id);
 if(field==1){return actor.motion.tile().x;}if(field==2){return actor.motion.tile().y;}
 if(field==3){return static_cast<int>(actor.motion.facing())==0?8:static_cast<int>(actor.motion.facing())==1?6:static_cast<int>(actor.motion.facing())==2?2:4;}
 const auto screen=scene_.sampleActorScreen(id);if(field==4){return int(screen.x);}if(field==5){return int(screen.y);}
 throw std::logic_error("Unbound final character attribute");
}
void FinalBossController::locate(int event,int x,int y,const std::string& self){auto actor=findCharacter(*scene_.world_,actorName(event,self));actor.motion=CharacterMotion({x,y},actor.motion.facing(),actor.motion.speed());replaceCharacter(*scene_.world_,actor);}
void FinalBossController::call(Work& work,int page){if(work.stack.size()>=16){throw std::logic_error("Final source call depth exceeded");}work.stack.push_back({page,0});}
bool FinalBossController::run(Work& work,bool pauseMessages){
 if(work.wait>0&&--work.wait>0){return false;}if(pauseMessages&&scene_.messageRequest_){return false;}
 for(int commands=0;commands<8192&&!work.stack.empty();++commands){const auto step=advance(work,work.stack.back());if(step==Step::Done){work.stack.pop_back();}else if(step==Step::Yield){return false;}}
 if(!work.stack.empty()){throw std::logic_error("Final fixed page failed to yield");}return true;
}
bool FinalBossController::play(const std::string& cue,const std::string& self,Work& work){
 if(profile_.at("cue_bindings").contains(cue)){const auto& xy=profile_.at("cue_bindings").at(cue);scene_.pictureAnchors_[cue]={double(scene_.session_.variable(xy.at(0))),double(scene_.session_.variable(xy.at(1)))};}
 const int old=scene_.wait_;scene_.wait_=0;const bool ready=ScenePresentation(scene_).executeCue(cue,self);work.wait=std::max(work.wait,scene_.wait_);scene_.wait_=old;return ready;
}
void FinalBossController::common5(){airAction_={};decisionSampled_=false;scene_.cancelPlayerActions("ce5");if(!scene_.session_.flag(10)){scene_.session_.setVariable(29,0);}}
void FinalBossController::common6(){if(!scene_.session_.flag(574)){scene_.playSound("ce6-c3");}}
void FinalBossController::common11(){
 if(!receiver_){throw std::logic_error("Final receiver without attack");}
 auto& s=scene_.session_;
 if(receiver_->kind==AttackKind::Rocket&&s.equipped(1)==23){
  // CE11 has no I23 critical branch; I22's threshold5 must not leak into air cannon.
  const int count=s.variable(35)+1;s.setVariable(35,count);s.setFlag(12,false);
  const auto hit=rollWeaponDamage(AttackKind::Rocket,s.attack(),s.variable(49),0,scene_.random_);s.setVariable(34,hit.amount);receiverEnd_=hit.endRequested;
  if(!s.flag(47)&&count%3==0){(void)scene_.random_(0,100);}s.setFlag(47,false);return;
 }
 const auto hit=EnemyHost(scene_,profile_.at("object")).receivePartDamage(*receiver_,s.variable(49));receiverEnd_=hit.endRequested;
 // Source CE11 returns pre-critical V34; each explicit receiver page doubles it itself.
 if(s.flag(12)){s.setVariable(34,hit.damage/2);}
}
void FinalBossController::common12(){EnemyHost(scene_,profile_.at("object")).receiveSourceDamage(scene_.session_.variable(23));}
void FinalBossController::sourceFlag(int flag,bool value){
 auto& s=scene_.session_;s.setFlag(flag,value);
 if(flag==6&&value){beginAir(false);}
 if(flag==8&&value){beginPlayerBlood(s.variable(24));}
 if(flag==13&&value){const auto& waits=scene_.content_.full->at("runtime").at("player_recovery").at("ce13").at("wait_ticks");s.startRecovery(0,waits.at(0).get<int>()+waits.at(1).get<int>());}
}
void FinalBossController::storyEquip(int item){
 auto& s=scene_.session_;const int slot=item==23?1:3;
 if((item!=23&&item!=49)||!s.canEquip(item,slot)){throw std::logic_error("Invalid final story equipment");}
 if(s.equipped(slot)==item){return;}
 // Only these two source commits may materialize zero-stock equipment. Menu equip stays strict.
 const bool absent=s.count(item)==0;if(absent){s.grant(item,1);}if(!s.equip(item,slot)){if(absent){s.discard(item,1);}throw std::logic_error("Final story equipment rejected");}
 if(item==23){scene_.weapon_->cancel(ActionCancel::Foreground);}
}
void FinalBossController::beginAir(bool player){
 auto& s=scene_.session_;const auto id=profile_.at("air").at("projectile").get<std::string>();
 std::optional<RoutePlayer> preparation;
 if(!player){for(const auto& route:scene_.routes_){if(route.actor()==id&&route.source()!="final-air"){preparation=route;break;}}}
 endAir();if(preparation){scene_.routes_.push_back(std::move(*preparation));}s.setFlag(6,true);s.setFlag(7,false);s.setFlag(16,false);s.setFlag(591,false);s.setFlag(9,true);airResolved_.clear();++airId_;airActive_=true;airStartYield_=true;airExploded_=false;
 auto actor=findCharacter(*scene_.world_,id);
 if(player){const auto p=scene_.world_->player();actor.motion=CharacterMotion(p.motion.tile(),p.motion.facing(),actor.motion.speed());}
 actor.through=true;actor.routeThrough=true;replaceCharacter(*scene_.world_,actor);
 const auto& object=scene_.content_.objects.at(id);const auto variant=std::find_if(object.variants.begin(),object.variants.end(),[&](const auto& v){return v.page==profile_.at("air").at("flight_page");});
 if(variant==object.variants.end()||variant->autonomousRoute.steps.empty()){throw std::logic_error("Missing final air route");}
 // The source forced turn completes before E1 advances its autonomous flight.
 scene_.ambientRoutes_.emplace_back(variant->autonomousRoute,id,"final-air");
}
void FinalBossController::endAir(){
 const auto id=profile_.at("air").at("projectile").get<std::string>();std::erase_if(scene_.routes_,[&](const auto& route){return route.actor()==id;});std::erase_if(scene_.ambientRoutes_,[&](const auto& route){return route.actor()==id;});
 airActive_=false;airReceiver_={};airExplosion_={};airResolved_.clear();
}
void FinalBossController::airContact(){
 auto& s=scene_.session_;if(s.flag(591)||s.variable(90)!=10){return;}
 const auto camera=scene_.camera_.center();const GridPosition center{s.variable(17)+camera.x-160.0,s.variable(18)+camera.y-120.0};
 // CE4 sets S16 for a rocket overlap before the explicit explosion receiver page.
 const bool overlap=std::abs(s.variable(17)-s.variable(83))<=s.variable(47)+s.variable(87)&&std::abs(s.variable(18)-s.variable(84))<=s.variable(48)+s.variable(88);
 if(overlap&&!s.flag(16)&&s.variable(91)<100){s.setFlag(16,true);return;}
 if(overlap){const auto hit=receive({center,{s.variable(47),s.variable(48)},AttackKind::Rocket,s.variable(22),0,false,airId_},airResolved_);if(hit.processed){s.setFlag(591,true);}}
}
void FinalBossController::advanceAir(){
 if(!airActive_){return;}auto& s=scene_.session_;if(s.flag(582)){endAir();return;}
 if(airStartYield_){airStartYield_=false;return;}
 if(!airExploded_){
  Work contact;call(contact,profile_.at("air").at("ce44"));run(contact,false);
  if(s.flag(16)){airExploded_=true;call(airExplosion_,profile_.at("air").at("ce7"));call(airReceiver_,profile_.at("air").at("ce45"));}
 }
 if(airExploded_){
  const bool pictureDone=run(airExplosion_,false),receiverDone=run(airReceiver_,false);
  if(pictureDone&&receiverDone){endAir();s.setFlag(5,false);s.setFlag(6,false);s.setFlag(7,false);s.setFlag(16,false);s.setFlag(591,false);locate(1,0,0,"m209-e1");}
 }
}
void FinalBossController::updateAirInput(const InputFrame& input){
 if(!airEquipped()){airAction_={};decisionSampled_=false;return;}auto& s=scene_.session_;
 if(!airAction_.stack.empty()){
  if(run(airAction_,false)){s.setFlag(17,false);decisionSampled_=false;}return;
 }
 const bool permitted=(s.flag(574)||s.flag(1))&&s.flag(2)&&!s.flag(19)&&!s.flag(5)&&!scene_.messageRequest_&&!scene_.inputRequest_;
 if(!permitted){decisionSampled_=false;return;}
 const auto& attack=input.buttons[static_cast<std::size_t>(InputAction::Attack)];
 if(!decisionSampled_){decisionSampled_=true;decisionAttack_=attack.held||attack.pressed;return;}
 decisionSampled_=false;if(!decisionAttack_||!(attack.held||attack.pressed)){return;}
 s.setVariable(229,5);s.setFlag(17,true);s.setVariable(7,23);call(airAction_,profile_.at("air").at("action"));run(airAction_,false);
}
InputFrame FinalBossController::recoveryInput(const InputFrame& input){
 if(!scene_.session_.flag(45)||!live()){recoveryWaiting_=false;return input;}
 auto sampled=input;if(!recoveryWaiting_){for(auto action:{InputAction::Up,InputAction::Right,InputAction::Left,InputAction::Down}){sampled.buttons[int(action)].pressed=false;}recoveryWaiting_=true;}
 else if(!scene_.messageRequest_&&!scene_.inputRequest_&&std::any_of(input.buttons.begin(),input.buttons.begin()+4,[](const auto& b){return b.pressed;})){recoveryWaiting_=false;}return sampled;
}
std::optional<FinalBossController::Victory> FinalBossController::update(const InputFrame& input){
 advancePlayerBlood();if(!live()){return {};}
 auto& s=scene_.session_;const auto p=scene_.sampleActorScreen("player");s.setVariable(1,scene_.world_->player().motion.tile().x);s.setVariable(2,scene_.world_->player().motion.tile().y);s.setVariable(3,int(p.x));s.setVariable(4,int(p.y)-8);
 // E1 resolves before E4; parallel E4/E5/E6 remain active throughout E10's foreground.
 advanceAir();updateAirInput(input);
 const auto& bossId=profile_.at("object").get_ref<const std::string&>();
 if(s.flag(582)&&!s.flag(15)&&foreground_.stack.empty()){
  scene_.weapon_->cancel(ActionCancel::Foreground);endAir();cancelRoutes();finished_=true;return Victory{generation_,bossId,profile_.at("winning_page")};
 }
 if(!foreground_.stack.empty()){if(run(foreground_)){scene_.finishForeground();}}
 if(foreground_.stack.empty()){
  const std::string entry=s.flag(581)?bossId:"m209-e4";const auto* selected=scene_.content_.activeVariant(scene_.content_.objects.at(entry),s);
  if(selected&&selected->definition.trigger==3&&((entry==bossId&&selected->page==8)||entry=="m209-e4")){
   scene_.startForeground(ForegroundKind::Boss);foreground_={};foreground_.page=selected->page+(entry==bossId?1000:400);call(foreground_,foreground_.page);run(foreground_);
  }
 }
 for(const auto& row:profile_.at("events")){
  const auto id=row.get<std::string>();if(id==profile_.at("air").at("projectile").get<std::string>()||erased_.contains(id)){continue;}
  const auto* selected=scene_.content_.activeVariant(scene_.content_.objects.at(id),s);auto& work=parallel_[id];
  if(!selected||selected->definition.trigger!=4){work={};continue;}
  const int page=findCharacter(*scene_.world_,id).eventId*100+selected->page;
  if(work.page!=page){work={};work.page=page;call(work,page);}else if(work.stack.empty()){call(work,page);}
  run(work,scene_.messageRequest_&&!scene_.messageState_.display.continueEvents);if(s.hp()<=0||s.flag(582)){break;}
 }
 EnemyHost host(scene_,bossId);s.setVariable(89,s.variable(89)+host.advanceBurn(GridPosition{double(s.variable(83)),double(s.variable(84))}));host.advanceBlood(profile_.at("blood"),s.variable(92));return {};
}
AttackResolution FinalBossController::receive(const AttackSample& sample,std::set<std::string>& resolved){
 AttackResolution result;const auto id=profile_.at("object").get<std::string>();auto& s=scene_.session_;
 if(!live()||s.flag(582)||resolved.contains(id)||s.variable(90)!=10){return result;}
 const auto camera=scene_.camera_.center();const GridPosition center{sample.center.x-camera.x+160,sample.center.y-camera.y+120};
 if(std::abs(center.x-s.variable(83))>sample.halfPixels[0]+s.variable(87)||std::abs(center.y-s.variable(84))>sample.halfPixels[1]+s.variable(88)){return result;}
 ++result.contacts;if(sample.receiverPage<2||sample.receiverPage>6){return result;}
 EnemyHost host(scene_,id);if(sample.kind==AttackKind::GlFlame&&host.burning()){return result;}
 const int before=s.variable(89);receiver_=sample;receiverEnd_=false;Work work;call(work,1000+sample.receiverPage);run(work,false);receiver_.reset();
 if(sample.kind==AttackKind::GlFlame){host.startBurn();}if(sample.kind==AttackKind::Rocket&&s.equipped(1)!=23){s.setFlag(37,false);host.startBurn();}
 s.setFlag(591,true);resolved.insert(id);++result.processed;result.damage=s.variable(89)-before;result.endRequested=s.flag(16)||receiverEnd_;return result;
}
void FinalBossController::updateVictoryPresentation(){
 if(!finished_||retired_||!scene_.active_||scene_.generation_!=generation_||!scene_.session_.flag(582)||!scene_.content_.catalog.contains("epilogue")){return;}
 // M209 E10P10 C11/20/28 still drive E5's head before its C35 transfer.
 const int head=scene_.session_.variable(105);auto& work=parallel_["m209-e5"];
 if(head>=1&&head<=5){
  const int page=500+head;if(work.page!=page){work={};work.page=page;call(work,page);}else if(work.stack.empty()){call(work,page);}
  run(work,false);
 }else{work={};}
 Work picture;call(picture,302);run(picture,false);
}
void FinalBossController::beginPlayerBlood(int type){
 if(playerBloodTicks_>0){return;}static const std::map<int,std::string> effects{{1,"blood-small"},{2,"blood-medium"},{3,"blood-large"},{11,"blood-small-low"}};
 if(!effects.contains(type)){scene_.session_.setFlag(8,false);return;}playerBloodType_=type;
 for(const auto& wait:scene_.content_.media.at("effects").at(effects.at(type)).at("wait_zero_count_after_stage")){playerBloodTicks_+=wait.get<int>();}
 if(playerBloodTicks_<=0){throw std::logic_error("Player blood effect without yield");}scene_.presentation_.push_back({PresentationEvent::Kind::Effect,effects.at(type),{},scene_.world_->player().motion.position(),0});
}
void FinalBossController::advancePlayerBlood(){
 if(playerBloodTicks_<=0||--playerBloodTicks_>0){return;}const int selected=scene_.session_.variable(24);
 if(selected>playerBloodType_&&(selected==2||selected==3||selected==11)){beginPlayerBlood(selected);}else{playerBloodType_=0;scene_.session_.setFlag(8,false);}
}
void FinalBossController::cancelRoutes(){
 const auto owns=[&](const std::string& id){return id=="player"||std::find(profile_.at("events").begin(),profile_.at("events").end(),id)!=profile_.at("events").end();};
 std::erase_if(scene_.routes_,[&](const auto& route){return owns(route.actor());});std::erase_if(scene_.ambientRoutes_,[&](const auto& route){return owns(route.actor());});
}
void FinalBossController::retire(){
 if(retired_){return;}retired_=true;endAir();parallel_.clear();foreground_={};airAction_={};receiver_.reset();
 cancelRoutes();
 std::set<int> erased;for(const auto& [id,cueDefinition]:scene_.content_.cues){const auto& cue=*cueDefinition;if((id.starts_with("m209-")||id=="ce7-c466")&&cue.at("kind")=="erase_picture"&&erased.insert(cue.at("picture_id").get<int>()).second){scene_.cue(id);}}
}
#include "final_boss_program.inc"
