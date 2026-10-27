#include "combat_scene.h"
#include "full_content.h"
#include "opening_controller.h"
#include "meeting_controller.h"
#include "scene_presentation.h"
#include "weapon_controller.h"
#include "enemy_controller.h"
#include "enemy_host.h"
#include "acid_projectile.h"
#include "guard_controller.h"
#include "crocodile_controller.h"
#include "dekisugi_controller.h"
#include "dora_controller.h"
#include "cash_controller.h"
#include "final_boss_controller.h"
#include "warehouse_g4_controller.h"
#include "escort_controller.h"
#include <algorithm>
#include <atomic>
#include <stdexcept>
#include <utility>
namespace{
    bool pressed(const InputFrame& f,InputAction a){return f.buttons[static_cast<std::size_t>(a)].pressed;}
}
namespace{
    std::uint64_t nextSceneIdentity(){static std::atomic<std::uint64_t> identity{1};return identity.fetch_add(1);}
}


CombatScene::CombatScene(const CombatContent& content,GameSession& session,const std::string& entry,Random random,PreparePresentation preparePresentation)
    :CombatScene(content,session,content.entries.at(entry),std::move(random),std::move(preparePresentation)){
    if(session_.flag(52)&&!session_.flag(101)){presentation_.push_back({PresentationEvent::Kind::Music,"",readSound(content_.media.at("sounds").at("first-encounter")),{},{}});}
}
CombatScene::CombatScene(const CombatContent& content,GameSession& session,const CombatEntry& start,Random random,PreparePresentation preparePresentation)
    :content_(content),session_(session),random_(std::move(random)),preparePresentation_(std::move(preparePresentation)){
    if(!random_){auto engine=std::make_shared<std::mt19937>(std::random_device{}());random_=[engine](int lo,int hi){return std::uniform_int_distribution<int>(lo,hi)(*engine);};}
    messageState_.faceImage="face-actor1";messageState_.faceIndex=1;
    rebuild(start.map,start.playerTile,start.direction,&start);weapon_=std::make_unique<WeaponController>(*this);
}
CombatScene::~CombatScene()=default;
bool CombatScene::bonusRoom() const{return content_.catalog.contains("bonus")&&map_==content_.catalog.at("bonus").at("map");}
void CombatScene::startBonusRoom(){if(!bonusRoom()){throw std::logic_error("Bonus entry outside room");}playerVisible_=false;startContentFlow(content_.catalog.at("bonus").at("entry_flow"));}
void CombatScene::startOpening(){opening_=std::make_unique<OpeningController>(*this);}
bool CombatScene::openingActive() const{return opening_&&!opening_->finished();}
bool CombatScene::worldSupported() const{
    if(!content_.catalog.contains("difficulty")){return true;}
    const auto& world=content_.catalog.at("difficulty").at("support").at("world");
    if(!world.contains(session_.difficulty())){return false;}const auto& maps=world.at(session_.difficulty());
    return std::find(maps.begin(),maps.end(),"*")!=maps.end()||std::find(maps.begin(),maps.end(),map_)!=maps.end();
}
std::string CombatScene::openingStage() const{return opening_?opening_->stage():std::string{};}
const ExplorationScene& CombatScene::world() const{return *world_;}
const SceneDefinition& CombatScene::definition() const{return definition_;}
const GameSession& CombatScene::session() const{return session_;}
const Dialogue& CombatScene::dialogue() const{return dialogue_;}
std::optional<ApplicationMode> CombatScene::takeApplicationRequest(){
    if(requestedMode_==ApplicationMode::Playing){return std::nullopt;}
    return std::exchange(requestedMode_,ApplicationMode::Playing);
}
void CombatScene::dismissBoundary(){
    exit_.reset();requestedMode_=ApplicationMode::Playing;
    if(!pendingTransition_.empty()){suppressedExit_=content_.transitions.at(pendingTransition_).at("object");}
}

ForegroundKind CombatScene::foreground() const{return foreground_;}
std::uint64_t CombatScene::generation() const{return generation_;}
std::vector<PresentationEvent> CombatScene::takePresentation(){return std::exchange(presentation_,{});}
bool CombatScene::raised() const{return weapon_->raised();}
bool CombatScene::aiming() const{return weapon_->aiming();}
const std::optional<ExitRequest>& CombatScene::exitRequest() const{return exit_;}
CombatScene::PreparedWorld CombatScene::prepareWorld(int map,TilePosition tile,Direction direction,const GameSession& session,const CombatEntry* entry) const{
    PreparedWorld prepared;prepared.map=map;prepared.applyEntryEffects=entry==nullptr;
    prepared.definition=content_.makeScene(map,tile,direction,session,entry);
    prepared.world=std::make_unique<ExplorationScene>(prepared.definition);
    const auto enemyId=content_.rules.at("enemy").at("id").get<std::string>();
    for(const auto& actor:prepared.world->objects()){
        if(content_.enemies.contains(actor.id)||content_.expansion.enemies.contains(actor.id)){continue;}
        const auto* selected=content_.activeVariant(content_.objects.at(actor.id),session,
            entry&&entry->pages.contains(actor.id)?std::optional<int>{entry->pages.at(actor.id)}:std::nullopt);
        if(selected&&!selected->autonomousRoute.steps.empty()){prepared.ambient.emplace_back(selected->autonomousRoute,actor.id);}
    }
    if(content_.objects.at(enemyId).map==map&&session.flag(101)){
        auto enemy=findCharacter(*prepared.world,enemyId);const TilePosition corpse{session.variable(396),session.variable(397)};
        enemy.motion=CharacterMotion(corpse,enemy.motion.facing(),8);enemy.layer=0;enemy.through=true;replaceCharacter(*prepared.world,enemy);
        const auto supplyId=content_.interactions.at("supply").at("object").get<std::string>();
        try{auto supply=findCharacter(*prepared.world,supplyId);supply.motion=CharacterMotion(corpse,supply.motion.facing(),8);replaceCharacter(*prepared.world,supply);}catch(const std::runtime_error&){if(!session.flag(54)){throw;}}
    }
    for(const auto& [id,e]:content_.enemies){if(e.map==map&&!e.story&&session.flag(e.deadFlag)){
        auto actor=findCharacter(*prepared.world,id);actor.active=false;actor.through=true;
        const auto corpse=e.rules.value("death_tile",std::array<int,2>{0,0});actor.motion=CharacterMotion({corpse[0],corpse[1]},actor.motion.facing(),8);replaceCharacter(*prepared.world,actor);
    }}
    for(const auto& [id,e]:content_.expansion.enemies){if(e.map==map&&session.flag(e.deadFlag)){
        auto actor=findCharacter(*prepared.world,id);actor.active=false;actor.through=true;
        actor.motion=CharacterMotion({0,0},actor.motion.facing(),actor.motion.speed());replaceCharacter(*prepared.world,actor);
    }}
    if(content_.expansion.entryPlacements.contains(std::to_string(map))){
        for(const auto& placement:content_.expansion.entryPlacements.at(std::to_string(map))){
            const auto id=placement.at("object").get<std::string>();if(entry&&entry->positions.contains(id)){continue;}
            if(!placement.at("if_flag").is_null()&&!session.flag(placement.at("if_flag"))){continue;}
            auto object=findCharacter(*prepared.world,id);const auto& tile=placement.at("tile");object.motion=CharacterMotion({tile[0],tile[1]},object.motion.facing(),object.motion.speed());replaceCharacter(*prepared.world,object);
        }
    }
    if(content_.catalog.contains("warehouse_g4")){const auto& boss=content_.catalog.at("warehouse_g4");if(map==boss.at("map")&&session.flag(boss.at("victory_fact"))){for(const auto& replacement:prepared.definition.upperTileReplacements){prepared.world->substituteUpperTile(replacement.fromIndex,replacement.toIndex);}}}
    if(preparePresentation_){preparePresentation_(prepared.definition);}
    return prepared;
}
void CombatScene::commitWorld(PreparedWorld prepared){
    musicPlayback_.reset();musicFirstPass_=false;creditsMusicWait_=-1;creditsPosition_={};
    if(finalBoss_){finalBoss_->retire();}
    if(warehouseG4_){warehouseG4_->retire();}
    applyEntryEffects_=prepared.applyEntryEffects;
    for(auto& enemy:enemies_){enemy->retire();}
    cancelToolUse();cancelMixing();enemyBloodTypes_.clear();
    gasWait_=-1;storyPresentation_={};panorama_.reset();pictureAnchors_.clear();environmentFlows_.clear();environmentTargets_.clear();lightStates_.clear();
    inputPrompt_.invalidate(generation_);inputRequest_.reset();messageRequest_.reset();dialogue_.invalidate();decisionWaiting_=false;
    routes_.clear();ambientRoutes_=std::move(prepared.ambient);definition_=std::move(prepared.definition);
    finalBoss_.reset();finalBossVictory_.reset();warehouseG4_.reset();cash_.reset();dora_.reset();dekisugi_.reset();escort_.reset();crocodile_.reset();fileReadingLocked_=false;guard_.reset();world_=std::move(prepared.world);map_=prepared.map;generation_=nextSceneIdentity();suppressedExit_.reset();refreshEnemies();
    presentation_.push_back({PresentationEvent::Kind::SceneChanged,{},{},{},0});
    // Ordinary transfers apply map music only after the candidate is committed.
    // Restores supply an entry and restore their saved presentation separately.
    if(prepared.applyEntryEffects&&content_.expansion.mapMusic.contains(std::to_string(map_))){
        const auto& music=content_.expansion.mapMusic.at(std::to_string(map_));
        if(music.at("mode")=="stop"||(music.at("mode")=="play"&&music.at("sound").at("resource").is_null())){presentation_.push_back({PresentationEvent::Kind::StopMusic,{},{},{},0});}
        else if(music.at("mode")=="play"){presentation_.push_back({PresentationEvent::Kind::Music,"map-entry",readSound(music.at("sound")),{},0});}
    }
    camera_.reset(world_->player().motion.position(),definition_.width,definition_.height,definition_.loopX);playerVisible_=true;
    if(content_.full->contains("runtime")&&content_.full->at("runtime").contains("parking")){const auto& g=content_.full->at("runtime").at("parking").at("first_guard");if(g.contains("runtime_profile")&&map_==g.at("map")){guard_=std::make_unique<GuardController>(*this,g);std::erase_if(ambientRoutes_,[&](const auto& r){return guard_->owns(r.actor());});}}
    if(content_.catalog.contains("guard_second")&&map_==content_.catalog.at("guard_second").at("map")){guard_=std::make_unique<GuardController>(*this,content_.catalog.at("guard_second"));std::erase_if(ambientRoutes_,[&](const auto& r){return guard_->owns(r.actor());});}
    if(content_.catalog.contains("crocodile")&&map_==content_.catalog.at("crocodile").at("map")){crocodile_=std::make_unique<CrocodileController>(*this,content_.catalog.at("crocodile"));fileReadingLocked_=true;}
    if(content_.catalog.contains("escort")&&content_.objects.at(content_.catalog.at("escort").at("object").get<std::string>()).map==map_){escort_=std::make_unique<EscortController>(*this,content_.catalog.at("escort"));}
    if(content_.catalog.contains("warehouse_g4")&&map_==content_.catalog.at("warehouse_g4").at("map")){warehouseG4_=std::make_unique<WarehouseG4Controller>(*this,content_.catalog.at("warehouse_g4"));}
    if(content_.catalog.contains("final_boss")&&map_==content_.catalog.at("final_boss").at("map")){finalBoss_=std::make_unique<FinalBossController>(*this,content_.catalog.at("final_boss"));}
    if(content_.catalog.contains("cash")&&map_==content_.catalog.at("cash").at("map")){cash_=std::make_unique<CashController>(*this,content_.catalog.at("cash"));}
    if(content_.catalog.contains("dora")&&map_==content_.catalog.at("dora").at("map")){dora_=std::make_unique<DoraController>(*this,content_.catalog.at("dora"));}
    if(content_.catalog.contains("dekisugi")&&map_==content_.catalog.at("dekisugi").at("map")){dekisugi_=std::make_unique<DekisugiController>(*this,content_.catalog.at("dekisugi"));}
    erasedEvents_.clear();blockedFlows_.clear();environmentFlows_.clear();selectedPages_.clear();saveRequest_.reset();clearSave_.reset();clearMusicTicks_.reset();directSave_.reset();chapterSave_.reset();activeContentFlow_.clear();activeFlowSteps_.clear();flowQuestion_.reset();activeLadder_.reset();
    for(const auto& o:world_->objects()){const auto* p=content_.activeVariant(content_.objects.at(o.id),session_);selectedPages_[o.id]=p?p->page:0;}
    // Reviewed parallel entry event is one-shot per visit. Restores bypass it.
    if(prepared.applyEntryEffects&&!content_.expansion.meetingStory.is_null()){
        const auto& skill=content_.expansion.meetingStory.at("safe_room_skill");
        if(map_==skill.at("map")&&isCurrentPage(skill.at("object"),skill.at("page"))){session_.learnSkill(skill.at("learn"));session_.forgetSkill(skill.at("forget"));}
    }
    if(!content_.catalog.is_null()){
        for(const auto& group:content_.catalog.at("anchored_pictures")){
            if(!isCurrentPage(group.at("object"),group.at("page"))){continue;}
            for(const auto& picture:group.at("pictures")){
                const std::string anchor=picture.at("anchor");
                presentation_.push_back({PresentationEvent::Kind::Cue,picture.at("cue"),{},findCharacter(*world_,anchor).motion.position(),0,anchor});
            }
        }
    }
}
void CombatScene::rebuild(int map,TilePosition tile,Direction direction,const CombatEntry* entry){
    commitWorld(prepareWorld(map,tile,direction,session_,entry));
}
PromptIdentity CombatScene::requestInput(const ChoicePrompt& definition){
    if(messageRequest_||(session_.hp()<=0&&!(foreground_==ForegroundKind::Death&&content_.catalog.contains("continue")&&session_.flag(586)))){throw std::runtime_error("Scene cannot request choice now");}
    const auto identity=inputPrompt_.request(definition,generation_);inputRequest_=identity;return identity;
}
PromptIdentity CombatScene::requestInput(const NumberPrompt& definition){
    if(messageRequest_||session_.hp()<=0){throw std::runtime_error("Scene cannot request number now");}
    const auto identity=inputPrompt_.request(definition,generation_);inputRequest_=identity;return identity;
}
std::optional<PromptCompletion> CombatScene::takeInputResult(PromptIdentity expected){
    if(session_.hp()<=0&&!(foreground_==ForegroundKind::Death&&content_.catalog.contains("continue")&&session_.flag(586))){inputPrompt_.invalidate(generation_);inputRequest_.reset();return std::nullopt;}
    if(expected.scene!=generation_||inputRequest_!=expected){return std::nullopt;}
    auto result=inputPrompt_.takeCompletion(expected);if(result){inputRequest_.reset();}return result;
}
bool CombatScene::cancelInput(PromptIdentity expected){
    if(expected.scene!=generation_||inputRequest_!=expected){return false;}
    inputPrompt_.invalidate(generation_);inputRequest_.reset();return true;
}
void CombatScene::requestMessage(const std::string& id){ScenePresentation(*this).message(id);}
void CombatScene::playSound(const std::string& id){ScenePresentation(*this).sound(id);}
void CombatScene::startRoute(const std::string& id,const std::string& self){ScenePresentation(*this).route(id,self);}
bool CombatScene::routesPending() const{return std::any_of(routes_.begin(),routes_.end(),[](const auto& route){return !route.firstPassComplete();});}
void CombatScene::cue(const std::string& id,const std::string& self){ScenePresentation(*this).cue(id,self);}
void CombatScene::startForeground(ForegroundKind kind,std::string object){decisionWaiting_=false;foreground_=kind;phase_=0;wait_=0;interactionObject_=std::move(object);}
void CombatScene::finishForeground(){decisionWaiting_=false;foreground_=ForegroundKind::None;phase_=0;interactionObject_.clear();}
void CombatScene::updateForeground(){
    if(messageRequest_||wait_>0){return;}
    switch(foreground_){
    case ForegroundKind::None:return;
    case ForegroundKind::Meeting:meeting_->advance();return;
    case ForegroundKind::Mixing:updateMixing();return;
    case ForegroundKind::Inspect:
        if(phase_==0){cue("m4-e4-p7-c1");cue("m4-e4-p7-c2");requestMessage("m4-e4-p7-c3");++phase_;}
        else{cue("m4-e4-p7-c7");finishForeground();}return;
    case ForegroundKind::Gun:
        switch(phase_){
        case 0:session_.setFlag(1,false);session_.setVariable(388,random_(1000,9999));cue("m4-e10-p2-c3");requestMessage(content_.interactions.at("gun").at("message"));++phase_;break;
        case 1:
            for(const auto& [id,n]:content_.interactions.at("gun").at("award").items()){session_.grant(std::stoi(id),n);}
            startRoute("m4-e10-p2-c6",interactionObject_);++phase_;break;
        case 2:if(!routesPending()){cue("m4-e10-p2-c8");++phase_;}break;
        case 3:startRoute("m4-e10-p2-c9");startRoute("m4-e10-p2-c10");++phase_;break;
        case 4:if(!routesPending()){cue("m4-e10-p2-c12");++phase_;}break;
        case 5:startRoute("m4-e10-p2-c13");cue("m4-e10-p2-c14");cue("m4-e10-p2-c15");requestMessage("m4-e10-p2-c16");++phase_;break;
        case 6:
            for(int flag:content_.interactions.at("gun").at("post_sequence_flags_on")){session_.setFlag(flag,true);}
            finishForeground();break;
        }return;
    case ForegroundKind::Supply:
        switch(phase_){
        case 0:session_.setFlag(1,false);session_.setFlag(18,false);session_.setFlag(13,true);cue("m4-e12-p2-c4");cue("m4-e12-p2-c5");cue("m4-e12-p2-c6");requestMessage("m4-e12-p2-c7");++phase_;break;
        case 1:session_.grant(24,content_.interactions.at("supply").at("award").at("24"));if(session_.difficulty()!="hard"){cue("m4-e12-p2-c12");requestMessage("m4-e12-p2-c13");}++phase_;break;
        case 2:
            if(session_.difficulty()!="hard"){session_.grant(31,content_.interactions.at("supply").at("award").at("31"));cue("m4-e12-p2-c17");}session_.setFlag(13,false);session_.setFlag(1,true);session_.setFlag(18,true);session_.setFlag(54,true);
            {auto o=findCharacter(*world_,interactionObject_);o.active=false;replaceCharacter(*world_,o);}finishForeground();break;
        }return;
    case ForegroundKind::Content:updateContentFlow();return;
    case ForegroundKind::Ladder:updateLadder();return;
    default:updateOutcome();return;
    }
}
void CombatScene::investigate(){

    const auto& p=world_->player();if(p.motion.moving()){return;}
    if(warehouseG4_&&warehouseG4_->interact()){return;}
    if(dora_&&dora_->interact()){return;}
    for(const auto& role:{"gun","supply"}){
        const auto id=content_.interactions.at(role).at("object").get<std::string>();const auto& c=content_.interactions.at(role).at("condition");
        if(!session_.flag(c.at("switch_on"))||session_.flag(c.at("switch_off"))){continue;}
        for(const auto& object:world_->objects()){
            if(object.id==id&&object.active&&object.motion.tile()==p.motion.tile()){
                startForeground(std::string(role)=="gun"?ForegroundKind::Gun:ForegroundKind::Supply,id);updateForeground();return;
            }
        }
    }
    for(const auto& object:world_->objects()){
        if(triggerTransfer(object,true)||triggerExploration(object)){return;}
    }
    const auto enemyId=content_.rules.at("enemy").at("id").get<std::string>();
    for(const auto& object:world_->objects()){
        if(object.id==enemyId&&!session_.flag(52)&&adjacentTile(p.motion.tile(),p.motion.facing())==object.motion.tile()){startForeground(ForegroundKind::Inspect,object.id);updateForeground();return;}
    }
}
void CombatScene::movePlayer(const InputFrame& input,bool lockFacing,bool turnOnly){
    movement_=MoveIntent{input,lockFacing,turnOnly};
}
void CombatScene::applyMovement(){
    if(!movement_){return;}if(crocodile_){crocodile_->movementSpeed();}const auto intent=*movement_;movement_.reset();
    const auto& input=intent.input;const bool lockFacing=intent.lockFacing||world_->player().directionLocked,turnOnly=intent.turnOnly;
    auto p=world_->player();if(p.motion.moving()||!input.movement){return;}
    if(!lockFacing){p.motion.face(*input.movement);}
    if(!turnOnly&&world_->canMove(p,*input.movement)){p.motion.beginStep(*input.movement,lockFacing);
        session_.walkedStep(content_.rules.at("bleeding").at("map_steps_per_hp_loss"),content_.rules.at("bleeding").at("hp_loss"));}
    world_->setPlayerState(p);
    if(!p.motion.moving()&&!turnOnly){
        const auto target=adjacentTile(p.motion.tile(),*input.movement);
        for(const auto& object:world_->objects()){if(object.active&&object.motion.tile()==target&&(triggerEncounter(object,true)||(!weapon_->raised()&&triggerExploration(object,true)))){break;}}
    }
}
bool CombatScene::restoreMessageDisplay(std::uint64_t scene,const MessageDisplay& display){
    if(!active_||scene!=generation_||messageRequest_||inputRequest_){return false;}
    messageState_.display=display;return true;
}
void CombatScene::deactivate(){
    musicPlayback_.reset();musicFirstPass_=false;creditsMusicWait_=-1;
    finalBossVictory_.reset();
    if(finalBoss_){finalBoss_->retire();}
    if(warehouseG4_){warehouseG4_->retire();}
    for(auto& enemy:enemies_){enemy->retire();}
    for(auto& acid:acidProjectiles_){acid->retire();}
    if(!active_){return;}
    clearRegistrationKeyboard();
    if(content_.catalog.contains("registration")&&session_.flag(content_.catalog.at("registration").at("session_fact"))){session_.setFlag(content_.catalog.at("registration").at("session_fact"),false);session_.setFlag(1,true);session_.setFlag(18,true);}
    clearDrainage();
    meeting_.reset();
    cancelToolUse();cancelMixing();
    playerRecovering_=false;playerRecoveryWait_=0;
    storyPresentation_={};
    active_=false;panorama_.reset();weapon_->cancel(ActionCancel::Transition);routes_.clear();ambientRoutes_.clear();
    inputPrompt_.invalidate(generation_);inputRequest_.reset();dialogue_.invalidate();messageRequest_.reset();decisionWaiting_=false;
    pictureAnchors_.clear();saveRequest_.reset();clearSave_.reset();clearMusicTicks_.reset();directSave_.reset();chapterSave_.reset();pendingWorld_.reset();activeContentFlow_.clear();activeFlowSteps_.clear();flowQuestion_.reset();activeLadder_.reset();
    requestedMode_=ApplicationMode::Playing;exit_.reset();pendingTransition_.clear();suppressedExit_.reset();
}
void CombatScene::update(const InputFrame& input){
    if(!active_){return;}
    if(!openingActive()&&!worldSupported()){
        if(!unsupportedWorldNotified_){presentation_.push_back({PresentationEvent::Kind::Notice,"当前开发包尚未开放该难度的地图游玩。按X返回标题。",{},{},600});unsupportedWorldNotified_=true;}
        if(pressed(input,InputAction::Cancel)){requestedMode_=ApplicationMode::Title;}return;
    }
    decisionHeld_=input.buttons[static_cast<std::size_t>(InputAction::Confirm)].held;
    world_->advanceClock();
    if(panorama_){
        const auto& p=panorama_->definition;const auto& size=content_.media.at("images").at(p.image).at("size_px");
        for(int axis=0;axis<2;++axis){
            const bool automatic=axis?p.autoY:p.autoX;const int speed=axis?p.speedY:p.speedX;
            const auto span=size.at(axis).get<std::int64_t>()*32;
            if(automatic&&speed){auto& phase=panorama_->phase[axis];phase=((phase+(speed<0?1:-1)*(1<<std::abs(speed)))%span+span)%span;}
        }
    }
    session_.tickEventTimer();
    session_.tickPlayClock();
    refreshObjectPages();
    if(openingActive()){opening_->update(input);return;}
    if(meeting_&&pressed(input,InputAction::Confirm)){meeting_->retry();}
    movement_.reset();camera_.tick(world_->player().motion.position());
    // Parallel player timers survive map transfer; equipment mode returns before this point.
    const bool recovering=session_.conditions().recoveryTicks>0;
    bool playerResponseStep=playerRecovering_||(content_.full->contains("runtime")&&content_.full->at("runtime").contains("player_recovery")&&session_.flag(45));
    const int protectionBefore=session_.conditions().invulnerableTicks;
    session_.tickConditions();updatePlayerProtection(protectionBefore);
    updateQuickshotPresentation();
    if(recovering){
        const int remaining=session_.conditions().recoveryTicks;
        const bool ownsPose=!grabOwner_&&foreground_==ForegroundKind::None&&session_.hp()>0&&
            std::none_of(routes_.begin(),routes_.end(),[](const auto& route){
                return route.actor()=="player"&&route.source()!="ce9-c9";
            });
        if(ownsPose&&remaining>0&&remaining%6==0){cue("ce9-c8");startRoute("ce9-c9");}
        if(remaining==0){
            if(content_.catalog.contains("access_keys")&&session_.flag(11)){session_.setFlag(10,false);cancelPlayerActions("ce5");session_.setFlag(11,false);session_.setFlag(1,true);session_.setVariable(29,0);}
            // Only retire this flow's route; a later foreground may own the actor.
            std::erase_if(routes_,[](const auto& route){return route.actor()=="player"&&route.source()=="ce9-c9";});
            if(!grabOwner_){session_.setFlag(10,false);}
            if(ownsPose){weapon_->cancel(ActionCancel::Grabbed);}
        }
    }
    updatePlayerRecovery(finalBoss_?finalBoss_->recoveryInput(input):warehouseG4_?warehouseG4_->recoveryInput(input):cash_?cash_->recoveryInput(input):dora_?dora_->recoveryInput(input):input);
    updateNeuralRecovery();
    session_.setVariable(9,session_.hp());
    session_.setVariable(40,session_.defense()); // CE1 samples once before source-ordered map events.
    sourcePlayerBody_=sampleActorScreen("player");sourcePlayerBody_.y-=8;
    for(auto& enemy:enemies_){enemy->updateParallel();}
    for(auto& acid:acidProjectiles_){acid->update();}
    updateGasMonitor();
    for(auto& enemy:enemies_){if(enemy->hasParallelMapBehavior()){enemy->update(input);}}
    updateEnvironmentRegistrations();
    updateTargetContacts();
    if(session_.hp()<=0){startDeath();}
    ScenePresentation(*this).advanceRoutes();
    if(finalBoss_){weapon_->advanceAttack();const auto victory=finalBoss_->update(input);if(victory){
        finalBossVictory_=FinalBossVictory{victory->scene,victory->object,victory->page};
        if(content_.catalog.contains("epilogue")){
            const auto& ending=content_.catalog.at("epilogue");
            if(victory->object!=ending.at("victory_object").get<std::string>()||victory->page!=ending.at("victory_page")){throw std::logic_error("Unbound epilogue victory");}
            startContentFlow(ending.at("victory_flow"));
        }else{startForeground(ForegroundKind::Boss);presentation_.push_back({PresentationEvent::Kind::Notice,"最终首领已击败。",{},{},180});}
    }}
    if(finalBoss_){finalBoss_->updateVictoryPresentation();}
    updateCreditsMusic();
    if(guard_){guard_->update(input);}
    if(warehouseG4_){warehouseG4_->update(input);}
    if(cash_){const auto flow=cash_->update(input);if(flow){startContentFlow(*flow);}}
    if(dora_){const auto flow=dora_->update(input);if(flow){startContentFlow(*flow);}}
    if(dekisugi_){const auto flow=dekisugi_->update();if(flow){startContentFlow(*flow);}}
    if(crocodile_){crocodile_->update();}
    if(escort_){escort_->update();}
    updateRescueCrises();
    updateRescueEnvironment();
    updateUndergroundExplosion();
    updateEnvironment();
    updateStoryPresentation();
    updateUndergroundLighting();
    if(wait_>0){--wait_;}
    if(inputRequest_){
        if(inputPrompt_.active()){
            const auto feedback=inputPrompt_.update(warehouseG4_?warehouseG4_->promptInput(input):input);
            if(feedback!=PromptFeedback::None){
                const std::string kind=feedback==PromptFeedback::Cursor?"cursor":feedback==PromptFeedback::Decision?"decision":"cancel";
                presentation_.push_back({PresentationEvent::Kind::Sound,"",readSound(content_.media.at("ui").at("menu").at("sounds").at(kind)),{},0});
            }
        }else{updateForeground();}
        world_->advanceCharacters();return;
    }
    if(messageRequest_){
        const bool choiceFollows=(foreground_==ForegroundKind::Death&&phase_==209)||(foreground_==ForegroundKind::Mixing&&mixChoice_)||(foreground_==ForegroundKind::Content&&phase_<static_cast<int>(activeFlowSteps_.size())&&(activeFlowSteps_[phase_].at("kind")=="discard_key"||activeFlowSteps_[phase_].at("kind")=="choice"));
        dialogue_.update(!choiceFollows&&pressed(input,InputAction::Confirm),!choiceFollows&&pressed(input,InputAction::Cancel));
        if(choiceFollows&&dialogue_.view().phase==DialoguePhase::AwaitingInput){
            flowQuestion_=dialogue_.view();dialogue_.invalidate();messageRequest_.reset();updateForeground();world_->advanceCharacters();return;
        }
        if(auto done=dialogue_.takeCompletion()){if(done->request==*messageRequest_){messageRequest_.reset();}}
        world_->advanceCharacters();return;
    }
    if(decisionWaiting_){
        if(pressed(input,InputAction::Confirm)){decisionWaiting_=false;session_.setVariable(229,5);updateForeground();}
        world_->advanceCharacters();return;
    }
    if(foreground_==ForegroundKind::Registration){updateRegistrationKeyboard(input);world_->advanceCharacters();return;}
    if(foreground_==ForegroundKind::Drainage){updateDrainage(input);world_->advanceCharacters();return;}
    if(foreground_!=ForegroundKind::None){if(foreground_==ForegroundKind::Death){for(auto& enemy:enemies_){enemy->updateDeathPresentation();}}updateForeground();world_->advanceCharacters();return;}
    updateHeatKeyAreas();updateToolAreas();checkAutorun();
    if(foreground_!=ForegroundKind::None){updateForeground();world_->advanceCharacters();return;}
    // Input locks must not freeze a weapon action that already owns its continuation.
    if(!grabOwner_&&!playerResponseStep&&(!finalBoss_||(!finalBoss_->airEquipped()&&(weapon_->busy()||(session_.flag(1)&&!session_.flag(19)))))){weapon_->update(input);}
    if(finalBoss_&&finalBoss_->airEquipped()&&!grabOwner_&&!playerResponseStep){finalBoss_->updateFreeControl(input);}
    if(requestedMode_!=ApplicationMode::Playing){return;}
    if(!finalBoss_){weapon_->advanceAttack();}
    if(storyEnding_){const int variant=*storyEnding_;storyEnding_.reset();enemyEnded(variant);}
    if(foreground_!=ForegroundKind::None){world_->advanceCharacters();return;}
    for(auto& enemy:enemies_){if(!enemy->hasParallelMapBehavior()){enemy->update(input);}}
    grantNextGrab();
    playerResponseStep|=content_.full->contains("runtime")&&content_.full->at("runtime").contains("player_recovery")&&session_.flag(45);
    if(!grabOwner_&&!playerResponseStep&&foreground_==ForegroundKind::None&&session_.hp()>0){applyMovement();}
    const bool playerRoute=std::any_of(routes_.begin(),routes_.end(),[](const auto& route){return route.actor()=="player";});
    if(!grabOwner_&&!playerResponseStep&&foreground_==ForegroundKind::None&&!playerRoute&&!weapon_->raised()&&pressed(input,InputAction::Confirm)){investigate();}
    if(session_.hp()==0){startDeath();}
    world_->advanceCharacters();
    if(foreground_==ForegroundKind::None&&!world_->player().motion.moving()){checkTouchTransitions();}
}

void CombatScene::refreshEnemies(){
    enemies_.clear();grabOwner_.reset();grabQueue_.clear();storyEnding_.reset();
    std::vector<const EnemyDefinition*> definitions;
    for(const auto& [id,e]:content_.enemies){if(e.map==map_){definitions.push_back(&e);}}
    std::sort(definitions.begin(),definitions.end(),[](const auto* a,const auto* b){return a->sourceOrder<b->sourceOrder;});
    for(const auto* e:definitions){enemies_.push_back(makeEnemyBehavior(*this,*e));}
    for(const auto& [id,e]:content_.expansion.enemies){if(e.map==map_){enemies_.push_back(makeEnemyBehavior(*this,e));}}
    acidProjectiles_.clear();if(content_.catalog.contains("acid_projectiles")){for(const auto& [id,r]:content_.catalog.at("acid_projectiles").items()){if(r.value("runtime_enabled",false)&&r.at("map")==map_){acidProjectiles_.push_back(std::make_unique<AcidProjectile>(*this,id,r));}}}
    const auto order=[&](const std::string& id){const auto it=content_.enemies.find(id);return it!=content_.enemies.end()?it->second.sourceOrder:content_.expansion.enemies.at(id).sourceOrder;};
    std::sort(enemies_.begin(),enemies_.end(),[&](const auto& a,const auto& b){return order(a->id())<order(b->id());});
}
std::vector<EnemyView> CombatScene::enemyStates() const{
    std::vector<EnemyView> result=guard_?guard_->views():std::vector<EnemyView>{};for(const auto& e:enemies_){result.push_back({e->id(),e->accumulatedDamage(),e->mode(),e->holdsPlayer(),e->sourceState()});}return result;
}
std::optional<int> CombatScene::companionHp() const{return guard_?std::optional<int>{guard_->hp()}:std::nullopt;}
AttackResolution CombatScene::resolveAttack(GridPosition center,bool muzzle,int advances,bool aimed,std::set<std::string>& resolved,AttackKind kind){
    const int page=kind==AttackKind::HandgunHp?(muzzle?2:session_.flag(159)||session_.variable(35)<=0?3:2):kind==AttackKind::Magnum||kind==AttackKind::De?5:kind==AttackKind::Knife?2:kind==AttackKind::Handgun&&!muzzle&&(aimed||session_.variable(35)<=0)?4:muzzle?2:3;
    const int half=kind==AttackKind::Knife?0:muzzle?1:5;
    return resolveAttack(AttackSample{center,{half,half},kind,page,advances,aimed},resolved);
}
AttackResolution CombatScene::resolveAttack(const AttackSample& sample,std::set<std::string>& resolved){
    if(!active_){return {};}
    validateAttackSample(sample);
    AttackResolution result=finalBoss_?finalBoss_->receive(sample,resolved):warehouseG4_?warehouseG4_->receive(sample,resolved):cash_?cash_->receive(sample,resolved):dora_?dora_->receive(sample,resolved):dekisugi_?dekisugi_->receive(sample,resolved):crocodile_?crocodile_->receive(sample,resolved):guard_?guard_->receive(sample,resolved):AttackResolution{};
    for(auto& enemy:enemies_){
        if(resolved.contains(enemy->id())){continue;}
        const auto hit=enemy->receiveAttack(sample);
        result.contacts+=hit.contact;result.damage+=hit.damage;result.endRequested|=hit.endRequested;
        if(hit.processed){resolved.insert(enemy->id());++result.processed;}
    }
    const auto environment=resolveEnvironmentAttack(sample,resolved);
    result.contacts+=environment.contacts;result.processed+=environment.processed;result.damage+=environment.damage;result.endRequested|=environment.endRequested;
    return result;
}
bool CombatScene::resolveShot(GridPosition center,bool muzzle,int advances,bool aimed,std::set<std::string>& resolved,AttackKind kind){return resolveAttack(center,muzzle,advances,aimed,resolved,kind).processed>0;}

void CombatScene::grantNextGrab(){
    if(grabOwner_||session_.hp()<=0||foreground_!=ForegroundKind::None){return;}
    while(!grabQueue_.empty()){
        const auto id=grabQueue_.front();grabQueue_.erase(grabQueue_.begin());
        if(guard_&&guard_->owns(id)){grabOwner_=id;if(guard_->grantGrab(id)){return;}grabOwner_.reset();continue;}
        for(auto& e:enemies_){if(e->id()==id&&e->mode()==EnemyMode::PendingGrab){grabOwner_=id;if(e->beginGrab()){return;}grabOwner_.reset();}}
    }
}
void CombatScene::releaseGrabOwner(const std::string& id){if(grabOwner_==id){grabOwner_.reset();}std::erase(grabQueue_,id);}
void CombatScene::yieldBehind(const std::string& id){
    const auto actor=findCharacter(*world_,id);const auto behind=adjacentTile(actor.motion.tile(),static_cast<Direction>((static_cast<int>(actor.motion.facing())+2)%4));
    const CharacterState* selected=nullptr;
    for(const auto& o:world_->objects()){if(o.id!=id&&o.active&&o.motion.tile()==behind&&(!selected||o.eventId>selected->eventId)){selected=&o;}}
    if(selected){for(auto& e:enemies_){if(e->id()==selected->id){e->collisionYield(selected->motion.facing());return;}}}
}

void CombatScene::startMeeting(){
    auto candidate=std::make_unique<MeetingController>(*this);
    weapon_->cancel(ActionCancel::Foreground);meeting_=std::move(candidate);startForeground(ForegroundKind::Meeting);updateForeground();
}

void CombatScene::updatePlayerRecovery(const InputFrame& input){
    if(!content_.full->contains("runtime")||!content_.full->at("runtime").contains("player_recovery")){return;}
    const auto& definitions=content_.full->at("runtime").at("player_recovery");const auto& recovery=definitions.at("ce36");
    if(!session_.flag(recovery.at("activation_fact"))||session_.flag(recovery.at("dead_fact"))||session_.hp()<=0){playerRecovering_=false;playerRecoveryWait_=0;return;}
    if(!playerRecovering_){
        if(grabOwner_){return;}
        playerRecovering_=true;session_.setFlag(1,false);
    }
    if(playerRecoveryWait_>0){
        if(--playerRecoveryWait_>0){return;}
        const auto& protection=definitions.at("ce13").at("wait_ticks");
        session_.startRecovery(0,protection.at(0).get<int>()+protection.at(1).get<int>());
        session_.setFlag(1,true);session_.setFlag(1,false);cancelPlayerActions(recovery.at("cleanup"));
        session_.setFlag(1,true);session_.setFlag(recovery.at("activation_fact"),false);playerRecovering_=false;return;
    }
    const auto& counter=recovery.at("input");
    if(!messageRequest_&&!inputRequest_){for(auto action:{InputAction::Down,InputAction::Left,InputAction::Right,InputAction::Up}){
        if(input.buttons[static_cast<std::size_t>(action)].pressed){const int id=counter.at("counter_source_variable");session_.setVariable(id,session_.variable(id)+counter.at("increment").get<int>());break;}
    }}
    int threshold=0;
    for(const auto& row:recovery.at("hp_thresholds")){if(row.value("otherwise",false)||session_.hp()<=row.at("at_most").get<int>()){threshold=row.at("input_count");break;}}
    const bool rise=session_.variable(counter.at("counter_source_variable"))>=threshold;
    const auto& phase=recovery.at(rise?"rise":"down");session_.setVariable(31,phase.at("mode"));
    if(!rise){startRoute(phase.at("hold_route"));}
    cue(phase.at("graphic"));startRoute(phase.at("route"));
    if(rise){playerRecoveryWait_=phase.at("wait_ticks");}
}

void CombatScene::updatePlayerProtection(int before){
    if(before<=0||!content_.full->contains("runtime")||!content_.full->at("runtime").contains("player_recovery")){return;}
    const auto& protection=content_.full->at("runtime").at("player_recovery").at("ce13");
    if(session_.flag(protection.at("dead_fact"))||session_.hp()<=0){return;}
    const int remaining=session_.conditions().invulnerableTicks,secondWait=protection.at("wait_ticks").at(1);
    const int stage=remaining==secondWait?0:remaining==0?1:-1;
    if(stage<0){return;}
    const auto& guards=protection.at("graphics_when_facts_off");
    if(!grabOwner_&&!playerRecovering_&&foreground_==ForegroundKind::None&&std::all_of(guards.begin(),guards.end(),[&](const auto& flag){return !session_.flag(flag);})){
        for(const auto& id:protection.at("graphic_pairs").at(stage)){cue(id.get<std::string>());}
    }
    if(remaining==0&&session_.flag(protection.at("enable_control_if_fact"))){session_.setFlag(1,true);}
}

void CombatScene::updateNeuralRecovery(){
    if(!content_.full->contains("runtime")||!content_.full->at("runtime").contains("player_recovery")){return;}
    const auto& definitions=content_.full->at("runtime").at("player_recovery");
    if(!definitions.contains("ce113")){return;}
    const auto& effect=definitions.at("ce112");
    if(session_.advanceNeuralEffect(session_.flag(effect.at("activation_fact")))){
        cue(effect.at("flash"),"player");
        const auto& guards=effect.at("route_when_facts_off");
        if(std::all_of(guards.begin(),guards.end(),[&](const auto& id){return !session_.flag(id);})){
            startRoute(effect.at("route"));
        }
    }
    const auto& recovery=definitions.at("ce113");const int active=recovery.at("activation_fact");
    if(session_.flag(active)&&!session_.neuralRecoveryWaiting()){
        session_.setFlag(recovery.at("presentation_fact"),true);
    }
    if(session_.advanceNeuralRecoveryWait(session_.flag(active))){
        session_.setNeuralParalysis(false);session_.setFlag(recovery.at("presentation_fact"),false);
        const auto& selection=recovery.at("restore_route_selection");
        const bool matches=session_.flag(selection.at("condition").at("id"))==selection.at("condition").at("value").get<bool>();
        startRoute(selection.at(matches?"then":"else"));session_.setFlag(active,false);
    }
}

void CombatScene::updateGasMonitor(){
    if(!content_.full->contains("runtime")){return;}
    const auto& runtime=content_.full->at("runtime");
    if(!runtime.contains("player_recovery")||!runtime.at("player_recovery").contains("ce113")){return;}
    const auto key=std::to_string(map_);const nlohmann::json* monitors=runtime.contains("gas_monitors")?&runtime.at("gas_monitors"):nullptr;
    if(runtime.contains("underground")&&runtime.at("underground").contains("gas_monitors")&&runtime.at("underground").at("gas_monitors").contains(key)){monitors=&runtime.at("underground").at("gas_monitors");}
    if(!monitors||!monitors->contains(key)){return;}
    const auto& monitor=monitors->at(key);
    if(!isCurrentPage(monitor.at("object"),monitor.at("page"))){gasWait_=-1;return;}
    if(gasWait_<0){
        gasEmissionPending_=!monitor.contains("activation_fact")||monitor.at("activation_fact").is_null()||session_.flag(monitor.at("activation_fact"));
        gasWait_=monitor.at(gasEmissionPending_?"wait_ticks":"inactive_wait_ticks");
    }
    if(gasWait_>0){--gasWait_;return;}
    gasWait_=-1;if(!gasEmissionPending_){return;}cue(monitor.at("animation_cue"),monitor.at("object"));
    const auto source=findCharacter(*world_,monitor.at("box_object")).motion.tile();
    const auto player=world_->player().motion.tile();const auto& bounds=monitor.at("bounds_tiles");
    if(player.x<source.x+bounds.at(0).get<int>()||player.x>source.x+bounds.at(1).get<int>()||
       player.y<source.y+bounds.at(2).get<int>()||player.y>source.y+bounds.at(3).get<int>()||
       session_.flag(monitor.at("guard_off"))){return;}
    cue(monitor.at("player_flash"),"player");
    applyPlayerDamage(biteDamage(monitor.at("player_attack"),session_.defense(),random_(-5,2)));
    session_.setNeuralParalysis(true);session_.setFlag(monitor.at("set_fact"),true);
}

void CombatScene::updateTargetContacts(){
    if(!content_.full->contains("runtime")||!content_.full->at("runtime").contains("training_target_contacts")){return;}
    const auto& contacts=content_.full->at("runtime").at("training_target_contacts");
    std::vector<std::string> selected;
    for(const auto& [id,contact]:contacts.items()){
        if(content_.objects.at(id).map==map_){selected.push_back(id);}
    }
    std::sort(selected.begin(),selected.end(),[&](const auto& a,const auto& b){return findCharacter(*world_,a).eventId<findCharacter(*world_,b).eventId;});
    for(const auto& id:selected){
        const auto& contact=contacts.at(id);const auto& object=content_.objects.at(id);
        const auto* page=content_.activeVariant(object,session_);
        if(!page||page->page!=contact.at("page")||page->definition.trigger!=4){continue;}
        if(contact.contains("response_object")){EnemyHost(*this,contact.at("response_object")).requestTargetReaction(id);continue;}
        const auto tile=findCharacter(*world_,contact.at("position_object")).motion.tile();
        if(tile!=world_->player().motion.tile()||std::any_of(contact.at("guards_off").begin(),contact.at("guards_off").end(),[&](const auto& fact){return session_.flag(fact);})){continue;}
        playSound(contact.at("sound"));session_.setVariable(24,contact.at("player_damage_mode"));
        session_.setFlag(contact.at("pulse_fact"),false);session_.setFlag(contact.at("pulse_fact"),true);
        const auto attacker=contact.at("attack_object").get<std::string>();const auto* selectedRules=difficultyEnemyRules(content_,session_,attacker);
        const int attack=(selectedRules?*selectedRules:content_.enemies.at(attacker).rules).at(contact.at("attack_stat").get<std::string>());
        session_.setVariable(23,attack);applyPlayerDamage(biteDamage(attack,session_.defense(),random_(-5,2)));
    }
}

void CombatScene::updateQuickshotPresentation(){
    if(session_.flag(48)){
        const auto& effect=content_.catalog.at("quickshot_presentation");const int period=effect.at("period_ticks");
        if(session_.quickshotTicks()%period==period-1){cue(effect.at("cue"),"player");}
    }
}
void CombatScene::updateReadingEffects(){if(!active_){return;}session_.tickMapCommonEffects(false);session_.tickPlayClock();if(session_.eventTimer().source==EventTimerSource::AccessKey){session_.tickEventTimer();}updateQuickshotPresentation();}

std::optional<FinalBossVictory> CombatScene::takeFinalBossVictory(){return std::exchange(finalBossVictory_,{});}
