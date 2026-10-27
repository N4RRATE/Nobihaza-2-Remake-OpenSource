#include "combat_scene.h"
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* reason){if(!ok){throw std::runtime_error(reason);}}
int main(int argc,char** argv){try{
    if(argc!=2){return 2;}auto content=loadCombatContent(argv[1]);
    // Domain fixture uses another map/object to exercise the public finite flow.
    // Original M52 E18P1 C2 sets60; C3 starts it separately.
    const std::string id="m6-e4";content.objects.at(id).variants.front().flags[156]=false;
    content.catalog["flows"]["timer-domain-fixture"]={{"object",id},{"page",1},{"trigger","autorun"},
        {"steps",nlohmann::json::array({{{"kind","timer_set"},{"timer_source","training"},{"seconds",60}},
            {{"kind","cue"},{"cue","timer-domain-yield"}}})}};
    content.setMediaDefinition("cues","timer-domain-yield",{{"kind","wait"},{"tenths",0}});
    auto entry=content.entries.at("defeated-claimed");entry.map=6;entry.playerTile={10,8};
    entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();entry.session.flags[156]=false;
    const auto& original=content.objects.at(id).variants.front().definition;
    const auto facing=original.direction=="up"?Direction::Down:Direction::Up;
    entry.directions[id]=facing;entry.through[id]=!original.through;
    GameSession orienting(content.items,entry.session,content.facts);CombatScene oriented(content,orienting,entry);
    const auto& object=findCharacter(oriented.world(),id);
    require(object.motion.tile()==content.objects.at(id).tile,"Independent entry overrides preserve the original object position");
    require(object.motion.facing()==facing,"Explicit direction does not require a duplicate position override");
    require(object.through!=original.through,"Explicit collision override does not require a duplicate position override");
    entry.directions.clear();entry.through.clear();
    GameSession session(content.items,entry.session,content.facts);CombatScene scene(content,session,entry);scene.update({});
    require(session.eventTimer()==EventTimer{EventTimerSource::Training,3659,false,false,false},"Set60 stores compatible seconds without starting it");
    require(scene.foreground()==ForegroundKind::Content,"Wait0 yields while the configured timer stays stopped");
    scene.deactivate();scene.update({});require(session.eventTimer().remainingTicks==3659,"Discarded flow cannot run its remaining timer operations");
    entry.session.eventTimer={EventTimerSource::Training,1234,true,true,false};
    GameSession active(content.items,entry.session,content.facts);CombatScene reset(content,active,entry);reset.update({});
    require(active.eventTimer()==EventTimer{EventTimerSource::Training,3659,true,true,false},"SetTimer replaces time without implicitly stopping an already active original clock");
    content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
        {{"kind","timer_start"},{"visible",false},{"battle",false}},{{"kind","flag"},{"id",156},{"value",true}}});
    entry.session.eventTimer={EventTimerSource::Training,1234,false,false,false};
    GameSession resuming(content.items,entry.session,content.facts);CombatScene resume(content,resuming,entry);resume.update({});
    require(resuming.eventTimer()==EventTimer{EventTimerSource::Training,1234,true,false,false},"Start uses stored residue rather than resetting60seconds");
    resume.update({});require(resuming.eventTimer().remainingTicks==1233,"Started timer advances exactly once on the following scene update");
    content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
        {{"kind","timer_stop"}},{{"kind","flag"},{"id",156},{"value",true}}});
    entry.session.eventTimer={EventTimerSource::Training,1234,true,true,false};
    GameSession stopping(content.items,entry.session,content.facts);CombatScene stop(content,stopping,entry);stop.update({});
    require(stopping.eventTimer()==EventTimer{EventTimerSource::Training,1233,false,false,false},"Original explicit stop keeps residue while hiding and retiring the clock");
    stop.update({});require(stopping.eventTimer().remainingTicks==1233,"Stopped clock no longer advances");
    content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
        {{"kind","toggle_flag"},{"id",160}},{{"kind","flag"},{"id",156},{"value",true}}});
    for(bool before:{false,true}){
        entry.session.flags[160]=before;entry.session.eventTimer={};
        GameSession toggling(content.items,entry.session,content.facts);CombatScene toggle(content,toggling,entry);toggle.update({});
        require(toggling.flag(160)!=before,"Original switch operation2 inverts both possible door states");
    }
    content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
        {{"kind","erase_event_once"},{"object","m6-e1"}},{{"kind","cue"},{"cue","timer-domain-yield"}},{{"kind","flag"},{"id",156},{"value",true}}});
    entry.session.flags[156]=false;entry.session.eventTimer={};
    GameSession erasing(content.items,entry.session,content.facts);CombatScene erased(content,erasing,entry);erased.update({});
    require(!findCharacter(erased.world(),"m6-e1").active,"A called event erase applies to its explicit target");
    require(findCharacter(erased.world(),id).active,"Erasing a called event preserves the calling event");
    for(const char* invalid:{"m4-e1","player"}){
        content.catalog["flows"]["timer-domain-fixture"]["steps"][0]["object"]=invalid;
        GameSession rejecting(content.items,entry.session,content.facts);CombatScene invalidTarget(content,rejecting,entry);
        bool rejected=false;try{invalidTarget.update({});}catch(const std::runtime_error&){rejected=true;}
        require(rejected&&!rejecting.flag(156),"Only current scene events can be erased before following state commits");
        require(findCharacter(invalidTarget.world(),id).active,"Invalid erase cannot hide the caller");
    }
    content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
        {{"kind","erase_event_once"}},{{"kind","flag"},{"id",156},{"value",true}}});
    GameSession legacyErase(content.items,entry.session,content.facts);CombatScene legacyTarget(content,legacyErase,entry);legacyTarget.update({});
    require(!findCharacter(legacyTarget.world(),id).active,"Existing erase without an explicit target still erases self");
    require(findCharacter(legacyTarget.world(),"m6-e1").active,"A recreated scene does not inherit another scene's erased target");
    if(content.full->at("runtime").contains("player_action_cleanup")){
        const auto& cleanup=content.full->at("runtime").at("player_action_cleanup").at("ce5");
        content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
            {{"kind","cancel_player_actions"},{"cleanup","ce5"}},{{"kind","flag"},{"id",156},{"value",true}}});
        for(bool injured:{false,true}){
            entry.session.flags[10]=injured;entry.session.flags[156]=false;
            for(int flag:{2,3,4,5,17,19}){entry.session.flags[flag]=true;}
            for(int variable:{10,11,12,29}){entry.session.variables[variable]=9;}
            entry.session.quickshotTicks=450;entry.session.eventTimer={EventTimerSource::Training,1234,true,false,false};
            GameSession resetting(content.items,entry.session,content.facts);CombatScene restored(content,resetting,entry);restored.update({});
            for(int variable:{10,11,12}){require(resetting.variable(variable)==0,"CE5 clears original temporary action values");}
            for(int flag:{2,3,4,5,17,19}){require(!resetting.flag(flag),"CE5 retires action flags without CE55 room reset");}
            require(resetting.quickshotTicks()==449&&resetting.eventTimer().remainingTicks==1233&&resetting.eventTimer().running,"Action cleanup preserves quickshot and a live training clock");
            const auto& branch=cleanup.at(injured?"injured":"normal");
            require(restored.world().player().spriteId==content.cue(branch.at("graphic_cue").get<std::string>()).at("sprite"),"CE5 selects the actual normal or injured Actor1 graphic");
            require(resetting.variable(29)==(injured?9:0),"Only the original normal branch clears weapon scratch29");
        }
    }
    // Complete D keeps source timer operations distinct; this variation uses M6.
    for(const std::string operation:{"set","start","stop"}){
        content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
            {{"kind","timer"},{"operation",operation},{"seconds",60},{"show",false},{"source_id","training"}},
            {{"kind","flag"},{"id",156},{"value",true}}});
        entry.session.flags[156]=false;entry.session.eventTimer={EventTimerSource::Training,1234,operation!="start",operation!="start",false};
        GameSession sourceTimer(content.items,entry.session,content.facts);CombatScene sourceFlow(content,sourceTimer,entry);sourceFlow.update({});
        const EventTimer expected=operation=="set"?EventTimer{EventTimerSource::Training,3659,true,true,false}:
            operation=="start"?EventTimer{EventTimerSource::Training,1234,true,false,false}:EventTimer{EventTimerSource::Training,1233,false,false,false};
        require(sourceTimer.eventTimer()==expected&&sourceTimer.flag(156),"Reviewed source timer set/start/stop reaches the same unique session clock");
    }
    for(const char* invalid:{"operation","source_id"}){
        content.catalog["flows"]["timer-domain-fixture"]["steps"][0][invalid]="unknown";
        entry.session.flags[156]=false;entry.session.eventTimer={EventTimerSource::Training,1234,false,false,false};
        GameSession rejectedTimer(content.items,entry.session,content.facts);CombatScene rejectedFlow(content,rejectedTimer,entry);
        bool rejected=false;try{rejectedFlow.update({});}catch(const std::exception&){rejected=true;}
        require(rejected&&!rejectedTimer.flag(156)&&rejectedTimer.eventTimer().remainingTicks==1234,"Unknown timer operation or source rejects without a following state commit");
        content.catalog["flows"]["timer-domain-fixture"]["steps"][0][invalid]=std::string(invalid)=="operation"?"stop":"training";
    }
    for(const char* kind:{"fact","item","actor"}){for(bool present:{false,true}){
        nlohmann::json condition{{"kind",kind},{"id",std::string(kind)=="fact"?160:std::string(kind)=="item"?73:1}};
        if(std::string(kind)=="fact"){condition["value"]=true;}
        if(std::string(kind)=="item"){condition["present"]=true;}
        content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
            {{"kind","branch"},{"condition",condition},
                {"then",nlohmann::json::array({{{"kind","flag"},{"id",161},{"value",true}}})},
                {"else",nlohmann::json::array({{{"kind","flag"},{"id",162},{"value",true}}})}},
            {{"kind","flag"},{"id",156},{"value",true}}});
        entry.session.flags[156]=false;entry.session.flags[160]=present;entry.session.flags[161]=false;entry.session.flags[162]=false;
        entry.session.eventTimer={};entry.session.inventory.erase(73);if(present){entry.session.inventory[73]=1;}
        entry.session.party=present?std::vector<int>{1}:std::vector<int>{3};
        GameSession branching(content.items,entry.session,content.facts);CombatScene branchFlow(content,branching,entry);branchFlow.update({});
        require(branching.flag(156)&&branching.flag(161)==present&&branching.flag(162)!=present,"Source fact/item/party conditions execute exactly one finite branch");
    }}
    content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
        {{"kind","branch"},{"condition",{{"kind","item"},{"id",11},{"present",true}}},
            {"then",nlohmann::json::array({{{"kind","flag"},{"id",161},{"value",true}}})},{"else",nlohmann::json::array()}},
        {{"kind","flag"},{"id",156},{"value",true}}});
    entry.session.flags[156]=false;entry.session.flags[161]=false;entry.session.inventory.erase(11);entry.session.mainWeapon=11;
    GameSession equippedBranch(content.items,entry.session,content.facts);CombatScene equippedFlow(content,equippedBranch,entry);equippedFlow.update({});
    require(equippedBranch.count(11)==0&&equippedBranch.flag(161),"Original item possession includes equipped weapons outside the inventory list");
    content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
        {{"kind","flag"},{"id",161},{"value",true}},
        {{"kind","branch"},{"condition",{{"kind","unknown"},{"id",1}}},{"then",nlohmann::json::array()},{"else",nlohmann::json::array()}}});
    entry.session.flags[156]=false;entry.session.flags[161]=false;
    GameSession invalidBranch(content.items,entry.session,content.facts);CombatScene invalidBranchFlow(content,invalidBranch,entry);
    bool invalidCondition=false;try{invalidBranchFlow.update({});}catch(const std::exception&){invalidCondition=true;}
    require(invalidCondition&&!invalidBranch.flag(161),"Unknown branch predicates are rejected before earlier actions commit");
    auto& skipped=content.catalog["flows"]["timer-domain-fixture"]["steps"][1];
    skipped["condition"]={{"kind","fact"},{"id",160},{"value",true}};
    skipped["then"]=nlohmann::json::array({{{"kind","message"},{"message","missing-source-message"}}});entry.session.flags[160]=false;
    GameSession invalidSkipped(content.items,entry.session,content.facts);CombatScene invalidSkippedFlow(content,invalidSkipped,entry);
    invalidCondition=false;try{invalidSkippedFlow.update({});}catch(const std::exception&){invalidCondition=true;}
    require(invalidCondition&&!invalidSkipped.flag(161),"A skipped branch still validates its presentation resources before state changes");
    entry.session.party={1};
    const auto key=[](InputAction action){InputFrame input;input.buttons[static_cast<int>(action)].pressed=true;return input;};
    for(int mode=0;mode<4;++mode){
        content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
            {{"kind","choice"},{"cancel_option",mode==3?0:2},{"options",nlohmann::json::array({
                {{"label","是"},{"steps",nlohmann::json::array({{{"kind","grant"},{"item",31},{"count",2}}})}},
                {{"label","否"},{"steps",nlohmann::json::array({{{"kind","flag"},{"id",162},{"value",true}}})}}})}},
            {{"kind","flag"},{"id",156},{"value",true}}});
        entry.session.flags[156]=false;entry.session.flags[162]=false;entry.session.inventory[31]=0;
        GameSession choosing(content.items,entry.session,content.facts);CombatScene choiceFlow(content,choosing,entry);choiceFlow.update({});
        require(choiceFlow.inputPromptView().phase!=DialoguePhase::Hidden&&!choosing.flag(156),"Source inline options wait for a typed request result before any branch commit");
        if(mode==1){choiceFlow.update(key(InputAction::Down));}
        if(mode==3){choiceFlow.update(key(InputAction::Cancel));require(!choosing.flag(156)&&choiceFlow.inputPromptView().phase!=DialoguePhase::Hidden,"Cancel0 keeps the original choice active");}
        choiceFlow.update(key(mode==2?InputAction::Cancel:InputAction::Confirm));choiceFlow.update({});
        const bool accepted=mode==0||mode==3;
        require(choosing.flag(156)&&choosing.count(31)==(accepted?2:0)&&choosing.flag(162)!=accepted,"Yes/no and cancel2 execute the corresponding finite branch exactly once");
        choiceFlow.update(key(InputAction::Confirm));require(choosing.count(31)==(accepted?2:0),"Repeated confirm cannot commit the source choice reward again");
    }
    entry.session.flags[156]=false;entry.session.inventory[31]=0;
    GameSession abandonedChoice(content.items,entry.session,content.facts);CombatScene abandonedFlow(content,abandonedChoice,entry);abandonedFlow.update({});abandonedFlow.deactivate();abandonedFlow.update(key(InputAction::Confirm));
    require(!abandonedChoice.flag(156)&&abandonedChoice.count(31)==0,"Retired scene cannot commit its old source choice result");
    for(int item:{72,74}){
        content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
            {{"kind","selected_key"},{"item",item}},
            {{"kind","choice"},{"cancel_option",0},{"options",nlohmann::json::array({
                {{"label","是"},{"steps",nlohmann::json::array({{{"kind","remove_selected_key"},{"key_slot",44},{"count",1}}})}},
                {{"label","否"},{"steps",nlohmann::json::array()}}})}},
            {{"kind","flag"},{"id",156},{"value",true}}});
        for(bool discard:{false,true}){
            entry.session.flags[156]=false;entry.session.inventory[72]=2;entry.session.inventory[74]=2;
            GameSession selectedKey(content.items,entry.session,content.facts);CombatScene keyFlow(content,selectedKey,entry);keyFlow.update({});
            require(selectedKey.variable(44)==item&&selectedKey.count(item)==2&&!selectedKey.flag(156),"Selecting a key stores its source slot without consuming it before the choice");
            if(!discard){keyFlow.update(key(InputAction::Down));}
            keyFlow.update(key(InputAction::Confirm));keyFlow.update({});
            require(selectedKey.flag(156)&&selectedKey.count(item)==(discard?1:2)&&selectedKey.count(item==72?74:72)==2,"Discarding uses the selected key ID and removes exactly one; keeping does not consume either key");
            keyFlow.update(key(InputAction::Confirm));require(selectedKey.count(item)==(discard?1:2),"A repeated confirm does not discard another key");
        }
    }
    content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
        {{"kind","flag"},{"id",161},{"value",true}},{{"kind","selected_key"},{"item",999999}}});
    entry.session.flags[156]=false;entry.session.flags[161]=false;
    GameSession unknownKey(content.items,entry.session,content.facts);CombatScene unknownKeyFlow(content,unknownKey,entry);
    bool invalidKey=false;try{unknownKeyFlow.update({});}catch(const std::exception&){invalidKey=true;}
    require(invalidKey&&!unknownKey.flag(161),"An unknown key definition fails preflight before an earlier state commit");
    const auto originalKeyFlow=content.catalog["flows"].value("ce100",nlohmann::json{});
    content.catalog["flows"]["ce100"]={{"steps",nlohmann::json::array({
        {{"kind","choice"},{"cancel_option",0},{"options",nlohmann::json::array({
            {{"label","是"},{"steps",nlohmann::json::array({{{"kind","remove_selected_key"},{"key_slot",44},{"count",1}}})}},
            {{"label","否"},{"steps",nlohmann::json::array()}}})}}})}};
    content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
        {{"kind","selected_key"},{"item",72}},{{"kind","discard_key_choice"},{"key_slot",44},{"flow","ce100"}},
        {{"kind","flag"},{"id",156},{"value",true}}});
    for(int count:{0,2}){
        entry.session.flags[156]=false;entry.session.inventory[72]=count;
        GameSession callingKey(content.items,entry.session,content.facts);CombatScene callingKeyFlow(content,callingKey,entry);callingKeyFlow.update({});
        require(!callingKey.flag(156)&&callingKeyFlow.inputPromptView().phase!=DialoguePhase::Hidden,"A call-only key flow inherits the caller lifecycle and waits for its choice");
        callingKeyFlow.update(key(InputAction::Confirm));callingKeyFlow.update({});
        require(callingKey.flag(156)&&callingKey.count(72)==(count?1:0),"The finite key call removes one or clamps an empty inventory without repeating the caller");
    }
    for(const char* bad:{"unknown","disabled","recursive"}){
        auto& call=content.catalog["flows"]["timer-domain-fixture"]["steps"][1];call["flow"]=std::string(bad)=="unknown"?"unknown":"ce100";
        content.catalog["flows"]["ce100"]["runtime_enabled"]=std::string(bad)!="disabled";
        if(std::string(bad)=="recursive"){content.catalog["flows"]["ce100"]["steps"][0]["options"][0]["steps"]=nlohmann::json::array({{{"kind","discard_key_choice"},{"key_slot",44},{"flow","ce100"}}});}
        entry.session.variables[44]=74;entry.session.flags[156]=false;
        GameSession invalidCall(content.items,entry.session,content.facts);CombatScene invalidCallFlow(content,invalidCall,entry);
        bool rejected=false;try{invalidCallFlow.update({});}catch(const std::exception&){rejected=true;}
        require(rejected&&invalidCall.variable(44)==74&&!invalidCall.flag(156),"Unknown, disabled or recursive key calls reject before changing the selected key");
    }
    content.catalog["flows"]["nested-erase-domain"]={{"object","m6-e1"},{"page",1},{"runtime_enabled",true},
        {"steps",nlohmann::json::array({{{"kind","erase_event_once"}}})}};
    content.catalog["flows"]["timer-domain-fixture"]["steps"][1]["flow"]="ce100";
    content.catalog["flows"]["ce100"]["runtime_enabled"]=true;
    content.catalog["flows"]["ce100"]["steps"][0]["options"][0]["steps"]=nlohmann::json::array({{{"kind","invoke_flow"},{"flow","nested-erase-domain"}}});
    entry.session.variables[44]=74;entry.session.flags[156]=false;
    GameSession nestedCall(content.items,entry.session,content.facts);CombatScene nestedScene(content,nestedCall,entry);
    bool nestedRejected=false;try{nestedScene.update({});}catch(const std::exception&){nestedRejected=true;}
    require(nestedRejected&&nestedCall.variable(44)==74&&!nestedCall.flag(156)&&findCharacter(nestedScene.world(),"m6-e1").active,"A finite public callee cannot invoke another event, even from an unselected choice branch, before the caller commits its key slot");
    content.catalog["flows"].erase("nested-erase-domain");
    if(!originalKeyFlow.is_null()){
        content.catalog["flows"]["ce100"]=originalKeyFlow;content.catalog["flows"]["ce100"]["runtime_enabled"]=true;
        content.catalog["flows"]["timer-domain-fixture"]["steps"][1]["flow"]="ce100";
        for(bool discard:{false,true}){
            entry.session.flags[156]=false;entry.session.inventory[72]=1;
            GameSession sourceKey(content.items,entry.session,content.facts);CombatScene sourceKeyFlow(content,sourceKey,entry);sourceKeyFlow.update({});
            require(sourceKeyFlow.dialogue().view().phase!=DialoguePhase::Hidden&&sourceKey.count(72)==1&&!sourceKey.flag(156),"The real CE100 source message precedes its choice and any key removal");
            for(int n=0;n<1000&&sourceKeyFlow.inputPromptView().phase==DialoguePhase::Hidden;++n){sourceKeyFlow.update({});}
            const auto& prompt=sourceKeyFlow.inputPromptView();
            require(prompt.phase==DialoguePhase::AwaitingInput&&prompt.lines.size()==4&&prompt.lines.at(1).find(U"这把钥匙")!=std::u32string::npos,"CE100 preserves the complete question, blank line and two choice labels in the same message window");
            if(!discard){sourceKeyFlow.update(key(InputAction::Down));}sourceKeyFlow.update(key(InputAction::Confirm));sourceKeyFlow.update({});
            require(sourceKey.flag(156)&&sourceKey.count(72)==(discard?0:1),"Both real CE100 source branches return to the caller without premature state changes");
            const auto events=sourceKeyFlow.takePresentation();int systemChanges=0;
            for(const auto& event:events){if(event.kind==PresentationEvent::Kind::SystemGraphic){++systemChanges;}}
            require(systemChanges==2,"The source key question switches its system graphic and restores the menu graphic on return");
        }
        std::cout<<"Real CE100 question, blank line, both choices and system restoration passed\n";
    }
    content.catalog["flows"].erase("ce100");
    content.catalog["flows"]["called-erase-domain"]={{"object","m6-e1"},{"page",1},{"trigger","action"},
        {"steps",nlohmann::json::array({{{"kind","erase_event_once"}}})}};
    content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
        {{"kind","invoke_flow"},{"flow","called-erase-domain"}},{{"kind","cue"},{"cue","timer-domain-yield"}},{{"kind","flag"},{"id",156},{"value",true}}});
    entry.session.flags[156]=false;
    GameSession calledErase(content.items,entry.session,content.facts);CombatScene calledEraseFlow(content,calledErase,entry);calledEraseFlow.update({});
    require(!calledErase.flag(156)&&!findCharacter(calledEraseFlow.world(),"m6-e1").active&&findCharacter(calledEraseFlow.world(),id).active,"A called erase uses the callee identity rather than erasing the calling event");
    calledEraseFlow.update({});require(calledErase.flag(156),"A called erase returns to the caller before its following state commit");
    for(int invalid=0;invalid<3;++invalid){
        auto& called=content.catalog["flows"]["called-erase-domain"];called["object"]=invalid==0?"m4-e1":"m6-e1";called["runtime_enabled"]=invalid!=1;
        if(invalid==2){called["steps"]=nlohmann::json::array({{{"kind","invoke_flow"},{"flow","called-erase-domain"}}});}
        entry.session.flags[156]=false;
        GameSession invalidEraseCall(content.items,entry.session,content.facts);CombatScene invalidEraseFlow(content,invalidEraseCall,entry);
        bool rejected=false;try{invalidEraseFlow.update({});}catch(const std::exception&){rejected=true;}
        require(rejected&&!invalidEraseCall.flag(156)&&findCharacter(invalidEraseFlow.world(),id).active,"Wrong-map, disabled or unsupported callee rejects without hiding the caller or committing its result");
    }
    content.catalog["flows"].erase("called-erase-domain");
    content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
        {{"kind","presentation_group"},{"cues",nlohmann::json::array({"timer-domain-yield","timer-domain-yield"})}},
        {{"kind","flag"},{"id",156},{"value",true}}});
    entry.session.flags[156]=false;
    GameSession grouped(content.items,entry.session,content.facts);CombatScene groupedFlow(content,grouped,entry);groupedFlow.update({});
    require(!grouped.flag(156)&&groupedFlow.foreground()==ForegroundKind::Content,"A presentation group waits at its first cue instead of running through its final state commit");
    groupedFlow.update({});require(!grouped.flag(156),"The second cue has an independent Wait0 barrier");
    groupedFlow.update({});require(grouped.flag(156),"The group continues only after both finite cue waits complete");
    content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
        {{"kind","flag"},{"id",161},{"value",true}},
        {{"kind","presentation_group"},{"cues",nlohmann::json::array({"timer-domain-yield","missing-group-cue"})}}});
    entry.session.flags[156]=false;entry.session.flags[161]=false;
    GameSession missingGroupCue(content.items,entry.session,content.facts);CombatScene missingGroupFlow(content,missingGroupCue,entry);
    bool missing=false;try{missingGroupFlow.update({});}catch(const std::exception&){missing=true;}
    require(missing&&!missingGroupCue.flag(161),"Every group cue is validated before any earlier state commit");
    content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
        {{"kind","flag"},{"id",161},{"value",true}},{{"kind","finish"}},
        {{"kind","grant"},{"item",31},{"count",1}},{{"kind","flag"},{"id",156},{"value",true}}});
    entry.session.flags[156]=false;entry.session.flags[161]=false;entry.session.inventory[31]=0;
    GameSession finishing(content.items,entry.session,content.facts);CombatScene finishedFlow(content,finishing,entry);finishedFlow.update({});
    require(finishing.flag(161)&&!finishing.flag(156)&&finishing.count(31)==0&&finishedFlow.foreground()==ForegroundKind::None,"EndEvent ends the entire finite flow while preserving its already committed prefix");
    RouteDefinition pendingActor;pendingActor.target=1;pendingActor.frequency=8;pendingActor.steps={{"wait"},{"switch_on","",{},160}};
    content.routes["room-reset-domain-route"]=pendingActor;content.setMediaDefinition("cues","room-reset-domain-route",{{"kind","forced_route"}});
    content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
        {{"kind","cue"},{"cue","room-reset-domain-route"}},{{"kind","room_reset"}},{{"kind","flag"},{"id",156},{"value",true}}});
    entry.session.flags[156]=false;entry.session.flags[160]=false;entry.session.quickshotTicks=450;entry.session.variables[44]=72;
    entry.session.eventTimer={EventTimerSource::Training,1234,true,false,false};
    GameSession roomResetting(content.items,entry.session,content.facts);roomResetting.setVariable(301,23);roomResetting.setVariable(93,111);roomResetting.setVariable(94,222);roomResetting.setBurnPictureStage(24,BurnPictureStage::MoveSecond);
    CombatScene roomResetFlow(content,roomResetting,entry);roomResetFlow.update({});
    require(roomResetting.quickshotTicks()==449&&!roomResetting.flag(48)&&roomResetting.variable(44)==0,"The source CE55 clears its temporary action/key state before returning to the finite caller");
    if(content.full->at("runtime").contains("player_action_cleanup")){require(!roomResetting.flag(156)&&roomResetFlow.foreground()==ForegroundKind::Content,"Source CE55 C34 waits for CE5's player restore route before the caller's next state commit");}
    require(roomResetting.eventTimer()==EventTimer{EventTimerSource::Training,1233,true,false,false}&&roomResetting.variable(301)==23,"CE55 preserves the active training timer and source burn lifetime");
    require(roomResetting.variable(93)==0&&roomResetting.variable(94)==0&&roomResetting.burnPictureStage(24)==BurnPictureStage::MoveSecond,"CE55 clears picture work but preserves the suspended common command");
    for(int tick=0;tick<32;++tick){roomResetFlow.update({});}
    require(roomResetting.flag(156)&&!roomResetting.flag(160),"CE55 cancels another actor's pending forced route, then returns after the player cleanup without later committing the cancelled actor's result");
    if(content.full->at("runtime").contains("player_action_cleanup")){
        const auto graphic=content.full->at("runtime").at("player_action_cleanup").at("ce5").at("normal").at("graphic_cue").get<std::string>();
        const auto savedCue=content.cue(graphic);content.cues.erase(graphic);
        content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
            {{"kind","flag"},{"id",161},{"value",true}},{{"kind","room_reset"}}});entry.session.flags[161]=false;
        GameSession missingCleanup(content.items,entry.session,content.facts);CombatScene invalidCleanup(content,missingCleanup,entry);
        bool rejected=false;try{invalidCleanup.update({});}catch(const std::exception&){rejected=true;}
        require(rejected&&!missingCleanup.flag(161),"Every CE55 player-cleanup branch is validated before an earlier state commit");
        content.setMediaDefinition("cues",graphic,savedCue);
    }
    content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
        {{"kind","relocate"},{"destination",{{"map",4},{"tile",{8,5}}}}},
        {{"kind","cue"},{"cue","timer-domain-yield"}},{{"kind","flag"},{"id",156},{"value",true}}});
    entry.session.flags[156]=false;entry.direction=Direction::Left;
    GameSession movedSession(content.items,entry.session,content.facts);CombatScene movedScene(content,movedSession,entry);
    const auto oldIdentity=movedScene.generation();movedScene.update({});
    require(movedScene.mapId()==4&&movedScene.world().player().motion.tile()==TilePosition{8,5}&&movedScene.world().player().motion.facing()==Direction::Left,"A finite original relocation commits its prepared destination while preserving direction");
    require(movedScene.generation()!=oldIdentity&&!movedSession.flag(156)&&movedScene.foreground()==ForegroundKind::Content,"The destination continues only the explicit finite caller after invalidating old scene results");
    movedScene.update({});require(movedSession.flag(156)&&movedScene.foreground()==ForegroundKind::None,"The caller resumes after its destination wait and commits once");
    if(content.full->at("runtime").contains("player_action_cleanup")){
        content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
            {{"kind","room_reset"}},{{"kind","relocate"},{"destination",{{"map",4},{"tile",{8,5}}}}},
            {{"kind","flag"},{"id",156},{"value",true}}});
        for(bool injured:{false,true}){
            entry.session.flags[10]=injured;entry.session.flags[156]=false;
            GameSession actorSession(content.items,entry.session,content.facts);CombatScene relocating(content,actorSession,entry);
            for(int tick=0;tick<32;++tick){relocating.update({});}
            require(relocating.mapId()==4&&actorSession.flag(156),"The source player restore route finishes before its finite transfer continuation");
            require(relocating.world().player().spriteId==(injured?"grab-7":"player-0"),"CN1.00 CE5 C18/C30 actor graphic survives the following teleport instead of becoming the target map default");
        }
        entry.session.flags[10]=false;
        auto explicitEntry=entry;explicitEntry.map=4;explicitEntry.playerTile={8,5};explicitEntry.sprite="grab-7";
        GameSession explicitSession(content.items,explicitEntry.session,content.facts);CombatScene explicitScene(content,explicitSession,explicitEntry);
        require(explicitScene.world().player().spriteId=="grab-7","A restore or opening entry retains its explicit graphic rather than invoking ordinary CE5 selection");
        auto foreignProfile=content;auto foreignFull=*content.full;foreignFull["runtime"]["player_action_cleanup"]["ce5"]["actor"]=30;foreignProfile.full=std::make_shared<const nlohmann::json>(std::move(foreignFull));
        explicitSession.setFlag(10,true);
        const auto unmatched=foreignProfile.makeScene(4,{8,5},Direction::Left,explicitSession);
        require(unmatched.player.spriteId=="player-0","Another actor's cleanup profile cannot select a graphic for the current session actor");
    }
    for(int bad=0;bad<2;++bad){
        content.catalog["flows"]["timer-domain-fixture"]["steps"]=nlohmann::json::array({
            {{"kind","flag"},{"id",161},{"value",true}},
            {{"kind","relocate"},{"destination",{{"map",4},{"tile",{bad?8:999,5}}}}}});
        entry.session.flags[161]=false;GameSession failedSession(content.items,entry.session,content.facts);CombatScene failedScene(content,failedSession,entry);
        const auto identity=failedScene.generation();if(bad){failedScene.configurePresentation([](const SceneDefinition&){throw std::runtime_error("Unavailable candidate display");});}
        bool rejected=false;try{failedScene.update({});}catch(const std::exception&){rejected=true;}
        require(rejected&&!failedSession.flag(161)&&failedScene.mapId()==6&&failedScene.generation()==identity,"Invalid destination or unavailable display rejects before prior state changes and preserves the current scene");
    }
    content.catalog["flows"].erase("timer-domain-fixture");
    auto& portal=content.objects.at(id).variants.front();portal.flags.erase(156);portal.definition.trigger=1;
    content.transitions["toggle-domain-transfer"]={{"object",id},{"page",portal.page},
        {"destination",{{"map",4},{"tile",{8,5}}}},{"facing","preserve"},
        {"flow",nlohmann::json::array({{{"kind","room_reset"}},{{"kind","toggle_flag"},{"id",160}},
            {{"kind","relocate"}},{{"kind","flag"},{"id",1},{"value",true}},{{"kind","flag"},{"id",18},{"value",true}}})}};
    entry.playerTile=content.objects.at(id).tile;
    for(bool before:{false,true}){
        entry.session.flags[160]=before;entry.session.flags[156]=true;entry.session.flags[10]=false;
        GameSession traveling(content.items,entry.session,content.facts);CombatScene transfer(content,traveling,entry);
        for(int n=0;n<4;++n){transfer.update({});}
        require(transfer.mapId()==4&&traveling.flag(160)!=before,"The same original toggle also reaches the shared map-transfer caller");
    }
    if(content.full->at("runtime").at("flows").contains("m52-e9-p2")){
        auto real=loadCombatContent(argv[1]);
        const bool trainingRuntime=real.full->at("runtime").contains("status_definitions");
        if(trainingRuntime){require(!real.pendingMapTickets.contains(52)&&real.catalog.at("flows").at("m52-e9-p2").at("runtime_enabled").get<bool>(),"Complete package exposes the actual training runtime without fixture overrides");}
        else{real.pendingMapTickets.erase(52);real.catalog["flows"]["m52-e9-p2"]["runtime_enabled"]=true;}
        if(trainingRuntime){
            require(real.transitions.at("m20-e3-p4").at("runtime_enabled").get<bool>(),"The reviewed old-area exit reaches the now implemented training exterior");
            auto approach=real.entries.at("defeated-claimed");approach.map=51;approach.playerTile={25,18};approach.direction=Direction::Up;
            approach.pages.clear();approach.positions.clear();approach.directions.clear();approach.through.clear();
            GameSession entering(real.items,approach.session,real.facts);CombatScene entrance(real,entering,approach);
            InputFrame up;up.movement=Direction::Up;up.buttons[static_cast<int>(InputAction::Up)].held=true;
            for(int n=0;n<600&&entrance.mapId()==51;++n){entrance.update(up);}
            require(entrance.mapId()==52&&!entrance.exitRequest(),"Walking through the real exterior entrance loads training instead of a development boundary");
            for(int n=0;n<1200&&entrance.foreground()!=ForegroundKind::None;++n){entrance.update({});}
            require(entrance.world().player().motion.tile()==TilePosition{2,15},"Original first training entrance keeps its own source destination");
        }
        for(bool accept:{false,true}){
            auto start=real.entries.at("defeated-claimed");start.map=52;start.playerTile={9,14};start.direction=Direction::Up;
            start.pages.clear();start.positions.clear();start.directions.clear();start.through.clear();start.session.flags[150]=false;
            GameSession choosingSwitch(real.items,start.session,real.facts);CombatScene switchScene(real,choosingSwitch,start);
            switchScene.update(key(InputAction::Confirm));bool answered=false;
            for(int ticks=0;ticks<2000&&(switchScene.foreground()!=ForegroundKind::None||!answered);++ticks){
                if(switchScene.inputPromptView().phase==DialoguePhase::AwaitingInput){
                    require(!answered&&!choosingSwitch.flag(150),"Source switch waits for a single choice before activating its permanent fact");
                    if(!accept){switchScene.update(key(InputAction::Down));}
                    switchScene.update(key(InputAction::Confirm));answered=true;
                }else if(switchScene.dialogue().view().phase==DialoguePhase::AwaitingInput){switchScene.update(key(InputAction::Confirm));}
                else{switchScene.update({});}
            }
            require(answered&&switchScene.foreground()==ForegroundKind::None,"Both real switch branches finish their source sequence");
            require(choosingSwitch.flag(150)==accept&&choosingSwitch.flag(1)&&choosingSwitch.flag(18)&&!choosingSwitch.flag(13),"Real switch applies only the accepted fact and restores original control flags");
            if(accept){require(findCharacter(switchScene.world(),"m52-e5").motion.tile()==TilePosition{8,4},"Accepted source switch places E5 at the original C18 tile");}
            switchScene.update(key(InputAction::Confirm));require(choosingSwitch.flag(150)==accept,"Repeated input cannot invert the committed source switch");
        }
        std::cout<<"Real M52 E9 P2 switch yes/no investigation and control restoration passed\n";
        if(!trainingRuntime){real.catalog["flows"]["m52-e5-p3"]["runtime_enabled"]=true;}
        auto pickupEntry=real.entries.at("defeated-claimed");pickupEntry.map=52;pickupEntry.playerTile={8,5};pickupEntry.direction=Direction::Up;
        pickupEntry.pages.clear();pickupEntry.positions.clear();pickupEntry.directions.clear();pickupEntry.through.clear();
        pickupEntry.session.flags[150]=true;pickupEntry.session.flags[154]=false;pickupEntry.session.inventory[72]=0;
        GameSession pickupSession(real.items,pickupEntry.session,real.facts);CombatScene pickupScene(real,pickupSession,pickupEntry);
        pickupScene.update(key(InputAction::Confirm));
        require(pickupScene.foreground()==ForegroundKind::Content&&pickupSession.count(72)==0,"Real key investigation begins its source presentation before granting the item");
        for(int ticks=0;ticks<2000&&pickupScene.foreground()!=ForegroundKind::None;++ticks){
            pickupScene.update(pickupScene.dialogue().view().phase==DialoguePhase::AwaitingInput?key(InputAction::Confirm):InputFrame{});
        }
        require(pickupScene.foreground()==ForegroundKind::None&&pickupSession.count(72)==1&&pickupSession.flag(154),"Original E5 key sequence grants one I72 and commits its actual collected fact");
        require(pickupSession.flag(1)&&pickupSession.flag(18)&&!pickupSession.flag(13),"Key presentation restores original control only after its remaining cues");
        pickupScene.update(key(InputAction::Confirm));
        for(int ticks=0;ticks<100;++ticks){pickupScene.update({});}
        require(pickupSession.count(72)==1,"Highest original collected page prevents duplicate key grants");
        std::cout<<"Real M52 E5 P3 key presentation, collected page and repeated investigation passed\n";
        if(!trainingRuntime){real.catalog["flows"]["m52-e18-p1"]["runtime_enabled"]=true;}
        InputFrame walking;walking.movement=Direction::Right;walking.buttons[static_cast<int>(InputAction::Right)].held=true;
        for(int ticks=0;ticks<160&&!pickupSession.flag(156);++ticks){pickupScene.update(walking);}
        require(pickupScene.world().player().motion.tile()==TilePosition{13,5}&&pickupSession.flag(156),"Walking from the real pickup reaches E18 and activates the original training trigger");
        require(pickupSession.eventTimer()==EventTimer{EventTimerSource::Training,3659,true,false,false},"E18 source sets sixty compatible seconds and starts the single session clock at its original contact step");
        pickupScene.update({});require(pickupSession.eventTimer().remainingTicks==3658,"The active training clock advances once on the following logical update");
        std::cout<<"Real key-to-contact walking path and source training timer start passed\n";
        if(trainingRuntime){
            auto backDoor=real.entries.at("defeated-claimed");backDoor.map=54;backDoor.playerTile={5,18};backDoor.direction=Direction::Down;
            backDoor.pages.clear();backDoor.positions.clear();backDoor.directions.clear();backDoor.through.clear();
            backDoor.session.flags[163]=false;backDoor.session.flags[158]=false;
            GameSession leaving(real.items,backDoor.session,real.facts);CombatScene doorScene(real,leaving,backDoor);
            doorScene.update(key(InputAction::Confirm));
            for(int ticks=0;ticks<2400&&doorScene.foreground()!=ForegroundKind::None;++ticks){
                doorScene.update(doorScene.dialogue().view().phase==DialoguePhase::AwaitingInput?key(InputAction::Confirm):InputFrame{});
            }
            require(doorScene.mapId()==51&&doorScene.foreground()==ForegroundKind::None&&leaving.flag(163),"Real back door keeps its branch and dialogue after transfer rather than using a flat transfer interpreter");
            require(!leaving.flag(158)&&leaving.flag(1)&&leaving.flag(18)&&!leaving.flag(13),"Leaving by the back door neither invents the defeat reward nor strands player control");
        }
    }
    std::cout<<"Training finite timer flow checks passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
