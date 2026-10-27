#include "game_application.h"
#include "save_snapshot.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace{
void require(bool value,const char* message){if(!value){throw std::runtime_error(message);}}
InputFrame press(InputAction action){InputFrame input;input.buttons[static_cast<int>(action)]={true,true,false};return input;}
void openItems(GameApplication& app){
    if(app.mode()==ApplicationMode::Playing){app.update(press(InputAction::Cancel));}
    require(app.menuView().page==MenuPage::Root,"Actual application opens the root menu");
    app.update({});app.update(press(InputAction::Confirm));
    require(app.menuView().page==MenuPage::Items,"Actual root selection opens items");
}
void select(GameApplication& app,int item){
    const int count=static_cast<int>(app.menuView().rows.size());
    for(int n=0;n<count;++n){
        const auto view=app.menuView();
        if(view.rows.at(view.selection).id==item){app.takePresentation();app.update(press(InputAction::Confirm));return;}
        app.update(press(InputAction::Right));
    }
    throw std::runtime_error("Required fixture item is absent from the actual item menu");
}
std::vector<SoundDefinition> sounds(GameApplication& app){
    std::vector<SoundDefinition> result;
    for(const auto& event:app.takePresentation()){if(event.kind==PresentationEvent::Kind::Sound){result.push_back(event.sound);}}
    return result;
}
CombatEntry fixture(const CombatContent& content){
    auto entry=content.entries.at("defeated-claimed");
    entry.map=16;entry.playerTile={10,21};entry.direction=Direction::Up;
    entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();
    entry.session.flags[1]=entry.session.flags[18]=true;return entry;
}
}
int main(int argc,char** argv){
    if(argc!=2){return 2;}
    try{
        auto content=loadCombatContent(argv[1]);auto entry=fixture(content);
        if(!content.items.contains(1)){
            entry.session.hp=70;entry.session.inventory={{2,2}};entry.session.variables.erase(382);
            content.entries["closure"]=entry;GameApplication legacy(content,"closure");
            openItems(legacy);select(legacy,2);legacy.update(press(InputAction::Confirm));
            require(legacy.session().hp()==100&&legacy.session().count(2)==1&&legacy.session().variable(382)==0,"Old-package medicine remains usable without admitting unavailable spray or GL content");
            require(std::none_of(content.catalog.at("weapon_bindings").begin(),content.catalog.at("weapon_bindings").end(),[](const auto& binding){return binding.at("kind")=="gl";}),"Legacy content gains no launcher binding");
            std::cout<<"F5 legacy closure checks passed: actual herb menu, no new statistic or admitted launcher\n";return 0;
        }
        entry.session.hp=70;entry.session.inventory={{1,2},{2,1}};entry.session.variables.erase(382);
        content.entries["closure"]=entry;GameApplication spray(content,"closure");
        openItems(spray);select(spray,1);
        require(spray.menuView().page==MenuPage::Target&&spray.session().variable(382)==0,"Spray target navigation does not count a use");
        spray.update(press(InputAction::Cancel));
        require(spray.session().hp()==70&&spray.session().count(1)==2&&spray.session().variable(382)==0,"Target cancellation leaves healing, quantity and statistics unchanged");
        select(spray,1);spray.update(press(InputAction::Confirm));
        require(spray.session().hp()==100&&spray.session().count(1)==1&&spray.session().variable(382)==1,"Successful source I1 consumption records one V382 immediately");
        spray.update(press(InputAction::Confirm));spray.update({});spray.update(press(InputAction::Confirm));
        require(spray.session().count(1)==1&&spray.session().variable(382)==1,"Old confirmation edge and full-health rejection never count again");
        auto used=entry;used.session=spray.session().exportState();used.session.flags[1]=used.session.flags[18]=true;
        GameSession saving(content.items,used.session,content.facts);CombatScene room(content,saving,used);
        const auto snapshot=captureSave(content,room,{room.generation(),"m16-e5","BSAA 1F入口"},{});
        auto restored=prepareRestore(content,decodeSave(content,encodeSave(content,snapshot)));
        require(restored.session->variable(382)==1&&restored.session->count(1)==1,"Real save encoding, decoding and scene recreation preserve spray count without a schema change");
        restored.session->loseHp(20);require(restored.session->useMedicine(2)==ItemUseResult::Applied&&restored.session->variable(382)==1,"Another medicine cannot count as spray");
        restored.session->loseHp(20);require(restored.session->useMedicine(1)==ItemUseResult::Applied&&restored.session->variable(382)==2,"Post-load spray continues exactly once");
        require(restored.session->useMedicine(1)==ItemUseResult::Missing&&restored.session->variable(382)==2,"Missing spray never counts");
        auto bleeding=entry.session;bleeding.hp=100;bleeding.conditions.bleedingTicks=20;
        GameSession cure(content.items,bleeding,content.facts);
        require(cure.useMedicine(1)==ItemUseResult::Applied&&!cure.conditions().bleedingActive&&cure.variable(382)==1,"Full-health bleeding target consumes and counts a successful spray");
        bleeding.hp=0;GameSession dead(content.items,bleeding,content.facts);
        require(dead.useMedicine(1)==ItemUseResult::InvalidTarget&&dead.variable(382)==0&&dead.count(1)==2,"Dead target refuses spray without count or consumption");
        bleeding.hp=70;bleeding.variables[382]=std::numeric_limits<int>::max()-1;
        GameSession exhausted(content.items,bleeding,content.facts);const auto exhaustedBefore=exhausted.exportState();
        require(exhausted.useMedicine(1)==ItemUseResult::Unavailable&&exhausted.exportState()==exhaustedBefore,"Exhausted valid statistic refuses before healing or producing an invalid save");

        const bool gl=content.catalog.at("weapon_bindings").contains("16");
        if(gl){
            SoundDefinition originalChangeSound;
            for(const auto& [id,audio]:content.media.at("audio").items()){
                if(audio.at("source_name")=="グレネードランチャー2"){originalChangeSound={id,100,100,50};break;}
            }
            require(!originalChangeSound.resource.empty(),"CE108/109/110 C20/C33 original sound is present in admitted content");
            const auto buzzer=readSound(content.media.at("ui").at("menu").at("sounds").at("buzzer"));
            const auto cancel=readSound(content.media.at("ui").at("menu").at("sounds").at("cancel"));
            auto armed=entry;armed.session.hp=100;armed.session.mainWeapon=16;
            armed.session.inventory={{35,99},{36,2},{37,1}};armed.session.magazines[16]=1;armed.session.magazines[17]=armed.session.magazines[18]=0;
            content.entries["closure"]=armed;GameApplication launcher(content,"closure");
            openItems(launcher);const auto before=launcher.session().exportState();launcher.takePresentation();launcher.update(press(InputAction::Cancel));
            require(launcher.session().exportState()==before,"Cancelling ammo list leaves its physical launcher and all rounds untouched");
            require(sounds(launcher)==std::vector<SoundDefinition>{cancel},"Cancel emits only the menu cancel sound, never the ammunition change sound");
            openItems(launcher);select(launcher,36);
            require(launcher.menuView().page==MenuPage::Root&&launcher.session().equipped(1)==17&&launcher.session().magazine(16)==0&&launcher.session().magazine(17)==1&&launcher.session().count(35)==99&&launcher.session().count(36)==1,"Real menu selects flame, replaces one loader and returns the old round at 99 cap");
            require(sounds(launcher)==std::vector<SoundDefinition>{originalChangeSound},"Successful flame change emits the exact original 100/100/50 sound once");
            const auto flame=launcher.session().exportState();launcher.update(press(InputAction::Confirm));
            require(launcher.menuView().page==MenuPage::Root&&launcher.session().exportState()==flame,"Held or repeated old confirm cannot enter and select again");
            require(sounds(launcher).empty(),"A suppressed confirmation cannot replay the submitted sound");
            openItems(launcher);select(launcher,36);require(launcher.menuView().page==MenuPage::Items&&launcher.session().exportState()==flame,"Same-form request is a harmless domain rejection");
            require(sounds(launcher)==std::vector<SoundDefinition>{buzzer},"Same-form rejection emits only buzzer");
            launcher.update({});select(launcher,37);
            require(launcher.session().equipped(1)==18&&launcher.session().magazine(17)==0&&launcher.session().magazine(18)==1&&launcher.session().count(36)==2&&launcher.session().count(37)==0,"A second actual menu request selects acid with the exact round return and consumption");
            require(sounds(launcher)==std::vector<SoundDefinition>{originalChangeSound},"Successful acid change emits the same source sound exactly once");
            auto carried=armed;carried.session.mainWeapon=11;carried.session.inventory[16]=1;
            content.entries["closure"]=carried;GameApplication bag(content,"closure");openItems(bag);select(bag,36);
            require(bag.session().equipped(1)==11&&bag.session().count(16)==0&&bag.session().count(17)==1,"Changing a carried launcher preserves another equipped gun");
            require(sounds(bag)==std::vector<SoundDefinition>{originalChangeSound},"Carried launcher change also emits the dedicated source sound once");
            carried.session.inventory.erase(16);content.entries["closure"]=carried;GameApplication noGun(content,"closure");
            openItems(noGun);const auto unowned=noGun.session().exportState();select(noGun,36);
            require(noGun.menuView().page==MenuPage::Items&&noGun.session().exportState()==unowned,"Ammo with no owned launcher rejects without creating a weapon");
            require(sounds(noGun)==std::vector<SoundDefinition>{buzzer},"Unowned launcher rejection never emits a successful change sound");
        }else{
            require(content.items.at(16).stateOnly&&!content.catalog.at("weapon_bindings").contains("17"),"Legacy content retains undelivered GL admission");
        }
        std::cout<<"F5 closure checks passed: actual menu edges, spray statistics/save roundtrip, "<<(gl?"all GL menu paths":"legacy admission")<<"\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
