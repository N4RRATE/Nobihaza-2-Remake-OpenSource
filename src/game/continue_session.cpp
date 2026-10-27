#include "game_session.h"
#include <limits>
#include <stdexcept>
namespace{
const std::map<int,int> backupMagazine{{51,275},{52,290},{53,291},{54,276},{55,292},{56,293},{57,294},{58,295},{59,296},{60,297},{61,298},{62,299},{63,273},{64,300}};
void need(bool ok,const char* message){if(!ok){throw std::runtime_error(message);}}
}
void GameSession::validateContinueCheckpoint(const std::map<int,int>& bindings,int map,TilePosition tile,bool active) const{
    need(!flag(545),"Unsupported Continue precision history");
    need(variable(272)==0||variable(272)==2||variable(272)==3,"Unsupported Continue condition history");
    for(const auto& [source,item]:bindings){need(backupMagazine.contains(source)&&definitions_.contains(item)&&definitions_.at(item).capacity>0,"Invalid Continue magazine binding");}
    if(!active){
        for(int field:{273,274,275,276}){need(variable(field)==0,"Continue backup without a captured destination");}
        for(int field=281;field<=300;++field){need(variable(field)==0,"Continue backup without a captured destination");}
        for(int field=401;field<=606;++field){need(variable(field)==0,"Continue inventory without a captured destination");}
        return;
    }
    need(variable(283)==map&&variable(281)==tile.x&&variable(282)==tile.y,"Unknown Continue destination");
    need(variable(288)>0&&variable(288)<=maxHp_&&variable(289)>=0&&variable(289)<=maxTp_,"Invalid Continue vital backup");
    auto equippedCandidate=*this;equippedCandidate.equipment_={variable(284),variable(285),variable(286),variable(287),0};
    for(int slot=1;slot<=4;++slot){need(equippedCandidate.canEquip(equippedCandidate.equipped(slot),slot),"Illegal Continue equipment backup");}
    for(int id=1;id<=206;++id){const int amount=variable(400+id);need(amount>=0&&amount<=99,"Invalid Continue inventory backup");if(amount){need(definitions_.contains(id)&&!definitions_.at(id).stateOnly&&!definitions_.at(id).pendingActionTicket,"Unavailable Continue inventory backup");}}
    for(const auto& [source,item]:bindings){const int amount=variable(backupMagazine.at(source));need(amount>=definitions_.at(item).magazineMinimum&&amount<=definitions_.at(item).capacity,"Invalid Continue magazine backup");}
    const int equippedRounds=variable(274);const int weapon=variable(284);
    need(equippedRounds>=0&&(weapon?equippedRounds<=definitions_.at(weapon).capacity:equippedRounds==0),"Invalid Continue equipped magazine backup");
}
void GameSession::captureContinueCheckpoint(const std::map<int,int>& bindings,int map,TilePosition tile){
    if(flag(586)){return;}
    need(map>0&&tile.x>=0&&tile.y>=0&&hp_>0,"Invalid Continue capture destination");
    auto candidate=*this;
    candidate.setVariable(281,tile.x);candidate.setVariable(282,tile.y);candidate.setVariable(283,map);
    for(int id=1;id<=206;++id){candidate.setVariable(400+id,count(id));}
    for(int slot=1;slot<=4;++slot){candidate.setVariable(283+slot,equipped(slot));}
    candidate.setVariable(288,hp_);candidate.setVariable(289,tp_);
    // CN1.00 CE141 C9–16: poison (state2) first, bleeding (state3) last; healthy preserves history.
    if(conditions_.poisoned){candidate.setVariable(272,2);}if(conditions_.bleedingActive){candidate.setVariable(272,3);}
    if(flag(20)){candidate.setFlag(545,true);}
    candidate.setVariable(274,equipped(1)?magazine(equipped(1)):0);
    for(const auto& [source,item]:bindings){candidate.setVariable(backupMagazine.at(source),magazine(item));}
    candidate.validateContinueCheckpoint(bindings,map,tile,true);candidate.setFlag(586,true);
    // Publish after validating the entire capture. The live collections are unchanged.
    worldValues_.swap(candidate.worldValues_);worldFlags_.swap(candidate.worldFlags_);variables_.swap(candidate.variables_);flags_.swap(candidate.flags_);
}
GameSession GameSession::prepareContinueRestore(const std::map<int,int>& bindings,int map,TilePosition tile) const{
    need(flag(586),"Continue has no checkpoint");validateContinueCheckpoint(bindings,map,tile,true);
    need(variable(630)>=0&&variable(630)<std::numeric_limits<int>::max()-1,"Continue counter exhausted");
    auto candidate=*this;
    // CE102 clears 1–210; CE103 restores only 1–206. The fifth slot stays empty.
    for(int id=1;id<=210;++id){candidate.inventory_.erase(id);candidate.itemUses_.erase(id);}
    for(int id=1;id<=206;++id){if(variable(400+id)){candidate.inventory_[id]=variable(400+id);}}
    candidate.equipment_={variable(284),variable(285),variable(286),variable(287),0};
    for(int slot=1;slot<=4;++slot){const int id=candidate.equipped(slot);if(id&&candidate.inventory_[id]>0){--candidate.inventory_[id];}}
    candidate.restoreActor();candidate.hp_=variable(288);candidate.tp_=variable(289);
    // CN1.00 CE140 C39–46 restores the single retained original state ID.
    if(variable(272)==2){candidate.conditions_.poisoned=true;}
    if(variable(272)==3){candidate.conditions_.bleedingActive=true;if(candidate.conditions_.bleedingTicks<=0){candidate.conditions_.bleedingTicks=360000;}}
    for(const auto& [source,item]:bindings){candidate.magazines_[item]=variable(backupMagazine.at(source));}
    if(candidate.equipped(1)&&definitions_.at(candidate.equipped(1)).capacity>0){candidate.magazines_[candidate.equipped(1)]=variable(274);}
    candidate.setVariable(7,candidate.equipped(1));candidate.setVariable(14,candidate.equipped(2));candidate.setVariable(50,candidate.equipped(4));
    candidate.setFlag(15,false);candidate.setFlag(45,false);candidate.setFlag(20,false);
    candidate.conditions_.invulnerableTicks=0;candidate.conditions_.recoveryTicks=0;
    candidate.setVariable(630,variable(630)+1);candidate.resetRoomFacts();
    return candidate;
}
void GameSession::commitContinueRestore(GameSession& candidate){
    need(&definitions_==&candidate.definitions_&&&facts_==&candidate.facts_,"Continue candidate belongs to another content lifetime");
    inventory_.swap(candidate.inventory_);magazines_.swap(candidate.magazines_);itemUses_.swap(candidate.itemUses_);
    variables_.swap(candidate.variables_);flags_.swap(candidate.flags_);worldFlags_.swap(candidate.worldFlags_);worldValues_.swap(candidate.worldValues_);
    std::swap(equipment_,candidate.equipment_);std::swap(hp_,candidate.hp_);std::swap(tp_,candidate.tp_);std::swap(conditions_,candidate.conditions_);
    std::swap(quickshotTicks_,candidate.quickshotTicks_);std::swap(pictureScratch_,candidate.pictureScratch_);
}
