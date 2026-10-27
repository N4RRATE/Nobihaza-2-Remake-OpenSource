#include "game_session.h"
#include "combat_scene.h"
#include "enemy_host.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
void require(bool value,const char* reason){if(!value){throw std::runtime_error(reason);}}
int main(int argc,char** argv){try{
    if(argc!=1&&argc!=2){return 2;}
    const ItemDefinitions items;const WorldFactDefinitions facts{{},{{301,"burn.slot-1.lifetime"}}};
    SessionEntry entry;entry.worldValues["burn.slot-1.lifetime"]=23;
    GameSession original(items,entry,facts);
    original.setVariable(83,168);original.setVariable(84,67);
    auto bodyCandidate=original;
    require(bodyCandidate.variable(83)==168&&bodyCandidate.variable(84)==67,"An ordinary candidate keeps the source body sample shared by CE15 and CE24");
    bodyCandidate.setVariable(83,201);
    require(original.variable(83)==168,"A candidate cannot overwrite the original body sample");
    const auto bodySnapshot=original.exportState();
    require(!bodySnapshot.variables.contains(83)&&!bodySnapshot.variables.contains(84),"The sampled body is transient picture work, not a second persistent actor position");
    GameSession loadedBody(items,bodySnapshot,facts);
    require(loadedBody.variable(83)==0&&loadedBody.variable(84)==0,"Load rebuilds the source body workspace at zero");
    bodyCandidate.resetRoomFacts();
    require(bodyCandidate.variable(83)==0&&bodyCandidate.variable(84)==0,"CE55 clears both source body coordinates");
    for(const int id:{83,84}){
        auto invalidBody=entry;invalidBody.variables[id]=1;bool invalid=false;
        try{GameSession rejectedBody(items,invalidBody,facts);}catch(const std::runtime_error&){invalid=true;}
        require(invalid,"Persistent input cannot smuggle either body coordinate into a new session");
        auto invalidFacts=facts;invalidFacts.variables[id]="invalid.body";invalid=false;
        try{GameSession rejectedBody(items,entry,invalidFacts);}catch(const std::runtime_error&){invalid=true;}
        require(invalid,"Content cannot declare the transient body as a persistent named fact");
    }
    // CN1.00 CE113 C1-C2: set149, enter Wait40, yield240 updates.
    require(!original.neuralRecoveryWaiting(),"Fresh CE113 has not executed C1");
    original.setNeuralParalysis(true);original.setFlag(148,true);original.setFlag(149,true);
    require(!original.advanceNeuralRecoveryWait(true),"CE113 enters its source240-update wait before state removal");
    for(int n=0;n<99;++n){require(!original.advanceNeuralRecoveryWait(true),"CE113 cannot remove state8 during its entered wait");}
    require(original.neuralRecoveryWaiting(),"An entered CE113 must not replay C1 during repeated gas hits");
    auto neuralCandidate=original;neuralCandidate.resetRoomFacts();
    for(int n=0;n<20;++n){require(!neuralCandidate.advanceNeuralRecoveryWait(false),"OFF pauses CE113 without clearing its entered wait");}
    for(int n=0;n<140;++n){require(!neuralCandidate.advanceNeuralRecoveryWait(true),"CE113 resumes the same wait after an ordinary transfer");}
    require(neuralCandidate.advanceNeuralRecoveryWait(true),"CE113 resumes C3 exactly on update241 rather than restarting its wait");
    require(!original.advanceNeuralRecoveryWait(true),"A prepared candidate cannot advance the original common continuation");
    const auto neuralSnapshot=original.exportState();
    require(neuralSnapshot.conditions.neuralParalysis&&neuralSnapshot.flags.at(148)&&neuralSnapshot.flags.at(149),"State8 and the two source switches remain independent persistent facts");
    GameSession loadedNeural(items,neuralSnapshot,facts);
    require(!loadedNeural.neuralRecoveryWaiting(),"Load reconstructs C1 instead of restoring a pending interpreter wait");
    for(int n=0;n<240;++n){require(!loadedNeural.advanceNeuralRecoveryWait(true),"Load restarts CE113 at C1 without persisting an interpreter cursor");}
    require(loadedNeural.advanceNeuralRecoveryWait(true),"Load receives a fresh full source240-update wait");
    loadedNeural.setNeuralParalysis(false);loadedNeural.setFlag(148,false);loadedNeural.setFlag(149,false);
    require(!loadedNeural.conditions().neuralParalysis,"Only the source removal command clears state8");
    loadedNeural.setNeuralParalysis(true);loadedNeural.setFlag(148,false);
    require(loadedNeural.exportState().conditions.neuralParalysis&&!loadedNeural.flag(148),"State8 must not be an alias of source activation148");
    loadedNeural.setFlag(148,true);
    for(int n=0;n<240;++n){require(!loadedNeural.advanceNeuralRecoveryWait(true),"A completed cycle resets its own finite wait for the next gas hit");}
    require(loadedNeural.advanceNeuralRecoveryWait(true),"A second activation completes independently");
    // CE112 C1/C9: flash then Wait1; reaching event end is a separate update.
    GameSession neuralEffect(items,entry,facts);
    require(neuralEffect.advanceNeuralEffect(true),"CE112 emits its first flash when activation149 starts");
    for(int n=0;n<3;++n){require(!neuralEffect.advanceNeuralEffect(true),"CE112 yields during the source6-update wait");}
    auto effectCandidate=neuralEffect;effectCandidate.resetRoomFacts();
    for(int n=0;n<20;++n){require(!effectCandidate.advanceNeuralEffect(false),"OFF pauses the CE112 entered wait without restarting its flash");}
    require(!effectCandidate.advanceNeuralEffect(true)&&!effectCandidate.advanceNeuralEffect(true)&&!effectCandidate.advanceNeuralEffect(true),"An ordinary copy retains the two wait updates and one event-end update");
    require(effectCandidate.advanceNeuralEffect(true),"CE112 restarts C1 on update8, not at its wait-ending update7");
    require(!neuralEffect.advanceNeuralEffect(true),"The source effect continuation is independent of the prepared copy");
    GameSession effectLoad(items,neuralEffect.exportState(),facts);
    require(effectLoad.advanceNeuralEffect(true),"Load restarts the finite CE112 program at its initial flash");
    require(original.burnWaitRemaining(24)==0,"Fresh CE24 has no running fire frame");
    require(!original.advanceBurnWait(24,true),"CE24 begins its six source waits before damage");
    require(original.burnWaitRemaining(24)==6,"The first fire frame starts its two source waits");
    for(int n=0;n<3;++n){require(!original.advanceBurnWait(24,true),"First three source waits do not commit damage");}
    auto transferred=original;transferred.resetRoomFacts();
    require(transferred.burnWaitRemaining(24)==3,"Ordinary map candidate keeps the current fire-frame phase");
    for(int n=0;n<20;++n){require(!transferred.advanceBurnWait(24,false),"OFF pauses the same common-event wait");}
    require(transferred.variable(301)==23,"Room reset keeps the persistent lifetime counter");
    require(!transferred.advanceBurnWait(24,true)&&!transferred.advanceBurnWait(24,true)&&transferred.advanceBurnWait(24,true),"Ordinary map candidates retain the remaining three waits");
    require(!original.advanceBurnWait(24,true),"Candidate progress does not mutate the source session");
    auto loaded=GameSession(items,transferred.exportState(),facts);
    require(loaded.burnWaitRemaining(24)==0,"Load rebuilds fire presentation rather than restoring an interpreter cursor");
    require(!loaded.advanceBurnWait(24,true),"Load begins a finite six-wait entry without a saved interpreter stack");
    for(int n=0;n<5;++n){require(!loaded.advanceBurnWait(24,true),"Load cannot reuse the old pending cursor");}
    require(loaded.advanceBurnWait(24,true)&&loaded.variable(301)==23,"Load preserves the lifetime counter but rebuilds the wait");
    require(!loaded.advanceBurnWait(31,true),"Another source slot has independent progress");
    loaded.finishBurnWait(24);require(!loaded.advanceBurnWait(24,true),"Terminal burn cancels only its own wait");
    for(int n=0;n<5;++n){require(!loaded.advanceBurnWait(31,true),"Finishing slot one leaves slot eight intact");}
    require(loaded.advanceBurnWait(31,true),"Slot eight reaches its own source endpoint");
    bool rejected=false;try{loaded.advanceBurnWait(23,true);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"Unknown burn common events are rejected");
    rejected=false;try{loaded.burnWaitRemaining(32);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"Unknown fire-phase queries are rejected");
    require(original.burnPictureStage(24)==BurnPictureStage::InitialShow,"Fresh finite picture continuation starts before its one initial Show");
    original.setBurnPictureStage(24,BurnPictureStage::MoveSecond);original.setVariable(93,123);original.setVariable(94,-45);
    auto pending=original;require(pending.burnPictureStage(24)==BurnPictureStage::MoveSecond&&pending.variable(93)==123&&pending.variable(94)==-45,"Ordinary candidates preserve the pending picture command and the one shared source workspace");
    pending.setVariable(93,999);require(original.variable(93)==123,"Preparing another session cannot overwrite the source picture workspace");
    const auto snapshot=original.exportState();require(!snapshot.variables.contains(93)&&!snapshot.variables.contains(94),"Transient picture coordinates do not enter the disk snapshot");
    GameSession rebuilt(items,snapshot,facts);require(rebuilt.burnPictureStage(24)==BurnPictureStage::InitialShow&&rebuilt.variable(93)==0&&rebuilt.variable(94)==0,"Load rebuilds picture commands and workspace rather than persisting an interpreter continuation");
    pending.resetRoomFacts();require(pending.burnPictureStage(24)==BurnPictureStage::MoveSecond&&pending.variable(93)==0&&pending.variable(94)==0,"CE55 clears the shared working coordinates but does not invent a new common-event cursor");
    pending.setBurnPictureStage(31,BurnPictureStage::Erase);pending.finishBurnWait(24);
    require(pending.burnPictureStage(24)==BurnPictureStage::InitialShow&&pending.burnPictureStage(31)==BurnPictureStage::Erase,"Completing one picture thread preserves another pending erase");
    rejected=false;try{pending.setBurnPictureStage(24,static_cast<BurnPictureStage>(999));}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"Unknown finite picture stages cannot silently corrupt the continuation");
    auto invalidSnapshot=snapshot;invalidSnapshot.variables[93]=123;
    rejected=false;try{GameSession invalid(items,invalidSnapshot,facts);}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"A snapshot cannot smuggle a transient picture workspace into persistent variables");
    original.setVariable(107,321);original.setVariable(108,654);original.setVariable(191,-3);original.setVariable(192,42);
    const auto allPictures=original.exportState();
    require(!allPictures.variables.contains(107)&&!allPictures.variables.contains(108)&&!allPictures.variables.contains(191)&&!allPictures.variables.contains(192),"The exact CE25 and CE31 source working pairs are transient rather than one misleading shared pair for all eight slots");
    auto copiedPictures=original;copiedPictures.resetRoomFacts();
    require(original.variable(107)==321&&original.variable(191)==-3&&copiedPictures.variable(107)==0&&copiedPictures.variable(192)==0,"Copy and CE55 handle all audited picture pairs without changing the source session");
    original.setBloodPictureProgress({2,1,true,true});auto copiedBlood=original;copiedBlood.resetRoomFacts();
    require(copiedBlood.bloodPictureProgress()==BloodPictureProgress{2,1,true,true},"CE15 retains its finite pending command across ordinary candidates and CE55");
    GameSession loadedBlood(items,original.exportState(),facts);
    require(loadedBlood.bloodPictureProgress()==BloodPictureProgress{},"Load rebuilds CE15 rather than saving its presentation cursor");
    rejected=false;try{original.setBloodPictureProgress({0,1,false,false});}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"An idle blood continuation cannot carry a nonzero frame");
    if(argc==2){
        auto content=loadCombatContent(argv[1]);content.pendingMapTickets.erase(52);auto sample=content.entries.at("defeated-claimed");sample.map=52;sample.playerTile={16,10};sample.positions["m52-e4"]={16,10};
        GameSession pictureSession(content.items,sample.session,content.facts);CombatScene scene(content,pictureSession,sample);EnemyHost host(scene,"m52-e4");
        const auto source=host.actorScreenFoot();pictureSession.setVariable(93,static_cast<int>(std::lround(source.x)));pictureSession.setVariable(94,static_cast<int>(std::lround(source.y))-21);
        const auto first=host.pictureWorkspacePosition(24);
        require(std::abs(first.x-16)<.001&&std::abs(first.y-8.6875)<.001,"Screen-foot work must become a grid anchor whose rendered foot preserves the source -21 pixel offset");
        const auto bodyAnchor=host.screenPicturePosition({source.x+13,source.y-21});
        require(std::abs(bodyAnchor.x-16.8125)<.001&&std::abs(bodyAnchor.y-8.6875)<.001,"E2 sampled work uses the same inverse-foot transform as common-event pictures without resampling the live actor");
        pictureSession.setVariable(191,static_cast<int>(std::lround(source.x))+13);pictureSession.setVariable(192,static_cast<int>(std::lround(source.y))-21);
        const auto last=host.pictureWorkspacePosition(31);
        require(std::abs(last.x-16.8125)<.001&&std::abs(last.y-8.6875)<.001,"A second source slot preserves its own +13/-21 image offset on a differently sized map");
    }
    std::cout<<"Finite burn continuation ownership checks passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
