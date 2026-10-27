#include "full_content.h"
#include "branch_content.h"
#include <algorithm>
#include <stdexcept>
namespace{
using Json=nlohmann::json;
void need(bool value,const char* message){if(!value){throw std::runtime_error(message);}}
void enableFlow(CombatContent& c,const std::string& id,const Json& source){
    auto ready=source;ready["runtime_enabled"]=true;c.catalog["flows"][id]=std::move(ready);
}
bool readyDestinations(const CombatContent& c,const Json& steps){
    for(const auto& step:steps){
        if(step.at("kind")=="relocate"&&c.pendingMapTickets.contains(step.at("destination").at("map").get<int>())){return false;}
        if(step.at("kind")=="branch"&&(!readyDestinations(c,step.at("then"))||!readyDestinations(c,step.at("else")))){return false;}
    }return true;
}
void installSubstitution(CombatContent& c,const Json& truth){
    for(const auto& [id,row]:truth.at("encounters").items()){
        need(row.at("map")==138&&row.at("implementation_ticket")==100,"Unsupported substituted-map enemy");
        const auto& profile=row.at("runtime_profile");
        validateBranchEnemyProfile(c,id,profile);
        auto rules=c.rules.at("enemy");rules["runtime_profile"]=profile;rules["bite_attack"]=row.at("normal_attributes").at("attack");rules["defense"]=row.at("normal_attributes").at("defense");rules["activation_fact"]=row.at("activation_fact");
        rules["grab_max_bites"]=row.at("grab").at("maximum_damage_cycles");rules["grab_escape_direction_inputs"]=row.at("grab").at("escape_input_count");rules["grab_bite_wait_tenths"]=row.at("grab").at("wait_tenths");
        need(c.enemies.emplace(id,EnemyDefinition{id,138,row.at("source_order"),row.at("dead_fact"),false,std::move(rules)}).second,"Duplicate substituted-map enemy");
    }
    c.pendingMapTickets.erase(138);
    for(const auto& [id,source]:truth.at("flows").items()){
        const std::string object=source.at("object");
        if(c.objects.at(object).map==138||object=="m39-e16"||object=="m45-e6"||object=="m46-e2"){
            if(!readyDestinations(c,source.at("steps"))){throw std::runtime_error("Substitution reaches undelivered map: "+id);}enableFlow(c,id,source);
        }
    }
}
}
void installParkingContent(CombatContent& c,const Json& parking){
    const auto& flows=parking.at("flows");
    // Earlier reviewed packages remain usable at their delivered guard boundary.
    if(!flows.contains("m70-e32-p2")){return;}
    for(const auto& row:parking.at("encounters")){if(row.at("type")=="licker"){need(row.contains("runtime_profile"),"Missing parking licker binding");}}
    // Existing B/C profiles already implement all parking zombies, including Suneo's rescue.
    for(const auto& [id,row]:parking.at("encounters").items()){
        need(row.at("implementation_ticket")==99&&c.objects.at(id).map==row.at("map"),"Unbound parking enemy");
        if(row.at("type")!="licker"){
            need(c.enemies.contains(id)&&c.enemies.at(id).deadFlag==row.at("dead_fact"),"Missing inherited parking zombie capability");
            validateBranchEnemyProfile(c,id,row.at("runtime_profile"));continue;
        }
        auto rules=row.at("rules");const auto& profile=row.at("runtime_profile");
        for(const auto& [role,number]:profile.at("pages").items()){
            const auto& variants=c.objects.at(id).variants;need(std::any_of(variants.begin(),variants.end(),[&](const auto& page){return page.page==number;}),"Missing parking licker role");
        }
        for(const auto& cue:profile.at("cues")){need(c.cues.contains(cue.get<std::string>()),"Missing parking licker cue");}
        for(const auto& cue:profile.at("helper_sounds")){need(c.cue(cue.get<std::string>()).at("kind")=="sound","Invalid parking licker helper sound");}
        for(const auto& row:profile.at("receiver_sounds")){for(const auto& cue:row){if(!cue.is_null()){need(c.cue(cue.get<std::string>()).at("kind")=="sound","Invalid parking receiver sound");}}}
        for(const auto* group:{"blood_effects","enemy_blood_effects"}){for(const auto& effect:profile.at(group)){need(c.media.at("effects").contains(effect.get<std::string>()),"Missing parking blood effect");}}
        const auto& ending=profile.at("ending");need(ending.at("page")==profile.at("pages").at("ending"),"Mismatched parking death role");need(c.cue(ending.at("conditional_cue").at("cue").get<std::string>()).at("kind")=="flash_character","Invalid parking death flash");
        if(profile.contains("post_drop")){const auto& drop=profile.at("post_drop");need(std::any_of(c.objects.at(id).variants.begin(),c.objects.at(id).variants.end(),[&](const auto& page){return page.page==drop.at("page");})&&drop.at("clear_attack_after")==profile.at("attack_switch")&&drop.at("clear_attack_value")==false,"Invalid parking landing role");need(drop.at("commands").size()==drop.at("cues").size(),"Incomplete parking landing sequence");for(const auto& cue:drop.at("cues")){need(c.cues.contains(cue.get<std::string>()),"Missing parking landing cue");}need(c.cue(drop.at("conditional_cue").at("cue").get<std::string>()).at("kind")=="flash_character","Invalid parking landing flash");}
        rules["runtime_profile"]=profile;
        EnemyBlueprint enemy;enemy.id=id;enemy.map=row.at("map");enemy.event=c.objects.at(id).variants.front().definition.sourceEventId;enemy.sourceOrder=row.at("source_order");enemy.sourceSlot=(row.at("source_slots").at("base").get<int>()-81)/14+1;enemy.deadFlag=row.at("dead_fact");enemy.attack=row.at("normal_attributes").at("attack");enemy.defense=row.at("normal_attributes").at("defense");enemy.terminationDamage=row.at("termination_damage_threshold");enemy.behavior=EnemyBehaviorKind::Licker;enemy.hitboxHalfPixels={8,8};enemy.rules=std::move(rules);
        need(c.expansion.enemies.emplace(id,std::move(enemy)).second,"Duplicate parking licker");
    }
    std::erase_if(c.pendingMapTickets,[](const auto& row){return row.second==99;});
    if(c.full->at("runtime").contains("truth")){installSubstitution(c,c.full->at("runtime").at("truth"));}
    const auto& car=parking.at("first_guard").at("car_move");auto carFlow=car;carFlow["trigger"]="action";enableFlow(c,"parking-car-move",carFlow);
    for(const auto& [id,source]:flows.items()){
        const std::string object=source.at("object");
        if(object=="m95-e5"){
            // The inherited crisis controller and rescue flows already own these pages.
            c.catalog["flows"].erase(id);continue;
        }
        if(object==parking.at("legal_save").at("object").get<std::string>()||object=="m96-e34"||object=="m89-e51"||object=="m89-e52"||object=="m89-e53"){continue;}
        if(source.at("implementation_ticket")==99&&readyDestinations(c,source.at("steps"))){enableFlow(c,id,source);}
        if(object=="m97-e2"&&c.full->at("runtime").contains("truth")){enableFlow(c,id,source);}
    }
    installSourceSavePoint(c,parking.at("legal_save"),flows);
    for(const auto& [name,source]:parking.at("tool_requests").items()){
        const std::string object=source.at("object"),origin=source.at("conversion_object");const std::string flow=object+"-p"+std::to_string(source.at("page").get<int>());
        need(c.catalog.at("flows").at(flow).value("runtime_enabled",false),"Parking tool flow not delivered");c.catalog["flows"][flow]["trigger"]="requested";
        // The contextual owner performs conversion; raw contact copies must not run a second time.
        for(const auto& [rawId,raw]:flows.items()){
            const auto actor=raw.at("object");const bool exit=source.contains("exit_objects")&&std::find(source.at("exit_objects").begin(),source.at("exit_objects").end(),actor)!=source.at("exit_objects").end();
            if(actor==origin||exit){c.catalog["flows"][rawId]["trigger"]="requested";}
        }
        // B/C already owns the complete tooth procedure and its single contextual request.
        if(std::any_of(c.catalog.at("context_tools").begin(),c.catalog.at("context_tools").end(),[&](const auto& tool){return tool.at("object")==object&&tool.at("inactive_item")==source.at("inactive_item")&&tool.at("usable_item")==source.at("usable_item");})){continue;}
        Json tool={{"map",source.at("map")},{"object",object},{"origin_object",origin},{"request_origin_page",1},{"inactive_origin_page",1},{"origin_requires_inactive_item",true},{"request_effective_page",source.at("page")},{"inactive_item",source.at("inactive_item")},{"usable_item",source.at("usable_item")},{"request_switch",source.at("request_fact")},{"completed_switch",source.at("completed_fact")},{"flow",flow},{"flows_by_page",{{std::to_string(source.at("page").get<int>()),flow}}}};
        if(name=="valve"){
            tool["use_tile"]=source.at("use_tile");tool["exit_tiles"]=Json::array();for(const auto& exit:source.at("exit_objects")){const auto xy=c.objects.at(exit.get<std::string>()).tile;tool["exit_tiles"].push_back({xy.x,xy.y});}
        }else{
            const auto& r=source.at("interior_tiles");const auto& b=source.at("boundary_tiles");tool["inside_rect"]={r.at("x")[0],r.at("y")[0],r.at("x")[1],r.at("y")[1]};tool["exit_vertical"]={b.at("x")[0],b.at("x")[1],b.at("y")[0],b.at("y")[1]};tool["exit_horizontal"]={b.at("y")[0],b.at("y")[1],b.at("x")[0],b.at("x")[1]};
        }
        c.catalog["context_tools"].push_back(std::move(tool));
    }
    for(auto& [id,link]:c.transitions.items()){
        const int ticket=link.value("implementation_ticket",0),from=c.objects.at(link.at("object").get<std::string>()).map,to=link.at("destination").at("map");
        if(c.pendingMapTickets.contains(from)||c.pendingMapTickets.contains(to)||!link.contains("flow")){continue;}
        // Projection flow IDs and source exit IDs differ; bind their complete, equal source lists.
        const auto matching=std::find_if(c.catalog.at("flows").begin(),c.catalog.at("flows").end(),[&](const auto& flow){return flow.contains("object")&&flow.contains("page")&&flow.contains("steps")&&flow.value("runtime_enabled",true)&&flow.at("object")==link.at("object")&&flow.at("page")==link.at("page")&&flow.at("steps")==link.at("flow");});
        if(matching!=c.catalog.at("flows").end()){const auto flow=*matching;c.catalog["flows"][id]=flow;}
        if((ticket>=96&&ticket<=99)||matching!=c.catalog.at("flows").end()){
            link["runtime_enabled"]=true;link["handoff_only"]=false;
        }
    }
}
