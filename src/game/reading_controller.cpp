#include "reading_controller.h"
#include <utility>
ReadingController::ReadingController(const CombatContent& content,const GameSession& session,std::string definition,std::uint64_t scene)
    :content_(content),session_(session),definition_(std::move(definition)),scene_(scene){(void)content_.catalog.at("readings").at(definition_);}
std::vector<PresentationEvent> ReadingController::takePresentation(){return std::exchange(presentation_,{});}
void ReadingController::close(){if(phase_!=Phase::Menu){throw std::logic_error("Reading close before menu return");}phase_=Phase::Close;next_=0;}
void ReadingController::nextPhase(){
    switch(phase_){case Phase::Open:phase_=Phase::Body;break;case Phase::Body:phase_=Phase::Return;break;case Phase::Return:phase_=Phase::Menu;break;case Phase::Close:phase_=Phase::Finished;break;default:break;}next_=0;
}
void ReadingController::update(const InputFrame& input,std::uint64_t scene){
    if(scene!=scene_){cancelled_=true;dialogue_.invalidate();presentation_.clear();phase_=Phase::Finished;return;}
    if(atMenu()||finished()){return;}
    dialogue_.update(input.buttons[static_cast<int>(InputAction::Confirm)].pressed,input.buttons[static_cast<int>(InputAction::Cancel)].pressed);
    if(dialogue_.active()){return;}dialogue_.takeCompletion();
    if(wait_>0&&--wait_>0){return;}
    for(;;){
        if(atMenu()||finished()){return;}
        const char* stage=phase_==Phase::Open?"open":phase_==Phase::Body?"body":phase_==Phase::Return?"return_to_menu":"close";
        const auto& steps=content_.catalog.at("readings").at(definition_).at(stage);
        if(next_==steps.size()){nextPhase();continue;}
        const auto& step=steps.at(next_++);
        if(step.at("kind")=="message"){
            const auto& source=content_.messages.at(step.at("message").get<std::string>());auto message=resolveCombatMessage(source,session_);
            if(source.inheritDisplay){message.display=display_;}message.restoreDisplay=display_;
            // CE151 is a new foreground root; no actor/menu face is inherited into messages.
            if(source.inheritFace){message.faceImage.clear();}dialogue_.request(message);return;
        }
        const std::string id=step.at("cue");const auto& cue=content_.cue(id);const std::string kind=cue.at("kind");
        if(kind=="message_options"){const auto& v=cue.at("value");display_={v.at("transparent"),v.at("position"),v.at("fixed"),v.at("continue_events")};}
        else if(kind=="sound"){presentation_.push_back({PresentationEvent::Kind::Sound,id,readSound(cue.at("sound")),{},0});}
        else if(kind=="wait"){wait_=std::max(1,cue.at("tenths").get<int>()*6);return;}
        else{presentation_.push_back({PresentationEvent::Kind::Cue,id,{},{},0});if(cue.value("wait",false)){wait_=cue.value("duration_tenths",0)*6;if(wait_>0){return;}}}
    }
}

void ReadingController::repeat(){if(!atMenu()){throw std::logic_error("Reading repeat before menu return");}phase_=Phase::Open;next_=0;wait_=0;}
