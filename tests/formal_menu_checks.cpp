#include "game_application.h"
#include <stdexcept>
#include <iostream>
void require(bool b,const char* m){if(!b){throw std::runtime_error(m);}}
InputFrame press(InputAction a){InputFrame i;i.buttons[static_cast<int>(a)].pressed=true;return i;}
int main(int argc,char** argv){
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);
 c.entries.at("post-pickup").session.inventory[46]=1;
 GameApplication g(c,"post-pickup",[](int lo,int){return lo;});
 g.update(press(InputAction::Cancel));
 require(g.menuView().page==MenuPage::Root,"F3 opens original root instead of equipment-only shell");
 require(g.menuView().rows.size()==5&&!g.menuView().rows[3].enabled,"blank save remains disabled");
 g.update(press(InputAction::Down));g.update(press(InputAction::Down));g.update(press(InputAction::Confirm));
 require(g.menuView().page==MenuPage::Slots&&g.menuView().rows.size()==5,"five source equipment slots");
 g.update(press(InputAction::Down));g.update(press(InputAction::Down));g.update(press(InputAction::Confirm));
 g.update(press(InputAction::Right));
 require(g.menuView().currentStats->defense==5&&g.menuView().previewStats->defense==8&&g.session().equipment().armor==45,"hover previews armor without applying it");
 g.update(press(InputAction::Cancel));require(g.session().equipment().armor==45,"cancel never applies preview");g.update(press(InputAction::Confirm));g.update(press(InputAction::Right));g.update(press(InputAction::Confirm));
 require(g.session().equipment().armor==46&&g.session().count(45)==1,"armor replaces old equipment through domain owner");
 // Returning to root, skill restrictions, title confirmation and input ownership.
 g.update(press(InputAction::Cancel));g.update(press(InputAction::Down));g.update(press(InputAction::Confirm));
 auto skills=g.menuView();require(skills.page==MenuPage::Skills&&skills.rows.size()==3&&!skills.rows[0].enabled&&skills.rows[1].enabled,"source-disabled skill differs from not-yet-implemented ability");
 g.update(press(InputAction::Down));g.update(press(InputAction::Confirm));require(g.mode()==ApplicationMode::Playing&&g.session().tp()==20&&g.session().flag(47),"precision skill commits through session");
 g.update(press(InputAction::Cancel));
 g.update(press(InputAction::Up));g.update(press(InputAction::Confirm));require(g.menuView().page==MenuPage::TitleConfirm,"real return confirmation");
 g.update(press(InputAction::Cancel));require(g.mode()==ApplicationMode::Equipment,"title cancel preserves game");
 for(int n=0;n<4;++n){g.update(press(InputAction::Down));}g.update(press(InputAction::Confirm));g.update(press(InputAction::Confirm));
 require(g.mode()==ApplicationMode::Title&&!g.quitRequested(),"confirmed return reaches title without quitting or saving");
 g.restartDevelopmentEntry("post-pickup");require(g.mode()==ApplicationMode::Playing&&!g.formalMenu(),"restart destroys old menu state");
 auto invalid=c.entries.at("post-pickup").session;invalid.hp=101;
 bool rejected=false;try{GameSession bad(c.items,invalid);}catch(const std::runtime_error&){rejected=true;}
 require(rejected,"domain rejects HP beyond maximum");
 auto defs=c.items;auto extra=defs.at(46);extra.id=900;extra.equipmentType=4;defs.emplace(900,extra);
 auto entry=c.entries.at("post-pickup").session;entry.inventory[900]=1;entry.inventory[45]=99;
 GameSession variant(defs,entry);require(variant.equip(900,4)&&variant.equipped(4)==900,"different content and accessory slot use same domain rule");
 require(variant.equip(46,3)&&variant.count(45)==99,"returned equipment respects inventory cap");
 require(variant.equip(11)&&variant.magazine(11)==15&&variant.equip(0)&&variant.equip(11)&&variant.magazine(11)==15,"re-equipping never initializes ammunition");
 auto dead=c;dead.entries.at("post-pickup").session.hp=0;GameApplication dying(dead,"post-pickup");bool stopped=false;
 for(int n=0;n<6000&&dying.mode()!=ApplicationMode::Title;++n){dying.update({});const auto events=dying.takePresentation();if(dying.mode()==ApplicationMode::Title){for(const auto& e:events){stopped=stopped||e.kind==PresentationEvent::Kind::ResetGame;}}}
 require(dying.mode()==ApplicationMode::Title&&stopped,"death application transition resets game presentation");
 std::cout<<"formal menu slice passed\n";
}
