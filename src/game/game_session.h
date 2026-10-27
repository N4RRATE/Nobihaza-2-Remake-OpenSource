#pragma once
#include "item_definition.h"
#include "world_geometry.h"
#include "ending_score.h"
#include <map>
#include <algorithm>
#include <string>
#include <set>
#include <array>
enum class SkillEffect{None,Precision,Quickshot};
struct SkillDefinition{int id{},type{},scope{},tpCost{},implementationTicket{};bool fieldUse{};std::string name,description;SkillEffect effect{SkillEffect::None};int durationTicks{};int magicalRate{3};};
enum class SkillUseResult{Applied,Unavailable,InsufficientTp,InvalidTarget};
enum class ItemUseResult{Applied,Missing,Unavailable,InvalidTarget,NoEffect};
enum class EventTimerSource{None,Training,Power,GuardFirst,GuardFinal,AccessKey};
enum class BurnPictureStage{InitialShow,Waiting,MoveFirst,MoveSecond,MoveThird,Erase};
struct BloodPictureProgress{int type{},frame{};bool sampled{},shown{};bool operator==(const BloodPictureProgress&) const=default;};
struct EventTimer{
    bool operator==(const EventTimer&) const=default;
    EventTimerSource source{EventTimerSource::None};
    int remainingTicks{};
    bool running{},visible{},battle{};
};
struct PlayerConditions{bool operator==(const PlayerConditions&) const=default;int bleedingTicks{};int invulnerableTicks{};int recoveryTicks{};int walkedSteps{};bool poisoned{};int poisonWaitTicks{};bool neuralParalysis{};bool bleedingActive{};bool corroded{};};
struct SessionEntry{
    bool operator==(const SessionEntry&) const=default;
    int actor{1},level{1},saveCount{};
    std::string actorName{"大雄"},difficulty{"normal"};
    std::string actorFace{"face-actor1"};int actorFaceIndex{1};
    std::vector<int> party{1};
    std::string actor8Name{"dammy"};
    std::map<std::string,bool> worldFlags;
    std::map<std::string,int> worldValues;
    int quickshotTicks{};
    bool quickshotActive{},commonEffectsKnown{};
    std::map<int,int> inventory;
    std::map<int,int> magazines;
    std::map<int,int> itemUses;
    std::map<int,bool> flags;
    std::map<int,int> variables;
    int mainWeapon{};
    int auxiliaryWeapon{};
    int armor{};
    int hp{100};
    int maxHp{100},tp{30},maxTp{30};
    int accessory{},otherEquipment{};
    int baseAttack{1};
    int baseDefense{1};
    int baseAgility{1},baseSpirit{1};
    PlayerConditions conditions;
    std::set<int> skills;
    EventTimer eventTimer;
    int pursuerHealth{500};
};
struct EquipmentStats{int attack{},defense{},spirit{},agility{};};
struct Equipment{int mainWeapon{}; int auxiliaryWeapon{}; int armor{};int accessory{};int other{};};
struct WorldFactDefinitions{
    std::map<int,std::string> switches;
    std::map<int,std::string> variables;
    std::set<std::string> flagIds() const;
    std::set<std::string> valueIds() const;
};
const WorldFactDefinitions& f3WorldFacts();
// One owner for facts that survive a room. Content is borrowed for the session lifetime.
class GameSession{
public:
    GameSession(const ItemDefinitions& definitions,const SessionEntry& entry,const WorldFactDefinitions& facts=f3WorldFacts());
    SessionEntry exportState() const;
    // CE141/140 own only source backup fields, never a second whole-game snapshot.
    void captureContinueCheckpoint(const std::map<int,int>& magazineBindings,int map,TilePosition tile);
    GameSession prepareContinueRestore(const std::map<int,int>& magazineBindings,int map,TilePosition tile) const;
    void validateContinueCheckpoint(const std::map<int,int>& magazineBindings,int map,TilePosition tile,bool active) const;
    void commitContinueRestore(GameSession& candidate);
    const std::string& difficulty() const{return identity_.difficulty;}
    int difficultyLevel() const;
    int actorId() const{return identity_.actor;}
    int saveCount() const{return identity_.saveCount;}
    void finishSave(int committedCount,int statistic,bool restoreControls=true);
    void setActorDisplay(const std::string& name,const std::string& face,int index){identity_.name=name;actorFace_=face;actorFaceIndex_=index;}
    const std::string& actor8Name() const{return actor8Name_;}
    void setActor8Name(std::string name){if(name!="dammy"&&name!="　通关数据"){throw std::runtime_error("Unsupported Actor8 name");}actor8Name_=std::move(name);}
    void joinParty(int actor);
    void leaveParty(int actor);
    EndingScoreInput endingScoreInput() const;
    void commitEndingRank();
    bool commitEndingUnlock(int flag);
    void recordEndingSpecialAction(int commonEvent);
    void tickPlayClock();
    bool inParty(int actor) const{return std::find(identity_.party.begin(),identity_.party.end(),actor)!=identity_.party.end();}
    void seedMagazine(int item,int rounds);
    int count(int item) const;
    bool canSelectItem(int item) const;
    bool convertTool(int from,int to);
    bool canMix(const MixingRecipe&) const;
    bool mix(const MixingRecipe&);
    ItemUseResult useMedicine(int item);
    ItemUseResult selectWeaponAmmunition(int item,const std::vector<int>& weaponForms);
    int itemUses(int item) const;
    bool skillUsable(const SkillDefinition&) const;
    void applyCorrosion(){if(hp_>0){conditions_.corroded=true;}}
    bool normalizeAccessKeys();
    SkillUseResult castSkill(const SkillDefinition& skill);
    int quickshotTicks() const{return quickshotTicks_;}
    bool knowsSkill(int id) const;
    void replaceKnownSkill(int from,int to);
    void learnSkill(int id){skills_.insert(id);}
    void forgetSkill(int id){skills_.erase(id);}
    const std::set<int>& skills() const{return skills_;}
    int magazine(int item) const;
    const Equipment& equipment() const;
    void grant(int item,int amount);
    bool discard(int item,int amount);
    bool equip(int item,int slot=1);
    bool canEquip(int item,int slot) const;
    int equipped(int slot) const;
    bool canFireRound(int slot=1) const;
    bool consumeRound(int slot=1);
    bool consumeEquippedItem(int slot);
    int reload(int slot=1);
    bool hasReserve(int slot=1) const;
    void organizeAmmo();
    bool flag(int id) const;
    void setFlag(int id,bool value);
    int variable(int id) const;
    void setVariable(int id,int value);
    bool worldFlag(const std::string& id) const;
    void setWorldFlag(const std::string& id,bool value);
    int worldValue(const std::string& id) const;
    void setWorldValue(const std::string& id,int value);
    int hp() const;
    int maxHp() const{return maxHp_;}
    int tp() const{return tp_;}
    int maxTp() const{return maxTp_;}
    int loseHp(int amount,bool lethal=true);
    void restoreActor();
    void addResources(int hp,int tp);
    int attack() const;
    int defense() const;
    int auxiliaryAttack() const;
    EquipmentStats equipmentStats(int replacement=0,int slot=0) const;
    void clearAimingAid(bool unequipAttachment=false);
    void resetRoomFacts();
    const PlayerConditions& conditions() const;
    void startBleeding(int ticks);
    void startPoison(){conditions_.poisoned=true;}
    void setNeuralParalysis(bool value){conditions_.neuralParalysis=value;}
    bool advanceNeuralRecoveryWait(bool enabled);
    bool neuralRecoveryWaiting() const{return neuralRecoveryWait_>=0;}
    bool advanceNeuralEffect(bool enabled);
    void startRecovery(int recoveryTicks,int invulnerableTicks);
    void tickMapCommonEffects(bool allowStarts);
    void tickConditions();
    void walkedStep(int interval,int damage);
    const EventTimer& eventTimer() const{return eventTimer_;}
    void setEventTimer(EventTimerSource source,int seconds);
    void startEventTimer(bool visible,bool battle);
    void startEventTimer(EventTimerSource source,int seconds,bool visible,bool battle);
    void stopEventTimer();
    void tickEventTimer();
    int pursuerHealth() const{return pursuerHealth_;}
    int damagePursuer(int amount);
    void resetPursuerHealth(){pursuerHealth_=500;}
    bool advanceBurnWait(int commonEvent,bool enabled);
    int burnWaitRemaining(int commonEvent) const;
    BurnPictureStage burnPictureStage(int commonEvent) const;
    void setBurnPictureStage(int commonEvent,BurnPictureStage stage);
    BloodPictureProgress bloodPictureProgress(int commonEvent=15) const;
    void setBloodPictureProgress(BloodPictureProgress progress,int commonEvent=15);
    void finishBurnWait(int commonEvent);
private:
    friend class WarehouseSaveState;
    const ItemDefinitions& definitions_;
    const WorldFactDefinitions& facts_;
    // Identity/progress fields only; mutable gameplay collections remain below.
    std::string actor8Name_{"dammy"};
    std::string actorFace_{"face-actor1"};int actorFaceIndex_{1};
    friend class GameApplication;
    friend class OpeningController;
    void beginOpeningInitialization();
    void confirmOpeningDifficulty(const std::string&);
    void finishOpeningInitialization(const std::vector<int>& easyPreseed);
    enum class OpeningInitialization{Complete,Choosing,Confirmed};
    OpeningInitialization openingInitialization_{OpeningInitialization::Complete};
    struct Identity{int actor,level,saveCount;std::string name,difficulty;std::vector<int> party;} identity_;
    std::map<int,int> inventory_,magazines_,variables_,itemUses_;
    std::map<int,bool> flags_;
    std::map<std::string,bool> worldFlags_;
    std::map<std::string,int> worldValues_;
    std::set<int> skills_;
    Equipment equipment_;
    int hp_;
    int maxHp_,tp_,maxTp_;
    int baseAttack_;
    int baseDefense_;
    int baseAgility_,baseSpirit_;
    PlayerConditions conditions_;
    int quickshotTicks_{};
    int playClockWait_{60};
    EventTimer eventTimer_;
    int pursuerHealth_{500};
    // CE24–31 keep their waits across ordinary maps; Load rebuilds these finite waits.
    std::array<int,8> burnWaits_{};
    // CE113 pauses when OFF; ordinary candidates copy it, Load starts at C1.
    int neuralRecoveryWait_{-1};
    int neuralEffectWait_{-1};
    std::array<BurnPictureStage,8> burnPictureStages_{};
    // CE24–31 use pairs 93/94, 107/108, ... 191/192; CE15 shares the first pair.
    std::array<int,19> pictureScratch_{};
    std::array<BloodPictureProgress,3> bloodPictureProgress_{};
};
