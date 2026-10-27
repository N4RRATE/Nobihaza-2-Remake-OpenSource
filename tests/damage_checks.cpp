#include "combat_rules.h"
#include <stdexcept>
#include <iostream>
void require(bool b,const char* why){if(!b){throw std::runtime_error(why);}}
int main(){
 require(biteDamage(10,5,-5)==3&&biteDamage(10,5,2)==10,"normal bite 3..10");
 require(handgunDamage(20,8,0,0,5)==7,"close low");require(handgunDamage(20,8,0,10,10)==22,"close high");
 require(handgunDamage(20,8,8,0,5)==3,"middle low");require(handgunDamage(20,8,12,0,5)==1,"far clamp");
 require(handgunDamage(40,8,0,5,0)==32,"aim effective attack");
 require(hitOverlap({0,0},{9,9},1,8)&&!hitOverlap({0,0},{10,9},1,8),"muzzle boundary");
 require(hitOverlap({0,0},{13,-13},5,8)&&!hitOverlap({0,0},{14,0},5,8),"flight boundary");
 require(criticalHit(3,10)&&!criticalHit(3,11)&&!criticalHit(2,0),"third qualified hit roll");
 std::cout<<"damage behavior checks passed\n";
}
