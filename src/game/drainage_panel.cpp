#include "combat_scene.h"
#include "weapon_controller.h"
#include <algorithm>
const nlohmann::json& CombatScene::drainageProfile() const{
    return activeDrainageProfile_=="drainage"?content_.catalog.at("drainage"):content_.catalog.at("nine_cell_panels").at(activeDrainageProfile_);
}
void CombatScene::openDrainage(const std::string& profile){
    activeDrainageProfile_=profile;const auto& p=drainageProfile();drainageCells_.fill(false);drainagePendingCheck_=false;
    for(const auto& cell:p.at("initial_lit_cells")){drainageCells_.at(cell.get<int>()-1)=true;}
    drainageCursor_=p.at("initial_cursor");startForeground(ForegroundKind::Drainage,p.at("object"));presentDrainage(true);
    // Opening consumes the message-closing input and yields before the first puzzle input.
    wait_=1;
}
void CombatScene::presentDrainage(bool opening,bool cells){
    const auto& p=drainageProfile();const auto& pictures=p.at("pictures");
    if(opening){cue(pictures.at("board"));}
    const auto picture=[&](const std::string& id,const nlohmann::json& xy,int transparency,int duration){
        PresentationEvent e{PresentationEvent::Kind::Cue,id,{},{},0};e.resolvedPicture=ResolvedPicture{{xy.at(0),xy.at(1)},transparency,duration};presentation_.push_back(std::move(e));
    };
    if(cells){for(int cell=1;cell<=9;++cell){const auto key=std::to_string(cell);picture(pictures.at("cells").at(key),pictures.at("cell_centers").at(key),pictures.at(drainageCells_[cell-1]?"lit_transparency":"unlit_transparency"),opening?0:p.at("input_wait_tenths").get<int>());}}
    if(opening||!cells){picture(pictures.at("cursor"),pictures.at("cursor_positions").at(std::to_string(drainageCursor_)),0,0);}
}
void CombatScene::updateDrainage(const InputFrame& input){
    if(wait_>0){return;}const auto& p=drainageProfile();
    if(drainagePendingCheck_){
        drainagePendingCheck_=false;
        if(std::all_of(drainageCells_.begin(),drainageCells_.end(),[](bool lit){return lit;})){startContentFlow(p.value("success_flow",std::string("drainage-success")));return;}
        return;
    }
    const auto pressed=[&](InputAction a){return input.buttons.at(static_cast<std::size_t>(a)).pressed;};
    if(pressed(InputAction::Cancel)){startContentFlow(p.value("cancel_flow",std::string("drainage-cancel")));return;}
    if(pressed(InputAction::Confirm)){
        cue(p.at("confirm_sound"));for(const auto& cell:p.at("toggle_cells").at(std::to_string(drainageCursor_))){auto& lit=drainageCells_.at(cell.get<int>()-1);lit=!lit;}
        presentDrainage(false);drainagePendingCheck_=true;wait_=p.at("input_wait_tenths").get<int>()*6;
    }else{
        std::optional<Direction> direction;
        for(const auto& [action,d]:std::initializer_list<std::pair<InputAction,Direction>>{{InputAction::Up,Direction::Up},{InputAction::Right,Direction::Right},{InputAction::Left,Direction::Left},{InputAction::Down,Direction::Down}}){if(pressed(action)){direction=d;break;}}
        if(!direction){return;}drainageCursor_=p.at("cursor_neighbors").at(std::to_string(drainageCursor_)).at(directionName(*direction));
        if(p.contains("direction_sound")){cue(p.at("direction_sound"));}presentDrainage(false,false);
    }
}
void CombatScene::clearDrainage(){
    if(activeDrainageProfile_.empty()){return;}const auto& p=drainageProfile();
    // Retiring a scene clears its transient panel and never commits a delayed success.
    for(const auto& id:p.at("cancel_cues")){if(content_.cue(id.get<std::string>()).at("kind")=="erase_picture"){cue(id);}}
    for(const auto& id:p.at("lifecycle").at("restore_facts_on_exit")){session_.setFlag(id,true);}
    activeDrainageProfile_.clear();drainagePendingCheck_=false;drainageCursor_=0;drainageCells_.fill(false);
}
