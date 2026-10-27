#include "pursuer_behavior.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
PursuerBehavior::PursuerBehavior(CombatScene& scene,const EnemyDefinition& definition):definition_(definition),host_(scene,definition.id){validateBodyManager();damage_=500-host_.session().pursuerHealth();}
bool PursuerBehavior::present() const{
    const auto& rules=definition_.rules;
    return !host_.session().flag(definition_.deadFlag)&&(!rules.contains("stop_fact")||!host_.session().flag(rules.at("stop_fact")))&&
        (rules.at("activation_fact").is_null()||host_.session().flag(rules.at("activation_fact")));
}
void PursuerBehavior::updateParallel(){
    advanceBlood();
    advanceBurn();
    advanceBody();
}
void PursuerBehavior::update(const InputFrame&){
    host_.claimAutonomy();
    if(!present()){
        mode_=EnemyMode::Dormant;reappearing_=false;reappearFlow_.clear();wait_=0;attackChoice_=0;attackTicks_=0;
        autonomousRoute_.reset();autonomousPage_=0;host_.cancelRoutes();
        return;
    }
    const auto& rules=definition_.rules;
    const auto* reappear=rules.contains("runtime_profile")&&rules.at("runtime_profile").contains("reappear")?&rules.at("runtime_profile").at("reappear"):nullptr;
    const auto* page=host_.content().activeVariant(host_.content().objects.at(id()),host_.session());
    const bool sourceAutonomy=rules.contains("runtime_profile")&&rules.at("runtime_profile").contains("autonomy");
    const bool requested=reappear&&host_.session().flag(reappear->at("flag"))&&
        (sourceAutonomy?(page&&page->page==rules.at("runtime_profile").at("pages").at("reappear")):mode_==EnemyMode::Dormant);
    if(requested){
        if(!reappearing_){
            // A higher original page replaces the old action, including active windup.
            mode_=EnemyMode::Dormant;attackChoice_=0;attackTicks_=0;autonomousRoute_.reset();autonomousPage_=0;host_.cancelRoutes();
            reappearing_=true;reappearTile_=host_.player().motion.tile();reappearPhase_=0;reappearSequence_=0;reappearFlow_.clear();
            specialReappearance_=false;
            if(reappear->value("spawn_before_special",false)){
                auto actor=host_.actor();actor.motion=CharacterMotion(reappearTile_,actor.motion.facing(),actor.motion.speed());host_.setActor(actor);
            }
            if(reappear->contains("special")){
                const auto& special=reappear->at("special");
                if(special.contains("player_tile")){specialReappearance_=reappearTile_==TilePosition{special.at("player_tile").at(0),special.at("player_tile").at(1)};}
                else{specialReappearance_=!host_.session().flag(special.at("guard_off"))&&reappearTile_.y==special.at("player_y")&&
                    reappearTile_.x>=special.at("player_x_inclusive").at(0).get<int>()&&reappearTile_.x<=special.at("player_x_inclusive").at(1).get<int>();}
            }
            if(specialReappearance_){
                const auto& special=reappear->at("special");wait_=special.contains("sequences")?special.at("sequences").at(0).at("wait_before_ticks").get<int>():special.at("wait_before_break_ticks").get<int>();
            }else{const auto& cue=host_.content().cue(reappear->at("wait").get<std::string>());wait_=std::max(1,cue.at("tenths").get<int>()*6);}
            return;
        }
        if(!advanceReappearance(*reappear)){return;}
        if(reappear->contains("sound")&&(!specialReappearance_||!reappear->at("special").value("skip_default_sound",false))){host_.sound(reappear->at("sound"));}
        if(!reappear->value("spawn_before_special",false)){
            auto actor=host_.actor();actor.motion=CharacterMotion(reappearTile_,actor.motion.facing(),actor.motion.speed());host_.setActor(actor);
        }
        damage_=500-host_.session().pursuerHealth();
        if(reappear->contains("music")){host_.cue(reappear->at("music"));}
        host_.session().setFlag(reappear->at("flag"),false);reappearing_=false;
    }
    if(reappearing_){reappearing_=false;wait_=0;mode_=damage_>=500?EnemyMode::Ending:EnemyMode::Pursuit;}
    if(mode_==EnemyMode::Ending){
        if(!completionSubmitted_&&definition_.rules.contains("completion_flow")){completionSubmitted_=host_.completeEncounter(definition_.rules.at("completion_flow"));}
        return;
    }
    if(mode_==EnemyMode::Dormant){mode_=EnemyMode::Pursuit;}
    if(rules.contains("runtime_profile")&&rules.at("runtime_profile").contains("attack_stages")){
        const auto& profile=rules.at("runtime_profile");
        if(attackChoice_){
            if(!host_.session().flag(profile.at("attack_fact"))){attackChoice_=0;mode_=EnemyMode::Pursuit;host_.cancelRoutes();return;}
            advanceAttack();return;
        }
        const int selector=host_.session().variable(profile.at("attack_selector_variable"));
        page=host_.content().activeVariant(host_.content().objects.at(id()),host_.session());
        if(host_.session().flag(profile.at("attack_fact"))&&selector>=1&&
           (!profile.contains("autonomy")||(page&&(page->page==profile.at("pages").at("slash")||page->page==profile.at("pages").at("sweep"))))){startAttack(selector>=2?2:1);return;}
        if(profile.contains("autonomy")){advanceAutonomy();}
    }
}
bool PursuerBehavior::advanceReappearanceFlow(){
    const auto& flow=host_.content().catalog.at("flows").at(reappearFlow_);
    if(reappearFlowStep_==0&&flow.value("wait_for_combat",false)&&!host_.session().flag(flow.at("wait_for_fact"))){wait_=flow.at("wait_check_ticks");return false;}
    const auto& steps=flow.at("steps");
    while(reappearFlowStep_<steps.size()){
        const auto& step=steps.at(reappearFlowStep_++);const std::string kind=step.at("kind");
        if(kind=="flag"){host_.session().setFlag(step.at("id"),step.at("value"));}
        else if(kind=="cue"){
            const auto& cue=host_.content().cue(step.at("cue").get<std::string>());
            if(cue.at("kind")=="wait"){wait_=std::max(1,cue.at("tenths").get<int>()*6);return false;}
            host_.reappearanceCue(reappearFlow_,step.at("cue"));
        }else{throw std::runtime_error("Unsupported finite reappearance step");}
    }
    reappearFlow_.clear();return true;
}
bool PursuerBehavior::advanceReappearance(const nlohmann::json& profile){
    if(wait_>0&&--wait_>0){return false;}
    if(!specialReappearance_){return true;}
    const auto& special=profile.at("special");
    if(!special.contains("sequences")){
        if(reappearPhase_==0){reappearFlow_=special.at("flow");reappearFlowStep_=0;reappearPhase_=1;}
        if(reappearPhase_==1){if(!advanceReappearanceFlow()){return false;}wait_=special.at("wait_after_break_ticks");reappearPhase_=2;return false;}
        return true;
    }
    const auto& sequences=special.at("sequences");
    if(reappearSequence_>=sequences.size()){return true;}
    const auto& step=sequences.at(reappearSequence_);
    if(reappearPhase_==0){
        if(step.contains("set_fact")){host_.session().setFlag(step.at("set_fact"),true);return true;}
        if(step.value("snapshot_player_tile",false)){reappearTile_=host_.player().motion.tile();}
        if(step.contains("place_objects_before_wait")){
            for(const auto& placement:step.at("place_objects_before_wait")){
                const auto& offset=placement.at("offset");host_.placeReappearanceObject(placement.at("object"),{reappearTile_.x+offset.at(0).get<int>(),reappearTile_.y+offset.at(1).get<int>()});
            }
            wait_=step.at("wait_between_ticks");reappearPhase_=1;return false;
        }
        if(step.contains("place_object")){host_.placeReappearanceObject(step.at("place_object"),{step.at("tile").at(0),step.at("tile").at(1)});}
        reappearPhase_=1;
    }
    if(reappearPhase_==1){
        if(step.contains("place_object_after_wait")){const auto& offset=step.at("offset");host_.placeReappearanceObject(step.at("place_object_after_wait"),{reappearTile_.x+offset.at(0).get<int>(),reappearTile_.y+offset.at(1).get<int>()});}
        reappearFlow_=step.at("invoke");reappearFlowStep_=0;reappearPhase_=2;
    }
    if(!advanceReappearanceFlow()){return false;}
    ++reappearSequence_;reappearPhase_=0;
    if(reappearSequence_>=sequences.size()){return true;}
    const auto& next=sequences.at(reappearSequence_);wait_=next.contains("wait_before_ticks")?next.at("wait_before_ticks").get<int>():next.at("wait_ticks").get<int>();return false;
}
void PursuerBehavior::applyPage(const std::string& role){
    const int pageId=definition_.rules.at("runtime_profile").at("pages").at(role);
    applyPage(pageId);
}
void PursuerBehavior::applyPage(int pageId){
    const auto& pages=host_.content().objects.at(id()).variants;
    const auto page=std::find_if(pages.begin(),pages.end(),[&](const auto& candidate){return candidate.page==pageId;});
    if(page==pages.end()){throw std::runtime_error("Missing pursuer presentation page");}
    auto actor=host_.actor();actor.spriteId=page->definition.spriteId;actor.layer=page->definition.layer;
    actor.setSpeedLevel(page->definition.moveSpeed);
    actor.setAnimation(CharacterAnimation(animationForSpeed(page->definition.moveSpeed),page->definition.initialColumn,page->definition.animationType),actor.motion.facing());
    host_.setActor(actor);
}
void PursuerBehavior::advanceAutonomy(){
    const auto& profile=definition_.rules.at("runtime_profile");
    const auto& autonomy=profile.at("autonomy");
    const auto& object=host_.content().objects.at(id());
    const auto* selected=host_.content().activeVariant(object,host_.session());
    if(!selected){autonomousRoute_.reset();autonomousPage_=0;return;}
    if(selected->page!=autonomousPage_){
        const auto old=std::find_if(object.variants.begin(),object.variants.end(),[&](const auto& p){return p.page==autonomousPage_;});
        const auto& steps=selected->autonomousRoute.steps;
        const bool same=old!=object.variants.end()&&old->autonomousRoute.steps.size()==steps.size()&&
            std::equal(steps.begin(),steps.end(),old->autonomousRoute.steps.begin(),[](const auto& a,const auto& b){return a.kind==b.kind;});
        applyPage(selected->page);
        if(same&&autonomousRoute_){autonomousRoute_->refreshDefinition(selected->autonomousRoute);}
        else if(!same){autonomousRoute_.reset();if(!steps.empty()){autonomousRoute_.emplace(selected->autonomousRoute,id());}}
        autonomousPage_=selected->page;
    }
    if(profile.contains("extra_modes")){
        for(const auto& extra:profile.at("extra_modes")){
            if(extra.at("page")==selected->page&&extra.at("role")=="proximity-entry"){advanceProximityEntry(extra);}
        }
    }
    const nlohmann::json* mode=nullptr;
    for(const auto& candidate:autonomy.at("modes")){if(candidate.at("page")==selected->page){mode=&candidate;break;}}
    if(mode){
        mode_=selected->page==profile.at("pages").at("approach")?EnemyMode::Approach:EnemyMode::Pursuit;
        auto& session=host_.session();const auto& half=definition_.rules.at("hitbox_half_pixels");session.setVariable(87,half.at(0));session.setVariable(88,half.at(1));session.setFlag(autonomy.at("attack_fact"),false);
        if(session.flag(autonomy.at("burn_flash_fact"))){host_.cue(autonomy.at("burn_flash_cues").at(std::to_string(selected->page)));}
        const auto direction=host_.sourceDirection();
        if(direction){
        const std::string facing(directionName(*direction));
        if(playerInside(mode->at("attack_bounds_tile").at(facing))){
            const auto& selector=mode->at("selector");
            if(selector.contains("constant")){
                session.setVariable(autonomy.at("selector_variable"),selector.at("constant"));session.setFlag(autonomy.at("attack_fact"),true);
            }else{
                const int rolled=host_.random(selector.at("random_inclusive").at(0),selector.at("random_inclusive").at(1));
                const auto& mapped=selector.at("output_by_random").at(rolled);
                // Original random writes precede the optional selector/attack branches.
                session.setVariable(autonomy.at("selector_variable"),mapped.is_null()?rolled:mapped.get<int>());
                if(!mapped.is_null()){session.setFlag(autonomy.at("attack_fact"),true);}
            }
        }
        const auto& turn=mode->at("terrain_turns").at(facing);
        auto tile=host_.actor().motion.tile();const auto& offset=turn.at("front_tile_offset");
        if(host_.terrain({tile.x+offset.at(0).get<int>(),tile.y+offset.at(1).get<int>()})==autonomy.at("terrain_id").get<int>()){
            const auto player=host_.player().motion.tile();const bool x=turn.at("comparison_axis")=="x";
            const int actorAxis=x?tile.x:tile.y,playerAxis=x?player.x:player.y;
            if(actorAxis!=playerAxis){host_.startRoute(turn.at(actorAxis<playerAxis?"actor_less_player_cue":"actor_more_player_cue"));}
        }
    }
        }
    if(autonomousRoute_&&!host_.routePending()&&host_.advance(*autonomousRoute_)){autonomousRoute_.reset();}
}
void PursuerBehavior::advanceProximityEntry(const nlohmann::json& mode){
    const auto& autonomy=definition_.rules.at("runtime_profile").at("autonomy");
    mode_=EnemyMode::Pursuit;const auto& half=definition_.rules.at("hitbox_half_pixels");host_.session().setVariable(87,half.at(0));host_.session().setVariable(88,half.at(1));
    if(host_.session().flag(autonomy.at("burn_flash_fact"))){host_.cue(mode.at("burn_flash_cue"));}
    const auto actor=host_.actor(),player=host_.player();
    const auto a=actor.motion.position(),p=player.motion.position();
    const double x=(a.x-p.x)*16,y=(a.y-p.y)*16;const auto& bounds=mode.at("screen_bounds_pixels");
    if(x>=bounds.at(0).get<int>()&&x<=bounds.at(1).get<int>()&&y>=bounds.at(2).get<int>()&&y<=bounds.at(3).get<int>()){
        const auto& commit=mode.at("proximity_commit");
        if(commit.contains("fact")){host_.cue(commit.at("music"));host_.session().setFlag(commit.at("fact"),commit.at("value"));}
        else if(commit.at("field")=="corridor_door_status"){host_.session().setVariable(193,commit.at("value"));}
        else{throw std::runtime_error("Unsupported pursuer proximity field");}
    }
    const auto facing=actor.motion.facing();
    if(host_.terrain(adjacentTile(actor.motion.tile(),facing))!=autonomy.at("terrain_id").get<int>()){return;}
    const auto foot=host_.actorScreenFoot();
    const bool horizontal=facing==Direction::Left||facing==Direction::Right;
    const int cached=host_.session().variable(horizontal?84:83);
    const double playerAxis=horizontal?foot.y-y:foot.x-x;
    if(cached==playerAxis){return;}
    for(const auto& turn:mode.at("terrain_turns")){
        if(turn.at("direction")==std::string(directionName(facing))){
            host_.startRoute(turn.at(cached<playerAxis?"actor_less_player_cue":"actor_more_player_cue"));return;
        }
    }
}
void PursuerBehavior::samplePictureWorkspace(int commonEvent,const nlohmann::json& offset){
    auto& session=host_.session();const int variable=93+14*(commonEvent-24);
    session.setVariable(variable,session.variable(83)+offset.at(0).get<int>());
    session.setVariable(variable+1,session.variable(84)+offset.at(1).get<int>());
}
void PursuerBehavior::presentPictureCue(const std::string& cue,int commonEvent){
    const int variable=93+14*(commonEvent-24);
    const GridPosition sample{static_cast<double>(host_.session().variable(variable)),static_cast<double>(host_.session().variable(variable+1))};
    host_.present({PresentationEvent::Kind::Cue,cue,{},host_.pictureWorkspacePosition(commonEvent),0,id(),sample});
}
void PursuerBehavior::advanceBlood(){
    const auto& rules=definition_.rules;if(!rules.contains("runtime_profile")||!rules.at("runtime_profile").contains("blood")){return;}
    const auto& profile=rules.at("runtime_profile");host_.advanceBlood(profile.at("blood"),host_.session().variable(profile.at("receiver").at("blood_type_variable")));
}
void PursuerBehavior::startAttack(int choice){
    attackChoice_=choice;attackTicks_=0;bodyFrameCounter_=0;mode_=EnemyMode::Windup;
    autonomousRoute_.reset();autonomousPage_=0;
    const std::string action=choice==1?"slash":"sweep";
    applyPage(action);host_.cancelRoutes();
    host_.startRoute(definition_.rules.at("runtime_profile").at("attack_stages").at(action).at("start_route"));
}
bool PursuerBehavior::factsOff(const nlohmann::json& facts) const{
    return std::all_of(facts.begin(),facts.end(),[&](const auto& fact){return !host_.session().flag(fact);});
}
bool PursuerBehavior::playerInside(const nlohmann::json& bounds) const{
    const auto origin=host_.actor().motion.tile(),player=host_.player().motion.tile();
    const int x=player.x-origin.x,y=player.y-origin.y;
    return x>=bounds.at(0).get<int>()&&x<=bounds.at(1).get<int>()&&y>=bounds.at(2).get<int>()&&y<=bounds.at(3).get<int>();
}
void PursuerBehavior::presentCue(const std::string& cue){
    const std::string kind=host_.content().cue(cue).at("kind");
    if(kind=="sound"||kind=="animation"){host_.cue(cue);}
    else if(kind=="shake"||kind=="erase_picture"){host_.present({PresentationEvent::Kind::Cue,cue,{},host_.actor().motion.position(),0,id()});}
    else{throw std::logic_error("Unsupported pursuer stage cue: "+kind);}
}
void PursuerBehavior::validateBodyManager() const{
    const auto& rules=definition_.rules;
    if(!rules.contains("runtime_profile")||!rules.at("runtime_profile").contains("presentation_lifecycle")){return;}
    const auto& lifecycle=rules.at("runtime_profile").at("presentation_lifecycle");
    const auto found=host_.content().objects.find(lifecycle.at("manager_object").get<std::string>());
    if(found==host_.content().objects.end()||found->second.map!=definition_.map){throw std::runtime_error("Unknown or foreign pursuer body manager");}
    const auto hasPage=[&](int number,int trigger){return std::any_of(found->second.variants.begin(),found->second.variants.end(),[&](const auto& page){return page.page==number&&page.definition.trigger==trigger;});};
    if(lifecycle.at("page_selection")!="highest-original-variant"||!hasPage(lifecycle.at("stop_page"),0)){
        throw std::runtime_error("Pursuer body manager lacks its source suppression page");
    }
    for(int page:{1,2,3}){if(!hasPage(page,4)){throw std::runtime_error("Pursuer body manager lacks a source parallel page");}}
    if(lifecycle.contains("terminal_pages")){for(const auto& page:lifecycle.at("terminal_pages")){if(!hasPage(page,0)){throw std::runtime_error("Unknown body suppression page");}}}
}
void PursuerBehavior::sampleBody(){
    const auto foot=host_.actorScreenFoot();
    const auto& offset=definition_.rules.at("runtime_profile").at("blood").at("body_offset_from_worldfoot_pixels");
    auto& session=host_.session();
    session.setVariable(83,static_cast<int>(foot.x)+offset.at(0).get<int>());
    session.setVariable(84,static_cast<int>(foot.y)+offset.at(1).get<int>());
}
bool PursuerBehavior::presentBodyCue(const std::string& cue){
    if(!host_.pictureCommandsAllowed()){return false;}
    host_.present({PresentationEvent::Kind::Cue,cue,{},host_.screenPicturePosition(bodyPictureSample_),0,id(),bodyPictureSample_});return true;
}
void PursuerBehavior::advanceBody(){
    const auto& rules=definition_.rules;
    if(!rules.contains("runtime_profile")||!rules.at("runtime_profile").contains("presentation_lifecycle")){return;}
    const auto& profile=rules.at("runtime_profile"),lifecycle=profile.at("presentation_lifecycle");
    const auto& manager=host_.content().objects.at(lifecycle.at("manager_object").get<std::string>());
    const auto* page=host_.content().activeVariant(manager,host_.session());
    if(!page||page->definition.trigger!=4){bodyPage_=0;bodyPhase_=BodyPhase::Sample;bodyCueIndex_=0;return;}
    if(page->page<1||page->page>3){throw std::runtime_error("Unsupported pursuer body parallel page");}
    if(bodyPage_!=page->page){bodyPage_=page->page;bodyStarting_=true;bodyPhase_=BodyPhase::Sample;bodyCueIndex_=0;}
    const bool idle=bodyPage_==1;
    const std::string action=idle?"idle":bodyPage_==2?"slash":"sweep";
    const auto& pictures=profile.at("presentation_profile").at(action);
    // A finite source cursor stops at each native picture guard. Completed writes
    // remain committed; a pending Move never resamples the live World on retry.
    while(true){
        switch(bodyPhase_){
        case BodyPhase::Sample:{
            if(idle||!bodyStarting_||bodyPage_==2){sampleBody();}
            if(idle){host_.session().setVariable(90,definition_.sourceOrder);const auto tile=host_.actor().motion.tile();host_.session().setVariable(81,tile.x);host_.session().setVariable(82,tile.y);}
            if(idle||bodyStarting_){host_.sampleSourceDirection();bodyDirection_=directionName(host_.actor().motion.facing());}
            const auto foot=host_.actorScreenFoot();bodyPictureSample_={static_cast<double>(static_cast<int>(foot.x)),static_cast<double>(static_cast<int>(foot.y))};
            const auto& direction=pictures.at("directions").at(bodyDirection_);
            const bool setup=idle?idleShownDirection_!=bodyDirection_:bodyStarting_;
            if(!setup&&!idle&&!direction.at("frames").contains(std::to_string(bodyFrameCounter_))){return;}
            const auto& offset=setup&&!idle?lifecycle.at("setup_offset_pixels").at(action):
                direction.at("frames").at(idle?"0":std::to_string(bodyFrameCounter_)).at("offset_pixels");
            bodyPictureSample_.x+=offset.at(0).get<int>();bodyPictureSample_.y+=offset.at(1).get<int>();
            bodyFrame_=idle?"0":std::to_string(bodyFrameCounter_);bodyCueIndex_=0;
            if(setup){if(idle){idleShownDirection_=bodyDirection_;}bodyPhase_=BodyPhase::Setup;}
            else{bodyPhase_=idle?BodyPhase::IdleTile:BodyPhase::Hide;}
            break;
        }
        case BodyPhase::Setup:{
            const auto& cues=pictures.at("directions").at(bodyDirection_).at("setup_cues");
            while(bodyCueIndex_<cues.size()){
                if(!presentBodyCue(cues.at(bodyCueIndex_))){return;}++bodyCueIndex_;
            }
            bodyCueIndex_=0;
            if(idle){bodyPhase_=BodyPhase::IdleTile;}
            else{bodyFrameCounter_=0;bodyStarting_=false;bodyPhase_=BodyPhase::Sample;}
            break;
        }
        case BodyPhase::IdleTile:{
            const auto tile=host_.actor().motion.tile();
            if(tile!=pictureTile_){walkPulse_+=pictures.at("walk_cycle").at("movement_tile_pulse").get<int>();pictureTile_=tile;}
            bodyPhase_=BodyPhase::Hide;break;
        }
        case BodyPhase::Hide:{
            const auto& frame=pictures.at("directions").at(bodyDirection_).at("frames").at(bodyFrame_);
            const auto& cues=idle?lifecycle.at("idle_hide_cues"):frame.at("hide_cues");
            while(bodyCueIndex_<cues.size()){
                if(!presentBodyCue(cues.at(bodyCueIndex_))){return;}++bodyCueIndex_;
            }
            bodyCueIndex_=0;bodyPhase_=BodyPhase::Select;break;
        }
        case BodyPhase::Select:{
            if(idle){
                const auto& cycle=pictures.at("walk_cycle");bodyFrameCounter_+=cycle.at("increment_per_tick").get<int>();
                bodyWalking_=walkPulse_>0;bodyVisible_=cycle.at("stationary_cue");
                if(bodyWalking_){for(const auto& range:cycle.at("ranges")){if(bodyFrameCounter_>=range.at("from").get<int>()){bodyVisible_=range.at("visible_cue");}}}
            }else{bodyVisible_=pictures.at("directions").at(bodyDirection_).at("frames").at(bodyFrame_).at("visible_cue");}
            bodyPhase_=BodyPhase::Visible;break;
        }
        case BodyPhase::Visible:
            if(!presentBodyCue(bodyVisible_)){return;}
            if(idle){
                if(bodyWalking_){
                    if(bodyFrameCounter_>=pictures.at("walk_cycle").at("reset_at").get<int>()){bodyFrameCounter_=0;}
                    --walkPulse_;
                }else{walkPulse_=0;}
            }
            bodyStarting_=false;bodyPhase_=BodyPhase::Sample;return; // Source Wait0 yields one fixed update.
        }
    }
}
void PursuerBehavior::advanceAttack(){
    ++attackTicks_;
    const auto& profile=definition_.rules.at("runtime_profile");
    const auto& action=profile.at("attack_stages").at(attackChoice_==1?"slash":"sweep");
    const auto cached=host_.sourceDirection();
    const auto live=host_.actor().motion.facing();
    const int prepEnd=action.at("directions").at("down").at("stages").at(0).at("tick");
    if(attackChoice_==1&&attackTicks_==prepEnd){slashHorizontal_=cached&&(*cached==Direction::Left||*cached==Direction::Right);}
    const auto& direction=action.at("directions").at(attackChoice_==1?(slashHorizontal_?"right":"down"):std::string(directionName(cached.value_or(Direction::Down))));
    const int prepStart=host_.content().cue(action.at("start_wait").get<std::string>()).at("tenths").get<int>()*6;
    const int frameTicks=action.at("prep_frame_ticks");
    if(attackTicks_>=prepStart&&attackTicks_<prepStart+action.at("prep_pairs").get<int>()*2*frameTicks){
        bodyFrameCounter_=1+(attackTicks_-prepStart)/frameTicks%2;
    }
    if(attackTicks_==prepStart&&action.contains("start_sound")){host_.sound(action.at("start_sound"));}
    for(const auto& timing:direction.at("stages")){
        if(timing.at("tick").get<int>()!=attackTicks_){continue;}
        bodyFrameCounter_=timing.at("frame");
        const auto& liveStages=action.at("directions").at(std::string(directionName(live))).at("stages");
        const auto& stage=attackChoice_==1&&timing.contains("damage_bounds")?
            *std::find_if(liveStages.begin(),liveStages.end(),[](const auto& item){return item.contains("damage_bounds");}):timing;
        for(const auto& cue:stage.at("cues")){presentCue(cue);}
        if(stage.contains("side_contacts")){
            for(const auto& contact:stage.at("side_contacts")){
                if(factsOff(contact.at("guards_off"))&&playerInside(contact.at("bounds"))){host_.requestSourceReaction(contact.at("receiver_page"));}
            }
        }
        if(stage.contains("damage_bounds")){
            host_.session().setVariable(24,stage.at("player_damage_mode"));
            if((attackChoice_==1||cached)&&factsOff(stage.at("damage_guards_off"))&&playerInside(stage.at("damage_bounds"))){
                host_.sound(stage.at("hit_sound"));
                host_.session().setVariable(23,definition_.rules.at("attack"));
                host_.damagePlayer(biteDamage(definition_.rules.at("attack"),host_.session().defense(),host_.random(-5,2)));
            }
            mode_=EnemyMode::Cooldown;
        }
    }
    if(attackTicks_==direction.at("rest_frame_tick").get<int>()){
        bodyFrameCounter_=0;
        if(!host_.pictureCommandsAllowed()){--attackTicks_;return;}
        for(const auto& cue:action.at("erase_cues")){presentCue(cue);}
    }
    if(attackTicks_>=direction.at("finish_tick").get<int>()){
        idleShownDirection_.clear();walkPulse_=0;bodyFrameCounter_=0;
        host_.session().setFlag(profile.at("attack_fact"),false);attackChoice_=0;mode_=EnemyMode::Pursuit;
    }
}
void PursuerBehavior::accumulateDamage(int amount){
    damage_+=amount;
    if(damage_>=500){mode_=EnemyMode::Ending;autonomousRoute_.reset();autonomousPage_=0;host_.cancelRoutes();}
}
bool PursuerBehavior::advanceBurn(){
    const auto& rules=definition_.rules;
    if(!rules.contains("runtime_profile")||!rules.at("runtime_profile").contains("burn")){return false;}
    const auto& burn=rules.at("runtime_profile").at("burn");
    auto& session=host_.session();const int commonEvent=burn.at("common_event"),counter=burn.at("counter_source_variable");
    if(!session.flag(burn.at("activation_fact"))){return false;}
    const bool media=burn.contains("frames");auto stage=session.burnPictureStage(commonEvent);
    const auto setStage=[&](BurnPictureStage next){stage=next;session.setBurnPictureStage(commonEvent,next);};
    const auto finish=[&]{session.setVariable(counter,0);setStage(BurnPictureStage::Erase);};
    const auto sample=[&](int frame){
        if(!media){return;}
        const GridPosition body{static_cast<double>(session.variable(83)),static_cast<double>(session.variable(84))};
        if(burn.at("stop_if_both_body_screen_nonpositive").get<bool>()&&body.x<=0&&body.y<=0){finish();return;}
        samplePictureWorkspace(commonEvent,burn.at("frames").at(frame).at("offset_from_body_pixels"));
    };
    if(stage==BurnPictureStage::InitialShow){
        if(media&&!host_.pictureCommandsAllowed()){return mode_==EnemyMode::Ending;}
        if(media){presentPictureCue(burn.at("show_cue"),commonEvent);}
        setStage(BurnPictureStage::MoveFirst);sample(0);
    }else if(stage==BurnPictureStage::Waiting){
        if(session.advanceBurnWait(commonEvent,true)){
            accumulateDamage(burn.at("damage"));const auto& increment=burn.at("counter_increment");
            const int next=session.variable(counter)+host_.random(increment.at(0),increment.at(1));
            if(next>=burn.at("lifetime_threshold").get<int>()){finish();}
            else{session.setVariable(counter,next);setStage(BurnPictureStage::MoveFirst);sample(0);}
        }else{
            const int remaining=session.burnWaitRemaining(commonEvent);
            if(remaining==4){setStage(BurnPictureStage::MoveSecond);sample(1);}
            else if(remaining==2){setStage(BurnPictureStage::MoveThird);sample(2);}
            else{return mode_==EnemyMode::Ending;}
        }
    }
    if(stage==BurnPictureStage::Erase){
        if(media&&!host_.pictureCommandsAllowed()){return mode_==EnemyMode::Ending;}
        if(media){presentPictureCue(burn.at("erase_cue"),commonEvent);}
        session.setFlag(burn.at("activation_fact"),false);session.finishBurnWait(commonEvent);
        return mode_==EnemyMode::Ending;
    }
    if(media&&!host_.pictureCommandsAllowed()){return mode_==EnemyMode::Ending;}
    if(media){
        const int frame=stage==BurnPictureStage::MoveFirst?0:stage==BurnPictureStage::MoveSecond?1:2;
        const auto& definition=burn.at("frames").at(frame);presentPictureCue(definition.at("move_cue"),commonEvent);
        const int bodyY=session.variable(84);
        if(bodyY<=burn.at("hide_if_body_screen_y_at_most").get<int>()){presentPictureCue(definition.at("hide_cues").at(0),commonEvent);}
        else if(bodyY>=burn.at("hide_if_body_screen_y_at_least").get<int>()){presentPictureCue(definition.at("hide_cues").at(1),commonEvent);}
    }
    setStage(BurnPictureStage::Waiting);
    if(session.burnWaitRemaining(commonEvent)==0){session.advanceBurnWait(commonEvent,true);}
    return mode_==EnemyMode::Ending;
}
AttackContact PursuerBehavior::receiveAttack(const AttackSample& sample){
    if(!present()||mode_==EnemyMode::Dormant||damage_>=500){return {};}
    // CE4 reads the preceding E2 screen-body cache; a blocked picture never resamples motion.
    const auto body=host_.screenPicturePosition({double(host_.session().variable(83)),double(host_.session().variable(84))});
    const auto& half=definition_.rules.at("hitbox_half_pixels");
    const GridPosition target{(body.x+.5)*16,(body.y+1)*16};
    if(sample.phase!=AttackPhase::ResolvedContact&&(std::abs(sample.center.x-target.x)>sample.halfPixels[0]+half.at(0).get<int>()||
       std::abs(sample.center.y-target.y)>sample.halfPixels[1]+half.at(1).get<int>())){return {};}
    if(sample.phase==AttackPhase::Flight){if(!grenadeLauncher(sample.kind)){throw std::invalid_argument("Unsupported pursuer flight");}return {true,false,0,false};}
    if((sample.phase==AttackPhase::Impact||sample.phase==AttackPhase::ResolvedContact)&&definition_.rules.contains("runtime_profile")&&definition_.rules.at("runtime_profile").contains("extra_modes")){
        for(const auto& receiver:definition_.rules.at("runtime_profile").at("extra_modes")){
            if(receiver.at("role")!="environment-damage"||receiver.at("page")!=sample.receiverPage){continue;}
            if(damage_>=receiver.at("accept_local_damage_below").get<int>()){return {true,false,0,false};}
            const auto& profile=definition_.rules.at("runtime_profile");auto& session=host_.session();
            session.setVariable(profile.at("receiver").at("blood_type_variable"),receiver.at("blood_type"));
            session.setFlag(receiver.at("blood_pulse_fact"),false);session.setFlag(receiver.at("blood_pulse_fact"),true);host_.sound(receiver.at("sound"));
            const int amount=receiver.at("damage");session.damagePursuer(amount);accumulateDamage(amount);return {true,true,amount,false};
        }
    }
    const auto kind=host_.damageKind(sample.kind);
    const int burnFact=kind==AttackKind::GlFlame?definition_.rules.at("runtime_profile").at("burn").at("activation_fact").get<int>():0;
    if(kind==AttackKind::GlFlame&&host_.session().flag(burnFact)){return {true,false,0,false};}
    if(sample.phase==AttackPhase::Burning){
        if(sample.kind!=AttackKind::GlFlame){throw std::invalid_argument("Unsupported pursuer burning attack");}
        host_.session().setFlag(burnFact,true);return {true,true,0,false};
    }
    if(sample.receiverPage<2||sample.receiverPage>6){return {true,false,0,false};}
    switch(kind){
    case AttackKind::HandgunHp:case AttackKind::Handgun:case AttackKind::Knife:case AttackKind::Magnum:case AttackKind::De:
    case AttackKind::Shotgun:case AttackKind::SemiAutoShotgun:case AttackKind::Grenade:case AttackKind::Sg:case AttackKind::Lc9:
    case AttackKind::Ar:case AttackKind::GlNormal:case AttackKind::GlFlame:case AttackKind::GlAcid:break;
    default:throw std::invalid_argument("Unsupported pursuer weapon damage kind");
    }
    if(kind==AttackKind::GlFlame){host_.session().setFlag(burnFact,true);}
    const auto& rules=definition_.rules;
    const nlohmann::json* receiver=nullptr;
    if(rules.contains("runtime_profile")&&rules.at("runtime_profile").contains("receiver")){
        const auto& profile=rules.at("runtime_profile");receiver=&profile.at("receiver").at("pages").at(std::to_string(sample.receiverPage));
        if(receiver->contains("before_weapon_route_cue")){host_.startRoute(receiver->at("before_weapon_route_cue"));}
        host_.session().setVariable(profile.at("receiver").at("blood_type_variable"),receiver->at("blood_type"));
        host_.session().setFlag(profile.at("receiver").at("blood_fact"),false);
        host_.session().setFlag(profile.at("receiver").at("blood_fact"),true);
    }
    const auto receiverSound=[&](int command){
        if(!command||!rules.contains("runtime_profile")||!rules.at("runtime_profile").contains("cues")){return;}
        const auto& cues=rules.at("runtime_profile").at("cues");
        const std::string key="p"+std::to_string(sample.receiverPage)+"-c"+std::to_string(command);
        if(cues.contains(key)){host_.sound(cues.at(key));}
    };
    receiverSound(sample.receiverPage==5?6:sample.receiverPage==6?0:5);
    auto& session=host_.session();session.setFlag(12,false);
    const int count=session.variable(35)+1;session.setVariable(35,count);
    const int attack=auxiliaryDamage(kind)?session.auxiliaryAttack():session.attack()+(sample.aimed?host_.content().items.at(host_.content().rules.at("handgun_damage").at("aim_attack_item")).attack:0);
    const int defense=definition_.rules.at("defense");session.setVariable(49,defense);
    const auto rolled=rollWeaponDamage(kind,attack,defense,sample.advances,[&](int lo,int hi){return host_.random(lo,hi);});
    int amount=rolled.amount;
    if(kind==AttackKind::Knife){
        if(count%3==0&&host_.random(0,100)-count<=15){amount=session.auxiliaryAttack()+host_.random(0,10);}
        host_.sound("f3-ce11-c13");
    }else if(!auxiliaryDamage(kind)){
        const bool critical=session.flag(47)||(count%3==0&&host_.random(0,100)-count<=mainCriticalThreshold(kind));
        session.setFlag(47,false);
        if(critical){
            session.setVariable(35,0);session.setFlag(12,true);
            amount*=2;
        }
    }
    if(sample.receiverPage==5){receiverSound(12);}
    else if(session.flag(12)&&(sample.receiverPage==2||sample.receiverPage==3)){
        receiverSound(sample.receiverPage==2?11:12);
    }
    if(receiver&&session.flag(12)&&receiver->contains("critical_route_cue")){host_.startRoute(receiver->at("critical_route_cue"));}
    session.damagePursuer(amount);accumulateDamage(amount);
    return {true,true,amount,rolled.endRequested||(sample.receiverPage>=3&&sample.receiverPage<=5)};
}
