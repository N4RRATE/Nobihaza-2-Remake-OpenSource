#include "game_session.h"
#include <iostream>
#include <stdexcept>
#include <tuple>
void require(bool ok,const char* reason){if(!ok){throw std::runtime_error(reason);}}
ItemDefinitions loaderDefinitions(){
    // Public domain fixture from CN1.00 I16/17/18 and CE108/109/110; no natural capability is enabled.
    ItemDefinitions d;
    for(const auto [id,attack,ammo]:{std::tuple{16,99,35},std::tuple{17,84,36},std::tuple{18,99,37}}){
        ItemDefinition gun;gun.id=id;gun.equipmentType=1;gun.attack=attack;gun.capacity=1;gun.looseItem=ammo;d.emplace(id,gun);
    }
    for(int id:{35,36,37}){ItemDefinition ammo;ammo.id=id;ammo.equipmentType=10;ammo.fieldUse=true;ammo.uses=0;d.emplace(id,ammo);}
    ItemDefinition other;other.id=11;other.equipmentType=1;d.emplace(11,other);return d;
}
SessionEntry loadedEntry(){
    SessionEntry e;e.mainWeapon=16;e.inventory={{35,6},{36,2},{37,3}};e.magazines={{16,1},{17,0},{18,0}};return e;
}
void rejects(GameSession& session,int item,const std::vector<int>& forms,ItemUseResult result,const char* reason){
    const auto before=session.exportState();require(session.selectWeaponAmmunition(item,forms)==result&&session.exportState()==before,reason);
}
int main(){try{
    auto definitions=loaderDefinitions();const std::vector<int> forms{16,17,18};GameSession session(definitions,loadedEntry());
    // CE109 C21-29: replace ordinary GL, return its loaded I35, load and consume one I36.
    require(session.selectWeaponAmmunition(36,forms)==ItemUseResult::Applied,"Equipped GL may switch to flame ammunition");
    require(session.equipped(1)==17&&session.count(16)==0&&session.count(17)==0,"Switch retains the one equipped physical launcher");
    require(session.magazine(16)==0&&session.magazine(17)==1&&session.magazine(18)==0,"Only the flame form remains loaded");
    require(session.count(35)==7&&session.count(36)==1&&session.count(37)==3,"Old loaded round returns and new ammunition commits exactly once");
    // CE108/110 provide the reverse ordinary and acid conversions, not additional guns.
    require(session.selectWeaponAmmunition(37,forms)==ItemUseResult::Applied,"Flame may switch to acid");
    require(session.equipped(1)==18&&session.magazine(16)==0&&session.magazine(17)==0&&session.magazine(18)==1&&session.count(35)==7&&session.count(36)==2&&session.count(37)==2,"Acid replaces the flame round and returns I36");
    require(session.selectWeaponAmmunition(35,forms)==ItemUseResult::Applied,"Acid may switch back to ordinary");
    require(session.equipped(1)==16&&session.magazine(16)==1&&session.magazine(17)==0&&session.magazine(18)==0&&session.count(35)==6&&session.count(36)==2&&session.count(37)==3,"Full cycle preserves the physical loader and source ammunition results");
    const auto unchanged=session.exportState();
    require(session.selectWeaponAmmunition(35,forms)==ItemUseResult::NoEffect&&session.exportState()==unchanged,"Selecting the current form neither reloads nor consumes");
    require(session.consumeRound()&&session.magazine(16)==0&&session.count(35)==6,"CE69 C4 firing commits separately from reserve loading");
    const auto spent=session.exportState();GameSession cancelled(definitions,spent);cancelled.resetRoomFacts();
    require(cancelled.magazine(16)==0&&cancelled.count(35)==6,"Room cancellation cannot refund a committed shot");
    require(cancelled.reload()==1&&cancelled.magazine(16)==1&&cancelled.count(35)==5,"CE69 C38/C57 automatic refill is a later single-round commit");
    GameSession reloaded(definitions,cancelled.exportState());reloaded.resetRoomFacts();
    require(reloaded.magazine(16)==1&&reloaded.count(35)==5&&reloaded.reload()==0,"Cancellation after loading preserves the committed round; a full loader consumes nothing");
    auto empty=loadedEntry();empty.magazines[16]=0;GameSession emptyLoader(definitions,empty);
    require(emptyLoader.selectWeaponAmmunition(36,forms)==ItemUseResult::Applied&&emptyLoader.count(35)==6&&emptyLoader.count(36)==1&&emptyLoader.magazine(16)==0&&emptyLoader.magazine(17)==1,"An empty old form returns no imaginary old round");
    auto bag=loadedEntry();bag.mainWeapon=11;bag.inventory[16]=1;GameSession carried(definitions,bag);
    require(carried.selectWeaponAmmunition(36,forms)==ItemUseResult::Applied&&carried.equipped(1)==11&&carried.count(16)==0&&carried.count(17)==1&&carried.magazine(17)==1&&carried.count(35)==7&&carried.count(36)==1,"A carried loader changes form without replacing another equipped gun");
    auto saturated=loadedEntry();saturated.inventory[35]=99;GameSession fullReserve(definitions,saturated);
    require(fullReserve.selectWeaponAmmunition(36,forms)==ItemUseResult::Applied&&fullReserve.count(35)==99&&fullReserve.count(36)==1&&fullReserve.magazine(16)==0&&fullReserve.magazine(17)==1,"Source AddItem clamps the returned old round at 99; conversion is not rejected");
    auto gatedDefinitions=definitions;gatedDefinitions.at(17).stateOnly=true;GameSession gated(gatedDefinitions,loadedEntry());const auto gatedBefore=gated.exportState();
    require(gated.selectWeaponAmmunition(36,forms)==ItemUseResult::Unavailable&&gated.exportState()==gatedBefore,"An undelivered new form must reject before changing the old gun or round");
    auto doubled=loadedEntry();doubled.inventory[17]=1;GameSession invalidOwnership(definitions,doubled);const auto invalidBefore=invalidOwnership.exportState();
    require(invalidOwnership.selectWeaponAmmunition(37,forms)==ItemUseResult::InvalidTarget&&invalidOwnership.exportState()==invalidBefore,"Two physical forms cannot be normalized by spending ammunition or deleting a gun");
    GameSession reverse(definitions,loadedEntry());
    require(reverse.selectWeaponAmmunition(37,{18,16,17})==ItemUseResult::Applied&&reverse.equipped(1)==18&&reverse.count(35)==7&&reverse.count(37)==2,"Definition IDs, not group order, select acid");
    require(reverse.selectWeaponAmmunition(36,forms)==ItemUseResult::Applied&&reverse.equipped(1)==17&&reverse.count(37)==3&&reverse.count(36)==1,"Acid-to-flame returns acid");
    require(reverse.selectWeaponAmmunition(35,forms)==ItemUseResult::Applied&&reverse.equipped(1)==16&&reverse.count(35)==6&&reverse.count(36)==2,"Flame-to-ordinary returns flame");
    GameSession sameEmpty(definitions,empty);rejects(sameEmpty,35,forms,ItemUseResult::NoEffect,"Selecting an empty current form does not refill it");
    auto none=loadedEntry();none.mainWeapon=0;GameSession noGun(definitions,none);
    rejects(noGun,36,forms,ItemUseResult::InvalidTarget,"Ammunition cannot create an unowned launcher");
    auto missing=loadedEntry();missing.inventory[36]=0;GameSession noRound(definitions,missing);
    rejects(noRound,36,forms,ItemUseResult::Missing,"Missing requested ammunition rejects without returning or moving the old round");
    auto doubleRound=loadedEntry();doubleRound.magazines[18]=1;GameSession invalidLoad(definitions,doubleRound);
    rejects(invalidLoad,36,forms,ItemUseResult::InvalidTarget,"Multiple loaded forms reject before committing anything");
    auto wrongLoad=loadedEntry();wrongLoad.magazines[16]=0;wrongLoad.magazines[18]=1;GameSession foreignLoad(definitions,wrongLoad);
    rejects(foreignLoad,36,forms,ItemUseResult::InvalidTarget,"A loaded unowned form is not transferred into the owned gun");
    rejects(session,999,forms,ItemUseResult::Unavailable,"Unknown ammunition has no effect");
    rejects(session,36,{},ItemUseResult::Unavailable,"An absent source binding has no effect");
    rejects(session,36,{16,17,17},ItemUseResult::Unavailable,"Duplicate forms are not a valid reviewed binding");
    rejects(session,36,{16,0,18},ItemUseResult::Unavailable,"The unequip sentinel is not a weapon form");
    rejects(session,36,{16,17,999},ItemUseResult::Unavailable,"Missing form definition rejects safely");
    auto duplicateAmmo=definitions;duplicateAmmo.at(18).looseItem=36;GameSession ambiguous(duplicateAmmo,loadedEntry());
    rejects(ambiguous,36,forms,ItemUseResult::Unavailable,"An ambiguous ammunition mapping cannot choose an arbitrary form");
    auto pending=definitions;pending.at(17).pendingActionTicket=91;GameSession actionPending(pending,loadedEntry());
    rejects(actionPending,36,forms,ItemUseResult::Unavailable,"Pending actions cannot be bypassed by a domain request");
    auto forbidden=definitions;forbidden.at(17).actorPermissions={false};GameSession actorForbidden(forbidden,loadedEntry());
    rejects(actorForbidden,36,forms,ItemUseResult::Unavailable,"A form unavailable to the player cannot be selected");
    auto noField=definitions;noField.at(36).fieldUse=false;GameSession itemForbidden(noField,loadedEntry());
    rejects(itemForbidden,36,forms,ItemUseResult::Unavailable,"A nonselectable item cannot start the menu ammunition operation");
    // Different IDs and two forms use the same public seam; they are an explicit variation fixture.
    ItemDefinitions variation;ItemDefinition a;a.id=71;a.equipmentType=1;a.capacity=1;a.looseItem=81;
    ItemDefinition b=a;b.id=72;b.looseItem=82;variation.emplace(71,a);variation.emplace(72,b);
    for(int id:{81,82}){ItemDefinition ammo;ammo.id=id;ammo.equipmentType=10;ammo.fieldUse=true;variation.emplace(id,ammo);}
    SessionEntry other;other.mainWeapon=71;other.inventory={{81,4},{82,1}};other.magazines={{71,1},{72,0}};GameSession varied(variation,other);
    require(varied.selectWeaponAmmunition(82,{72,71})==ItemUseResult::Applied&&varied.equipped(1)==72&&varied.magazine(71)==0&&varied.magazine(72)==1&&varied.count(81)==5&&varied.count(82)==0,"Another identity group shares the loader without original IDs");
    GameSession restored(variation,varied.exportState());
    require(restored.magazine(71)==0&&restored.magazine(72)==1&&restored.count(81)==5&&restored.count(82)==0&&restored.equipped(1)==72,"A fresh Session preserves the converted load without reseeding");
    std::cout<<"GL shared loader domain checks passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
