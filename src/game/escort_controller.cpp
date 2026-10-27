#include "escort_controller.h"
#include <algorithm>
#include <stdexcept>
namespace{
int sourceFacing(Direction facing){return facing==Direction::Down?2:facing==Direction::Left?4:facing==Direction::Right?6:8;}
}
EscortController::EscortController(CombatScene& scene,const nlohmann::json& profile):scene_(scene),profile_(profile){}
int EscortController::value(const std::string& field) const{
    if(field=="escort_skin_enabled"){return enabled_;}if(field=="escort_skin_frame"){return frame_;}
    if(field=="escort_last_facing"){return lastFacing_;}if(field=="escort_walk_phase"){return walkPhase_;}
    throw std::runtime_error("Unsupported escort field: "+field);
}
void EscortController::assign(const std::string& field,int value){
    if(field=="escort_skin_enabled"){enabled_=value;}else if(field=="escort_skin_frame"){frame_=value;}
    else if(field=="escort_last_facing"){lastFacing_=value;}else if(field=="escort_walk_phase"){walkPhase_=value;}
    else{throw std::runtime_error("Unsupported escort field: "+field);}
}
void EscortController::picture(const std::string& id,GridPosition position){
    const auto& cue=scene_.content_.cue(id);PresentationEvent event{PresentationEvent::Kind::Cue,id,{},{},0};
    event.resolvedPicture=ResolvedPicture{position,cue.at("transparency_percent"),0};scene_.presentation_.push_back(std::move(event));
}
void EscortController::update(){
    const auto& actor=findCharacter(*scene_.world_,profile_.at("object"));
    const auto& costume=scene_.content_.objects.at(profile_.at("skin_object").get<std::string>());
    // V91 only selects the costume page; it is not a durable combat mode.
    int selected=0;for(const auto& variant:costume.variants){bool matches=true;
        for(const auto& [id,on]:variant.flags){matches&=scene_.session_.flag(id)==on;}
        if(variant.variableMinimum){matches&=enabled_>=variant.variableMinimum->second;}
        if(matches){selected=variant.page;}
    }
    if(!selected){page_=0;return;}
    const auto& row=profile_.at("details").at("pages").at(std::to_string(selected));
    const auto liveFacing=directionName(actor.motion.facing());const bool changed=selected!=page_;
    auto position=scene_.sampleActorScreen(profile_.at("object"));
    if(changed||(selected==1&&lastFacing_!=sourceFacing(actor.motion.facing()))){
        page_=selected;facing_=liveFacing;lastFacing_=sourceFacing(actor.motion.facing());
        GridPosition show=position;
        if(selected==1){const auto& xy=row.at("walking").at("offsets").at(facing_);show.x+=xy[0].get<int>();show.y+=xy[1].get<int>();}
        else{show.y-=21;frame_=0;}
        for(const auto& id:row.at("shows").at(facing_)){picture(id,show);}
    }
    if(selected==1){
        const auto& rule=row.at("walking");const auto tile=actor.motion.tile();const int key=tile.x*rule.at("movement_key_multiplier").get<int>()+tile.y;
        if(key!=movementKey_){movementKey_=key;walkPhase_+=rule.at("activity_add").get<int>();}
        for(const auto& id:rule.at("hide_cues")){picture(id,position);}
        frame_+=rule.at("frame_add").get<int>();int channel=1;
        if(walkPhase_>0){for(const auto& threshold:rule.at("thresholds")){if(frame_>=threshold.at(0).get<int>()){channel=threshold.at(1);break;}}
            if(frame_>=rule.at("reset_at_least").get<int>()){frame_=0;}walkPhase_-=rule.at("activity_subtract").get<int>();
        }else{walkPhase_=0;}
        const auto& xy=rule.at("offsets").at(facing_);position.x+=xy[0].get<int>();position.y+=xy[1].get<int>();
        picture(rule.at("visible_cues").at(std::to_string(channel)),position);
    }else{
        const auto id=std::to_string(frame_);if(!row.at("frames").contains(id)){return;}
        const auto& pose=row.at("frames").at(id);const auto& xy=pose.at("offsets").at(facing_);position.x+=xy[0].get<int>();position.y+=xy[1].get<int>();
        for(const auto& cue:pose.at("cues")){picture(cue,position);}
    }
}
