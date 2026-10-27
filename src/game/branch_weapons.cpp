#include "combat_content.h"
#include "content_hash.h"
#include <stdexcept>

void loadBranchWeapons(CombatContent& content){
    if(!content.branch.contains("action_rules")){return;}
    const auto& rules=content.branch.at("action_rules");
    if(contentSha256(rules.dump())!="62dd6ea6a06329211b4fbdd15ed6eed4dd7307462f44e80d411a047257c53c29"){throw std::runtime_error("Unreviewed B/C weapon action rules");}
    for(const auto& [id,row]:rules.at("shared").at("grenade_receivers").at("legacy_profiles").items()){
        if(content.enemies.contains(id)){
            auto& enemy=content.enemies.at(id);auto& profile=enemy.story?content.rules["enemy"]["runtime_profile"]:enemy.rules["runtime_profile"];profile.merge_patch(row.at("profile"));
        }else{
            const auto kind=content.expansion.enemies.at(id).behavior;
            const std::string name=kind==EnemyBehaviorKind::RunningZombie?"running-zombie":kind==EnemyBehaviorKind::FallingZombie?"falling-zombie":"soldier-zombie";
            content.expansion.behaviorDefinitions[name]["runtime_profile"].merge_patch(row.at("profile"));
        }
    }
    for(auto& [id,enemy]:content.enemies){enemy.rules["runtime_profile"]["grenade_clear_attack"]=false;}
    for(const auto& id:rules.at("shared").at("grenade_receivers").at("clear_attack_objects")){if(content.enemies.contains(id.get<std::string>())){content.enemies.at(id.get<std::string>()).rules["runtime_profile"]["grenade_clear_attack"]=true;}}
    for(auto& [id,enemy]:content.enemies){auto& p=enemy.rules["runtime_profile"];if(p.contains("body_page")&&!p.contains("body_cancel")){
        const auto cue=id+"-p"+std::to_string(p.at("body_page").get<int>())+"-c2";
        if(content.cue(cue).at("kind")!="cancel_routes"){throw std::runtime_error("Missing source body cancellation");}p["body_cancel"]=cue;
    }}
    const std::string blood="enemy-slot-16-blood-13";
    if(!content.media.at("effects").contains(blood)){throw std::runtime_error("Missing grenade licker blood");}
    content.expansion.behaviorDefinitions["licker"]["runtime_profile"]["enemy_blood_effects"]["13"]=blood;
    const auto& profiles=rules.at("profiles");
    const auto compile=[&](const nlohmann::json& steps){
        std::vector<std::string> sequence;
        for(const auto& step:steps){const std::string kind=step.at("kind");
            if(kind=="cue"){
                const std::string id=step.at("cue");const auto& cue=content.cue(id);
                sequence.push_back(cue.at("kind")=="wait_routes"?"wait-player-route":id);
            }else if(kind=="start_attack"){sequence.push_back("commit-grenade");}
            else if(kind=="set_direction_lock"){/* Direction locks belong to the finite route, not the actor. */}
            else if(kind!="set_weapon_posture"&&kind!="cancel_if_control_lost"){throw std::runtime_error("Unsupported grenade action step");}
        }
        return sequence;
    };
    for(const auto& [name,profile]:profiles.items()){
        if(name=="sg-reload-easy"||(!name.starts_with("sg-")&&!name.starts_with("lc9-")&&!name.starts_with("grenade-"))){continue;}
        auto steps=profile.at("steps");nlohmann::json action;
        if(name=="sg-fire"||name=="sg-quick-fire"||name=="lc9-fire"){
            action["sound_cue"]=steps.at(0).at("cue");
            steps.erase(steps.begin(),steps.begin()+4);
            action["replacement_wait_tenths"]=0;
        }
        auto sequence=compile(steps);
        if(name=="grenade-throw"){sequence.push_back("finish-grenade");}
        action["cue_order"]=sequence;
        if(profile.contains("preserve_facing_route_by_source_direction")){action["restore_facing"]=profile.at("preserve_facing_route_by_source_direction");}
        content.actionProfiles[name]=std::move(action);
    }
    const auto& throwing=profiles.at("grenade-throw");
    for(const auto* state:{"remains","empty"}){content.actionProfiles[std::string("grenade-finish-")+state]={{"cue_order",compile(throwing.at(std::string("finish_if_auxiliary_")+(std::string(state)=="empty"?"empty":"item_remains")))}};}
    content.catalog["weapon_bindings"]["29"]={{"kind","grenade"},{"raise","grenade-raise"},{"lower","grenade-lower"},{"fire","grenade-throw"},{"aim",false}};
    for(int item:{12,27}){
        const auto& weapon=rules.at("weapons").at(std::to_string(item));
        nlohmann::json binding={{"kind",item==12?"sg":"lc9"},{"raise",weapon.at("raise_profile")},{"lower",weapon.at("lower_profile")},{"fire",weapon.at("fire_profile")},{"reload",weapon.at("reload").at("profile")},{"aim",weapon.at("input").at("sustained_aim")},{"flash",weapon.at("muzzle_presentation").at("flash_cue")},{"effect_prefix","muzzle-"},{"empty_sound",weapon.at("empty_trigger").at("otherwise_cue")}};
        if(item==12){binding["quick_fire"]=weapon.at("quick_fire").at("profile");content.expansion.reloadEquipment["12"]={{"equipment_item",51},{"profile","sg-reload-bag"}};}
        content.catalog["weapon_bindings"][std::to_string(item)]=std::move(binding);
    }
    for(int item:{12,27,29}){content.items.at(item).stateOnly=false;content.items.at(item).pendingActionTicket=0;}
}
