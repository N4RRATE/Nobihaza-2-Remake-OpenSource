#include "dekisugi_controller.h"
#include "combat_scene.h"
#include "full_content.h"
#include "scene_presentation.h"
#include "weapon_controller.h"
#include "enemy_host.h"
#include <algorithm>
#include <stdexcept>
#include <cmath>
namespace{bool overlaps(GridPosition a,GridPosition b,std::array<int,2> ah,std::array<int,2> bh){return std::abs(a.x-b.x)<=ah[0]+bh[0]&&std::abs(a.y-b.y)<=ah[1]+bh[1];}}
DekisugiController::DekisugiController(CombatScene& scene,const nlohmann::json& profile):scene_(scene),profile_(profile),generation_(scene.generation_){
 if(const auto* patch=difficultyControllerRules(scene.content_,scene.session_,"dekisugi")){
  auto selected=*patch;const auto defenses=selected.at("part_defense");selected.erase("part_defense");profile_.merge_patch(selected);
  for(auto& part:profile_.at("parts")){part["defense"]=defenses.at(part.at("object").get<std::string>());}
 }
}
bool DekisugiController::owns(const std::string& id) const{return std::find(profile_.at("owned_objects").begin(),profile_.at("owned_objects").end(),id)!=profile_.at("owned_objects").end();}
void DekisugiController::call(Work& work,int page){if(work.stack.size()>=16){throw std::logic_error("Dekisugi call depth exceeded");}work.stack.push_back({page,0});}
int DekisugiController::calledPage(int event,int page) const{
    if(event==5&&page>=2&&page<=12){return event*100+page;}
    if(event==9&&page>=1&&page<=5){return event*100+page;}
    throw std::logic_error("Unbound Dekisugi dynamic page");
}
bool DekisugiController::run(Work& work){
    if(work.wait>0){--work.wait;if(work.wait>0){return false;}}
    if(scene_.messageRequest_){return false;}
    for(int commands=0;commands<8192&&!work.stack.empty();++commands){
        const auto result=advance(work,work.stack.back());
        if(result==Step::Done){work.stack.pop_back();}
        else if(result==Step::Yield){return false;}
    }
    if(!work.stack.empty()){throw std::logic_error("Dekisugi source loop did not yield");}
    return true;
}
bool DekisugiController::play(const std::string& id,const std::string& self,Work& work){
    const auto& bindings=profile_.at("cue_bindings");
    const auto emit=[&](const std::string& cue){
        if(bindings.contains(cue)){
            const auto& b=bindings.at(cue);const auto& xy=b.at("xy");GridPosition p;
            if(b.at("variables").get<bool>()){p={double(scene_.session_.variable(xy.at(0))),double(scene_.session_.variable(xy.at(1)))};}
            else{p={xy.at(0).get<double>(),xy.at(1).get<double>()};}
            p.y+=b.value("offset_y",0);scene_.pictureAnchors_[cue]=p;
        }
        const int old=scene_.wait_;scene_.wait_=0;const bool ready=ScenePresentation(scene_).executeCue(cue,self);work.wait=std::max(work.wait,scene_.wait_);scene_.wait_=old;return ready;
    };
    if(!emit(id)){return false;}
    if(profile_.at("cue_segments").contains(id)){for(const auto& cue:profile_.at("cue_segments").at(id)){emit(cue.get<std::string>());}}
    return true;
}
void DekisugiController::common6(){if(!scene_.session_.flag(574)){scene_.playSound("ce6-c3");}}
void DekisugiController::common9(){scene_.cue("f4bc-ce9-c18");scene_.startRoute("f4bc-ce9-c19");}
void DekisugiController::common12(){EnemyHost(scene_,profile_.at("eye_object")).receiveSourceDamage(scene_.session_.variable(23));}
void DekisugiController::common141(){const auto& c=profile_.at("checkpoint");scene_.session_.captureContinueCheckpoint(scene_.content_.magazineSeedBindings,c.at("map"),{c.at("tile").at(0),c.at("tile").at(1)});}
std::optional<std::string> DekisugiController::update(){
    if(finished_||!scene_.active_||scene_.generation_!=generation_||scene_.session_.hp()<=0||scene_.foreground_==ForegroundKind::Death){return {};}
    auto& s=scene_.session_;
    if(!started_){if(scene_.foreground_!=ForegroundKind::None){return {};}started_=true;scene_.fileReadingLocked_=true;scene_.weapon_->cancel(ActionCancel::Foreground);scene_.startForeground(ForegroundKind::Boss);call(main_,301);}
    if(!introDone_){if(run(main_)){introDone_=true;scene_.finishForeground();}return {};}
    if(!winning_&&s.variable(103)>=profile_.at("body_threshold").get<int>()){
        winning_=true;main_={};body_={};scene_.weapon_->cancel(ActionCancel::Foreground);scene_.routes_.clear();scene_.startForeground(ForegroundKind::Boss);call(main_,307);
    }
    if(winning_){if(run(main_)&&winTransfer_){finished_=true;scene_.finishForeground();return profile_.at("win_flow").get<std::string>();}return {};}
    // CE1 player sampling precedes source ordered map events, including all nested calls.
    const auto player=scene_.sampleActorScreen("player");s.setVariable(3,int(player.x));s.setVariable(4,int(player.y));
    if(main_.stack.empty()){call(main_,306);}if(run(main_)){call(main_,306);run(main_);}
    for(int slot=0;slot<2;++slot){const auto& part=profile_.at("parts").at(slot);EnemyHost host(scene_,part.at("object"));const GridPosition center{double(s.variable(part.at("x_variable"))),double(s.variable(part.at("y_variable")))};
        const int amount=host.advanceBurn(center);const int damage=part.at("damage_variable");s.setVariable(damage,s.variable(damage)+amount);
        host.advanceBlood(part.at("blood"),s.variable(part.at("feedback_variable")));
    }
    return {};
}
AttackResolution DekisugiController::receive(const AttackSample& sample,std::set<std::string>& resolved){
    AttackResolution result;if(!introDone_||winning_||finished_||scene_.generation_!=generation_||!scene_.active_||scene_.session_.hp()<=0||scene_.foreground_==ForegroundKind::Death){return result;}
    auto& s=scene_.session_;const auto position=GridPosition{sample.center.x-(scene_.camera_.center().x-160),sample.center.y-(scene_.camera_.center().y-120)};
    for(const auto& part:profile_.at("parts")){
        const std::string id=part.at("object");const int field=part.at("damage_variable");if(resolved.contains(id)||s.variable(part.at("registration_variable"))==0||s.variable(field)>=part.at("threshold").get<int>()){continue;}
        const GridPosition center{double(s.variable(part.at("x_variable"))),double(s.variable(part.at("y_variable")))};
        if(!overlaps(position,center,sample.halfPixels,{8,8})){continue;}++result.contacts;
        if(sample.phase==AttackPhase::Flight){continue;}
        if(sample.phase==AttackPhase::Burning){EnemyHost(scene_,id).startBurn();resolved.insert(id);++result.processed;continue;}
        if(sample.receiverPage<2||sample.receiverPage>6){continue;}
        if(sample.kind==AttackKind::GlFlame&&EnemyHost(scene_,id).burning()){continue;}
        const bool eye=part.at("eye");const bool open=eye||s.variable(profile_.at("open_variable"))==1;
        int amount=0;bool end=false;
        if(open){
            const auto roll=EnemyHost(scene_,id).receivePartDamage(sample,part.at("defense"));amount=roll.damage;end=roll.endRequested;
            s.setVariable(field,s.variable(field)+amount);
            const auto& media=part.at("receiver_cues").at(std::to_string(sample.receiverPage));for(const auto& cue:media){scene_.cue(cue,id);}
            if(eye&&sample.receiverPage>=3&&sample.receiverPage<=5){s.setFlag(16,true);end=true;}
        }
        const int feedback=eye?112:sample.receiverPage==2?1:sample.receiverPage==6?3:2;
        s.setVariable(part.at("feedback_variable"),open?feedback:feedback+120);s.setFlag(part.at("hit_fact"),false);s.setFlag(part.at("hit_fact"),true);
        if(sample.kind==AttackKind::GlFlame){EnemyHost(scene_,id).startBurn();}
        resolved.insert(id);++result.processed;result.damage+=amount;result.endRequested|=end;
    }
    return result;
}
#include "dekisugi_program.inc"
