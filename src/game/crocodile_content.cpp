#include "full_content.h"
#include <algorithm>
#include <stdexcept>
namespace{using Json=nlohmann::json;
bool reachesMap(const Json& steps,int map){for(const auto& s:steps){if(s.at("kind")=="relocate"&&s.at("destination").at("map")==map){return true;}if(s.at("kind")=="branch"&&(reachesMap(s.at("then"),map)||reachesMap(s.at("else"),map))){return true;}}return false;}
bool ready(const CombatContent& c,const Json& steps){for(const auto& s:steps){if(s.at("kind")=="relocate"&&c.pendingMapTickets.contains(s.at("destination").at("map").get<int>())){return false;}if(s.at("kind")=="branch"&&(!ready(c,s.at("then"))||!ready(c,s.at("else")))){return false;}}return true;}
}
void installPostCrocodileContent(CombatContent& c,const Json& evacuation){
    std::erase_if(c.pendingMapTickets,[](const auto& row){return row.second==103;});
    // Admit the original vent landing; its compulsory autorun remains owned by 104.
    const auto& landing=evacuation.at("flows").at(evacuation.at("runtime_profile").at("route").at("vents").back().get<std::string>()).at("steps").back().at("destination");
    c.pendingMapTickets.erase(landing.at("map").get<int>());
    auto intro=evacuation.at("runtime_profile").at("second_guard").at("orchestration").at("intro");
    intro["trigger"]="autorun";intro["runtime_enabled"]=false;intro["implementation_ticket"]=104;c.catalog["flows"]["guard-second-intro"]=std::move(intro);
    for(const auto& [id,source]:evacuation.at("flows").items()){
        auto& installed=c.catalog["flows"].at(id);
        if(installed.value("runtime_enabled",true)||!ready(c,source.at("steps"))){continue;}
        if(source.at("implementation_ticket")!=103&&(source.at("implementation_ticket")!=101||!reachesMap(source.at("steps"),evacuation.at("runtime_profile").at("route").at("required_post_boss_maps").back().get<int>()))){continue;}
        installed["runtime_enabled"]=true;
    }
    for(auto& [id,link]:c.transitions.items()){
        if(!link.contains("flow")||c.pendingMapTickets.contains(c.objects.at(link.at("object").get<std::string>()).map)||c.pendingMapTickets.contains(link.at("destination").at("map").get<int>())){continue;}
        const auto matching=std::find_if(c.catalog.at("flows").begin(),c.catalog.at("flows").end(),[&](const auto& flow){return flow.contains("object")&&flow.contains("page")&&flow.contains("steps")&&flow.value("runtime_enabled",true)&&flow.at("object")==link.at("object")&&flow.at("page")==link.at("page")&&flow.at("steps")==link.at("flow");});
        if(matching!=c.catalog.at("flows").end()){const auto flow=*matching;c.catalog["flows"][id]=flow;link["runtime_enabled"]=true;link["handoff_only"]=false;}
    }
}
void installCrocodileContent(CombatContent& c,const Json& evacuation){
    const auto& p=evacuation.at("runtime_profile");const auto& boss=p.at("crocodile");
    if(boss.at("schema")!="nobihaza.crocodile-profile.v1"||boss.at("initialization_is_victory")!=false||boss.at("slots").size()!=3||boss.at("victory_at_least")!=1000){throw std::runtime_error("Unsupported crocodile contract");}
    for(const auto& [object,pages]:boss.at("roles").items()){for(const auto& [page,role]:pages.items()){for(const auto& cue:role.at("media")){(void)c.cue(cue.at("cue").get<std::string>());}}}
    for(const auto& pages:boss.at("receivers")){for(const auto& receiver:pages){if(receiver.at("enabled").get<bool>()){(void)c.media.at("effects").at("enemy-slot-18-blood-"+std::to_string(receiver.at("blood_work").get<int>()));}}}
    c.catalog["crocodile"]=boss;
    // Shared CE24-26 burn progression reads the explicit three body samples supplied by the boss.

    auto& burnSources=c.enemyBurnSlots["weapon_actions"];
    burnSources["m109-e9"]=1;burnSources["m109-e8"]=2;burnSources["crocodile-side-third"]=3;
    // The pre-battle map shares ticket 103's source allocation; it is required by 102's natural entrance.
    c.pendingMapTickets.erase(c.objects.at(p.at("intro").at("object").get<std::string>()).map);
    c.pendingMapTickets.erase(boss.at("map").get<int>());c.pendingMapTickets.erase(boss.at("victory").at("destination").at("map").get<int>());
    const auto& intro=p.at("intro");c.catalog["flows"]["crocodile-intro"]={{"object",intro.at("object")},{"page",intro.at("page")},{"trigger","requested"},{"steps",intro.at("steps")},{"runtime_enabled",true}};
    const auto& win=boss.at("victory");c.catalog["flows"]["crocodile-victory"]={{"object",win.at("object")},{"page",win.at("page")},{"trigger","requested"},{"steps",win.at("steps")},{"runtime_enabled",true}};
    const int introMap=c.objects.at(intro.at("object").get<std::string>()).map;
    for(const auto& [id,source]:evacuation.at("flows").items()){
        const int map=c.objects.at(source.at("object").get<std::string>()).map;
        // Only reopen 101 exits blocked by the pre-battle map. Preserve its normalized environment and source-save flows.
        if(map!=introMap&&(source.at("implementation_ticket")!=101||!reachesMap(source.at("steps"),introMap))){continue;}
        if(!ready(c,source.at("steps"))){continue;}
        auto flow=source;flow["runtime_enabled"]=true;c.catalog["flows"][id]=std::move(flow);
    }
    for(auto& [id,link]:c.transitions.items()){
        if(!link.contains("flow")||c.pendingMapTickets.contains(c.objects.at(link.at("object").get<std::string>()).map)||c.pendingMapTickets.contains(link.at("destination").at("map").get<int>())){continue;}
        const auto it=std::find_if(c.catalog.at("flows").begin(),c.catalog.at("flows").end(),[&](const auto& f){return f.contains("object")&&f.contains("page")&&f.contains("steps")&&f.value("runtime_enabled",true)&&f.at("object")==link.at("object")&&f.at("page")==link.at("page")&&f.at("steps")==link.at("flow");});
        if(it!=c.catalog.at("flows").end()){const auto flow=*it;c.catalog["flows"][id]=flow;link["runtime_enabled"]=true;link["handoff_only"]=false;}
    }

}
