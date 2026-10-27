#include "weapon_controller.h"
#include "projectile_policy.h"
#include "combat_scene.h"
#include "dora_controller.h"
#include "enemy_controller.h"
#include <algorithm>
#include <functional>
namespace{
    const ButtonState& button(const InputFrame& f,InputAction a){return f.buttons[static_cast<std::size_t>(a)];}
    ButtonState postureButton(const InputFrame& frame){
        const auto& main=button(frame,InputAction::Weapon);const auto& auxiliary=button(frame,InputAction::AuxiliaryWeapon);
        return {main.held||auxiliary.held,main.pressed||auxiliary.pressed,main.released||auxiliary.released};
    }
    class CueAction final:public WeaponAction{
    public:
        CueAction(std::vector<std::string> sequence,std::function<int(const std::string&)> emit,
                  std::function<bool()> routes,std::function<void()> cancel)
            :sequence_(std::move(sequence)),emit_(std::move(emit)),routes_(std::move(routes)),cancel_(std::move(cancel)){}
        void start() override{tick();}
        bool tick() override{
            if(cancelled_){return true;}if(wait_>0&&--wait_>0){return false;}
            while(next_<sequence_.size()){
                const auto& id=sequence_[next_];
                if(id=="wait-player-route"){if(routes_()){return false;}++next_;continue;}
                ++next_;wait_=emit_(id);if(wait_<0){next_=sequence_.size();return true;}if(wait_>0){return false;}
            }
            return true;
        }
        void cancel(ActionCancel) override{cancelled_=true;cancel_();}
        nlohmann::json saveState() const override{return {{"sequence",sequence_},{"next",next_},{"wait",wait_}};}
        void restoreState(const nlohmann::json& j) override{
            if(j.size()!=3||j.at("sequence")!=sequence_||!j.at("next").is_number_integer()||!j.at("wait").is_number_integer()){throw std::runtime_error("Invalid saved weapon action");}
            const int next=j.at("next"),wait=j.at("wait");if(next<0||next>int(sequence_.size())||wait<0||wait>360000){throw std::runtime_error("Invalid saved weapon cursor");}next_=next;wait_=wait;
        }
    private:
        std::vector<std::string> sequence_;std::function<int(const std::string&)> emit_;
        std::function<bool()> routes_;std::function<void()> cancel_;std::size_t next_{};int wait_{};bool cancelled_{};
    };
}
WeaponController::WeaponController(CombatScene& scene):scene_(scene){}
void WeaponController::requestAttackEnd(std::uint64_t id){if(id&&attack_&&attack_->id==id){requestedEnd_=id;}}
bool WeaponController::raised() const{return raised_;}
bool WeaponController::aiming() const{return aimed_;}
bool WeaponController::busy() const{return bool(action_)||pendingAttack_.has_value();}
void WeaponController::begin(const std::vector<std::string>& sequence,bool start){
    const auto epoch=scene_.generation_;
    action_=std::make_unique<CueAction>(sequence,[this,epoch](const std::string& id){
        if(epoch!=scene_.generation_){return 0;}
        if(id=="reload-finish"){scene_.session_.setFlag(17,false);scene_.session_.setFlag(19,false);return 0;}
        if(id=="reload-guard"){if(scene_.session_.flag(17)){return 0;}std::erase_if(scene_.routes_,[](const auto& route){return route.actor()=="player";});scene_.session_.setFlag(19,false);return -1;}
        if(id=="commit-grenade"){commitGrenade();return 0;}
        if(id=="finish-grenade"){grenadeFinish_=true;return 0;}
        if(id=="transact-gl-reload"){
            launcherReloadPending_=false;
            if(!infiniteAmmunition()&&scene_.session_.reload()==0){return -1;}
            return 0;
        }
        const auto& cue=scene_.content_.cue(id);
        if(cue.at("kind")=="wait"){return std::max(1,cue.at("tenths").get<int>()*6);}
        scene_.cue(id);return 0;
    },[this]{return std::any_of(scene_.routes_.begin(),scene_.routes_.end(),[](const auto& r){return r.actor()=="player";});},
    [this]{std::erase_if(scene_.routes_,[](const auto& r){return r.actor()=="player";});});
    if(start){action_->start();}
}
const nlohmann::json& WeaponController::binding() const{
    static const nlohmann::json legacy={{"kind","handgun"},{"raise","raise"},{"lower","lower"},{"fire","fire"},{"reload","reload"},{"aim",true},{"flash","ce7-c4"},{"effect_prefix","muzzle-"}};
    if(!scene_.content_.catalog.contains("weapon_bindings")){return legacy;}
    return scene_.content_.catalog.at("weapon_bindings").at(std::to_string(auxiliary_?scene_.session_.equipment().auxiliaryWeapon:scene_.session_.equipment().mainWeapon));
}
const nlohmann::json& WeaponController::mapProfile(int weapon) const{
    static const nlohmann::json empty=nlohmann::json::object();
    const auto& catalog=scene_.content_.catalog;const auto map=std::to_string(scene_.map_),item=std::to_string(weapon);
    if(!catalog.contains("weapon_map_profiles")||!catalog.at("weapon_map_profiles").contains(map)||!catalog.at("weapon_map_profiles").at(map).contains(item)){return empty;}
    return catalog.at("weapon_map_profiles").at(map).at(item);
}
const nlohmann::json& WeaponController::profile(const std::string& name) const{
    const auto action=[&](const std::string& id)->const nlohmann::json&{
        const auto& catalog=scene_.content_.catalog;if(name=="reload"&&catalog.contains("difficulty")&&catalog.at("difficulty").contains("reload_aliases")&&catalog.at("difficulty").at("reload_aliases").contains(id)){return scene_.content_.actionProfiles.at(catalog.at("difficulty").at("reload_aliases").at(id).get<std::string>());}
        return scene_.content_.actionProfiles.at(id);
    };
    if(name=="fire"&&(binding().at("kind")=="handgun"||binding().at("kind")=="handgun-hp"||binding().at("kind")=="sg"||binding().at("kind")=="saa")&&scene_.session_.flag(48)&&binding().contains("quick_fire")){return action(binding().at("quick_fire").get<std::string>());}
    if(name=="reload"&&!auxiliary_){
        if(scene_.content_.catalog.contains("difficulty")){
            const auto& rules=scene_.content_.catalog.at("difficulty");const auto weapon=std::to_string(scene_.session_.equipment().mainWeapon);
            if(rules.contains("reload_profiles")&&rules.at("reload_profiles").contains(scene_.session_.difficulty())&&rules.at("reload_profiles").at(scene_.session_.difficulty()).contains(weapon)){return action(rules.at("reload_profiles").at(scene_.session_.difficulty()).at(weapon).get<std::string>());}
        }
        if(binding().contains("quick_reload")&&scene_.session_.flag(48)){return action(binding().at("quick_reload").get<std::string>());}
        const auto& rules=scene_.content_.expansion.reloadEquipment;
        const auto weapon=std::to_string(scene_.session_.equipment().mainWeapon);
        if(rules.contains(weapon)){
            const auto& rule=rules.at(weapon);const int item=rule.at("equipment_item");
            const auto definition=scene_.content_.items.find(item);
            if(definition!=scene_.content_.items.end()&&scene_.session_.equipped(definition->second.equipmentType)==item){
                return action(rule.at("profile").get<std::string>());
            }
        }
    }
    return action(binding().at(name).get<std::string>());
}
void WeaponController::posture(bool raise){
    clearAim();postureTarget_=raise;
    begin(profile(raise?"raise":"lower").at("cue_order").get<std::vector<std::string>>());
}
void WeaponController::cancel(ActionCancel reason,bool preserveAimingAid){
    scene_.session_.setFlag(17,false);scene_.session_.setFlag(19,false);
    // CE69 cancellation jumps past recoil to its ammo commit before testing S17 again.
    if(launcherReloadPending_){launcherReloadPending_=false;if(!infiniteAmmunition()){scene_.session_.reload();}}
    if(launcherPresentation_){
        launcherPresentation_->cancel(reason);launcherPresentation_.reset();
        const auto& rule=scene_.content_.catalog.at("weapon_bindings").at(std::to_string(launcherPresentationWeapon_));
        for(const auto& cue:rule.at("explosion_cue_order")){if(scene_.content_.cue(cue).at("kind")=="erase_picture"){emitLauncherCue(cue);}}
    }
    if(action_){action_->cancel(reason);action_.reset();}if(attack_&&!(attack_->kind==AttackKind::Grenade&&reason==ActionCancel::Grabbed)){endAttack(false,preserveAimingAid);}grenadeFinish_=false;pendingAttack_.reset();pendingFireCues_.clear();pendingFireWait_=0;postureTarget_.reset();auxiliary_=false;knifeWindup_=false;raised_=false;aimed_=false;aimPending_=false;shiftCount_=cancelCount_=aimCount_=controlWait_=0;turned_=false;initialRaise_=false;controlSample_.reset();
    if(reason!=ActionCancel::Grabbed&&grenade_){scene_.cue(scene_.content_.branch.at("action_rules").at("weapons").at("29").at("presentation").at("erase_cue"));grenade_.reset();}
    if(!preserveAimingAid){scene_.session_.clearAimingAid();}
    auto p=scene_.world_->player();p.spriteId=scene_.definition_.player.spriteId;p.setSpeedLevel(4);scene_.world_->setPlayerState(p);scene_.session_.setFlag(3,false);
}
void WeaponController::reload(){
    if(scene_.session_.reload(auxiliary_?2:1)==0){return;}
    const auto sequence=profile("reload").at("cue_order").get<std::vector<std::string>>();
    if(std::find(sequence.begin(),sequence.end(),"reload-guard")!=sequence.end()){scene_.session_.setFlag(17,true);scene_.session_.setFlag(19,true);}
    // CE53 commits inventory before the first presentation command. Route waits and event waits are separate.
    if(profile("reload").contains("sound_cue")){scene_.playSound(profile("reload").at("sound_cue"));}
    begin(sequence);
}
bool WeaponController::infiniteAmmunition() const{
    const auto& full=*scene_.content_.full;
    const auto item=std::to_string(scene_.session_.equipped(1));
    const auto& catalog=scene_.content_.catalog;
    return (!auxiliary_&&catalog.contains("weapon_bindings")&&catalog.at("weapon_bindings").contains(item)&&catalog.at("weapon_bindings").at(item).value("infinite",false))||(full.contains("runtime")&&full.at("runtime").contains("weapon_actions")&&
        scene_.session_.equipment().accessory==full.at("runtime").at("weapon_actions").at("equipment_rules").at("infinite_ammo_item"));
}
void WeaponController::fire(){
    if(auxiliary_&&binding().at("kind")=="grenade"){if(!attack_){begin(profile("fire").at("cue_order").get<std::vector<std::string>>());}return;}
    if(auxiliary_&&binding().at("kind")=="knife"){knifeWindup_=true;begin(profile("fire").at("cue_order").get<std::vector<std::string>>());return;}
    const bool freeRound=infiniteAmmunition()&&(!auxiliary_||binding().at("kind")!="lc9");
    if(!freeRound&&!scene_.session_.canFireRound(auxiliary_?2:1)){return;}
    if(binding().contains("special_event")){scene_.session_.recordEndingSpecialAction(binding().at("special_event"));}
    scene_.playSound(profile("fire").at("sound_cue"));
    if(binding().contains("direction_cues")){scene_.cue(binding().at("direction_cues").at(std::to_string(int(scene_.world_->player().motion.facing()))));}
    if(profile("fire").contains("start_cue_order")){for(const auto& cue:profile("fire").at("start_cue_order")){scene_.cue(cue);}}
    if(!freeRound&&!scene_.session_.consumeRound(auxiliary_?2:1)){return;}
    if(binding().contains("flash")&&!binding().at("flash").get<std::string>().empty()){scene_.cue(binding().at("flash"));}
    const auto& p=scene_.world_->player();
    const auto tile=p.motion.tile();
    const bool replacing=attack_.has_value();if(replacing){endAttack();}
    attack_=AttackState{{static_cast<double>(tile.x),static_cast<double>(tile.y)},p.motion.facing(),0,0,true,scene_.generation_,true,{},AttackKind::Handgun,std::nullopt};
    attack_->id=nextAttackId_++;
    if(binding().at("kind")=="de"||binding().at("kind")=="shotgun"){
        attack_->kind=binding().at("kind")=="de"?AttackKind::De:binding().value("damage_kind",std::string{})=="semi-auto-shotgun"?AttackKind::SemiAutoShotgun:AttackKind::Shotgun;attack_->definition=scene_.content_.expansion.attacks.at(scene_.session_.equipment().mainWeapon);attack_->definitionWeapon=scene_.session_.equipment().mainWeapon;attack_->muzzle=attack_->definition->muzzleTest;
    }
    const auto& variation=mapProfile(scene_.session_.equipped(auxiliary_?2:1));
    attack_->through=variation.value("through",false);
    if(attack_->definition&&variation.contains("flight_calls")){attack_->definition->flightCalls=variation.at("flight_calls");}
    if(binding().at("kind")=="handgun-hp"){attack_->kind=AttackKind::HandgunHp;}
    if(binding().at("kind")=="sg"){attack_->kind=AttackKind::Sg;}
    if(binding().at("kind")=="lc9"){attack_->kind=AttackKind::Lc9;}
    if(binding().at("kind")=="magnum"){attack_->muzzle=false;attack_->kind=AttackKind::Magnum;}
    if(binding().at("kind")=="saa"){attack_->kind=AttackKind::Saa;}
    if(binding().at("kind")=="ar"){attack_->kind=binding().value("infinite",false)?AttackKind::InfiniteAr:AttackKind::Ar;attack_->muzzle=false;}
    if(binding().at("kind")=="gl"||binding().at("kind")=="rocket"){
        const auto ammunition=binding().value("ammunition",std::string("normal"));
        static const std::map<std::string,AttackKind> kinds{{"normal",AttackKind::GlNormal},{"flame",AttackKind::GlFlame},{"acid",AttackKind::GlAcid}};
        attack_->kind=binding().at("kind")=="rocket"?AttackKind::Rocket:kinds.at(ammunition);launcherReloadPending_=binding().at("kind")=="gl";
        attack_->muzzle=false;launcher_.emplace(LauncherFlight{CharacterMotion(tile,p.motion.facing(),1<<(1+binding().at("projectile").at("speed").get<int>())),scene_.session_.equipped(1)});
    }
    if(binding().contains("effect_prefix")){
        const auto effect=binding().at("effect_prefix").get<std::string>()+std::string(directionName(p.motion.facing()));
        if(scene_.content_.media.at("effects").contains(effect)){scene_.presentation_.push_back({PresentationEvent::Kind::Effect,effect,{},p.motion.position(),0});}
    }
    if(replacing&&profile("fire").contains("replacement_wait_tenths")){
        pendingAttack_=std::move(attack_);attack_.reset();pendingFireCues_=profile("fire").at("cue_order").get<std::vector<std::string>>();
        pendingFireWait_=std::max(1,profile("fire").at("replacement_wait_tenths").get<int>()*6);return;
    }
    begin(profile("fire").at("cue_order").get<std::vector<std::string>>());
}
void WeaponController::strikeKnife(){
    knifeWindup_=false;
    if(profile("fire").contains("sound_cue")){scene_.playSound(profile("fire").at("sound_cue"));}
    const auto& p=scene_.world_->player();const auto position=p.motion.position();const auto delta=adjacentTile({0,0},p.motion.facing());
    std::set<std::string> resolved;
    const GridPosition center{(position.x+.5)*16+delta.x*16,(position.y+1)*16-8+delta.y*16};
    if(binding().contains("strike_effect_prefix")){scene_.presentation_.push_back({PresentationEvent::Kind::Effect,binding().at("strike_effect_prefix").get<std::string>()+std::string(directionName(p.motion.facing())),{},position,0});}
    if(binding().contains("half_pixels_by_direction")){
        scene_.resolveAttack({center,binding().at("half_pixels_by_direction").at(static_cast<int>(p.motion.facing())).get<std::array<int,2>>(),AttackKind::Knife,2,0,false},resolved);
    }else{scene_.resolveAttack(center,false,0,false,resolved,AttackKind::Knife);}
    begin(profile("recover").at("cue_order").get<std::vector<std::string>>());
}
void WeaponController::update(const InputFrame& input){
    if(scene_.bonusRoom()){scene_.movePlayer(input);return;}
    if(pendingAttack_){
        if(--pendingFireWait_<=0){attack_=std::move(pendingAttack_);pendingAttack_.reset();begin(pendingFireCues_);pendingFireCues_.clear();}return;
    }
    if(scene_.session_.conditions().recoveryTicks>0){scene_.movePlayer(input);return;}
    if(aimPending_&&(!postureButton(input).held||input.movement)){
        action_->cancel(ActionCancel::Foreground);action_.reset();clearAim();
        auto p=scene_.world_->player();p.spriteId=scene_.content_.actionProfiles.at("aim").at("restore_sprite").get<std::string>();scene_.world_->setPlayerState(p);
    }
    if(action_){if(action_->tick()){action_.reset();
        if(postureTarget_){
            raised_=*postureTarget_;
            if(!raised_&&profile("lower").contains("restore_facing")){
                const auto facing=static_cast<int>(scene_.world_->player().motion.facing());
                scene_.cue(profile("lower").at("restore_facing").at(std::to_string(facing)));
            }
            postureTarget_.reset();
        }
        if(grenadeFinish_){
            grenadeFinish_=false;const bool remains=scene_.session_.equipment().auxiliaryWeapon==29;
            begin(scene_.content_.actionProfiles.at(remains?"grenade-finish-remains":"grenade-finish-empty").at("cue_order").get<std::vector<std::string>>());
            if(!remains){raised_=false;auxiliary_=false;}return;
        }
        if(knifeWindup_){strikeKnife();return;}
        if(aimPending_){aimPending_=false;aimed_=true;scene_.session_.setFlag(159,true);}
        if(initialRaise_&&!postureButton(input).held){initialRaise_=false;shiftCount_=aimCount_=0;}
    }return;}
    const auto& rules=scene_.content_.rules.at("input");
    const auto& cancel=button(input,InputAction::Cancel);
    const auto& fireInput=button(input,InputAction::Attack);
    const auto& auxiliary=button(input,InputAction::AuxiliaryWeapon);
    if(!raised_&&(auxiliary.held||auxiliary.pressed)){
        if(!scene_.content_.catalog.contains("weapon_bindings")){if(auxiliary.pressed){scene_.presentation_.push_back({PresentationEvent::Kind::Notice,"副武器需要F3资源入口",{},{},180});}return;}
        if(!scene_.session_.equipment().auxiliaryWeapon){return;}
        auxiliary_=true;
    }else if(!raised_&&(button(input,InputAction::Weapon).held||button(input,InputAction::Weapon).pressed)){auxiliary_=false;}
    const auto shift=postureButton(input);
    if(aimed_&&!shift.held){clearAim();auto p=scene_.world_->player();p.spriteId=scene_.content_.actionProfiles.at("aim").at("restore_sprite").get<std::string>();scene_.world_->setPlayerState(p);}
    if(!scene_.session_.flag(18)){scene_.movePlayer(input);return;}
    if(!raised_&&cancel.pressed&&scene_.session_.flag(1)){
        scene_.session_.organizeAmmo();scene_.requestedMode_=ApplicationMode::Equipment;return;
    }
    if(!(auxiliary_?scene_.session_.equipment().auxiliaryWeapon:scene_.session_.equipment().mainWeapon)){scene_.movePlayer(input);return;}
    // CE77 samples Shift/X/direction twice around Wait0; a changed sample does not count.
    bool sampled=false;
    if(controlWait_>0){
        controlWait_=0;
        sampled=controlSample_&&controlSample_->movement==input.movement&&
            postureButton(*controlSample_).held==shift.held&&
            button(*controlSample_,InputAction::Cancel).held==cancel.held;
        if(sampled){
            if(shift.held){++shiftCount_;if(!input.movement){++aimCount_;}}
            if(cancel.held){++cancelCount_;}
        }
    }else{controlSample_=input;controlWait_=1;}
    if((shift.held||shift.pressed)&&!raised_){initialRaise_=true;posture(true);return;}
    if(sampled&&shift.held&&shiftCount_>=rules.at("shift_turn_count_min").get<int>()&&input.movement){turned_=true;clearAim();scene_.movePlayer(input,false,true);}
    if(sampled&&!shift.held&&(shiftCount_>0||initialRaise_)){
        aimCount_=0;
        if(shiftCount_>=rules.at("shift_release_count_min").get<int>()&&shiftCount_<rules.at("shift_release_count_max_exclusive").get<int>()&&!turned_&&!initialRaise_){posture(false); }
        shiftCount_=0;turned_=false;initialRaise_=false;
        if(aimed_){aimed_=false;scene_.session_.clearAimingAid();auto p=scene_.world_->player();p.spriteId=scene_.content_.actionProfiles.at("aim").at("restore_sprite").get<std::string>();scene_.world_->setPlayerState(p);}
        return;
    }
    if(sampled&&raised_&&binding().at("aim").get<bool>()&&!aimed_&&shift.held&&aimCount_>=rules.at("aim_count_min").get<int>()&&!input.movement){
        aimPending_=true;begin(scene_.content_.actionProfiles.at("aim").at("cue_order").get<std::vector<std::string>>());return;
    }
    if(sampled&&!cancel.held&&cancelCount_>0){
        if(raised_&&(!auxiliary_||binding().at("kind")=="lc9")&&!input.movement&&cancelCount_>=rules.at("reload_release_count_min").get<int>()&&cancelCount_<=rules.at("reload_release_count_max").get<int>()){reload();}
        cancelCount_=0;if(action_){return;}
    }
    if(raised_&&auxiliary_&&fireInput.pressed){
        if(scene_.dora_&&scene_.dora_->consumeAttack(true)){return;}
        if(binding().at("kind")!="lc9"||scene_.session_.canFireRound(2)){fire();}
        else if(scene_.session_.hasReserve(2)){reload();}else{scene_.playSound(binding().at("empty_sound"));}return;
    }
    if(raised_&&!auxiliary_&&(fireInput.held||fireInput.pressed)){
        if(launcher_){if(binding().at("kind")=="gl"&&fireInput.pressed){explodeLauncher();}return;}
        if(infiniteAmmunition()||scene_.session_.magazine(scene_.session_.equipment().mainWeapon)>0){if(scene_.dora_&&scene_.dora_->consumeAttack(false)){return;}fire();return;}
        if(fireInput.pressed||(scene_.dora_&&(scene_.session_.variable(50)==55||scene_.session_.variable(7)>=57))){if(scene_.dora_&&scene_.dora_->consumeAttack(false)){return;}if(binding().at("kind")!="rocket"){if(scene_.session_.hasReserve()){reload();}else{scene_.playSound(binding().value("empty_sound",std::string("ce39-c410")));}}return;}
    }
    if(!shift.held){
        const bool locked=raised_&&cancel.held;
        if(scene_.session_.flag(3)!=locked){
            int speed=locked?binding().value("locked_speed",3):4;
            if(locked&&scene_.content_.full->contains("runtime")&&scene_.content_.full->at("runtime").contains("weapon_actions")){
                const auto& rules=scene_.content_.full->at("runtime").at("weapon_actions").at("equipment_rules");
                speed=scene_.session_.equipment().accessory==rules.at("run_shoes_item")?rules.at("shoe_speed").get<int>():binding().value("locked_speed",rules.at("walking_speed").get<int>());
            }
            auto player=scene_.world_->player();player.setSpeedLevel(speed);scene_.world_->setPlayerState(player);
            scene_.session_.setFlag(3,locked);
        }
        scene_.movePlayer(input,locked);
    }
}
void WeaponController::advanceAttack(){
    if(launcherPresentation_&&launcherPresentation_->tick()){launcherPresentation_.reset();}
    if(grenade_){advanceGrenade();}
    if(launcher_){advanceLauncher();return;}
    if(!attack_){return;}auto& a=*attack_;
    if(requestedEnd_==a.id){endAttack();return;}
    if(a.generation!=scene_.generation_){attack_.reset();return;}
    if(a.startYield){a.startYield=false;return;}
    if(a.kind==AttackKind::Grenade){return;}
    const auto delta=adjacentTile({0,0},a.direction);
    const bool shotgun=a.definition&&a.definition->strategy==ProjectileStrategy::ShotgunDistance;
    const int offset=a.muzzle?8:16;
    const GridPosition center{(a.position.x+.5)*16+delta.x*offset,(a.position.y+1)*16-8+delta.y*offset};
    const bool ce42=a.kind==AttackKind::Saa||a.kind==AttackKind::InfiniteAr||a.kind==AttackKind::Sg||a.kind==AttackKind::Lc9||a.kind==AttackKind::HandgunHp;
    const int main=scene_.session_.equipped(1);
    const int receiver=a.muzzle?2:main==20||main==21||main==58?5:scene_.session_.flag(159)||scene_.session_.variable(35)<=0?3:2;
    AttackSample sample;
    if(a.kind==AttackKind::Ar||a.kind==AttackKind::InfiniteAr){
        const int hits=scene_.session_.variable(35);
        sample={center,{5,5},a.kind,a.kind==AttackKind::InfiniteAr?(scene_.session_.flag(159)||hits<=0?3:2):(scene_.session_.flag(159)||(hits>0&&hits%3==0)?3:2),a.advances,aimed_};
    }else if(ce42){sample={center,{a.muzzle?1:5,a.muzzle?1:5},a.kind,receiver,a.advances,aimed_};}
    else if(a.definition){sample=sampleProjectile(*a.definition,a.position,a.direction,a.muzzle,a.advances);}
    else{
        const int page=a.kind==AttackKind::Magnum?5:!a.muzzle&&(aimed_||scene_.session_.variable(35)<=0)?4:a.muzzle?2:3;
        sample={center,{a.muzzle?1:5,a.muzzle?1:5},a.kind,page,a.advances,aimed_};
    }
    if(a.kind==AttackKind::SemiAutoShotgun&&sample.kind==AttackKind::Shotgun){sample.kind=a.kind;}
    sample.attackId=a.id;const auto hit=scene_.resolveAttack(sample,a.resolved);
    // CE41 clears S16 after its muzzle calls. CE43 only honors a target's
    // actual end request at its counter threshold; contact alone is insufficient.
    if(requestedEnd_==a.id||(ce42?hit.endRequested:shotgun?(!a.muzzle&&hit.endRequested&&a.advances>=a.definition->endOnHitFromAdvance):((hit.processed>0&&(!a.definition||a.definition->endOnTarget))||hit.endRequested))){endAttack();return;}
    if(a.muzzle){a.muzzle=false;return;}
    const int x=static_cast<int>(a.position.x),y=static_cast<int>(a.position.y);
    if(x<0||y<0||x>=scene_.definition_.width||y>=scene_.definition_.height||(!a.through&&scene_.world_->mappedLowerTerrain({x,y}).value_or(scene_.content_.terrainIds.at(scene_.map_).at(y*scene_.definition_.width+x))==(a.definition?a.definition->wallTerrain:scene_.content_.rules.at("attack_geometry").at("blocking_terrain_id").get<int>()))){endAttack();return;}
    if(shotgun||ce42){
        a.position.x+=delta.x;a.position.y+=delta.y;++a.advances;
        if(a.advances>=(a.definition?a.definition->flightCalls:19)){endAttack();}
    }else{
        a.position.x+=delta.x*.5;a.position.y+=delta.y*.5;
        if(++a.movementUpdates%2==0){++a.advances;}
        if(a.advances>=19){endAttack();}
    }
}

void WeaponController::clearAim(bool preserveAimingAid){
    aimCount_=0;aimed_=false;aimPending_=false;if(!preserveAimingAid){scene_.session_.clearAimingAid();}
}
void WeaponController::explodeLauncher(){
    if(!launcher_||!attack_||launcher_->stage>=0){return;}
    launcher_->stage=0;launcher_->wait=0;attack_->resolved.clear();
    const auto& rule=scene_.content_.catalog.at("weapon_bindings").at(std::to_string(launcher_->weapon));
    launcherPresentationWeapon_=launcher_->weapon;launcherPresentationPosition_=launcher_->motion.position();
    if(rule.contains("explosion_cue_order")){
        launcherPresentation_=std::make_unique<CueAction>(rule.at("explosion_cue_order").get<std::vector<std::string>>(),[this](const std::string& cue){return emitLauncherCue(cue);},[]{return false;},[]{});
        launcherPresentation_->start();
    }
}
int WeaponController::emitLauncherCue(const std::string& cue){
    const auto& value=scene_.content_.cue(cue);
    if(value.at("kind")=="wait"){return std::max(1,value.at("tenths").get<int>()*6);}
    if(value.at("kind")=="sound"){scene_.playSound(cue);return 0;}
    const auto& rule=scene_.content_.catalog.at("weapon_bindings").at(std::to_string(launcher_?launcher_->weapon:launcherPresentationWeapon_));
    const auto p=launcher_?launcher_->motion.position():launcherPresentationPosition_;const auto camera=scene_.camera_.center();
    GridPosition screen{(p.x+.5)*16-camera.x+160,(p.y+.5)*16-camera.y+120};
    if(rule.contains("presentation_positions")&&rule.at("presentation_positions").contains(cue)){
        const auto& offset=rule.at("presentation_positions").at(cue);screen.x+=offset.at(0).get<int>();screen.y+=offset.at(1).get<int>();
    }
    const GridPosition world{(screen.x+camera.x-160)/16-.5,(screen.y+camera.y-120)/16-1};
    scene_.presentation_.push_back({PresentationEvent::Kind::Cue,cue,{},world,0,"launcher",screen});return 0;
}
void WeaponController::advanceLauncher(){
    if(!attack_||attack_->generation!=scene_.generation_){launcher_.reset();attack_.reset();return;}
    auto& flight=*launcher_;auto& attack=*attack_;
    const auto& rule=scene_.content_.catalog.at("weapon_bindings").at(std::to_string(flight.weapon));
    const auto emit=[&](const std::string& cue){emitLauncherCue(cue);};
    if(attack.startYield){attack.startYield=false;if(rule.contains("flight_cue_order")){for(const auto& cue:rule.at("flight_cue_order")){emit(cue);}}return;}
    if(flight.stage<0){
        const auto p=flight.motion.position();const auto delta=adjacentTile({0,0},flight.motion.facing());
        AttackSample sample{{(p.x+.5)*16+delta.x*8,(p.y+.5)*16+delta.y*8},{3,3},attack.kind,1,0,false,attack.id,AttackPhase::Flight};
        const auto hit=scene_.resolveAttack(sample,attack.resolved);
        if(hit.contacts){explodeLauncher();}
        else if(!flight.motion.moving()){
            auto projectile=rule.at("projectile");const auto& variation=mapProfile(flight.weapon);
            if(variation.contains("launcher_steps")){projectile["steps_forward"]=variation.at("launcher_steps");}
            if(variation.contains("through")){projectile["through"]=variation.at("through");}
            while(flight.steps<projectile.at("steps_forward").get<int>()){
                CharacterState actor{"launcher",0,flight.motion};actor.layer=projectile.at("layer");actor.through=projectile.at("through");
                const bool allowed=scene_.world_->canMove(actor,flight.motion.facing());
                if(allowed){flight.motion.beginStep(flight.motion.facing());}
                // CN Map72 E1P5 has a skippable route: a blocked forward still
                // consumes an attempt; frequency8 skips consecutive failures now.
                if(allowed||projectile.at("skip_blocked").get<bool>()){++flight.steps;}
                if(allowed||!projectile.at("skip_blocked").get<bool>()){break;}
            }
            if(!flight.motion.moving()&&flight.steps>=projectile.at("steps_forward").get<int>()){explodeLauncher();}
        }
        if(flight.stage<0){flight.motion.advance();attack.position=flight.motion.position();if(rule.contains("flight_move_cue")){emit(rule.at("flight_move_cue"));}return;}
    }
    if(flight.wait>0&&--flight.wait>0){return;}
    const auto& stages=rule.at("explosion_stages");
    if(flight.stage>=static_cast<int>(stages.size())){endAttack();return;}
    const auto& stage=stages.at(flight.stage++);const auto p=flight.motion.position();
    for(const auto& cue:stage.at("cue_order")){emit(cue);}
    attack.resolved.clear();
    const auto identity=attack.id;
    scene_.resolveAttack({{(p.x+.5)*16,(p.y+.5)*16},stage.at("half_pixels").get<std::array<int,2>>(),attack.kind,stage.at("receiver"),0,false,attack.id,stage.at("phase")=="burning"?AttackPhase::Burning:AttackPhase::Impact},attack.resolved);
    if(launcher_&&attack_&&attack_->id==identity){launcher_->wait=stage.at("wait_ticks");}
}
void WeaponController::endAttack(bool preserveExplosion,bool preserveAimingAid){
    if(attack_){scene_.session_.setFlag(47,false);}attack_.reset();requestedEnd_=0;clearAim(preserveAimingAid);
    if(grenade_&&!preserveExplosion){scene_.cue(scene_.content_.branch.at("action_rules").at("weapons").at("29").at("presentation").at("erase_cue"));grenade_.reset();}
    if(launcher_){
        const auto& rule=scene_.content_.catalog.at("weapon_bindings").at(std::to_string(launcher_->weapon));
        if(rule.contains("flight_erase_cue")){emitLauncherCue(rule.at("flight_erase_cue"));}launcher_.reset();
    }
}

void WeaponController::commitGrenade(){
    const auto& rule=scene_.content_.branch.at("action_rules").at("weapons").at("29");
    if(scene_.session_.equipment().auxiliaryWeapon!=29){throw std::runtime_error("Grenade equipment changed before commit");}
    const auto& p=scene_.world_->player();const auto tile=p.motion.tile();
    attack_=AttackState{{double(tile.x),double(tile.y)},p.motion.facing(),0,0,false,scene_.generation_,true,{},AttackKind::Grenade,{}};
    attack_->id=nextAttackId_++;
    grenade_.emplace(GrenadeFlight{CharacterMotion(tile,p.motion.facing(),1<<(1+rule.at("projectile").at("speed").get<int>()))});
    scene_.presentation_.push_back({PresentationEvent::Kind::Cue,rule.at("presentation").at("projectile_cue"),{},p.motion.position(),0,"grenade"});
}

void WeaponController::advanceGrenade(){
    auto& g=*grenade_;const auto& rule=scene_.content_.branch.at("action_rules").at("weapons").at("29");const auto& visual=rule.at("presentation");
    const auto emit=[&](const std::string& cue,int offset){auto position=g.motion.position();position.y-=double(offset)/16;scene_.presentation_.push_back({PresentationEvent::Kind::Cue,cue,{},position,0,"grenade"});};
    if(g.explosionAge>=0){
        ++g.explosionAge;const auto& frames=visual.at("explosion_frames");
        if(g.explosionAge%2==0&&g.explosionAge/2<static_cast<int>(frames.size())){emit(frames.at(g.explosionAge/2).at("cue"),12);}
        if(g.explosionAge==7&&attack_){endAttack(true);}
        if(g.explosionAge>=2*static_cast<int>(frames.size())){emit(visual.at("erase_cue"),0);grenade_.reset();}return;
    }
    if(attack_&&attack_->startYield){return;}
    if(g.age==0&&g.steps==0&&scene_.session_.equipment().accessory!=rule.at("consumption").at("except_accessory").get<int>()){
        if(!scene_.session_.consumeEquippedItem(2)){throw std::runtime_error("Grenade unavailable at CE41 commit");}
    }
    if(!g.motion.moving()&&g.steps<rule.at("projectile").at("steps_forward").get<int>()){
        const auto next=adjacentTile(g.motion.tile(),g.motion.facing());++g.steps;
        // Original through ignores walls; a failed step beyond map bounds is skipped.
        if(next.x>=0&&next.y>=0&&next.x<scene_.definition_.width&&next.y<scene_.definition_.height){g.motion.beginStep(g.motion.facing());}
    }
    g.motion.advance();++g.age;
    for(const auto& band:visual.at("height_counter_bands")){if(g.age>=band.at("min").get<int>()&&(band.at("max").is_null()||g.age<=band.at("max").get<int>())){g.height=std::max(0,g.height+band.at("delta").get<int>());break;}}
    emit(visual.at("move_cue"),g.height);if(attack_){attack_->position=g.motion.position();}
    if(g.steps==rule.at("projectile").at("steps_forward").get<int>()&&!g.motion.moving()){
        g.explosionAge=0;scene_.playSound(rule.at("explosion").at("sound_cue"));emit(visual.at("flash_cue"),0);emit(visual.at("shake_cue"),0);emit(visual.at("explosion_frames").at(0).at("cue"),12);
        if(attack_){const auto p=g.motion.position();scene_.resolveAttack({{(p.x+.5)*16,(p.y+.5)*16},{30,30},AttackKind::Grenade,6,g.age,false},attack_->resolved);}
    }
}

#include "warehouse_save_state.h"
using namespace warehouse_save;
nlohmann::json WeaponController::captureWarehouseState() const{
 const auto attack=[&](const std::optional<AttackState>& state)->Json{if(!state){return nullptr;}const auto& a=*state;return {{"position",xy(a.position)},{"direction",int(a.direction)},{"advances",a.advances},{"updates",a.movementUpdates},{"muzzle",a.muzzle},{"start_yield",a.startYield},{"resolved",a.resolved},{"kind",int(a.kind)},{"definition",a.definition? a.definitionWeapon:0},{"through",a.through},{"end_requested",requestedEnd_==a.id}};};
 Json control=nullptr;if(controlSample_){Json buttons=Json::array();for(const auto& b:controlSample_->buttons){buttons.push_back({b.held,b.pressed,b.released});}control={{"buttons",buttons},{"movement",controlSample_->movement?Json(int(*controlSample_->movement)):Json()}};}
 return {{"switches",{auxiliary_,knifeWindup_,grenadeFinish_,launcherReloadPending_,raised_,aimed_,aimPending_,turned_,initialRaise_}},{"counters",{shiftCount_,aimCount_,cancelCount_,controlWait_,pendingFireWait_}},{"control",control},{"posture",postureTarget_?Json(*postureTarget_):Json()},{"action",action_?action_->saveState():Json()},{"attack",attack(attack_)},{"pending_attack",attack(pendingAttack_)},{"pending_cues",pendingFireCues_},{"grenade",grenade_?Json{{"motion",WarehouseSaveState::motion(grenade_->motion)},{"steps",grenade_->steps},{"age",grenade_->age},{"height",grenade_->height},{"explosion_age",grenade_->explosionAge}}:Json()},{"launcher",launcher_?Json{{"motion",WarehouseSaveState::motion(launcher_->motion)},{"weapon",launcher_->weapon},{"steps",launcher_->steps},{"stage",launcher_->stage},{"wait",launcher_->wait}}:Json()},{"explosion",launcherPresentation_?Json{{"weapon",launcherPresentationWeapon_},{"position",xy(launcherPresentationPosition_)},{"action",launcherPresentation_->saveState()}}:Json()}};
}
void WeaponController::restoreWarehouseState(const nlohmann::json& j){
 keys(j,{"switches","counters","control","posture","action","attack","pending_attack","pending_cues","grenade","launcher","explosion"});const auto& bits=j.at("switches");need(bits.is_array()&&bits.size()==9,"Warehouse weapon switches");bool* bs[]={&auxiliary_,&knifeWindup_,&grenadeFinish_,&launcherReloadPending_,&raised_,&aimed_,&aimPending_,&turned_,&initialRaise_};for(int i=0;i<9;++i){*bs[i]=boolean(bits[i]);}
 const auto& counts=j.at("counters");need(counts.is_array()&&counts.size()==5,"Warehouse weapon counters");int* cs[]={&shiftCount_,&aimCount_,&cancelCount_,&controlWait_,&pendingFireWait_};for(int i=0;i<5;++i){*cs[i]=integer(counts[i],0,360000);}
 controlSample_.reset();if(!j.at("control").is_null()){const auto& v=j.at("control");keys(v,{"buttons","movement"});InputFrame input;const auto& b=v.at("buttons");need(b.is_array()&&b.size()==input.buttons.size(),"Warehouse input shape");for(int i=0;i<int(b.size());++i){need(b[i].is_array()&&b[i].size()==3,"Warehouse button shape");input.buttons[i]={boolean(b[i][0]),boolean(b[i][1]),boolean(b[i][2])};}if(!v.at("movement").is_null()){input.movement=Direction(integer(v.at("movement"),0,3));}controlSample_=input;}
 postureTarget_=j.at("posture").is_null()?std::optional<bool>{}:boolean(j.at("posture"));
 const auto legalSequence=[&](const Json& value){need(value.is_array()&&value.size()<=512,"Warehouse weapon sequence shape");const auto sequence=value.get<std::vector<std::string>>();bool legal=false;for(const auto& [id,p]:scene_.content_.actionProfiles.items()){if(p.contains("cue_order")&&p.at("cue_order")==value){legal=true;break;}}need(legal,"Unreviewed warehouse weapon sequence");return sequence;};
 action_.reset();if(!j.at("action").is_null()){begin(legalSequence(j.at("action").at("sequence")),false);action_->restoreState(j.at("action"));}
 attack_.reset();pendingAttack_.reset();requestedEnd_=0;nextAttackId_=1;
 const auto readAttack=[&](const Json& v)->std::optional<AttackState>{if(v.is_null()){return {};}keys(v,{"position","direction","advances","updates","muzzle","start_yield","resolved","kind","definition","through","end_requested"});AttackState a;a.position=grid(v.at("position"),-1,std::max(scene_.definition_.width,scene_.definition_.height)+1);a.direction=Direction(integer(v.at("direction"),0,3));a.advances=integer(v.at("advances"),0,128);a.movementUpdates=integer(v.at("updates"),0,256);a.muzzle=boolean(v.at("muzzle"));a.startYield=boolean(v.at("start_yield"));a.kind=AttackKind(integer(v.at("kind"),0,int(AttackKind::InfiniteAr)));if(a.kind==AttackKind::Saa||a.kind==AttackKind::InfiniteAr){const int item=a.kind==AttackKind::Saa?56:57;const auto& bindings=scene_.content_.catalog.at("weapon_bindings");need(bindings.contains(std::to_string(item))&&bindings.at(std::to_string(item)).at("kind")== (item==56?"saa":"ar")&&(item==56||bindings.at("57").value("infinite",false)),"Unavailable saved special projectile");}a.through=boolean(v.at("through"));need(a.kind!=AttackKind::Knife,"Knife impact is synchronous, not a saved projectile");need(v.at("resolved").is_array()&&v.at("resolved").size()<=scene_.world_->objects().size(),"Warehouse attack result set");for(const auto& id:v.at("resolved")){const auto target=string(id);findCharacter(*scene_.world_,target);need(a.resolved.insert(target).second,"Warehouse duplicate attack result");}const int definitionWeapon=integer(v.at("definition"),0,999);need((definitionWeapon!=0)==(a.kind==AttackKind::De||a.kind==AttackKind::Shotgun||a.kind==AttackKind::SemiAutoShotgun),"Warehouse projectile is missing its fired definition");if(definitionWeapon){const int weapon=definitionWeapon;a.definitionWeapon=weapon;need(scene_.content_.expansion.attacks.contains(weapon),"Missing warehouse attack definition");const auto& original=scene_.content_.catalog.at("weapon_bindings").at(std::to_string(weapon));need((original.at("kind")=="de"&&a.kind==AttackKind::De)||(original.at("kind")=="shotgun"&&(a.kind==AttackKind::Shotgun||a.kind==AttackKind::SemiAutoShotgun)),"Warehouse projectile/definition kind mismatch");a.definition=scene_.content_.expansion.attacks.at(weapon);const auto& variation=mapProfile(weapon);if(variation.contains("flight_calls")){a.definition->flightCalls=variation.at("flight_calls");}}a.generation=scene_.generation_;a.id=nextAttackId_++;if(boolean(v.at("end_requested"))){requestedEnd_=a.id;}return a;};attack_=readAttack(j.at("attack"));pendingAttack_=readAttack(j.at("pending_attack"));
 pendingFireCues_=j.at("pending_cues").get<std::vector<std::string>>();need(pendingFireCues_.empty()||j.at("pending_cues")==profile("fire").at("cue_order"),"Warehouse pending fire definition");need(pendingAttack_.has_value()==(pendingFireWait_>0),"Warehouse pending fire wait");
 grenade_.reset();if(!j.at("grenade").is_null()){const auto& v=j.at("grenade");keys(v,{"motion","steps","age","height","explosion_age"});grenade_.emplace(GrenadeFlight{WarehouseSaveState::motion(v.at("motion"),scene_.definition_.width,scene_.definition_.height),integer(v.at("steps"),0,100),integer(v.at("age"),0,360000),integer(v.at("height"),-100,100),integer(v.at("explosion_age"),-1,360000)});need((attack_&&attack_->kind==AttackKind::Grenade)||(!attack_&&grenade_->explosionAge>=7),"Warehouse grenade missing live attack before committed impact");}
 launcher_.reset();if(!j.at("launcher").is_null()){const auto& v=j.at("launcher");keys(v,{"motion","weapon","steps","stage","wait"});const int weapon=integer(v.at("weapon"),1,999);need(scene_.content_.catalog.at("weapon_bindings").contains(std::to_string(weapon)),"Warehouse launcher weapon");const auto& binding=scene_.content_.catalog.at("weapon_bindings").at(std::to_string(weapon));need(binding.at("kind")=="gl"||binding.at("kind")=="rocket","Warehouse launcher definition kind");launcher_.emplace(LauncherFlight{WarehouseSaveState::motion(v.at("motion"),scene_.definition_.width,scene_.definition_.height),weapon,integer(v.at("steps"),0,100),integer(v.at("stage"),-1,binding.at("explosion_stages").size()),integer(v.at("wait"),0,360000)});need(attack_&&(attack_->kind==AttackKind::GlNormal||attack_->kind==AttackKind::GlFlame||attack_->kind==AttackKind::GlAcid||attack_->kind==AttackKind::Rocket),"Warehouse launcher without its attack");}
 launcherPresentation_.reset();if(!j.at("explosion").is_null()){const auto& v=j.at("explosion");keys(v,{"weapon","position","action"});launcherPresentationWeapon_=integer(v.at("weapon"),1,999);need(scene_.content_.catalog.at("weapon_bindings").contains(std::to_string(launcherPresentationWeapon_)),"Warehouse explosion weapon");launcherPresentationPosition_=grid(v.at("position"));const auto& rule=scene_.content_.catalog.at("weapon_bindings").at(std::to_string(launcherPresentationWeapon_));need(rule.contains("explosion_cue_order"),"Warehouse explosion definition");launcherPresentation_=std::make_unique<CueAction>(rule.at("explosion_cue_order").get<std::vector<std::string>>(),[this](const std::string& cue){return emitLauncherCue(cue);},[]{return false;},[]{});launcherPresentation_->restoreState(v.at("action"));}
 need(!aimed_||raised_,"Warehouse aim without raised weapon");need(captureWarehouseState()==j,"Warehouse weapon state mismatch");
}
