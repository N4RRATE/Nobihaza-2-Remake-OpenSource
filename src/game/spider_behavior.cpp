#include "spider_behavior.h"
#include "combat_scene.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

SpiderBehavior::SpiderBehavior(CombatScene& scene,const EnemyDefinition& definition)
    :definition_(definition),host_(scene,definition.id),
     acidHost_(scene,definition.rules.at("runtime_profile").at("acid_source_object")){
    const auto& p=profile();
    for(const auto* key:{"acid_source_object","body_source_object"}){
        if(host_.content().objects.at(p.at(key).get<std::string>()).map!=definition.map){throw std::logic_error("Spider companion belongs to another map");}
    }
    host_.claimAutonomy();acidHost_.claimAutonomy();
    corpse_=host_.session().flag(definition.deadFlag);lastPosition_=host_.actor().motion.position();
}
const nlohmann::json& SpiderBehavior::profile() const{return definition_.rules.at("runtime_profile");}
const ObjectVariant& SpiderBehavior::page(int n) const{
    const auto& pages=host_.content().objects.at(id()).variants;
    const auto found=std::find_if(pages.begin(),pages.end(),[&](const auto& value){return value.page==n;});
    if(found==pages.end()){throw std::runtime_error("Missing spider source page");}return *found;
}
void SpiderBehavior::applyPage(int n){
    const auto& d=page(n).definition;auto actor=host_.actor();actor.spriteId=d.spriteId;actor.layer=d.layer;
    actor.through=actor.routeThrough.value_or(d.through);actor.setSpeedLevel(d.moveSpeed);
    actor.setAnimation(CharacterAnimation(animationForSpeed(d.moveSpeed),d.initialColumn,d.animationType),actor.motion.facing());host_.setActor(actor);
}
EnemyMode SpiderBehavior::mode() const{
    if(corpse_){return EnemyMode::Corpse;}if(ending_){return EnemyMode::Ending;}if(!active_){return EnemyMode::Dormant;}
    return state_>=10?EnemyMode::HitRecovery:state_==6?EnemyMode::Rush:state_>=2?EnemyMode::Windup:EnemyMode::Pursuit;
}
bool SpiderBehavior::activateState(int state){
    if(corpse_||ending_||!((state>=0&&state<=6)||(state>=10&&state<=12))){return false;}
    active_=true;change(state);return true;
}
bool SpiderBehavior::setScriptedDamage(int damage){
    if(damage<0||corpse_){return false;}damage_=damage;if(damage_>=profile().at("death").at("damage_threshold").get<int>()){end();}return true;
}
void SpiderBehavior::change(int state){
    state_=state;phase_=wait_=0;cues_.clear();cueCursor_=0;movement_.reset();host_.cancelRoutes();
    const auto& p=profile();
    const std::string role=state==0?"roam":state==1?"pursuit":state==6?"swoop":state==10?"stun":state==11?"knockback":state==12?"explosive_knockback":"acid_prepare";
    applyPage(p.at("pages").at(role));
}
void SpiderBehavior::sequence(const nlohmann::json& cues){cues_=cues.get<std::vector<std::string>>();cueCursor_=0;wait_=0;}
bool SpiderBehavior::presentCue(const std::string& cue){
    const auto& c=host_.content().cue(cue);const std::string kind=c.at("kind");
    if(kind=="show_picture"||kind=="move_picture"||kind=="erase_picture"){
        if(!host_.pictureCommandsAllowed()){return false;}
        const auto screen=deathSample_;
        host_.present({PresentationEvent::Kind::Cue,cue,{},host_.screenPicturePosition(screen),0,id(),screen});
    }else if(kind!="wait"){host_.cue(cue);}
    return true;
}
bool SpiderBehavior::advanceSequence(){
    if(wait_>0&&--wait_>0){return false;}
    while(cueCursor_<cues_.size()){
        const auto& cue=cues_[cueCursor_];
        if(state_==6){
            const auto& swoop=profile().at("swoop");
            for(const auto* list:{"frame_changes","finish_frame_changes"}){
                for(const auto& change:swoop.at(list)){if(change.at("before_cue")==cue){jumpFrame_=change.at("frame");}}
            }
        }
        const auto& c=host_.content().cue(cue);
        if(!presentCue(cue)){return false;}++cueCursor_;
        if(c.at("kind")=="wait"){wait_=std::max(1,c.at("tenths").get<int>()*6);return false;}
        if(c.at("kind")=="animation"&&c.value("wait",false)){
            const auto& a=host_.content().media.at("animations").at(c.at("animation").get<std::string>());
            wait_=static_cast<int>(a.at("frames").size())*a.at("playback").at("logic_updates_per_frame").get<int>();return false;
        }
    }
    return true;
}
int SpiderBehavior::frontDistance() const{
    const auto actor=host_.actor();const auto a=actor.motion.tile(),b=host_.player().motion.tile();const auto facing=actor.motion.facing();
    if((facing==Direction::Up||facing==Direction::Down)?a.x!=b.x:a.y!=b.y){return -1;}
    return facing==Direction::Up?a.y-b.y:facing==Direction::Down?b.y-a.y:facing==Direction::Right?b.x-a.x:a.x-b.x;
}
void SpiderBehavior::move(){
    if(host_.routePending()){return;}const auto actor=host_.actor();
    if(host_.terrain(adjacentTile(actor.motion.tile(),actor.motion.facing()))==profile().at("motion").at("terrain_block_tag")){
        const bool vertical=actor.motion.facing()==Direction::Up||actor.motion.facing()==Direction::Down;
        const auto target=host_.player().motion.position();const auto pos=actor.motion.position();const double difference=vertical?target.x-pos.x:target.y-pos.y;
        if(difference){host_.startRoute(profile().at("motion").at("terrain_turns").at(std::string(directionName(actor.motion.facing()))).at(difference>0?"positive":"negative"));return;}
    }
    if(!movement_){movement_.emplace(page(profile().at("pages").at(state_==0?"roam":"pursuit")).autonomousRoute,id());}
    host_.advance(*movement_);
}
void SpiderBehavior::end(){
    if(ending_||corpse_){return;}deathSample_=host_.actorScreenFoot();const auto& offset=profile().at("death").at("picture_offset_pixels");deathSample_.x+=offset.at(0).get<int>();deathSample_.y+=offset.at(1).get<int>();ending_=true;host_.cancelRoutes();movement_.reset();applyPage(profile().at("death").at("page"));
    sequence(profile().at("death").at("cue_order"));shownBody_.clear();pendingBody_.clear();
}
int SpiderBehavior::calculateDamage(const AttackSample& sample){
    auto& s=host_.session();const auto kind=host_.damageKind(sample.kind);
    s.setVariable(49,definition_.rules.at("normal_attributes").at("defense"));s.setFlag(12,false);
    const int count=s.variable(35)+1;s.setVariable(35,count);
    const int attack=auxiliaryDamage(kind)?s.auxiliaryAttack():s.attack()+(sample.aimed?host_.content().items.at(host_.content().rules.at("handgun_damage").at("aim_attack_item")).attack:0);
    const auto result=rollWeaponDamage(kind,attack,s.variable(49),sample.advances,[&](int lo,int hi){return host_.random(lo,hi);});
    damageRequestedEnd_=result.endRequested;if(result.endRequested){host_.requestAttackEnd(sample.attackId);}int damage=result.amount;
    if(kind==AttackKind::Knife){
        if(count%3==0&&host_.random(0,100)-count<=15){damage=s.auxiliaryAttack()+host_.random(0,10);}host_.sound("f3-ce11-c13");
    }else if(!auxiliaryDamage(kind)){
        const bool critical=s.flag(47)||(count%3==0&&host_.random(0,100)-count<=mainCriticalThreshold(kind));s.setFlag(47,false);
        if(critical){s.setFlag(12,true);s.setVariable(35,0);damage*=profile().at("receivers").at(std::to_string(sample.receiverPage)).at("critical_multiplier").get<int>();}
    }
    s.setVariable(34,damage);damage_+=damage;return damage;
}
AttackContact SpiderBehavior::receiveAttack(const AttackSample& sample){
    if(corpse_||ending_||damage_>=profile().at("death").at("damage_threshold").get<int>()){return {};}
    const auto pos=host_.actor().motion.position();const auto& off=profile().at("center_offset_pixels"),half=profile().at("half_pixels");
    const GridPosition center{(pos.x+.5)*16+off.at(0).get<int>(),(pos.y+1)*16+off.at(1).get<int>()};
    if(std::abs(sample.center.x-center.x)>sample.halfPixels[0]+half.at(0).get<int>()||std::abs(sample.center.y-center.y)>sample.halfPixels[1]+half.at(1).get<int>()){return {};}
    if(sample.phase==AttackPhase::Flight){return {true,false,0,false};}
    if(sample.kind==AttackKind::GlFlame&&host_.burning()){return {true,false,0,false};}
    if(sample.phase==AttackPhase::Burning){host_.startBurn();return {true,true,0,false};}
    if(sample.receiverPage==1){collisionYield(host_.actor().motion.facing());return {true,true,0,false};}
    const auto& receivers=profile().at("receivers");const auto found=receivers.find(std::to_string(sample.receiverPage));
    if(found==receivers.end()){throw std::invalid_argument("Unsupported spider receiver page");}
    const auto& r=*found;if(state_>=r.at("guard_state_below").get<int>()||damage_>=r.at("guard_damage_below").get<int>()){return {true,false,0,false};}
    if(sample.kind==AttackKind::GlFlame){host_.startBurn();}
    // P2 writes blood work 101 only; it does not accidentally replace movement state.
    const int sourceSlot=profile().at("source_slot");const int blood=r.at("blood_work");
    host_.bloodType(sourceSlot,sourceSlot,blood);host_.present({PresentationEvent::Kind::Effect,definition_.rules.at("blood_effects").at(std::to_string(blood)),{},pos,0,id()});
    host_.session().setFlag(r.at("blood_pulse_fact"),false);host_.session().setFlag(r.at("blood_pulse_fact"),true);
    const int damage=calculateDamage(sample);active_=true;
    if(!r.at("next_state").is_null()){change(r.at("next_state"));}
    if(damage_>=profile().at("death").at("damage_threshold").get<int>()){end();}
    return {true,true,damage,damageRequestedEnd_};
}
void SpiderBehavior::collisionYield(Direction facing){
    if(corpse_||ending_){return;}const auto& r=profile().at("receivers").at("1");
    if(damage_>=r.at("guard_damage_below").get<int>()){return;}
    host_.session().setFlag(profile().at("attack_switch"),false);host_.startRoute(r.at("collision_yield_cues").at(std::string(directionName(facing))));
}
void SpiderBehavior::hitPlayer(){
    const auto& hit=profile().at("swoop").at("player_hit");auto& s=host_.session();
    s.setFlag(1,false);host_.sound(hit.at("sound"));s.setVariable(24,hit.at("reaction_mode"));s.setFlag(8,false);s.setFlag(8,true);
    const int attack=definition_.rules.at("normal_attributes").at("attack").get<int>()+hit.at("attack_add").get<int>();s.setVariable(23,attack);
    host_.damagePlayer(biteDamage(attack,s.defense(),host_.random(-5,2)));host_.requestSpiderReaction(sampledDirection_);s.setFlag(hit.at("commit_fact"),true);
}
void SpiderBehavior::advanceSwoop(){
    const auto& swoop=profile().at("swoop");
    if(phase_==0){sequence(swoop.at("startup_cues"));phase_=1;}
    if(phase_==1){if(!advanceSequence()){return;}host_.startRoute(swoop.at("direction_routes").at(std::string(directionName(sampledDirection_))));contacts_=0;phase_=2;}
    if(phase_==2){
        if(wait_>0&&--wait_>0){return;}
        const auto tile=host_.actor().motion.tile();const auto front=adjacentTile(tile,sampledDirection_);auto& s=host_.session();
        if(front==host_.player().motion.tile()&&std::none_of(swoop.at("guard_off").begin(),swoop.at("guard_off").end(),[&](const auto& fact){return s.flag(fact);})){hitPlayer();}
        if(++contacts_<swoop.at("contact_iterations").get<int>()){wait_=swoop.at("contact_wait_ticks");return;}
        sequence(swoop.at("finish_cues"));phase_=3;
    }
    if(phase_==3&&advanceSequence()){jumpFrame_=0;shownBody_.clear();walkingTicks_=settleTicks_=0;change(swoop.at("next_state"));}
}
void SpiderBehavior::update(const InputFrame&){
    if(corpse_){return;}if(ending_){if(advanceSequence()){host_.session().setFlag(profile().at("death").at("completed_fact"),true);ending_=false;corpse_=true;applyPage(profile().at("death").at("corpse_page"));}return;}
    if(!host_.session().flag(profile().at("activation_fact"))||host_.session().hp()<=0){return;}
    if(!active_){active_=true;change(0);}
    if(state_>=10){
        const auto& recovery=profile().at("recovery").at(std::to_string(state_));if(phase_==0){sequence(recovery.at("cue_order"));phase_=1;}
        if(advanceSequence()){change(recovery.at("next_state"));}return;
    }
    if(state_==6){advanceSwoop();return;}
    if(state_>=2){
        const auto& prep=profile().at("acid").at("prepare");if(phase_==0){
            const auto& motion=profile().at("motion");if(host_.session().flag(motion.at("burn_flash_facts").at("acid_prepare"))){host_.cue(motion.at("burn_flash_cues").at("acid_prepare"));}
            sequence(prep.at("cue_order"));phase_=1;}
        if(phase_==1){if(!advanceSequence()){return;}host_.session().setFlag(prep.at("commit_attack_fact"),true);sequence(prep.at("recovery_cues"));phase_=2;}
        if(phase_==2&&advanceSequence()){change(prep.at("next_state"));}return;
    }
    const std::string motionRole=state_==0?"roam":"pursuit";
    const auto& motion=profile().at("motion");if(host_.session().flag(motion.at("burn_flash_facts").at(motionRole))){host_.cue(motion.at("burn_flash_cues").at(motionRole));}
    const int distance=frontDistance();
    if(state_==0){
        const auto e=host_.actor().motion.position(),p=host_.player().motion.position();const auto& r=profile().at("roam"),bounds=r.at("close_screen_box_pixels");
        if((distance>=r.at("front_axis_minimum_tiles").get<int>()&&distance<=r.at("front_axis_distance_tiles").get<int>())||((e.x-p.x)*16>=bounds.at(0).get<int>()&&(e.x-p.x)*16<=bounds.at(1).get<int>()&&(e.y-p.y)*16>=bounds.at(2).get<int>()&&(e.y-p.y)*16<=bounds.at(3).get<int>())){change(r.at("next_state"));return;}
    }else{
        const auto& p=profile().at("pursuit"),range=p.at("front_axis_distance_tiles");
        if(distance>=range.at(0).get<int>()&&distance<=range.at(1).get<int>()){
            int next=distance+p.at("acid_state_distance_offset").get<int>();
            if(!host_.session().flag(p.at("swoop_guard_off"))){
                const auto& random=p.at("random_inclusive");const int roll=host_.random(random.at(0),random.at(1));
                if(roll>=p.at("swoop_at_least").get<int>()&&!p.at("swoop_state_overwritten").get<bool>()){next=p.at("swoop_next_state");}
            }
            change(next);return;
        }
    }
    move();
}
void SpiderBehavior::advanceAcid(){
    const auto& acid=profile().at("acid");auto& s=host_.session();
    const auto& object=host_.content().objects.at(profile().at("acid_source_object").get<std::string>());
    const auto* selected=host_.content().activeVariant(object,s);
    if(acidPhase_!=0&&(!selected||selected->page!=acid.at("manager_page"))){acidHost_.cancelRoutes();acidPhase_=acidWait_=0;return;}
    if(acidPhase_==0){
        if(!s.flag(acid.at("clear_attack_fact"))){return;}
        if(!selected||selected->page!=acid.at("manager_page")){return;}
        auto actor=acidHost_.actor();const auto& d=selected->definition;actor.layer=d.layer;actor.through=d.through;actor.spriteId=d.spriteId;actor.setSpeedLevel(d.moveSpeed);
        actor.motion=CharacterMotion(sampledTile_,host_.actor().motion.facing(),actor.motion.speed());acidHost_.setActor(actor);
        acidHost_.sound(acid.at("launch_sound"));acidHost_.cue(acid.at("direction_cues").at(std::string(directionName(host_.actor().motion.facing()))));acidWait_=1;acidPhase_=1;return;
    }
    if(acidWait_>0&&--acidWait_>0){return;}
    if(acidPhase_==1){
        const auto& routes=acid.at("distance_routes");const auto route=routes.find(std::to_string(state_-1));
        if(route!=routes.end()){acidHost_.startRoute(*route);acidWait_=acid.at("travel_wait_ticks");}acidPhase_=2;return;
    }
    if(acidPhase_==2){
        for(const auto& cue:acid.at("impact_cues")){acidHost_.cue(cue);}
        const auto& hit=acid.at("hit_rule");
        const auto player=host_.player().motion.position(),actor=acidHost_.actor().motion.position();
        const int dx=static_cast<int>((actor.x-player.x)*16)+hit.at("projectile_offset_pixels").at(0).get<int>()-hit.at("player_center_offset_pixels").at(0).get<int>();
        const int dy=static_cast<int>((actor.y-player.y)*16)+hit.at("projectile_offset_pixels").at(1).get<int>()-hit.at("player_center_offset_pixels").at(1).get<int>();
        bool contact;
        if(hit.contains("screen_axis_bounds")){
            const auto& b=hit.at("screen_axis_bounds");s.setVariable(194,dy);contact=dx>=b.at(0).get<int>()&&dx<=b.at(1).get<int>()&&dy>=b.at(2).get<int>()&&dy<=b.at(3).get<int>();
        }else{
            // Four original acid pages use a signed sum; opposite offsets can cancel.
            const int sum=static_cast<int>((actor.x-player.x+actor.y-player.y)*16)+hit.at("projectile_offset_pixels").at(0).get<int>()+hit.at("projectile_offset_pixels").at(1).get<int>()-hit.at("player_center_offset_pixels").at(0).get<int>()-hit.at("player_center_offset_pixels").at(1).get<int>();
            const auto& b=hit.at("signed_screen_sum_bounds");s.setVariable(194,sum);contact=sum>=b.at(0).get<int>()&&sum<=b.at(1).get<int>();
        }
        if(contact){
            const int attack=definition_.rules.at("normal_attributes").at("attack");s.setVariable(23,attack);acidHost_.cue(hit.at("player_flash_cue"));host_.damagePlayer(biteDamage(attack,s.defense(),host_.random(-5,2)));
            const auto& range=hit.at("poison_random_inclusive");if(host_.random(range.at(0),range.at(1))>=hit.at("poison_at_least").get<int>()){s.startPoison();}
        }
        acidWait_=acid.at("finish_wait_ticks");acidPhase_=3;return;
    }
    if(acidPhase_==3){
        auto actor=acidHost_.actor();const auto& tile=acid.at("finish_tile");actor.motion=CharacterMotion({tile.at(0),tile.at(1)},actor.motion.facing(),actor.motion.speed());acidHost_.setActor(actor);acidWait_=acid.at("finish_second_wait_ticks");acidPhase_=4;return;
    }
    s.setFlag(acid.at("clear_attack_fact"),false);acidPhase_=0;
}
void SpiderBehavior::advanceBody(){
    if(corpse_||ending_){return;}const auto& body=profile().at("body_presentation");
    const bool jumping=state_==6;const bool hit=state_>=7;const std::string role=jumping?"jump":hit?"hit":"normal";
    if(!pendingBodyRole_.empty()&&pendingBodyRole_!=role){pendingBody_.clear();shownBody_.clear();walkingTicks_=settleTicks_=0;}
    if(!pendingBody_.empty()){if(!host_.pictureCommandsAllowed()){return;}for(auto& event:pendingBody_){host_.present(std::move(event));}pendingBody_.clear();return;}
    const auto actor=host_.actor();sampledDirection_=actor.motion.facing();sampledTile_=actor.motion.tile();pendingBodyRole_=role;
    const std::string direction(directionName(sampledDirection_));const std::string shown=role+direction;
    auto screen=host_.actorScreenFoot();const auto& off=body.at("offset_pixels_by_direction").at(direction);screen.x+=off.at(0).get<int>();screen.y+=off.at(1).get<int>();
    const auto emit=[&](const std::string& cue,GridPosition position){pendingBody_.push_back({PresentationEvent::Kind::Cue,cue,{},host_.screenPicturePosition(position),0,id(),position});};
    const auto flush=[&]{if(host_.pictureCommandsAllowed()){for(auto& event:pendingBody_){host_.present(std::move(event));}pendingBody_.clear();}};
    if(shownBody_!=shown){for(const auto& frame:body.at(role+"_frames").at(direction)){emit(frame.at("show_cue"),screen);}shownBody_=shown;}
    if(jumping){const auto& stage=body.at("jump_stages").at(jumpFrame_);const auto& extra=stage.at("extra_offset_pixels");screen.x+=extra.at(0).get<int>();screen.y+=extra.at(1).get<int>();for(const auto& cue:stage.at("cue_order")){emit(cue,screen);}flush();return;}
    const auto pos=actor.motion.position();if(pos.x!=lastPosition_.x||pos.y!=lastPosition_.y){settleTicks_=body.at("movement_settle_ticks");lastPosition_=pos;}
    if(settleTicks_>0){--settleTicks_;walkingTicks_=walkingTicks_%body.at("movement_settle_ticks").get<int>()+1;}else{walkingTicks_=0;}
    const auto& hide=body.at(hit?"hit_hide_cues":"hide_cues");for(const auto& cue:hide){emit(cue,screen);}
    const auto& stages=body.at("walking_stages");
    const auto selected=std::find_if(stages.begin(),stages.end(),[&](const auto& stage){return walkingTicks_<=stage.at("through_tick").template get<int>();});
    std::string cue=selected==stages.end()?stages.at(0).at("move_cue").get<std::string>():selected->at("move_cue").get<std::string>();
    if(hit){for(const auto& [key,value]:body.at("normal_move_cues").items()){if(value==cue){cue=body.at("hit_move_cues").at(key);break;}}}
    emit(cue,screen);flush();
}
void SpiderBehavior::updateParallel(){
    advanceBody();advanceAcid();const int damage=host_.advanceBurn();if(damage&&!corpse_&&!ending_){damage_+=damage;if(damage_>=profile().at("death").at("damage_threshold").get<int>()){end();}}
}
