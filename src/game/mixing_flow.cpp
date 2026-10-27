#include "combat_scene.h"
#include "weapon_controller.h"
#include <utility>

std::optional<PromptIdentity> CombatScene::requestMixing(){
    mixRequest_.reset();const auto& m=content_.expansion.mixing;
    if(m.is_null()||!active_||session_.hp()<=0||foreground_!=ForegroundKind::None||grabOwner_||!session_.knowsSkill(m.at("enabled_skill"))||session_.knowsSkill(m.at("disabled_skill"))){return {};}
    if(world_->player().motion.moving()||weapon_->raised()||messageRequest_||inputRequest_||routesPending()){return {};}
    const auto& skill=content_.skills.at(m.at("enabled_skill").get<int>());
    if(!skill.fieldUse||skill.type!=3||skill.tpCost!=0){return {};}
    if(!mixAtMenu_&&(!session_.flag(1)||!session_.flag(18))){return {};}
    mixRequest_=PromptIdentity{generation_,++nextMixRequest_};return mixRequest_;
}
bool CombatScene::startMixing(PromptIdentity request){
    if(!mixRequest_||request!=*mixRequest_||request.scene!=generation_){return false;}
    const auto valid=requestMixing();mixRequest_.reset();if(!valid){return false;}
    mixAtMenu_=false;mixChoice_=false;flowQuestion_.reset();mixSegment_="begin";
    session_.setFlag(content_.expansion.mixing.at("request_flag"),true);
    weapon_->cancel(ActionCancel::Foreground);startForeground(ForegroundKind::Mixing);updateMixing();return true;
}
bool CombatScene::runMixSegment(const std::string& segment){
    const auto& steps=content_.expansion.mixing.at(segment);
    while(phase_<static_cast<int>(steps.size())){
        const auto& step=steps.at(phase_++);const auto k=step.at("kind").get<std::string>();
        if(k=="flag"){session_.setFlag(step.at("id"),step.at("value"));}
        else if(k=="cue"){cue(step.at("cue"),"player");}
        else if(k=="message"){requestMessage(step.at("message"));}
        else{throw std::runtime_error("Unsupported mixing presentation step: "+k);}
        if(messageRequest_||wait_>0){return false;}
    }
    phase_=0;return true;
}
void CombatScene::updateMixing(){
    const auto& m=content_.expansion.mixing;
    if(mixSegment_=="begin"){
        if(!runMixSegment("begin")){return;}
        mixChoice_=session_.canMix(*content_.expansion.mixingRecipe);mixSegment_=mixChoice_?"question":"missing";
    }
    if(mixSegment_=="question"){
        if(!runMixSegment("question")){return;}mixSegment_="choice";
    }
    if(mixSegment_=="choice"){
        if(!inputRequest_){
            const auto& source=content_.cue(m.at("choice").get<std::string>());ChoicePrompt prompt;
            prompt.cancelType=source.at("cancel_type");prompt.display=messageState_.display;prompt.question=std::exchange(flowQuestion_,{});
            for(const auto& raw:source.at("choices")){const auto text=raw.get<std::string>();prompt.options.push_back({text,{{MessageTokenKind::Text,decodeMessageText(text),0}}});}
            requestInput(prompt);return;
        }
        const auto result=takeInputResult(*inputRequest_);if(!result){return;}
        mixChoice_=false;const auto* selected=std::get_if<ChoiceSelected>(&result->result);
        if(!selected){throw std::runtime_error("Mixing choice result mismatch");}
        mixSegment_=selected->index==0?(session_.mix(*content_.expansion.mixingRecipe)?"success":"missing"):"before_menu";
    }
    if(mixSegment_=="success"||mixSegment_=="missing"){
        if(!runMixSegment(mixSegment_)){return;}mixSegment_="before_menu";
    }
    if(mixSegment_=="before_menu"){
        if(!runMixSegment("before_menu")){return;}
        finishForeground();mixAtMenu_=true;requestedMode_=ApplicationMode::Equipment;
    }
}
void CombatScene::finishMixingMenu(){
    mixRequest_.reset();if(!mixAtMenu_){return;}mixAtMenu_=false;
    if(!runMixSegment("finish")){throw std::runtime_error("Mixing menu cleanup cannot suspend");}
}
