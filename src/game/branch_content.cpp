#include "branch_content.h"
#include "combat_content.h"
#include "content_hash.h"
#include <set>
#include <functional>
#include <regex>
#include <stdexcept>
void loadBranchExploration(CombatContent& c){
    auto reviewed=nlohmann::json::object();for(const auto* name:{"wardrobe","lighter_pickup","lighter"}){reviewed[name]=c.branch.at("episodes").at(name);}reviewed["hint"]=c.branch.at("episodes").at("elements").at("hint");
    if(contentSha256(reviewed.dump())!="5604c652f7be4d1290bc9a81a272b1cb93d8d9d1cc1b0e30d75346b47583d62a"){throw std::runtime_error("Unreviewed wardrobe or lighter definition");}
    auto extra=nlohmann::json::array();for(const auto& row:c.branch.at("additional_enemies").at("instances")){if(row.at("map")!=48&&row.at("behavior")!="rescue-zombie"){extra.push_back(row);}}
    if(contentSha256(extra.dump())!="7aa31698c29e60ba383c70fd29fe75b00eadab7385d11c0bd5aa58a1021f64f1"){throw std::runtime_error("Unreviewed staff-room enemies");}
    auto profiles=c.branch.at("zombie_profiles");profiles.erase("m48-e2");profiles.erase("m63-e12");profiles.erase("m95-e12");
    const std::set<std::string> expected{"m27-e3","m27-e5","m27-e7","m27-e9","m30-e3","m30-e5","m30-e7","m30-e9","m30-e11","m38-e2","m38-e5","m39-e2","m39-e4","m39-e7","m39-e10","m39-e14","m40-e3","m40-e7","m44-e2","m44-e4","m44-e6","m80-e2","m80-e4","m81-e2","m81-e4","m81-e6","m83-e2","m83-e4","m94-e2","m96-e2","m96-e4","m96-e6"};
    std::set<std::string> received;
    const std::regex cuePattern("m[0-9]+-e[0-9]+-p[0-9]+-c[0-9]+");
    const std::function<void(const nlohmann::json&)> references=[&](const auto& value){
        if(value.is_structured()){for(const auto& child:value){references(child);}}
        else if(value.is_string()){const auto id=value.template get<std::string>();if(std::regex_match(id,cuePattern)&&!c.cues.contains(id)){throw std::runtime_error("Missing zombie profile reference: "+id);}}
    };
    if(contentSha256(profiles.dump())!="74fe81008c3655184053a05dd5d4198b6f4c28ce79b7eafa4d10989f89473dde"){throw std::runtime_error("Missing or unreviewed zombie role bindings");}
    auto instances=c.branch.at("enemy_instances");for(const auto& row:c.branch.at("additional_enemies").at("instances")){if(row.at("map")!=48&&row.at("behavior")!="rescue-zombie"){instances.push_back(row);}}
    std::size_t count=0;
    for(const auto& row:instances){
        const auto behavior=row.at("behavior").get<std::string>();
        if(behavior=="dormant-licker"||behavior=="cerberus"){continue;}
        const auto id=row.at("id").get<std::string>();const auto& profile=profiles.at(id);references(profile);received.insert(id);
        if(!c.objects.contains(id)){throw std::runtime_error("Missing zombie actor");}
        validateBranchEnemyProfile(c,id,profile);
        auto rules=c.rules.at("enemy");rules["id"]=id;rules["bite_attack"]=row.at("normal_attributes").at("attack");rules["defense"]=row.at("normal_attributes").at("defense");
        rules["death_damage_threshold"]=row.at("termination_damage_threshold");rules["runtime_profile"]=profile;rules["activation_fact"]=row.at("activation_fact");
        if(!c.enemies.emplace(id,EnemyDefinition{id,row.at("map"),row.at("source_order"),row.at("dead_switch"),false,std::move(rules)}).second){throw std::runtime_error("Duplicate zombie identity");}++count;
    }
    if(count!=profiles.size()||received!=expected){throw std::runtime_error("Unknown or missing zombie profile");}
    auto dogs=nlohmann::json::array();for(const auto& row:c.branch.at("enemy_instances")){if(row.at("behavior")=="cerberus"){dogs.push_back(row);}}
    if(contentSha256(dogs.dump())!="9e6c6dba95d0096650e837cc401bd562f7b2312f3a80cce84cd892e6f1706b6f"){throw std::runtime_error("Missing or unreviewed canine role bindings");}
    c.facts.variables.emplace(49,"combat.last-defense");
    for(const auto& row:c.branch.at("enemy_instances")){
        if(row.at("behavior")!="cerberus"){continue;}
        auto rules=row;
        // CN M32 E2P17 alone reads V49 after CE11; M31 four dogs read before.
        rules["retained_defense_on_blow"]=row.at("id")=="m32-e2";
        const int slot=row.at("source_order");rules["blow_blood_type"]=slot==1?2:1;
        for(int type:{1,2,3,11,12}){const auto effect="enemy-slot-"+std::to_string(14+slot)+"-blood-"+std::to_string(type);if(!c.media.at("effects").contains(effect)){throw std::runtime_error("Missing canine blood effect");}rules["blood_effects"][std::to_string(type)]=effect;}
        const auto id=row.at("id").get<std::string>();
        for(const auto& [name,role]:row.at("page_roles").items()){
            const auto& variants=c.objects.at(id).variants;
            if(std::none_of(variants.begin(),variants.end(),[&](const auto& v){return v.page==role.at("page");})){throw std::runtime_error("Missing canine page");}
            for(const auto& cue:role.at("cues")){if(!c.cues.contains(cue.at("id").get<std::string>())){throw std::runtime_error("Missing canine presentation");}}
        }
        if(!c.enemies.emplace(id,EnemyDefinition{id,row.at("map"),row.at("source_order"),row.at("dead_switch"),false,std::move(rules)}).second){throw std::runtime_error("Duplicate canine identity");}
    }
    c.catalog["context_tools"]=nlohmann::json::array({c.branch.at("power_tool")});
    const auto& episodes=c.branch.at("episodes");const auto& wardrobe=episodes.at("wardrobe");
    for(const std::string direction:{"left","right"}){
        auto steps=wardrobe.at("before");for(const auto& step:wardrobe.at(direction)){steps.push_back(step);}for(const auto& step:wardrobe.at("after")){steps.push_back(step);}
        c.catalog["flows"]["wardrobe-"+direction]={{"object",wardrobe.at("object")},{"page",wardrobe.at("page")},{"trigger","contact"},{"facing",direction},{"requires_lowered",true},{"steps",steps}};
    }
    const auto& pickup=episodes.at("lighter_pickup");c.catalog["flows"]["lighter-pickup"]={{"object",pickup.at("object")},{"page",pickup.at("page")},{"trigger","action"},{"facing",pickup.at("facing")},{"steps",pickup.at("steps")}};
    const auto& lighter=episodes.at("lighter");
    c.catalog["context_tools"].push_back({{"map",lighter.at("map")},{"object",lighter.at("object")},{"request_origin_page",lighter.at("origin_page")},{"request_effective_page",lighter.at("effective_page")},{"inactive_item",lighter.at("inactive_item")},{"usable_item",lighter.at("usable_item")},{"use_tile",lighter.at("use_tiles").at(0)},{"exit_tiles",lighter.at("exit_tiles")},{"request_switch",lighter.at("request_flag")},{"completed_switch",lighter.at("completed_flag")},{"flow","lighter-ignite"}});
    c.catalog["flows"]["lighter-ignite"]={{"object",lighter.at("object")},{"page",lighter.at("effective_page")},{"trigger","requested"},{"steps",lighter.at("steps")}};
    const auto& hint=episodes.at("elements").at("hint");
    for(int page:hint.at("pages")){for(const std::string direction:{"up","right","down","left"}){
        auto steps=hint.at("before");const auto& body=direction==hint.at("back_facing").get<std::string>()?hint.at("back_actor1"):hint.at("front_by_page").at(std::to_string(page));
        for(const auto& step:body){steps.push_back(step);}for(const auto& step:hint.at("after")){steps.push_back(step);}
        c.catalog["flows"]["lighter-hint-"+std::to_string(page)+"-"+direction]={{"object",lighter.at("object")},{"page",page},{"trigger","action"},{"facing",direction},{"steps",steps}};
    }}
    for(const auto& [id,flow]:c.branch.at("additional_flows").items()){
        const int map=c.objects.at(flow.at("object").get<std::string>()).map;
        if(map==44||map==92){c.catalog["flows"][id]=flow;}
    }
    const std::set<int> initialMaps{26,27,28,29,30,31,32,33,34,37,38,39,40,41,43,44,46,49,50,92};
    for(int map:initialMaps){if(!c.maps.contains(map)){throw std::runtime_error("Missing initial branch map");}c.pendingMapTickets.erase(map);}
    for(auto& [id,t]:c.transitions.items()){
        const int target=t.at("destination").at("map");
        if(c.maps.contains(target)&&!c.pendingMapTickets.contains(target)){t["runtime_enabled"]=true;t["handoff_only"]=false;}
    }
    const std::set<std::string> operations{"flag","cue","message","erase_event_once","grant","place_object","enemy_state","restore_dead_enemies","system_graphic","remove","choice","all_dogs_defeated"};
    for(const auto& [id,source]:c.branch.at("flows").items()){
        auto flow=source;const auto object=flow.at("object").get<std::string>();
        if(!c.objects.contains(object)){throw std::runtime_error("Missing branch flow object");}
        int deferred=0;
        for(const auto& step:flow.at("steps")){
            const std::string kind=step.at("kind");if(!operations.contains(kind)){throw std::runtime_error("Unknown branch flow operation");}

        }

        if(id=="m50-e3-p1"){deferred=76;}
        if(deferred){flow["implementation_ticket"]=deferred;flow["runtime_enabled"]=false;}
        for(auto& step:flow.at("steps")){
            if(step.at("kind")!="enemy_state"){continue;}
            const int map=c.objects.at(object).map,slot=step.at("source_slot");std::string actor;
            for(const auto& [enemy,d]:c.enemies){if(d.map==map&&d.sourceOrder==slot){actor=enemy;break;}}
            if(actor.empty()){throw std::runtime_error("Unbound branch enemy slot");}step["enemy"]=actor;
        }
        c.catalog["flows"][id]=std::move(flow);
    }
    for(const auto& [id,music]:c.branch.at("map_music").items()){c.expansion.mapMusic[id]=music;}
}
