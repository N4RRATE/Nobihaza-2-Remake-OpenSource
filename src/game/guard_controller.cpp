#include "guard_controller.h"
#include "full_content.h"
#include "enemy_controller.h"
#include "cerberus_behavior.h"
#include "enemy_host.h"
#include "scene_presentation.h"
#include "weapon_controller.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace{
using Json=nlohmann::json;
GridPosition body(const CharacterState& a){const auto p=a.motion.position();return {(p.x+.5)*16,(p.y+1)*16-8};}
bool overlap(GridPosition a,GridPosition b,std::array<int,2> ah,std::array<int,2> bh={8,8}){return std::abs(a.x-b.x)<=ah[0]+bh[0]&&std::abs(a.y-b.y)<=ah[1]+bh[1];}
Json guardDogRules(CombatScene& scene,const std::string& id){EnemyHost host(scene,id);auto rules=host.content().catalog.at("guard_dog");if(const auto* patch=difficultyEnemyRules(host.content(),host.session(),id)){rules.merge_patch(*patch);}return rules;}
TilePosition xy(const Json& a){return {a.at(0),a.at(1)};}
Direction faceToward(TilePosition a,TilePosition b,Direction old){if(a.x!=b.x){old=a.x<b.x?Direction::Right:Direction::Left;}if(a.y!=b.y){old=a.y<b.y?Direction::Down:Direction::Up;}return old;}
}
GuardController::GuardController(CombatScene& scene,const Json& definition):scene_(scene),profile_(definition.at("runtime_profile")){
    if(const auto* patch=difficultyControllerRules(scene.content_,scene.session_,profile_.at("object"))){profile_.merge_patch(*patch);}
    weapon_=definition.at("equipment").at(0);restore();supportWait_=profile_.at("support").at("delay_tenths").get<int>()*6;
    for(int i=0;i<8;++i){auto& slot=slots_[i];slot.number=i+1;const auto& r=row(slot);auto actor=findCharacter(*scene_.world_,r.at("object"));actor.active=true;replaceCharacter(*scene_.world_,actor);
        if(r.value("type",std::string{})=="dog"){auto rules=guardDogRules(scene_,r.at("object"));
            dog_=std::make_unique<CerberusBehavior>(scene_,EnemyDefinition{r.at("object"),profile_.at("map"),8,r.at("dead_fact"),false,std::move(rules)});if(!scene_.session_.flag(r.at("dead_fact"))){dog_->activateState(1);}continue;}
        const auto mode=static_cast<SlotMode>(value(slot,"mode"));slot.holdingNpc=mode==SlotMode::CompanionFront||mode==SlotMode::CompanionBack;slot.holdingPlayer=mode==SlotMode::PlayerFront||mode==SlotMode::PlayerBack;
        if(slot.holdingPlayer){scene_.grabOwner_=actor.id;}}

}
GuardController::~GuardController()=default;
const Json& GuardController::row(const Slot& slot) const{return profile_.at("slots").at(std::to_string(slot.number));}
int GuardController::value(const Slot& s,const char* name) const{return scene_.session_.variable(row(s).at("source_slots").at(std::string(name)=="mode"&&row(s).at("source_slots").contains("state")?"state":name));}
void GuardController::set(Slot& s,const char* name,int n){scene_.session_.setVariable(row(s).at("source_slots").at(std::string(name)=="mode"&&row(s).at("source_slots").contains("state")?"state":name),n);}
bool GuardController::owns(const std::string& id) const{
    if(id==profile_.at("object").get<std::string>()||id==profile_.at("projectile").at("object").get<std::string>()){return true;}
    return std::any_of(slots_.begin(),slots_.end(),[&](const auto& s){return row(s).at("object").template get<std::string>()==id;});
}
bool GuardController::grantGrab(const std::string& id){return dog_&&dog_->id()==id&&dog_->mode()==EnemyMode::PendingGrab&&dog_->beginGrab();}
void GuardController::applyRole(const std::string& id,const Json& role){
    auto a=findCharacter(*scene_.world_,id);const auto& variants=scene_.content_.objects.at(id).variants;
    const auto it=std::find_if(variants.begin(),variants.end(),[&](const auto& v){return v.page==role.at("page");});
    if(it==variants.end()){throw std::logic_error("Guard page disappeared");}
    const auto& d=it->definition;a.active=true;a.spriteId=d.spriteId;a.layer=d.layer;a.through=a.routeThrough.value_or(d.through);a.setSpeedLevel(d.moveSpeed);a.transparency=d.transparency;
    a.setAnimation(CharacterAnimation(animationForSpeed(d.moveSpeed),d.initialColumn,d.animationType),a.motion.facing());replaceCharacter(*scene_.world_,a);
}
// Media and named gameplay commits share a finite role cursor, not source opcodes.
bool GuardController::advanceRole(const std::string& id,const Json& role,RoleProgress& progress,const std::function<void(int)>& commit){
    if(progress.wait>0){--progress.wait;if(progress.wait>0){return false;}}
    const auto& media=role.at("media");
    while(progress.cursor<media.size()){
        const auto& cue=media.at(progress.cursor);const int command=cue.at("command");if(commit){commit(command);}
        const std::string kind=cue.at("kind"),key=cue.at("cue");
        if(kind=="wait"){progress.wait=std::max(1,scene_.content_.cue(key).at("tenths").get<int>()*6);++progress.cursor;return false;}
        if(kind=="forced_route"){scene_.startRoute(key,id);}
        else if(kind=="sound"){scene_.playSound(key);}
        else if(kind=="cancel_routes"){std::erase_if(scene_.routes_,[&](const auto& route){return route.actor()==id;});}
        else if(kind=="fade_bgm"||kind=="flash_character"){scene_.cue(key,id);}
        else{scene_.presentation_.push_back({PresentationEvent::Kind::Cue,key,{},findCharacter(*scene_.world_,id).motion.position(),0,id,progress.sampledScreen});}
        ++progress.cursor;
    }
    if(commit){commit(100000);}return true;
}
void GuardController::stepActor(const std::string& id,Direction d,bool backwards){
    auto a=findCharacter(*scene_.world_,id);if(a.motion.moving()||std::any_of(scene_.routes_.begin(),scene_.routes_.end(),[&](const auto& r){return r.actor()==id;})){return;}
    if(!backwards){a.face(d);}if(scene_.world_->canMove(a,d)){a.motion.beginStep(d,backwards);}replaceCharacter(*scene_.world_,a);
}
void GuardController::sampleBodies(){
    auto& s=scene_.session_;const auto& sampling=profile_.at("source_sampling");const auto& rule=profile_.at("rules").at("body_sampling");const auto npc=findCharacter(*scene_.world_,profile_.at("object"));const auto tile=npc.motion.tile();const auto screen=scene_.sampleActorScreen(npc.id);
    s.setVariable(rule.at("tile_fields").at(0),tile.x);s.setVariable(rule.at("tile_fields").at(1),tile.y);
    for(int axis=0;axis<2;++axis){s.setVariable(sampling.at("body_screen_variables").at(axis),static_cast<int>(axis?screen.y:screen.x)+sampling.at("body_center_offset_pixels").at(axis).get<int>());}
    s.setVariable(244,sampling.at("body_half_pixels").at(0));s.setVariable(245,sampling.at("body_half_pixels").at(1));s.setVariable(rule.at("weapon_field"),weapon_);
    if(s.flag(10)&&s.variable(261)<=19&&s.variable(261)!=10&&s.variable(261)!=11&&(!sampling.contains("player_grab_excluded_modes")||s.variable(261)!=14)){const auto player=scene_.sampleActorScreen("player");const int signedSum=static_cast<int>(player.x-screen.x+player.y-screen.y);if(signedSum>=-160&&signedSum<=160){s.setVariable(261,13);}}
    for(auto& slot:slots_){const auto& r=row(slot);const auto& b=r.at("body_sampling");const auto a=findCharacter(*scene_.world_,r.at("object"));const auto tile=a.motion.tile();const auto pixel=scene_.sampleActorScreen(a.id);
        const int base=r.at("source_slots").at("base");s.setVariable(base,tile.x);s.setVariable(base+1,tile.y);s.setVariable(r.at("source_slots").at("event"),a.eventId);
        if(slot.regeneration==Regeneration::Alive&&value(slot,"damage")<100){s.setVariable(base+2,static_cast<int>(pixel.x)+b.at("screen_offset_pixels").at(0).get<int>());s.setVariable(base+3,static_cast<int>(pixel.y)+b.at("screen_offset_pixels").at(1).get<int>());for(int n=0;n<2;++n){s.setVariable(b.at("half_variables").at(n),b.at(b.contains("half_pixels")?"half_pixels":"pursuit_half_pixels").at(n));}}
    }
}
std::vector<EnemyView> GuardController::views() const{
    std::vector<EnemyView> result;for(const auto& s:slots_){const int mode=value(s,"mode"),damage=value(s,"damage");if(row(s).value("type",std::string{})=="dog"){result.push_back({row(s).at("object"),damage,s.regeneration!=Regeneration::Alive?EnemyMode::Corpse:dog_->mode(),dog_->holdsPlayer(),dog_->sourceState()});continue;}result.push_back({row(s).at("object"),damage,s.regeneration!=Regeneration::Alive?EnemyMode::Corpse:mode==7||mode==8?EnemyMode::Grab:mode==100?EnemyMode::Fallen:EnemyMode::Pursuit,s.holdingPlayer,mode});}return result;
}
void GuardController::beginGrab(Slot& slot,bool companion){
    auto& session=scene_.session_;const auto& definition=row(slot);const std::string id=definition.at("object");auto actor=findCharacter(*scene_.world_,id);
    const bool back=actor.motion.facing()==Direction::Up;const std::string name=companion?(back?"companion_grab_back":"companion_grab_front"):(back?"player_grab_back":"player_grab_front");
    const auto& role=definition.at("roles").at(name);const std::string target=companion?profile_.at("object").get<std::string>():"player";
    std::erase_if(scene_.routes_,[&](const auto& route){return route.actor()==target||route.actor()==id;});
    slot.holdingNpc=companion;slot.holdingPlayer=!companion;slot.bites=slot.escape=0;session.setFlag(companion?221:10,true);
    if(companion){session.setVariable(261,profile_.at("rules").at("hit_response").at("grab_mode"));scene_.playSound(profile_.at("npc_roles").at("response").at("media").at(0).at("cue"));}else{scene_.weapon_->cancel(ActionCancel::Grabbed);scene_.grabOwner_=id;}
    set(slot,"mode",definition.at("states").at(name));applyRole(id,role);
    actor=findCharacter(*scene_.world_,id);const auto tile=findCharacter(*scene_.world_,target).motion.tile();actor.motion=CharacterMotion(tile,actor.motion.facing(),actor.motion.speed());replaceCharacter(*scene_.world_,actor);
    std::vector<std::string> routes;for(const auto& cue:role.at("media")){if(cue.at("kind")=="forced_route"){routes.push_back(cue.at("cue"));}else if(cue.at("kind")=="message_options"){scene_.cue(cue.at("cue"),id);break;}}
    // The first four routes are the source's directional held pose; the fifth is the holder pose.
    routes.clear();for(const auto& cue:role.at("media")){if(cue.at("kind")=="forced_route"){routes.push_back(cue.at("cue"));}}
    if(routes.size()>=5){scene_.startRoute(routes.at(static_cast<int>(actor.motion.facing())),id);scene_.startRoute(routes.at(4),id);}
    if(companion){const auto& grab=definition.at("companion_grab");slot.wait=std::max(1,grab.at("initial_yield_tenths").get<int>()*6)+grab.at("initial_wait_tenths").get<int>()*6;}else{slot.wait=7;}
}
void GuardController::release(Slot& slot,bool rescued){
    auto& s=scene_.session_;const auto& r=row(slot);const bool held=slot.holdingNpc||slot.holdingPlayer;
    if(slot.holdingNpc||value(slot,"mode")==7||value(slot,"mode")==8){s.setFlag(profile_.at("rescues").at("held_fact"),false);slot.holdingNpc=false;
        if(rescued&&s.flag(profile_.at("rescues").at("player_action_fact"))){s.setVariable(267,s.variable(267)+profile_.at("rescues").at("counter_add").get<int>());s.setVariable(261,profile_.at("rescues").at("next_mode"));const auto& voices=profile_.at("rescues").at("voice_pages");if(voices.contains(std::to_string(s.variable(267)))){showVoice(voices.at(std::to_string(s.variable(267))));}}
        else{s.setVariable(261,row(slot).at("companion_grab").at("released_companion_mode"));}
    }
    if(slot.holdingPlayer){scene_.releaseGrabOwner(r.at("object"));s.setFlag(10,false);slot.holdingPlayer=false;}
    if(slot.holdingNpc||slot.holdingPlayer){throw std::logic_error("Guard release retained a holder");}
    if(held){for(const auto& name:{"companion_grab_front","player_grab_front"}){const auto& media=r.at("roles").at(name).at("media");for(auto it=media.rbegin();it!=media.rend();++it){if(it->at("kind")=="message_options"){scene_.cue(it->at("cue"),r.at("object"));break;}}}}
    s.setFlag(r.at("attack_fact"),false);set(slot,"mode",r.at("states").at("retreat"));slot.wait=r.at("companion_grab").at("release_wait_tenths").get<int>()*6;
}
AttackContact GuardController::hit(Slot& slot,int page,int amount,const AttackSample* sample){
    auto& s=scene_.session_;const auto& r=row(slot);if(r.value("type",std::string{})=="dog"){const auto contact=sample?dog_->receiveAttack(*sample):dog_->receiveCompanion(page,amount?amount:outgoingDamage(slot));set(slot,"damage",dog_->accumulatedDamage());set(slot,"mode",dog_->sourceState());return contact;}const auto& receiver=r.at("receivers").at(std::to_string(page));const int before=value(slot,"damage"),beforeMode=value(slot,"mode");
    if(before>=receiver.at("guard_damage_below").get<int>()||slot.regeneration!=Regeneration::Alive){return {};}
    if((page==7||page==8)&&beforeMode>=100&&!s.flag(223)){return {};}
    bool end=false,critical=false;
    if(receiver.at("damage_rule")=="ce11"){
        if(!receiver.at("critical_damage").is_null()){amount=receiver.at("critical_damage").get<int>()-before;critical=true;}
        else{
            s.setFlag(12,false);const int count=s.variable(35)+1;s.setVariable(35,count);
            EnemyHost host(scene_,r.at("object"));const auto kind=host.damageKind(sample->kind);
            const int attack=auxiliaryDamage(kind)?s.auxiliaryAttack():s.attack()+(sample->aimed?scene_.content_.items.at(scene_.content_.rules.at("handgun_damage").at("aim_attack_item")).attack:0);
            s.setVariable(49,r.at("attributes").at("defense"));
            const auto damage=rollWeaponDamage(kind,attack,s.variable(49),sample->advances,scene_.random_);amount=damage.amount;end=damage.endRequested;
            if(kind==AttackKind::Knife){
                if(count%3==0&&scene_.random_(0,100)-count<=15){amount=s.auxiliaryAttack()+scene_.random_(0,10);}scene_.playSound("f3-ce11-c13");
            }else if(!auxiliaryDamage(kind)){
                critical=s.flag(47)||(count%3==0&&scene_.random_(0,100)-count<=mainCriticalThreshold(kind));s.setFlag(47,false);
                if(critical){s.setVariable(35,0);amount=150-before;}
            }
        }
        s.setFlag(12,critical);
    }else if(!receiver.at("critical_damage").is_null()){amount=receiver.at("critical_damage").get<int>()-before;}
    else{amount=outgoingDamage(slot);}
    set(slot,"damage",before+amount);
    const bool holder=slot.holdingNpc||slot.holdingPlayer;
    if(holder&&(receiver.at("release_companion").get<bool>()||value(slot,"damage")>=100)){release(slot,receiver.at("owner")=="player");}
    // These source receiver pages interrupt an attack and then own their route.
    if(page==3||page==4||page==8){s.setFlag(r.at("attack_fact"),false);set(slot,"mode",page==8?1:0);slot.wait=0;}
    else if(receiver.at("owner")=="companion"&&value(slot,"mode")<100){set(slot,"mode",r.at("states").at("stun"));slot.wait=12;}
    std::vector<std::string> routes;
    for(const auto& cue:receiver.at("role").at("media")){
        if(cue.at("kind")=="forced_route"){routes.push_back(cue.at("cue"));}
        else if(cue.at("kind")=="sound"&&!critical){scene_.playSound(cue.at("cue"));}
    }
    if(!routes.empty()){
        // The second route belongs to the feeding branch; P4 always has its full reaction.
        slot.receiverRoute=routes.at(beforeMode>=100&&routes.size()>1?1:0);
        scene_.startRoute(slot.receiverRoute,r.at("object"));
    }
    if(receiver.at("quota_on_threshold_crossing").get<bool>()&&before<100&&value(slot,"damage")>=100){s.setVariable(269,s.variable(269)+1);}
    s.setFlag(r.at("blood_fact"),true);s.setVariable(49,r.at("attributes").at("defense"));
    int blood=page==3?2:page==4||page==6?3:1;
    if(!receiver.at("blood_work").is_null()){
        const auto& work=receiver.at("blood_work");if(page==7){blood=beforeMode==100&&!s.flag(223)?work.at("when_feeding_and_agility_off").get<int>():work.at("normal").get<int>();}
        else if(page==8){blood=s.flag(223)?work.at("normal").get<int>():work.at("when_agility_off").get<int>();}
        else if(page==9){blood=work.at(s.flag(223)?"when_agility_on":"when_agility_off");}
    }
    const int base=r.at("source_slots").at("base");s.setVariable(base+4,blood);
    if(page!=5&&page!=10){scene_.presentation_.push_back({PresentationEvent::Kind::Effect,blood==3?"blood-large":blood==2||blood==12?"enemy-blood-medium":"enemy-blood-small",{},findCharacter(*scene_.world_,r.at("object")).motion.position(),0,r.at("object")});}

    return {true,true,amount,end};
}
AttackResolution GuardController::receive(const AttackSample& sample,std::set<std::string>& resolved){
    AttackResolution result;if(finished_||!scene_.session_.flag(profile_.at("activation_fact"))){return result;}
    for(auto& slot:slots_){const auto& r=row(slot);const std::string id=r.at("object");if(resolved.contains(id)||scene_.session_.flag(r.at("dead_fact"))||value(slot,"damage")>=100||slot.regeneration!=Regeneration::Alive){continue;}
        if(!overlap(sample.center,body(findCharacter(*scene_.world_,id)),sample.halfPixels)){continue;}
        ++result.contacts;if(sample.phase==AttackPhase::Flight){continue;}
        const int page=sample.receiverPage;if(page<2||page>6){throw std::invalid_argument("Player cannot invoke companion receiver");}
        if(sample.phase==AttackPhase::Burning){scene_.session_.setFlag(r.at("burn_fact"),true);resolved.insert(id);++result.processed;continue;}
        const auto h=hit(slot,page,0,&sample);result.damage+=h.damage;result.endRequested|=h.endRequested;result.processed+=h.processed;if(h.processed){resolved.insert(id);}
    }return result;
}
void GuardController::hurtNpc(int attack){
    auto& s=scene_.session_;const auto& rule=profile_.at("rules").at("hurt");if(s.flag(221)||s.flag(222)||hp_<=0){return;}
    s.setVariable(261,rule.at("next_mode"));const auto& range=rule.at("random_inclusive");const int roll=scene_.random_(range.at(0),range.at(1));s.setVariable(260,roll);
    const auto& sounds=profile_.at("npc_roles").at("receive").at("media");scene_.playSound(sounds.at(roll<=rule.at("hit_at_most").get<int>()?0:1).at("cue"));
    if(roll>rule.at("hit_at_most").get<int>()){return;}
    const auto& variance=rule.at("variance_inclusive");const int damage=std::max(rule.at("minimum").get<int>(),attack-profile_.at("attributes").at("defense").get<int>()/rule.at("damage_defense_divisor").get<int>()+scene_.random_(variance.at(0),variance.at(1)));
    hp_=std::max(0,hp_-damage);s.setFlag(218,true);bloodType_=hp_?1:3;s.setVariable(252,bloodType_);if(rule.contains("low_hp_at_most")){const int fact=rule.at("low_hp_voice_fact");if(hp_>rule.at("low_hp_at_most").get<int>()){s.setFlag(fact,false);}else if(!s.flag(fact)){showVoice(6);s.setFlag(fact,true);}}if(!hp_){s.setFlag(rule.at("dead_fact"),true);}
}
void GuardController::regenerate(Slot& slot){
    auto& s=scene_.session_;const auto& r=row(slot);const auto& regen=r.at("regeneration");const std::string id=r.at("object");
    if(slot.regeneration==Regeneration::Alive){
        if(r.value("type",std::string{})=="dog"){if(dog_->mode()!=EnemyMode::Corpse){dog_->update({});set(slot,"damage",dog_->accumulatedDamage());set(slot,"mode",dog_->sourceState());return;}slot.regeneration=Regeneration::Fading;slot.fade=0;slot.wait=regen.at("wait_before_fade_tenths").get<int>()*6;return;}
        release(slot);slot.fade=0;const int damage=value(slot,"damage");
        if(!s.flag(r.at("dead_fact"))){
            std::string role=damage>=2000?"body_death":damage>=150?"head_death":"normal_death";
            if(role!="normal_death"&&!s.flag(12)){set(slot,"damage",100);role="normal_death";}
            slot.presentation={role};applyRole(id,r.at("roles").at(role));slot.regeneration=Regeneration::Dying;
        }else{slot.regeneration=Regeneration::Fading;slot.wait=regen.at("wait_before_fade_tenths").get<int>()*6;}
    }
    if(slot.regeneration==Regeneration::Dying){
        if(!advanceRole(id,r.at("roles").at(slot.presentation.role),slot.presentation)){return;}
        if(slot.presentation.role=="body_death"){set(slot,"damage",2100);}else if(slot.presentation.role=="head_death"){set(slot,"damage",1600);}
        s.setFlag(r.at("dead_fact"),true);for(const auto& v:r.at("body_sampling").at("death_clear_screen")){s.setVariable(v,0);}
        const int damage=value(slot,"damage");applyRole(id,r.at("roles").at(damage>=2100?"body_dead":damage>=1600?"head_dead":"dead"));
        slot.regeneration=Regeneration::Fading;slot.wait=regen.at("wait_before_fade_tenths").get<int>()*6;return;
    }
    if(slot.wait>0&&--slot.wait>0){return;}
    auto actor=findCharacter(*scene_.world_,id);
    if(slot.regeneration==Regeneration::Fading){actor.transparency=std::min(7,++slot.fade);replaceCharacter(*scene_.world_,actor);if(slot.fade<regen.at("fade_steps").get<int>()){slot.wait=regen.at("fade_wait_tenths").get<int>()*6;return;}
        actor.motion=CharacterMotion(xy(regen.at("hide_tile")),actor.motion.facing(),actor.motion.speed());actor.active=false;replaceCharacter(*scene_.world_,actor);
        for(const auto& range:regen.at("exact_cache_clear_ranges")){for(int v=range.at(0);v<=range.at(1);++v){s.setVariable(v,0);}}s.setVariable(regen.at("scratch_clear"),0);
        const auto invalidate=xy(regen.at("invalidate_tile"));const int base=r.at("source_slots").at("base");s.setVariable(base,invalidate.x);s.setVariable(base+1,invalidate.y);
        slot.regeneration=Regeneration::Hidden;slot.wait=regen.at("hidden_wait_tenths").get<int>()*6;return;
    }
    const auto& tiles=regen.at("spawn_tiles");const auto range=regen.value("random_spawn_inclusive",Json{});const int index=range.is_null()?0:scene_.random_(range.at(0),range.at(1));actor.active=true;actor.transparency=0;actor.motion=CharacterMotion(xy(tiles.at(index)),actor.motion.facing(),actor.motion.speed());replaceCharacter(*scene_.world_,actor);if(regen.contains("restore_opacity_route")){scene_.startRoute(regen.at("restore_opacity_route"),id);}
    s.setFlag(r.at("dead_fact"),false);set(slot,"damage",0);set(slot,"mode",r.at("has_roam_page").get<bool>()?0:1);slot.regeneration=Regeneration::Alive;slot.presentation={};slot.bites=0;if(r.value("type",std::string{})=="dog"){dog_=std::make_unique<CerberusBehavior>(scene_,EnemyDefinition{id,profile_.at("map"),8,r.at("dead_fact"),false,guardDogRules(scene_,id)});dog_->activateState(1);}
}
void GuardController::updateSlot(Slot& slot,const InputFrame& input){
    auto& s=scene_.session_;const auto& r=row(slot);const std::string id=r.at("object");if(slot.regeneration!=Regeneration::Alive||s.flag(r.at("dead_fact"))||value(slot,"damage")>=100){regenerate(slot);return;}
    if(r.value("type",std::string{})=="dog"){dog_->update(input);set(slot,"damage",dog_->accumulatedDamage());set(slot,"mode",dog_->sourceState());s.setFlag(r.at("attack_fact"),dog_->attacking());return;}
    if(!slot.receiverRoute.empty()){
        const bool pending=std::any_of(scene_.routes_.begin(),scene_.routes_.end(),[&](const auto& route){return route.actor()==id&&route.source()==slot.receiverRoute;});
        if(pending&&!slot.holdingNpc&&!slot.holdingPlayer){return;}
        if(!pending){slot.receiverRoute.clear();}
    }
    const auto mode=static_cast<SlotMode>(value(slot,"mode"));const auto actor=findCharacter(*scene_.world_,id);const auto tile=actor.motion.tile();
    if(slot.holdingPlayer){for(int n=0;n<4;++n){slot.escape+=input.buttons[n].pressed;}if(slot.escape>=r.at("player_grab").at("escape_at_least").get<int>()||!s.flag(r.at("attack_fact"))){release(slot);return;}}
    if(slot.wait>0){--slot.wait;return;}
    if(mode==SlotMode::CompanionFront||mode==SlotMode::CompanionBack){
        if(!slot.holdingNpc||!s.flag(221)||hp_<=0){release(slot);return;}
        const auto& g=r.at("companion_grab");const auto range=g.at("escape_random_inclusive");s.setVariable(260,s.variable(260)+scene_.random_(range.at(0),range.at(1)));s.setVariable(257,++slot.bites);
        // The original receiver guards still apply to a bite from a holding enemy.
        hurtNpc(r.at("attributes").at("attack"));s.setVariable(261,20);
        if(slot.bites>=g.at("maximum_bites").get<int>()||s.variable(260)>=g.at("escape_at_least").get<int>()){release(slot);return;}
        slot.wait=g.at("period_tenths").get<int>()*6;return;
    }
    if(mode==SlotMode::PlayerFront||mode==SlotMode::PlayerBack){if(!slot.holdingPlayer){release(slot);return;}scene_.applyPlayerDamage(biteDamage(r.at("attributes").at("attack"),s.defense(),scene_.random_(-5,3)));if(++slot.bites>=r.at("player_grab").at("maximum_bites").get<int>()){release(slot);return;}slot.wait=r.at("player_grab").at("period_tenths").get<int>()*6;return;}
    if(mode==SlotMode::Retreat){stepActor(id,static_cast<Direction>((static_cast<int>(actor.motion.facing())+2)%4),true);set(slot,"mode",1);slot.wait=6;return;}
    if(mode==SlotMode::Stun||mode==SlotMode::Knockback){
        const std::string name=mode==SlotMode::Stun?"stun":"knockback";auto role=r.at("roles").at(name);
        if(slot.presentation.role!=name){slot.presentation={name};applyRole(id,role);}
        if(mode==SlotMode::Knockback){auto media=Json::array();const auto away=static_cast<Direction>((static_cast<int>(actor.motion.facing())+2)%4);
            for(const auto& cue:role.at("media")){if(cue.at("kind")!="forced_route"){media.push_back(cue);continue;}const auto& route=scene_.content_.routes.at(cue.at("cue"));if(std::any_of(route.steps.begin(),route.steps.end(),[&](const auto& step){return step.kind==directionName(away);})){media.push_back(cue);}}
            role["media"]=std::move(media);
        }
        if(advanceRole(id,role,slot.presentation)){set(slot,"mode",r.at("states").at("pursuit"));slot.presentation={};}return;
    }
    if(mode==SlotMode::Feeding){if(slot.presentation.role!="feeding"){slot.presentation={"feeding"};applyRole(id,r.at("roles").at("feeding"));}if(advanceRole(id,r.at("roles").at("feeding"),slot.presentation)){set(slot,"mode",r.at("states").at("bite"));slot.presentation={};}return;}
    const auto npc=findCharacter(*scene_.world_,profile_.at("object"));const auto player=scene_.world_->player();
    const int nd=std::abs(npc.motion.tile().x-tile.x)+std::abs(npc.motion.tile().y-tile.y),pd=std::abs(player.motion.tile().x-tile.x)+std::abs(player.motion.tile().y-tile.y);
    const bool targetNpc=nd<=pd&&!s.flag(221);const auto target=targetNpc?npc:player;const auto targetTile=target.motion.tile();
    if(mode==SlotMode::Bite||mode==SlotMode::GrabPrepare){
        if(targetNpc&&nd<=1){const auto& response=profile_.at("rules").at("hit_response");const auto range=response.at("random_inclusive");
            if(!s.flag(222)&&scene_.random_(range.at(0),range.at(1))<=response.at("grab_at_most").get<int>()&&mode==SlotMode::GrabPrepare){beginGrab(slot,true);return;}
            hurtNpc(r.at("attributes").at("attack"));
        }else if(!targetNpc&&pd<=1&&!s.flag(9)&&!s.flag(10)&&!s.flag(13)){
            if(mode==SlotMode::GrabPrepare&&!scene_.grabOwner_){beginGrab(slot,false);return;}
            scene_.applyPlayerDamage(biteDamage(r.at("attributes").at("attack"),s.defense(),scene_.random_(-5,3)));s.startRecovery(12,24);
        }
        s.setFlag(r.at("attack_fact"),false);set(slot,"mode",6);slot.wait=18;return;
    }
    const std::string role=mode==SlotMode::Roam&&r.at("has_roam_page").get<bool>()?"roam":"pursuit";
    if(slot.presentation.role!=role){slot.presentation={role};applyRole(id,r.at("roles").at(role));}
    for(const auto& check:r.at("targeting").at("feeding_checks")){if(check.at("role")==role&&value(slot,"damage")>=check.at("damage_at_least").get<int>()&&s.variable(check.at("scratch_variable"))==0){s.setVariable(check.at("scratch_variable"),check.at("scratch_commit"));set(slot,"mode",100);applyRole(id,r.at("roles").at("feeding"));slot.presentation={"feeding"};slot.wait=0;return;}}
    if((nd<=1&&targetNpc)||(pd<=1&&!targetNpc)){const auto range=r.at("targeting").at("attack_modes_random_inclusive");const int next=scene_.random_(range.at(0),range.at(1));set(slot,"mode",next);s.setFlag(r.at("attack_fact"),true);slot.wait=6;applyRole(id,r.at("roles").at(next==2?"bite":"grab_prepare"));return;}
    // Replacing x movement with y intent matches the original source axis order.
    stepActor(id,faceToward(tile,targetTile,actor.motion.facing()));set(slot,"mode",1);

}
bool GuardController::select(TilePosition probe,int radius,bool evade){
    auto& s=scene_.session_;s.setVariable(258,probe.x);s.setVariable(259,probe.y);const auto& selector=profile_.at("selectors").at(evade?"evade":"ordinary");
    for(const auto& number:profile_.at("rules").at("selector").at("slot_order")){
        auto& slot=slots_.at(number.get<int>()-1);const auto& r=row(slot);if(value(slot,"damage")>=selector.at("damage_below").get<int>()||value(slot,"mode")>=selector.at("mode_below").get<int>()||slot.regeneration!=Regeneration::Alive||s.variable(266)==slot.number){continue;}
        if(findCharacter(*scene_.world_,r.at("object")).motion.tile()!=probe){continue;}
        auto npc=findCharacter(*scene_.world_,profile_.at("object"));npc.face(faceToward(npc.motion.tile(),probe,npc.motion.facing()));replaceCharacter(*scene_.world_,npc);
        s.setVariable(264,slot.number);int next=evade?selector.at("candidate_mode").get<int>():s.variable(261)==4?8:radius==2&&value(slot,"mode")==1?12:radius>=6?6:5;s.setVariable(263,next);return true;
    }
    if(s.variable(261)==5||s.variable(261)==6){s.setVariable(263,0);}return false;
}
int GuardController::outgoingDamage(const Slot& slot){
    auto& session=scene_.session_;const auto& rule=profile_.at("rules").at("outgoing_damage");
    const char* stat=session.flag(rule.at("agility_when_fact"))?"agility":"attack";
    int damage=profile_.at("attributes").at(stat).get<int>()/rule.at("stat_divisor").get<int>();
    if(rule.at("subtract_target_defense").get<bool>()){damage-=row(slot).at("attributes").at("defense").get<int>();}
    const auto& range=rule.at("random_roll_inclusive");const int roll=scene_.random_(range.at(0),range.at(1));
    if(roll<=rule.at("low_at_most").get<int>()){const bool add=rule.contains("low_add_inclusive");const auto& low=rule.at(add?"low_add_inclusive":"low_subtract_inclusive");const int variation=scene_.random_(low.at(0),low.at(1));damage=std::max(rule.at("low_minimum").get<int>(),damage+(add?variation:-variation));}
    else if(roll>=rule.at("high_at_least").get<int>()){const auto& high=rule.at("high_add_inclusive");damage+=scene_.random_(high.at(0),high.at(1));}
    session.setFlag(rule.at("critical_fact_cleared"),false);session.setFlag(rule.at("voice_pulse_fact"),true);return damage;
}
void GuardController::melee(const std::string& role,int command){
    auto& s=scene_.session_;const auto& a=profile_.at("attacks").at(role);const auto contacts=a.contains("contacts")?a.at("contacts"):Json::array({a});if(a.value("writes_shared_half",false)){s.setVariable(47,a.at("half_pixels").at(0));s.setVariable(48,a.at("half_pixels").at(1));}
    while(meleeContact_<contacts.size()&&contacts.at(meleeContact_).at("commit_command").get<int>()<=command){const auto& hitRow=contacts.at(meleeContact_++);const auto npc=findCharacter(*scene_.world_,profile_.at("object"));const auto offset=adjacentTile({0,0},npc.motion.facing());auto center=body(npc);center.x+=offset.x*hitRow.at("facing_offset_pixels").get<int>();center.y+=offset.y*hitRow.at("facing_offset_pixels").get<int>();
        const auto screen=scene_.sampleActorScreen(npc.id);const auto& fields=profile_.at("source_sampling").at("attack_center_variables");s.setVariable(fields.at(0),static_cast<int>(screen.x)+offset.x*hitRow.at("facing_offset_pixels").get<int>());s.setVariable(fields.at(1),static_cast<int>(screen.y)-8+offset.y*hitRow.at("facing_offset_pixels").get<int>());
        const int page=hitRow.at("receiver_page").is_null()?(scene_.random_(hitRow.at("receiver_choice").at("random_inclusive").at(0),hitRow.at("receiver_choice").at("random_inclusive").at(1))<=hitRow.at("receiver_choice").at("at_most").get<int>()?hitRow.at("receiver_choice").at("low_page").get<int>():hitRow.at("receiver_choice").at("otherwise_page").get<int>()):hitRow.at("receiver_page").get<int>();
        for(auto& slot:slots_){if(value(slot,"damage")>=100||slot.regeneration!=Regeneration::Alive){continue;}const auto& r=row(slot);if(!overlap(center,body(findCharacter(*scene_.world_,r.at("object"))),{s.variable(47),s.variable(48)})){continue;}
            hit(slot,page,0);if(role=="kick"){const auto& grenade=profile_.at("attached_grenade");if(grenade.at("anchor_slots").contains(std::to_string(slot.number))){s.setVariable(grenade.at("occupied_slot_variable"),slot.number);}}
        }
    }
}
void GuardController::shoot(){
    auto& s=scene_.session_;const auto& fire=profile_.at("rules").at("fire");if(s.variable(247)<=fire.at("empty_at_most").get<int>()){s.setVariable(261,fire.at("empty_mode"));return;}
    if(bulletActive_){return;}s.setVariable(247,s.variable(247)-fire.at("ammo_cost").get<int>());s.setFlag(219,true);s.setFlag(215,true);s.setFlag(220,true);
    auto bullet=findCharacter(*scene_.world_,profile_.at("projectile").at("object"));const auto npc=findCharacter(*scene_.world_,profile_.at("object"));bullet.motion=CharacterMotion(npc.motion.tile(),npc.motion.facing(),128);bullet.through=true;bullet.active=true;replaceCharacter(*scene_.world_,bullet);bulletActive_=true;bulletDistance_=0;
}
void GuardController::updateProjectile(){
    if(!bulletActive_){return;}auto& s=scene_.session_;const auto& p=profile_.at("projectile");auto bullet=findCharacter(*scene_.world_,p.at("object"));const auto d=adjacentTile({0,0},bullet.motion.facing());auto center=body(bullet);if(bulletDistance_){center.x+=d.x*p.at("travel").at("facing_offset_pixels").get<int>();center.y+=d.y*p.at("travel").at("facing_offset_pixels").get<int>();}
    const auto& stage=p.at(bulletDistance_?"travel":"init");s.setVariable(47,stage.at("half_pixels").at(0));s.setVariable(48,stage.at("half_pixels").at(1));
    s.setVariable(248,bullet.motion.tile().x);s.setVariable(249,bullet.motion.tile().y);
    if(!bulletDistance_){const auto& variables=p.at("init").at("source_player_center_write_variables");const int offset=p.at("init").at("source_player_center_offset_write");s.setVariable(variables.at(0),s.variable(variables.at(0))+d.x*offset);s.setVariable(variables.at(1),s.variable(variables.at(1))+d.y*offset);}
    const auto screen=scene_.sampleActorScreen(bullet.id);s.setVariable(250,static_cast<int>(screen.x)+d.x*(bulletDistance_?8:0));s.setVariable(251,static_cast<int>(screen.y)-8+d.y*(bulletDistance_?8:0));
    bool contact=false;for(auto& slot:slots_){if(slot.regeneration!=Regeneration::Alive||value(slot,"damage")>=100){continue;}const auto& r=row(slot);if(overlap(center,body(findCharacter(*scene_.world_,r.at("object"))),{stage.at("half_pixels").at(0),stage.at("half_pixels").at(1)})){
        if(hit(slot,p.at("travel").at("receiver_page"),0).processed){contact=true;s.setFlag(220,false);break;}
    }}
    EnemyHost host(scene_,bullet.id);const auto& steps=p.at("travel").at("variant").at("autonomous_route").at("steps");const int distanceLimit=std::count_if(steps.begin(),steps.end(),[](const auto& step){return step.at("kind")=="forward";});
    if(contact||bulletDistance_>=distanceLimit||host.terrain(bullet.motion.tile())==p.at("travel").at("terrain_termination_tag").get<int>()){
        bullet.motion=CharacterMotion(xy(p.at("finish").at("tile")),bullet.motion.facing(),128);replaceCharacter(*scene_.world_,bullet);bulletActive_=false;for(const auto& fact:p.at("finish").at("clear_facts")){s.setFlag(fact,false);}const auto range=p.at("finish").at("clear_source_range");for(int i=range.at(0);i<=range.at(1);++i){s.setVariable(i,0);}return;
    }
    s.setFlag(215,false);s.setFlag(216,true);if(!bullet.motion.moving()){
        // Original nonrepeating skip-blocked route consumes a forward slot even at the map edge.
        if(scene_.world_->canMove(bullet,bullet.motion.facing())){bullet.motion.beginStep(bullet.motion.facing());replaceCharacter(*scene_.world_,bullet);}
        ++bulletDistance_;
    }
}
void GuardController::updateNpc(){
    auto& s=scene_.session_;const int mode=s.variable(261);int selectedMode=mode>=21?21:mode>=20?20:std::clamp(mode,0,15);const auto& modes=profile_.at("rules").at("modes");while(selectedMode>0&&!modes.contains(std::to_string(selectedMode))){--selectedMode;}const std::string role=mode>=21?"grab-recovery":mode>=20?"rescued":modes.at(std::to_string(selectedMode)).get<std::string>();
    const std::map<std::string,NpcMode> second{{"bat",NpcMode::Knife},{"bat-recovery",NpcMode::KickRecovery}};const auto state=mode>=21?NpcMode::GrabRecovery:mode>=20?NpcMode::Rescued:second.contains(role)?second.at(role):static_cast<NpcMode>(selectedMode);
    if(npcMode_!=mode){if(mode<=4){s.setVariable(260,profile_.at("rules").at("search").at("initial_radius"));}npcMode_=mode;npcPresentation_={role};meleeContact_=0;applyRole(profile_.at("object"),profile_.at("npc_roles").at(role));if(state==NpcMode::Combo){const auto& rule=profile_.at("rules").at("close_attack");const auto range=rule.at("random_inclusive");const int roll=scene_.random_(range.at(0),range.at(1));s.setVariable(260,roll);if(roll>=rule.at("knife_random_between").at(0).get<int>()){s.setVariable(261,roll<=rule.at("knife_random_between").at(1).get<int>()&&s.variable(266)==0?14:12);return;}}if(profile_.at("attacks").contains(role)){const auto& attack=profile_.at("attacks").at(role);if(attack.contains("agility_fact")){s.setFlag(attack.at("agility_fact"),true);}else if(attack.value("uses_agility",false)){s.setFlag(223,true);}}if(state==NpcMode::Fire||state==NpcMode::LongFire){s.setFlag(223,false);}}
    if(npcWait_>0){--npcWait_;return;}
    if(mode>=20){
        if(state==NpcMode::Rescued&&s.flag(221)){return;}
        if(mode>=21){if(npcPresentation_.cursor==0){s.setFlag(222,true);const auto& recovery=profile_.at("rescues").at("recovery");recoveryStage_=0;npcRecovery_=(recovery.contains("sequence")?recovery.at("sequence").at(0).at("wait_tenths").get<int>():recovery.at("reset_wait_tenths").get<int>())*6;}
            if(!advanceRole(profile_.at("object"),profile_.at("npc_roles").at(role),npcPresentation_)){return;}s.setFlag(221,false);s.setVariable(260,0);}
        s.setVariable(261,0);return;
    }
    if(state==NpcMode::Reload){if(advanceRole(profile_.at("object"),profile_.at("npc_roles").at(role),npcPresentation_)){s.setVariable(247,profile_.at("rules").at("reload").at("ammo"));s.setVariable(261,4);}return;}
    if(state==NpcMode::Combo||state==NpcMode::Knife||state==NpcMode::Kick||state==NpcMode::KickRecovery){if(advanceRole(profile_.at("object"),profile_.at("npc_roles").at(role),npcPresentation_,[&](int command){melee(role,command);})){if(state==NpcMode::Combo){s.setFlag(223,false);if(s.flag(220)){showVoice(2);}}s.setVariable(261,profile_.at("attacks").at(role).value("next_mode",state==NpcMode::Kick?15:4));}return;}
    if(state==NpcMode::Fire||state==NpcMode::LongFire){
        const auto& rule=profile_.at("rules").at("fire");if(s.variable(247)<=rule.at("empty_at_most").get<int>()){s.setVariable(261,rule.at("empty_mode"));return;}
        if(npcPresentation_.cursor==0){
            if(state==NpcMode::LongFire&&rule.at("long_fire_requires_cursor_off").get<bool>()&&s.flag(rule.at("cursor_fact"))){return;}
            const int limit=rule.at(state==NpcMode::Fire?"short_search_exit_radius_at_least":"long_search_exit_radius_at_least");
            const auto tile=findCharacter(*scene_.world_,profile_.at("object")).motion.tile();bool selected=false;
            for(int radius=1;radius<limit&&!selected;++radius){s.setVariable(260,radius);for(Direction d:{Direction::Up,Direction::Right,Direction::Down,Direction::Left}){const auto offset=adjacentTile({0,0},d);if(select({tile.x+offset.x*radius,tile.y+offset.y*radius},radius)){selected=true;break;}}}
            if(!selected){s.setVariable(261,rule.at("final_mode"));return;}
            if(s.variable(263)!=mode){s.setVariable(261,s.variable(263));return;}
            shoot();
        }
        if(advanceRole(profile_.at("object"),profile_.at("npc_roles").at(role),npcPresentation_)){s.setVariable(261,rule.at("final_mode"));}return;
    }
    if(state==NpcMode::Evade){
        const auto& rule=profile_.at("rules").at("evade");
        if(npcPresentation_.cursor==0){
            for(const auto& number:rule.at("slot_order")){
                const auto& slot=slots_.at(number.get<int>()-1);const auto& r=row(slot);
                const auto target=findCharacter(*scene_.world_,r.at("object")).motion.tile();
                if(target!=TilePosition{s.variable(258),s.variable(259)}||value(slot,"damage")>=rule.at("alive_damage_below").get<int>()||!s.flag(r.at("attack_fact"))){continue;}
                auto npc=findCharacter(*scene_.world_,profile_.at("object"));
                npc.face(static_cast<Direction>((static_cast<int>(faceToward(npc.motion.tile(),target,npc.motion.facing()))+2)%4));replaceCharacter(*scene_.world_,npc);break;
            }
        }
        auto mediaRole=profile_.at("npc_roles").at(role);auto media=Json::array();
        for(const auto& cue:mediaRole.at("media")){if(cue.at("command").get<int>()>=282){media.push_back(cue);}}mediaRole["media"]=media;
        if(advanceRole(profile_.at("object"),mediaRole,npcPresentation_)){
            s.setVariable(260,1);const auto tile=findCharacter(*scene_.world_,profile_.at("object")).motion.tile();
            for(Direction direction:{Direction::Up,Direction::Right,Direction::Down,Direction::Left}){select(adjacentTile(tile,direction),1,true);s.setVariable(261,s.variable(263));}
            s.setVariable(261,rule.at("next_mode"));
        }return;
    }
    if(state==NpcMode::Retract){
        auto mediaRole=profile_.at("npc_roles").at(role);auto media=Json::array();
        const int direction=static_cast<int>(findCharacter(*scene_.world_,profile_.at("object")).motion.facing());
        // M72 E6 P15 selects one of four directional route/wait pairs.
        const int first=3+direction*5;
        for(const auto& cue:mediaRole.at("media")){const int command=cue.at("command");if(command==1||command==first||command==first+1){media.push_back(cue);}}mediaRole["media"]=media;
        if(advanceRole(profile_.at("object"),mediaRole,npcPresentation_)){s.setVariable(261,profile_.at("rules").at("retract").at("next_mode"));}return;
    }
    const auto npc=findCharacter(*scene_.world_,profile_.at("object"));const auto tile=npc.motion.tile();
    if(state==NpcMode::ClosePlayer){if(!s.flag(10)){s.setVariable(261,4);}else if(adjacentTile(tile,npc.motion.facing())==scene_.world_->player().motion.tile()){s.setVariable(261,11);}return;}
    if(state==NpcMode::ApproachTarget){const int n=s.variable(264);if(n<1||n>8||slots_[n-1].regeneration!=Regeneration::Alive||value(slots_[n-1],"damage")>=100){s.setVariable(261,4);return;}const auto target=findCharacter(*scene_.world_,row(slots_[n-1]).at("object")).motion.tile();if(std::abs(target.x-tile.x)+std::abs(target.y-tile.y)<=1){s.setVariable(261,profile_.at("rules").contains("close_attack")?9:11);}else{stepActor(npc.id,faceToward(tile,target,npc.motion.facing()));}return;}
    const auto& search=profile_.at("rules").at("search");int radius=s.variable(260);if(radius<1){radius=search.at("initial_radius");}s.setVariable(263,mode);
    bool found=false;
    if(state==NpcMode::Perimeter){
        std::vector<TilePosition> probes;
        for(int n=-radius;n<=radius;++n){probes.push_back({tile.x+n,tile.y-radius});}
        for(int n=-radius;n<=radius;++n){probes.push_back({tile.x+radius,tile.y+n});}
        for(int n=radius;n>=-radius;--n){probes.push_back({tile.x+n,tile.y+radius});}
        for(int n=radius;n>=-radius;--n){probes.push_back({tile.x-radius,tile.y+n});}
        for(const auto probe:probes){if(select(probe,radius)){found=true;break;}}
    }else{
        found=select(tile,radius);if(!found){for(Direction d:{Direction::Up,Direction::Right,Direction::Down,Direction::Left}){const auto offset=adjacentTile({0,0},d);if(select({tile.x+offset.x*radius,tile.y+offset.y*radius},radius)){found=true;break;}}}
    }
    if(found){s.setVariable(261,s.variable(263));s.setVariable(260,0);return;}
    if(state==NpcMode::Random){stepActor(npc.id,static_cast<Direction>(scene_.random_(0,3)));}else if(state==NpcMode::ApproachPlayer||state==NpcMode::LeavePlayer){const auto toward=faceToward(tile,scene_.world_->player().motion.tile(),npc.motion.facing());stepActor(npc.id,state==NpcMode::ApproachPlayer?toward:static_cast<Direction>((static_cast<int>(toward)+2)%4));}
    ++radius;s.setVariable(260,radius);
    const int limit=state==NpcMode::Neutral?search.at("neutral_exit_radius_at_least").get<int>():state==NpcMode::Perimeter?search.at("perimeter_exit_radius_at_least").get<int>():search.at("random_and_approach_exit_radius_at_least").get<int>();if(state!=NpcMode::LeavePlayer&&radius>=limit){s.setVariable(261,state==NpcMode::Perimeter?1:4);s.setVariable(260,0);}npcWait_=1;
}
void GuardController::updateAttachedGrenade(){
    auto& session=scene_.session_;const auto& grenade=profile_.at("attached_grenade");const int number=session.variable(grenade.at("occupied_slot_variable"));
    if(number<1||number>8){grenadeWait_=-1;grenadePresentation_={};return;}
    if(grenadeWait_<0){grenadeWait_=0;grenadePresentation_={"explode"};}
    const auto commit=[&](int command){
        if(command>=44&&grenadeWait_==0){
            const auto& anchors=grenade.at("anchor_slots");if(anchors.contains(std::to_string(number))){const auto screen=scene_.sampleActorScreen(row(slots_[number-1]).at("object"));session.setVariable(grenade.at("anchor_variables").at(0),static_cast<int>(screen.x));session.setVariable(grenade.at("anchor_variables").at(1),static_cast<int>(screen.y));}
            const auto& offset=grenade.at("picture_offset_pixels");session.setVariable(221,session.variable(221)+offset.at(0).get<int>());session.setVariable(222,session.variable(222)+offset.at(1).get<int>());
            grenadePresentation_.sampledScreen=GridPosition{double(session.variable(221)),double(session.variable(222))};grenadeWait_=1;
        }
        if(command>=51&&grenadeWait_==1){
            const auto& offset=grenade.at("damage_offset_from_picture_pixels");session.setVariable(250,session.variable(221)+offset.at(0).get<int>());session.setVariable(251,session.variable(222)+offset.at(1).get<int>());
            const auto camera=scene_.camera_.center();const GridPosition center{session.variable(250)+camera.x-160.,session.variable(251)+camera.y-120.};
            const std::array<int,2> half{grenade.at("half_pixels").at(0),grenade.at("half_pixels").at(1)};session.setVariable(47,half[0]);session.setVariable(48,half[1]);
            for(auto& slot:slots_){const auto& rowData=row(slot);if(slot.regeneration!=Regeneration::Alive||value(slot,"damage")>=100||session.variable(rowData.at("source_slots").at("event"))<grenade.at("registered_event_at_least").get<int>()){continue;}
                if(overlap(center,body(findCharacter(*scene_.world_,rowData.at("object"))),half)){hit(slot,grenade.at("receiver_page"),2000);session.setFlag(rowData.at("burn_fact"),true);}}
            grenadeWait_=2;
        }
    };
    if(advanceRole(profile_.at("object"),grenade.at("roles").at("explode"),grenadePresentation_,commit)){session.setVariable(grenade.at("occupied_slot_variable"),0);grenadeWait_=-1;}
}
void GuardController::updateSupport(){
    auto& s=scene_.session_;const auto& support=profile_.at("support");if(s.variable(268)>0){return;}
    const bool fixed=support.contains("spawn_tile");if(fixed&&s.variable(269)<support.at("player_kills_at_least").get<int>()){return;}
    if(supportWait_>0&&--supportWait_>0){return;}
    const int mode=s.variable(261);if((support.at("rescues_at_least").is_null()||s.variable(267)>=support.at("rescues_at_least").get<int>())&&s.variable(269)>=support.at("player_kills_at_least").get<int>()&&mode<=support.at("mode_at_most").get<int>()&&std::find(support.at("excluded_modes").begin(),support.at("excluded_modes").end(),mode)==support.at("excluded_modes").end()){
        const int item=s.hp()<=support.at("player_hp_at_most").get<int>()?support.at("low_hp_item_mode").get<int>():scene_.random_(1,fixed?3:2);s.setVariable(268,item);auto actor=findCharacter(*scene_.world_,support.at("object"));const auto npc=findCharacter(*scene_.world_,profile_.at("object"));actor.motion=CharacterMotion(fixed?xy(support.at("spawn_tile")):npc.motion.tile(),actor.motion.facing(),actor.motion.speed());replaceCharacter(*scene_.world_,actor);showVoice(support.at("voice_page"));
    }supportWait_=support.at("delay_tenths").get<int>()*6;
}
void GuardController::advanceTrack(CueTrack& track){
    if(track.wait>0&&--track.wait>0){return;}
    const auto npc=findCharacter(*scene_.world_,profile_.at("object"));
    while(track.cursor<track.cues.size()){
        const auto& id=track.cues[track.cursor++];const auto& cue=scene_.content_.cue(id);const std::string kind=cue.at("kind");
        if(kind=="wait"){track.wait=std::max(1,cue.at("tenths").get<int>()*6);return;}
        auto anchor=scene_.sampleActorScreen(npc.id);anchor.x+=track.offset[0];anchor.y+=track.offset[1];
        if(track.workspace[0]){scene_.session_.setVariable(track.workspace[0],static_cast<int>(anchor.x));scene_.session_.setVariable(track.workspace[1],static_cast<int>(anchor.y));}
        scene_.presentation_.push_back({PresentationEvent::Kind::Cue,id,{},npc.motion.position(),0,npc.id,anchor});
        if(kind=="move_picture"&&cue.value("wait",false)){track.wait=std::max(1,cue.at("duration_tenths").get<int>()*6);return;}
    }
}
void GuardController::showVoice(int page){
    const auto& v=profile_.at("presentation").at("voice");voice_={};
    for(const auto& cue:v.at("pages").at(std::to_string(page)).at("media")){voice_.cues.push_back(cue.at("cue"));}
    int reset=0;for(const auto& [number,row]:v.at("pages").items()){reset=std::max(reset,std::stoi(number));}if(page!=reset){for(const auto& cue:v.at("pages").at(std::to_string(reset)).at("media")){voice_.cues.push_back(cue.at("cue"));}}
    scene_.session_.setFlag(v.at("pulse_fact"),true);
}
void GuardController::beginNpcDeath(){
    dying_=true;const auto& d=profile_.at("rules").at("death");auto& s=scene_.session_;
    for(const auto& flag:d.at("disable_flags")){s.setFlag(flag,false);}s.setVariable(252,d.at("blood_type"));bloodType_=d.at("blood_type");s.setFlag(d.at("blood_pulse_fact"),true);
    deathPresentation_={"companion-dead"};applyRole(profile_.at("object"),profile_.at("npc_roles").at("companion-dead"));
    presentNpcPulses();advanceRole(profile_.at("object"),profile_.at("npc_roles").at("companion-dead"),deathPresentation_);
}
void GuardController::presentNpcPulses(){
    auto& s=scene_.session_;const auto& p=profile_.at("presentation");const auto npc=findCharacter(*scene_.world_,profile_.at("object"));
    if(s.flag(p.at("muzzle").at("fact"))){const auto& m=p.at("muzzle");scene_.cue(m.at("npc_flash"),npc.id);const auto d=std::string(directionName(findCharacter(*scene_.world_,profile_.at("projectile").at("object")).motion.facing()));
        if(m.at("directions").contains(d)){const auto& row=m.at("directions").at(d);muzzle_={};muzzle_.offset={row.at("offset_pixels").at(0),row.at("offset_pixels").at(1)};muzzle_.workspace={255,256};for(const auto& cue:row.at("cues")){muzzle_.cues.push_back(cue);}}
        s.setFlag(m.at("clear_fact"),false);}
    if(s.flag(p.at("blood").at("fact"))){const auto& b=p.at("blood");const auto& type=b.at("types").at(std::to_string(s.variable(252)));blood_={};blood_.offset={type.at("offset_pixels").at(0),type.at("offset_pixels").at(1)};blood_.workspace={253,254};
        for(const auto& cue:type.at("shows")){blood_.cues.push_back(cue);}for(const auto& stage:type.at("stages")){blood_.cues.push_back(stage.at("move"));if(!stage.at("wait_zero").is_null()){blood_.cues.push_back(stage.at("wait_zero"));}blood_.cues.push_back(stage.at("erase"));}
        s.setFlag(b.at("clear_fact"),false);}
    advanceTrack(muzzle_);advanceTrack(blood_);advanceTrack(voice_);
    if(voice_.cursor==voice_.cues.size()){s.setFlag(p.at("voice").at("pulse_fact"),false);}
}
void GuardController::update(const InputFrame& input){
    if(finished_){return;}auto& s=scene_.session_;sampleBodies();
    if(npcRecovery_>0&&--npcRecovery_==0){const auto& recovery=profile_.at("rescues").at("recovery");if(recovery.contains("sequence")){const auto& stage=recovery.at("sequence").at(recoveryStage_++);s.setFlag(222,stage.at("value"));if(recoveryStage_<static_cast<int>(recovery.at("sequence").size())){npcRecovery_=recovery.at("sequence").at(recoveryStage_).at("wait_tenths").get<int>()*6;}}else{s.setFlag(222,false);}}
    // Source burn channels outlive death/fade/hidden pages; Cerberus excludes corpse damage.
    for(auto& slot:slots_){if(row(slot).value("type",std::string{})=="dog"){dog_->updateParallel();continue;}EnemyHost host(scene_,row(slot).at("object"));const int damage=host.advanceBurn();if(damage&&slot.regeneration==Regeneration::Alive&&value(slot,"damage")<100){set(slot,"damage",value(slot,"damage")+damage);}}
    if(dying_){presentNpcPulses();if(advanceRole(profile_.at("object"),profile_.at("npc_roles").at("companion-dead"),deathPresentation_)){finished_=true;scene_.startDeath(false);}return;}
    if(s.flag(profile_.at("companion_dead_fact"))||hp_<=0){beginNpcDeath();return;}
    if(s.hp()<=0||s.flag(15)){applyRole(profile_.at("object"),profile_.at("npc_roles").at("player-dead"));return;}
    if(!s.flag(profile_.at("activation_fact"))){return;}
    const auto& orchestration=profile_.at("orchestration");const bool second=profile_.at("schema")=="nobihaza.guard-second-profile.v1";
    if(scene_.foreground_==ForegroundKind::None&&(s.variable(269)>=orchestration.at("quota").get<int>()||(s.eventTimer().source==(second?EventTimerSource::GuardFinal:EventTimerSource::GuardFirst)&&s.eventTimer().remainingTicks/60==0))){finished_=true;scene_.startContentFlow(std::string(second?"guard-second-end-":"guard-first-end-")+(s.variable(269)>=orchestration.at("quota").get<int>()?"2":"1"));return;}
    if((scene_.messageRequest_&&!scene_.dialogue_.view().display.continueEvents)||(scene_.inputRequest_&&!scene_.inputPrompt_.view().display.continueEvents)){return;}
    for(auto& slot:slots_){updateSlot(slot,input);}updateNpc();updateProjectile();updateAttachedGrenade();updateSupport();updateCoverFire();presentNpcPulses();
}

void GuardController::updateCoverFire(){
    if(!profile_.contains("cover_fire")){return;}const auto& cover=profile_.at("cover_fire");
    const auto commit=[&](int command){if(command<93||coverSelected_){return;}coverSelected_=true;
        const auto& range=cover.at("random_slot_inclusive");auto& slot=slots_.at(scene_.random_(range.at(0),range.at(1))-1);if(value(slot,"damage")>=cover.at("alive_damage_below").get<int>()||value(slot,"mode")>=cover.at("mode_below").get<int>()){return;}
        AttackSample sample{body(findCharacter(*scene_.world_,row(slot).at("object"))),{0,0},AttackKind::Ar,cover.at("receiver_page")};hit(slot,cover.at("receiver_page"),0,&sample);
    };
    if(advanceRole(cover.at("object"),cover.at("roles").at("2"),coverPresentation_,commit)){coverPresentation_={};coverSelected_=false;}
}
