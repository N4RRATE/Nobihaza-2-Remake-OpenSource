#include "warehouse_g4_controller.h"
#include "combat_scene.h"
#include "full_content.h"
#include "enemy_host.h"
#include "scene_presentation.h"
#include "weapon_controller.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
WarehouseG4Controller::WarehouseG4Controller(CombatScene& s,const nlohmann::json& p):scene_(s),profile_(p),generation_(s.generation_){if(const auto* patch=difficultyControllerRules(s.content_,s.session_,"warehouse_g4")){profile_.merge_patch(*patch);}}
bool WarehouseG4Controller::valid() const{return !retired_&&scene_.active_&&scene_.generation_==generation_&&!failureCommitted_&&scene_.foreground_!=ForegroundKind::Death&&scene_.foreground_!=ForegroundKind::Failure;}
int WarehouseG4Controller::selectedPage(const std::string& id) const{const auto* p=scene_.content_.activeVariant(scene_.content_.objects.at(id),scene_.session_);return p?findCharacter(*scene_.world_,id).eventId*100+p->page:0;}
std::string WarehouseG4Controller::actorName(int event,const std::string& self) const{
 if(event==10001){return "player";}if(event==10005){return self;}
 for(const auto& a:scene_.world_->objects()){if(a.eventId==event){return a.id;}}
 throw std::logic_error("Unbound Warehouse character");
}
int WarehouseG4Controller::characterValue(int event,int field,const std::string& self) const{
 const auto id=actorName(event,self);const auto a=findCharacter(*scene_.world_,id);
 if(field==1){return a.motion.tile().x;}if(field==2){return a.motion.tile().y;}
 if(field==3){return std::array<int,4>{8,6,2,4}.at(static_cast<int>(a.motion.facing()));}
 const auto p=scene_.sampleActorScreen(id);if(field==4){return int(p.x);}if(field==5){return int(p.y);}throw std::logic_error("Unbound Warehouse character field");
}
void WarehouseG4Controller::locate(int event,int x,int y,const std::string& self){auto a=findCharacter(*scene_.world_,actorName(event,self));a.motion=CharacterMotion({x,y},a.motion.facing(),a.motion.speed());replaceCharacter(*scene_.world_,a);}
int WarehouseG4Controller::terrain(int x,int y) const{return EnemyHost(scene_,profile_.at("object")).terrain({x,y});}
void WarehouseG4Controller::call(Work& w,int page){if(w.stack.size()>=8){throw std::logic_error("Warehouse fixed call depth");}w.stack.push_back({page,0});if(page==10166){scene_.fileReadingLocked_=true;}if(page==10167){scene_.fileReadingLocked_=false;}}
bool WarehouseG4Controller::run(Work& w){
 if(w.wait>0&&--w.wait>0){return false;}
 if(scene_.messageRequest_){
  const bool entryChoice=w.page==501&&!w.stack.empty()&&(w.stack.back().next==10||w.stack.back().next==11)&&scene_.dialogue_.view().phase==DialoguePhase::AwaitingInput;
  if(!entryChoice&&!(w.refresh&&scene_.messageState_.display.continueEvents)){return false;}
 }
 for(int count=0;count<8192&&!w.stack.empty();++count){
  if(w.refresh){
   const int active=selectedPage(w.actor);
   if(active!=w.page){
    const auto* page=scene_.content_.activeVariant(scene_.content_.objects.at(w.actor),scene_.session_);
    w.stack.clear();w.wait=0;w.page=active;
    if(!page||page->definition.trigger!=4){return true;}call(w,active);
   }
  }
  const auto step=advance(w,w.stack.back());
  if(!valid()){w.stack.clear();return true;}
  if(step==Step::Done){w.stack.pop_back();}else if(step==Step::Yield){return false;}
 }
 if(!w.stack.empty()){throw std::logic_error("Warehouse source loop did not yield: page "+std::to_string(w.stack.back().page)+" command "+std::to_string(w.stack.back().next+1));}return true;
}
bool WarehouseG4Controller::play(const std::string& cue,const std::string& actor,Work& w){
 if(profile_.at("cue_bindings").contains(cue)){const auto& xy=profile_.at("cue_bindings").at(cue);scene_.pictureAnchors_[cue]={double(scene_.session_.variable(xy.at(0))),double(scene_.session_.variable(xy.at(1)))};}
 const int old=scene_.wait_;scene_.wait_=0;const bool ready=ScenePresentation(scene_).executeCue(cue,actor);w.wait=std::max(w.wait,scene_.wait_);scene_.wait_=old;return ready;
}
void WarehouseG4Controller::common5(){scene_.cancelPlayerActions("ce5");if(!scene_.session_.flag(10)){scene_.session_.setVariable(29,0);}}
void WarehouseG4Controller::common6(){if(!scene_.session_.flag(574)){scene_.playSound("ce6-c3");}}
void WarehouseG4Controller::common11(){if(!receiver_){throw std::logic_error("Warehouse receiver lacks attack");}const auto hit=EnemyHost(scene_,profile_.at("object")).receivePartDamage(*receiver_,scene_.session_.variable(49));receiverEnd_=hit.endRequested;if(scene_.session_.flag(12)){scene_.session_.setVariable(34,hit.damage/2);}}
void WarehouseG4Controller::common12(){EnemyHost(scene_,profile_.at("object")).receiveSourceDamage(scene_.session_.variable(23));}
void WarehouseG4Controller::common38(){failureCommitted_=true;scene_.startDeath(false);}
void WarehouseG4Controller::systemGraphic(const std::string& image){scene_.presentation_.push_back({PresentationEvent::Kind::SystemGraphic,image,{},{},0});}
void WarehouseG4Controller::sourceFlag(int id,bool value){
 scene_.session_.setFlag(id,value);
 if(id==8&&value){beginPlayerBlood(scene_.session_.variable(24));}
 if(id==13&&value){const auto& waits=scene_.content_.full->at("runtime").at("player_recovery").at("ce13").at("wait_ticks");scene_.session_.startRecovery(0,waits.at(0).get<int>()+waits.at(1).get<int>());}
}
void WarehouseG4Controller::beginPlayerBlood(int type){
 if(playerBloodTicks_>0){return;}
 static const std::map<int,std::string> effects{{1,"blood-small"},{2,"blood-medium"},{3,"blood-large"},{11,"blood-small-low"}};
 if(!effects.contains(type)){scene_.session_.setFlag(8,false);return;}playerBloodType_=type;
 for(const auto& wait:scene_.content_.media.at("effects").at(effects.at(type)).at("wait_zero_count_after_stage")){playerBloodTicks_+=wait.get<int>();}
 if(playerBloodTicks_<=0){throw std::logic_error("Warehouse player blood failed to yield");}
 scene_.presentation_.push_back({PresentationEvent::Kind::Effect,effects.at(type),{},scene_.world_->player().motion.position(),0});
}
void WarehouseG4Controller::advancePlayerBlood(){
 if(playerBloodTicks_<=0||--playerBloodTicks_>0){return;}const int type=scene_.session_.variable(24);
 if(type>playerBloodType_&&(type==2||type==3||type==11)){beginPlayerBlood(type);}else{playerBloodType_=0;scene_.session_.setFlag(8,false);}
}
bool WarehouseG4Controller::choice(Work&){
 if(!choice_){
  ChoicePrompt p;p.cancelType=0;p.display=scene_.messageState_.display;
  for(const auto& label:profile_.at("entry_choice")){const auto text=label.get<std::string>();p.options.push_back({text,{{MessageTokenKind::Text,decodeMessageText(text),0}}});}
  if(scene_.messageRequest_){p.question=scene_.dialogue_.view();scene_.dialogue_.invalidate();scene_.messageRequest_.reset();}
  choice_=scene_.requestInput(p);choiceCreated_=true;return false;
 }
 if(const auto result=scene_.takeInputResult(*choice_)){const auto* selected=std::get_if<ChoiceSelected>(&result->result);if(!selected){throw std::logic_error("Warehouse noncancelable choice cancelled");}choiceIndex_=selected->index;choice_.reset();return true;}return false;
}
bool WarehouseG4Controller::interact(){
 if(!valid()||scene_.foreground_!=ForegroundKind::None||scene_.weapon_->raised()||scene_.session_.flag(577)||scene_.session_.flag(45)){return false;}
 const auto id=profile_.at("entry_object").get<std::string>();const auto& p=scene_.world_->player();
 if(p.motion.moving()||adjacentTile(p.motion.tile(),p.motion.facing())!=findCharacter(*scene_.world_,id).motion.tile()){return false;}
 scene_.weapon_->cancel(ActionCancel::Foreground);scene_.startForeground(ForegroundKind::Boss,id);foreground_={};foreground_.page=501;foreground_.actor=id;call(foreground_,501);run(foreground_);return true;
}
void WarehouseG4Controller::schedule(const std::string& id){
 const auto* p=scene_.content_.activeVariant(scene_.content_.objects.at(id),scene_.session_);auto& w=parallel_[id];
 if(!p||p->definition.trigger!=4){w={};return;}
 const int page=selectedPage(id);if(w.page!=page){w={};w.page=page;w.actor=id;w.refresh=true;call(w,page);}else if(w.stack.empty()){call(w,page);}run(w);
}
void WarehouseG4Controller::update(const InputFrame&){
 choiceCreated_=false;advancePlayerBlood();if(!valid()){return;}auto& s=scene_.session_;
 if(s.hp()<=0){return;}
 const auto p=scene_.sampleActorScreen("player");s.setVariable(1,scene_.world_->player().motion.tile().x);s.setVariable(2,scene_.world_->player().motion.tile().y);s.setVariable(3,int(p.x));s.setVariable(4,int(p.y)-8);
 if(!foreground_.stack.empty()){if(run(foreground_)&&scene_.foreground_==ForegroundKind::Boss){scene_.finishForeground();}if(!valid()){return;}}
 const auto id=profile_.at("object").get<std::string>();
 if(foreground_.stack.empty()){
  schedule(id);
  const int page=selectedPage(id);
  if(page==216||page==222){
   if(scene_.foreground_==ForegroundKind::None){scene_.weapon_->cancel(ActionCancel::Foreground);scene_.startForeground(ForegroundKind::Boss,id);foreground_={};foreground_.page=page;foreground_.actor=id;call(foreground_,page);run(foreground_);}
  }
 }
 if(!valid()){return;}schedule(profile_.at("picture_object"));schedule(profile_.at("cache_object"));
 if(!s.flag(578)&&s.flag(577)){EnemyHost host(scene_,id);s.setVariable(89,s.variable(89)+host.advanceBurn(GridPosition{double(s.variable(83)),double(s.variable(84))}));host.advanceBlood(profile_.at("blood"),s.variable(92));}
}
InputFrame WarehouseG4Controller::promptInput(const InputFrame& input) const{return choiceCreated_?InputFrame{}:input;}
InputFrame WarehouseG4Controller::recoveryInput(const InputFrame& input){
 if(!valid()||!scene_.session_.flag(45)){recoveryWaiting_=false;return input;}auto sampled=input;
 if(!recoveryWaiting_){for(int i=0;i<4;++i){sampled.buttons[i].pressed=false;}recoveryWaiting_=true;}
 else if(!scene_.messageRequest_&&!scene_.inputRequest_&&std::any_of(input.buttons.begin(),input.buttons.begin()+4,[](const auto& b){return b.pressed;})){recoveryWaiting_=false;}return sampled;
}
AttackResolution WarehouseG4Controller::receive(const AttackSample& sample,std::set<std::string>& resolved){
 AttackResolution result;auto& s=scene_.session_;const auto id=profile_.at("object").get<std::string>();
 if(!valid()||s.hp()<=0||!s.flag(577)||s.flag(578)||foreground_.page==222||resolved.contains(id)||s.variable(90)!=2){return result;}
 const auto camera=scene_.camera_.center();const GridPosition c{sample.center.x-camera.x+160,sample.center.y-camera.y+120};
 if(std::abs(c.x-s.variable(83))>sample.halfPixels[0]+s.variable(87)||std::abs(c.y-s.variable(84))>sample.halfPixels[1]+s.variable(88)){return result;}++result.contacts;
 EnemyHost host(scene_,id);if(sample.receiverPage<1||sample.receiverPage>6||(sample.kind==AttackKind::GlFlame&&s.equipped(1)!=59&&host.burning())){return result;}
 const int before=s.variable(89);receiver_=sample;receiverEnd_=false;Work w;call(w,200+sample.receiverPage);run(w);receiver_.reset();
 if(sample.kind==AttackKind::GlFlame){host.startBurn();}if(sample.kind==AttackKind::Rocket||s.equipped(1)==59){s.setFlag(37,false);host.startBurn();}if(sample.kind==AttackKind::Magnum){s.setFlag(16,true);}
 s.setFlag(591,true);resolved.insert(id);++result.processed;result.damage=s.variable(89)-before;result.endRequested=receiverEnd_||s.flag(16);return result;
}
void WarehouseG4Controller::retire(){
 if(retired_){return;}retired_=true;if(choice_){scene_.cancelInput(*choice_);}choice_.reset();choiceCreated_=false;parallel_.clear();foreground_={};receiver_.reset();
 std::erase_if(scene_.routes_,[&](const auto& r){return r.actor()=="player"||r.actor()==profile_.at("object").get<std::string>();});
 std::erase_if(scene_.ambientRoutes_,[&](const auto& r){return r.actor()==profile_.at("object").get<std::string>();});
 std::set<int> erased;for(const auto& [id,cueDefinition]:scene_.content_.cues){const auto& c=*cueDefinition;if((id.starts_with("m204-")||id.starts_with("ce144-")||id.starts_with("ce145-"))&&c.at("kind")=="erase_picture"&&erased.insert(c.at("picture_id").get<int>()).second){scene_.cue(id);}}
}
#include "warehouse_g4_program.inc"
