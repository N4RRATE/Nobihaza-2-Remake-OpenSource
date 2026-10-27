#include "game_application.h"
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* reason){if(!ok){throw std::runtime_error(reason);}}
InputFrame press(InputAction action){InputFrame input;input.buttons[static_cast<int>(action)]={true,true,false};return input;}
int main(int argc,char** argv){
    if(argc!=2){return 2;}
    try{
        auto content=loadCombatContent(argv[1]);
        auto entry=content.entries.at("natural");entry.session.eventTimer={EventTimerSource::Training,3659,true,true,false};
        GameSession session(content.items,entry.session,content.facts);
        CombatScene scene(content,session,entry);
        scene.update(press(InputAction::Confirm));
        require(scene.foreground()!=ForegroundKind::None,"real corpse investigation starts foreground");
        require(session.eventTimer().remainingTicks==3658,"foreground entry advances timer exactly once");
        for(int n=0;n<10;++n){scene.update({});}
        require(session.eventTimer().remainingTicks==3648,"message foreground continues global timer");
        scene.deactivate();scene.update({});
        require(session.eventTimer().remainingTicks==3648,"destroyed scene never ticks its borrowed session");
        CombatObject different;different.id="timer-varied-object";different.map=22;different.tile={2,3};
        ObjectVariant base;base.page=1;ObjectVariant expiry;expiry.page=2;expiry.timerMaximumSeconds=0;
        different.variants={base,expiry};
        require(content.activeVariant(different,session)->page==1,"timer page cannot activate while integer seconds remain");
        auto expiredEntry=entry.session;expiredEntry.eventTimer={EventTimerSource::Training,59,false,false,false};
        GameSession expired(content.items,expiredEntry,content.facts);
        require(content.activeVariant(different,expired)->page==2,"original timer threshold uses integer seconds and highest eligible page");
        different.variants.back().actor=1;expiredEntry.party.clear();GameSession absent(content.items,expiredEntry,content.facts);
        require(content.activeVariant(different,absent)->page==1,"actor conditional page requires that actor actually in party");
        auto menuEntry=content.entries.at("defeated-claimed");menuEntry.session.eventTimer={EventTimerSource::Training,1234,true,true,false};
        content.entries["timer-menu-fixture"]=menuEntry;
        GameApplication application(content,"timer-menu-fixture");
        application.update(press(InputAction::Cancel));
        require(application.mode()==ApplicationMode::Equipment,"actual application opens equipment menu");
        require(application.session().eventTimer().remainingTicks==1233,"menu opening advances one Playing step");
        for(int n=0;n<10;++n){application.update({});}
        require(application.session().eventTimer().remainingTicks==1233,"menu pauses global event clock");
        application.returnToTitle();require(!application.hasGame(),"title drops old session and event clock");
        application.restartDevelopmentEntry("defeated-claimed");
        require(application.session().eventTimer()==EventTimer{},"new independent session never inherits old timer");
        // Independent value fixture: real save point and old content, new codec only.
        auto full=content;full.compatibility="f4-normal-1";
        full.legacySource=std::make_shared<const CombatContent>(content);
        auto saveEntry=content.entries.at("defeated-claimed");saveEntry.map=16;saveEntry.playerTile={10,21};saveEntry.direction=Direction::Up;
        saveEntry.pages.clear();saveEntry.positions.clear();saveEntry.directions.clear();saveEntry.through.clear();
        GameSession saving(content.items,saveEntry.session,content.facts);CombatScene saveRoom(content,saving,saveEntry);
        auto oldSnapshot=captureSave(content,saveRoom,{saveRoom.generation(),"m16-e5","BSAA 1F入口"},{});
        auto snapshot=oldSnapshot;snapshot.contentCompatibility="f4-normal-1";
        snapshot.session.eventTimer={EventTimerSource::Training,1234,true,true,false};snapshot.session.pursuerHealth=317;
        snapshot.session.conditions.poisoned=true;snapshot.session.conditions.poisonWaitTicks=17;
        snapshot.session.conditions.neuralParalysis=true;
        const auto bytes=encodeSave(full,snapshot);const auto json=nlohmann::json::parse(bytes);
        require(json.at("schema")=="nobihaza.save.v4"&&json.at("version")==4,"new common-effect shape uses v4 explicitly");
        require(json.at("session").value("neural_paralysis",false),"V2 explicitly stores state8 rather than losing it during encoding");
        require(decodeSave(full,bytes)==snapshot,"new values survive strict JSON roundtrip");
        auto restored=prepareRestore(full,snapshot);
        require(restored.session->eventTimer()==snapshot.session.eventTimer&&restored.session->pursuerHealth()==317,"Load preserves timer residue and pursuer HP without reinitialization");
        require(restored.session->conditions().poisoned&&restored.session->conditions().poisonWaitTicks==17,"Load preserves active poison and entered finite wait");
        auto cured=snapshot;cured.session.conditions.poisoned=false;
        require(decodeSave(full,encodeSave(full,cured))==cured,"cured poison may still contain an entered continuation");
        const auto oldBytes=encodeSave(content,oldSnapshot);const auto adapted=decodeSave(full,oldBytes);
        require(adapted.session.eventTimer==EventTimer{}&&adapted.session.pursuerHealth==500,"reviewed old source gains only absent semantic initial values");
        require(oldBytes==encodeSave(content,oldSnapshot),"adaptation never changes source snapshot");
        auto reject=[&](const nlohmann::json& malformed){bool refused=false;try{decodeSave(full,malformed.dump());}catch(const std::exception&){refused=true;}require(refused,"malformed v2 snapshot must be rejected");};
        for(int variation=0;variation<19;++variation){auto bad=json;
            if(variation==0){bad["schema"]="nobihaza.save.v1";bad["version"]=1;}
            else if(variation==1){bad["version"]=3;}
            else if(variation==2){bad["session"]["event_timer"]["source"]="future";}
            else if(variation==3){bad["session"]["pursuer_health"]=0;}
            else if(variation==4){bad["session"]["pursuer_health"]=501;}
            else if(variation==5){bad["session"]["event_timer"]["remaining_ticks"]=-1;}
            else if(variation==6){bad["session"]["event_timer"]["remaining_ticks"]=7260;}
            else if(variation==7){bad["session"]["event_timer"]["unknown"]=false;}
            else if(variation==8){bad["session"]["event_timer"]["running"]=false;}
            else if(variation==9){bad["session"].erase("pursuer_health");}
            else if(variation==10){bad["session"]["poison_wait_ticks"]=-1;}
            else if(variation==11){bad["session"]["poison_wait_ticks"]=181;}
            else if(variation==12){bad["session"]["poisoned"]=1;}
            else if(variation==13){bad["session"].erase("poisoned");}
            else if(variation==14){bad["session"]["event_timer"]["remaining_ticks"]=59;}
            else if(variation==15){bad["session"]["event_timer"]["remaining_ticks"]=3660;}
            else if(variation==16){bad["session"]["event_timer"]["source"]="power";bad["session"]["event_timer"]["remaining_ticks"]=5460;}
            else if(variation==17){bad["session"]["neural_paralysis"]=1;}
            else{bad["session"].erase("neural_paralysis");}
            reject(bad);
        }
        auto illegalLegacy=oldSnapshot;illegalLegacy.session.eventTimer=snapshot.session.eventTimer;
        bool refused=false;try{encodeSave(content,illegalLegacy);}catch(const std::exception&){refused=true;}
        require(refused,"old v1 writer cannot silently drop a new active timer");
        illegalLegacy=oldSnapshot;illegalLegacy.session.conditions.poisoned=true;
        refused=false;try{encodeSave(content,illegalLegacy);}catch(const std::exception&){refused=true;}
        require(refused,"v1 cannot silently discard poison");
        illegalLegacy.session.conditions.poisoned=false;illegalLegacy.session.conditions.poisonWaitTicks=17;
        refused=false;try{encodeSave(content,illegalLegacy);}catch(const std::exception&){refused=true;}
        require(refused,"v1 cannot silently discard cured pending poison");
        illegalLegacy=oldSnapshot;illegalLegacy.session.conditions.neuralParalysis=true;
        refused=false;try{encodeSave(content,illegalLegacy);}catch(const std::exception&){refused=true;}
        require(refused,"V1 cannot silently drop state8, even with source148 currently OFF");
        require(!adapted.session.conditions.neuralParalysis,"Strictly reviewed V1 adapts absent state8 to false");
        require(restored.session->conditions().neuralParalysis,"Load preserves state8 independently of source148 and149");
        auto oldMalformed=nlohmann::json::parse(oldBytes);oldMalformed["session"]["event_timer"]=json["session"]["event_timer"];reject(oldMalformed);
        std::cout<<"F4 full session lifecycle checks passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
