#include "game_application.h"
#include "menu_controller.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

void require(bool ok,const char* message){if(!ok){throw std::runtime_error(message);}}
InputFrame press(InputAction action){InputFrame input;input.buttons[static_cast<int>(action)]={true,true,false};return input;}
void choose(MenuController& menu,int id){
    auto view=menu.view();
    const auto row=std::find_if(view.rows.begin(),view.rows.end(),[&](const auto& candidate){return candidate.id==id;});
    require(row!=view.rows.end(),"Required public menu row is missing");
    const auto target=static_cast<int>(row-view.rows.begin());
    for(std::size_t n=0;menu.view().selection!=target&&n<view.rows.size();++n){menu.update(press(InputAction::Down));}
    require(menu.view().selection==target,"Menu selection did not reach the requested row");
    menu.update(press(InputAction::Confirm));
}
int firstHit(const CombatContent& content,AttackKind kind,bool worn){
    // Public domain fixture: I54 is granted here; its natural training reward belongs to ticket93.
    auto entry=content.entries.at("post-pickup");
    GameSession session(content.items,entry.session,content.facts);
    require(session.equip(11,1),"Owned real handgun can be equipped");
    if(kind==AttackKind::Knife){session.grant(24,1);require(session.equip(24,2),"Real auxiliary knife can be equipped");}
    if(worn){session.grant(54,1);require(session.equip(54,4),"I54 can be worn before the hit");}
    CombatScene scene(content,session,entry,[](int low,int high){return low==0&&high==10?5:high;});
    const auto id=content.rules.at("enemy").at("id").get<std::string>();
    const auto position=findCharacter(scene.world(),id).motion.position();
    std::set<std::string> resolved;
    const auto result=scene.resolveAttack({{(position.x+.5)*16,(position.y+.5)*16},{0,0},kind,2,0,false},resolved);
    require(result.processed==1,"Real first enemy receives one ordinary noncritical hit");
    return result.damage;
}
int main(int argc,char** argv){
    if(argc!=2){return 2;}
    try{
        const auto content=loadCombatContent(argv[1]);
        require(content.compatibility=="f4-normal-1","This check requires the actual full package");
        const auto& scope=content.items.at(54);
        require(scope.equipmentType==4&&scope.attack==10&&scope.defense==0&&scope.spirit==0&&scope.agility==0,"CN1.00 Item54 fields3/11-14 define head equipment attack+10 only");
        auto entry=content.entries.at("defeated-claimed");
        GameSession session(content.items,entry.session,content.facts);
        const int attack=session.attack(),auxiliary=session.auxiliaryAttack(),magazine=session.magazine(11);
        session.grant(54,1);
        MenuController menu(content,session);
        choose(menu,2);choose(menu,4);choose(menu,54);
        require(session.equipped(4)==54&&session.count(54)==0,"Actual menu commits one inventory item to the head slot");
        require(session.attack()==attack+10&&session.auxiliaryAttack()==auxiliary,"Head equipment raises actual attack but not auxiliary agility");
        require(session.magazine(11)==magazine,"Menu equip never refills the handgun");
        const auto before=session.exportState();
        require(!session.equip(54,2)&&session.exportState()==before,"Wrong-slot request is rejected before changing inventory or equipment");
        require(session.equip(54,4)&&session.exportState()==before,"Equipping the same worn item does not duplicate it");
        choose(menu,4);
        menu.update(press(InputAction::Cancel));
        require(session.exportState()==before,"Cancelling equipment selection preserves the complete session");
        choose(menu,4);choose(menu,0);
        require(session.count(54)==1&&session.equipped(4)==0&&session.attack()==attack,"Unequip returns exactly one item and removes its attack contribution");
        require(session.equip(54,4)&&session.consumeRound(),"Equip and one actual ammunition commit both succeed");
        require(session.equip(0,4)&&session.equip(54,4)&&session.magazine(11)==magazine-1,"Head-slot reequipping preserves an already spent round");
        require(firstHit(content,AttackKind::Handgun,true)==firstHit(content,AttackKind::Handgun,false)+10,"Real enemy hit callback reads the current attack including I54");
        require(firstHit(content,AttackKind::Knife,true)==firstHit(content,AttackKind::Knife,false),"Real auxiliary hit callback receives no I54 agility bonus");

        // Actual original save object and v2 codec, with explicit equipment fixture rather than natural reward claims.
        entry.map=16;entry.playerTile={10,21};entry.direction=Direction::Up;
        entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();
        CombatScene saveRoom(content,session,entry);
        const auto snapshot=captureSave(content,saveRoom,{saveRoom.generation(),"m16-e5","BSAA 1F入口"},{});
        const auto candidate=prepareRestore(content,decodeSave(content,encodeSave(content,snapshot)));
        require(candidate.session->equipped(4)==54&&candidate.session->count(54)==0,"New scene/session restores one authoritative worn I54");
        require(candidate.session->attack()==attack+10&&candidate.session->auxiliaryAttack()==auxiliary&&candidate.session->magazine(11)==magazine-1,"Restore preserves attributes and spent ammunition without reseeding");
        require(candidate.session->equip(0,4)&&candidate.session->count(54)==1,"Restored equipment can be unequipped without losing or duplicating ownership");
        require(candidate.session->discard(54,1),"Fixture can remove its only owned scope");
        const auto absent=candidate.session->exportState();
        require(!candidate.session->equip(54,4)&&candidate.session->exportState()==absent,"Missing-item equip request does not invent ownership");
        std::cout<<"I54 full package menu, hit and v2 behavior passed (domain fixtures; natural reward pending93)\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
