#include "game_application.h"
#include <iostream>
#include <stdexcept>
void require(bool b,const char* m){if(!b){throw std::runtime_error(m);}}
InputFrame press(InputAction a){InputFrame f;f.buttons[static_cast<int>(a)]={true,true,false};return f;}
struct Result{int ticks;int magazine;int loose;};
Result perform(CombatContent content,int weapon,bool equipped,bool cancel=false,int bag=51,int map=22){
    auto entry=content.entries.at("post-pickup");entry.map=map;entry.playerTile=map==22?TilePosition{10,5}:TilePosition{10,21};entry.direction=Direction::Up;
    entry.positions.clear();entry.pages.clear();entry.directions.clear();entry.through.clear();
    entry.session.flags[52]=false;entry.session.flags[1]=true;entry.session.flags[18]=true;
    entry.session.mainWeapon=weapon;entry.session.magazines[weapon]=0;entry.session.inventory[bag]=1;
    entry.session.inventory[content.items.at(weapon).boxItem]=1;entry.session.inventory[content.items.at(weapon).looseItem]=0;
    GameSession state(content.items,entry.session,content.facts);
    if(equipped){require(state.equip(bag,content.items.at(bag).equipmentType),"I51 uses actual auxiliary equipment slot4");}
    CombatScene scene(content,state,entry);scene.update(press(InputAction::Weapon));for(int n=0;n<100;++n){scene.update({});}
    require(scene.raised(),"Raise completes before reload input");scene.update(press(InputAction::Attack));
    const int full=content.items.at(weapon).capacity;
    require(state.magazine(weapon)==full&&state.count(content.items.at(weapon).looseItem)==30-full,"Ammo committed once before reload presentation");
    if(cancel){scene.deactivate();for(int n=0;n<100;++n){scene.update({});}return {0,state.magazine(weapon),state.count(content.items.at(weapon).looseItem)};}
    for(int n=1;n<500;++n){scene.update(press(InputAction::Attack));if(state.magazine(weapon)<full){return {n,state.magazine(weapon),state.count(content.items.at(weapon).looseItem)};}}
    throw std::runtime_error("Reload never released next attack");
}
int main(int argc,char** argv){try{
    if(argc!=2){return 2;}auto content=loadCombatContent(argv[1]);
    for(int weapon:{11,20,21,14}){
        auto held=perform(content,weapon,false),worn=perform(content,weapon,true);
        require(worn.ticks<held.ticks,"I51 must shorten each applicable reload only while equipped");
        // CN1.00 CE53: event-wait sums differ by3 tenths (I11/I21),2 tenths (I20).
        require(held.ticks-worn.ticks==(weapon==14?48:weapon==20?12:18),"Per-weapon waits are preserved, not a uniform speed multiplier");
        require(worn.magazine==held.magazine&&worn.loose==held.loose,"I51 does not change ammunition arithmetic");
        const auto cancelled=perform(content,weapon,true,true);require(cancelled.magazine==content.items.at(weapon).capacity,"Cancelled presentation retains submitted ammunition");
        std::cout<<weapon<<" normal="<<held.ticks<<" equipped="<<worn.ticks<<'\n';
    }
    auto changed=content;auto bag=changed.items.at(51);bag.id=901;bag.equipmentType=5;changed.items.emplace(901,bag);
    changed.expansion.reloadEquipment["11"]["equipment_item"]=901;
    require(perform(changed,11,true,false,901,16).ticks==perform(content,11,true).ticks,"Content equipment identity/slot and map variation share the same rule");
    auto menuContent=content;menuContent.entries.at("post-pickup").session.inventory[51]=1;
    GameApplication menu(menuContent,"post-pickup");menu.update(press(InputAction::Cancel));
    menu.update(press(InputAction::Down));menu.update(press(InputAction::Down));menu.update(press(InputAction::Confirm));
    for(int n=0;n<3;++n){menu.update(press(InputAction::Down));}menu.update(press(InputAction::Confirm));
    menu.update(press(InputAction::Down));menu.update(press(InputAction::Confirm));
    require(menu.session().equipped(4)==51&&menu.session().count(51)==0,"Actual menu equips bag through authoritative slot");
    menu.update(press(InputAction::Confirm));menu.update(press(InputAction::Confirm));
    require(menu.session().equipped(4)==0&&menu.session().count(51)==1,"Actual menu returns unequipped bag to inventory");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
