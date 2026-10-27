#include "branch_content.h"
#include "combat_scene.h"
#include "enemy_controller.h"
#include "content_hash.h"
#include <algorithm>
#include <regex>
#include <stdexcept>
void loadBranchRescues(CombatContent& c){
    if(!c.branch.contains("episodes")||!c.branch.at("episodes").contains("gian")){return;}
    c.catalog["rescue_crises"]=nlohmann::json::array();
    for(const std::string name:{"gian","suneo"}){
    const auto& episode=c.branch.at("episodes").at(name);
    nlohmann::json reviewed={{"episode",episode},{"instances",nlohmann::json::array()},{"profiles",nlohmann::json::object()}};
    for(const auto& row:c.branch.at("additional_enemies").at("instances")){if(row.at("map")==episode.at("map")||(name=="gian"&&row.at("map")==48)){reviewed["instances"].push_back(row);reviewed["profiles"][row.at("id").get<std::string>()]=c.branch.at("zombie_profiles").at(row.at("id").get<std::string>());}}
    const auto& crisis=c.branch.at("additional_enemies").at("rescue_crises").at(name=="gian"?0:1);reviewed["crisis"]=crisis;
    if(contentSha256(reviewed.dump())!=(name=="gian"?"9a63e68e1fa3244805dca14fc1d89eed9fed06ddb83c56b77a2d0cb20059b308":"8649f0220a6f9b805259c9d1e0fc7bce7d0c0f9a9cde361bc5e4a7a288251350")){throw std::runtime_error("Unreviewed rescue content");}
    const std::regex cuePattern("(?:f[0-9]+bc-)?(?:m[0-9]+-e[0-9]+-p[0-9]+|ce[0-9]+)-c[0-9]+");
    const std::function<void(const nlohmann::json&)> references=[&](const auto& value){
        if(value.is_structured()){for(const auto& child:value){references(child);}}
        else if(value.is_string()){const auto id=value.template get<std::string>();if(std::regex_match(id,cuePattern)&&!c.cues.contains(id)&&!c.messages.contains(id)){throw std::runtime_error("Missing rescue presentation: "+id);}}
    };
    references(reviewed);
    for(const char* role:{"object","enemy","support_object"}){const auto id=episode.at(role).get<std::string>();if(!c.objects.contains(id)||c.objects.at(id).map!=episode.at("map").get<int>()){throw std::runtime_error("Missing rescue actor: "+id);}}
    const auto npcPage=[&](int page){const auto& variants=c.objects.at(crisis.at("npc").get<std::string>()).variants;if(std::none_of(variants.begin(),variants.end(),[&](const auto& v){return v.page==page;})){throw std::runtime_error("Missing rescue NPC page");}};
    npcPage(crisis.at("monitor").at("npc_page"));for(const auto& page:crisis.at("outcome_pages")){npcPage(page);}
    for(const auto& [id,profile]:reviewed.at("profiles").items()){validateBranchEnemyProfile(c,id,profile);}
    for(const auto& cueId:crisis.at("enemy_target").at("chase_cue_ids")){
        const std::string id=cueId;const auto& cue=c.cue(id);
        if(cue.at("kind")!="forced_route"||cue.contains("runtime_support")||!c.routes.contains(id)||c.routes.at(id).target!=10005){throw std::runtime_error("Invalid rescue chase route");}
    }
    c.catalog["rescue_crises"].push_back(crisis);
    for(const auto& row:reviewed.at("instances")){
        const std::string id=row.at("id");auto rules=c.rules.at("enemy");rules["runtime_profile"]=reviewed.at("profiles").at(id);rules["bite_attack"]=row.at("normal_attributes").at("attack");rules["defense"]=row.at("normal_attributes").at("defense");rules["death_damage_threshold"]=row.at("termination_damage_threshold");rules["activation_fact"]=row.at("activation_fact");
        if(id==crisis.at("enemy").get<std::string>()){rules["rescue"]=crisis;}
        if(!c.enemies.emplace(id,EnemyDefinition{id,row.at("map"),row.at("source_order"),row.at("dead_switch"),false,std::move(rules)}).second){throw std::runtime_error("Duplicate rescue-area enemy");}
    }
    for(const std::string outcome:{"intervened","timely"}){
        auto steps=episode.at(outcome);for(auto& step:steps){if(step.at("kind")=="set_enemy_damage"){step["enemy"]=episode.at("enemy");}}
        c.catalog["flows"]["rescue-"+name+"-"+outcome]={{"object",episode.at("object")},{"page",crisis.at("outcome_pages").at(outcome)},{"trigger","autorun"},{"steps",steps}};
    }
    }
    nlohmann::json suneoFlows=nlohmann::json::object();
    for(const auto& [id,flow]:c.branch.at("additional_flows").items()){if(c.objects.at(flow.at("object").get<std::string>()).map==95){suneoFlows[id]=flow;}}
    if(contentSha256(suneoFlows.dump())!="3f3eb4af02203b7d5f1fe8d72ad6de397c68c3efa875424b70d8de3e5199d6c4"){throw std::runtime_error("Unreviewed Suneo environment flows");}
    for(const auto& flow:suneoFlows){
        const auto& owner=c.objects.at(flow.at("object").get<std::string>());if(owner.map!=95||std::none_of(owner.variants.begin(),owner.variants.end(),[&](const auto& v){return v.page==flow.at("page").get<int>();})){throw std::runtime_error("Missing Suneo environment page");}
        for(const auto& step:flow.at("steps")){
            if(step.at("kind")=="cue"){const auto& cue=c.cue(step.at("cue").get<std::string>());if(cue.at("kind")!="bgm"||cue.contains("runtime_support")){throw std::runtime_error("Invalid Suneo environment music");}}
            if(step.at("kind")=="restore_dead_enemies"){for(const auto& placement:step.at("placements")){if(c.objects.at(placement.at("object").get<std::string>()).map!=owner.map){throw std::runtime_error("Invalid rescue corpse placement");}}}
        }
    }
    for(const auto& [id,flow]:c.branch.at("additional_flows").items()){
        const int map=c.objects.at(flow.at("object").get<std::string>()).map;if(map==47||map==48||map==63||map==95){c.catalog["flows"][id]=flow;}
    }
    for(int map:{47,48,63,95}){c.pendingMapTickets.erase(map);}
    for(auto& [id,t]:c.transitions.items()){const int target=t.at("destination").at("map");if(c.maps.contains(target)&&!c.pendingMapTickets.contains(target)){t["runtime_enabled"]=true;t["handoff_only"]=false;}}
}
void CombatScene::updateRescueCrises(){
    if(!content_.catalog.contains("rescue_crises")||session_.hp()<=0){return;}
    bool changed=false;
    for(const auto& crisis:content_.catalog.at("rescue_crises")){
        if(crisis.at("map")!=map_){continue;}
        const auto found=std::find_if(enemies_.begin(),enemies_.end(),[&](const auto& enemy){return enemy->id()==crisis.at("enemy").get<std::string>();});
        if(found==enemies_.end()){throw std::logic_error("Missing crisis enemy");}
        const auto& monitor=crisis.at("monitor");const auto& tile=monitor.at("enemy_tile");
        if(isCurrentPage(crisis.at("npc"),monitor.at("npc_page"))&&findCharacter(*world_,crisis.at("enemy")).motion.tile()==TilePosition{tile[0],tile[1]}&&(*found)->accumulatedDamage()<monitor.at("enemy_damage_below").get<int>()){
            session_.setFlag(crisis.at("intervention_switch"),true);changed=true;
        }
        // Enemy parallel work already advanced once at the start of this scene update.
        // NPC monitoring must not advance the source death-page wait a second time.
        const auto npc=crisis.at("npc").get<std::string>();
        const auto* page=content_.activeVariant(content_.objects.at(npc),session_);
        changed|=page&&selectedPages_.at(npc)!=page->page;
    }
    if(changed){refreshObjectPages();}
}
