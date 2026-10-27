#include "cash_controller.h"
#include "combat_scene.h"
#include "full_content.h"
#include "scene_presentation.h"
#include "weapon_controller.h"
#include "enemy_host.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
CashController::CashController(CombatScene& s,const nlohmann::json& p):scene_(s),profile_(p),generation_(s.generation_){if(const auto* patch=difficultyControllerRules(s.content_,s.session_,"cash")){profile_.merge_patch(*patch);}}
bool CashController::live() const{return !finished_&&!retired_&&scene_.active_&&scene_.generation_==generation_&&scene_.session_.hp()>0&&scene_.foreground_!=ForegroundKind::Death&&scene_.foreground_!=ForegroundKind::Failure;}
std::string CashController::actorName(int event,const std::string& self) const{
 if(event==10001){return "player";}if(event==10005){return self;}
 for(const auto& actor:scene_.world_->objects()){if(actor.eventId==event){return actor.id;}}
 throw std::logic_error("Unbound Cash character operand");
}
int CashController::characterValue(int event,int field,const std::string& self) const{
 const auto id=actorName(event,self);const auto& actor=findCharacter(*scene_.world_,id);
 if(field==1){return actor.motion.tile().x;}if(field==2){return actor.motion.tile().y;}if(field==3){switch(actor.motion.facing()){case Direction::Down:return 2;case Direction::Left:return 4;case Direction::Right:return 6;case Direction::Up:return 8;}}
 const auto screen=scene_.sampleActorScreen(id);if(field==4){return int(screen.x);}if(field==5){return int(screen.y);}
 throw std::logic_error("Unbound Cash character attribute");
}
void CashController::locate(int event,int x,int y,const std::string& self){auto actor=findCharacter(*scene_.world_,actorName(event,self));actor.motion=CharacterMotion({x,y},actor.motion.facing(),actor.motion.speed());replaceCharacter(*scene_.world_,actor);}
int CashController::terrain(int x,int y) const{return EnemyHost(scene_,profile_.at("object")).terrain({x,y});}
void CashController::call(Work& work,int page){if(work.stack.size()>=16){throw std::logic_error("Cash source call depth exceeded");}work.stack.push_back({page,0});}
bool CashController::run(Work& work,bool pauseMessages){
 if(work.wait>0){--work.wait;if(work.wait>0){return false;}}
 if(pauseMessages&&scene_.messageRequest_){return false;}
 for(int commands=0;commands<8192&&!work.stack.empty();++commands){const auto result=advance(work,work.stack.back());if(result==Step::Done){work.stack.pop_back();}else if(result==Step::Yield){return false;}}
 if(!work.stack.empty()){throw std::logic_error("Cash source loop failed to yield");}return true;
}
bool CashController::play(const std::string& cue,const std::string& self,Work& work){
 if(profile_.at("cue_bindings").contains(cue)){const auto& xy=profile_.at("cue_bindings").at(cue);scene_.pictureAnchors_[cue]={double(scene_.session_.variable(xy.at(0))),double(scene_.session_.variable(xy.at(1)))};}
 const int old=scene_.wait_;scene_.wait_=0;const bool ready=ScenePresentation(scene_).executeCue(cue,self);work.wait=std::max(work.wait,scene_.wait_);scene_.wait_=old;return ready;
}
void CashController::common5(){scene_.cancelPlayerActions("ce5");if(!scene_.session_.flag(10)){scene_.session_.setVariable(29,0);}}
void CashController::common6(){if(!scene_.session_.flag(574)){scene_.playSound("ce6-c3");}}
void CashController::common11(){if(!receiver_){throw std::logic_error("Cash receiver damage without attack");}const auto hit=EnemyHost(scene_,profile_.at("object")).receivePartDamage(*receiver_,scene_.session_.variable(49));receiverEnd_=hit.endRequested;if(scene_.session_.flag(12)){scene_.session_.setVariable(34,hit.damage/2);}}
void CashController::common12(){EnemyHost(scene_,profile_.at("object")).receiveSourceDamage(scene_.session_.variable(23));}
void CashController::common55(){scene_.session_.resetRoomFacts();}
void CashController::retire(){
 if(retired_){return;}retired_=true;parallel_.clear();foregroundWork_={};receiver_.reset();
 const auto owns=[&](const std::string& id){return std::find(profile_.at("events").begin(),profile_.at("events").end(),id)!=profile_.at("events").end();};
 std::erase_if(scene_.routes_,[&](const auto& route){return owns(route.actor());});std::erase_if(scene_.ambientRoutes_,[&](const auto& route){return owns(route.actor());});
 std::set<int> erased;for(const auto& [id,cueDefinition]:scene_.content_.cues){const auto& cue=*cueDefinition;if(id.starts_with("m191-")&&cue.at("kind")=="erase_picture"&&erased.insert(cue.at("picture_id").get<int>()).second){scene_.cue(id);}}
}
InputFrame CashController::recoveryInput(const InputFrame& input){
 if(!scene_.session_.flag(45)||!live()){recoveryWaiting_=false;return input;}
 // CE32 wait-input starts by clearing its pending trigger.
 auto sampled=input;if(!recoveryWaiting_){for(auto action:{InputAction::Up,InputAction::Right,InputAction::Left,InputAction::Down}){sampled.buttons[int(action)].pressed=false;}recoveryWaiting_=true;}
 else if(!scene_.messageRequest_&&!scene_.inputRequest_&&std::any_of(input.buttons.begin(),input.buttons.begin()+4,[](const auto& b){return b.pressed;})){recoveryWaiting_=false;}
 return sampled;
}
std::optional<std::string> CashController::update(const InputFrame& input){
 advancePlayerBlood();
 if(!live()){return {};}
 auto& s=scene_.session_;const auto player=scene_.sampleActorScreen("player");s.setVariable(1,scene_.world_->player().motion.tile().x);s.setVariable(2,scene_.world_->player().motion.tile().y);s.setVariable(3,int(player.x));s.setVariable(4,int(player.y)-8);
 // E2 precedes the remaining map events; each has an independent continuation.
 const auto updateEvents=[&]{
 for(const auto& row:profile_.at("events")){
  const auto id=row.get<std::string>();if(id==profile_.at("object").get<std::string>()){continue;}const auto* selected=scene_.content_.activeVariant(scene_.content_.objects.at(id),s);auto& work=parallel_[id];
  if(!selected||selected->definition.trigger!=4){work={};continue;}
  const int page=findCharacter(*scene_.world_,id).eventId*100+selected->page;
  if(work.page!=page){work={};work.page=page;call(work,page);}
  else if(work.stack.empty()){call(work,page);}
  const bool pause=id!="m191-e17"&&scene_.messageRequest_&&!scene_.messageState_.display.continueEvents;
  run(work,pause);if(s.hp()<=0){break;}
 }
 };
 if(s.hp()<=0){return {};}
 (void)input;
 const auto* selected=scene_.content_.activeVariant(scene_.content_.objects.at(profile_.at("object").get<std::string>()),s);if(!selected){return {};}
 if(!foregroundWork_.stack.empty()){
  if(run(foregroundWork_)&&winTransfer_){finished_=true;scene_.finishForeground();return profile_.at("win_flow").get<std::string>();}
  if(!foregroundWork_.stack.empty()){updateEvents();return {};}
 }
 selected=scene_.content_.activeVariant(scene_.content_.objects.at(profile_.at("object").get<std::string>()),s);
 if(selected->page==24){
  if(scene_.foreground_!=ForegroundKind::None&&scene_.foreground_!=ForegroundKind::Boss){return {};}
  winning_=true;scene_.weapon_->cancel(ActionCancel::Foreground);scene_.startForeground(ForegroundKind::Boss);foregroundWork_={};foregroundWork_.page=200+selected->page;call(foregroundWork_,foregroundWork_.page);run(foregroundWork_);updateEvents();return {};
 }
 if(scene_.foreground_==ForegroundKind::Boss){scene_.finishForeground();}
 if(selected->definition.trigger==4){auto& work=parallel_[profile_.at("object").get<std::string>()];const int page=200+selected->page;if(work.page!=page){work={};work.page=page;call(work,page);}else if(work.stack.empty()){call(work,page);}run(work,scene_.messageRequest_&&!scene_.messageState_.display.continueEvents);}
 updateEvents();EnemyHost host(scene_,profile_.at("object"));const GridPosition body{double(s.variable(83)),double(s.variable(84))};s.setVariable(89,s.variable(89)+host.advanceBurn(body));host.advanceBlood(profile_.at("blood"),s.variable(92));return {};
}
void CashController::beginPlayerBlood(int type){
 if(playerBloodTicks_>0){return;}
 static const std::map<int,std::string> effects{{1,"blood-small"},{2,"blood-medium"},{3,"blood-large"},{11,"blood-small-low"}};
 if(!effects.contains(type)){scene_.session_.setFlag(8,false);return;}
 playerBloodType_=type;
 for(const auto& wait:scene_.content_.media.at("effects").at(effects.at(type)).at("wait_zero_count_after_stage")){playerBloodTicks_+=wait.get<int>();}
 if(playerBloodTicks_<=0){throw std::logic_error("Player blood effect does not yield");}
 scene_.presentation_.push_back({PresentationEvent::Kind::Effect,effects.at(type),{},scene_.world_->player().motion.position(),0});
}
void CashController::advancePlayerBlood(){
 if(playerBloodTicks_<=0||--playerBloodTicks_>0){return;}
 const int selected=scene_.session_.variable(24);
 // CE14 checks later branches after their predecessor's four Wait0s; C81 clears S8.
 if(selected>playerBloodType_&&(selected==2||selected==3||selected==11)){beginPlayerBlood(selected);}
 else{playerBloodType_=0;scene_.session_.setFlag(8,false);}
}
void CashController::sourceFlag(int flag,bool value){
 scene_.session_.setFlag(flag,value);
 if(flag==8&&value){beginPlayerBlood(scene_.session_.variable(24));}
 if(flag==13&&value){const auto& waits=scene_.content_.full->at("runtime").at("player_recovery").at("ce13").at("wait_ticks");scene_.session_.startRecovery(0,waits.at(0).get<int>()+waits.at(1).get<int>());}
}
AttackResolution CashController::receive(const AttackSample& sample,std::set<std::string>& resolved){
 AttackResolution result;const auto id=profile_.at("object").get<std::string>();auto& s=scene_.session_;
 if(!live()||winning_||resolved.contains(id)||s.variable(90)!=2){return result;}
 const auto camera=scene_.camera_.center();const GridPosition center{sample.center.x-camera.x+160,sample.center.y-camera.y+120};
 if(std::abs(center.x-s.variable(83))>sample.halfPixels[0]+s.variable(87)||std::abs(center.y-s.variable(84))>sample.halfPixels[1]+s.variable(88)){return result;}
 ++result.contacts;EnemyHost host(scene_,id);

 if(sample.receiverPage<1||sample.receiverPage>6||(sample.kind==AttackKind::GlFlame&&s.equipped(1)!=59&&host.burning())){return result;}
 const int before=s.variable(89);receiverEnd_=false;receiver_=sample;Work work;call(work,200+sample.receiverPage);run(work);receiver_.reset();
 if(sample.kind==AttackKind::GlFlame){host.startBurn();}
 if(sample.kind==AttackKind::Rocket||s.equipped(1)==59){s.setFlag(37,false);host.startBurn();}
 if(sample.kind==AttackKind::Magnum){s.setFlag(16,true);}
 s.setFlag(591,true);resolved.insert(id);++result.processed;result.damage=s.variable(89)-before;result.endRequested=s.flag(16)||receiverEnd_;return result;
}
#include "cash_program.inc"
