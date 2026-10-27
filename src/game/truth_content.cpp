#include "full_content.h"
#include <algorithm>
#include <stdexcept>
namespace{
using Json=nlohmann::json;
void need(bool value,const char* message){if(!value){throw std::runtime_error(message);}}
bool ready(const CombatContent& c,const Json& steps){
    for(const auto& step:steps){
        if(step.at("kind")=="relocate"&&(!c.maps.contains(step.at("destination").at("map").get<int>())||c.pendingMapTickets.contains(step.at("destination").at("map").get<int>()))){return false;}
        if(step.at("kind")=="branch"&&(!ready(c,step.at("then"))||!ready(c,step.at("else")))){return false;}
    }return true;
}
}
void installTruthContent(CombatContent& c,const Json& truth){
    if(!truth.contains("runtime_profile")){return;}
    const auto& p=truth.at("runtime_profile");
    need(p.at("schema")=="nobihaza.truth-profile.v1"&&p.at("implementation_ticket")==100,"Unsupported truth capability");
    const auto& eyes=p.at("eyes");const std::string object=eyes.at("object");
    const auto& derived=eyes.at("derived_count");
    need(derived.at("source_variable")==278&&derived.at("inventory_items")==Json::array({90,92})&&derived.at("mutable")==false,"Unsupported nearby inventory query");
    // The cumulative package was checked against the inherited B/C source by the loader.
    // Replace its delivered single-blue request only when the complete two-eye profile exists.
    auto& tools=c.catalog["context_tools"];
    tools.erase(std::remove_if(tools.begin(),tools.end(),[&](const auto& tool){return tool.at("object")==object;}),tools.end());
    for(const auto& [color,row]:eyes.at("colors").items()){
        const auto& requestRules=row.at("request_requires");const int installed=row.at("installed_fact");
        need(requestRules.at("installed_fact_off")==installed&&requestRules.at("usable_count_at_least")==1&&row.at("consume_count")==1,"Invalid eye consumption contract");
        (void)c.items.at(row.at("ordinary_item").get<int>());(void)c.items.at(row.at("usable_item").get<int>());
        Json uses=Json::object();
        for(const auto& page:row.at("delivery_pages")){
            const auto id=object+"-p"+std::to_string(page.get<int>());auto flow=truth.at("flows").at(id);
            need(flow.at("steps").size()==3&&flow.at("steps").at(1).at("kind")=="remove"&&flow.at("steps").at(1).at("item")==row.at("usable_item")&&flow.at("steps").at(2).at("id")==installed,"Invalid atomic eye flow");
            flow["runtime_enabled"]=true;flow["trigger"]="requested";c.catalog["flows"][id]=std::move(flow);uses[std::to_string(page.get<int>())]=id;
        }
        const int first=row.at("delivery_pages").at(0);const int originOther=color=="red"?eyes.at("colors").at("blue").at("repeat_page").get<int>():eyes.at("colors").at("red").at("repeat_page").get<int>();
        tools.push_back({{"map",requestRules.at("map")},{"object",object},{"request_origin_page",1},{"request_origin_pages",{1,originOther}},{"request_effective_page",first},{"inactive_item",row.at("ordinary_item")},{"usable_item",row.at("usable_item")},{"use_tile",requestRules.at("tile")},{"request_switch",row.at("request_fact")},{"completed_switch",installed},{"flow",object+"-p"+std::to_string(first)},{"flows_by_page",uses},{"conversion_in_flow",true}});
    }
    c.catalog["flows"].erase("blue-eye-use");
    for(const auto& [id,reading]:truth.at("readings").items()){
        need(reading.at("item").is_null()&&reading.at("continuation").at("after_reading")=="calling-flow"&&reading.at("open").size()==6&&reading.at("body").size()==32&&reading.at("close").size()==2,"Incomplete truth reading");
        need(c.catalog["readings"].emplace(id,reading).second,"Duplicate truth reading");
    }
    std::erase_if(c.pendingMapTickets,[](const auto& row){return row.second==100;});
    for(const auto& [id,source]:truth.at("flows").items()){
        if(c.catalog["flows"].at(id).value("trigger",std::string{})=="requested"){continue;}
        if(!ready(c,source.at("steps"))){continue;}
        auto flow=source;flow["runtime_enabled"]=true;c.catalog["flows"][id]=std::move(flow);
    }
    for(auto& [id,link]:c.transitions.items()){
        if(c.pendingMapTickets.contains(c.objects.at(link.at("object").get<std::string>()).map)||c.pendingMapTickets.contains(link.at("destination").at("map").get<int>())||!link.contains("flow")){continue;}
        const auto matching=std::find_if(c.catalog.at("flows").begin(),c.catalog.at("flows").end(),[&](const auto& flow){return flow.contains("object")&&flow.contains("page")&&flow.contains("steps")&&flow.value("runtime_enabled",true)&&flow.at("object")==link.at("object")&&flow.at("page")==link.at("page")&&flow.at("steps")==link.at("flow");});
        if(matching!=c.catalog.at("flows").end()){const auto flow=*matching;c.catalog["flows"][id]=flow;link["runtime_enabled"]=true;link["handoff_only"]=false;}
    }
}
