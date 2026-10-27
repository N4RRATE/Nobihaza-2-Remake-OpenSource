#include "save_controller.h"
#include <utility>
SaveController::SaveController(const CombatContent& c,GameSession& session,CombatScene& scene,SaveFiles& files,const SaveRequest& request,const SavedPresentation& p)
    :content_(c),session_(session),scene_(scene),files_(files),sourcePresentation_(p),request_(request),generation_(scene.generation()){prepare();}
void SaveController::prepare(){
    try{candidate_=captureSave(content_,scene_,request_,sourcePresentation_);}catch(const std::exception& error){preparationFailed_=true;preparationError_=error.what();return;}
    preparationFailed_=false;preparationError_.clear();
    const auto& c=content_;const auto& request=request_;
    if(request.kind=="clear_after_file"||request.kind=="chapter_handoff_after_file"||request.kind=="direct_unscored"){filesUi_=std::make_unique<FileMenu>(files_,candidate_);return;}
    const auto& flow=c.catalog.at("save_flow");auto message=c.messages.at(flow.at("message").get<std::string>()).message;message.faceImage.clear();dialogue_.request(message);
    session_.setFlag(1,false);session_.setFlag(18,false);presentation_.push_back({PresentationEvent::Kind::SystemGraphic,flow.at("question_system"),{},{},0});
}
std::optional<FileMenuView> SaveController::fileView() const{
    if(preparationFailed_){FileMenuView view;view.title=preparationError_.empty()?"保存尚未完成":"无法准备保存";view.prompt=preparationError_.empty()?std::string{}:preparationError_+" / ";view.prompt+="确认：重试保存";if(!preparationError_.empty()){view.prompt+=" / 取消：关闭提示";}return view;}
    return filesUi_?std::optional<FileMenuView>{filesUi_->view()}:std::nullopt;
}
std::vector<PresentationEvent> SaveController::takePresentation(){return std::exchange(presentation_,{});}
void SaveController::sound(const std::string& name){
    if(request_.kind=="clear_after_file"&&name=="decision"){presentation_.push_back({PresentationEvent::Kind::Sound,"",readSound(content_.catalog.at("clear").at("slot_sound")),{},0});return;}
    const auto& sounds=content_.catalog.at("save_flow").at(filesUi_?"slot_sounds":"restore_sounds");
    const auto definition=sounds.contains(name)?sounds.at(name):content_.media.at("ui").at("menu").at("sounds").at(name);
    presentation_.push_back({PresentationEvent::Kind::Sound,"",readSound(definition),{},0});
}
void SaveController::finish(bool committed){
    if(request_.kind=="clear_after_file"){
        if(!scene_.clearSavePending(request_)){finished_=true;return;}if(committed){finishSaveContinuation(session_,candidate_);}scene_.finishClearSave(request_);finished_=true;return;
    }
    if(request_.kind=="direct_unscored"){
        if(!scene_.directSavePending(request_)){finished_=true;return;}if(committed){finishSaveContinuation(session_,candidate_);}scene_.finishDirectSave(request_);finished_=true;return;
    }
    if(request_.kind=="chapter_handoff_after_file"){
        if(!scene_.chapterSavePending(request_)){finished_=true;return;}
        if(committed){finishSaveContinuation(session_,candidate_);}scene_.finishChapterSave(request_);finished_=true;return;
    }
    if(committed){finishSaveContinuation(session_,candidate_);}else{session_.setFlag(1,true);session_.setFlag(18,true);}
    const auto& flow=content_.catalog.at("save_flow");const auto& v=flow.at("restore_options");scene_.restoreMessageDisplay(generation_,{v.at("transparent"),v.at("position"),v.at("fixed"),v.at("continue_events")});
    if(filesUi_){presentation_.push_back({PresentationEvent::Kind::StopSounds,"",{},{},0});}presentation_.push_back({PresentationEvent::Kind::SystemGraphic,flow.at("restore_system"),{},{},0});
    if(committed){presentation_.push_back({PresentationEvent::Kind::Notice,"保存成功",{},{},120});}finished_=true;dialogue_.invalidate();prompt_.invalidate(generation_);combined_={};
}
void SaveController::update(const InputFrame& input){
    if(finished_){return;}if((request_.kind=="clear_after_file"&&!scene_.clearSavePending(request_))||!scene_.active()||scene_.generation()!=generation_||(request_.kind=="chapter_handoff_after_file"&&!scene_.chapterSavePending(request_))||(request_.kind=="direct_unscored"&&!scene_.directSavePending(request_))){finished_=true;dialogue_.invalidate();prompt_.invalidate(generation_);return;}
    if(preparationFailed_){
        if(input.buttons.at(static_cast<int>(InputAction::Confirm)).pressed){prepare();}
        else if(input.buttons.at(static_cast<int>(InputAction::Cancel)).pressed){if(request_.kind=="ordinary"){finished_=true;}else{preparationError_.clear();}}
        return;
    }
    if(filesUi_){filesUi_->update(input);if(auto s=filesUi_->takeSound();!s.empty()){sound(s);}if(auto result=filesUi_->takeResult()){finish(result->kind==FileMenuResult::Kind::Saved);}return;}
    if(!askingChoice_){dialogue_.update(false,false);if(dialogue_.view().phase!=DialoguePhase::AwaitingInput){return;}
        questionLines_=dialogue_.view().lines;questionColors_=dialogue_.view().colors;dialogue_.invalidate();const auto& cue=content_.cue(content_.catalog.at("save_flow").at("choice").get<std::string>());ChoicePrompt choice;choice.cancelType=cue.at("cancel_type");choice.display=content_.messages.at(content_.catalog.at("save_flow").at("message").get<std::string>()).message.display;
        for(const auto& text:cue.at("choices")){const auto raw=text.get<std::string>();choice.options.push_back({raw,{{MessageTokenKind::Text,decodeMessageText(raw),0}}});}promptId_=prompt_.request(choice,generation_);askingChoice_=true;
    }else{
        const auto feedback=prompt_.update(input);if(feedback!=PromptFeedback::None){sound(feedback==PromptFeedback::Cursor?"cursor":feedback==PromptFeedback::Cancel?"cancel":"decision");}
        if(auto result=prompt_.takeCompletion(promptId_)){
            const auto* answer=std::get_if<ChoiceSelected>(&result->result);if(!answer||answer->index!=0){finish(false);return;}
            filesUi_=std::make_unique<FileMenu>(files_,candidate_);combined_={};presentation_.push_back({PresentationEvent::Kind::SystemGraphic,content_.catalog.at("save_flow").at("slot_system"),{},{},0});return;
        }
    }
    combined_=prompt_.view();combined_.lines.insert(combined_.lines.begin(),questionLines_.begin(),questionLines_.end());combined_.colors.insert(combined_.colors.begin(),questionColors_.begin(),questionColors_.end());if(combined_.cursor){combined_.cursor->line+=static_cast<int>(questionLines_.size());}
}
