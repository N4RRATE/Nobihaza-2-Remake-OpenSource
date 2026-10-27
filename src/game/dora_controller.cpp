#include "dora_controller.h"
#include "combat_scene.h"
#include "full_content.h"
#include "scene_presentation.h"
#include "weapon_controller.h"
#include "enemy_host.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
DoraController::DoraController(CombatScene& s,const nlohmann::json& p):scene_(s),profile_(p),generation_(s.generation_){if(const auto* patch=difficultyControllerRules(s.content_,s.session_,"dora")){profile_.merge_patch(*patch);}}
bool DoraController::live() const{return !finished_&&!retired_&&scene_.active_&&scene_.generation_==generation_&&scene_.session_.hp()>0&&scene_.foreground_!=ForegroundKind::Death;}
std::string DoraController::actorName(int event,const std::string& self) const{
 if(event==10001){return "player";}if(event==10005){return self;}
 for(const auto& actor:scene_.world_->objects()){if(actor.eventId==event){return actor.id;}}
 throw std::logic_error("Unbound Dora character operand");
}
int DoraController::characterValue(int event,int field,const std::string& self) const{
 const auto id=actorName(event,self);const auto& actor=findCharacter(*scene_.world_,id);
 if(field==1){return actor.motion.tile().x;}if(field==2){return actor.motion.tile().y;}
 const auto screen=scene_.sampleActorScreen(id);if(field==4){return int(screen.x);}if(field==5){return int(screen.y);}
 throw std::logic_error("Unbound Dora character attribute");
}
void DoraController::locate(int event,int x,int y,const std::string& self){auto actor=findCharacter(*scene_.world_,actorName(event,self));actor.motion=CharacterMotion({x,y},actor.motion.facing(),actor.motion.speed());replaceCharacter(*scene_.world_,actor);}
int DoraController::terrain(int x,int y) const{return EnemyHost(scene_,profile_.at("object")).terrain({x,y});}
void DoraController::call(Work& work,int page){if(work.stack.size()>=16){throw std::logic_error("Dora source call depth exceeded");}work.stack.push_back({page,0});}
bool DoraController::run(Work& work,bool pauseMessages,const std::string& parallelEvent){
 if(work.wait>0){--work.wait;if(work.wait>0){return false;}}
 if(pauseMessages&&scene_.messageRequest_){return false;}
 for(int commands=0;commands<8192&&!work.stack.empty();++commands){
  if(!parallelEvent.empty()){
   const auto* selected=scene_.content_.activeVariant(scene_.content_.objects.at(parallelEvent),scene_.session_);
   // A parallel event changing its own page retires the old tail before its next command.
   if(!selected||selected->page!=work.page%100){work={};return false;}
  }
  const auto result=advance(work,work.stack.back());if(result==Step::Done){work.stack.pop_back();}else if(result==Step::Yield){return false;}
 }
 if(!work.stack.empty()){throw std::logic_error("Dora source loop failed to yield");}return true;
}
bool DoraController::play(const std::string& cue,const std::string& self,Work& work){
 if(profile_.at("cue_bindings").contains(cue)){const auto& xy=profile_.at("cue_bindings").at(cue);scene_.pictureAnchors_[cue]={double(scene_.session_.variable(xy.at(0))),double(scene_.session_.variable(xy.at(1)))};}
 const int old=scene_.wait_;scene_.wait_=0;const bool ready=ScenePresentation(scene_).executeCue(cue,self);work.wait=std::max(work.wait,scene_.wait_);scene_.wait_=old;return ready;
}
void DoraController::common5(){scene_.cancelPlayerActions("ce5");if(!scene_.session_.flag(10)){scene_.session_.setVariable(29,0);}}
void DoraController::common6(){if(!scene_.session_.flag(574)){scene_.playSound("ce6-c3");}}
void DoraController::common11(){if(!receiver_){throw std::logic_error("Dora receiver damage without attack");}EnemyHost(scene_,profile_.at("object")).receivePartDamage(*receiver_,scene_.session_.variable(49));}
void DoraController::common12(){EnemyHost(scene_,profile_.at("object")).receiveSourceDamage(scene_.session_.variable(23));}
void DoraController::common55(){scene_.session_.resetRoomFacts();}
void DoraController::retire(){
 if(retired_){return;}retired_=true;parallel_.clear();foregroundWork_={};receiver_.reset();
 const auto owns=[&](const std::string& id){return std::find(profile_.at("events").begin(),profile_.at("events").end(),id)!=profile_.at("events").end();};
 std::erase_if(scene_.routes_,[&](const auto& route){return owns(route.actor());});std::erase_if(scene_.ambientRoutes_,[&](const auto& route){return owns(route.actor());});
 std::set<int> erased;for(const auto& [id,cueDefinition]:scene_.content_.cues){const auto& cue=*cueDefinition;if(id.starts_with("m192-")&&cue.at("kind")=="erase_picture"&&erased.insert(cue.at("picture_id").get<int>()).second){scene_.cue(id);}}
}
bool DoraController::consumeAttack(bool auxiliary){
 if(!live()||!scene_.session_.flag(507)){return false;}
 auto& s=scene_.session_;if(s.flag(225)){s.setFlag(17,false);return true;}
 const auto& groups=profile_.at(auxiliary?"auxiliary_feedback":"main_feedback");const int selected=scene_.random_(0,int(groups.size())-1);s.setVariable(44,selected);
 Work media;for(const auto& cue:groups.at(selected)){play(cue,profile_.at("object"),media);}
 s.setFlag(225,false);s.setFlag(225,true);s.setFlag(17,false);return true;
}
InputFrame DoraController::recoveryInput(const InputFrame& input){
 if(!scene_.session_.flag(45)||!live()){recoveryWaiting_=false;return input;}
 // CE32 wait-input starts by clearing its pending trigger. CE8 has its own cursor.
 auto sampled=input;if(!recoveryWaiting_){for(auto action:{InputAction::Up,InputAction::Right,InputAction::Left,InputAction::Down}){sampled.buttons[int(action)].pressed=false;}recoveryWaiting_=true;}
 else if(!scene_.messageRequest_&&!scene_.inputRequest_&&std::any_of(input.buttons.begin(),input.buttons.begin()+4,[](const auto& b){return b.pressed;})){recoveryWaiting_=false;}
 return sampled;
}
void DoraController::directionInput(const InputFrame& input){
 auto& s=scene_.session_;if(!s.flag(10)){directionWaiting_=false;directionWait_=0;return;}
 if(scene_.messageRequest_||scene_.inputRequest_){return;}
 if(directionWait_>0){--directionWait_;return;}
 if(!directionWaiting_){directionWaiting_=true;return;}
 const std::array<InputAction,4> actions{InputAction::Down,InputAction::Left,InputAction::Right,InputAction::Up};
 for(std::size_t n=0;n<actions.size();++n){if(input.buttons[int(actions[n])].pressed){s.setVariable(232,int(n)+1);s.setVariable(29,s.variable(29)+1);directionWaiting_=false;directionWait_=1;break;}}
}
bool DoraController::interact(){
 if(!live()||scene_.foreground_!=ForegroundKind::None||scene_.weapon_->raised()||scene_.session_.flag(45)){return false;}
 const auto id=profile_.at("object").get<std::string>();const auto* page=scene_.content_.activeVariant(scene_.content_.objects.at(id),scene_.session_);
 const auto& player=scene_.world_->player();const auto& actor=findCharacter(*scene_.world_,id);
 if(!page||page->page!=17||player.motion.moving()||adjacentTile(player.motion.tile(),player.motion.facing())!=actor.motion.tile()){return false;}
 scene_.weapon_->cancel(ActionCancel::Foreground);scene_.startForeground(ForegroundKind::Boss);foregroundWork_={};foregroundWork_.page=217;call(foregroundWork_,217);run(foregroundWork_);return true;
}
std::optional<std::string> DoraController::update(const InputFrame& input){
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
  const bool pause=id!="m192-e17"&&scene_.messageRequest_&&!scene_.messageState_.display.continueEvents;
  run(work,pause,id);if(s.hp()<=0){break;}
 }
 };
 if(s.hp()<=0){return {};}
 directionInput(input);
 const auto* selected=scene_.content_.activeVariant(scene_.content_.objects.at(profile_.at("object").get<std::string>()),s);if(!selected){return {};}
 if(!foregroundWork_.stack.empty()){
  if(run(foregroundWork_)&&winTransfer_){finished_=true;scene_.finishForeground();return profile_.at("win_flow").get<std::string>();}
  if(!foregroundWork_.stack.empty()){updateEvents();return {};}
 }
 selected=scene_.content_.activeVariant(scene_.content_.objects.at(profile_.at("object").get<std::string>()),s);
 if(selected->page==20||selected->page==21){
  if(scene_.foreground_!=ForegroundKind::None&&scene_.foreground_!=ForegroundKind::Boss){return {};}
  winning_=selected->page==21;scene_.weapon_->cancel(ActionCancel::Foreground);scene_.startForeground(ForegroundKind::Boss);foregroundWork_={};foregroundWork_.page=200+selected->page;call(foregroundWork_,foregroundWork_.page);run(foregroundWork_);updateEvents();return {};
 }
 if(scene_.foreground_==ForegroundKind::Boss){scene_.finishForeground();}
 if(selected->definition.trigger==4){auto& work=parallel_[profile_.at("object").get<std::string>()];const int page=200+selected->page;if(work.page!=page){work={};work.page=page;call(work,page);}else if(work.stack.empty()){call(work,page);}run(work,scene_.messageRequest_&&!scene_.messageState_.display.continueEvents,profile_.at("object").get<std::string>());}
 updateEvents();return {};
}
AttackResolution DoraController::receive(const AttackSample& sample,std::set<std::string>& resolved){
 AttackResolution result;const auto id=profile_.at("object").get<std::string>();if(!live()||winning_||resolved.contains(id)||sample.receiverPage<2||sample.receiverPage>6){return result;}
 const auto position=scene_.sampleActorScreen(id);const auto camera=scene_.camera_.center();const GridPosition center{sample.center.x-camera.x+160,sample.center.y-camera.y+120};
 if(std::abs(center.x-position.x)>sample.halfPixels[0]+8||std::abs(center.y-(position.y-8))>sample.halfPixels[1]+8){return result;}
 ++result.contacts;if(sample.phase==AttackPhase::Flight){return result;}
 receiver_=sample;Work work;call(work,200+sample.receiverPage);run(work);receiver_.reset();resolved.insert(id);++result.processed;result.damage=sample.receiverPage<=3?scene_.session_.variable(34):0;return result;
}
#include "dora_program.inc"
