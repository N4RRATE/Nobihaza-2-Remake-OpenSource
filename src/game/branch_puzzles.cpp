#include "branch_content.h"
#include "combat_content.h"
#include "content_hash.h"
#include <stdexcept>
namespace{
using Json=nlohmann::json;
Json joined(std::initializer_list<Json> parts){auto result=Json::array();for(const auto& part:parts){for(const auto& step:part){result.push_back(step);}}return result;}
}
void loadBranchPuzzles(CombatContent& c){
    if(!c.branch.at("episodes").contains("stool")){return;}
    const auto& e=c.branch.at("episodes");const auto& stool=e.at("stool");const auto& puzzle=e.at("elements");
    const Json reviewed={{"stool",stool},{"elements",puzzle}};
    if(contentSha256(reviewed.dump())!="51cd4ecb18ee600f786a5aba9ca09c4676c74e2f0e35c106c8d799d936fc78db"){throw std::runtime_error("Unreviewed spatial puzzle rules");}
    if(!stool.at("climb_decision_triggered").get<bool>()){throw std::runtime_error("Unsupported stool trigger");}
    auto& flows=c.catalog["flows"];
    const auto flow=[&](const Json& object,int page,const std::string& trigger,const Json& steps){return Json{{"object",object},{"page",page},{"trigger",trigger},{"steps",steps}};};
    const auto origin=stool.at("object");
    auto climb=flow(origin,stool.at("page"),"action",joined({stool.at("climb"),stool.at("after")}));
    climb["requires_lowered"]=true;climb["facing"]=stool.at("climb_facing");climb["player_relative"]={{"object",origin},{"offset",stool.at("climb_from_offset")}};flows["stool-climb"]=climb;
    for(const auto& [direction,steps]:stool.at("push").items()){
        auto push=flow(origin,stool.at("page"),"contact",joined({stool.at("push_before"),steps,stool.at("after")}));
        push["requires_lowered"]=true;push["decision_excluded"]=true;push["facing"]=direction;
        push["destination"]={{"object",origin},{"direction",direction},{"blocked_terrain",stool.at("blocked_terrain")},{"blocked_object",stool.at("blocked_object")}};
        flows["stool-push-"+direction]=push;
    }
    const auto& descend=stool.at("descend");
    auto down=flow(descend.at("object"),descend.at("page"),"action",joined({descend.at("before"),descend.at("steps"),descend.at("after")}));
    down["destination"]={{"object",origin},{"offset",descend.at("landing_from_stool")},{"blocked_terrain",descend.at("unless_terrain")}};flows["stool-descend"]=down;
    const auto& high=stool.at("switch");
    auto button=flow(high.at("object"),high.at("page"),"action",joined({high.at("before"),Json::array({{{"kind","choice"},{"cue",high.at("choice")},{"accept",high.at("accept")},{"decline",high.at("decline")}}}),high.at("after")}));
    button["player_tile"]=high.at("player_tile");flows["stool-switch"]=button;
    for(const auto& [object,statue]:puzzle.at("statues").items()){
        for(const auto& [direction,steps]:statue.at("directions").items()){
            auto push=flow(object,statue.at("page"),"contact",joined({statue.at("before"),steps,statue.at("after_move"),Json::array({{{"kind","if_player_moved"},{"steps",statue.at("on_position_changed")}}}),statue.at("after")}));
            push["requires_lowered"]=true;push["facing"]=direction;flows["statue-"+object+"-"+direction]=push;
        }
    }
    flows["elements-monitor"]=flow("m49-e11",1,"environment",Json::array({{{"kind","match_positions"},{"positions",puzzle.at("solution")},{"flag",puzzle.at("solved_flag")}}}));
    flows["elements-reveal"]=flow("m49-e3",4,"autorun",puzzle.at("reveal"));
    flows["elements-restore"]=flow("m49-e15",1,"environment",puzzle.at("restore"));
    const auto& key=puzzle.at("key");
    flows["elements-key"]=flow("m49-e3",5,"action",joined({key.at("before"),Json::array({{{"kind","unless_flag"},{"flag",274},{"steps",key.at("unless_cooperation")}}}),key.at("after")}));
    // ponytail: natural key pickup remains gated until tickets77/80 deliver rescue and return consumers.
    flows["elements-key"]["runtime_enabled"]=false;flows["elements-key"]["implementation_ticket"]=77;
    for(const auto& [id,f]:c.branch.at("additional_flows").items()){
        const int map=c.objects.at(f.at("object").get<std::string>()).map;
        if(map==49){flows[id]=f;}
    }
    c.pendingMapTickets.erase(stool.at("map").get<int>());
    for(auto& [id,t]:c.transitions.items()){
        const int target=t.at("destination").at("map");
        if(c.maps.contains(target)&&!c.pendingMapTickets.contains(target)){t["runtime_enabled"]=true;t["handoff_only"]=false;}
    }
}
