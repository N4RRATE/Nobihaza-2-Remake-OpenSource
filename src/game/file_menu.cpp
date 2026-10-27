#include "file_menu.h"
#include <algorithm>
#include <utility>
namespace{bool pressed(const InputFrame& f,InputAction a){return f.buttons[static_cast<int>(a)].pressed;}}
FileMenu::FileMenu(SaveFiles& files,std::optional<SaveSnapshot> save):files_(files),save_(std::move(save)){refresh();}
void FileMenu::refresh(){slots_=files_.list();}
std::optional<FileMenuResult> FileMenu::takeResult(){return std::exchange(result_,{});}
std::string FileMenu::takeSound(){return std::exchange(sound_,{});}
void FileMenu::write(){
    const auto result=files_.write(selected_+1,*save_);
    if(result.outcome==WriteOutcome::Committed){phase_=Phase::Finished;result_=FileMenuResult{FileMenuResult::Kind::Saved,{}};sound_="decision";}
    else{message_="保存失败："+result.message;phase_=Phase::Error;sound_="buzzer";refresh();}
}
void FileMenu::select(){
    const auto& slot=slots_.at(selected_);
    if(slot.state==SlotState::Corrupt||slot.state==SlotState::Incompatible||slot.state==SlotState::RecoveryRequired){
        try{recovery_=files_.recoveryChoices(selected_+1);}catch(const std::exception& e){message_=e.what();phase_=Phase::Error;return;}
        if(recovery_.empty()){message_="此槽位没有可读取或恢复的版本。原文件已保留。";phase_=Phase::Error;sound_="buzzer";}
        else{phase_=Phase::Recovery;recoverySelection_=0;sound_="decision";}return;
    }
    if(save_){if(slot.state==SlotState::Readable){phase_=Phase::Overwrite;answer_=1;sound_="decision";}else{write();}return;}
    if(slot.state==SlotState::Empty){message_="此槽位没有存档。";phase_=Phase::Error;sound_="buzzer";return;}
    try{auto snapshot=files_.read(selected_+1);result_=FileMenuResult{FileMenuResult::Kind::Loaded,std::move(snapshot)};phase_=Phase::Finished;sound_="decision";}
    catch(const std::exception& e){message_=e.what();phase_=Phase::Error;sound_="buzzer";refresh();}
}
void FileMenu::update(const InputFrame& input){
    if(phase_==Phase::Finished){return;}
    if(phase_==Phase::Error){if(pressed(input,InputAction::Confirm)||pressed(input,InputAction::Cancel)){phase_=Phase::Browse;sound_="cancel";}return;}
    if(phase_==Phase::Overwrite||phase_==Phase::ConfirmRecovery){
        if(pressed(input,InputAction::Up)||pressed(input,InputAction::Down)||pressed(input,InputAction::Left)||pressed(input,InputAction::Right)){answer_=1-answer_;sound_="cursor";}
        if(pressed(input,InputAction::Cancel)){phase_=Phase::Browse;sound_="cancel";return;}
        if(pressed(input,InputAction::Confirm)){
            if(answer_){phase_=Phase::Browse;sound_="cancel";return;}
            if(phase_==Phase::Overwrite){write();}
            else{const auto& selected=recovery_.at(recoverySelection_);const bool completesCurrent=save_&&selected.snapshot==*save_;const auto r=files_.recover(selected_+1,selected);refresh();message_=r.message;sound_=r.outcome==WriteOutcome::Committed?"decision":"buzzer";
                if(completesCurrent&&r.outcome==WriteOutcome::Committed){phase_=Phase::Finished;result_=FileMenuResult{FileMenuResult::Kind::Saved,{}};}else{phase_=Phase::Error;}}

        }return;
    }
    if(pressed(input,InputAction::Cancel)){
        sound_="cancel";if(phase_==Phase::Recovery){phase_=Phase::Browse;}else{phase_=Phase::Finished;result_=FileMenuResult{FileMenuResult::Kind::Cancelled,{}};}return;
    }
    int& selection=phase_==Phase::Recovery?recoverySelection_:selected_;const int count=phase_==Phase::Recovery?static_cast<int>(recovery_.size()):15;
    if(pressed(input,InputAction::Up)||pressed(input,InputAction::Down)){selection=(selection+count+(pressed(input,InputAction::Up)?-1:1))%count;sound_="cursor";}
    if(pressed(input,InputAction::Confirm)){if(phase_==Phase::Recovery){phase_=Phase::ConfirmRecovery;answer_=1;}else{select();}}
}
FileMenuView FileMenu::view() const{
    FileMenuView view;view.title=save_?"记录调查状况":"读取调查记录";view.selection=selected_;view.first=std::clamp(selected_-2,0,10);
    for(const auto& slot:slots_){std::string line="记录 "+std::to_string(slot.slot)+"  ";
        if(slot.snapshot){const auto& s=*slot.snapshot;line+=s.summary.label+" LV"+std::to_string(s.continuation.kind=="clear_after_file"?1:s.session.level)+" HP "+std::to_string(s.continuation.kind=="clear_after_file"?1:s.session.hp);}
        else{switch(slot.state){case SlotState::Empty:line+="空";break;case SlotState::Corrupt:line+="损坏（原件保留）";break;case SlotState::Incompatible:line+="版本不兼容";break;case SlotState::RecoveryRequired:line+="写入中断，需要恢复";break;default:break;}}
        view.lines.push_back(std::move(line));view.summaries.push_back(slot.snapshot?std::optional<SlotSummary>{slot.snapshot->summary}:std::nullopt);
    }
    if(phase_==Phase::Recovery||phase_==Phase::ConfirmRecovery){view.title="选择要恢复的完整版本";view.lines.clear();view.summaries.clear();view.first=0;view.selection=recoverySelection_;for(const auto& r:recovery_){view.lines.push_back(r.source+"  HP "+std::to_string(r.snapshot.session.hp)+"  保存 "+std::to_string(r.snapshot.session.saveCount));}}
    view.confirmation=phase_==Phase::Overwrite||phase_==Phase::ConfirmRecovery;view.answer=answer_;
    if(view.confirmation){view.prompt=phase_==Phase::Overwrite?"覆盖此记录？  是 / 否":"恢复所选版本？其他版本会归档保留。 是 / 否";}
    else if(phase_==Phase::Error){view.prompt=message_;}
    else{view.prompt="W/S 选择 · Z 确认 · X 返回";}return view;
}
