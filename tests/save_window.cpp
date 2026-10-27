#include "combat_presentation.h"
#include <iostream>
int main(int argc,char** argv){
 try{
    if(argc<3){return 2;}auto c=loadCombatContent(argv[1]);const std::string entry=argc>3?argv[3]:"save-fixture";
    if(entry=="save-fixture"){
        auto e=c.entries.at("defeated-claimed");e.map=16;e.playerTile={10,21};e.direction=Direction::Up;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.saveCount=7;e.session.variables[399]=5;
        if(argc>4){e.session.hp=std::stoi(argv[4]);}c.entries[entry]=e;
    }
    return runCombatGame(c,entry,18000,argv[2]);
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
