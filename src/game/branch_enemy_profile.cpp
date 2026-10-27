#include "branch_content.h"
#include "combat_content.h"
#include <algorithm>
#include <stdexcept>
void validateBranchEnemyProfile(const CombatContent& c,const std::string& id,const nlohmann::json& profile){
        const auto& object=c.objects.at(id);const auto& map=c.maps.at(object.map);
        const auto cueRole=[&](const std::string& id,const std::string& kind){
            const auto& cue=c.cue(id);if(cue.contains("runtime_support")||cue.at("kind")!=kind||(kind=="forced_route"&&!c.routes.contains(id))){throw std::runtime_error("Invalid enemy cue role: "+id);}
            if(kind=="forced_route"){
                const auto& route=c.routes.at(id);if(route.target!=10005&&route.target!=object.variants.front().definition.sourceEventId){throw std::runtime_error("Invalid enemy route target");}
                for(const auto& step:route.steps){if(step.kind=="graphic"&&!map.sprites->contains(step.sprite)){throw std::runtime_error("Missing enemy route graphic");}}
            }
            if(kind=="animation"&&!c.media.at("animations").contains(cue.at("animation").get<std::string>())){throw std::runtime_error("Missing enemy animation");}
        };
        for(const auto& [role,cue]:profile.at("cues").items()){cueRole(cue,role.ends_with("_sound")?"sound":"forced_route");}
        for(const char* role:{"retreat_routes","collision_yield_routes","evade_routes"}){if(profile.contains(role)){for(const auto& cue:profile.at(role)){cueRole(cue,"forced_route");}}}
        if(profile.contains("terrain_routes")){for(const auto& routes:profile.at("terrain_routes")){for(const auto& cue:routes){cueRole(cue,"forced_route");}}}
        if(profile.contains("feeding")){for(const auto& stage:profile.at("feeding")){cueRole(stage.at("route"),"forced_route");if(stage.contains("sound")){cueRole(stage.at("sound"),"sound");}}}
        for(const char* role:{"head_start_route","head_final_route","body_start_route","body_final_route"}){if(profile.contains(role)){cueRole(profile.at(role),"forced_route");}}
        for(const char* role:{"grab_release_wait","head_delay","body_wait"}){cueRole(profile.at(role),"wait");}
        cueRole(profile.at("head_animation"),"animation");if(profile.contains("body_cancel")){cueRole(profile.at("body_cancel"),"cancel_routes");}cueRole(profile.at("retreat_flash"),"flash_character");

        for(const auto& [role,sprite]:profile.at("sprites").items()){if(!map.sprites->contains(sprite.get<std::string>())){throw std::runtime_error("Missing enemy graphic");}}
        for(const auto& [role,effect]:profile.at("blood_effects").items()){if(!c.media.at("effects").contains(effect.get<std::string>())){throw std::runtime_error("Missing enemy effect for "+id+": "+effect.get<std::string>());}}
        const auto page=[&](int id){if(std::none_of(object.variants.begin(),object.variants.end(),[&](const auto& v){return v.page==id;})){throw std::runtime_error("Missing enemy page");}};
        for(const auto& [role,value]:profile.items()){if(role.ends_with("_page")){page(value);}}
        if(profile.contains("corpse_pages")){for(int id:profile.at("corpse_pages")){page(id);}}
        if(profile.contains("fall")){
            const auto& f=profile.at("fall");page(f.at("page"));if(f.contains("rise_sound")){cueRole(f.at("rise_sound"),"sound");}cueRole(f.at("start_sound"),"sound");cueRole(f.at("wait"),"wait");
            for(const char* role:{"rise_route","grounded_hit_route","grounded_knockback_route"}){if(f.contains(role)){cueRole(f.at(role),"forced_route");}}
        }
}
