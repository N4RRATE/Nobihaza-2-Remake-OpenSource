#include "guard_controller.h"
#include <stdexcept>
namespace{using Json=nlohmann::json;void need(bool v,const char* s){if(!v){throw std::runtime_error(s);}}
Json flow(const std::string& object,int page,const Json& steps,const char* trigger){return {{"object",object},{"page",page},{"steps",steps},{"trigger",trigger},{"implementation_ticket",98},{"runtime_enabled",true}};}
}
int guardSourceVariable(const std::string& field){
    static const std::map<std::string,int> fields{{"terminal_stage",628},{"rescue_stage",629},{"completion_count",635},{"truth_stage",627},{"heat_ground_x",385},{"heat_ground_y",386},{"authentication_phase",387},{"access_password",44},{"lift_floor",626},{"treatment_remaining",389},{"puzzle_sample_x",44},{"clock_hours",44},{"clock_minutes",45},{"aviation_story_phase",91},{"aviation_story_pose",206},{"aviation_rotor_phase",201},{"encounter_difficulty_level",280},{"player_tile_x",1},{"player_tile_y",2},{"selected_actor",13},{"enemy_slot_6_damage",159},{"extinguish_effect_phase",277},{"suneo_threat_tile_x",248},{"suneo_threat_tile_y",249},{"terrain_tag",31},{"equipped_main_weapon",7},{"companion_ammo",247},{"guard_rescue_count",267},{"guard_support_item",268},{"guard_player_kill_count",269},{"guard_first_score",395},{"guard_second_score",394},{"player_pose",31},{"selected_key",44},{"old_zone_licker_mode",91},{"pursuer_tile_x",81},{"pursuer_tile_y",82}};
    return fields.at(field);
}
void installFirstGuardContent(CombatContent& c,const Json& guard){
    const auto& p=guard.at("runtime_profile");
    need(p.at("schema")=="nobihaza.guard-first-profile.v1"&&p.at("implementation_ticket")==98&&!p.at("runtime_enabled").get<bool>(),"Unreviewed first guard capability");
    need(p.at("slots").size()==8&&p.at("npc_roles").size()==22&&p.at("attributes")==guard.at("effective_attributes"),"Incomplete first guard profile");
    need(p.at("rules").at("hurt").at("hit_at_most")==2&&p.at("rules").at("attack_geometry").at("half_size_source_variables")==Json::array({47,48}),"Invalid first guard receiver rule");
    const int map=guard.at("map");
    const auto checkRole=[&](const std::string& object,const Json& role){
        need(c.objects.at(object).map==map,"Guard role belongs to another map");const auto& variants=c.objects.at(object).variants;
        need(std::any_of(variants.begin(),variants.end(),[&](const auto& v){return v.page==role.at("page");}),"Missing guard source page");
        for(const auto& cue:role.at("media")){need(c.cues.contains(cue.at("cue").get<std::string>()),"Missing guard media");}
    };
    for(const auto& [name,role]:p.at("npc_roles").items()){checkRole(p.at("object"),role);}
    for(const auto& [page,role]:p.at("projectile").at("page_roles").items()){checkRole(p.at("projectile").at("object"),role);}
    for(const auto& [page,role]:p.at("presentation").at("voice").at("pages").items()){checkRole(p.at("presentation").at("voice").at("object"),role);}
    for(const auto& [name,role]:p.at("attached_grenade").at("roles").items()){checkRole(p.at("attached_grenade").at("object"),role);}

    for(const auto& [number,s]:p.at("slots").items()){
        need(c.objects.at(s.at("object").get<std::string>()).map==map&&c.objects.at(s.at("manager").get<std::string>()).map==map,"Unbound guard slot");
        for(const auto& [role,page]:s.at("roles").items()){
            const auto& variants=c.objects.at(s.at("object").get<std::string>()).variants;
            need(std::any_of(variants.begin(),variants.end(),[&](const auto& v){return v.page==page.at("page");}),"Missing guard source role");
            for(const auto& cue:page.at("media")){need(c.cues.contains(cue.at("cue").get<std::string>()),"Missing guard media");}
        }
    }
    const auto& intro=guard.at("intro");c.catalog["flows"]["guard-first-intro"]=flow(intro.at("object"),intro.at("page"),intro.at("steps"),"autorun");
    for(const auto& [page,ending]:guard.at("endings").items()){c.catalog["flows"]["guard-first-end-"+page]=flow(ending.at("steps").at(3).at("cue").get<std::string>().substr(0,ending.at("steps").at(3).at("cue").get<std::string>().find("-p")),std::stoi(page),ending.at("steps"),"invoked");}
    const auto& rewards=guard.at("rewards");Json body=rewards.at("prefix");
    body.push_back({{"kind","guard_reward_tier"},{"tiers",rewards.at("tiers")}});
    body.insert(body.end(),rewards.at("tail").begin(),rewards.at("tail").end());
    c.catalog["flows"]["guard-first-rewards"]=flow(intro.at("object"),2,body,"autorun");
    const auto& support=p.at("support");for(const auto& [mode,pickup]:support.at("pickups").items()){c.catalog["flows"]["guard-first-support-"+mode]=flow(support.at("object"),pickup.at("page"),pickup.at("steps"),"action");}
    c.pendingMapTickets.erase(map);c.pendingMapTickets.erase(c.objects.at(intro.at("object").get<std::string>()).map);
    // Only routes whose source and destination capabilities have both shipped open.
    for(auto& [id,t]:c.transitions.items()){
        const int ticket=t.value("implementation_ticket",0),source=c.objects.at(t.at("object").get<std::string>()).map,dest=t.at("destination").at("map");
        if((ticket==96||ticket==97||ticket==98)&&!c.pendingMapTickets.contains(source)&&!c.pendingMapTickets.contains(dest)){t["runtime_enabled"]=true;t["handoff_only"]=false;if(c.catalog.at("flows").contains(id)){auto& caller=c.catalog["flows"][id];if(caller.value("implementation_ticket",0)==ticket){caller["runtime_enabled"]=true;}}}
    }
}

void installSecondGuardContent(CombatContent& c,const Json& evacuation){
    const auto& source=evacuation.at("runtime_profile").at("second_guard");
    need(source.at("schema")=="nobihaza.guard-second-profile.v1"&&source.at("actor")==3&&source.at("equipment")==Json::array({12,400,45,0,0}),"Unsupported second guard identity");
    need(source.at("slots").size()==8&&source.at("npc_roles").size()==20&&source.at("orchestration").at("quota")==30&&source.at("orchestration").at("timer_seconds")==120,"Incomplete second guard capability");
    const auto verify=[&](const std::string& id,const Json& role){const auto& object=c.objects.at(id);need(object.map==source.at("map"),"Unbound second guard role");need(std::any_of(object.variants.begin(),object.variants.end(),[&](const auto& v){return v.page==role.at("page");}),"Missing second guard page");for(const auto& cue:role.at("media")){need(c.cues.contains(cue.at("cue").get<std::string>()),"Missing second guard cue");}};
    for(const auto& [name,role]:source.at("npc_roles").items()){verify(source.at("object"),role);}
    for(const auto& [n,slot]:source.at("slots").items()){for(const auto& [roleName,role]:slot.at(n=="8"?"page_roles":"roles").items()){verify(slot.at("object"),role);}}
    auto p=source;
    // Adapt reviewed field shapes to the shared coordinator; source definitions remain immutable.
    p["projectile"]["init"]["source_player_center_offset_write"]=8;
    p["projectile"]["travel"]["variant"]=p.at("projectile").at("page_roles").at("3").at("variant");
    c.catalog["guard_second"]={{"map",source.at("map")},{"equipment",source.at("equipment")},{"runtime_profile",std::move(p)}};
    const auto& o=source.at("orchestration");
    const auto bind=[&](const char* id,const Json& definition,const char* trigger){auto installed=flow(definition.value("object",std::string("m117-e33")),definition.value("page",1),definition.at("steps"),trigger);installed["implementation_ticket"]=104;c.catalog["flows"][id]=std::move(installed);};
    bind("guard-second-intro",o.at("intro"),"autorun");
    for(const auto& [page,ending]:o.at("endings").items()){auto row=ending;row["object"]="m117-e33";row["page"]=std::stoi(page);bind(("guard-second-end-"+page).c_str(),row,"invoked");}
    // 105 consumes the unexecuted story tail after the original C1-8 score commits.
    Json prefix=Json::array(),tail=Json::array();for(const auto& step:o.at("post_success").at("steps")){const auto where=step.at("source").at("location").get<std::string>();const int command=std::stoi(where.substr(where.rfind(" C")+2));(command<=8?prefix:tail).push_back(step);}
    auto settlement=o.at("post_success");settlement["steps"]=prefix;settlement["steps"].push_back({{"kind","finish"}});bind("guard-second-score",settlement,"autorun");c.catalog["flows"]["guard-second-score"]["next_flow"]="guard-second-story-tail";
    c.catalog["guard_second_story_tail"]=tail;
    // The following stage stays locked until its owner admits the full story.
    auto pending=settlement;pending["steps"]=tail;bind("guard-second-story-tail",pending,"autorun");c.catalog["flows"]["guard-second-story-tail"]["implementation_ticket"]=105;c.catalog["flows"]["guard-second-story-tail"]["runtime_enabled"]=false;
    const auto& support=source.at("support");for(const auto& [mode,pickup]:support.at("pickups").items()){auto definition=pickup;definition["object"]=support.at("object");bind(("guard-second-support-"+mode).c_str(),definition,"action");}
    const auto& dog=source.at("slots").at("8");Json roles=Json::object();
    for(const auto& [name,role]:dog.at("page_roles").items()){auto r=role;r["presentation"]=dog.at("object").get<std::string>()+"-p"+std::to_string(role.at("page").get<int>());r["cues"]=Json::array();for(const auto& cue:role.at("media")){r["cues"].push_back({{"id",cue.at("cue")}});}roles[name=="knockdown"?"knocked_down":name=="knockback"?"knocked_back":name]=std::move(r);}
    roles["roam"]=roles.at("pursuit");roles["knocked_back"]=roles.at("explosive_knockback");
    c.catalog["guard_dog"]={{"page_roles",roles},{"activation_fact",18},{"attack_switch",dog.at("attack_fact")},{"normal_attributes",dog.at("attributes")},{"retained_defense_on_blow",false},{"blow_blood_type",1},{"encounter_receivers",dog.at("receivers")},{"cue_commands",{{"receiver_2",{{"17",18}}}}},{"blood_effects",{{"1","enemy-blood-small"},{"2","enemy-blood-medium"},{"3","blood-large"},{"11","enemy-blood-small"}}}};

    for(const auto& [number,slot]:source.at("slots").items()){c.enemyBurnSlots["weapon_actions"][slot.at("object").get<std::string>()]=std::stoi(number);}
    c.pendingMapTickets.erase(source.at("map").get<int>());

}
