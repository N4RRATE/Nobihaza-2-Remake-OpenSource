#include "enemy_controller.h"
#include "full_content.h"
#include "combat_scene.h"
#include "zombie_behavior.h"
#include "cerberus_behavior.h"
#include "pursuer_behavior.h"
#include "spider_behavior.h"
#include "hangar_behavior.h"
#include "hunter_behavior.h"
#include "chimera_behavior.h"
#include <stdexcept>
std::unique_ptr<EnemyController> makeEnemyBehavior(CombatScene& scene,const EnemyDefinition& source){
    auto definition=source;EnemyHost host(scene,source.id);
    if(definition.story&&!definition.rules.contains("behavior")){
        auto rules=host.content().rules.at("enemy");if(!definition.rules.is_null()){rules.merge_patch(definition.rules);}definition.rules=std::move(rules);
    }
    if(const auto* patch=difficultyEnemyRules(host.content(),host.session(),definition.id)){definition.rules.merge_patch(*patch);}
    if(definition.rules.is_object()){
        const auto behavior=definition.rules.value("behavior",std::string{});
        if(behavior=="chimera"){return std::make_unique<ChimeraBehavior>(scene,definition);}
        if(behavior=="hunter"){return std::make_unique<HunterBehavior>(scene,definition);}
        if(behavior=="hangar"){return std::make_unique<HangarBehavior>(scene,definition);}
        if(behavior=="spider"){return std::make_unique<SpiderBehavior>(scene,definition);}
        if(behavior=="pursuer"){return std::make_unique<PursuerBehavior>(scene,definition);}
        if(behavior=="cerberus"){return std::make_unique<CerberusBehavior>(scene,definition);}
        if(!behavior.empty()&&behavior!="zombie"){throw std::runtime_error("Unknown enemy behavior: "+behavior);}
    }
    return std::make_unique<ZombieBehavior>(scene,definition);
}
#include "licker_behavior.h"
#include "combat_scene.h"
std::unique_ptr<EnemyController> makeEnemyBehavior(CombatScene& scene,const EnemyBlueprint& definition){
    if(definition.behavior==EnemyBehaviorKind::Licker){
        auto selected=definition;EnemyHost host(scene,definition.id);
        if(const auto* patch=difficultyEnemyRules(host.content(),host.session(),definition.id)){
            selected.attack=patch->at("attack");selected.defense=patch->at("defense");
            if(patch->contains("damage_sources")){
                if(selected.rules.is_null()){selected.rules=host.content().expansion.behaviorDefinitions.at("licker");}
                selected.rules["damage_sources"].merge_patch(patch->at("damage_sources"));
            }
        }
        return std::make_unique<LickerBehavior>(scene,selected);
    }
    const std::string name=definition.behavior==EnemyBehaviorKind::RunningZombie?"running-zombie":definition.behavior==EnemyBehaviorKind::FallingZombie?"falling-zombie":"soldier-zombie";
    EnemyHost host(scene,definition.id);auto parameters=host.content().rules.at("enemy");
    parameters["id"]=definition.id;parameters["bite_attack"]=definition.attack;parameters["defense"]=definition.defense;
    parameters["death_damage_threshold"]=definition.terminationDamage;
    parameters["runtime_profile"]=host.content().expansion.behaviorDefinitions.at(name).at("runtime_profile");
    return makeEnemyBehavior(scene,EnemyDefinition{definition.id,definition.map,definition.sourceOrder,definition.deadFlag,false,std::move(parameters)});
}
