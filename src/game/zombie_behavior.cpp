#include "zombie_behavior.h"
#include "combat_scene.h"
#include "weapon_controller.h"
#include "combat_rules.h"
#include <algorithm>
#include <cmath>
ZombieBehavior::ZombieBehavior(CombatScene& scene,const EnemyDefinition& definition):definition_(definition),host_(scene,definition.id),id_(definition.id){
    damage_=definition.story?host_.session().variable(89):0;
    if(host_.session().flag(definition.deadFlag)){mode_=EnemyMode::Corpse;}
}
const nlohmann::json& ZombieBehavior::rules() const{return definition_.rules;}
std::string ZombieBehavior::sprite(const std::string& role) const{return rules().at("runtime_profile").at("sprites").at(role);}
std::string ZombieBehavior::cue(const std::string& role) const{return rules().at("runtime_profile").at("cues").at(role);}
bool ZombieBehavior::present() const{
    const auto& r=rules();return host_.content().objects.at(id_).map==host_.map()&&(!r.contains("activation_fact")||r.at("activation_fact").is_null()||host_.session().flag(r.at("activation_fact")));
}
EnemyMode ZombieBehavior::mode() const{return mode_;}
int ZombieBehavior::accumulatedDamage() const{return damage_;}
int ZombieBehavior::sourceState() const{
    if(mode_==EnemyMode::Arrival){return rules().at("arrival").at("mode");}
    if(fallenState_){return 100;}
    if(mode_==EnemyMode::Rush){return 2;}
    if(mode_==EnemyMode::Approach||mode_==EnemyMode::Windup){return attackChoice_;}
    return mode_==EnemyMode::Pursuit?0:-1;
}
bool ZombieBehavior::holdsPlayer() const{return mode_==EnemyMode::Grab&&host_.holdsPlayer();}
void ZombieBehavior::setSprite(const std::string& sprite,int layer){auto e=host_.actor();e.spriteId=sprite;e.layer=layer;host_.setActor(e);}
void ZombieBehavior::resumePursuit(){
    mode_=EnemyMode::Pursuit;wait_=0;pursuit_.reset();variantRoute_.reset();applyPage(rules().at("runtime_profile").at("pursuit_page"),true);
}
void ZombieBehavior::beginAttack(bool deferRoute){
    const auto& r=rules();fallenState_=false;attackChoice_=host_.random(1,2);
    if(attackChoice_==2&&r.at("runtime_profile").contains("rush")){startRush();return;}
    if(r.at("runtime_profile").contains("bite_page")){applyPage(r.at("runtime_profile").at(attackChoice_==1?"bite_page":"grab_approach_page"),true);}
    mode_=EnemyMode::Approach;phase_=deferRoute?-1:0;
    const auto& profile=r.at("runtime_profile");
    if(attackChoice_==2&&profile.contains("contact_grab_page")){
        const int page=profile.at("contact_grab_page");applyPage(page,true);variantRoute_.reset();
        for(const auto& v:host_.content().objects.at(id_).variants){if(v.page==page){variantRoute_.emplace(v.autonomousRoute,id_);break;}}
        if(!variantRoute_){throw std::runtime_error("Missing contact grab route");}wait_=0;return;
    }
    if(deferRoute){wait_=0;return;}
    if(r.contains("rescue")){wait_=0;return;}
    host_.startRoute(attackChoice_==1?cue("approach_bite"):cue("approach_grab"));wait_=r.at("approach_wait_tenths").get<int>()*6;
}
void ZombieBehavior::bite(){
    if(host_.session().flag(13)||host_.session().hp()==0){return;}
    const int defense=host_.session().defense();
    int attack=rules().at("bite_attack");
    if(rules().contains("attack_sources")){
        const auto& profile=rules().at("runtime_profile");const int page=holdsPlayer()?attackPage_:profile.at("bite_page").get<int>();
        attack=rules().at("attack_sources").at(std::to_string(page)).at("attack");host_.session().setVariable(23,attack);
    }
    const int n=biteDamage(attack,defense,host_.random(-5,2));
    host_.sound(holdsPlayer()?cue("grab_sound"):cue("bite_sound"));
    host_.present({PresentationEvent::Kind::Effect,"blood-small",{},host_.player().motion.position(),0});
    host_.damagePlayer(n);
}
bool ZombieBehavior::beginGrab(){
    if(host_.session().flag(13)||host_.session().hp()<=0||adjacentTile(host_.actor().motion.tile(),host_.actor().motion.facing())!=host_.player().motion.tile()){
        resumePursuit();wait_=0;pursuit_.reset();return false;
    }
    host_.cancelWeapon();host_.cancelRoutes(true);mode_=EnemyMode::Grab;phase_=0;bites_=0;escapeInputs_=0;wait_=7;
    auto e=host_.actor();auto p=host_.player();const auto direction=e.motion.facing();
    p.motion=CharacterMotion(p.motion.tile(),static_cast<Direction>((static_cast<int>(direction)+2)%4),32);
    p.spriteId=sprite("player_grab");host_.setPlayer(p);
    e.motion=CharacterMotion(p.motion.tile(),direction,8);e.spriteId=sprite("grab");e.layer=direction==Direction::Up?2:1;e.setAnimation(CharacterAnimation(animationForSpeed(2),1,1),direction);host_.setActor(e);
    const auto& profile=rules().at("runtime_profile");
    if(profile.contains("grab_pose_routes")){
        const int page=profile.at(direction==Direction::Up?"grab_back_page":"grab_front_page");attackPage_=page;applyPage(page,true);variantRoute_.reset();
        for(const auto& v:host_.content().objects.at(id_).variants){if(v.page==page&&!v.autonomousRoute.steps.empty()){variantRoute_.emplace(v.autonomousRoute,id_);}}
        wait_=host_.content().cue(profile.at("grab_open_wait").get<std::string>()).at("tenths").get<int>()*6;phase_=-1;
    }
    host_.session().setFlag(10,true);host_.session().setFlag(1,false);return true;
}
void ZombieBehavior::releaseGrab(){
    if(!holdsPlayer()){return;}host_.releaseGrab();variantRoute_.reset();
    host_.session().startRecovery(30,90);host_.session().setFlag(1,true);
    host_.session().setVariable(31,1);host_.cue("ce9-c8",false);host_.startRoute("ce9-c9",true);
    // P13/P14 C83 has already waited in the grab pose. P15 now changes
    // presentation and starts retreat concurrently with the player's CE10.
    mode_=EnemyMode::Retreat;retreatSteps_=0;wait_=0;
    const auto& profile=rules().at("runtime_profile");
    applyPresentationPage("retreat_page");
    const auto e=host_.actor();
    host_.cue(profile.at("retreat_flash"));
    host_.startRoute(profile.at("retreat_routes").at(std::string(directionName(e.motion.facing()))));
}
void ZombieBehavior::interruptGrab(){
    if(!holdsPlayer()){host_.releaseGrab();return;}host_.releaseGrab();variantRoute_.reset();
    // P13/P14 C36-55 exit early: CE5 + invulnerability, not CE10/P15.
    host_.session().setFlag(10,false);host_.session().setFlag(1,true);
    host_.session().startRecovery(0,90);host_.cancelWeapon();
    applyPresentationPage("pursuit_page");
}
void ZombieBehavior::applyPresentationPage(const std::string& role){
    const int pageId=rules().at("runtime_profile").at(role);
    applyPage(pageId);
}
void ZombieBehavior::applyPage(int pageId,bool preserveFacing){
    const auto& variants=host_.content().objects.at(id_).variants;
    const auto found=std::find_if(variants.begin(),variants.end(),[&](const auto& v){return v.page==pageId;});
    if(found==variants.end()){throw std::runtime_error("Missing enemy presentation page: "+std::to_string(pageId));}
    const auto& page=found->definition;
    auto e=host_.actor();
    e.spriteId=page.spriteId;e.layer=page.layer;e.setSpeedLevel(page.moveSpeed);
    e.setAnimation(CharacterAnimation(animationForSpeed(page.moveSpeed),page.initialColumn,page.animationType),preserveFacing?e.motion.facing():directionFromName(page.direction));
    host_.setActor(e);
}
void ZombieBehavior::update(const InputFrame& input){
    if(!present()||host_.session().hp()==0||mode_==EnemyMode::Corpse){return;}
    if(mode_==EnemyMode::Arrival){return;}
    if(mode_==EnemyMode::Ending){
        if(rules().contains("rescue")){return;}
        if(definition_.story&&host_.session().flag(definition_.deadFlag)){
            const auto* page=host_.content().activeVariant(host_.content().objects.at(id_),host_.session());
            if(!page){throw std::logic_error("Missing story corpse page");}applyPage(page->page,true);mode_=EnemyMode::Corpse;
        }else if(!definition_.story||bodyEnding_){updateEnding();}return;
    }
    if(!host_.session().flag(18)||(definition_.story&&!host_.session().flag(52))){return;}
    if(mode_==EnemyMode::PendingGrab){return;}
    if(host_.session().flag(10)&&!holdsPlayer()&&mode_!=EnemyMode::Retreat){
        if(mode_==EnemyMode::Approach||mode_==EnemyMode::Windup){resumePursuit();wait_=0;pursuit_.reset();}
        const auto& profile=rules().at("runtime_profile");
        if(profile.contains("grabbed_pursuit_page")&&(mode_==EnemyMode::Dormant||mode_==EnemyMode::Pursuit)){
            if(mode_==EnemyMode::Dormant){resumePursuit();}
            if(!grabbedPursuit_){grabbedPursuit_=true;variantRoute_.reset();applyPage(profile.at("grabbed_pursuit_page"),true);}
            advancePursuit(profile.at("grabbed_pursuit_page"),variantRoute_);
        }
        return;
    }
    if(grabbedPursuit_&&!host_.session().flag(10)){grabbedPursuit_=false;variantRoute_.reset();resumePursuit();}
    if(mode_==EnemyMode::Grab){
        if(variantRoute_){host_.advance(*variantRoute_);}
        for(auto action:{InputAction::Up,InputAction::Right,InputAction::Down,InputAction::Left}){if(input.buttons[static_cast<std::size_t>(action)].pressed){++escapeInputs_;break;}}
    }
    if(mode_==EnemyMode::Fallen){
        if(variantRoute_){host_.advance(*variantRoute_);}
        if(wait_>0&&--wait_>0){return;}
        const auto& f=rules().at("runtime_profile").at("fall");if(f.contains("rise_sound")){host_.sound(f.at("rise_sound"));}if(f.contains("rise_route")){host_.startRoute(f.at("rise_route"));}
        grounded_=fallenState_=false;variantRoute_.reset();applyPage(rules().at("runtime_profile").at(rules().at("runtime_profile").contains("grab_approach_page")?"grab_approach_page":"pursuit_page"),true);
        attackChoice_=2;mode_=EnemyMode::Approach;host_.startRoute(cue("approach_grab"));wait_=rules().at("approach_wait_tenths").get<int>()*6;return;
    }
    if(mode_==EnemyMode::Approach&&attackChoice_==2&&rules().at("runtime_profile").contains("contact_grab_page")){
        const bool done=host_.advance(*variantRoute_);const auto player=host_.player().motion.tile();const auto actor=host_.actor();
        const bool bumped=input.movement&&adjacentTile(player,*input.movement)==actor.motion.tile();
        if((variantRoute_->contactedPlayer()||bumped)&&adjacentTile(actor.motion.tile(),actor.motion.facing())==player&&!host_.session().flag(13)){
            variantRoute_.reset();mode_=EnemyMode::PendingGrab;host_.requestGrab();return;
        }
        if(done){variantRoute_.reset();resumePursuit();}return;
    }
    if(mode_==EnemyMode::Rush){updateRush();return;}
    if(wait_>0){--wait_;return;}
    auto e=host_.actor();const auto player=host_.player().motion.tile();
    const bool front=adjacentTile(e.motion.tile(),e.motion.facing())==player;
    switch(mode_){
    case EnemyMode::Dormant:applyPresentationPage("pursuit_page");resumePursuit();return;
    case EnemyMode::Pursuit:{
        const auto& profile=rules().at("runtime_profile");
        if(profile.contains("fall")&&!fallenOnce_&&damage_>=profile.at("fall").at("damage_threshold").get<int>()){
            const auto& f=profile.at("fall");fallenOnce_=grounded_=fallenState_=true;mode_=EnemyMode::Fallen;
            host_.cancelRoutes();pursuit_.reset();host_.sound(f.at("start_sound"));applyPage(f.at("page"),true);
            for(const auto& v:host_.content().objects.at(id_).variants){if(v.page==f.at("page")){variantRoute_.emplace(v.autonomousRoute,id_);}}
            wait_=host_.content().cue(f.at("wait").get<std::string>()).at("tenths").get<int>()*6;return;
        }
        bool rescueAttack=false;bool deferredAttack=false;
        if(rules().contains("rescue")||profile.value("acquire_while_moving",false)||!e.motion.moving()){
            const auto frontTile=adjacentTile(e.motion.tile(),e.motion.facing());
            const auto side=static_cast<Direction>((static_cast<int>(e.motion.facing())+1)%4);
            const auto other=static_cast<Direction>((static_cast<int>(e.motion.facing())+3)%4);
            const bool acquire=frontTile==player||adjacentTile(frontTile,side)==player||adjacentTile(frontTile,other)==player;
            if(acquire&&(rules().contains("rescue")||profile.value("acquire_during_invulnerability",false)||!host_.session().flag(13))){if(rules().contains("rescue")){rescueAttack=true;}else if(profile.value("acquire_while_moving",false)){deferredAttack=true;beginAttack(true);}else{beginAttack();return;}}
        }
        if(profile.contains("rush")&&rushInRange()){
            host_.sound(profile.at("rush").at("trigger_sounds").at(std::string(directionName(e.motion.facing()))));startRush();return;
        }
        if(rules().contains("rescue")){
            const auto front=adjacentTile(e.motion.tile(),e.motion.facing());
            if(host_.terrain(front)==3){
                const auto enemyPosition=e.motion.position(),playerPosition=host_.player().motion.position();
                const bool vertical=e.motion.facing()==Direction::Up||e.motion.facing()==Direction::Down;
                const double delta=vertical?playerPosition.x-enemyPosition.x:playerPosition.y-enemyPosition.y;
                if(delta!=0){const auto& avoid=profile.at("terrain_routes").at(std::string(directionName(e.motion.facing())));host_.startRoute(avoid.at(delta>0?"positive":"negative"));}
            }
            const auto& crisis=rules().at("rescue");const auto target=host_.targetTile(crisis.at("npc"));const auto here=e.motion.tile();
            const auto& cues=crisis.at("enemy_target").at("chase_cue_ids");
            if(here.x>target.x){host_.startRoute(cues.at(0));}else if(here.x<target.x){host_.startRoute(cues.at(1));}
            if(here.y>target.y){host_.startRoute(cues.at(2));}else if(here.y<target.y){host_.startRoute(cues.at(3));}
            wait_=crisis.at("enemy_target").at("chase_wait_tenths").get<int>()*6;if(rescueAttack){beginAttack();}return;
        }
        if(!profile.contains("terrain_routes")&&host_.routePending()){return;}
        const auto ahead=adjacentTile(e.motion.tile(),e.motion.facing());
        if(host_.terrain(ahead)==3){
            const bool vertical=e.motion.facing()==Direction::Up||e.motion.facing()==Direction::Down;
            const auto ep=e.motion.position(),pp=host_.player().motion.position();
            const double delta=vertical?pp.x-ep.x:pp.y-ep.y;
            if(delta&&profile.contains("terrain_routes")){host_.startRoute(profile.at("terrain_routes").at(std::string(directionName(e.motion.facing()))).at(delta>0?"positive":"negative"));return;}
            if(delta){RouteDefinition side;side.frequency=6;side.steps.push_back({std::string(directionName(vertical?(delta>0?Direction::Right:Direction::Left):(delta>0?Direction::Down:Direction::Up))),{},{},0});host_.route(side);return;}
        }
        if(deferredAttack||host_.routePending()){return;}
        if(!pursuit_){
            for(const auto& v:host_.content().objects.at(id_).variants){if(v.page==rules().at("runtime_profile").at("pursuit_page")){pursuit_.emplace(v.autonomousRoute,id_);break;}}
        }
        if(pursuit_){host_.advance(*pursuit_);}return;
    }
    case EnemyMode::Approach:
        if(phase_==-1){phase_=0;host_.startRoute(attackChoice_==1?cue("approach_bite"):cue("approach_grab"));wait_=rules().at("approach_wait_tenths").get<int>()*6;return;}
        if(!front){resumePursuit();wait_=0;pursuit_.reset();return;}
        mode_=EnemyMode::Windup;phase_=0;host_.sound(cue("windup_sound"));setSprite(sprite("windup1"));wait_=6;return;
    case EnemyMode::Windup:
        if(phase_==0){setSprite(sprite("windup2"));wait_=6;++phase_;return;}
        if(phase_==1){setSprite(sprite("windup3"));wait_=1;++phase_;return;}
        if(front&&!host_.session().flag(13)){
            if(attackChoice_==2){mode_=EnemyMode::PendingGrab;host_.requestGrab();return;}bite();
        }
        mode_=EnemyMode::Cooldown;wait_=rules().at("bite_cooldown_wait_tenths").get<int>()*6;return;
    case EnemyMode::HitRecovery:
        if(host_.routePending()){return;}
        resumePursuit();return;
    case EnemyMode::Cooldown:setSprite(sprite("recover"));recoverAway();return;
    case EnemyMode::Evade:resumePursuit();pursuit_.reset();return;
    case EnemyMode::Grab:
        if(phase_==-1){
            const auto& profile=rules().at("runtime_profile");const bool back=host_.actor().motion.facing()==Direction::Up;
            host_.startRoute(profile.at("grab_pose_routes").at(back?"back":"front"));
            wait_=host_.content().cue(profile.at("grab_pose_wait").get<std::string>()).at("tenths").get<int>()*6;phase_=0;return;
        }
        if(phase_==2){releaseGrab();return;}
        if(phase_==1){
            if(bites_>=rules().at("grab_max_bites").get<int>()||escapeInputs_>=rules().at("grab_escape_direction_inputs").get<int>()){
                phase_=2;
                const auto& profile=rules().at("runtime_profile");
                wait_=std::max(1,host_.content().cue(profile.at("grab_release_wait").get<std::string>()).at("tenths").get<int>()*6);
                return;
            }
            phase_=0;
        }
        bite();++bites_;
        if(host_.session().hp()==0){return;}
        phase_=1;
        wait_=rules().at("grab_bite_wait_tenths").get<int>()*6;return;
    case EnemyMode::Retreat:
        host_.yieldBehind();
        if(++retreatSteps_>=rules().at("retreat_wait0_loop_limit").get<int>()){
            host_.cancelRoutes();
            e=host_.actor();e.setSpeedLevel(2);e.layer=1;host_.setActor(e);
            resumePursuit();pursuit_.reset();
        }
        return;
    default:return;
    }
}
AttackContact ZombieBehavior::receiveAttack(const AttackSample& sample){
    const auto center=sample.center;const auto kind=host_.damageKind(sample.kind);const int advances=sample.advances;const bool aimed=sample.aimed;
    const auto& r=rules();const bool dormantReceiver=r.contains("dormant_receive_activation")&&host_.map()==host_.content().objects.at(id_).map;
    if((!present()&&!dormantReceiver)||mode_==EnemyMode::Corpse||mode_==EnemyMode::Ending||mode_==EnemyMode::Arrival||damage_>=r.at("death_damage_threshold").get<int>()){return {};}
    auto e=host_.actor();const auto position=e.motion.position();const GridPosition target{(position.x+.5)*16,(position.y+1)*16-8};
    if(std::abs(center.x-target.x)>sample.halfPixels[0]+8||std::abs(center.y-target.y)>sample.halfPixels[1]+8){return {};}
    if(sample.phase==AttackPhase::Flight){return {true,false,0,false};}
    if(dormantReceiver){
        const auto& activation=r.at("dormant_receive_activation");const auto& pages=activation.at("receiver_pages");
        if(std::find(pages.begin(),pages.end(),sample.receiverPage)!=pages.end()){host_.session().setFlag(activation.at("fact"),true);}
        else if(!present()){return {true,false,0,false};}
    }
    if(kind==AttackKind::GlFlame&&host_.burning()){return {true,false,0,false};}
    if(sample.phase==AttackPhase::Burning){host_.startBurn();return {true,true,0,false};}
    if(kind==AttackKind::GlFlame){host_.startBurn();}
    const auto& profile=rules().at("runtime_profile");
    if(fallenState_&&(sample.receiverPage==2||sample.receiverPage==3)&&(profile.at("fall").contains("auxiliary_flag")?!host_.session().flag(profile.at("fall").at("auxiliary_flag")):kind!=AttackKind::Knife)){return {true,true,0,false};}
    if(sample.receiverPage==6){
        const int before=damage_;damage_=2000;host_.session().setFlag(12,true);
        if(holdsPlayer()||mode_==EnemyMode::PendingGrab){interruptGrab();}
        if(profile.value("grenade_clear_attack",false)){attackChoice_=0;}
        host_.present({PresentationEvent::Kind::Effect,(profile.contains("p6_blood_effect")?profile.at("p6_blood_effect"):profile.at("blood_effects").at("3")),{},position,0,id_});
        mode_=EnemyMode::Ending;bodyEnding_=true;headEnding_=false;phase_=-1;wait_=0;if(rules().contains("rescue")){beginRescueEnding();}return {true,true,damage_-before,false};
    }
    const bool groundHit=grounded_;
    const bool knockback=sample.receiverPage==3||sample.receiverPage==4;
    if(groundHit&&sample.receiverPage==3&&profile.contains("grounded_receiver_3_clear_flags")){for(const auto& flag:profile.at("grounded_receiver_3_clear_flags")){host_.session().setFlag(flag.get<int>(),false);}}
    const bool interrupted=groundHit&&sample.receiverPage==3?profile.at("fall").value("grounded_receiver_3_interrupts_attack",true):knockback||(sample.receiverPage==2&&profile.value("receiver_2_interrupts_attack",false));
    if(interrupted){
        if(holdsPlayer()||mode_==EnemyMode::PendingGrab){interruptGrab();}mode_=EnemyMode::HitRecovery;wait_=0;pursuit_.reset();
    }
    if(interrupted&&grounded_){grounded_=false;variantRoute_.reset();applyPage(profile.at("pursuit_page"),true);}
    if(groundHit&&sample.receiverPage<=3){host_.startRoute(profile.at("fall").at(sample.receiverPage==3?"grounded_knockback_route":"grounded_hit_route"));}
    else{host_.startRoute(cue(sample.receiverPage==4&&profile.at("cues").contains("large_knockback_route")?"large_knockback_route":knockback?"knockback_route":"normal_hit_route"));}

    const int damageBefore=damage_;
    if(profile.contains("blood_effects")&&sample.receiverPage!=5){
        int type=groundHit&&sample.receiverPage<=3?(sample.receiverPage==2?11:12):sample.receiverPage==2?1:sample.receiverPage==3?2:3;
        if(profile.contains("blood_slot")){
            const int slot=profile.at("blood_slot");const auto& writes=profile.at("blood_write_slots");
            type=host_.bloodType(writes.value(std::to_string(sample.receiverPage),slot),slot,type);
        }
        if(type){host_.present({PresentationEvent::Kind::Effect,profile.at("blood_effects").at(std::to_string(type)),{},position,0,id_});}
    }
    bool critical=false,endRequested=false;
    if(sample.receiverPage==5){damage_=150;critical=true;host_.session().setFlag(12,true);}
    else{
        host_.session().setFlag(12,false);const int count=host_.session().variable(35)+1;host_.session().setVariable(35,count);
        const int effectiveAttack=auxiliaryDamage(kind)?host_.session().auxiliaryAttack():host_.session().attack()+(aimed?host_.content().items.at(host_.content().rules.at("handgun_damage").at("aim_attack_item")).attack:0);
        host_.session().setVariable(49,rules().at("defense"));
        const auto rolled=rollWeaponDamage(kind,effectiveAttack,rules().at("defense"),advances,[&](int lo,int hi){return host_.random(lo,hi);});
        int amount=rolled.amount;endRequested=rolled.endRequested;
        if(kind==AttackKind::Knife){
            if(count%3==0&&host_.random(0,100)-count<=15){amount=host_.session().auxiliaryAttack()+host_.random(0,10);}
            host_.sound("f3-ce11-c13");
        }else if(!auxiliaryDamage(kind)){
            critical=host_.session().flag(47)||(count%3==0&&host_.random(0,100)-count<=mainCriticalThreshold(kind));
            if(host_.session().flag(47)){host_.session().setFlag(47,false);}
        }
        damage_+=amount;
        if(!critical){if(kind!=AttackKind::Knife&&!groundHit){host_.sound(cue("hit_sound"));}if(!profile.contains("blood_effects")){host_.present({PresentationEvent::Kind::Effect,"blood-small",{},position,0});}}
        if(critical){host_.session().setFlag(12,true);host_.session().setVariable(35,0);damage_=150;}
    }
    if(damage_>=rules().at("death_damage_threshold").get<int>()){beginDamageEnding(critical);}
    return {true,true,damage_-damageBefore,endRequested};
}
void ZombieBehavior::beginDamageEnding(bool critical){
    const auto& profile=rules().at("runtime_profile");
        if(holdsPlayer()||mode_==EnemyMode::PendingGrab){interruptGrab();}
        mode_=EnemyMode::Ending;grounded_=fallenState_=false;variantRoute_.reset();phase_=0;headEnding_=critical&&(definition_.story||host_.session().flag(12));
        host_.cancelRoutes();
        if(rules().contains("rescue")){beginRescueEnding();return;}
        if(!definition_.story&&profile.contains("bite_page")){applyPage(profile.at(headEnding_?"head_page":"normal_death_page"),true);}
        else{setSprite(headEnding_?sprite("head_transition"):sprite("normal_end"),0);}
        if(definition_.story){host_.storyEnding(headEnding_?2:1);}
        else if(headEnding_){
            const auto& profile=rules().at("runtime_profile");const auto& animation=host_.content().cue(profile.at("head_animation").get<std::string>());
            if(profile.contains("head_start_route")){host_.startRoute(profile.at("head_start_route"));}
            host_.cue(profile.at("head_animation"));
            const auto& data=host_.content().media.at("animations").at(animation.at("animation").get<std::string>());
            wait_=static_cast<int>(data.at("frames").size())*data.at("playback").at("logic_updates_per_frame").get<int>();
        }else{host_.sound(cue("normal_death_sound"));wait_=rules().at("runtime_profile").at("normal_death_wait_tenths").get<int>()*6;}

}

void ZombieBehavior::updateDeathPresentation(){
    if(!holdsPlayer()||!present()){return;}
    if(feedingWait_>0){--feedingWait_;return;}
    if(!rules().at("runtime_profile").contains("feeding")){return;}
    const auto& stages=rules().at("runtime_profile").at("feeding");
    if(stages.empty()){return;}
    const auto& stage=stages.at(feedingStage_);
    host_.startRoute(stage.at("route"));
    feedingWait_=stage.at("wait_tenths").get<int>()*6;
    if(stage.contains("sound")){
        host_.sound(stage.at("sound"));host_.present({PresentationEvent::Kind::Effect,"blood-large",{},host_.player().motion.position(),0});
    }
    feedingStage_=(feedingStage_+1)%static_cast<int>(stages.size());
}

void ZombieBehavior::recoverAway(){
    const auto& profile=rules().at("runtime_profile");
    if(!profile.contains("evade_routes")){resumePursuit();pursuit_.reset();return;}
    const auto e=host_.actor().motion.tile();const auto p=host_.player().motion.tile();
    std::optional<Direction> direction;
    if(e.x<p.x){direction=Direction::Left;}else if(e.x>p.x){direction=Direction::Right;}
    else if(e.y<p.y){direction=Direction::Up;}else if(e.y>p.y){direction=Direction::Down;}
    if(direction){host_.startRoute(profile.at("evade_routes").at(std::string(directionName(*direction))));}
    mode_=EnemyMode::Evade;wait_=12;
}
void ZombieBehavior::updateEnding(){
    if(wait_>0){--wait_;return;}
    const auto& profile=rules().at("runtime_profile");
    if(bodyEnding_){
        if(phase_==-1){
            if(!host_.session().flag(12)){damage_=100;bodyEnding_=false;phase_=0;if(definition_.story){host_.storyEnding(1);return;}applyPage(profile.at("normal_death_page"),true);host_.sound(cue("normal_death_sound"));wait_=profile.at("normal_death_wait_tenths").get<int>()*6;return;}
            if(profile.contains("body_cancel")){host_.cue(profile.at("body_cancel"));}if(profile.contains("body_sound")&&!profile.at("body_sound").is_null()){host_.sound(profile.at("body_sound"));}applyPage(profile.at("body_page"),true);host_.startRoute(profile.at("body_start_route"));wait_=host_.content().cue(profile.at("body_wait").get<std::string>()).at("tenths").get<int>()*6;phase_=0;return;
        }
        host_.startRoute(profile.at("body_final_route"));damage_=2100;bodyEnding_=false;if(definition_.story){host_.storyEnding(3);return;}
    }
    if(headEnding_&&phase_==0){wait_=host_.content().cue(profile.at("head_delay").get<std::string>()).at("tenths").get<int>()*6;++phase_;return;}
    if(headEnding_&&phase_==1){host_.startRoute(profile.at("head_final_route"));++phase_;if(profile.value("head_wait_for_final_route",true)){return;}damage_=1600;}
    if(headEnding_&&profile.value("head_wait_for_final_route",true)&&host_.routePending()){return;}
    host_.session().setFlag(definition_.deadFlag,true);mode_=EnemyMode::Corpse;
    auto e=host_.actor();e.layer=0;e.through=true;host_.setActor(e);
}
void ZombieBehavior::collisionYield(Direction facing){
    const auto& profile=rules().at("runtime_profile");
    if(fallenState_||damage_>=rules().at("death_damage_threshold").get<int>()||!profile.contains("collision_yield_routes")){return;}
    // An already running two-step yield must not restart on every Wait0.
    const std::string route=profile.at("collision_yield_routes").at(std::string(directionName(facing)));
    if(host_.routePending(route)){return;}
    const bool interrupt=profile.value("receiver_1_interrupts_attack",true);
    if(interrupt&&(holdsPlayer()||mode_==EnemyMode::PendingGrab)){interruptGrab();}
    host_.startRoute(route);if(interrupt){mode_=EnemyMode::HitRecovery;wait_=0;pursuit_.reset();}
}

void ZombieBehavior::startRush(){
    mode_=EnemyMode::Rush;phase_=0;wait_=0;fallenState_=false;pursuit_.reset();variantRoute_.reset();
    applyPage(rules().at("runtime_profile").at("rush").at("page"),true);
}
bool ZombieBehavior::rushInRange() const{
    const auto& r=rules().at("runtime_profile").at("rush");const auto e=host_.actor();
    const auto origin=e.motion.position(),target=host_.player().motion.position();
    const double dx=(target.x-origin.x)*16,dy=(target.y-origin.y)*16;
    const auto d=e.motion.facing();const double front=d==Direction::Up?-dy:d==Direction::Down?dy:d==Direction::Right?dx:-dx;
    const double side=d==Direction::Up||d==Direction::Down?dx:dy;
    return front>=r.at("trigger_front_pixels")[0].get<int>()&&front<=r.at("trigger_front_pixels")[1].get<int>()&&side>=r.at("trigger_side_pixels")[0].get<int>()&&side<=r.at("trigger_side_pixels")[1].get<int>();
}
void ZombieBehavior::advancePursuit(int page,std::optional<RoutePlayer>& route){
    if(host_.routePending()){return;}
    const auto e=host_.actor();const auto player=host_.player().motion.tile();
    if(host_.terrain(adjacentTile(e.motion.tile(),e.motion.facing()))==3){
        const bool vertical=e.motion.facing()==Direction::Up||e.motion.facing()==Direction::Down;
        const int delta=vertical?player.x-e.motion.tile().x:player.y-e.motion.tile().y;
        if(delta){RouteDefinition side;side.frequency=8;side.steps.push_back({std::string(directionName(vertical?(delta>0?Direction::Right:Direction::Left):(delta>0?Direction::Down:Direction::Up))),{},{},0});host_.route(side);return;}
    }
    if(!route){for(const auto& v:host_.content().objects.at(id_).variants){if(v.page==page){route.emplace(v.autonomousRoute,id_);break;}}}
    if(route){host_.advance(*route);}
}
void ZombieBehavior::updateRush(){
    const auto& r=rules().at("runtime_profile").at("rush");const auto e=host_.actor();const auto player=host_.player();
    if(phase_==1){
        phase_=0;
        if(adjacentTile(e.motion.tile(),e.motion.facing())==player.motion.tile()&&!host_.session().flag(13)){
            mode_=EnemyMode::PendingGrab;host_.requestGrab();return;
        }
    }else{
        const auto ep=e.motion.position(),pp=player.motion.position();const auto& offset=r.at("front_probe_offsets").at(std::string(directionName(e.motion.facing())));
        const int half=r.at("geometry_half_extent");
        if(std::abs((ep.x-pp.x)*16+offset[0].get<int>())<=half&&std::abs((ep.y-pp.y)*16+offset[1].get<int>())<=half){phase_=1;return;}
    }
    advancePursuit(r.at("page"),variantRoute_);
}

// Source rescue pages remain parallel while the NPC outcome owns foreground control.
void ZombieBehavior::updateParallel(){
    if(mode_==EnemyMode::Arrival&&present()&&host_.session().hp()>0){updateArrival();}
    if(mode_==EnemyMode::Dormant&&rules().contains("dormant_presentation")&&host_.map()==host_.content().objects.at(id_).map&&host_.session().hp()>0){
        const auto& presentation=rules().at("dormant_presentation");
        const auto* page=host_.content().activeVariant(host_.content().objects.at(id_),host_.session());
        if(page&&page->page==presentation.at("page")&&!host_.session().flag(presentation.at("stop_fact"))&&advancePresentation(presentation.at("cues"),dormantCue_,dormantWait_)){
            if(!presentation.at("repeat").get<bool>()){throw std::logic_error("Unsupported dormant zombie presentation");}dormantCue_=0;
        }
    }
    const int damage=host_.advanceBurn();
    if(damage&&present()&&mode_!=EnemyMode::Corpse&&mode_!=EnemyMode::Ending){damage_+=damage;if(damage_>=rules().at("death_damage_threshold").get<int>()){beginDamageEnding(false);}}
    if(rules().contains("rescue")&&mode_==EnemyMode::Ending&&host_.session().hp()>0){updateRescueEnding();}
}
bool ZombieBehavior::activateState(int state){
    const auto& r=rules();
    if(!r.contains("arrival")||state!=r.at("arrival").at("mode")||mode_==EnemyMode::Corpse||mode_==EnemyMode::Ending||mode_==EnemyMode::Arrival||host_.map()!=host_.content().objects.at(id_).map){return false;}
    host_.cancelRoutes();pursuit_.reset();mode_=EnemyMode::Arrival;arrivalCue_=0;wait_=0;applyPage(r.at("arrival").at("page"),true);return true;
}
void ZombieBehavior::updateArrival(){
    if(!advancePresentation(rules().at("arrival").at("cues"),arrivalCue_,wait_)||host_.routePending()){return;}
    if(rules().at("arrival").at("end_mode")!=0){throw std::logic_error("Unsupported zombie arrival outcome");}
    resumePursuit();
}
bool ZombieBehavior::advancePresentation(const nlohmann::json& cues,std::size_t& cursor,int& wait){
    if(wait>0&&--wait>0){return false;}
    while(cursor<cues.size()){
        const auto id=cues.at(cursor).get<std::string>();const auto& cue=host_.content().cue(id);const std::string kind=cue.at("kind");
        if(kind=="wait_routes"&&host_.routePending()){return false;}
        ++cursor;
        if(kind=="wait"){wait=std::max(1,cue.at("tenths").get<int>()*6);return false;}
        if(kind=="forced_route"){host_.startRoute(id);}
        else if(kind=="sound"){host_.sound(id);}
        else if(kind!="wait_routes"){throw std::logic_error("Unsupported zombie presentation cue");}
    }
    return true;
}
bool ZombieBehavior::setScriptedDamage(int damage){
    if(!rules().contains("rescue")||damage!=rules().at("death_damage_threshold").get<int>()||mode_==EnemyMode::Corpse){return false;}
    if(holdsPlayer()||mode_==EnemyMode::PendingGrab){interruptGrab();}
    damage_=damage;mode_=EnemyMode::Ending;beginRescueEnding();return true;
}
void ZombieBehavior::beginRescueEnding(){host_.cancelRoutes();pursuit_.reset();phase_=-1;wait_=0;}
void ZombieBehavior::startRescueEndingPage(){
    const auto& pages=rules().at("rescue").at("timely_commit").at("death_pages");
    phase_=damage_>=2000?2:damage_>=150?1:0;
    if(phase_&&!host_.session().flag(pages.at(phase_).at("requires_switch_on"))){damage_=100;phase_=0;}
    const auto& page=pages.at(phase_);applyPage(page.at("page"),true);
    const auto& profile=rules().at("runtime_profile");
    if(phase_==0){host_.sound(cue("normal_death_sound"));}
    else if(phase_==1){host_.cue(profile.at("head_start_route"));host_.cue(profile.at("head_animation"));}
    else{if(profile.contains("body_cancel")){host_.cue(profile.at("body_cancel"));}if(profile.contains("body_sound")&&!profile.at("body_sound").is_null()){host_.sound(profile.at("body_sound"));}host_.startRoute(profile.at("body_start_route"));}
    wait_=page.at("wait_tenths").get<int>()*6;
    if(phase_==1){
        const auto& cue=host_.content().cue(profile.at("head_animation").get<std::string>());
        if(cue.at("wait").get<bool>()){const auto& animation=host_.content().media.at("animations").at(cue.at("animation").get<std::string>());wait_+=static_cast<int>(animation.at("frames").size())*animation.at("playback").at("logic_updates_per_frame").get<int>();}
    }
}
void ZombieBehavior::updateRescueEnding(){
    if(phase_<0){startRescueEndingPage();return;}
    if(wait_>0&&--wait_>0){return;}
    const auto& crisis=rules().at("rescue");const auto& page=crisis.at("timely_commit").at("death_pages").at(phase_);const auto& profile=rules().at("runtime_profile");
    if(phase_){host_.startRoute(profile.at(phase_==1?"head_final_route":"body_final_route"));damage_=page.at("set_damage_before_commit");}
    if(!host_.session().flag(crisis.at("intervention_switch"))){host_.session().setFlag(crisis.at("timely_switch"),true);}
    host_.session().setFlag(crisis.at("dead_switch"),true);mode_=EnemyMode::Corpse;applyPage(profile.at("corpse_pages").at(phase_),true);
}
