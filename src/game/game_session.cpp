#include "game_session.h"
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
namespace{
    constexpr std::array<int,6> timerSeconds{0,60,90,120,120,90};
    int pictureVariableIndex(int id){
        if(id==201){return 18;}
        if(id==83||id==84){return 16+id-83;}
        if(id<93||id>192){return -1;}
        const int slot=(id-93)/14,axis=(id-93)%14;
        return axis<2?slot*2+axis:-1;
    }
    // Bounded source adapter. Persistent facts have one named storage; transient
    // source slots remain in the legacy adapter until their owning subsystem migrates.
    const std::map<int,std::string> worldSwitches{
        {51,"m4-e2.unlocked"},{52,"m4-e10.collected"},{54,"m4-e4.reward-claimed"},
        {55,"m9-e8.visited"},{56,"m18-e9.visited"},{58,"m15.hall-seen"},
        {59,"m17-e7.collected"},{60,"m13-e7.unlocked"},{62,"m14-e4.collected"},
        {63,"m14-e5.collected"},{64,"m14-e6.collected"},{65,"m17-e8.collected"},
        {101,"m4-e4.dead"},{102,"m10-e3.dead"},{103,"m10-e5.dead"},
        {104,"m11-e2.dead"},{106,"m17-e3.dead"},{107,"m13-e9.dead"},
        {112,"m17-e5.dead"},{459,"m14-e10.claimed"}};
    const std::map<int,std::string> worldVariables{
        {388,"m14-e10.password"},{396,"m4-e4.corpse-x"},
        {397,"m4-e4.corpse-y"},{398,"m4-e4.corpse-variant"}};
    template<class V> V lookup(const std::map<int,V>& values,int id){
        const auto it=values.find(id); return it==values.end()?V{}:it->second;
    }
}
const WorldFactDefinitions& f3WorldFacts(){static const WorldFactDefinitions facts{worldSwitches,worldVariables};return facts;}
GameSession::GameSession(const ItemDefinitions& definitions,const SessionEntry& entry,const WorldFactDefinitions& facts)
    :definitions_(definitions),facts_(facts),identity_{entry.actor,entry.level,entry.saveCount,entry.actorName,entry.difficulty,entry.party},inventory_(entry.inventory),magazines_(entry.magazines),
     variables_(entry.variables),itemUses_(entry.itemUses),flags_(entry.flags),
     worldFlags_(entry.worldFlags),worldValues_(entry.worldValues),skills_(entry.skills),equipment_{entry.mainWeapon,entry.auxiliaryWeapon,entry.armor,entry.accessory,entry.otherEquipment},hp_(entry.hp),maxHp_(entry.maxHp),tp_(entry.tp),maxTp_(entry.maxTp),baseAttack_(entry.baseAttack),baseDefense_(entry.baseDefense),baseAgility_(entry.baseAgility),baseSpirit_(entry.baseSpirit),conditions_(entry.conditions),quickshotTicks_(entry.quickshotTicks){
    (void)difficultyLevel();
    if(!entry.commonEffectsKnown){conditions_.bleedingActive=conditions_.bleedingTicks>0;}
    if(conditions_.bleedingTicks<0||conditions_.bleedingTicks>360000||(conditions_.bleedingActive&&conditions_.bleedingTicks==0)||quickshotTicks_<0){throw std::runtime_error("Invalid common-effect continuation");}
    const bool quickActive=entry.commonEffectsKnown?entry.quickshotActive:entry.quickshotTicks>0;
    if(quickActive&&quickshotTicks_==0){throw std::runtime_error("Quickshot enabled without a wait");}
    flags_[48]=quickActive;
    actor8Name_=entry.actor8Name;setActor8Name(actor8Name_);actorFace_=entry.actorFace;actorFaceIndex_=entry.actorFaceIndex;
    eventTimer_=entry.eventTimer;
    for(int id=83;id<=201;++id){
        if(pictureVariableIndex(id)>=0&&(variables_.contains(id)||facts_.variables.contains(id))){throw std::runtime_error("Picture workspace cannot be restored as persistent state");}
    }
    pursuerHealth_=entry.pursuerHealth;
    if(pursuerHealth_<1||pursuerHealth_>500){throw std::runtime_error("Invalid persistent pursuer health");}
    if(conditions_.poisonWaitTicks<0||conditions_.poisonWaitTicks>180){throw std::runtime_error("Invalid poison continuation");}
    if(eventTimer_.source<EventTimerSource::None||eventTimer_.source>EventTimerSource::AccessKey||
       eventTimer_.remainingTicks<0||eventTimer_.remainingTicks>timerSeconds.at(static_cast<int>(eventTimer_.source))*60+59||
       (eventTimer_.running&&eventTimer_.remainingTicks<60)||
       (eventTimer_.source==EventTimerSource::None&&eventTimer_!=EventTimer{})||
       (!eventTimer_.running&&eventTimer_.visible)){
        throw std::runtime_error("Invalid event timer state");
    }
    variables_.erase(278);variables_.erase(280);
    if(facts_.variables.contains(49)){worldValues_.try_emplace(facts_.variables.at(49),0);}
    for(const auto& [id,key]:facts_.switches){
        if(flags_.contains(id)){worldFlags_[key]=flags_.at(id);flags_.erase(id);}
    }
    for(const auto& [id,key]:facts_.variables){
        if(variables_.contains(id)){worldValues_[key]=variables_.at(id);variables_.erase(id);}
    }
    for(const auto [id,slot]:{std::pair{equipment_.mainWeapon,1},std::pair{equipment_.auxiliaryWeapon,2},std::pair{equipment_.armor,3},std::pair{equipment_.accessory,4},std::pair{equipment_.other,5}}){
        if(id&&(!definitions_.contains(id)||definitions_.at(id).equipmentType!=slot||definitions_.at(id).stateOnly||definitions_.at(id).pendingActionTicket)){throw std::runtime_error("Invalid entry equipment slot");}
    }
    for(const auto& [id,d]:definitions_){
        const bool validBox=(d.boxItem==0&&d.boxRounds==0)||(d.boxRounds>0&&definitions_.contains(d.boxItem));
        if(d.capacity>0&&((d.regularReload&&(!validBox||!definitions_.contains(d.looseItem)))||(!d.regularReload&&(d.looseItem||d.boxItem||d.boxRounds)))){throw std::runtime_error("Missing weapon ammunition definition");}
    }
    if(maxHp_<=0||maxTp_<0||tp_<0||tp_>maxTp_){throw std::runtime_error("Invalid entry health limits");}
    if(hp_<0||hp_>maxHp_){throw std::runtime_error("Entry HP outside maximum");}
    for(const auto& [id,n]:inventory_){if(n<0||!definitions_.contains(id)||(n>0&&definitions_.at(id).stateOnly)){throw std::runtime_error("Invalid entry inventory");}}
    for(const auto& [id,n]:itemUses_){if(!definitions_.contains(id)||count(id)<=0||n<0||definitions_.at(id).uses<=0||n>=definitions_.at(id).uses){throw std::runtime_error("Invalid current-item usage");}}
    for(const auto& [id,n]:magazines_){if(!definitions_.contains(id)||definitions_.at(id).capacity<=0||n<definitions_.at(id).magazineMinimum||n>definitions_.at(id).capacity){throw std::runtime_error("Invalid entry magazine");}}
}
int GameSession::difficultyLevel() const{
    if(openingInitialization_==OpeningInitialization::Choosing){return 1;}
    if(difficulty()=="easy"){return 1;}if(difficulty()=="normal"){return 2;}if(difficulty()=="hard"){return 3;}
    throw std::runtime_error("Unknown session difficulty");
}
void GameSession::beginOpeningInitialization(){
    if(openingInitialization_!=OpeningInitialization::Complete||!identity_.party.empty()||flag(1)||flag(18)||flag(15)){throw std::logic_error("Opening initialization is not available");}
    openingInitialization_=OpeningInitialization::Choosing;
}
void GameSession::confirmOpeningDifficulty(const std::string& difficulty){
    if(openingInitialization_!=OpeningInitialization::Choosing||!identity_.party.empty()||flag(1)||flag(18)||flag(15)||
       (difficulty!="easy"&&difficulty!="normal"&&difficulty!="hard")){throw std::logic_error("Stale or invalid opening difficulty commit");}
    identity_.difficulty=difficulty;openingInitialization_=OpeningInitialization::Confirmed;
}
void GameSession::finishOpeningInitialization(const std::vector<int>& easyPreseed){
    if(openingInitialization_!=OpeningInitialization::Confirmed){throw std::logic_error("Opening initialization is not confirmed");}
    if(difficulty()=="easy"){for(int id:easyPreseed){if(!facts_.switches.contains(id)){throw std::logic_error("Unregistered EASY preset");}}for(int id:easyPreseed){setFlag(id,true);}}
    openingInitialization_=OpeningInitialization::Complete;
}
SessionEntry GameSession::exportState() const{
    SessionEntry e;e.actor=identity_.actor;e.level=identity_.level;e.saveCount=identity_.saveCount;e.actorName=identity_.name;e.difficulty=identity_.difficulty;e.party=identity_.party;
    e.actor8Name=actor8Name_;e.actorFace=actorFace_;e.actorFaceIndex=actorFaceIndex_;
    e.inventory=inventory_;e.magazines=magazines_;e.itemUses=itemUses_;e.flags=flags_;e.flags.erase(48);e.variables=variables_;e.worldFlags=worldFlags_;e.worldValues=worldValues_;e.skills=skills_;
    e.mainWeapon=equipment_.mainWeapon;e.auxiliaryWeapon=equipment_.auxiliaryWeapon;e.armor=equipment_.armor;e.accessory=equipment_.accessory;e.otherEquipment=equipment_.other;
    e.hp=hp_;e.maxHp=maxHp_;e.tp=tp_;e.maxTp=maxTp_;e.baseAttack=baseAttack_;e.baseDefense=baseDefense_;e.baseAgility=baseAgility_;e.baseSpirit=baseSpirit_;e.conditions=conditions_;e.quickshotTicks=quickshotTicks_;e.quickshotActive=flag(48);e.commonEffectsKnown=true;e.eventTimer=eventTimer_;e.pursuerHealth=pursuerHealth_;return e;
}
void GameSession::setEventTimer(EventTimerSource source,int seconds){
    if(source<=EventTimerSource::None||source>EventTimerSource::AccessKey||seconds<1||seconds>timerSeconds.at(static_cast<int>(source))){throw std::runtime_error("Invalid event timer start");}
    // Fixed Player timer keeps a subsecond residue when its displayed seconds reach zero.
    eventTimer_.source=source;eventTimer_.remainingTicks=seconds*60+59;
}
void GameSession::startEventTimer(EventTimerSource source,int seconds,bool visible,bool battle){
    setEventTimer(source,seconds);startEventTimer(visible,battle);
}
void GameSession::startEventTimer(bool visible,bool battle){
    if(eventTimer_.source==EventTimerSource::None||eventTimer_.remainingTicks<60){throw std::runtime_error("Event timer has no positive configured time");}
    eventTimer_.running=true;eventTimer_.visible=visible;eventTimer_.battle=battle;
}
void GameSession::stopEventTimer(){eventTimer_.running=false;eventTimer_.visible=false;}
void GameSession::tickEventTimer(){
    if(!eventTimer_.running){return;}
    if(eventTimer_.remainingTicks>0){--eventTimer_.remainingTicks;}
    if(eventTimer_.remainingTicks/60==0){stopEventTimer();}
}
int GameSession::damagePursuer(int amount){
    if(amount<0){throw std::runtime_error("Invalid pursuer damage");}
    const int applied=std::min(amount,pursuerHealth_-1);pursuerHealth_-=applied;return applied;
}
std::set<std::string> WorldFactDefinitions::flagIds() const{std::set<std::string> out;for(const auto& [id,key]:switches){out.insert(key);}return out;}
std::set<std::string> WorldFactDefinitions::valueIds() const{std::set<std::string> out;for(const auto& [id,key]:variables){out.insert(key);}return out;}
void GameSession::finishSave(int committedCount,int statistic,bool restoreControls){
    if(committedCount<identity_.saveCount||statistic<0){throw std::runtime_error("Invalid save continuation");}
    identity_.saveCount=committedCount;setVariable(399,statistic);if(restoreControls){setFlag(1,true);setFlag(18,true);}
}
int GameSession::itemUses(int item) const{return lookup(itemUses_,item);}
void GameSession::addResources(int hp,int tp){
    hp_=static_cast<int>(std::clamp(static_cast<long long>(hp_)+hp,0LL,static_cast<long long>(maxHp_)));
    tp_=static_cast<int>(std::clamp(static_cast<long long>(tp_)+tp,0LL,static_cast<long long>(maxTp_)));
}
void GameSession::restoreActor(){
    hp_=maxHp_;tp_=maxTp_;conditions_.bleedingActive=false;conditions_.poisoned=false;conditions_.neuralParalysis=false;conditions_.corroded=false;
    // FullHeal clears actor states, not already-running common-event waits or protection switches.
}
bool GameSession::canSelectItem(int id) const{
    const auto it=definitions_.find(id);if(it==definitions_.end()||count(id)<=0){return false;}const auto& d=it->second;
    return d.fieldUse&&(d.equipmentType==6||d.equipmentType==10)&&(d.actorPermissions.empty()||d.actorPermissions.front());
}
bool GameSession::convertTool(int from,int to){
    if(from==to||!definitions_.contains(from)||!definitions_.contains(to)||count(from)!=1||count(to)!=0){return false;}
    inventory_[from]=0;inventory_[to]=1;return true;
}
ItemUseResult GameSession::useMedicine(int id){
    if(count(id)<=0){return ItemUseResult::Missing;}
    if(!canSelectItem(id)||definitions_.at(id).equipmentType!=6){return ItemUseResult::Unavailable;}
    const auto& d=definitions_.at(id);const auto& effect=d.medicine;
    if(hp_==0||effect.koOnly){return ItemUseResult::InvalidTarget;}
    const int hp=std::min(maxHp_,hp_+maxHp_*effect.hpPercent/100+effect.hpPoints);
    const int tp=std::min(maxTp_,tp_+maxTp_*effect.tpPercent/100+effect.tpPoints);
    const bool cureBleeding=conditions_.bleedingActive&&std::find(effect.cureStates.begin(),effect.cureStates.end(),3)!=effect.cureStates.end();
    const bool curePoison=conditions_.poisoned&&std::find(effect.cureStates.begin(),effect.cureStates.end(),2)!=effect.cureStates.end();
    if(hp==hp_&&tp==tp_&&!cureBleeding&&!curePoison){return ItemUseResult::NoEffect;}
    const bool consumedSpray=id==1&&d.uses>0&&itemUses(id)==d.uses-1;
    if(consumedSpray&&(variable(382)<0||variable(382)>=std::numeric_limits<int>::max()-1)){return ItemUseResult::Unavailable;}
    hp_=hp;tp_=tp;if(cureBleeding){conditions_.bleedingActive=false;}
    if(curePoison){conditions_.poisoned=false;}
    if(d.uses>0&&++itemUses_[id]>=d.uses){--inventory_[id];itemUses_.erase(id);}
    // CE89 C31-38 counts consumed I1; a menu cannot gain spray or save before returning.
    if(consumedSpray){setVariable(382,variable(382)+1);}
    return ItemUseResult::Applied;
}
ItemUseResult GameSession::selectWeaponAmmunition(int item,const std::vector<int>& weaponForms){
    if(!definitions_.contains(item)||definitions_.at(item).stateOnly||definitions_.at(item).pendingActionTicket){return ItemUseResult::Unavailable;}
    if(count(item)<=0){return ItemUseResult::Missing;}
    if(!canSelectItem(item)||definitions_.at(item).equipmentType!=10||weaponForms.size()<2){return ItemUseResult::Unavailable;}
    int current=0,target=0,owned=0,loaded=0;
    std::set<int> forms,ammunition;
    for(int id:weaponForms){
        if(!definitions_.contains(id)||!canEquip(id,1)||!forms.insert(id).second){return ItemUseResult::Unavailable;}
        const auto& form=definitions_.at(id);
        if(form.capacity!=1||form.boxItem||form.boxRounds||!definitions_.contains(form.looseItem)||!ammunition.insert(form.looseItem).second||form.twoHanded!=definitions_.at(weaponForms.front()).twoHanded){return ItemUseResult::Unavailable;}
        const auto& ammo=definitions_.at(form.looseItem);
        if(ammo.equipmentType!=10||ammo.stateOnly||ammo.pendingActionTicket||count(ammo.id)>99){return ItemUseResult::Unavailable;}
        if(form.looseItem==item){target=id;}
        if(count(id)>1){return ItemUseResult::InvalidTarget;}
        owned+=count(id)+(equipped(1)==id?1:0);
        if(owned>1){return ItemUseResult::InvalidTarget;}
        if(equipped(1)==id||count(id)>0){current=id;}
        if(magazine(id)>0){if(loaded){return ItemUseResult::InvalidTarget;}loaded=id;}
    }
    if(!current||!target||(loaded&&loaded!=current)){return ItemUseResult::InvalidTarget;}
    if(current==target){return ItemUseResult::NoEffect;}
    // CE108-110: replace the physical form, return its old round (clamped at 99), then load the new round.
    if(equipped(1)==current){equipment_.mainWeapon=target;}
    else{inventory_[current]=0;itemUses_.erase(current);inventory_[target]=1;}
    if(magazine(current)==1){magazines_[current]=0;grant(definitions_.at(current).looseItem,1);}
    magazines_[target]=1;--inventory_[item];return ItemUseResult::Applied;
}
bool GameSession::skillUsable(const SkillDefinition& skill) const{
    return knowsSkill(skill.id)&&skill.fieldUse&&hp_>0&&tp_>=skill.tpCost&&
        !(conditions_.corroded&&skill.magicalRate>=1);
}
bool GameSession::normalizeAccessKeys(){
    if(eventTimer_.source!=EventTimerSource::AccessKey||eventTimer_.remainingTicks/60>0){return false;}
    bool changed=false;
    for(const auto [fact,from,to]:{std::array<int,3>{470,107,105},{470,108,106},{471,109,105},{471,110,106}}){
        if(flag(fact)&&count(from)>0){changed|=convertTool(from,to);}
    }
    if(flag(470)){setFlag(470,false);changed=true;}
    if(flag(471)){setFlag(471,false);changed=true;}
    return changed;
}
SkillUseResult GameSession::castSkill(const SkillDefinition& skill){
    if(!knowsSkill(skill.id)||!skill.fieldUse||skill.effect==SkillEffect::None||(conditions_.corroded&&skill.magicalRate>=1)){return SkillUseResult::Unavailable;}
    if(hp_<=0){return SkillUseResult::InvalidTarget;}
    if(skill.tpCost<0||tp_<skill.tpCost){return SkillUseResult::InsufficientTp;}
    tp_-=skill.tpCost;
    if(skill.effect==SkillEffect::Precision){setFlag(47,true);}
    if(skill.effect==SkillEffect::Quickshot&&!flag(48)){if(quickshotTicks_==0){quickshotTicks_=skill.durationTicks;}setFlag(48,true);}
    return SkillUseResult::Applied;
}
bool GameSession::knowsSkill(int id) const{return skills_.contains(id);}
void GameSession::replaceKnownSkill(int from,int to){if(skills_.erase(from)){skills_.insert(to);}}
int GameSession::count(int id) const{return lookup(inventory_,id);}
int GameSession::magazine(int id) const{return lookup(magazines_,id);}
const Equipment& GameSession::equipment() const{return equipment_;}
void GameSession::grant(int id,int n){
    if(!definitions_.contains(id)||definitions_.at(id).stateOnly||n<0||count(id)>std::numeric_limits<int>::max()-n){throw std::runtime_error("Invalid item grant");}
    inventory_[id]=std::min(99,count(id)+n);
}
bool GameSession::discard(int item,int amount){
    if(amount<=0||!definitions_.contains(item)||count(item)<amount){return false;}
    inventory_[item]-=amount;if(count(item)==0){itemUses_.erase(item);}return true;
}
int GameSession::equipped(int slot) const{
    switch(slot){case 1:return equipment_.mainWeapon;case 2:return equipment_.auxiliaryWeapon;case 3:return equipment_.armor;case 4:return equipment_.accessory;case 5:return equipment_.other;default:return 0;}
}
bool GameSession::canEquip(int id,int slot) const{
    if(slot<1||slot>5){return false;}if(!id){return true;}
    const auto it=definitions_.find(id);if(it==definitions_.end()||it->second.equipmentType!=slot||it->second.stateOnly||it->second.pendingActionTicket){return false;}
    const auto& d=it->second;
    return (d.actorPermissions.empty()||d.actorPermissions.front())&&!(slot==2&&equipment_.mainWeapon&&definitions_.at(equipment_.mainWeapon).twoHanded);
}
bool GameSession::equip(int id,int slot){
    if(!canEquip(id,slot)){return false;}
    const int old=equipped(slot);if(old==id){return true;}
    if(id&&count(id)==0){return false;}
    if(old){grant(old,1);}if(id){--inventory_[id];}
    switch(slot){case 1:equipment_.mainWeapon=id;break;case 2:equipment_.auxiliaryWeapon=id;break;case 3:equipment_.armor=id;break;case 4:equipment_.accessory=id;break;case 5:equipment_.other=id;break;}
    if(slot==1&&id&&definitions_.at(id).twoHanded&&equipment_.auxiliaryWeapon){grant(equipment_.auxiliaryWeapon,1);equipment_.auxiliaryWeapon=0;}
    return true;
}
bool GameSession::canFireRound(int slot) const{
    const int id=slot==1||slot==2?equipped(slot):0;if(!id){return false;}
    const auto& definition=definitions_.at(id);
    return definition.capacity>0&&(magazine(id)>0||(definition.magazineMinimum<0&&definition.overdrawAccessory&&equipment_.accessory==definition.overdrawAccessory));
}
bool GameSession::consumeRound(int slot){
    if(!canFireRound(slot)){return false;}const int id=equipped(slot);
    // Fixed Player clamps variable subtraction at its signed lower limit, not at zero.
    magazines_[id]=std::max(definitions_.at(id).magazineMinimum,magazine(id)-1);return true;
}
bool GameSession::consumeEquippedItem(int slot){
    const int id=equipped(slot);if(!id||!equip(0,slot)){return false;}
    discard(id,1);if(count(id)>0){equip(id,slot);}return true;
}
void GameSession::organizeAmmo(){
    for(const auto& [id,d]:definitions_){if(d.capacity>0&&d.boxRounds>0){
        const int boxes=count(d.looseItem)/d.boxRounds;inventory_[d.looseItem]-=boxes*d.boxRounds;inventory_[d.boxItem]=std::min(99,count(d.boxItem)+boxes);
    }}
}
bool GameSession::hasReserve(int slot) const{
    const int id=slot==1||slot==2?equipped(slot):0;if(!id){return false;}
    const auto& d=definitions_.at(id);
    return d.regularReload&&(count(d.looseItem)>0||count(d.boxItem)>0);
}
int GameSession::reload(int slot){
    const int id=slot==1||slot==2?equipped(slot):0;if(!id){return 0;}
    const auto& d=definitions_.at(id);if(d.capacity<=0||!d.regularReload){return 0;}const int before=magazine(id);
    while(magazines_[id]<d.capacity){
        if(count(d.looseItem)==0){
            if(count(d.boxItem)==0){break;}
            --inventory_[d.boxItem];
            if(d.boxReloadDebit&&magazines_[id]>before){
                // CN CE53 preserves its literal debit before assigning a full magazine.
                inventory_[d.looseItem]=std::clamp(d.boxRounds+magazines_[id]-d.boxReloadDebit,0,99);
                magazines_[id]=d.capacity;break;
            }
            inventory_[d.looseItem]+=d.boxRounds;
        }
        const int n=std::min(d.capacity-magazines_[id],count(d.looseItem));
        magazines_[id]+=n; inventory_[d.looseItem]-=n;
    }
    return magazine(id)-before;
}
bool GameSession::flag(int id) const{return facts_.switches.contains(id)?worldFlag(facts_.switches.at(id)):lookup(flags_,id);}
void GameSession::setFlag(int id,bool value){if(facts_.switches.contains(id)){setWorldFlag(facts_.switches.at(id),value);}else{flags_[id]=value;}}
int GameSession::variable(int id) const{
    // V13 is the selected durable actor, computed by CE89 in the original.
    if(id==13){return identity_.actor;}
    if(id==280){return openingInitialization_==OpeningInitialization::Complete?difficultyLevel():0;}
    // CN1.00 V278 is the nearby usable inventory, never another writable count.
    if(id==278){return count(90)+count(92);}
    if(const int index=pictureVariableIndex(id);index>=0){return pictureScratch_.at(index);}
    return facts_.variables.contains(id)?worldValue(facts_.variables.at(id)):lookup(variables_,id);
}
void GameSession::setVariable(int id,int value){
    if(id==280){if(value!=variable(280)){throw std::logic_error("Difficulty is owned by opening initialization");}return;}
    if(id==278){throw std::logic_error("Nearby gem count is derived from inventory");}
    if(const int index=pictureVariableIndex(id);index>=0){pictureScratch_.at(index)=value;return;}
    if(facts_.variables.contains(id)){setWorldValue(facts_.variables.at(id),value);}else{variables_[id]=value;}
}
bool GameSession::worldFlag(const std::string& id) const{const auto it=worldFlags_.find(id);return it!=worldFlags_.end()&&it->second;}
void GameSession::setWorldFlag(const std::string& id,bool value){if(id=="truth.trap-triggered"&&worldFlag(id)&&!value){throw std::logic_error("Truth trap cannot revert within a session");}if(!facts_.flagIds().contains(id)){throw std::runtime_error("Unknown world flag ID");}worldFlags_[id]=value;}
int GameSession::worldValue(const std::string& id) const{const auto it=worldValues_.find(id);return it==worldValues_.end()?0:it->second;}
void GameSession::setWorldValue(const std::string& id,int value){if(!facts_.valueIds().contains(id)){throw std::runtime_error("Unknown world value ID");}worldValues_[id]=value;}
int GameSession::hp() const{return hp_;}
int GameSession::loseHp(int amount,bool lethal){
    if(amount<0){throw std::runtime_error("Negative damage");}
    const int before=hp_; hp_=std::max(lethal?0:std::min(1,hp_),hp_-amount); if(hp_==0){conditions_.corroded=false;} return before-hp_;
}
EquipmentStats GameSession::equipmentStats(int replacement,int slot) const{
    EquipmentStats result{baseAttack_,baseDefense_,baseSpirit_,baseAgility_};
    const bool twoHanded=slot==1&&replacement&&definitions_.at(replacement).twoHanded;
    for(int n=1;n<=5;++n){const int id=n==slot?replacement:(twoHanded&&n==2?0:equipped(n));if(!id){continue;}
        const auto& d=definitions_.at(id);result.attack+=d.attack;result.defense+=d.defense;result.spirit+=d.spirit;result.agility+=d.agility;
    }
    if(conditions_.bleedingActive||conditions_.corroded){result.defense=std::max(1,result.defense/2);}return result;
}
int GameSession::attack() const{return equipmentStats().attack;}
int GameSession::defense() const{return equipmentStats().defense;}
int GameSession::auxiliaryAttack() const{return equipmentStats().agility;}
void GameSession::clearAimingAid(bool unequipAttachment){if(flag(159)){if(unequipAttachment){equip(0,4);}inventory_.erase(158);setFlag(159,false);}}
void GameSession::resetRoomFacts(){
    conditions_.corroded=false;
    pictureScratch_.fill(0);
    // CN1.00 CE55 C1-35, CE5 normal-actor reset. Persistent stats V35/V41 and bleeding survive.
    for(int id:{1,18,2,3,4,5,17,19,48,83,241,477}){setFlag(id,false);}
    for(int i=591;i<=598;++i){setFlag(i,false);}
    for(int i=215;i<=225;++i){setFlag(i,false);}
    for(int id:{10,11,12,22,23,24,29,30,31,32,33,34,44,45,46,36,37,226}){setVariable(id,0);}
    for(int i=330;i<=372;++i){setVariable(i,0);}
    clearAimingAid(true);
    for(int i=81;i<=222;++i){variables_.erase(i);}
    for(int i=21;i<=44;++i){flags_.erase(i);}
}

bool GameSession::advanceNeuralEffect(bool enabled){
    if(!enabled){return false;}
    // CE112 consumes one of its six waits on entry; its end yields once more.
    if(neuralEffectWait_<0){neuralEffectWait_=5;return true;}
    if(neuralEffectWait_>0){--neuralEffectWait_;}
    else{neuralEffectWait_=-1;}
    return false;
}

bool GameSession::advanceNeuralRecoveryWait(bool enabled){
    if(!enabled){return false;}
    if(neuralRecoveryWait_<0){neuralRecoveryWait_=240;}
    if(neuralRecoveryWait_>0){--neuralRecoveryWait_;return false;}
    neuralRecoveryWait_=-1;return true;
}

bool GameSession::advanceBurnWait(int commonEvent,bool enabled){
    if(commonEvent<24||commonEvent>31){throw std::invalid_argument("Unknown burn common event");}
    if(!enabled){return false;}
    auto& wait=burnWaits_.at(commonEvent-24);
    if(wait==0){wait=6;return false;}
    if(--wait>0){return false;}
    wait=6;return true;
}
int GameSession::burnWaitRemaining(int commonEvent) const{
    if(commonEvent<24||commonEvent>31){throw std::invalid_argument("Unknown burn common event");}
    return burnWaits_.at(commonEvent-24);
}
BurnPictureStage GameSession::burnPictureStage(int commonEvent) const{
    (void)burnWaitRemaining(commonEvent);return burnPictureStages_.at(commonEvent-24);
}
void GameSession::setBurnPictureStage(int commonEvent,BurnPictureStage stage){
    (void)burnWaitRemaining(commonEvent);
    if(stage<BurnPictureStage::InitialShow||stage>BurnPictureStage::Erase){throw std::invalid_argument("Unknown burn picture stage");}
    burnPictureStages_.at(commonEvent-24)=stage;
}
BloodPictureProgress GameSession::bloodPictureProgress(int commonEvent) const{
    if(commonEvent<15||commonEvent>17){throw std::invalid_argument("Unknown finite blood common event");}
    return bloodPictureProgress_.at(commonEvent-15);
}
void GameSession::setBloodPictureProgress(BloodPictureProgress progress,int commonEvent){
    (void)bloodPictureProgress(commonEvent);
    // CN1.00 CE15/16/17 have finite four-frame variants, including yellow/closed-shell feedback.
    const bool known=progress.type==0||progress.type==1||progress.type==2||progress.type==3||
        progress.type==11||progress.type==12||progress.type==13||progress.type==101||progress.type==102||progress.type==103||
        progress.type==121||progress.type==122||progress.type==123||(commonEvent==15&&(progress.type==111||progress.type==112));
    if(!known||progress.frame<0||progress.frame>4||(progress.type==0&&progress!=BloodPictureProgress{})){
        throw std::invalid_argument("Invalid finite blood picture progress");
    }
    bloodPictureProgress_.at(commonEvent-15)=progress;
}
void GameSession::finishBurnWait(int commonEvent){
    if(commonEvent<24||commonEvent>31){throw std::invalid_argument("Unknown burn common event");}
    burnWaits_.at(commonEvent-24)=0;
    burnPictureStages_.at(commonEvent-24)=BurnPictureStage::InitialShow;
}

const PlayerConditions& GameSession::conditions() const{return conditions_;}
void GameSession::startBleeding(int ticks){
    if(ticks<=0||ticks>360000){throw std::invalid_argument("Invalid bleeding duration");}
    conditions_.bleedingActive=true;
    if(conditions_.bleedingTicks==0){conditions_.bleedingTicks=ticks;}
}
void GameSession::startRecovery(int recoveryTicks,int invulnerableTicks){conditions_.recoveryTicks=recoveryTicks;conditions_.invulnerableTicks=invulnerableTicks;setFlag(13,true);}
void GameSession::tickMapCommonEffects(bool allowStarts){
    // CE33 does not recheck State2 after Wait30; curing preserves its pending one-HP loss.
    if(conditions_.poisonWaitTicks>0){if(--conditions_.poisonWaitTicks==0){loseHp(1,false);}}
    else if(allowStarts&&conditions_.poisoned&&flag(1)&&identity_.actor==1){conditions_.poisonWaitTicks=180;}
    if(flag(48)&&quickshotTicks_>0&&--quickshotTicks_==0){setFlag(48,false);}
    if(conditions_.bleedingTicks>0&&--conditions_.bleedingTicks==0){conditions_.bleedingActive=false;}
}
void GameSession::tickConditions(){
    tickMapCommonEffects(true);
    if(conditions_.invulnerableTicks>0&&--conditions_.invulnerableTicks==0){setFlag(13,false);}
    if(conditions_.recoveryTicks>0){--conditions_.recoveryTicks;}
}
EndingScoreInput GameSession::endingScoreInput() const{
    EndingScoreInput in;in.sprayCount=variable(382);in.retryCount=variable(630);in.saveCount=variable(399);
    in.hours=variable(390);in.minutes=variable(391);in.seconds=variable(392);in.guardFirstScore=variable(395);
    in.difficultyLevel=difficulty()=="hard"?3:difficulty()=="normal"?2:difficulty()=="easy"?1:0;
    in.specialItemUsed=flag(584);for(int id=601;id<=605;++id){in.unlocked[id-601]=flag(id);}return in;
}
void GameSession::commitEndingRank(){setVariable(381,endingScore(endingScoreInput()).rank);}
bool GameSession::commitEndingUnlock(int id){
    if(id<601||id>605){throw std::runtime_error("Unknown ending unlock");}
    if(flag(id)){return false;}
    const auto result=endingScore(endingScoreInput());
    if(std::find(result.unlocks.begin(),result.unlocks.end(),id)==result.unlocks.end()||variable(381)!=result.rank){throw std::runtime_error("Ending unlock outside committed score");}
    setFlag(id,true);return true;
}
void GameSession::recordEndingSpecialAction(int commonEvent){
    if(commonEvent!=72&&commonEvent!=75&&commonEvent!=88&&commonEvent!=89){throw std::runtime_error("Unknown special score writer");}
    setFlag(584,true);
}
void GameSession::tickPlayClock(){
    if(!facts_.variables.contains(390)){return;}
    // The pinned common-event interpreter pauses its wait while S18 is OFF; Load starts at C1.
    if(!flag(18)){return;}
    if(--playClockWait_>0){return;}playClockWait_=60;
    const auto in=endingScoreInput();(void)endingScore(in);
    int seconds=in.seconds+1,minutes=in.minutes,hours=in.hours;
    if(seconds==60){seconds=0;if(++minutes==60){minutes=0;hours=static_cast<int>(std::min(1LL+hours,1LL*std::numeric_limits<int>::max()-1));}}
    setVariable(390,hours);setVariable(391,minutes);setVariable(392,seconds);
}
void GameSession::walkedStep(int interval,int damage){
    if(interval<=0||damage<0){throw std::runtime_error("Invalid map damage rule");}
    ++conditions_.walkedSteps;
    if(conditions_.bleedingActive&&conditions_.walkedSteps%interval==0){loseHp(damage,false);}
}

void GameSession::joinParty(int actor){if(actor!=1&&actor!=8){throw std::runtime_error("Unsupported party actor");}if(!inParty(actor)){identity_.party.push_back(actor);}}
void GameSession::leaveParty(int actor){if(actor!=1&&actor!=8){throw std::runtime_error("Unsupported party actor");}std::erase(identity_.party,actor);}
void GameSession::seedMagazine(int item,int rounds){if(definitions_.at(item).capacity<=0||rounds<0||rounds>definitions_.at(item).capacity||magazines_.contains(item)){throw std::runtime_error("Invalid duplicate magazine initialization");}magazines_[item]=rounds;}

// CE37: capacity truncates the grant after consumption; it is not a precondition.
bool GameSession::canMix(const MixingRecipe& recipe) const{
    if(hp_<=0||recipe.ingredients.empty()||recipe.count<=0||recipe.count>99||!definitions_.contains(recipe.result)||definitions_.at(recipe.result).stateOnly){return false;}
    for(const auto& [id,amount]:recipe.ingredients){if(amount<=0||amount>99||!definitions_.contains(id)||count(id)<amount){return false;}}
    return true;
}
bool GameSession::mix(const MixingRecipe& recipe){
    if(!canMix(recipe)){return false;}
    for(const auto& [id,amount]:recipe.ingredients){discard(id,amount);itemUses_.erase(id);}
    grant(recipe.result,recipe.count);return true;
}
