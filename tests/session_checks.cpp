#include "game_session.h"
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* why){if(!ok){throw std::runtime_error(why);}}
int main(){
    ItemDefinition gun{11,"handgun","",1,19,0,15,31,32,30};
    ItemDefinitions definitions{{11,gun},{31,{31,"loose",""}},{32,{32,"box",""}}};
    SessionEntry entry; entry.magazines[11]=15;
    GameSession session(definitions,entry);
    require(!session.equip(11),"cannot equip an unowned gun");
    session.grant(11,1); require(session.equipment().mainWeapon==0,"pickup does not equip");
    require(session.equip(11),"equip owned gun");
    require(session.consumeRound(),"fire loaded"); require(session.magazine(11)==14,"15 -> 14");
    require(session.equip(0)&&session.equip(11),"unequip/re-equip"); require(session.magazine(11)==14,"no free reload");
    session.grant(31,1); require(session.reload()==1,"one loose round"); require(session.count(31)==0,"reserve consumed");
    session.grant(32,1); require(session.reload()==0&&session.count(32)==1,"full magazine preserves box");
    for(int i=0;i<15;++i){require(session.consumeRound(),"consume");}
    require(!session.consumeRound(),"empty is rejected");
    require(session.reload()==15 && session.count(31)==15 && session.count(32)==0,"box remainder");
    SessionEntry other; other.inventory={{11,1},{31,1},{32,1}}; other.magazines={{11,13}}; other.mainWeapon=11;
    GameSession mixed(definitions,other);
    require(mixed.reload()==2&&mixed.count(31)==29&&mixed.count(32)==0,"loose before box");
    SessionEntry bad; bad.mainWeapon=31;
    bool rejected=false; try{GameSession invalid(definitions,bad);}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"entry cannot equip ammo as weapon");
    ItemDefinitions otherDefinitions=definitions;otherDefinitions.emplace(71,ItemDefinition{71,"other gun","",1,7,0,6,31,32,30});
    SessionEntry variation;variation.mainWeapon=71;variation.magazines[71]=5;variation.inventory[31]=2;
    GameSession alternate(otherDefinitions,variation);require(alternate.reload()==1&&alternate.magazine(71)==6,"other weapon capacity and identity");
    alternate.startBleeding(300);alternate.startRecovery(30,90);alternate.resetRoomFacts();
    require(alternate.conditions().bleedingTicks==300&&alternate.conditions().invulnerableTicks==90,"room reset preserves unrelated conditions");
    for(int n=0;n<5;++n){alternate.walkedStep(5,1);}
    require(alternate.hp()==99,"bleed damages by steps");alternate.loseHp(98);for(int n=0;n<5;++n){alternate.walkedStep(5,1);}
    require(alternate.hp()==1,"map bleed cannot kill");for(int n=0;n<300;++n){alternate.tickConditions();}
    require(alternate.conditions().bleedingTicks==0&&!alternate.flag(13),"condition expiry is independent of map identity");
    session.setFlag(55,true);
    require(session.worldFlag("m9-e8.visited"),"original adapter writes one named world fact");
    session.setWorldFlag("m9-e8.visited",false);require(!session.flag(55),"named fact is authoritative for source adapter");
    session.setWorldValue("m14-e10.password",1234);require(session.variable(388)==1234,"password stored once");
    session.resetRoomFacts();require(session.worldValue("m14-e10.password")==1234,"world password survives room reset");
    session.grant(31,1000);require(session.count(31)==99,"source inventory saturates at 99");
    session.grant(31,1);require(session.count(31)==99,"repeat grant at capacity stays 99");
    // CN1.00 timer +59 ticks; fixed Player stops when integer seconds become zero.
    GameSession timed(definitions,entry);
    timed.startEventTimer(EventTimerSource::Training,1,true,false);
    require(timed.eventTimer().remainingTicks==119&&timed.eventTimer().running,"timer original 1-second seed");
    for(int n=0;n<59;++n){timed.tickEventTimer();}
    require(timed.eventTimer().remainingTicks==60&&timed.eventTimer().running,"timer last displayed second remains active");
    timed.tickEventTimer();
    require(timed.eventTimer().remainingTicks==59&&!timed.eventTimer().running&&!timed.eventTimer().visible,"integer-zero expiry preserves subsecond residue");
    timed.tickEventTimer();require(timed.eventTimer().remainingTicks==59,"stopped clock does not tick");
    timed.startEventTimer(EventTimerSource::Power,90,true,true);
    for(int n=0;n<17;++n){timed.tickEventTimer();}
    timed.stopEventTimer();
    require(timed.eventTimer().remainingTicks==5442&&!timed.eventTimer().visible&&timed.eventTimer().battle,"explicit stop retains residue and battle flag");
    timed.resetRoomFacts();
    GameSession restored(definitions,timed.exportState());
    require(restored.eventTimer()==timed.eventTimer(),"room reset and session value reconstruction preserve event clock");
    const auto beforeInvalid=timed.exportState();
    for(int seconds:{-1,0,121}){
        rejected=false;try{timed.startEventTimer(EventTimerSource::GuardFirst,seconds,true,false);}catch(const std::runtime_error&){rejected=true;}
        require(rejected&&timed.exportState()==beforeInvalid,"invalid duration leaves event clock unchanged");
    }
    SessionEntry invalidTimer=entry;invalidTimer.eventTimer={EventTimerSource::None,60,true,true,false};
    rejected=false;try{GameSession invalid(definitions,invalidTimer);}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"timer state without an owner cannot enter a session");
    require(restored.pursuerHealth()==500,"NORMAL persistent pursuer starts at 500");
    require(restored.damagePursuer(25)==25&&restored.pursuerHealth()==475,"direct damage persists independently of player HP");
    GameSession nextRoom(definitions,restored.exportState());
    require(nextRoom.pursuerHealth()==475,"pursuer health survives a new scene/session value");
    require(nextRoom.damagePursuer(999)==474&&nextRoom.pursuerHealth()==1,"original nonlethal actor damage clamps at one");
    require(nextRoom.damagePursuer(50)==0&&nextRoom.pursuerHealth()==1,"further direct damage cannot underflow persistent HP");
    nextRoom.resetRoomFacts();require(nextRoom.pursuerHealth()==1,"ordinary room cleanup does not revive pursuer");
    nextRoom.resetPursuerHealth();require(nextRoom.pursuerHealth()==500,"explicit fuse/new-game reset restores persistent health");
    require(nextRoom.hp()==100,"pursuer damage never damages player");
    rejected=false;try{nextRoom.damagePursuer(-1);}catch(const std::runtime_error&){rejected=true;}
    require(rejected&&nextRoom.pursuerHealth()==500,"negative damage is rejected before mutation");
    SessionEntry invalidPursuer=entry;invalidPursuer.pursuerHealth=0;
    rejected=false;try{GameSession invalid(definitions,invalidPursuer);}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"dead persistent pursuer cannot enter NORMAL session");
    // GL has loose rounds and no ammo-box representation; another ID proves reuse.
    auto singleDefinitions=definitions;singleDefinitions.emplace(72,ItemDefinition{72,"single loader","",1,0,0,1,31,0,0});
    SessionEntry singleEntry;singleEntry.inventory={{72,1},{31,2}};singleEntry.mainWeapon=72;singleEntry.magazines={{72,0}};
    GameSession single(singleDefinitions,singleEntry);
    require(single.reload()==1&&single.magazine(72)==1&&single.count(31)==1,"single loader consumes one loose round without a fake box");
    require(single.consumeRound()&&single.reload()==1&&single.count(31)==0,"single loader can fire and reload independently");
    single.organizeAmmo();require(single.count(31)==0&&single.count(0)==0,"organizing loose-only ammo cannot synthesize boxes");
    singleDefinitions.at(72).boxItem=32;
    rejected=false;try{GameSession invalid(singleDefinitions,singleEntry);}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"present box with zero round count is malformed");
    ItemDefinition blue{3,"blue herb","",6};blue.fieldUse=true;blue.medicine.hpPoints=10;blue.medicine.cureStates={2};
    auto medicineDefinitions=definitions;medicineDefinitions.emplace(3,blue);
    SessionEntry toxicEntry=entry;toxicEntry.flags[1]=true;toxicEntry.inventory[3]=1;
    GameSession toxic(medicineDefinitions,toxicEntry);toxic.startPoison();toxic.tickConditions();
    require(toxic.conditions().poisoned&&toxic.conditions().poisonWaitTicks==180,"CE33 enters a finite three-second wait");
    for(int n=0;n<179;++n){toxic.tickConditions();}
    require(toxic.hp()==100&&toxic.conditions().poisonWaitTicks==1,"poison does not damage before its pending wait ends");
    toxic.tickConditions();require(toxic.hp()==99,"custom parallel poison damage is one nonlethal HP");
    toxic.tickConditions();require(toxic.conditions().poisonWaitTicks==180,"ongoing poison starts its next source wait");
    require(toxic.useMedicine(3)==ItemUseResult::Applied&&toxic.hp()==100&&!toxic.conditions().poisoned&&toxic.count(3)==0,"blue herb heals ten HP and cures actual poison");
    require(toxic.conditions().poisonWaitTicks==180,"cure does not erase a CE33 wait already entered");
    auto poisonedCopy=toxic.exportState();GameSession trailing(medicineDefinitions,poisonedCopy);
    trailing.resetRoomFacts();for(int n=0;n<180;++n){trailing.tickConditions();}
    require(trailing.hp()==99&&!trailing.conditions().poisoned&&trailing.conditions().poisonWaitTicks==0,"cured pending continuation survives rooms and executes one trailing damage");
    for(int n=0;n<300;++n){trailing.tickConditions();}
    require(trailing.hp()==99,"cured poison cannot restart its loop");
    auto oneHp=poisonedCopy;oneHp.hp=1;GameSession lowest(medicineDefinitions,oneHp);
    for(int n=0;n<180;++n){lowest.tickConditions();}require(lowest.hp()==1,"poison tail never kills a live actor");
    trailing.grant(3,1);require(trailing.useMedicine(3)==ItemUseResult::Applied&&trailing.hp()==100,"blue herb also heals a healthy target");
    trailing.grant(3,1);require(trailing.useMedicine(3)==ItemUseResult::NoEffect&&trailing.count(3)==1,"full unpoisoned target does not consume herb");
    trailing.startPoison();require(trailing.useMedicine(3)==ItemUseResult::Applied&&trailing.hp()==100,"full HP poisoned target may still cure");
    std::cout << "session behavior checks passed\n";
}
