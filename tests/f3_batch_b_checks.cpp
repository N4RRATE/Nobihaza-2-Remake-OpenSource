#include "combat_content.h"
#include <stdexcept>
#include <iostream>
void require(bool b,const char* message){if(!b){throw std::runtime_error(message);}}
int main(int argc,char** argv){
    if(argc!=2){return 2;}const auto c=loadCombatContent(argv[1]);
    require(c.maps.contains(17)&&c.maps.contains(18),"B contains both future maps");
    const auto& r=c.catalog.at("readings").at("manual");int messages=0;
    for(const auto& step:r.at("body")){if(step.at("kind")=="message"){
        const auto& m=c.messages.at(step.at("message").get<std::string>()).message;
        require(!m.lines.empty(),"full manual page survives C++ reader");++messages;
    }}
    require(messages==12,"all twelve original manual blocks");
    require(c.enemies.at("m17-e3").map==17&&c.enemies.at("m17-e5").map==17,"B defines both future enemy instances");
    require(c.transitions.at("m16-e4-p2").at("runtime_enabled").get<bool>(),"ticket44 enables the reviewed route");
    require(c.media.at("terms").contains("95"),"original ammo count term");
    std::cout<<"B content handoff passed\n";
}
