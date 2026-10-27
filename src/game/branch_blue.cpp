#include "branch_content.h"
#include "combat_content.h"
#include "content_hash.h"
#include <stdexcept>
#include <set>
#include <algorithm>
void loadBranchBlue(CombatContent& c){
    const auto& tooth=c.branch.at("episodes").at("tooth");
    if(contentSha256(tooth.dump())!="6c04d2c1fba253447fd34db03c6ed068560a509ddc64c82bf612bc4b96fc7b65"){throw std::runtime_error("Unreviewed tooth tool");}
    const auto page=[&](const std::string& id,int pageId){
        const auto& object=c.objects.at(id);if(object.map!=tooth.at("map").get<int>()||std::none_of(object.variants.begin(),object.variants.end(),[&](const auto& v){return v.page==pageId;})){throw std::runtime_error("Invalid tooth object/page");}
    };
    page(tooth.at("origin_object"),tooth.at("origin_page"));page(tooth.at("origin_object"),1);page(tooth.at("object"),tooth.at("effective_page"));
    for(const char* role:{"inactive_item","usable_item"}){if(!c.items.contains(tooth.at(role).get<int>())){throw std::runtime_error("Missing tooth item");}}
    auto tool=tooth;tool["request_origin_page"]=tooth.at("origin_page");tool["inactive_origin_page"]=1;tool["request_effective_page"]=tooth.at("effective_page");tool["request_switch"]=tooth.at("request_flag");tool["completed_switch"]=tooth.at("completed_flag");tool["flow"]="tooth-use";
    c.catalog["context_tools"].push_back(tool);
    auto steps=tooth.at("before");steps.push_back({{"kind","unless_flag"},{"flag",274},{"steps",tooth.at("unless_cooperation")}});for(const auto& step:tooth.at("after")){steps.push_back(step);}
    c.catalog["flows"]["tooth-use"]={{"object",tooth.at("object")},{"page",tooth.at("effective_page")},{"trigger","requested"},{"steps",steps},{"runtime_enabled",false},{"implementation_ticket",78}};
    c.catalog["flows"]["m50-e3-p1"]["runtime_enabled"]=true;
    const std::set<int> maps{80,81,83,84,88,89,90,91,93,94,96};
    nlohmann::json reviewed=nlohmann::json::object();
    for(const auto& [id,flow]:c.branch.at("additional_flows").items()){
        const int map=c.objects.at(flow.at("object").get<std::string>()).map;
        if(maps.contains(map)){reviewed[id]=flow;}
    }
    if(contentSha256(reviewed.dump())!="2e4b6f6954d7c712e8a985f2a6e18b54efbc6f1feb16d3861d74d7f07117d21b"){throw std::runtime_error("Unreviewed blue route flows");}
    const auto validate=[&](const nlohmann::json& steps){for(const auto& step:steps){
        const std::string kind=step.at("kind");
        if(kind=="message"&&!c.messages.contains(step.at("message").get<std::string>())){throw std::runtime_error("Missing blue route message");}
        if(kind=="cue"){const auto id=step.at("cue").get<std::string>();const auto& cue=c.cue(id);if(cue.contains("runtime_support")||(cue.at("kind")=="forced_route"&&!c.routes.contains(id))){throw std::runtime_error("Unsupported blue route cue");}}
        if(kind=="grant"&&!c.items.contains(step.at("item").get<int>())){throw std::runtime_error("Missing blue route item");}
    }};
    validate(tooth.at("before"));validate(tooth.at("unless_cooperation"));validate(tooth.at("after"));
    for(const auto& [id,flow]:reviewed.items()){validate(flow.at("steps"));c.catalog["flows"][id]=flow;}
    const auto& eye=c.branch.at("episodes").at("blue_eye");
    if(contentSha256(eye.dump())!="754b74588cc70babe0fe1806d3e5ea5064692ee366c7259d067845728049e8fb"){throw std::runtime_error("Unreviewed blue eye tool");}
    const auto& object=c.objects.at(eye.at("object").get<std::string>());
    for(int pageId:{eye.at("origin_page").get<int>(),eye.at("effective_page").get<int>(),5}){if(object.map!=eye.at("map").get<int>()||std::none_of(object.variants.begin(),object.variants.end(),[&](const auto& v){return v.page==pageId;})){throw std::runtime_error("Missing blue eye page");}}
    for(const char* role:{"inactive_item","usable_item"}){if(!c.items.contains(eye.at(role).get<int>())){throw std::runtime_error("Missing blue eye item");}}
    validate(eye.at("steps"));
    const auto& eyeSound=c.cue(eye.at("steps").at(0).at("cue").get<std::string>());if(eyeSound.at("kind")!="sound"){throw std::runtime_error("Invalid blue eye sound cue");}
    auto eyeTool=eye;eyeTool["request_origin_page"]=eye.at("origin_page");eyeTool["request_effective_page"]=eye.at("effective_page");eyeTool["request_switch"]=eye.at("request_flag");eyeTool["completed_switch"]=eye.at("completed_flag");eyeTool["flow"]="blue-eye-use";eyeTool["use_tile"]=eye.at("use_tiles").at(0);
    c.catalog["context_tools"].push_back(eyeTool);
    c.catalog["flows"]["blue-eye-use"]={{"object",eye.at("object")},{"page",eye.at("effective_page")},{"trigger","requested"},{"steps",eye.at("steps")}};
    if(!c.enemies.contains("m95-e12")||!c.catalog["flows"].contains("rescue-suneo-timely")||!c.catalog["flows"].contains("rescue-suneo-intervened")){throw std::runtime_error("Incomplete Suneo rescue capability");}
    c.catalog["flows"]["tooth-use"]["runtime_enabled"]=true;c.catalog["flows"]["tooth-use"].erase("implementation_ticket");
    for(int map:maps){c.pendingMapTickets.erase(map);}
    for(auto& [id,t]:c.transitions.items()){const int map=t.at("destination").at("map");if(c.maps.contains(map)&&!c.pendingMapTickets.contains(map)){t["runtime_enabled"]=true;t["handoff_only"]=false;}}

}
