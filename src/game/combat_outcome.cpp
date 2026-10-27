#include "combat_scene.h"
#include "final_boss_controller.h"
#include "dora_controller.h"
#include "cash_controller.h"
#include "warehouse_g4_controller.h"
#include "full_content.h"
#include "scene_presentation.h"
#include "weapon_controller.h"
#include "meeting_controller.h"
#include "enemy_controller.h"
#include <algorithm>
#include <set>
void CombatScene::applyPlayerDamage(int damage){
    session_.loseHp(damage);const int wounds=session_.variable(41)+1;session_.setVariable(41,wounds);
    if(wounds>=content_.rules.at("player_damage").at("bleeding_check_hits").get<int>()){
        if(random_(0,10)<=content_.rules.at("player_damage").at("bleeding_roll_max").get<int>()){session_.startBleeding(content_.rules.at("bleeding").at("clear_after_wait_tenths").get<int>()*6);}
        session_.setVariable(41,0);
    }
    if(session_.hp()>0&&!session_.flag(10)&&!session_.flag(574)){playSound("ce6-c3");}
}
void CombatScene::startDeath(bool physical){
    if(finalBoss_){finalBoss_->retire();session_.setFlag(586,false);finalBossVictory_.reset();}
    if(foreground_==ForegroundKind::Death){return;}
    if(warehouseG4_){warehouseG4_->retire();}
    if(cash_){cash_->retire();}
    if(dora_){dora_->retire();}
    continueDeclined_=false;
    meeting_.reset();cancelToolUse();cancelMixing();inputPrompt_.invalidate(generation_);inputRequest_.reset();grabQueue_.clear();
    messageRequest_.reset();dialogue_.invalidate();saveRequest_.reset();activeContentFlow_.clear();activeLadder_.reset();
    camera_.reset(world_->player().motion.position(),definition_.width,definition_.height);playerVisible_=true;
    weapon_->cancel(ActionCancel::Death);routes_.clear();session_.setFlag(15,true);session_.setFlag(13,false);
    for(int i=1;i<=4;++i){session_.setFlag(i,false);}
    startForeground(ForegroundKind::Death);if(!physical){phase_=1;}updateForeground();
}
void CombatScene::startFailure(const std::string& flow,const std::string& actor){
    (void)content_.expansion.failureFlows.at(flow);
    // Parallel enemies may repeat the request while the committed CE38 outcome advances.
    if(foreground_==ForegroundKind::Failure||foreground_==ForegroundKind::Death){return;}
    meeting_.reset();cancelToolUse();cancelMixing();
    inputPrompt_.invalidate(generation_);inputRequest_.reset();messageRequest_.reset();dialogue_.invalidate();
    saveRequest_.reset();activeContentFlow_.clear();activeFlowSteps_.clear();activeLadder_.reset();
    weapon_->cancel(ActionCancel::Death);routes_.clear();ambientRoutes_.clear();grabQueue_.clear();
    for(int flag:{1,2,3,4,18}){session_.setFlag(flag,false);}
    failureFlow_=flow;failureActor_=actor;failureBody_.clear();startForeground(ForegroundKind::Failure);
}
void CombatScene::enemyEnded(int variant){
    if(variant==3&&session_.flag(55)){
        cue("m4-e4-p22-c24");const auto tile=findCharacter(*world_,content_.rules.at("enemy").at("id")).motion.tile();
        session_.setVariable(396,tile.x);session_.setVariable(397,tile.y);session_.setVariable(398,3);session_.setFlag(101,true);return;
    }
    weapon_->cancel(ActionCancel::Foreground);routes_.clear();corpseVariant_=variant;
    session_.setFlag(1,false);session_.setFlag(18,false);startForeground(ForegroundKind::EnemyEnd);updateForeground();
}
bool CombatScene::requestCompletion(const std::string& object){
    if(!content_.catalog.contains("completion_boundary")){return false;}
    const auto& end=content_.catalog.at("completion_boundary");
    if(!end.at("runtime_enabled").get<bool>()||object!=end.at("object").get<std::string>()||!isCurrentPage(object,end.at("page"))||suppressedExit_==object){return false;}
    cancelToolUse();cancelMixing();weapon_->cancel(ActionCancel::Transition);routes_.clear();
    inputPrompt_.invalidate(generation_);inputRequest_.reset();messageRequest_.reset();dialogue_.invalidate();saveRequest_.reset();
    pendingTransition_.clear();suppressedExit_=object;
    // A completed request never prepares a destination Scene or uses a landing tile.
    exit_=ExitRequest{end.at("destination_map"),{},true};requestedMode_=ApplicationMode::Boundary;return true;
}
void CombatScene::transfer(const std::string& id){
    const auto& t=content_.transitions.at(id);
    if(t.contains("condition")&&t.at("condition").contains("switch")&&!session_.flag(t.at("condition").at("switch"))){return;}
    if(content_.catalog.contains("completion_boundary")){
        const auto& end=content_.catalog.at("completion_boundary");
        if(end.at("runtime_enabled").get<bool>()&&t.at("object")==end.at("object")&&t.at("page")==end.at("page")&&t.at("destination").at("map")==end.at("destination_map")){
            if(requestCompletion(t.at("object"))){pendingTransition_=id;}return;
        }
    }
    if(content_.catalog.contains("flows")&&content_.catalog.at("flows").contains(id)){
        const auto& flow=content_.catalog.at("flows").at(id);
        if(flow.value("runtime_enabled",true)&&flow.at("object")==t.at("object")&&flow.at("page")==t.at("page")){
            // The reviewed finite flow owns branches and post-transfer dialogue too.
            startContentFlow(id);return;
        }
    }
    const std::set<std::string> playableCues{"forced_route","wait_routes","wait","sound","actor_graphic","cancel_routes","bgm","fade_bgm","face","message_options","erase_screen","show_screen","show_picture","move_picture","erase_picture","animation","flash_character","camera","tint","flash_screen","player_visibility","transition_setting","weather","shake"};
    const bool unsupported=!t.contains("flow")||std::any_of(t.at("flow").begin(),t.at("flow").end(),[&](const auto& step){
        const bool missingLicker=step.at("kind")=="dormant_licker_entry"&&session_.flag(step.at("requires_flag"))&&std::none_of(content_.expansion.enemies.begin(),content_.expansion.enemies.end(),[&](const auto& item){const auto& enemy=item.second;return enemy.map==t.at("destination").at("map")&&enemy.sourceSlot==step.at("source_slot")&&enemy.behavior==EnemyBehaviorKind::Licker&&enemy.rules.contains("entry_state_variable");});
        return missingLicker||(step.at("kind")=="cue"&&!playableCues.contains(content_.cue(step.at("cue").template get<std::string>()).at("kind").template get<std::string>()));
    });
    const bool boundary=t.value("handoff_only",false)||!t.value("runtime_enabled",true)||unsupported;
    if(!boundary){
        try{
            auto candidateSession=session_;candidateSession.resetRoomFacts();
            const auto& dest=t.at("destination");const auto& xy=dest.at("tile");
            const auto facing=t.value("facing",std::string("preserve"));
            pendingWorld_=prepareWorld(dest.at("map"),{xy.at(0),xy.at(1)},facing=="preserve"?world_->player().motion.facing():directionFromName(facing),candidateSession);
        }catch(const std::exception& e){
            transferError_=e.what();suppressedExit_=t.at("object").get<std::string>();
            presentation_.push_back({PresentationEvent::Kind::Notice,"换图准备失败，保留当前场景",{},{},180});return;
        }
    }
    transferError_.clear();pendingTransition_=id;
    if(boundary){
        const auto& dest=t.at("destination");exit_=ExitRequest{dest.at("map"),{dest.at("tile").at(0),dest.at("tile").at(1)}};requestedMode_=ApplicationMode::Boundary;return;
    }
    weapon_->cancel(ActionCancel::Transition);routes_.clear();startForeground(ForegroundKind::Transfer);
}
bool CombatScene::triggerTransfer(const CharacterState& object,bool action){
    if(!object.active||suppressedExit_==object.id){return false;}
    const auto& p=world_->player();
    if(action){if(object.trigger!=0||object.motion.tile()!=(object.layer==1?adjacentTile(p.motion.tile(),p.motion.facing()):p.motion.tile())){return false;}}
    else if((object.trigger!=1&&object.trigger!=2)||object.motion.tile()!=p.motion.tile()){return false;}
    const auto* page=content_.activeVariant(content_.objects.at(object.id),session_);
    if(!page){return false;}
    for(const auto& [id,t]:content_.transitions.items()){
        if(t.at("object")==object.id&&t.at("page")==page->page){transfer(id);return true;}
    }
    return false;
}
void CombatScene::checkTouchTransitions(){
    const auto tile=world_->player().motion.tile();
    if(suppressedExit_){
        const auto it=std::find_if(world_->objects().begin(),world_->objects().end(),[&](const auto& o){return o.id==*suppressedExit_;});
        if(it==world_->objects().end()||(it->trigger==0&&it->layer==1?std::abs(it->motion.tile().x-tile.x)+std::abs(it->motion.tile().y-tile.y)>1:it->motion.tile()!=tile)){suppressedExit_.reset();}
    }
    for(const auto& o:world_->objects()){
        if(!o.active||o.motion.tile()!=tile){continue;}
        if(triggerEncounter(o)){return;}
        if(triggerTransfer(o,false)){return;}
        const auto& refused=content_.interactions.at("refusal").at("objects");
        if(std::find(refused.begin(),refused.end(),o.id)!=refused.end()&&!session_.flag(content_.interactions.at("refusal").at("condition").at("switch_off"))){startForeground(ForegroundKind::Refusal,o.id);return;}
    }
}
void CombatScene::updateOutcome(){
    const auto enemyId=content_.rules.at("enemy").at("id").get<std::string>();
    if(foreground_==ForegroundKind::EnemyEnd){
        const std::string prefix=corpseVariant_==1?"m4-e4-p18":corpseVariant_==2?"m4-e4-p20":"m4-e4-p22";
        switch(phase_){
        case 0:
            if(corpseVariant_==2){
                presentation_.push_back({PresentationEvent::Kind::Animation,"1",{},findCharacter(*world_,enemyId).motion.position(),0});
                const auto& animation=content_.media.at("animations").at("1");
                wait_=static_cast<int>(animation.at("frames").size())*animation.at("playback").at("logic_updates_per_frame").get<int>();
            }
            ++phase_;break;
        case 1:if(corpseVariant_==2){cue("m4-e4-p19-c4");}++phase_;break;
        case 2:
            if(corpseVariant_==2){startRoute("m4-e4-p19-c5",enemyId);}else if(corpseVariant_==1){playSound("m4-e4-p18-c3");}
            cue(prefix+"-c2");session_.setFlag(19,true);wait_=60;++phase_;break;
        case 3:{
            auto e=findCharacter(*world_,enemyId);e.layer=0;e.through=true;replaceCharacter(*world_,e);
            cue(prefix+(corpseVariant_==1?"-c9":"-c8"));requestMessage(prefix+(corpseVariant_==1?"-c10":"-c9"));++phase_;break;
        }
        case 4:requestMessage(prefix+(corpseVariant_==1?"-c13":"-c12"));++phase_;break;
        case 5:{
            const auto tile=findCharacter(*world_,enemyId).motion.tile();
            session_.setVariable(396,tile.x);session_.setVariable(397,tile.y);session_.setVariable(398,corpseVariant_);
            session_.setFlag(101,true);session_.setFlag(19,false);session_.setFlag(1,true);session_.setFlag(18,true);
            presentation_.push_back({PresentationEvent::Kind::StopMusic,{},{},{},0});
            const auto player=world_->player();rebuild(map_,player.motion.tile(),player.motion.facing());finishForeground();break;
        }
        }return;
    }
    if(foreground_==ForegroundKind::Failure){
        const auto& flow=content_.expansion.failureFlows.at(failureFlow_).at("steps");
        while(phase_<static_cast<int>(flow.size())){
            const auto& step=flow[phase_];const std::string kind=step.at("kind");
            if(kind=="cue"){
                const std::string id=step.at("cue");const auto& media=content_.cue(id);
                if(media.at("kind")=="wait_routes"&&routesPending()){return;}
                ++phase_;
                if(media.at("kind")=="forced_route"&&content_.routes.at(id).target==1){
                    if(failureBody_.empty()){throw std::logic_error("Failure body not prepared");}
                    std::erase_if(routes_,[&](const auto& r){return r.actor()==failureBody_;});
                    routes_.emplace_back(content_.routes.at(id),failureBody_,id);
                }else if(media.at("kind")!="wait_routes"){cue(id,failureActor_);}
            }else if(kind=="place_attack_actor_at_player"){
                auto actor=world_->player();actor.id="transient-death-"+std::to_string(generation_);actor.eventId=0;actor.layer=1;actor.through=true;actor.interaction={};
                failureBody_=actor.id;world_->addTransientActor(actor);++phase_;
            }else if(kind=="return_title_via_death"){startDeath(false);return;}
            else{throw std::logic_error("Unsupported failure step");}
            if(wait_>0){return;}
        }
        return;
    }
    if(foreground_==ForegroundKind::Death){
        static const std::vector<std::string> end{"ce38-c2","ce38-c3","ce38-c4","ce38-c5","ce38-c6","ce38-c7","ce38-c8","ce38-c9","ce38-c10","ce38-c32","ce38-c33","ce38-c34","ce38-c35"};
        if(phase_==0){
            cue("ce23-c1");cue("ce23-c4");
            if(session_.variable(31)==5&&content_.fatalDeath.has_value()){
                const auto& fatal=content_.fatalDeath.value();
                if(session_.variable(13)!=fatal.at("actor").get<int>()){throw std::runtime_error("Unsupported source fatal actor");}
                for(const auto& id:fatal.at("pose5")){cue(id.get<std::string>());}phase_=100;return;
            }
            cue("ce23-c18");startRoute("ce23-c19");presentation_.push_back({PresentationEvent::Kind::Effect,"blood-large",{},world_->player().motion.position(),0});cue("ce23-c36");++phase_;return;
        }
        if(phase_==100){ScenePresentation(*this).executeCue("ce23-c8");++phase_;return;}
        if(phase_==101){cue("ce23-c9");++phase_;return;}
        if(phase_==102){
            cue("ce23-c10");session_.setVariable(31,6);
            for(const auto& id:content_.fatalDeath.value().at("pose6")){cue(id.get<std::string>());}
            cue("ce23-c36");phase_=1;return;
        }
        if(phase_==10&&!continueDeclined_&&content_.catalog.contains("continue")&&session_.flag(586)){
            const auto& checkpoint=content_.catalog.at("continue");cue(checkpoint.at("options_cue"));presentation_.push_back({PresentationEvent::Kind::SystemGraphic,checkpoint.at("system_image"),{},{},0});requestMessage(checkpoint.at("message"));phase_=209;return;
        }
        if(phase_==209){
            if(messageRequest_){return;}
            const auto& source=content_.cue(content_.catalog.at("continue").at("choice_cue").get<std::string>());
            ChoicePrompt choice;choice.display=messageState_.display;choice.question=std::exchange(flowQuestion_,{});for(const auto& label:source.at("choices")){choice.options.push_back({label.get<std::string>(),{{MessageTokenKind::Text,decodeUiText(label.get<std::string>()),0}}});}choice.cancelType=source.at("cancel_type");
            requestInput(choice);phase_=210;return;
        }
        if(phase_==210){
            if(!inputRequest_){return;}const auto result=takeInputResult(*inputRequest_);if(!result){return;}
            const auto* choice=std::get_if<ChoiceSelected>(&result->result);
            if(choice&&choice->index==0){
                try{
                    const auto& checkpoint=continueCheckpoint(content_,session_.variable(283));const auto& tile=checkpoint.at("tile");
                    const auto& flow=content_.catalog.at("flows").at(checkpoint.at("flow").get<std::string>());
                    if(!flow.value("runtime_enabled",true)||content_.objects.at(flow.at("object").get<std::string>()).map!=checkpoint.at("map")){throw std::runtime_error("Unbound Continue entry flow");}
                    ScenePresentation(*this).validateSequence(flow.at("steps"),flow.at("object"));
                    auto candidate=session_.prepareContinueRestore(content_.magazineSeedBindings,checkpoint.at("map"),{tile.at(0),tile.at(1)});
                    for(const auto& id:checkpoint.at("restore_clear_flags")){candidate.setFlag(id,false);}
                    auto room=prepareWorld(checkpoint.at("map"),{tile.at(0),tile.at(1)},Direction::Up,candidate);
                    const auto& retry=checkpoint.at("retry");const auto& landing=retry.at("tile");
                    (void)prepareWorld(retry.at("map"),{landing.at(0),landing.at(1)},Direction::Up,candidate);
                    session_.commitContinueRestore(candidate);commitWorld(std::move(room));
                    // CN1.00 CE38 C18 restores System6 only after a successful Yes candidate.
                    presentation_.push_back({PresentationEvent::Kind::SystemGraphic,content_.media.at("ui").at("menu").at("system_image"),{},{},0});
                    finishForeground();startContentFlow(checkpoint.at("flow"));return;
                }catch(const std::exception& error){transferError_=error.what();presentation_.push_back({PresentationEvent::Kind::Notice,"Continue准备失败，保留当前局与备份",{},{},180});}
            }
            continueDeclined_=true;phase_=10;
        }
        if(phase_<=static_cast<int>(end.size())){cue(end.at(phase_-1));++phase_;return;}
        routes_.clear();presentation_.push_back({PresentationEvent::Kind::StopMusic,{},{},{},0});requestedMode_=ApplicationMode::Title;finishForeground();return;
    }
    if(foreground_==ForegroundKind::Refusal){
        if(phase_==0){cue(interactionObject_+"-p2-c3");requestMessage(interactionObject_+"-p2-c4");++phase_;return;}
        if(phase_==1){startRoute(interactionObject_+"-p2-c6");++phase_;return;}
        if(!routesPending()){finishForeground();}return;
    }
    if(foreground_==ForegroundKind::Transfer){
        const auto& flow=content_.transitions.at(pendingTransition_).at("flow");
        while(phase_<static_cast<int>(flow.size())){
            const auto& step=flow.at(phase_++);const std::string kind=step.at("kind");
            if(kind=="flag"){session_.setFlag(step.at("id"),step.at("value"));}
            else if(kind=="flags"){for(const auto& id:step.at("ids")){session_.setFlag(id,step.at("value"));}}
            else if(kind=="toggle_flag"){const int id=step.at("id");session_.setFlag(id,!session_.flag(id));}
            else if(kind=="room_reset"){session_.resetRoomFacts();}
            else if(kind=="tile_substitution"){applyTileSubstitution(step);}
            else if(kind=="enemy_entry_state"){
                const auto& enemy=content_.expansion.enemies.at(step.at("enemy").get<std::string>());
                if(enemy.map!=content_.transitions.at(pendingTransition_).at("destination").at("map")||!enemy.rules.contains("entry_state_variable")||step.at("state")!=9){throw std::runtime_error("Invalid licker entry state");}
                session_.setVariable(enemy.rules.at("entry_state_variable"),9);
            }
            else if(kind=="relocate"){
                commitWorld(std::move(*pendingWorld_));pendingWorld_.reset();
            }else if(kind=="cue"){
                const std::string object=content_.transitions.at(pendingTransition_).at("object");
                const std::string self=content_.objects.at(object).map==map_?object:std::string{};
                if(!ScenePresentation(*this).executeCue(step.at("cue"),self)){--phase_;return;}
            }
            else if(kind=="message"){requestMessage(step.at("message"));}
            else if(kind=="enable_combine_if_known"){
                session_.replaceKnownSkill(step.at("from_skill"),step.at("to_skill"));
            }else if(kind=="dormant_licker_entry"){
                if(session_.flag(step.at("requires_flag"))){
                    const int target=content_.transitions.at(pendingTransition_).at("destination").at("map");
                    const auto enemy=std::find_if(content_.expansion.enemies.begin(),content_.expansion.enemies.end(),[&](const auto& item){return item.second.map==target&&item.second.sourceSlot==step.at("source_slot")&&item.second.behavior==EnemyBehaviorKind::Licker&&item.second.rules.contains("entry_state_variable");});
                    if(enemy==content_.expansion.enemies.end()){throw std::logic_error("Missing dormant licker entry binding");}
                    session_.setVariable(enemy->second.rules.at("entry_state_variable"),step.at("state"));
                }
            }else{throw std::runtime_error("Unsupported transfer operation: "+kind);}
            if(wait_>0||messageRequest_||routesPending()){return;}
        }
        finishForeground();return;
    }
}
