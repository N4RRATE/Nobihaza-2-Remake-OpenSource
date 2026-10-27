#include "game_application.h"
#include "full_content.h"
#include <algorithm>
#include <utility>
namespace{
    bool pressed(const InputFrame& input,InputAction action){return input.buttons[static_cast<std::size_t>(action)].pressed;}
}
GameApplication::GameApplication(CombatContent content,const std::string& entry,CombatScene::Random random,CombatScene::PreparePresentation preparePresentation)
    :content_(std::move(content)),random_(std::move(random)),preparePresentation_(std::move(preparePresentation)){if(!content_.catalog.empty()){saveFiles_=std::make_unique<SaveFiles>(content_);}if(entry=="title"){returnToTitle();}else{restartDevelopmentEntry(entry);}}
void GameApplication::configureSaveDirectory(const std::filesystem::path& path,SaveFiles::Fault fault){
    if(saving_||loading_){throw std::logic_error("Cannot replace save service during transaction");}saveFiles_=std::make_unique<SaveFiles>(content_,path,std::move(fault));
    if(mode_==ApplicationMode::Title){titleLoadEnabled_=saveFiles_->hasArtifacts();menuSelection_=titleLoadEnabled_?1:0;}
}
SaveSnapshot GameApplication::captureF5Recovery() const{
    if(mode_!=ApplicationMode::Playing||!scene_){throw std::runtime_error("Development recovery requires active player control");}
    return ::captureF5Recovery(content_,*scene_,presentationState_);
}
void GameApplication::restartDevelopmentEntry(const std::string& entry){
    auto session=std::make_unique<GameSession>(content_.items,content_.entries.at(entry).session,content_.facts);
    auto scene=std::make_unique<CombatScene>(content_,*session,entry,random_,preparePresentation_);
    // Prepare the entire candidate before invalidating a live game.
    loading_.reset();saving_.reset();reading_.reset();actionAfterReading_.reset();menu_.reset();scene_.reset();session_=std::move(session);scene_=std::move(scene);
    mode_=ApplicationMode::Playing;menuSelection_=0;menuSlot_=1;quitRequested_=false;presentation_.clear();pendingPresentationReset_=true;resetAlarm();alarmPresentation_.clear();
}
std::vector<PresentationEvent> GameApplication::takePresentation(){
    auto result=scene_?scene_->takePresentation():std::vector<PresentationEvent>{};
    result.insert(result.begin(),alarmPresentation_.begin(),alarmPresentation_.end());alarmPresentation_.clear();
    if(std::exchange(pendingPresentationReset_,false)){result.insert(result.begin(),{PresentationEvent::Kind::ResetGame,{},{},{},0});}
    if(saving_){auto events=saving_->takePresentation();result.insert(result.end(),events.begin(),events.end());}
    if(reading_){auto events=reading_->takePresentation();result.insert(result.end(),events.begin(),events.end());}
    result.insert(result.end(),presentation_.begin(),presentation_.end());presentation_.clear();return result;
}
void GameApplication::playUiSound(const std::string& kind){
    const auto& ui=content_.media.at("ui").at((mode_==ApplicationMode::Title||mode_==ApplicationMode::Loading)?"title":"menu");
    presentation_.push_back({PresentationEvent::Kind::Sound,"",readSound(ui.at("sounds").at(kind)),{},0});
}
std::vector<int> GameApplication::equipmentChoices() const{
    std::vector<int> choices{0};if(!session_){return choices;}
    for(const auto& [id,item]:content_.items){if(item.equipmentType==menuSlot_&&session_->count(id)>0){choices.push_back(id);}}
    return choices;
}
void GameApplication::submitTool(const ToolUseRequest& request){
    if(scene_){scene_->finishMixingMenu();}
    const bool applied=scene_&&scene_->useTool(request);
    if(scene_){scene_->cancelToolUse();}menu_.reset();mode_=ApplicationMode::Playing;
    if(!applied){playUiSound("buzzer");}
}
void GameApplication::submitMenuAction(const MenuResult& result){
    if(result.kind==MenuResult::Kind::Read){
        for(const auto& [id,definition]:content_.catalog.at("readings").items()){if(definition.at("item")==result.item){reading_=std::make_unique<ReadingController>(content_,*session_,id,scene_->generation());mode_=ApplicationMode::Reading;return;}}
        throw std::runtime_error("Missing selected reading");
    }
    if(result.kind==MenuResult::Kind::Tool&&result.tool){submitTool(*result.tool);return;}
    if(result.kind==MenuResult::Kind::Mix&&result.mix){
        const bool applied=scene_&&scene_->startMixing(*result.mix);menu_.reset();mode_=ApplicationMode::Playing;
        if(!applied){if(scene_){scene_->finishMixingMenu();}playUiSound("buzzer");}return;
    }
    if(scene_){scene_->finishMixingMenu();}menu_.reset();mode_=ApplicationMode::Playing;
}
void GameApplication::updateMenu(const InputFrame& input){
    if(menu_){
        menu_->update(input);const auto sound=menu_->takeSound();
        if(const auto actionSound=menu_->takeActionSound()){presentation_.push_back({PresentationEvent::Kind::Sound,"",*actionSound,{},0});}
        if(sound.starts_with("skill:")){
            const auto& definition=content_.media.at("skills").at(sound.substr(6));
            if(!definition.at("sound").at("resource").is_null()){presentation_.push_back({PresentationEvent::Kind::Sound,"",readSound(definition.at("sound")),{},0});}
        }else if(!sound.empty()){playUiSound(sound);}
        if(const auto result=menu_->takeResult()){
            if(result->kind==MenuResult::Kind::Close){
                if(content_.catalog.contains("special_equipment")&&session_->equipped(4)==content_.catalog.at("special_equipment").at("glasses")){session_->recordEndingSpecialAction(89);}
                scene_->cancelToolUse();
                if(reading_){actionAfterReading_=*result;reading_->close();mode_=ApplicationMode::Reading;}else{submitMenuAction(*result);}
            }
            else if(result->kind==MenuResult::Kind::Title){returnToTitle();}
            else if(result->kind==MenuResult::Kind::Tool||result->kind==MenuResult::Kind::Mix){
                if(reading_){actionAfterReading_=*result;reading_->close();mode_=ApplicationMode::Reading;}
                else{submitMenuAction(*result);}
            }
            else if(result->kind==MenuResult::Kind::Read){
                if(scene_->fileReadingLocked()){playUiSound("buzzer");return;}
                for(const auto& [id,definition]:content_.catalog.at("readings").items()){if(definition.at("item")==result->item){
                    if(reading_&&reading_->definition()==id){reading_->repeat();mode_=ApplicationMode::Reading;}
                    else if(reading_){actionAfterReading_=*result;reading_->close();mode_=ApplicationMode::Reading;}
                    else{submitMenuAction(*result);}break;}}
            }
        }return;
    }
    if(pressed(input,InputAction::Cancel)){playUiSound("cancel");mode_=ApplicationMode::Playing;return;}
    if(pressed(input,InputAction::Left)||pressed(input,InputAction::Right)){menuSlot_=menuSlot_==1?2:1;menuSelection_=0;playUiSound("cursor");}
    const auto choices=equipmentChoices();
    if(pressed(input,InputAction::Down)){menuSelection_=(menuSelection_+1)%static_cast<int>(choices.size());playUiSound("cursor");}
    if(pressed(input,InputAction::Up)){menuSelection_=(menuSelection_+static_cast<int>(choices.size())-1)%static_cast<int>(choices.size());playUiSound("cursor");}
    if(pressed(input,InputAction::Confirm)){
        playUiSound(session_->equip(choices.at(menuSelection_),menuSlot_)?"decision":"buzzer");menuSelection_=0;
    }
}
bool GameApplication::newGame(std::string& error){
    try{
        const auto& init=content_.catalog.at("opening").at("normal_initialization");SessionEntry fresh;fresh.party.clear();
        fresh.hp=fresh.maxHp=init.at("hp");fresh.tp=fresh.maxTp=init.at("tp");fresh.actorName=init.at("name");
        fresh.skills=init.at("skills").get<std::set<int>>();
        const auto& attributes=init.at("base_attributes");fresh.baseAttack=attributes.at("attack");fresh.baseDefense=attributes.at("defense");fresh.baseSpirit=attributes.at("spirit");fresh.baseAgility=attributes.at("agility");
        return startOpening(std::move(fresh),error);
    }catch(const std::exception& e){error=e.what();return false;}
}
bool GameApplication::enterBonusRoom(const PostClearRequest& request,std::string& error){
    try{
        const auto active=postClearRequest();
        if(mode_!=ApplicationMode::Playing||!active||active->scene!=request.scene||active->selection!=request.selection||request.selection!=1||!content_.catalog.contains("bonus")){throw std::runtime_error("Stale or unsupported bonus request");}
        const auto& bonus=content_.catalog.at("bonus");
        auto nextSession=std::make_unique<GameSession>(content_.items,session_->exportState(),content_.facts);
        CombatEntry entry;entry.map=bonus.at("map");const auto& tile=bonus.at("tile");entry.playerTile={tile.at(0),tile.at(1)};entry.direction=world().player().motion.facing();entry.sprite=world().player().spriteId.value_or(content_.maps.at(entry.map).player.spriteId);
        auto nextScene=std::make_unique<CombatScene>(content_,*nextSession,entry,random_,preparePresentation_);
        nextScene->startBonusRoom();
        if(prepareRestorePresentation_){prepareRestorePresentation_(nextScene->definition(),{});}else if(preparePresentation_){throw std::runtime_error("Bonus resource preparation is not configured");}
        returnToTitle();session_=std::move(nextSession);scene_=std::move(nextScene);mode_=ApplicationMode::Playing;error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
bool GameApplication::startNewCycle(const PostClearRequest& request,std::string& error){
    try{
        const auto active=postClearRequest();
        if(mode_!=ApplicationMode::Playing||!active||active->scene!=request.scene||active->selection!=request.selection||request.selection!=0||!content_.catalog.contains("cycle")){
            throw std::runtime_error("Stale or unsupported new cycle request");
        }
        const auto old=session_->exportState();
        if(old.actor!=1||old.actorName!="大雄"||old.level!=1||!old.party.empty()||old.actor8Name!="dammy"||old.saveCount<0||old.hp!=old.maxHp||old.tp!=old.maxTp||old.maxHp<=0||old.maxHp>9999||old.maxTp<0||old.maxTp>9999){throw std::runtime_error("Invalid Clear cycle source");}
        if(content_.catalog.contains("difficulty")){
            const auto& allowed=content_.catalog.at("difficulty").at("support").at("clear");
            if(std::find(allowed.begin(),allowed.end(),old.difficulty)==allowed.end()){throw std::runtime_error("Unsupported Clear cycle difficulty");}
        }else if(old.difficulty!="normal"){throw std::runtime_error("Unsupported legacy Clear cycle difficulty");}
        for(int id:{574,575,576,580,581,582}){if(!session_->flag(id)){throw std::runtime_error("Incomplete Clear cycle source");}}
        for(int id:old.skills){if(!content_.skills.contains(id)){throw std::runtime_error("Unknown cycle skill");}}
        for(int value:{old.baseAttack,old.baseDefense,old.baseAgility,old.baseSpirit}){if(value<1||value>999){throw std::runtime_error("Invalid cycle attribute");}}
        // CE102/M2 clear equipment, inventory and source S/V; Actor1 pure values survive.
        SessionEntry seed;seed.party.clear();seed.level=old.level;seed.saveCount=old.saveCount;
        seed.hp=old.hp;seed.maxHp=old.maxHp;seed.tp=old.tp;seed.maxTp=old.maxTp;
        seed.baseAttack=old.baseAttack;seed.baseDefense=old.baseDefense;seed.baseAgility=old.baseAgility;seed.baseSpirit=old.baseSpirit;
        seed.skills=old.skills;seed.skills.erase(3);seed.skills.erase(4);
        initializeFullSession(content_,seed);
        for(auto& [id,value]:seed.worldFlags){value=false;}
        for(auto& [id,value]:seed.worldValues){value=0;}
        GameSession candidate(content_.items,seed,content_.facts);
        const auto& cycle=content_.catalog.at("cycle");
        const int previous=session_->variable(cycle.at("counter"));
        if(previous<0){throw std::runtime_error("Invalid cycle counter");}
        candidate.setVariable(cycle.at("counter"),previous>=cycle.at("counter_max").get<int>()?cycle.at("counter_max").get<int>():previous+1);
        for(const auto& reward:cycle.at("rewards")){
            const int flag=reward.at("flag");candidate.setFlag(flag,session_->flag(flag));
            if(candidate.flag(flag)){candidate.grant(reward.at("item"),1);}
        }
        return startOpening(candidate.exportState(),error,true);
    }catch(const std::exception& e){error=e.what();return false;}
}
bool GameApplication::startOpening(SessionEntry fresh,std::string& error,bool cycle){
    try{
        const auto& init=content_.catalog.at("opening").at("normal_initialization");
        initializeFullSession(content_,fresh);
        // Opening commands own their magazine commits; unbound full-package slots keep their defaults.
        for(const auto& stage:content_.catalog.at("opening").at("stages")){
            if(!stage.contains("operations")){continue;}
            for(const auto& op:stage.at("operations")){
                if(op.at("kind")!="initial_value"){continue;}
                if(const auto seed=content_.magazineSeedBindings.find(op.at("original_variable"));seed!=content_.magazineSeedBindings.end()){fresh.magazines.erase(seed->second);}
            }
        }
        auto nextSession=std::make_unique<GameSession>(content_.items,fresh,content_.facts);CombatEntry entry;entry.map=init.at("start").at("map");const auto& xy=init.at("start").at("tile");entry.playerTile={xy.at(0),xy.at(1)};entry.direction=Direction::Down;entry.sprite=content_.maps.at(entry.map).player.spriteId;
        if(cycle){const auto& start=content_.catalog.at("cycle").at("start");entry.map=start.at("map");entry.playerTile={start.at("tile").at(0),start.at("tile").at(1)};entry.direction=world().player().motion.facing();}
        auto nextScene=std::make_unique<CombatScene>(content_,*nextSession,entry,random_);nextScene->configurePresentation(preparePresentation_);nextSession->beginOpeningInitialization();nextScene->startOpening();
        // Stage both supported opening branches before retiring the Clear owner.
        if(content_.catalog.contains("cycle")&&preparePresentation_){for(const auto& map:content_.catalog.at("cycle").at("opening_maps")){preparePresentation_(content_.maps.at(map.get<int>()));}}
        if(prepareRestorePresentation_){prepareRestorePresentation_(nextScene->definition(),{});}else if(preparePresentation_){throw std::runtime_error("New game resource preparation is not configured");}
        returnToTitle();session_=std::move(nextSession);scene_=std::move(nextScene);mode_=ApplicationMode::Playing;error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
void GameApplication::failOpening(const std::string& error){returnToTitle();presentation_.push_back({PresentationEvent::Kind::Notice,"开场失败："+error,{},{},600});}
void GameApplication::returnToTitle(){
    attemptedCycleScene_=0;
    loading_.reset();saving_.reset();reading_.reset();actionAfterReading_.reset();menu_.reset();scene_.reset();session_.reset();mode_=ApplicationMode::Title;titleLoadEnabled_=saveFiles_&&saveFiles_->hasArtifacts();menuSelection_=titleLoadEnabled_?1:0;titleOpeningTicks_=0;pendingPresentationReset_=false;presentation_.clear();presentationState_={};resetAlarm();alarmPresentation_.clear();
    presentation_.push_back({PresentationEvent::Kind::ResetGame,{},{},{},0});presentation_.push_back({PresentationEvent::Kind::SystemGraphic,content_.media.at("ui").at("title").at("system_image"),{},{},0});
    const auto& bgm=content_.media.at("ui").at("title").at("bgm");if(!bgm.is_null()){presentation_.push_back({PresentationEvent::Kind::Music,"",readSound(bgm),{},0});}
}
bool GameApplication::restore(const SaveSnapshot& snapshot,std::string& error){
    try{
        auto candidate=prepareRestore(content_,snapshot,snapshot.continuation.kind=="chapter_handoff_after_file"?preparePresentation_:CombatScene::PreparePresentation{},random_);
        candidate.scene->configurePresentation(preparePresentation_);
        // Runtime adapter must stage independent resources; it publishes them only after generation changes.
        if(prepareRestorePresentation_){prepareRestorePresentation_(candidate.scene->definition(),candidate.presentation);}
        else if(preparePresentation_){throw std::runtime_error("Restore resource preparation is not configured");}
        saving_.reset();reading_.reset();actionAfterReading_.reset();menu_.reset();scene_.reset();session_=std::move(candidate.session);scene_=std::move(candidate.scene);presentationState_=candidate.presentation;
        mode_=ApplicationMode::Playing;pendingPresentationReset_=false;presentation_.clear();resetAlarm();alarmPresentation_.clear();if(snapshot.continuation.kind!="direct_unscored"){presentation_.push_back({PresentationEvent::Kind::SystemGraphic,content_.catalog.at("save_flow").at("restore_system"),{},{},0});}error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
void GameApplication::update(const InputFrame& input){
    if(mode_==ApplicationMode::Loading){
        loading_->update(input);if(auto sound=loading_->takeSound();!sound.empty()){playUiSound(sound);}
        if(auto result=loading_->takeResult()){
            if(result->kind==FileMenuResult::Kind::Cancelled){loading_.reset();mode_=ApplicationMode::Title;titleOpeningTicks_=43;}
            else if(result->kind==FileMenuResult::Kind::Loaded){std::string error;if(restore(*result->snapshot,error)){loading_.reset();}else{loading_->showError("读取失败："+error);}}
        }return;
    }

    if(mode_==ApplicationMode::Saving){
        saving_->update(input);if(saving_->finished()){auto events=saving_->takePresentation();presentation_.insert(presentation_.end(),events.begin(),events.end());saving_.reset();mode_=ApplicationMode::Playing;}return;
    }
    if(mode_==ApplicationMode::Reading){
        if(reading_->belongsTo(scene_->generation())){updateAlarm();scene_->updateReadingEffects();}
        reading_->update(input,scene_->generation());
        if(reading_->finished()){const auto restore=reading_->restore();if(restore){scene_->restoreMessageDisplay(restore->scene,restore->display);}
            auto events=reading_->takePresentation();presentation_.insert(presentation_.end(),events.begin(),events.end());reading_.reset();mode_=ApplicationMode::Playing;
            auto pending=std::exchange(actionAfterReading_,{});
            if(!pending||pending->kind!=MenuResult::Kind::Read){menu_.reset();}
            if(restore&&pending){submitMenuAction(*pending);}else if(!restore){menu_.reset();}}
        else if(reading_->atMenu()){scene_->prepareAccessKeyMenu();menu_->refreshTools(scene_->requestToolUses());menu_->returnFromReading();mode_=ApplicationMode::Equipment;}
        return;
    }
    if(mode_==ApplicationMode::Equipment){updateMenu(input);return;}
    if(mode_==ApplicationMode::Title){
        titleOpeningTicks_=std::min(titleOpeningTicks_+1,43);
        if(titleOpeningTicks_<43){return;}
        if(pressed(input,InputAction::Up)||pressed(input,InputAction::Down)){menuSelection_=(menuSelection_+(pressed(input,InputAction::Up)?2:1))%3;playUiSound("cursor");}
        if(pressed(input,InputAction::Confirm)){if(menuSelection_==0&&content_.catalog.contains("opening")){std::string error;if(!newGame(error)){presentation_.push_back({PresentationEvent::Kind::Notice,"无法开始新游戏："+error,{},{},240});}}else if(menuSelection_==2){quitRequested_=true;playUiSound("decision");}else if(menuSelection_==1&&titleLoadEnabled_){loading_=std::make_unique<FileMenu>(*saveFiles_);mode_=ApplicationMode::Loading;playUiSound("decision");}else{playUiSound("buzzer");}}
        return;
    }
    if(mode_==ApplicationMode::Boundary){
        if(scene_->exitRequest()&&scene_->exitRequest()->completed){
            if(pressed(input,InputAction::Cancel)){returnToTitle();}else if(pressed(input,InputAction::Confirm)){scene_->dismissBoundary();mode_=ApplicationMode::Playing;}
        }else if(pressed(input,InputAction::Cancel)){scene_->dismissBoundary();mode_=ApplicationMode::Playing;}return;
    }
    if(scene_->openingActive()||scene_->worldSupported()){updateAlarm();}
    try{scene_->update(input);}catch(const std::exception& e){if(!scene_->openingActive()){throw;}failOpening(e.what());return;}
    if(const auto request=postClearRequest();request&&request->selection==0&&content_.catalog.contains("cycle")){
        if(attemptedCycleScene_!=request->scene||pressed(input,InputAction::Confirm)){
            attemptedCycleScene_=request->scene;std::string error;
            if(!startNewCycle(*request,error)){presentation_.push_back({PresentationEvent::Kind::Notice,"无法开始新周回（确认重试）："+error,{},{},240});}
        }
        return;
    }
    if(const auto request=postClearRequest();request&&request->selection==1&&content_.catalog.contains("bonus")){
        if(attemptedCycleScene_!=request->scene||pressed(input,InputAction::Confirm)){
            attemptedCycleScene_=request->scene;std::string error;
            if(!enterBonusRoom(*request,error)){presentation_.push_back({PresentationEvent::Kind::Notice,"无法进入特典房间（确认重试）："+error,{},{},240});}
        }
        return;
    }
    if(saveFiles_){if(const auto request=scene_->takeSaveRequest()){
        try{saving_=std::make_unique<SaveController>(content_,*session_,*scene_,*saveFiles_,*request,presentationState_);mode_=ApplicationMode::Saving;}
        catch(const std::exception& e){presentation_.push_back({PresentationEvent::Kind::Notice,std::string("无法准备保存：")+e.what(),{},{},240});}return;
    }}
    if(const auto request=scene_->takeApplicationRequest()){
        mode_=*request;if(mode_==ApplicationMode::Title){returnToTitle();return;}
        if(mode_==ApplicationMode::Equipment&&!content_.catalog.empty()){scene_->prepareAccessKeyMenu();menu_=std::make_unique<MenuController>(content_,*session_,scene_->requestToolUses(),scene_->requestMixing());}
        menuSelection_=mode_==ApplicationMode::Title?2:0;menuSlot_=1;
    }
}

void GameApplication::updateAlarm(){
    if(!session_||!content_.catalog.contains("g6")||!content_.catalog.at("g6").contains("alarm")){return;}
    const auto& alarm=content_.catalog.at("g6").at("alarm");
    if(!session_->flag(alarm.at("flag"))){resetAlarm();return;}
    if(alarmWait_>0&&--alarmWait_>0){return;}
    const auto& steps=alarm.at("steps");
    // One same-session cursor survives ordinary world swaps and pauses with map logic.
    while(alarmNext_<steps.size()){
        const auto& step=steps.at(alarmNext_++);
        if(step.contains("sound")){alarmPresentation_.push_back({PresentationEvent::Kind::Sound,"ce142",readSound(step.at("sound")),{},0});}
        else{alarmWait_=step.at("wait_ticks");break;}
    }
    if(alarmNext_==steps.size()){alarmNext_=0;}
}
