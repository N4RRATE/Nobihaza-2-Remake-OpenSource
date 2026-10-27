#include "combat_content.h"
#include "game_session.h"
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);
 if(!c.maps.contains(23)||!c.maps.contains(24)||!c.items.contains(63)){throw std::runtime_error("Missing B map/tool definitions");}
 GameSession state(c.items,c.entries.at("natural").session,c.facts);
 if(state.magazine(14)!=5){throw std::runtime_error("Prepared shotgun must retain its independent seed");}
 if(c.actionProfiles.at("shotgun-fire").at("cue_order").size()!=17){throw std::runtime_error("Incomplete shotgun action");}
 std::cout<<"F4 B content readable; shotgun runtime available\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
