#include "opening_controller.h"
#include "combat_scene.h"
#include "scene_presentation.h"
#include <algorithm>
#include <cmath>
namespace{bool pressed(const InputFrame& i,InputAction a){return i.buttons[static_cast<int>(a)].pressed;}}
OpeningController::OpeningController(CombatScene& scene):scene_(scene){scene_.playerVisible_=false;scene_.messageState_.faceImage.clear();enter(scene_.content_.catalog.at("opening").at("entry_stage"));}
void OpeningController::enter(const std::string& name){
    const auto& stages=scene_.content_.catalog.at("opening").at("stages");
    if(stages.contains(name)){stage_=name;subsequence_.clear();}else{subsequence_=name;}
    const auto& stage=stages.at(stage_);cursor_=0;choice_=nullptr;
    self_=stage_=="transport"?"m87-e19":stage_=="escort"||stage_=="monologue-explosion"?"m4-e3":stage_=="soldier-collapse"?"m4-e2":"m2-e1";
    if(stage_=="existing-first-encounter-handoff"){finished_=true;scene_.finishForeground();scene_.refreshObjectPages();return;}
    if(stage_=="door-approach-common"){operations_=nlohmann::json::array();scene_.refreshObjectPages();return;}
    if(stage.value("control","")=="branch-graph"){
        if(subsequence_.empty()){
            ScenePresentation(scene_).validateSequence(stage.at("enter_once"),self_);for(const auto& op:stage.at("enter_once")){execute(op);}subsequence_=stage.at("entry_subsequence");
        }
        operations_=stage.at("subsequence").at(subsequence_).at("operations");
    }else{operations_=stage.at("operations");}
    ScenePresentation(scene_).validateSequence(operations_,self_);
    if(stage_=="transport"){environment_=true;environmentWait_=0;environmentNext_=0;}
}
void OpeningController::execute(const nlohmann::json& op){
    auto& s=scene_;const std::string k=op.at("kind");
    if(k=="message"){s.requestMessage(op.at("ref"));}
    else if(k=="cue"){
        const std::string id=op.at("ref");const auto& cue=s.content_.cue(id);
        if(cue.at("kind")=="choices"){
            const auto& stage=s.content_.catalog.at("opening").at("stages").at(stage_);
            for(const auto& choice:stage.at("choices")){if(choice.at("cue")==id){choice_=choice;break;}}
            ChoicePrompt prompt;prompt.cancelType=choice_.at("cancel_type");prompt.display=s.messageState_.display;prompt.display.fixedPosition=true;prompt.display.continueEvents=false;
            int index=0;for(const auto& text:cue.at("choices")){MessageLine line;bool enabled=!choice_.at("targets").at(index++).is_null();prompt.enabled.push_back(enabled);if(!enabled){line.tokens.push_back({MessageTokenKind::Color,{},3});}line.tokens.push_back({MessageTokenKind::Text,decodeMessageText(text.get<std::string>()),0});prompt.options.push_back(line);}
            s.requestInput(prompt);
        }else if(cue.at("kind")!="wait_routes"){ScenePresentation(s).executeCue(id,std::any_of(s.world_->objects().begin(),s.world_->objects().end(),[&](const auto& a){return a.id==self_;})?self_:std::string{});}
    }else if(k=="world_fact"){s.session_.setFlag(op.at("switch"),op.at("value"));s.refreshObjectPages();}
    else if(k=="initial_value"){
        const int id=op.at("original_variable"),value=op.at("value");
        if(const auto binding=s.content_.magazineSeedBindings.find(id);binding!=s.content_.magazineSeedBindings.end()){s.session_.seedMagazine(binding->second,value);}
        else if(id==13){s.session_.setVariable(id,value);} // Unavailable later weapon seeds remain offline evidence.
    }else if(k=="grant_item"){s.session_.grant(op.at("item"),op.at("count"));}
    else if(k=="party_member"){if(op.at("playable")){s.session_.joinParty(op.at("actor"));}}
    else if(k=="equip_armor"){s.session_.grant(op.at("item"),1);if(!s.session_.equip(op.at("item"),3)){throw std::runtime_error("Opening armor cannot equip");}}
    else if(k=="system_graphic"){s.presentation_.push_back({PresentationEvent::Kind::SystemGraphic,op.at("system_image"),{},{},0});}
    else if(k=="scene_transfer"){
        const auto& xy=op.at("tile");const auto player=s.world_->player();
        CombatEntry entry;entry.map=op.at("map");entry.playerTile={xy.at(0),xy.at(1)};entry.direction=player.motion.facing();entry.sprite=player.spriteId.value_or("player");
        auto candidate=s.prepareWorld(entry.map,entry.playerTile,entry.direction,s.session_,&entry);const bool visible=s.playerVisible_;
        s.commitWorld(std::move(candidate));auto p=s.world_->player();p.setSpeedLevel(static_cast<int>(std::log2(player.motion.speed()))-1);p.through=player.through;s.world_->setPlayerState(p);s.playerVisible_=visible;
    }else if(k=="stop_environment"){environment_=false;}
    else if(k=="normal_confirm_commit"||k=="difficulty_confirm_commit"){
        const std::string difficulty=k=="normal_confirm_commit"?"normal":op.at("difficulty").get<std::string>();
        if(difficulty!="normal"&&!s.content_.catalog.contains("difficulty")){throw std::runtime_error("Difficulty initialization unavailable");}
        s.session_.confirmOpeningDifficulty(difficulty);
    }
    else if(k=="normal_enemy_initialization"||k=="difficulty_enemy_initialization"){
        s.session_.finishOpeningInitialization(s.content_.catalog.contains("difficulty")?s.content_.catalog.at("difficulty").at("easy_preseed").get<std::vector<int>>():std::vector<int>{});s.refreshObjectPages();
    }
    else if(k=="actor_name"){if(op.at("value")!="大雄"){throw std::runtime_error("Unsupported opening actor name");}}
    else if(k=="decision_sound"){decisionSound_=readSound(op.at("sound"));}
    else if(k!="menu_permission"&&k!="save_permission"&&k!="difficulty_baseline"&&k!="erase_origin_event"){
        throw std::runtime_error("Unsupported opening operation: "+k);
    }
}
void OpeningController::update(const InputFrame& input){
    auto& s=scene_;if(finished_){return;}s.camera_.tick(s.world_->player().motion.position());
    if(s.inputRequest_){
        if(s.inputPrompt_.active()){
            auto feedback=s.inputPrompt_.update(input);
            if(feedback!=PromptFeedback::None){
                const auto sound=feedback==PromptFeedback::Decision?decisionSound_:readSound(s.content_.media.at("ui").at("title").at("sounds").at(feedback==PromptFeedback::Cursor?"cursor":"cancel"));
                s.presentation_.push_back({PresentationEvent::Kind::Sound,"",sound,{},0});
            }
            return;
        }
        auto result=s.takeInputResult(*s.inputRequest_);if(!result){return;}
        const auto* selected=std::get_if<ChoiceSelected>(&result->result);if(!selected){throw std::runtime_error("Unsupported opening cancel branch");}
        const auto& target=choice_.at("targets").at(selected->index);
        if(target.is_null()){
            --cursor_;s.presentation_.push_back({PresentationEvent::Kind::Notice,"本版本仅支持 NORMAL；请选择 NORMAL。",{},{},180});return;
        }
        enter(target.get<std::string>());return;
    }
    ScenePresentation(s).advanceRoutes();
    s.world_->advanceCharacters();s.refreshObjectPages();
    if(environment_){
        if(environmentWait_>0){--environmentWait_;}else{
            const auto& refs=s.content_.catalog.at("opening").at("stages").at("transport").at("environment").at("repeat_presentation");
            const std::string id=refs.at(environmentNext_++);environmentNext_%=refs.size();const auto& c=s.content_.cue(id);
            if(c.at("kind")=="wait"){environmentWait_=c.at("tenths").get<int>()*6;}else{s.cue(id,"m87-e20");}
        }
    }
    if(s.wait_>0){--s.wait_;return;}
    if(s.messageRequest_){
        s.dialogue_.update(pressed(input,InputAction::Confirm),pressed(input,InputAction::Cancel));
        if(auto done=s.dialogue_.takeCompletion()){s.messageRequest_.reset();}return;
    }
    if(stage_=="door-approach-common"){
        auto p=s.world_->player();if(!p.motion.moving()&&input.movement){p.setSpeedLevel(4);p.face(*input.movement);if(s.world_->canMove(p,*input.movement)){p.motion.beginStep(*input.movement);}s.world_->setPlayerState(p);}
        const auto& door=s.content_.objects.at("m4-e2");
        if(s.session_.flag(46)&&!p.motion.moving()&&pressed(input,InputAction::Confirm)&&adjacentTile(p.motion.tile(),p.motion.facing())==door.tile){enter("soldier-collapse");}return;
    }
    if(cursor_<operations_.size()){
        const auto& op=operations_.at(cursor_);
        if(op.at("kind")=="cue"&&s.content_.cue(op.at("ref").get<std::string>()).at("kind")=="wait_routes"&&s.routesPending()){return;}
        ++cursor_;try{execute(op);}catch(const std::exception& e){throw std::runtime_error(stage_+" "+op.at("source").at("location").get<std::string>()+": "+e.what());}return;
    }
    const auto& stage=s.content_.catalog.at("opening").at("stages").at(stage_);
    const auto& segment=subsequence_.empty()?stage:stage.at("subsequence").at(subsequence_);
    if(segment.contains("next")&&!segment.at("next").is_null()){enter(segment.at("next"));}
}
