#include "combat_scene.h"
#include "enemy_controller.h"
#include "enemy_host.h"
#include "scene_presentation.h"
#include <algorithm>
#include <cmath>
namespace{
const nlohmann::json* cachedBarrelProfile(const CombatContent& content,int map){
    if(content.catalog.contains("cached_environment_explosions")){for(const auto& profile:content.catalog.at("cached_environment_explosions")){if(content.objects.at(profile.at("object").get<std::string>()).map==map){return &profile;}}}
    if(!content.full->contains("runtime")||!content.full->at("runtime").contains("underground")){return nullptr;}
    const auto& underground=content.full->at("runtime").at("underground");
    if(!underground.contains("environmental_explosion")){return nullptr;}
    const auto& barrel=underground.at("environmental_explosion");
    if(!barrel.contains("source_registration")||content.objects.at(barrel.at("object").get<std::string>()).map!=map){return nullptr;}
    return &barrel;
}
}

void CombatScene::updateEnvironmentRegistrations(){
    if(const auto* profile=cachedBarrelProfile(content_,map_);profile&&profile->contains("cached_registration")&&isCurrentPage(profile->at("object"),profile->at("setup_page"))){
        const auto& r=profile->at("cached_registration");const std::string id=profile->at("object");auto& state=environmentTargets_[id];state.registered=true;state.contact=sampleActorScreen(id);
        session_.setVariable(r.at("event_variable"),content_.objects.at(id).variants.front().definition.sourceEventId);
        for(int axis=0;axis<2;++axis){session_.setVariable(r.at("half_variables").at(axis),profile->at("source_registration").at("half_pixels").at(axis));session_.setVariable(r.at("screen_variables").at(axis),int(axis?state.contact.y:state.contact.x));}
    }
    if(!content_.catalog.contains("environment_targets")){return;}
    for(const auto& target:content_.catalog.at("environment_targets")){
        if(target.value("response",std::string{})!="breakable"||target.at("map")!=map_||!isCurrentPage(target.at("registration_object"),target.at("register_page"))){continue;}
        auto& state=environmentTargets_[target.at("object")];const auto p=findCharacter(*world_,target.at("object")).motion.position();const auto& offset=target.at("body_offset_pixels");
        state.registered=true;state.contact={(p.x+.5)*definition_.tileSize+offset.at(0).get<int>(),(p.y+1)*definition_.tileSize+offset.at(1).get<int>()};
    }
}

AttackResolution CombatScene::resolveEnvironmentAttack(const AttackSample& sample,std::set<std::string>& resolved){
    AttackResolution result;
    if(const auto* barrel=cachedBarrelProfile(content_,map_)){
        const std::string id=barrel->at("object");
        if(barrel->contains("cached_registration")){
            const auto cache=environmentTargets_.find(id);const auto& registration=barrel->at("cached_registration");
            if(!resolved.contains(id)&&cache!=environmentTargets_.end()&&cache->second.registered){
                const auto origin=camera_.center();const GridPosition contact{double(session_.variable(registration.at("screen_variables").at(0))),double(session_.variable(registration.at("screen_variables").at(1)))};
                if(std::abs(sample.center.x-(origin.x-160)-contact.x)<=sample.halfPixels[0]+session_.variable(registration.at("half_variables").at(0))&&std::abs(sample.center.y-(origin.y-120)-contact.y)<=sample.halfPixels[1]+session_.variable(registration.at("half_variables").at(1))){
                    ++result.contacts;
                    if(sample.phase==AttackPhase::Flight){session_.setFlag(barrel->at("start_fact"),true);}
                    else if(sample.phase!=AttackPhase::Burning&&((sample.kind!=AttackKind::Knife&&!session_.flag(4))||std::find(barrel->at("melee_items").begin(),barrel->at("melee_items").end(),session_.equipped(2))!=barrel->at("melee_items").end())&&std::find(barrel->at("receiver_pages").begin(),barrel->at("receiver_pages").end(),sample.receiverPage)!=barrel->at("receiver_pages").end()){
                        resolved.insert(id);++result.processed;if(!session_.flag(barrel->at("spent_fact"))){session_.setFlag(barrel->at("activation_fact"),true);}
                    }
                }
            }
        }else if(!resolved.contains(id)&&!session_.flag(barrel->at("activation_fact"))&&!session_.flag(barrel->at("spent_fact"))&&isCurrentPage(id,barrel->at("setup_page"))){
            const auto p=findCharacter(*world_,id).motion.position();const auto& half=barrel->at("source_registration").at("half_pixels");
            if(std::abs(sample.center.x-(p.x+.5)*definition_.tileSize)<=sample.halfPixels[0]+half.at(0).get<int>()&&std::abs(sample.center.y-(p.y+1)*definition_.tileSize)<=sample.halfPixels[1]+half.at(1).get<int>()){
                ++result.contacts;if(sample.phase!=AttackPhase::Flight&&sample.phase!=AttackPhase::Burning&&std::find(barrel->at("receiver_pages").begin(),barrel->at("receiver_pages").end(),sample.receiverPage)!=barrel->at("receiver_pages").end()){session_.setFlag(barrel->at("activation_fact"),true);resolved.insert(id);++result.processed;}
            }
        }
    }
    if(!content_.catalog.contains("environment_targets")){return result;}
    for(const auto& target:content_.catalog.at("environment_targets")){
        const std::string id=target.at("object");
        if(target.value("response",std::string{})=="breakable"){
            const auto cached=environmentTargets_.find(id);
            if(target.at("map")!=map_||resolved.contains(id)||cached==environmentTargets_.end()||!cached->second.registered){continue;}
            const auto& p=cached->second.contact;const auto& half=target.at("contact_half_pixels");
            if(std::abs(sample.center.x-p.x)>sample.halfPixels[0]+half.at(0).get<int>()||std::abs(sample.center.y-p.y)>sample.halfPixels[1]+half.at(1).get<int>()){continue;}
            ++result.contacts;
            if(sample.phase==AttackPhase::Flight||std::find(target.at("receiver_pages").begin(),target.at("receiver_pages").end(),sample.receiverPage)==target.at("receiver_pages").end()){continue;}
            if(!session_.flag(target.at("completed_fact"))){
                nlohmann::json steps=nlohmann::json::array();for(const auto& cue:target.at("response_cues")){steps.push_back({{"kind","cue"},{"cue",cue}});}ScenePresentation(*this).validateSequence(steps,id);
                for(const auto& cue:target.at("response_cues")){playSound(cue);}
                session_.setFlag(target.at("completed_fact"),true);refreshObjectPages();
            }
            resolved.insert(id);++result.processed;continue;
        }
        if(target.at("map")!=map_||resolved.contains(id)||session_.flag(target.at("unused_fact"))||session_.flag(target.at("trigger_fact"))||!isCurrentPage(id,target.at("register_page"))){continue;}
        const auto p=findCharacter(*world_,id).motion.position();const auto& offset=target.at("body_offset_pixels");const auto& half=target.at("contact_half_pixels");
        if(std::abs(sample.center.x-((p.x+.5)*definition_.tileSize+offset.at(0).get<int>()))>sample.halfPixels[0]+half.at(0).get<int>()||
           std::abs(sample.center.y-((p.y+1)*definition_.tileSize+offset.at(1).get<int>()))>sample.halfPixels[1]+half.at(1).get<int>()){continue;}
        ++result.contacts;
        if(sample.phase==AttackPhase::Flight){continue;}
        session_.setFlag(target.at("trigger_fact"),true);resolved.insert(id);++result.processed;
    }
    return result;
}

void CombatScene::updateUndergroundExplosion(){
    const auto* profile=cachedBarrelProfile(content_,map_);if(!profile){return;}
    const auto& barrel=*profile;const std::string id=barrel.at("object");
    if(session_.flag(barrel.at("spent_fact"))||!session_.flag(barrel.at("activation_fact"))){return;}
    auto& state=environmentTargets_[id];
    if(state.flow.wait>0&&--state.flow.wait>0){return;}
    // Source ShowPicture waits for the message window; its earlier position sample remains fixed.
    if(!state.started){
        state.started=true;state.picture=sampleActorScreen(id);const auto& offset=barrel.at("picture_offset_pixels");
        state.picture.x+=offset.at(0).get<int>();state.picture.y+=offset.at(1).get<int>();
        if(barrel.contains("cached_registration")){const auto& vars=barrel.at("cached_registration").at("screen_variables");session_.setVariable(vars[0],int(state.picture.x));session_.setVariable(vars[1],int(state.picture.y));}
        session_.setFlag(barrel.at("start_fact"),true);
        for(const auto& cueId:barrel.at("startup_cues")){cue(cueId,id);}
    }
    if(messageRequest_||inputRequest_){return;}
    const auto& frames=barrel.at("frame_cues");
    if(state.flow.next==frames.size()){
        cue(barrel.at("clear_picture"),id);session_.setFlag(barrel.at("spent_fact"),true);refreshObjectPages();return;
    }
    presentation_.push_back({PresentationEvent::Kind::Cue,frames.at(state.flow.next),{}, {},0,id,state.picture});
    ++state.flow.next;state.flow.wait=barrel.at("ticks_per_frame");
    if(state.flow.next!=barrel.at("damage_after_frame").get<std::size_t>()){return;}
    const auto center=sampleActorScreen(id);const auto& offset=barrel.at("blast_center_offset_pixels");
    const GridPosition blast{center.x+offset.at(0).get<int>(),center.y+offset.at(1).get<int>()};
    const auto& half=barrel.at("blast_half_pixels");const auto& registrations=barrel.at("target_source_event_variables");
    const int receiver=barrel.contains("fixed_receiver")?barrel.at("fixed_receiver").get<int>():session_.variable(registrations.at(0))==barrel.at("receiver_selection").at("if_event_id")?barrel.at("receiver_selection").at("then_page").get<int>():barrel.at("receiver_selection").at("otherwise_page").get<int>();
    for(std::size_t slot=0;slot<registrations.size();++slot){
        const int eventVariable=registrations.at(slot),event=session_.variable(eventVariable);
        if(event<1||std::abs(blast.x-session_.variable(eventVariable-7))>half.at(0).get<int>()+session_.variable(eventVariable-3)||
           std::abs(blast.y-session_.variable(eventVariable-6))>half.at(1).get<int>()+session_.variable(eventVariable-2)){continue;}
        const int pulse=barrel.at("burn_pulse_facts").at(slot);session_.setFlag(pulse,false);session_.setFlag(pulse,true);
        const auto target=std::find_if(enemies_.begin(),enemies_.end(),[&](const auto& enemy){return content_.enemies.at(enemy->id()).sourceOrder==event;});
        if(target==enemies_.end()){continue;}
        // P7 has already tested the source cache; the called page does not sample geometry again.
        (*target)->receiveAttack({{}, {0,0},AttackKind::Grenade,receiver,0,false,0,AttackPhase::ResolvedContact});
    }
    const auto player=sampleActorScreen("player");const auto& damage=barrel.at("player_damage");
    if(!session_.flag(damage.at("guard_off"))&&session_.hp()>0&&std::abs(blast.x-player.x)<=half.at(0).get<int>()+session_.variable(87)&&std::abs(blast.y-(player.y-8))<=half.at(1).get<int>()+session_.variable(88)){
        const auto& reaction=content_.catalog.at("rescue_environment").at("player_reaction");const int mode=damage.at("mode"),pulse=reaction.at("activation_fact"),attack=damage.at("attack");
        session_.setVariable(reaction.at("mode_source_variable"),mode);session_.setFlag(pulse,false);session_.setFlag(pulse,true);session_.setVariable(23,attack);
        if(barrel.value("source_player_damage",false)){EnemyHost(*this,id).receiveSourceDamage(attack);}else{applyPlayerDamage(biteDamage(attack,session_.defense(),random_(-5,2)));}
        presentation_.push_back({PresentationEvent::Kind::Effect,reaction.at("modes").at(std::to_string(mode)).at("effect"),{},world_->player().motion.position(),0,"player"});session_.setFlag(reaction.at("finish_clear_fact"),false);
    }
}

void CombatScene::updateRescueEnvironment(){
    if(!content_.catalog.contains("rescue_environment")){return;}
    const auto& rescue=content_.catalog.at("rescue_environment");
    const auto& clear=rescue.at("clearance");
    const auto& deaths=session_.difficulty()=="easy"&&content_.catalog.contains("difficulty")&&content_.catalog.at("difficulty").contains("rescue")?content_.catalog.at("difficulty").at("rescue").at("easy_dead_facts"):clear.at("dead_facts");
    if(isCurrentPage(clear.at("object"),clear.at("page"))&&session_.flag(clear.at("activation_fact"))&&
       std::all_of(deaths.begin(),deaths.end(),[&](const auto& fact){return session_.flag(fact);})){
        session_.setFlag(clear.at("completed_fact"),true);
    }
    const auto& reinforcement=rescue.at("reinforcement");
    const std::string controller=reinforcement.at("object");
    if((!content_.catalog.contains("difficulty")||!content_.catalog.at("difficulty").contains("rescue")||session_.difficultyLevel()>=content_.catalog.at("difficulty").at("rescue").at("reinforcement_min_level").get<int>())&&isCurrentPage(controller,reinforcement.at("page"))&&session_.flag(reinforcement.at("activation_fact"))&&
       std::none_of(reinforcement.at("stop_facts").begin(),reinforcement.at("stop_facts").end(),[&](const auto& fact){return session_.flag(fact);})){
        auto& state=environmentFlows_[controller];
        if(state.next==0){state.next=1;state.wait=reinforcement.at("wait_ticks");}
        else if(state.wait>0&&--state.wait==0){
            const std::string id=reinforcement.at("enemy");auto actor=findCharacter(*world_,id);const auto& xy=reinforcement.at("tile");
            actor.motion=CharacterMotion({xy.at(0),xy.at(1)},actor.motion.facing(),actor.motion.speed());replaceCharacter(*world_,actor);
            const auto enemy=std::find_if(enemies_.begin(),enemies_.end(),[&](const auto& e){return e->id()==id;});
            if(enemy==enemies_.end()||!(*enemy)->activateState(reinforcement.at("mode"))){throw std::logic_error("Missing reinforcement arrival behavior");}
            playSound(reinforcement.at("sound_cue"));session_.setFlag(reinforcement.at("activation_commit"),true);
        }
    }
    for(const auto& target:content_.catalog.at("environment_targets")){
        if(target.value("response",std::string{})=="breakable"||target.at("map")!=map_){continue;}
        const std::string id=target.at("object");auto& state=environmentTargets_[id];
        const auto& source=target.at("player_half_source");
        const auto enemy=std::find_if(enemies_.begin(),enemies_.end(),[&](const auto& e){return e->id()==source.at("enemy").get<std::string>();});
        if(enemy!=enemies_.end()&&(*enemy)->mode()==EnemyMode::Pursuit&&session_.flag(source.at("write_conditions").at("switch_a"))&&session_.flag(source.at("write_conditions").at("switch_b"))){
            state.playerHalf=source.at("write_values").get<std::array<int,2>>();
        }
        if(session_.flag(target.at("finished_fact"))||!session_.flag(target.at("trigger_fact"))){continue;}
        if(!state.started){
            state.started=true;const auto p=findCharacter(*world_,id).motion.position();const auto& offset=target.at("picture_offset_pixels");
            state.picture={p.x+offset.at(0).get<double>()/definition_.tileSize,p.y+offset.at(1).get<double>()/definition_.tileSize};
        }
        if(state.flow.wait>0&&--state.flow.wait>0){continue;}
        const auto& steps=target.at("steps");
        while(state.flow.next<steps.size()){
            const auto& step=steps.at(state.flow.next++);const std::string kind=step.at("kind");
            if(kind=="flag"){session_.setFlag(step.at("id"),step.at("value"));continue;}
            if(kind=="damage"){
                const auto p=findCharacter(*world_,id).motion.position();const auto& offset=target.at("center_offset_pixels");
                const GridPosition center{(p.x+.5)*definition_.tileSize+offset.at(0).get<int>(),(p.y+1)*definition_.tileSize+offset.at(1).get<int>()};
                const auto half=step.at("half_pixels").get<std::array<int,2>>();
                const AttackSample blast{center,half,AttackKind::Grenade,step.at("receiver_page")};
                for(auto& e:enemies_){
                    const auto hit=e->receiveAttack(blast);
                    if(hit.processed){
                        const auto& runtime=content_.full->at("runtime");const auto& actions=runtime.at(runtime.contains("combat_effects")?"combat_effects":"weapon_actions");
                        const int slot=content_.enemyBurnSources().at(e->id());
                        const int fact=actions.at("burns").at(std::to_string(slot)).at("activation_fact");
                        session_.setFlag(fact,false);session_.setFlag(fact,true);
                    }
                }
                const auto player=world_->player().motion.position();
                if(!session_.flag(target.at("player_guard_off"))&&session_.hp()>0&&
                   std::abs(center.x-(player.x+.5)*definition_.tileSize)<=half[0]+state.playerHalf[0]&&
                   std::abs(center.y-((player.y+1)*definition_.tileSize-8))<=half[1]+state.playerHalf[1]){
                    const auto& reaction=rescue.at("player_reaction");const int mode=step.at("player_reaction_mode"),pulse=reaction.at("activation_fact");
                    session_.setVariable(reaction.at("mode_source_variable"),mode);session_.setFlag(pulse,false);session_.setFlag(pulse,true);
                    const int attack=step.at("player_raw_damage");session_.setVariable(23,attack);applyPlayerDamage(biteDamage(attack,session_.defense(),random_(-5,2)));
                    presentation_.push_back({PresentationEvent::Kind::Effect,reaction.at("modes").at(std::to_string(mode)).at("effect"),{},player,0,"player"});session_.setFlag(reaction.at("finish_clear_fact"),false);
                }
                continue;
            }
            if(kind!="cue"){throw std::logic_error("Unsupported environmental target step");}
            const std::string cueId=step.at("cue");
            if(target.at("explosion_picture_positions").contains(cueId)){
                presentation_.push_back({PresentationEvent::Kind::Cue,cueId,{},state.picture,0,id});continue;
            }
            const int foregroundWait=wait_;wait_=0;
            const bool complete=ScenePresentation(*this).executeCue(cueId,id);
            state.flow.wait=wait_;wait_=foregroundWait;
            if(!complete){--state.flow.next;break;}
            if(state.flow.wait>0){break;}
        }
    }
}
