#include "combat_scene.h"
#include <algorithm>
void CombatScene::openRegistrationKeyboard(){
    const auto& p=content_.catalog.at("registration");registrationSlots_.fill(0);registrationSlot_=p.at("initial_slot");registrationKey_=p.at("initial_key");registrationOpen_=true;
    startForeground(ForegroundKind::Registration,p.at("object"));presentRegistrationKeyboard(true);wait_=30;
}
void CombatScene::presentRegistrationKeyboard(bool opening){
    const auto& p=content_.catalog.at("registration");
    if(opening){for(const auto& id:p.at("open_cues")){cue(id);}}
    const auto picture=[&](const std::string& id,int x,int y,int transparency){PresentationEvent ev{PresentationEvent::Kind::Cue,id,{},{},0};ev.resolvedPicture=ResolvedPicture{{double(x),double(y)},transparency,0};presentation_.push_back(std::move(ev));};
    const auto literalPicture=[&](const std::string& id,int transparency){const auto& xy=content_.cue(id).at("position").at("xy");picture(id,xy.at(0),xy.at(1),transparency);};
    literalPicture(p.at("user_label"),registrationSlot_<8?0:50);literalPicture(p.at("password_label"),registrationSlot_<8?50:0);
    for(int i=0;i<16;++i){const auto& s=p.at("slots").at(i);const int value=registrationSlots_[i];const auto& segments=s.at("segments");const auto segment=std::find_if(segments.rbegin(),segments.rend(),[&](const auto& band){return value>=band.at("first_code").template get<int>();});
        picture(segment->at("cue"),s.at("x"),s.at("base_y").get<int>()-(s.at("full_height").get<int>()-segment->at("height").get<int>())/2-(value-segment->at("first_code").get<int>())*240,0);
    }
    const auto& xy=p.at("cursor_positions").at(std::to_string(registrationKey_));picture(p.at("small_cursor"),xy.at(0),xy.at(1),registrationKey_==209?100:40);literalPicture(p.at("enter_cursor"),registrationKey_==209?40:100);
}
void CombatScene::clearRegistrationKeyboard(){
    if(!content_.catalog.contains("registration")||(!registrationOpen_&&!session_.flag(content_.catalog.at("registration").at("session_fact")))){return;}for(const auto& id:content_.catalog.at("registration").at("erase_cues")){cue(id);}registrationOpen_=false;registrationSlots_.fill(0);registrationKey_=registrationSlot_=0;
}
void CombatScene::finishRegistrationKeyboard(bool submit){
    const auto& p=content_.catalog.at("registration");std::string flow="registration-cancel";
    if(submit){
        if(!session_.flag(447)){flow="registration-no-server";}
        else if(registrationSlots_!=p.at("expected").get<std::array<int,16>>()){flow="registration-wrong";}
        else{flow=session_.count(101)>0?"registration-already":"registration-card";}
    }
    registrationOpen_=false;registrationSlots_.fill(0);registrationKey_=registrationSlot_=0;startContentFlow(flow);
}
void CombatScene::updateRegistrationKeyboard(const InputFrame& input){
    if(wait_>0){return;}const auto& p=content_.catalog.at("registration");const auto pressed=[&](InputAction a){return input.buttons.at(static_cast<std::size_t>(a)).pressed;};
    // Original CheckInput takes one result; physical Cancel is consumed without a P1 action.
    if(pressed(InputAction::Cancel)){wait_=1;return;}
    if(pressed(InputAction::Confirm)){
        if(registrationKey_==209){cue(p.at("sounds").at("enter"));if(registrationSlot_<8){registrationSlot_=8;}else{session_.setVariable(45,-2);}}
        else{
            const int value=p.at("codes").at(std::to_string(registrationKey_));session_.setVariable(45,value);
            if(value==-1){cue(p.at("sounds").at("cancel"));if(registrationSlot_>=8){session_.setVariable(45,0);registrationSlot_=0;while(registrationSlot_<7&&registrationSlots_[registrationSlot_]!=0){++registrationSlot_;}}}
            else{
                cue(p.at("sounds").at("character"));const int first=registrationSlot_<8?0:8,last=first+7;
                if(value==0&&registrationSlot_>first){if(registrationSlot_!=last||registrationSlots_[last]==0){--registrationSlot_;}}
                registrationSlots_[registrationSlot_]=value;
                if(value>0&&registrationSlot_<last){++registrationSlot_;}
            }
        }
        if(session_.variable(45)==-1||session_.variable(45)==-2){finishRegistrationKeyboard(session_.variable(45)==-2);return;}
        presentRegistrationKeyboard();wait_=1;return;
    }
    for(const auto& [a,d]:std::initializer_list<std::pair<InputAction,const char*>>{{InputAction::Up,"up"},{InputAction::Right,"right"},{InputAction::Left,"left"},{InputAction::Down,"down"}}){if(pressed(a)){registrationKey_=p.at("neighbors").at(std::to_string(registrationKey_)).at(d);presentRegistrationKeyboard();wait_=1;return;}}
}
