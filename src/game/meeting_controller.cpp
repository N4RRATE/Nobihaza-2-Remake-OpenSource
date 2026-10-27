#include "meeting_controller.h"
#include "combat_scene.h"
#include "scene_presentation.h"
#include <cmath>
MeetingController::MeetingController(CombatScene& scene):scene_(scene),generation_(scene.generation()){
 const auto& story=scene_.content_.expansion.meetingStory;ScenePresentation presentation(scene_);
 const auto& c=story.at("conference");const auto& e=story.at("escape");
 presentation.validateSequence(c.at("before_facing"),c.at("object"));
 for(const auto& branch:c.at("facing_branches"))presentation.validateSequence(branch.at("steps"),c.at("object"));
 presentation.validateSequence(c.at("after_facing"),c.at("object"));
 auto next=nlohmann::json::array({{{"kind","scene_transfer"},{"map",e.at("map")}}});
 for(const auto& step:c.at("after_transfer"))next.push_back(step);
 for(const auto& step:e.at("steps"))next.push_back(step);
 presentation.validateSequence(next,e.at("object"));
 const auto& returning=story.at("return");auto last=nlohmann::json::array({{{"kind","scene_transfer"},{"map",returning.at("map")}}});
 for(const auto& step:returning.at("steps"))last.push_back(step);
 presentation.validateSequence(last,returning.at("object"));
}
bool MeetingController::waitingForReturn() const{return stage_==Stage::AwaitReturn;}
bool MeetingController::run(const nlohmann::json& steps,const std::string& self){
 auto& s=scene_;ScenePresentation presentation(s);
 while(cursor_<steps.size()){
  const auto& step=steps.at(cursor_);const std::string kind=step.at("kind");
  try{
   if(kind=="cue"){if(!presentation.executeCue(step.at("cue"),self))return false;}
   else if(kind=="message")presentation.message(step.at("message"));
   else if(kind=="flag"){s.session_.setFlag(step.at("id"),step.at("value"));s.refreshObjectPages();}
   else if(kind=="grant")s.session_.grant(step.at("item"),step.at("count"));
   else throw std::runtime_error("Unsupported meeting operation");
  }catch(const std::exception& e){throw std::runtime_error("Meeting "+step.at("source").dump()+": "+e.what());}
  ++cursor_;if(s.wait_>0||s.messageRequest_)return false;
 }
 return true;
}
bool MeetingController::transfer(const nlohmann::json& definition){
 auto& s=scene_;if(failed_)return false;
 try{
  const auto player=s.world_->player();const auto& xy=definition.at("tile");
  CombatEntry entry;entry.map=definition.at("map");entry.playerTile={xy.at(0),xy.at(1)};entry.direction=player.motion.facing();entry.sprite=player.spriteId.value_or("player");
  auto candidate=s.prepareWorld(entry.map,entry.playerTile,entry.direction,s.session_,&entry);
  const bool visible=s.playerVisible_;s.commitWorld(std::move(candidate));generation_=s.generation();
  auto p=s.world_->player();p.setSpeedLevel(static_cast<int>(std::log2(player.motion.speed()))-1);p.through=player.through;s.world_->setPlayerState(p);s.playerVisible_=visible;
  s.transferError_.clear();return true;
 }catch(const std::exception& e){
  failed_=true;s.transferError_=e.what();s.presentation_.push_back({PresentationEvent::Kind::Notice,"演出换图准备失败，当前场景已保留；按 Z 重试。",{},{},600});return false;
 }
}
void MeetingController::advance(){
 auto& s=scene_;if(generation_!=s.generation()||failed_)return;
 const auto& story=s.content_.expansion.meetingStory;const auto& c=story.at("conference");const auto& e=story.at("escape");
 switch(stage_){
 case Stage::BeforeFacing:
  if(run(c.at("before_facing"),c.at("object"))){
   facingSteps_=nlohmann::json::array();const auto direction=directionName(findCharacter(*s.world_,c.at("object")).motion.facing());
   // Preserve the source's two independent up conditions and route replacement.
   for(const auto& branch:c.at("facing_branches"))if(branch.at("direction")==direction)for(const auto& step:branch.at("steps"))facingSteps_.push_back(step);
   enter(Stage::Facing);
  }break;
 case Stage::Facing:if(run(facingSteps_,c.at("object")))enter(Stage::AfterFacing);break;
 case Stage::AfterFacing:if(run(c.at("after_facing"),c.at("object")))enter(Stage::Transfer25);break;
 case Stage::Transfer25:if(transfer(c.at("transfer")))enter(Stage::AfterTransfer);break;
 case Stage::AfterTransfer:if(run(c.at("after_transfer"),e.at("object")))enter(Stage::Escape);break;
 case Stage::Escape:
  if(run(e.at("steps"),e.at("object")))enter(Stage::Transfer12);break;
 case Stage::Transfer12:
  if(transfer(e.at("transfer"))){
   const auto& skill=e.at("skill_conversion");if(s.session_.knowsSkill(skill.at("if_known"))){s.session_.learnSkill(skill.at("learn"));s.session_.forgetSkill(skill.at("forget"));}
   enter(Stage::AwaitReturn);
  }break;
 case Stage::AwaitReturn:s.startContentFlow("f4-meeting-return");break;
 }
}
