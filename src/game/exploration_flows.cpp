#include "combat_scene.h"
#include "full_content.h"
#include "guard_controller.h"
#include "dekisugi_controller.h"
#include "escort_controller.h"
#include "scene_presentation.h"
#include "weapon_controller.h"
#include "enemy_controller.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <utility>
bool CombatScene::isCurrentPage(const std::string& id,int page) const{
    const auto& object=content_.objects.at(id);if(erasedEvents_.contains(id)||object.map!=map_){return false;}
    const auto* selected=content_.activeVariant(object,session_);return selected&&selected->page==page;
}
bool CombatScene::contentFlowAllowed(const nlohmann::json& flow) const{
    if(flow.contains("facing")&&flow.at("facing")!=directionName(world_->player().motion.facing())){return false;}
    if(flow.value("requires_lowered",false)&&weapon_->raised()){return false;}
    const auto player=world_->player().motion.tile();
    if(flow.contains("player_tile")){const auto& xy=flow.at("player_tile");if(player!=TilePosition{xy[0],xy[1]}){return false;}}
    if(flow.contains("player_relative")){
        const auto& relative=flow.at("player_relative");const auto base=findCharacter(*world_,relative.at("object")).motion.tile();const auto& xy=relative.at("offset");
        if(player!=TilePosition{base.x+xy[0].get<int>(),base.y+xy[1].get<int>()}){return false;}
    }
    if(flow.contains("destination")){
        const auto& d=flow.at("destination");auto target=findCharacter(*world_,d.at("object")).motion.tile();
        if(d.contains("direction")){target=adjacentTile(target,directionFromName(d.at("direction").get<std::string>()));}else{target.x+=d.at("offset").at(0).get<int>();target.y+=d.at("offset").at(1).get<int>();}
        if(target.x<0||target.y<0||target.x>=definition_.width||target.y>=definition_.height||world_->mappedLowerTerrain(target).value_or(content_.terrainIds.at(map_).at(target.y*definition_.width+target.x))==d.at("blocked_terrain").get<int>()){return false;}
        if(d.contains("blocked_object")&&findCharacter(*world_,d.at("blocked_object")).motion.tile()==target){return false;}
    }
    return true;
}
std::optional<SaveRequest> CombatScene::takeSaveRequest(){return std::exchange(saveRequest_,{});}
bool CombatScene::clearSavePending(const SaveRequest& r) const{
    return active_&&r.scene==generation_&&clearSave_&&r.request==clearSave_->request&&r.object==clearSave_->object&&r.kind=="clear_after_file"&&foreground_==ForegroundKind::Content;
}
bool CombatScene::finishClearSave(const SaveRequest& r){if(!clearSavePending(r)){return false;}clearSave_.reset();++phase_;return true;}
void CombatScene::installClearContinuation(const std::string& node){
    const auto& clear=content_.catalog.at("clear");if(node!=clear.at("node").get<std::string>()||map_!=clear.at("map")){throw std::runtime_error("Unknown Clear continuation");}
    activeContentFlow_=clear.at("flow");activeFlowSteps_=clear.at("after_file").get<std::vector<nlohmann::json>>();startForeground(ForegroundKind::Content);playerVisible_=false;
    // Load rebuilds stored BGM but never reexecutes source C2/C6.
    musicPlayback_=MusicPlaybackIdentity{generation_,++nextMusicPlayback_,clear.at("music").at("resource")};clearMusicTicks_.reset();
    presentation_.push_back({PresentationEvent::Kind::Music,"restore-clear-instance",readSound(clear.at("music")),{},{}});presentation_.back().musicPlayback=musicPlayback_;
}
bool CombatScene::reportMusicTicks(const MusicPlaybackIdentity& identity,int ticks){
    if(!active_||identity.scene!=generation_||musicPlayback_!=identity||!content_.catalog.contains("clear")||identity.resource!=content_.catalog.at("clear").at("music").at("resource").get<std::string>()||ticks<0||ticks>2000){return false;}
    clearMusicTicks_=ticks;return true;
}
std::optional<PostClearRequest> CombatScene::postClearRequest() const{
    if(!active_||foreground_!=ForegroundKind::Content||activeContentFlow_!="m216-e1-p1"||phase_<0||phase_>=int(activeFlowSteps_.size())||activeFlowSteps_[phase_].at("kind")!="post_clear_request"){return {};}
    return PostClearRequest{generation_,activeFlowSteps_[phase_].at("selection")};
}
bool CombatScene::chapterSavePending(const SaveRequest& r) const{
    return active_&&r.scene==generation_&&chapterSave_&&r.request==chapterSave_->request&&r.object==chapterSave_->object&&r.kind=="chapter_handoff_after_file"&&foreground_==ForegroundKind::Content;
}
bool CombatScene::finishChapterSave(const SaveRequest& r){
    if(!chapterSavePending(r)){return false;}chapterSave_.reset();++phase_;return true;
}
void CombatScene::installChapterContinuation(const std::string& node){
    const auto& chapter=content_.catalog.at("chapter");if(node!=chapter.at("node").get<std::string>()||map_!=chapter.at("map")){throw std::runtime_error("Unknown chapter continuation");}
    ScenePresentation(*this).validateSequence(chapter.at("after_file"),chapter.at("object"));
    activeContentFlow_=chapter.at("flow");activeFlowSteps_=chapter.at("after_file").get<std::vector<nlohmann::json>>();
    chapterSave_.reset();playerVisible_=chapter.at("player_visible");startForeground(ForegroundKind::Content);
}
void CombatScene::refreshObjectPages(){
    for(const auto& original:world_->objects()){
        if(erasedEvents_.contains(original.id)||(guard_&&guard_->owns(original.id))||(dekisugi_&&dekisugi_->owns(original.id))){continue;}
        // Failure presentation owns temporary actors without catalogue event pages.
        const auto object=content_.objects.find(original.id);if(object==content_.objects.end()){continue;}
        const auto controller=std::find_if(enemies_.begin(),enemies_.end(),[&](const auto& enemy){return enemy->id()==original.id;});
        // Before activation, normal event pages own visibility and the story corpse.
        // Once combat starts, the enemy state machine owns its presentation.
        if(controller!=enemies_.end()&&(*controller)->mode()!=EnemyMode::Dormant){continue;}
        const auto* selected=content_.activeVariant(object->second,session_);
        const int page=selected?selected->page:0;if(selectedPages_[original.id]==page){continue;}
        selectedPages_[original.id]=page;auto actor=original;
        actor.active=selected!=nullptr;
        if(selected){const auto& d=selected->definition;actor.setSpeedLevel(d.moveSpeed);actor.through=actor.routeThrough.value_or(d.through);actor.spriteId=d.spriteId;actor.layer=d.layer;actor.renderTiePriority=d.renderTiePriority;actor.transparency=d.transparency;actor.tilePassageFlags=d.tilePassageFlags;actor.trigger=d.trigger;actor.overlapForbidden=d.overlapForbidden;actor.setAnimation(CharacterAnimation(animationForSpeed(d.moveSpeed),d.initialColumn,d.animationType),directionFromName(d.direction));}
        replaceCharacter(*world_,actor);
        std::erase_if(ambientRoutes_,[&](const auto& r){return r.actor()==actor.id;});
        if(controller==enemies_.end()&&selected&&!selected->autonomousRoute.steps.empty()){ambientRoutes_.emplace_back(selected->autonomousRoute,actor.id);}
    }
}
void CombatScene::eraseEvent(const std::string& id){
    if(!content_.objects.contains(id)||content_.objects.at(id).map!=map_){throw std::runtime_error("Invalid erased event target: "+id);}
    auto actor=findCharacter(*world_,id);erasedEvents_.insert(id);actor.active=false;replaceCharacter(*world_,actor);
    std::erase_if(routes_,[&](const auto& r){return r.actor()==id;});std::erase_if(ambientRoutes_,[&](const auto& r){return r.actor()==id;});
}
void CombatScene::startContentFlow(const std::string& id){
    const auto& entry=content_.catalog.at("flows").at(id);
    if(!entry.value("runtime_enabled",true)){
        blockedFlows_.insert(id);presentation_.push_back({PresentationEvent::Kind::Notice,entry.value("pending_label","此流程等待任务 "+std::to_string(entry.at("implementation_ticket").get<int>())+" 接入。"),{},{},180});
        // A compulsory scene cannot become a free exploration room while awaiting its owner.
        if(entry.at("trigger")=="autorun"||(entry.at("trigger")=="contact"&&entry.value("handoff_only",false))){activeContentFlow_=id;activeFlowSteps_.clear();startForeground(ForegroundKind::Content);}
        return;
    }
    if(id=="escort-fall"){throw std::runtime_error("Escort fall must run inside its calling story");}
    const auto& definition=content_.catalog.at("flows").at(id);
    try{ScenePresentation(*this).validateSequence(definition.at("steps"),definition.at("object"));}
    catch(const std::exception& error){
        // An action transfer is still in its old scene: preparation has not locked
        // controls, reset room facts or published any part of the next scene.
        if(definition.at("trigger")!="action"){throw;}
        presentation_.push_back({PresentationEvent::Kind::Notice,std::string("无法准备演出：")+error.what(),{},{},240});return;
    }
    if(definition.contains("nine_cell_panel")){activeDrainageProfile_=definition.at("nine_cell_panel");}
    else if(id=="drainage-open"){activeDrainageProfile_="drainage";}
    flowOriginTile_=world_->player().motion.tile();activeContentFlow_=id;activeFlowSteps_=content_.catalog.at("flows").at(id).at("steps").get<std::vector<nlohmann::json>>();
    const auto actor=world_->player();const bool weaponPose=weapon_->raised()||weapon_->busy()||weapon_->aiming()||session_.flag(3);
    // Leave the CE5 attachment condition in Session until its explicit source step commits.
    const bool sourceCleanup=std::any_of(activeFlowSteps_.begin(),activeFlowSteps_.end(),[](const auto& step){return step.value("kind",std::string{})=="room_reset"||(step.value("kind",std::string{})=="cancel_player_actions"&&step.value("cleanup",std::string{})=="ce5");});
    weapon_->cancel(ActionCancel::Foreground,sourceCleanup);
    // Weapon cleanup must not overwrite an idle actor's explicit story graphic/speed.
    if(!weaponPose){auto player=world_->player();player.spriteId=actor.spriteId;player.setSpeedLevel(static_cast<int>(std::log2(actor.motion.speed()))-1);world_->setPlayerState(player);}
    startForeground(ForegroundKind::Content);updateForeground();
}
std::optional<EndingClearRequest> CombatScene::endingClearRequest() const{
    if(!active_||foreground_!=ForegroundKind::Content||!content_.catalog.contains("score")||activeContentFlow_!="m214-e1-p1"||phase_<0||phase_>=static_cast<int>(activeFlowSteps_.size())){return {};}
    const auto& step=activeFlowSteps_[phase_];if(step.at("kind")!="required_relocate"||step.at("destination")!=content_.catalog.at("score").at("clear_destination")){return {};}
    const auto& d=step.at("destination");return EndingClearRequest{generation_,d.at("map"),{d.at("tile").at(0),d.at("tile").at(1)}};
}
bool CombatScene::reportMusicFirstPass(const MusicPlaybackIdentity& identity){
    if(!active_||identity.scene!=generation_||musicPlayback_!=identity||musicFirstPass_||!content_.catalog.contains("epilogue")){return false;}
    const auto& source=content_.catalog.at("epilogue").at("credits_music");
    if(!isCurrentPage(source.at("object"),source.at("page"))){return false;}
    musicFirstPass_=true;return true;
}
void CombatScene::updateCreditsMusic(){
    if(!content_.catalog.contains("epilogue")){return;}
    const auto& source=content_.catalog.at("epilogue").at("credits_music");const auto self=source.at("object").get<std::string>();
    if(!musicPlayback_||musicPlayback_->scene!=generation_||!isCurrentPage(self,source.at("page"))){return;}
    if(creditsMusicWait_<0){
        if(!musicFirstPass_){return;}
        cue(source.at("fade"),self);creditsMusicWait_=source.at("wait_tenths").get<int>()*6;return;
    }
    if(creditsMusicWait_>0&&--creditsMusicWait_>0){return;}
    cue(source.at("stop"),self);eraseEvent(self);
}
void CombatScene::updateContentFlow(){
    const std::map<std::string,EventTimerSource> timerSources{{"training",EventTimerSource::Training},{"power",EventTimerSource::Power},{"guard-first",EventTimerSource::GuardFirst},{"guard-final",EventTimerSource::GuardFinal},{"access-key",EventTimerSource::AccessKey},{"guard-second",EventTimerSource::GuardFinal}};
    const auto& flow=content_.catalog.at("flows").at(activeContentFlow_);const auto& steps=activeFlowSteps_;
    if(!flow.value("runtime_enabled",true)){return;}
    const auto self=content_.objects.at(flow.at("object").get<std::string>()).map==map_?flow.at("object").get<std::string>():std::string{};
    while(phase_<static_cast<int>(steps.size())){
        const auto step=steps.at(phase_);const std::string kind=step.at("kind");
        if(kind=="finish"){break;}
        if(kind=="victory_head"){session_.setVariable(105,step.at("state"));++phase_;continue;}
        if(kind=="credits_begin"){creditsPosition_={};session_.setVariable(91,1);++phase_;continue;}
        if(kind=="credits_coordinate"){
            auto& value=step.at("axis")=="x"?creditsPosition_.x:creditsPosition_.y;
            value=step.at("operation")=="set"?step.at("value").get<double>():value+step.at("value").get<double>();++phase_;continue;
        }
        if(kind=="party_change"){
            if(step.at("join").get<bool>()){session_.joinParty(step.at("actor"));}else{session_.leaveParty(step.at("actor"));}++phase_;continue;
        }
        if(kind=="ending_rank"){session_.commitEndingRank();++phase_;continue;}
        if(kind=="ending_rewards"){
            const auto in=session_.endingScoreInput();const auto result=endingScore(in);
            if(session_.variable(381)!=result.rank){throw std::runtime_error("Reward presentation lacks committed rank");}
            std::vector<nlohmann::json> body;
            const auto& awards=content_.catalog.at("score").at("awards");
            for(int id:result.unlocks){const auto key=id==601?(in.hours==0?std::string("601-early"):std::string("601-exact")):std::to_string(id);const auto& rows=awards.at(key);body.insert(body.end(),rows.begin(),rows.end());}
            activeFlowSteps_.insert(activeFlowSteps_.begin()+phase_+1,body.begin(),body.end());++phase_;continue;
        }
        if(kind=="registration_next"){startContentFlow(step.at("flow"));return;}
        if(kind=="registration_keyboard"){activeContentFlow_.clear();openRegistrationKeyboard();return;}

        if(kind=="post_clear_request"){return;}
        if(kind=="clear_actor_name"){session_.setActor8Name(step.at("name"));++phase_;continue;}
        if(kind=="clear_music_gate"){
            if(!clearMusicTicks_){return;}const auto& gate=content_.catalog.at("clear").at("midi_gate");session_.setVariable(gate.at("variable"),*clearMusicTicks_);
            const nlohmann::json tail=*clearMusicTicks_<=gate.at("threshold").get<int>()?nlohmann::json::array({{{"kind","required_relocate"},{"destination",gate.at("destination")}},{{"kind","finish"}}}):nlohmann::json::array({{{"kind","clear_title"}}});
            activeFlowSteps_.insert(activeFlowSteps_.begin()+phase_+1,tail.begin(),tail.end());++phase_;continue;
        }
        if(kind=="clear_title"){requestedMode_=ApplicationMode::Title;++phase_;return;}
        if(kind=="clear_save"){
            if(!clearSave_){const auto& clear=content_.catalog.at("clear");if(activeContentFlow_!=clear.at("flow").get<std::string>()||!isCurrentPage(clear.at("object"),clear.at("page"))){throw std::runtime_error("Invalid Clear save source");}clearSave_=SaveRequest{generation_,clear.at("node"),clear.at("location"),"clear_after_file",++nextSaveRequest_};}
            saveRequest_=clearSave_;return;
        }
        if(kind=="chapter_save"){
            if(!chapterSave_){const auto& chapter=content_.catalog.at("chapter");if(activeContentFlow_!=chapter.at("flow").get<std::string>()||step.at("node")!=chapter.at("node")||!isCurrentPage(chapter.at("object"),chapter.at("page"))){throw std::runtime_error("Invalid chapter save source");}
                chapterSave_=SaveRequest{generation_,step.at("node"),chapter.at("location"),"chapter_handoff_after_file",++nextSaveRequest_};}
            saveRequest_=chapterSave_;return;
        }
        const bool routeBarrier=kind=="wait_routes"||(kind=="cue"&&content_.cue(step.at("cue").get<std::string>()).at("kind")=="wait_routes");
        if(routeBarrier&&routesPending()){return;}
        if(kind=="discard_key"||kind=="password_reward"||kind=="number_input"||kind=="choice"){
            if(!inputRequest_){
                if(kind=="discard_key"||kind=="choice"){
                    const bool inlineOptions=kind=="choice"&&step.contains("options");
                    const auto& source=inlineOptions?step:content_.cue(step.at(kind=="choice"?"cue":"choice").get<std::string>());ChoicePrompt prompt;prompt.cancelType=source.at(inlineOptions?"cancel_option":"cancel_type");prompt.display=messageState_.display;prompt.question=std::exchange(flowQuestion_,{});
                    for(const auto& raw:source.at(inlineOptions?"options":"choices")){const auto text=inlineOptions?raw.at("label").get<std::string>():raw.get<std::string>();prompt.options.push_back({text,{{MessageTokenKind::Text,decodeMessageText(text),0}}});}
                    requestInput(prompt);
                }else{NumberPrompt prompt;prompt.digits=step.at("digits");if(kind=="number_input"){int limit=1;for(int digit=0;digit<prompt.digits;++digit){limit*=10;}prompt.initial=std::clamp(session_.variable(guardSourceVariable(step.at("field"))),0,limit-1);}prompt.display=messageState_.display;requestInput(prompt);}
                return;
            }
            const auto result=takeInputResult(*inputRequest_);if(!result){return;}
            if(kind=="choice"){
                const auto* selected=std::get_if<ChoiceSelected>(&result->result);
                if(step.contains("options")&&!selected){throw std::runtime_error("Finite choice result mismatch");}
                const auto& branch=step.contains("options")?step.at("options").at(selected->index).at("steps"):step.at(selected&&selected->index==0?"accept":"decline");
                activeFlowSteps_.insert(activeFlowSteps_.begin()+phase_+1,branch.begin(),branch.end());
            }else if(kind=="discard_key"){
                const auto* selected=std::get_if<ChoiceSelected>(&result->result);if(!selected){throw std::runtime_error("Key choice result mismatch");}
                if(selected->index==0&&!session_.discard(step.at("item"),1)){throw std::runtime_error("Key disappeared during discard request");}
            }else{
                const auto* number=std::get_if<NumberSubmitted>(&result->result);if(!number){throw std::runtime_error("Password result mismatch");}
                if(kind=="number_input"){session_.setVariable(guardSourceVariable(step.at("field")),number->value);}
                else{const auto& branch=step.at(number->value==session_.variable(step.at("password_variable"))?"success":"failure");
                activeFlowSteps_.insert(activeFlowSteps_.begin()+phase_+1,branch.begin(),branch.end());}
            }
            ++phase_;continue;
        }
        if(kind=="if_player_moved"||kind=="unless_flag"||kind=="if_flag"){
            const bool apply=kind=="if_player_moved"?world_->player().motion.tile()!=flowOriginTile_:session_.flag(step.at("flag"))==(kind=="if_flag");
            if(apply){const auto body=step.at("steps");activeFlowSteps_.insert(activeFlowSteps_.begin()+phase_+1,body.begin(),body.end());}
            ++phase_;continue;
        }
        if(kind=="guard_reward_tier"){for(const auto& tier:step.at("tiers")){if(session_.variable(269)>=tier.at("kills_at_least").get<int>()&&session_.variable(267)>=tier.at("rescues_at_least").get<int>()){const auto body=tier.at("steps");activeFlowSteps_.insert(activeFlowSteps_.begin()+phase_+1,body.begin(),body.end());break;}}++phase_;continue;}
        if(kind=="branch"){
            const auto& condition=step.at("condition");const auto type=condition.at("kind").get<std::string>();const int id=condition.value("id",0);
            bool apply=false;
            if(type=="fact"){apply=session_.flag(id)==condition.at("value").get<bool>();}
            else if(type=="item"){
                bool owns=session_.count(id)>0;for(int slot=1;slot<=5;++slot){owns|=session_.equipped(slot)==id;}
                apply=owns==condition.at("present").get<bool>();
            }
            else if(type=="actor"){apply=session_.inParty(id);}
            else if(type=="known_skill"){apply=id==session_.actorId()&&session_.knowsSkill(condition.at("value"));}
            else if(type=="player_facing"){apply=directionName(world_->player().motion.facing())==condition.at("value").get<std::string>();}
            else if(type=="difficulty_at_least"){apply=(session_.difficulty()=="hard"?3:session_.difficulty()=="normal"?2:1)>=condition.at("value").get<int>();}
            else if(type=="actor_facing"){
                const auto facing=directionName(findCharacter(*world_,condition.at("object")).motion.facing());const auto& directions=condition.at("directions");apply=std::find(directions.begin(),directions.end(),facing)!=directions.end();
            }
            else if(type=="domain_compare"){
                const auto field=condition.at("field");
                const int coordinate=field=="player_tile_x"?world_->player().motion.tile().x:field=="player_tile_y"?world_->player().motion.tile().y:session_.variable(guardSourceVariable(field)),boundary=condition.at("operand").at("constant");
                const auto op=condition.at("operator");apply=op=="at_least"?coordinate>=boundary:op=="at_most"?coordinate<=boundary:op=="equal"?coordinate==boundary:op=="not_equal"?coordinate!=boundary:op=="greater"?coordinate>boundary:coordinate<boundary;
            }
            else{throw std::runtime_error("Unsupported finite branch condition: "+type);}
            const auto body=step.at(apply?"then":"else");activeFlowSteps_.insert(activeFlowSteps_.begin()+phase_+1,body.begin(),body.end());
            ++phase_;continue;
        }
        if(kind=="discard_key_choice"){
            const auto body=content_.catalog.at("flows").at(step.at("flow").get<std::string>()).at("steps");
            activeFlowSteps_.insert(activeFlowSteps_.begin()+phase_+1,body.begin(),body.end());++phase_;continue;
        }
        if(kind=="file_reading"){
            const auto& reading=content_.catalog.at("readings").at(step.at("reading").get<std::string>());
            std::vector<nlohmann::json> body;for(const auto* part:{"open","body","close"}){for(const auto& op:reading.at(part)){body.push_back(op);}}
            activeFlowSteps_.insert(activeFlowSteps_.begin()+phase_+1,body.begin(),body.end());++phase_;continue;
        }
        if(kind=="presentation_group"){
            std::vector<nlohmann::json> body;for(const auto& id:step.at("cues")){body.push_back({{"kind","cue"},{"cue",id}});}
            activeFlowSteps_.insert(activeFlowSteps_.begin()+phase_+1,body.begin(),body.end());++phase_;continue;
        }
        if(kind=="cue"){if(!ScenePresentation(*this).executeCue(step.at("cue"),self)){return;}++phase_;if(wait_>0||messageRequest_){return;}continue;}
        // A rejected candidate must retry this transfer, without replaying earlier rewards.
        if(kind=="track_picture_until_flag"){
            samplePictureAnchor({{"anchor",step.at("anchor")},{"actor",step.at("actor")},{"sample",nlohmann::json::array({"x","y"})},{"offset",step.at("offset")}},self);
            cue(step.at("cue"),self);
            if(session_.flag(step.at("flag"))!=step.at("value").get<bool>()){wait_=1;return;}
            ++phase_;continue;
        }
        if(kind=="continue_flow"){
            // The source callers end here: switch body and Self without releasing foreground.
            activeContentFlow_=step.at("flow");activeFlowSteps_=content_.catalog.at("flows").at(activeContentFlow_).at("steps").get<std::vector<nlohmann::json>>();phase_=0;
            return updateContentFlow();
        }
        if(kind=="required_relocate"){
            const int target=step.at("destination").at("map");
            if(content_.catalog.contains("pending_maps")&&content_.catalog.at("pending_maps").contains(std::to_string(target))){
                if(transferError_.empty()){transferError_="Required destination awaits its gameplay consumer";presentation_.push_back({PresentationEvent::Kind::Notice,"后续战斗尚未接入，入口前奏保持源转场占用",{},{},180});}
                wait_=1;return;
            }
        }
        if(kind=="relocate"||kind=="required_relocate"){
            const auto& destination=step.at("destination");const auto& tile=destination.at("tile");const auto facing=step.value("facing",std::string("preserve"));
            if(destination.at("map").get<int>()==map_){
                const TilePosition target{tile.at(0),tile.at(1)};
                if(!world_->containsTile(target)){throw std::runtime_error("Same-map transfer outside map");}
                auto player=world_->player();player.motion=CharacterMotion(target,facing=="preserve"?player.motion.facing():directionFromName(facing),player.motion.speed());player.face(player.motion.facing());world_->setPlayerState(player);
                ++phase_;continue;
            }
            PreparedWorld prepared;
            try{prepared=prepareWorld(destination.at("map"),{tile.at(0),tile.at(1)},facing=="preserve"?world_->player().motion.facing():directionFromName(facing),session_);}
            catch(const std::exception& error){
                if(kind!="required_relocate"){throw;}
                if(transferError_.empty()){presentation_.push_back({PresentationEvent::Kind::Notice,"转场准备失败，保留入口与检查点",{},{},180});}transferError_=error.what();wait_=1;return;
            }
            transferError_.clear();
            // Source transfers retain actor graphics and route speed, but start new motion.
            auto player=prepared.world->player();player.spriteId=world_->player().spriteId;
            player.setSpeedLevel(static_cast<int>(std::log2(world_->player().motion.speed()))-1);
            if(step.value("preserve_player_through",false)){player.through=world_->player().through;player.routeThrough=world_->player().routeThrough;}
            prepared.world->setPlayerState(player);
            const auto continuation=activeContentFlow_;auto remaining=std::move(activeFlowSteps_);const auto next=phase_+1;const bool visible=playerVisible_;
            commitWorld(std::move(prepared));playerVisible_=visible;activeContentFlow_=continuation;activeFlowSteps_=std::move(remaining);phase_=next;
            return updateContentFlow();
        }
        ++phase_;
        if(kind=="capture_continue_once"){
            const auto& destination=step.at("destination");const auto& checkpoint=continueCheckpoint(content_,destination.at("map"));const auto& tile=destination.at("tile");
            if(checkpoint.at("tile")!=tile){throw std::runtime_error("Unbound Continue capture tile");}
            session_.captureContinueCheckpoint(content_.magazineSeedBindings,destination.at("map"),{tile.at(0),tile.at(1)});
        }
        else if(kind=="clear_combat_workspace"){for(int id=step.at("first");id<=step.at("last").get<int>();++id){session_.setVariable(id,0);}}
        else if(kind=="actor_display"){if(step.at("actor")!=session_.actorId()){throw std::runtime_error("Actor display target unavailable");}auto difficulty=session_.difficulty();std::transform(difficulty.begin(),difficulty.end(),difficulty.begin(),[](unsigned char c){return std::toupper(c);});session_.setActorDisplay(step.contains("name_by_difficulty")?step.at("name_by_difficulty").at(difficulty):step.at("name"),step.at("face"),step.at("face_index"));}
        else if(kind=="sample_equipped_weapon"){session_.setVariable(7,session_.equipped(1));}
        else if(kind=="flag"){session_.setFlag(step.at("id"),step.at("value"));refreshObjectPages();}
        else if(kind=="ending_unlock"){session_.commitEndingUnlock(step.at("id"));}
        else if(kind=="actor_resource"){session_.addResources(step.at("resource")=="hp"?step.at("points").get<int>():0,step.at("resource")=="tp"?step.at("points").get<int>():0);}
        else if(kind=="restore_actor"){
            if(step.at("actor")==session_.actorId()){session_.restoreActor();}
            else{session_.resetPursuerHealth();}
        }
        else if(kind=="flags"){
            for(const auto& id:step.at("ids")){session_.setFlag(id,step.at("value"));}refreshObjectPages();
        }
        else if(kind=="toggle_flag"){const int id=step.at("id");session_.setFlag(id,!session_.flag(id));refreshObjectPages();}
        else if(kind=="timer_set"){
            session_.setEventTimer(timerSources.at(step.at("timer_source").get<std::string>()),step.at("seconds"));
        }
        else if(kind=="timer_start"){session_.startEventTimer(step.at("visible"),step.at("battle"));}
        else if(kind=="timer_stop"){session_.stopEventTimer();}
        else if(kind=="room_reset"){
            routes_.clear();session_.resetRoomFacts();
            if(content_.full->contains("runtime")&&content_.full->at("runtime").contains("player_action_cleanup")){cancelPlayerActions("ce5",self);}
            activeFlowSteps_.insert(activeFlowSteps_.begin()+phase_,nlohmann::json{{"kind","wait_routes"}});
        }
        else if(kind=="timer"){
            const auto source=timerSources.at(step.at("source_id").get<std::string>());
            const auto operation=step.at("operation").get<std::string>();
            if(operation=="set"){session_.setEventTimer(source,step.at("seconds"));}
            else if(operation=="start"){session_.startEventTimer(step.at("show"),false);}
            else if(operation=="stop"){session_.stopEventTimer();}
            else{throw std::runtime_error("Unsupported finite timer operation: "+operation);}
        }
        else if(kind=="system_graphic"){presentation_.push_back({PresentationEvent::Kind::SystemGraphic,step.at("image"),{},{},0});}
        else if(kind=="learn_skill"){session_.learnSkill(step.at("skill"));}
        else if(kind=="forget_skill"){session_.forgetSkill(step.at("skill"));}
        else if(kind=="combine_skill"){
            if(step.at("learn").get<bool>()){session_.learnSkill(step.at("skill"));}else{session_.forgetSkill(step.at("skill"));}
        }
        else if(kind=="scene_sample"){
            const auto tile=findCharacter(*world_,step.at("entity")).motion.tile();session_.setVariable(guardSourceVariable(step.at("field")),step.at("component")=="tile_x"?tile.x:tile.y);
        }
        else if(kind=="domain_value"){
            if(step.at("field")=="player_pose"){session_.setVariable(31,step.at("value"));}
            else if(step.at("field")=="corridor_door_status"){session_.setVariable(193,step.at("value"));}
            else if(step.at("field")=="light_facing"||step.at("field")=="light_saved_facing"){lightStates_.clear();}
            else{const int id=guardSourceVariable(step.at("field"));session_.setVariable(id,step.at("operation")=="add"?session_.variable(id)+step.at("value").get<int>():step.at("operation")=="subtract"?session_.variable(id)-step.at("value").get<int>():step.at("value").get<int>());}
            refreshObjectPages();
        }
        else if(kind=="domain_arithmetic"&&step.at("field").get<std::string>().starts_with("escort_")){
            if(!escort_){throw std::runtime_error("Escort domain outside its Scene");}const auto field=step.at("field").get<std::string>();const int n=step.at("operand").at("constant");const auto operation=step.at("operation");escort_->assign(field,operation=="set"?n:operation=="add"?escort_->value(field)+n:escort_->value(field)*n);
        }
        else if(kind=="tile_substitution"){applyTileSubstitution(step);}
        else if(kind=="domain_arithmetic"){const int id=guardSourceVariable(step.at("field"));const auto& operand=step.at("operand");const int n=operand.contains("constant")?operand.at("constant").get<int>():session_.variable(guardSourceVariable(operand.at("field")));const auto op=step.at("operation");session_.setVariable(id,op=="multiply"?session_.variable(id)*n:op=="add"?session_.variable(id)+n:n);}
        else if(kind=="companion_level_refresh"||kind=="companion_party"){} // Encounter-local party never changes the durable Actor1 snapshot.
        else if(kind=="reset_guard_work"){const auto& range=step.at("range");for(int id=range.at(0);id<=range.at(1);++id){session_.setVariable(id,0);}}
        else if(kind=="restore_companion"){if(guard_){guard_->restore();}}
        else if(kind=="invoke_controller"){
            if(step.at("controller")=="escort-fall-choreography"){
                if(!escort_||step.at("object")!=content_.catalog.at("escort").at("fall_object")){throw std::runtime_error("Unbound escort fall");}
                const auto body=content_.catalog.at("flows").at("escort-fall").at("steps");activeFlowSteps_.insert(activeFlowSteps_.begin()+phase_,body.begin(),body.end());continue;
            }
            if(step.at("controller")!="crocodile-intro"){throw std::runtime_error("Unsupported finite controller");}
            const auto body=content_.catalog.at("flows").at("crocodile-intro").at("steps");activeFlowSteps_.insert(activeFlowSteps_.begin()+phase_,body.begin(),body.end());
        }
        else if(kind=="decision_sample"){
            // CE39 samples Decision before its S1 attack gate. CE55 owns the reset;
            // equipped ID, magazine and accessory are Session state, never scratch V8.
            if(step.at("profile")!="ce39-after-room-reset"||session_.flag(1)||session_.flag(4)||session_.flag(19)||session_.flag(574)){
                throw std::runtime_error("CE39 Decision sample outside reviewed post-reset state");
            }
            const int weapon=session_.equipped(1);
            const bool waiting=session_.magazine(weapon)<=0&&session_.equipped(4)!=55&&weapon<57;
            session_.setVariable(229,waiting?0:(decisionHeld_?5:0));
            if(waiting){decisionWaiting_=true;return;} // This frame's trigger predates source ResetTriggerKeys.
        }
        else if(kind=="file_reading_lock"){fileReadingLocked_=step.at("locked");}
        else if(kind=="normalize_loose_ammo"){session_.organizeAmmo();}
        else if(kind=="finish_event"){}
        else if(kind=="place_player"){auto actor=world_->player();const auto& tile=step.at("tile");actor.motion=CharacterMotion({tile[0],tile[1]},actor.motion.facing(),actor.motion.speed());world_->setPlayerState(actor);}
        else if(kind=="reset_player_actions"){weapon_->cancel(ActionCancel::Foreground);}
        else if(kind=="set_enemy_damage"){
            const auto id=step.at("enemy").get<std::string>();const auto found=std::find_if(enemies_.begin(),enemies_.end(),[&](const auto& enemy){return enemy->id()==id;});
            if(found==enemies_.end()||!(*found)->setScriptedDamage(step.at("damage"))){throw std::runtime_error("Unsupported scripted enemy damage");}
        }
        else if(kind=="picture_anchor"){samplePictureAnchor(step,self);}
        else if(kind=="grant"){session_.grant(step.at("item"),step.at("count"));}
        else if(kind=="selected_key"){session_.setVariable(44,step.at("item"));}
        else if(kind=="remove_selected_key"){
            const int item=session_.variable(step.at("key_slot"));(void)content_.items.at(item);
            const int amount=std::min(session_.count(item),step.at("count").get<int>());if(amount){session_.discard(item,amount);}
        }
        else if(kind=="convert_tool"){if(!session_.convertTool(step.at("from"),step.at("to"))){throw std::runtime_error("Tool changed during foreground conversion");}}
        else if(kind=="remove"){const int amount=std::min(session_.count(step.at("item")),step.at("count").get<int>());if(amount){session_.discard(step.at("item"),amount);}}
        else if(kind=="place_at_player"){
            auto object=findCharacter(*world_,step.at("object"));object.motion=CharacterMotion(world_->player().motion.tile(),object.motion.facing(),object.motion.speed());replaceCharacter(*world_,object);
        }
        else if(kind=="place_object"){
            auto object=findCharacter(*world_,step.at("object"));const auto tile=step.contains("tile_fields")?TilePosition{session_.variable(guardSourceVariable(step.at("tile_fields")[0])),session_.variable(guardSourceVariable(step.at("tile_fields")[1]))}:TilePosition{step.at("tile")[0],step.at("tile")[1]};object.motion=CharacterMotion(tile,object.motion.facing(),object.motion.speed());replaceCharacter(*world_,object);
        }else if(kind=="cancel_player_actions"){
            cancelPlayerActions(step.value("cleanup",std::string{}),self);
        }
        else if(kind=="enemy_entry_state"){
            const auto& enemy=content_.expansion.enemies.at(step.at("enemy"));
            if(step.at("state")!=9||!enemy.rules.contains("entry_state_variable")){throw std::runtime_error("Unsupported enemy entry state");}
            session_.setVariable(enemy.rules.at("entry_state_variable"),9);
        }
        else if(kind=="enemy_state"){
            const auto found=std::find_if(enemies_.begin(),enemies_.end(),[&](const auto& enemy){return enemy->id()==step.at("enemy").get<std::string>();});
            if(found==enemies_.end()||!(*found)->activateState(step.at("state"))){throw std::runtime_error("Unsupported encounter activation");}
        }
        else if(kind=="erase_event_once"){eraseEvent(step.value("object",flow.at("object").get<std::string>()));}
        else if(kind=="invoke_flow"){
            const auto& called=content_.catalog.at("flows").at(step.at("flow").get<std::string>());
            eraseEvent(called.at("steps").at(0).value("object",called.at("object").get<std::string>()));
        }
        else if(kind=="message"){requestMessage(step.at("message"));}
        else if(!routeBarrier){throw std::runtime_error("Unsupported finite exploration step: "+kind);}
        if(wait_>0||messageRequest_){return;}
    }
    if(activeContentFlow_=="drainage-open"||flow.contains("open_nine_cell_panel")){const auto profile=flow.value("open_nine_cell_panel",std::string("drainage"));activeContentFlow_.clear();openDrainage(profile);return;}
    if(!activeDrainageProfile_.empty()&&flow.at("object")==drainageProfile().at("object")){activeDrainageProfile_.clear();drainagePendingCheck_=false;drainageCursor_=0;drainageCells_.fill(false);}
    const auto next=flow.value("next_flow",std::string{});finishForeground();activeContentFlow_.clear();if(!next.empty()){startContentFlow(next);}
}
void CombatScene::checkAutorun(){
    if(content_.catalog.is_null()||foreground_!=ForegroundKind::None){return;}
    for(const auto& [id,f]:content_.catalog.at("flows").items()){
        if(f.value("trigger",std::string{})=="autorun"&&!blockedFlows_.contains(id)&&isCurrentPage(f.at("object"),f.at("page"))){startContentFlow(id);return;}
    }
}
void CombatScene::updateEnvironment(){
    if(content_.catalog.is_null()){return;}
    for(const auto& [id,f]:content_.catalog.at("flows").items()){
        if(f.value("trigger",std::string{})!="environment"){continue;}
        if(f.value("natural_entry_only",false)&&!applyEntryEffects_){continue;}
        if(!isCurrentPage(f.at("object"),f.at("page"))){if(f.value("reset_on_inactive",false)){environmentFlows_.erase(id);}continue;}
        if(!f.value("runtime_enabled",true)){continue;}
        auto& state=environmentFlows_[id];if(state.wait>0&&--state.wait>0){continue;}
        if(f.value("requires_control",false)&&!session_.flag(1)){state.next=0;continue;}
        const auto& steps=f.at("steps");
        while(state.next<steps.size()){
            const auto& step=steps.at(state.next++);const std::string kind=step.at("kind");
            if(kind=="convert_tool"){if(session_.convertTool(step.at("from"),step.at("to"))){refreshObjectPages();}continue;}
            if(kind=="domain_value"){
                const int variable=guardSourceVariable(step.at("field"));
                session_.setVariable(variable,(step.at("operation")=="add"?session_.variable(variable):0)+step.at("value").get<int>());refreshObjectPages();continue;
            }
            if(kind=="flag"){session_.setFlag(step.at("id"),step.at("value"));refreshObjectPages();continue;}
            if(kind=="combine_skill"){
                const int skill=step.at("skill");
                if(step.at("actor")!=session_.actorId()||(skill!=3&&skill!=4)){throw std::runtime_error("Unsupported environment skill owner or skill: "+id);}
                if(step.at("learn").get<bool>()){session_.learnSkill(step.at("skill"));}else{session_.forgetSkill(step.at("skill"));}continue;
            }
            if(kind=="branch"){
                const auto& condition=step.at("condition");
                if(condition.at("kind")!="fact"){throw std::runtime_error("Unsupported environment condition: "+id);}
                // Reviewed underground music pages choose one immediate BGM cue.
                // Waiting or nested branches need an explicit continuation contract.
                for(const auto* arm:{"then","else"}){
                    const auto& body=step.at(arm);
                    if(body.size()!=1||body.at(0).at("kind")!="cue"||content_.cue(body.at(0).at("cue").get<std::string>()).at("kind")!="bgm"){
                        throw std::runtime_error("Unsupported environment music branch: "+id);
                    }
                }
                const auto& body=step.at(session_.flag(condition.at("id"))==condition.at("value").get<bool>()?"then":"else");
                cue(body.at(0).at("cue"),f.at("object"));continue;
            }
            if(kind=="tile_substitution"){applyTileSubstitution(step);continue;}
            if(kind=="picture_anchor"){samplePictureAnchor(step,f.at("object"));continue;}
            if(kind=="erase_event_once"){eraseEvent(step.value("object",f.at("object").get<std::string>()));break;}
            if(kind=="enable_combine_if_known"){session_.replaceKnownSkill(step.at("from_skill"),step.at("to_skill"));continue;}
            if(kind=="all_dogs_defeated"){
                const auto& flags=step.at("flags");if(std::all_of(flags.begin(),flags.end(),[&](const auto& flag){return session_.flag(flag);})){
                    for(const auto& action:step.at("then")){if(action.at("kind")=="cue"){cue(action.at("cue"),f.at("object"));}else if(action.at("kind")=="erase_event_once"){eraseEvent(action.value("object",f.at("object").get<std::string>()));}else{throw std::runtime_error("Unsupported defeated-dogs operation");}}
                    break;
                }continue;
            }
            if(kind=="match_positions"){
                const auto& positions=step.at("positions");bool matched=true;
                for(const auto& [object,xy]:positions.items()){matched&=findCharacter(*world_,object).motion.tile()==TilePosition{xy[0],xy[1]};}
                if(matched){session_.setFlag(step.at("flag"),true);refreshObjectPages();}continue;
            }
            if(kind=="place_object"){
                auto actor=findCharacter(*world_,step.at("object"));const auto tile=step.contains("tile_fields")?TilePosition{session_.variable(guardSourceVariable(step.at("tile_fields")[0])),session_.variable(guardSourceVariable(step.at("tile_fields")[1]))}:TilePosition{step.at("tile")[0],step.at("tile")[1]};actor.motion=CharacterMotion(tile,actor.motion.facing(),actor.motion.speed());replaceCharacter(*world_,actor);continue;
            }
            if(kind=="restore_dead_enemies"){
                for(const auto& placement:step.at("placements")){if(session_.flag(placement.at("if_flag"))){
                    // Source off-map corpse parking removes this scene object; world bounds remain strict.
                    if(placement.value("hidden",false)){eraseEvent(placement.at("object"));continue;}
                    auto actor=findCharacter(*world_,placement.at("object"));const auto& xy=placement.at("tile");
                    actor.motion=CharacterMotion({xy[0],xy[1]},actor.motion.facing(),actor.motion.speed());replaceCharacter(*world_,actor);
                }}continue;
            }
            if(kind!="cue"){throw std::runtime_error("Unsupported environment operation: "+id+" / "+kind);}
            const auto cueId=step.at("cue").get<std::string>();
            const auto& value=content_.cue(cueId);
            if(value.at("kind")=="wait_routes"){if(routesPending()){--state.next;break;}continue;}
            if(value.at("kind")=="wait"){state.wait=std::max(1,value.at("tenths").get<int>()*6);break;}
            const int foregroundWait=wait_;wait_=0;
            cue(cueId,f.at("object"));state.wait=wait_;wait_=foregroundWait;
            if(value.at("kind")=="animation"&&value.value("wait",false)){
                const auto& animation=content_.media.at("animations").at(value.at("animation").get<std::string>());
                state.wait=static_cast<int>(animation.at("frames").size())*animation.at("playback").at("logic_updates_per_frame").get<int>();
            }
            if(state.wait>0){break;}
        }
        if(state.next==steps.size()){state.next=0;}
    }
}
void CombatScene::samplePictureAnchor(const nlohmann::json& step,const std::string& self){
    const auto name=step.at("anchor").get<std::string>(),actorId=step.at("actor")=="self"?self:step.at("actor").get<std::string>();
    const auto sample=sampleActorScreen(actorId);auto& anchor=pictureAnchors_[name];
    for(const auto& axis:step.at("sample")){if(axis=="x"){anchor.x=sample.x;}else{anchor.y=sample.y;}}
    if(step.contains("y")){anchor.y=step.at("y").get<int>();}
    anchor.x+=step.at("offset").at(0).get<int>();anchor.y+=step.at("offset").at(1).get<int>();
}
GridPosition CombatScene::sampleActorScreen(const std::string& id) const{
    const auto& actor=findCharacter(*world_,id);const auto position=actor.motion.position(),center=camera_.center();
    return {(position.x+.5)*definition_.tileSize-center.x+160,(position.y+1)*definition_.tileSize-center.y+120-actor.motion.jumpHeight()};
}
void CombatScene::updateUndergroundLighting(){
    if(!content_.catalog.contains("underground_lighting")){return;}
    for(auto& [object,state]:lightStates_){if(!isCurrentPage(object,state.page)){state.pending.reset();}}
    for(const auto& [id,light]:content_.catalog.at("underground_lighting").items()){
        if(!isCurrentPage(light.at("object"),light.at("page"))){continue;}
        const auto actorId=light.at("actor").get<std::string>();const auto& actor=findCharacter(*world_,actorId);
        auto& state=lightStates_[light.at("object").get<std::string>()];
        state.page=light.at("page");
        if(!state.pending){
            state.facing=actor.motion.facing();const auto& offset=light.at("directions").at(directionName(state.facing)).at("offset_pixels");
            auto position=sampleActorScreen(actorId);position.x+=offset.at(0).get<int>();position.y+=offset.at(1).get<int>();state.pending=position;
        }
        // Source picture commands resume with coordinates sampled before the message wait.
        if(dialogue_.view().phase!=DialoguePhase::Hidden||inputPrompt_.view().phase!=DialoguePhase::Hidden){continue;}
        const auto position=*state.pending;const auto& data=light.at("directions").at(directionName(state.facing));
        const auto emit=[&](const nlohmann::json& cues){for(const auto& cue:cues){presentation_.push_back({PresentationEvent::Kind::Cue,cue,{},actor.motion.position(),0,actorId,position});}};
        if(state.shown!=state.facing){emit(data.at("show_cues"));state.shown=state.facing;}
        emit(light.at("move_cues"));state.pending.reset();
    }
}
bool CombatScene::triggerExploration(const CharacterState& actor,bool pushing){
    if(content_.catalog.is_null()||!actor.active||world_->player().motion.moving()){return false;}
    const auto& player=world_->player();const auto tile=player.motion.tile();const auto front=adjacentTile(tile,player.motion.facing());
    const auto adjacent=actor.motion.tile()==front;
    for(const auto& ladder:content_.catalog.at("ladders")){
        const auto id=ladder.at("object").get<std::string>();
        if(content_.objects.at(id).map!=map_){continue;}
        if(actor.id==id&&isCurrentPage(id,ladder.at("page"))&&adjacent&&!weapon_->raised()){
            activeLadder_=ladder;ladderDirection_=player.motion.facing();ladderAction_=pushing?LadderAction::Push:LadderAction::Climb;
            if(pushing&&ladder.contains("push_completed_flag")&&session_.flag(ladder.at("push_completed_flag"))){activeLadder_.reset();return true;}
            weapon_->cancel(ActionCancel::Foreground);startForeground(ForegroundKind::Ladder,id);updateForeground();return true;
        }
        if(!pushing){
            if(ladder.contains("priority_flow")){
                const auto key=ladder.at("priority_flow").get<std::string>();const auto& f=content_.catalog.at("flows").at(key);
                const auto& target=findCharacter(*world_,f.at("object").get<std::string>());
                if(target.active&&target.motion.tile()==front&&isCurrentPage(target.id,f.at("page"))&&contentFlowAllowed(f)){startContentFlow(key);return true;}
            }
            for(const auto& [direction,h]:ladder.at("helpers").items()){
            if(actor.id==h.at("object").get<std::string>()&&isCurrentPage(actor.id,h.at("page"))&&adjacent){
                // A blocked helper must not consume the source's co-located doorway.
                if(!ladderDestinationAllowed(id,directionFromName(direction))){
                    for(const auto& target:world_->objects()){
                        if(target.id!=actor.id&&target.motion.tile()==actor.motion.tile()&&triggerTransfer(target,true)){return true;}
                    }
                }
                activeLadder_=ladder;ladderDirection_=directionFromName(direction);ladderAction_=LadderAction::Exit;
                weapon_->cancel(ActionCancel::Foreground);startForeground(ForegroundKind::Ladder,id);updateForeground();return true;
            }
        }}
    }
    if(pushing||actor.trigger>2||actor.motion.tile()!=(actor.layer==1?front:tile)){return false;}
    if(requestCompletion(actor.id)){return true;}
    for(const auto& point:content_.catalog.at("save_points")){
        if(point.contains("required_flags")&&!std::all_of(point.at("required_flags").begin(),point.at("required_flags").end(),[&](const auto& id){return session_.flag(id);})){continue;}
        if(point.contains("absent_flags")&&std::any_of(point.at("absent_flags").begin(),point.at("absent_flags").end(),[&](const auto& id){return session_.flag(id);})){continue;}
        if(point.contains("allowed_party")&&!std::any_of(point.at("allowed_party").begin(),point.at("allowed_party").end(),[&](const auto& id){return session_.inParty(id);})) {continue;}
        if(actor.id==point.at("object").get<std::string>()&&isCurrentPage(actor.id,point.at("page"))&&(point.value("geometry",std::string{})=="same_tile"?actor.motion.tile()==tile:directionName(player.motion.facing())==point.at("facing").get<std::string>())){
            const auto kind=point.value("kind",std::string{"ordinary"});
            SaveRequest request{generation_,actor.id,point.at("location"),kind,++nextSaveRequest_};if(kind=="direct_unscored"){directSave_=request;}saveRequest_=request;
            return true;
        }
    }
    if(content_.catalog.contains("warehouse_direct")){const auto& hidden=content_.catalog.at("warehouse_direct");if(actor.id==hidden.at("supply_object").get<std::string>()&&actor.motion.tile()==tile&&isCurrentPage(actor.id,1)){session_.grant(hidden.at("supply_item"),hidden.at("supply_amount"));return true;}}
    for(const auto& b:content_.catalog.at("boundaries")){
        if(actor.id==b.at("object").get<std::string>()&&isCurrentPage(actor.id,b.at("page"))){presentation_.push_back({PresentationEvent::Kind::Notice,b.at("label"),{},{},240});return true;}
    }
    for(const auto& [id,f]:content_.catalog.at("flows").items()){
        const auto trigger=f.value("trigger",std::string{});
        if((trigger=="action"||(trigger=="contact"&&!f.value("decision_excluded",false)))&&actor.id==f.at("object").get<std::string>()&&isCurrentPage(actor.id,f.at("page"))&&contentFlowAllowed(f)){
            if(f.contains("owns_item")){const int item=f.at("owns_item");bool owns=session_.count(item)>0;for(int slot=1;slot<=5;++slot){owns|=session_.equipped(slot)==item;}if(owns!=f.at("owns").get<bool>()){continue;}}
            if(!content_.expansion.meeting.is_null()&&actor.id==content_.expansion.meeting.at("meeting_trigger").get<std::string>()&&isCurrentPage(actor.id,content_.expansion.meeting.at("meeting_page"))){
                const auto& flags=content_.expansion.meeting.at("talk_flags");const bool ready=std::all_of(flags.begin(),flags.end(),[&](const auto& flag){return session_.flag(flag);});
                if(ready&&!content_.expansion.meetingStory.is_null()){startMeeting();return true;}
                if(ready){presentation_.push_back({PresentationEvent::Kind::Notice,"会合条件已满足；正式会议演出由任务 62 接入。",{},{},240});return true;}
            }
            if(f.value("instant_contact_sequence",false)){return triggerEncounter(actor,true);}
            startContentFlow(id);return true;
        }
    }
    return false;
}
bool CombatScene::ladderDestinationAllowed(const std::string& id,Direction direction) const{
    const auto target=adjacentTile(findCharacter(*world_,id).motion.tile(),direction);
    return target.x>=0&&target.y>=0&&target.x<definition_.width&&target.y<definition_.height&&world_->mappedLowerTerrain(target).value_or(content_.terrainIds.at(map_).at(target.y*definition_.width+target.x))!=3;
}
void CombatScene::updateLadder(){
    const auto& l=*activeLadder_;const auto id=l.at("object").get<std::string>();const std::string direction(directionName(ladderDirection_));
    if(phase_==0){
        session_.setFlag(1,false);const auto ladder=findCharacter(*world_,id);
        if(ladderAction_==LadderAction::Push||ladderAction_==LadderAction::Exit){
            if(ladderAction_==LadderAction::Exit){session_.setFlag(18,false);}
            const bool permitted=(ladderAction_==LadderAction::Push&&!l.value("push_terrain_gate",true))||ladderDestinationAllowed(id,ladderDirection_);
            if(!permitted){phase_=3;}
            else if(ladderAction_==LadderAction::Push){
                session_.setFlag(l.at("busy_flag"),true);ladderPushOrigin_=world_->player().motion.tile();ladderPushWait_=0;
                const auto& push=l.at("push").at(direction);startRoute(push.at("object_route"),id);startRoute(push.at("player_route"),id);
                if(l.contains("push_sound_wait")){ladderPushWait_=content_.cue(l.at("push_sound_wait").get<std::string>()).at("tenths").get<int>()*6;phase_=4;}else{phase_=2;}return;
            }else{session_.setFlag(l.at("mounted_flag"),false);refreshObjectPages();startRoute(l.at("helpers").at(direction).at("exit_route"),id);phase_=2;return;}
        }else{
            session_.setFlag(l.at("busy_flag"),true);session_.setFlag(l.at("mounted_flag"),true);refreshObjectPages();
            startRoute(l.at("climb").at(direction),id);phase_=1;return;
        }
    }
    if(phase_==4){
        if(ladderPushWait_>0&&--ladderPushWait_>0){return;}
        if(world_->player().motion.tile()!=ladderPushOrigin_){cue(l.at("push_sound"),id);}phase_=2;
    }
    if(routesPending()){return;}
    if(phase_==1){
        for(const auto& [direction,h]:l.at("helpers").items()){
            const auto helperId=h.at("object").get<std::string>();auto helper=findCharacter(*world_,helperId);
            helper.motion=CharacterMotion(world_->player().motion.tile(),helper.motion.facing(),helper.motion.speed());replaceCharacter(*world_,helper);
            startRoute(h.at("place_route"),id);
        }
        phase_=2;return;
    }
    session_.setFlag(l.at("busy_flag"),false);session_.setFlag(1,true);
    if(ladderAction_==LadderAction::Exit){session_.setFlag(18,true);}
    activeLadder_.reset();finishForeground();
}

bool CombatScene::triggerEncounter(const CharacterState& actor,bool touching){
    if(!actor.active||foreground_!=ForegroundKind::None||world_->player().motion.moving()||content_.catalog.is_null()){return false;}
    const auto tile=world_->player().motion.tile();
    const bool contact=actor.layer==1?(touching&&actor.motion.tile()==adjacentTile(tile,world_->player().motion.facing())):actor.motion.tile()==tile;
    if(!contact){return false;}
    for(const auto& [id,flow]:content_.catalog.at("flows").items()){
        if(flow.value("trigger",std::string{})=="contact"&&flow.at("object")==actor.id&&(!flow.contains("requires_fact_off")||!session_.flag(flow.at("requires_fact_off")))&&isCurrentPage(actor.id,flow.at("page"))&&contentFlowAllowed(flow)){
            if(flow.value("instant_contact_sequence",false)){
                const auto& steps=flow.at("steps");cue(steps[0].at("cue"),actor.id);
                auto target=findCharacter(*world_,steps[1].at("object"));const auto& xy=steps[1].at("tile");
                target.motion=CharacterMotion({xy[0],xy[1]},target.motion.facing(),target.motion.speed());replaceCharacter(*world_,target);
                session_.setFlag(steps[2].at("id"),true);refreshObjectPages();
            }else if(flow.value("instant_contact_fact",false)){
                const auto& step=flow.at("steps").at(0);session_.setFlag(step.at("id"),step.at("value"));refreshObjectPages();
            }else{startContentFlow(id);}return true;
        }
    }
    return false;
}

void CombatScene::cancelPlayerActions(const std::string& cleanupName,const std::string& owner){
    if(cleanupName=="ce5"){session_.clearAimingAid(true);}
    weapon_->cancel(ActionCancel::Foreground);
    if(!cleanupName.empty()){
        if(content_.full->empty()){
            if(cleanupName!="ce5"){throw std::runtime_error("Unsupported legacy player cleanup");}
            return;
        }
        const auto& cleanup=content_.full->at("runtime").at("player_action_cleanup").at(cleanupName);
        if(cleanup.at("actor")!=1){throw std::runtime_error("Unsupported player action cleanup actor");}
        // CN1.00 CE5 C1; the reviewed first profile predates the explicit variable list.
        for(const auto& id:cleanup.value("clear_action_variables",nlohmann::json::array({10,11,12}))){session_.setVariable(id.get<int>(),0);}
        for(const auto& id:cleanup.at("clear_action_facts")){session_.setFlag(id.get<int>(),false);}
        playerVisible_=cleanup.at("show_player");
        const auto& branch=cleanup.at(session_.flag(cleanup.at("injured_flag"))?"injured":"normal");
        if(branch.contains("clear_weapon_variable")){session_.setVariable(branch.at("clear_weapon_variable"),0);}
        ScenePresentation(*this).cue(branch.at("graphic_cue"),owner);
        ScenePresentation(*this).cue(branch.at("route_cue"),owner);
    }
}

void CombatScene::updateStoryPresentation(){
    if(!content_.catalog.contains("aviation")){return;}
    const auto& data=content_.catalog.at("aviation");
    const auto actorId=data.at("actor").get<std::string>();
    if(content_.objects.at(actorId).map!=map_||session_.flag(data.at("stop_fact"))||session_.hp()<=0){storyPresentation_={};return;}
    auto& state=storyPresentation_;
    const auto& actor=findCharacter(*world_,actorId);
    const auto* body=content_.activeVariant(content_.objects.at(data.at("body_object").get<std::string>()),session_);
    const auto emit=[&](const nlohmann::json& frames){
        for(const auto& frame:frames){
            if(frame.contains("literal")){cue(frame.at("cue"),actorId);continue;}
            auto screen=sampleActorScreen(actorId);
            screen.x+=frame.at("offset").at(0).get<int>();screen.y+=frame.at("offset").at(1).get<int>();
            if(frame.value("height",false)){screen.y-=state.height;}
            const auto center=camera_.center();
            const GridPosition origin{center.x-160,center.y-120};
            const GridPosition position{(screen.x+origin.x)/definition_.tileSize-.5,(screen.y+origin.y)/definition_.tileSize-1};
            PresentationEvent event{PresentationEvent::Kind::Cue,frame.at("cue"),{},position,0,actorId,screen};
            event.pictureMapOrigin=origin;presentation_.push_back(std::move(event));
        }
    };
    // Source E3 precedes E13. Its picture commands wait behind a message;
    // the parallel action's timers and finite routes retain their own continuation.
    if(body&&data.at("body").contains(std::to_string(body->page))&&dialogue_.view().phase==DialoguePhase::Hidden&&inputPrompt_.view().phase==DialoguePhase::Hidden){
        int pose=session_.variable(guardSourceVariable(data.at("pose_field")));
        if(body->page==1){
            if(actor.motion.tile()!=state.lastTile){state.walkPulse+=32;state.lastTile=actor.motion.tile();}
            state.walkCounter=std::min(76,state.walkCounter+1);
            pose=state.walkPulse>0?state.walkCounter:0;
            if(state.walkPulse>0&&state.walkCounter>=76){state.walkCounter=0;}
            state.walkPulse=std::max(0,state.walkPulse-1);
        }
        if(body->page==1||state.bodyPage!=body->page){session_.setVariable(201,actor.motion.facing()==Direction::Up?8:actor.motion.facing()==Direction::Right?6:actor.motion.facing()==Direction::Down?2:4);}
        const auto& direction=data.at("body").at(std::to_string(body->page)).at(directionName(actor.motion.facing()));
        const auto& frames=direction.at(std::to_string(pose));
        if(!state.shown||state.bodyPage!=body->page||state.direction!=actor.motion.facing()){
            emit(frames.at("show"));state.shown=true;state.bodyPage=body->page;state.direction=actor.motion.facing();
        }
        emit(frames.at("frame"));
    }
    if(!session_.flag(data.at("active_fact"))){state.phase=0;state.cursor=0;state.wait=0;state.completed=false;state.height=0;return;}
    const int phase=session_.variable(guardSourceVariable(data.at("stage_field")));
    if(!data.at("actions").contains(std::to_string(phase))){return;}
    if(state.phase!=phase){state.phase=phase;state.cursor=0;state.wait=0;state.completed=false;state.height=0;}
    if(state.completed||(state.wait>0&&--state.wait>0)){return;}
    const auto& action=data.at("actions").at(std::to_string(phase));const auto& steps=action.at("steps");
    while(state.cursor<steps.size()){
        const auto& step=steps.at(state.cursor++);const std::string kind=step.at("kind");
        if(kind=="pose"){session_.setVariable(guardSourceVariable(data.at("pose_field")),step.at("value"));}
        else if(kind=="height"){state.height=step.at("value");}
        else if(kind=="wait"){state.wait=std::max(1,step.at("tenths").get<int>()*6);return;}
        else if(kind=="cue"){cue(step.at("cue"),actorId);}
        else{throw std::runtime_error("Unsupported finite story action");}
    }
    if(action.at("repeat").get<bool>()){state.cursor=0;return;}
    state.completed=true;state.height=0;session_.setVariable(202,0);session_.setVariable(205,0);
    session_.setVariable(guardSourceVariable(data.at("pose_field")),0);session_.setFlag(data.at("active_fact"),false);refreshObjectPages();
}

void CombatScene::applyTileSubstitution(const nlohmann::json& step){
    if(step.at("layer")=="upper"){world_->substituteUpperTile(step.at("from_index"),step.at("to_index"));}
    else if(step.at("layer")=="lower"){world_->substituteLowerTile(step.at("from_index"),step.at("to_index"));}
    else{throw std::runtime_error("Invalid tile substitution layer");}
}
