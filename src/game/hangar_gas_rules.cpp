#include "hangar_gas_rules.h"
#include <algorithm>
#include <stdexcept>
HangarGasRequests activeHangarGases(int level,const std::array<bool,3>& occupied){
    if(level<1||level>3){throw std::invalid_argument("Unknown encounter difficulty");}
    const bool any=std::any_of(occupied.begin(),occupied.end(),[](bool value){return value;});
    HangarGasRequests result;result.callsEmitter=level>=2||!any;
    for(std::size_t n=0;n<3;++n){result.slots[n]=level==3?!occupied[n]:!any;}return result;
}
HangarGasRequests passiveHangarGases(int before,int after,const std::array<bool,3>& occupied){
    HangarGasRequests result;result.callsEmitter=before<4&&after<4;
    for(std::size_t n=0;n<3;++n){result.slots[n]=result.callsEmitter&&!occupied[n];}return result;
}
