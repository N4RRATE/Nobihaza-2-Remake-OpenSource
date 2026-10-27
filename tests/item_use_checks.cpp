#include "game_application.h"
#include "combat_rules.h"
#include <stdexcept>
#include <iostream>
void require(bool b,const char* m){if(!b){throw std::runtime_error(m);}}
InputFrame press(InputAction a){InputFrame i;i.buttons[static_cast<int>(a)].pressed=true;return i;}
int main(int argc,char** argv){
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);auto e=c.entries.at("post-pickup").session;e.hp=70;e.inventory[2]=2;
 GameSession s(c.items,e);require(s.useMedicine(2)==ItemUseResult::Applied&&s.hp()==100&&s.count(2)==1,"I2 restores 35 capped at max and consumes once");
 require(s.useMedicine(2)==ItemUseResult::NoEffect&&s.count(2)==1,"full HP has no effect and no consumption");
 e.hp=0;GameSession dead(c.items,e);require(dead.useMedicine(2)==ItemUseResult::InvalidTarget&&dead.hp()==0&&dead.count(2)==2,"ordinary medicine does not revive");
 e.hp=100;e.tp=20;e.inventory[5]=1;e.conditions.bleedingTicks=100;
 GameSession mixed(c.items,e);require(mixed.useMedicine(5)==ItemUseResult::Applied&&mixed.tp()==30&&mixed.conditions().bleedingTicks==100,"I5 heals missing TP without curing unlisted bleeding");
 e.hp=20;e.inventory[8]=2;GameSession multi(c.items,e);
 for(int n=0;n<3;++n){if(n){multi.loseHp(20);}require(multi.useMedicine(8)==ItemUseResult::Applied,"each successful partial-piece use applies");if(n<2){require(multi.count(8)==2&&multi.itemUses(8)==n+1,"usage differs from quantity");}}
 require(multi.count(8)==1&&multi.itemUses(8)==0,"third use consumes one piece and resets usage");
 require(!multi.conditions().bleedingActive,"I8 cures explicitly listed state");
 auto ui=c;auto& initial=ui.entries.at("post-pickup").session;initial.hp=70;initial.inventory.clear();initial.inventory[2]=2;
 GameApplication app(ui,"post-pickup");app.update(press(InputAction::Cancel));app.update(press(InputAction::Confirm));app.update(press(InputAction::Confirm));
 require(app.menuView().page==MenuPage::Target,"medicine opens actor target without consumption");
 app.update(press(InputAction::Cancel));require(app.session().count(2)==2&&app.session().hp()==70,"target cancel has no effect");
 app.update(press(InputAction::Confirm));app.update(press(InputAction::Confirm));
 require(app.session().hp()==100&&app.session().count(2)==1,"menu confirmation commits medicine");
 app.update(press(InputAction::Confirm));require(app.session().count(2)==1,"held or repeated confirmation does not double consume");
 auto low=ui;low.entries.at("post-pickup").session.hp=10;GameApplication repeat(low,"post-pickup");
 repeat.update(press(InputAction::Cancel));repeat.update(press(InputAction::Confirm));repeat.update(press(InputAction::Confirm));repeat.update(press(InputAction::Confirm));repeat.update(press(InputAction::Confirm));
 require(repeat.session().hp()==45&&repeat.session().count(2)==1,"repeated edge cannot apply a second still-effective dose");
 auto cancelHeld=press(InputAction::Cancel);cancelHeld.buttons[static_cast<int>(InputAction::Confirm)].held=true;repeat.update(cancelHeld);
 require(repeat.menuView().page==MenuPage::Items,"held use key never traps cancel navigation");
 auto document=c;document.entries.at("post-pickup").session.inventory.clear();document.entries.at("post-pickup").session.inventory[180]=1;
 GameApplication book(document,"post-pickup");book.update(press(InputAction::Cancel));book.update(press(InputAction::Confirm));book.update(press(InputAction::Confirm));
 require(book.mode()!=ApplicationMode::Equipment,"selecting readable document starts its foreground presentation");
 std::set<MessageRequestId> pages;
 for(int tick=0;tick<6000&&book.mode()!=ApplicationMode::Equipment;++tick){if(book.dialogue().active()){pages.insert(book.dialogue().view().request);}book.update(tick%3==0?press(InputAction::Confirm):InputFrame{});}
 require(book.mode()==ApplicationMode::Equipment&&book.menuView().page==MenuPage::Root&&pages.size()==12,"all manual blocks return to original root menu");
 require(book.session().count(180)==1,"reading never consumes document");
 book.update(press(InputAction::Confirm));book.update(press(InputAction::Confirm));
 for(int n=0;n<120;++n){book.update({});}require(book.dialogue().active(),"second read starts a fresh first page");
 book.restartDevelopmentEntry("post-pickup");require(book.mode()==ApplicationMode::Playing&&!book.dialogue().active(),"replacement cancels old file message and completion");
 auto corridor=document;auto& ce=corridor.entries.at("post-pickup");ce.map=6;ce.playerTile={20,4};ce.direction=Direction::Up;ce.pages.clear();ce.positions.clear();ce.through.clear();ce.directions.clear();ce.session.flags[52]=false;ce.session.flags[1]=true;ce.session.flags[18]=true;
 GameApplication finished(corridor,"post-pickup");finished.update(press(InputAction::Cancel));finished.update(press(InputAction::Confirm));finished.update(press(InputAction::Confirm));
 for(int tick=0;tick<6000&&finished.mode()!=ApplicationMode::Equipment;++tick){finished.update(tick%3==0?press(InputAction::Confirm):InputFrame{});finished.takePresentation();}
 finished.update(press(InputAction::Cancel));bool erased=false;
 for(int tick=0;tick<100&&finished.mode()!=ApplicationMode::Playing;++tick){finished.update({});for(const auto& ev:finished.takePresentation()){erased=erased||ev.id=="ce151-c79";}}
 require(finished.mode()==ApplicationMode::Playing&&erased,"file closes background only after leaving reentered menu and retains final erase event");
 InputFrame move;move.movement=Direction::Up;for(int n=0;n<90&&!finished.dialogue().active();++n){finished.update(move);}
 require(finished.dialogue().active()&&finished.dialogue().view().display.continueEvents,"file C80 restore survives into Map6 inherited refusal message");
 auto vest=e;vest.hp=100;vest.conditions.bleedingTicks=0;vest.inventory[46]=1;GameSession armor(c.items,vest);
 require(biteDamage(10,armor.defense(),-5)==3&&biteDamage(10,armor.defense(),2)==10,"clothes NORMAL bite boundary");
 require(armor.equip(46,3)&&biteDamage(10,armor.defense(),-5)==1&&biteDamage(10,armor.defense(),2)==8,"vest actually changes damage boundary");
 auto abilityEntry=c.entries.at("post-pickup").session;abilityEntry.tp=30;
 GameSession abilities(c.items,abilityEntry);
 require(abilities.castSkill(c.skills.at(9))==SkillUseResult::Applied&&abilities.tp()==25,"quickshot costs five TP");
 for(int n=0;n<120;++n){abilities.tickConditions();}
 require(abilities.quickshotTicks()==780,"quickshot uses simulation time");
 require(abilities.castSkill(c.skills.at(9))==SkillUseResult::Applied&&abilities.tp()==20&&abilities.quickshotTicks()==780,"repeat cast costs TP without renewing");
 require(abilities.castSkill(c.skills.at(8))==SkillUseResult::Applied&&abilities.tp()==10&&abilities.flag(47),"precision primes next applicable hit");
 require(abilities.castSkill(c.skills.at(7))==SkillUseResult::Unavailable&&abilities.tp()==10,"aim is not a menu spell");
 abilities.resetRoomFacts();require(abilities.quickshotTicks()==780&&!abilities.flag(48)&&abilities.flag(47),"CE55 pauses quickshot but retains precision");
 std::cout<<"item rules passed\n";
}
