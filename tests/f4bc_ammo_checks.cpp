#include "combat_content.h"
#include "game_session.h"
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* why){if(!ok){throw std::runtime_error(why);}}
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}const auto c=loadCombatContent(argv[1]);auto items=c.items;
 for(int id:{12,27,29}){require(!items.at(id).stateOnly,"Published action capability must be enabled");}
 auto e=c.entries.at("defeated-claimed").session;e.mainWeapon=12;e.auxiliaryWeapon=27;e.magazines[12]=0;e.magazines[27]=0;e.inventory[31]=0;e.inventory[32]=1;
 GameSession s(items,e,c.facts);require(s.reload(2)==7&&s.magazine(27)==7&&s.count(31)==23,"LC9 loads seven from shared box");require(s.reload(1)==13&&s.magazine(12)==13&&s.count(31)==10,"SG loads independently from remaining shared spare");require(s.consumeRound(2)&&s.magazine(27)==6&&s.magazine(12)==13,"Aux shot only spends its own magazine");
 s.equip(0,2);s.equip(27,2);require(s.magazine(27)==6,"Reequip does not seed ammunition");require(s.reload(2)==1&&s.count(31)==9,"Partial LC9 uses only missing round");require(!s.consumeRound(3)&&s.reload(3)==0&&!s.hasReserve(3),"Armor slot cannot consume ammunition");
 s.equip(0,2);s.grant(29,2);s.equip(29,2);require(s.consumeEquippedItem(2)&&s.equipped(2)==29&&s.count(29)==0,"First grenade preserves remaining equipped one");require(s.consumeEquippedItem(2)&&s.equipped(2)==0&&!s.consumeEquippedItem(2),"Last grenade leaves no equipped item and cannot double consume");
 std::cout<<"Independent main/aux ammunition and equipped consumption passed\n";
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
