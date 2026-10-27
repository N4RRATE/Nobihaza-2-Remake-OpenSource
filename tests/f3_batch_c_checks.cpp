#include "combat_content.h"
#include <iostream>
#include <stdexcept>
void require(bool b,const char* text){if(!b){throw std::runtime_error(text);}}
int main(int argc,char** argv){
    if(argc!=2){return 2;}
    const auto content=loadCombatContent(argv[1]);
    const auto& opening=content.catalog.at("opening");
    require(content.maps.contains(87)&&opening.at("implementation_ticket")==43,"C resources available to future opening");
    require(opening.at("runtime_enabled"),"43 enables audited opening runtime");
    for(const auto& key:opening.at("message_ids")){
        require(content.messages.contains(key.get<std::string>()),"43 shared dialogue can execute all opening controls");
    }
    require(content.messages.contains("ce151-c9"),"current manual still available");
    require(content.objects.at("m4-e2").variants.size()==4,"actual door pages retained");
    std::cout<<"C catalog loads while preserving explicit runtime boundary\n";
}
