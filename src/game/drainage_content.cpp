#include "full_content.h"
#include <algorithm>
#include <stdexcept>
namespace{
using Json=nlohmann::json;
void need(bool v,const char* message){if(!v){throw std::runtime_error(message);}}
bool ready(const CombatContent& c,const Json& steps){
    for(const auto& step:steps){
        if(step.at("kind")=="relocate"&&c.pendingMapTickets.contains(step.at("destination").at("map").get<int>())){return false;}
        if(step.at("kind")=="branch"&&(!ready(c,step.at("then"))||!ready(c,step.at("else")))){return false;}
    }return true;
}
}
void installDrainageContent(CombatContent& c,const Json& evacuation){
    const auto& p=evacuation.at("runtime_profile");const auto& panel=p.at("drainage");
    need(p.at("schema")=="nobihaza.evacuation-profile.v1"&&panel.at("rows")==3&&panel.at("columns")==3&&panel.at("toggle_cells").size()==9,"Unsupported drainage profile");
    c.catalog["drainage"]=panel;
    need(c.cues.contains("m107-e5-p1-c256"),"Missing original drainage cursor cue");
    c.catalog["drainage"]["direction_sound"]="m107-e5-p1-c256";
    const std::string object=panel.at("object");
    c.catalog["flows"]["drainage-open"]={{"object",object},{"page",panel.at("page")},{"trigger","action"},{"steps",panel.at("opening")}};
    for(const auto* outcome:{"success","cancel"}){
        auto steps=Json::array();for(const auto& id:panel.at(std::string(outcome)+"_cues")){steps.push_back({{"kind","cue"},{"cue",id}});}
        if(std::string(outcome)=="success"){steps.push_back({{"kind","flag"},{"id",panel.at("complete_fact")},{"value",true}});}
        for(const auto& id:panel.at("lifecycle").at("restore_facts_on_exit")){steps.push_back({{"kind","flag"},{"id",id},{"value",true}});}
        c.catalog["flows"][std::string("drainage-")+outcome]={{"object",object},{"page",panel.at("page")},{"trigger","requested"},{"steps",steps}};
    }
    std::erase_if(c.pendingMapTickets,[](const auto& row){return row.second==101;});
    auto save=p.at("water").at("save");save["facing"]="up";save["source"]=p.at("water").at("source");installSourceSavePoint(c,save,evacuation.at("flows"));
    for(const auto& [id,source]:evacuation.at("flows").items()){
        if(source.at("implementation_ticket")!=101||id==save.at("flow").get<std::string>()||!ready(c,source.at("steps"))){continue;}
        auto flow=source;flow["runtime_enabled"]=true;
        // Reuse the existing one-shot skill normalization instead of interpreting an environment branch.
        if(id=="m107-e7-p1"){flow["steps"]=Json::array({{{"kind","enable_combine_if_known"},{"from_skill",4},{"to_skill",3}},{{"kind","erase_event_once"}}});}
        c.catalog["flows"][id]=std::move(flow);
    }
    // Truth's source exit branches become executable when their real water maps are delivered.
    for(const auto& [id,source]:c.full->at("runtime").at("truth").at("flows").items()){
        if(c.objects.at(source.at("object").get<std::string>()).map!=104||!ready(c,source.at("steps"))){continue;}
        auto flow=source;flow["runtime_enabled"]=true;c.catalog["flows"][id]=std::move(flow);
    }
    for(const auto& [id,row]:p.at("encounters").items()){
        need(row.at("implementation_ticket")==101&&row.at("type")=="spider"&&c.objects.at(id).map==row.at("map"),"Unbound water-map spider");
        auto profile=row.at("runtime_profile");auto rules=row.at("rules");
        // Media roles reuse the donor; the reviewed source slot owns both acid fact writes.
        profile["acid"]["prepare"]["commit_attack_fact"]=row.at("attack_switch");
        profile["acid"]["clear_attack_fact"]=row.at("attack_switch");
        rules["behavior"]="spider";for(const auto* key:{"normal_attributes","runtime_profile","activation_fact","attack_switch","source_slots","dead_fact","source_slot"}){rules[key]=row.at(key);}
        rules["runtime_profile"]=profile;
        rules["blood_effects"]=profile.at("blood_effects");for(const auto& effect:rules.at("blood_effects")){need(c.media.at("effects").contains(effect.get<std::string>()),"Missing water spider blood effect");}
        need(c.enemies.emplace(id,EnemyDefinition{id,row.at("map"),row.at("source_order"),row.at("dead_fact"),false,std::move(rules)}).second,"Duplicate water spider");
    }
    for(auto& [id,link]:c.transitions.items()){
        if(c.pendingMapTickets.contains(c.objects.at(link.at("object").get<std::string>()).map)||c.pendingMapTickets.contains(link.at("destination").at("map").get<int>())||!link.contains("flow")){continue;}
        const auto matching=std::find_if(c.catalog.at("flows").begin(),c.catalog.at("flows").end(),[&](const auto& f){return f.contains("object")&&f.contains("page")&&f.contains("steps")&&f.value("runtime_enabled",true)&&f.at("object")==link.at("object")&&f.at("page")==link.at("page")&&f.at("steps")==link.at("flow");});
        if(matching!=c.catalog.at("flows").end()){c.catalog["flows"][id]=*matching;link["runtime_enabled"]=true;link["handoff_only"]=false;}
    }
}
