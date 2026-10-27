#include "full_content.h"
#include "branch_content.h"
#include <algorithm>
#include <array>
#include <stdexcept>
namespace{
using Json=nlohmann::json;
void need(bool ok,const char* message){if(!ok){throw std::runtime_error(message);}}
const ObjectVariant& page(const CombatContent& c,const std::string& object,int number){
    const auto& variants=c.objects.at(object).variants;
    const auto found=std::find_if(variants.begin(),variants.end(),[&](const auto& p){return p.page==number;});
    need(found!=variants.end(),"Missing underground source page");return *found;
}
std::string lightActor(const CombatContent& c,int map,const std::string& sample){
    if(sample=="player"){return sample;}
    const std::string prefix="map-object-";need(sample.starts_with(prefix),"Unsupported underground light anchor");
    const int event=std::stoi(sample.substr(prefix.size()));std::string id;
    for(const auto& [key,object]:c.objects){if(object.map==map&&!object.variants.empty()&&object.variants.front().definition.sourceEventId==event){need(id.empty(),"Ambiguous underground light anchor");id=key;}}
    need(!id.empty(),"Unbound underground light anchor");return id;
}
Json compileLight(const CombatContent& c,const Json& row){
    const std::string object=row.at("object");const auto& steps=row.at("steps");
    page(c,object,row.at("page"));need(steps.size()==10&&row.at("implementation_ticket")==96,"Unsupported underground light shape");
    const std::string anchor=steps.at(0).at("entity");
    for(int i=0;i<3;++i){need(steps.at(i).at("kind")=="scene_sample"&&steps.at(i).at("entity")==anchor,"Inconsistent underground light sampling");}
    need(steps.at(0).at("component")=="screen_x"&&steps.at(1).at("component")=="screen_y"&&steps.at(2).at("component")=="facing","Unsupported underground light components");
    const auto x=steps.at(0).at("field"),y=steps.at(1).at("field"),facing=steps.at(2).at("field");
    const auto& changed=steps.at(7);const auto& condition=changed.at("condition");
    need(changed.at("kind")=="branch"&&condition.at("kind")=="domain_compare"&&condition.at("field")==facing&&condition.at("operator")=="not_equal"&&changed.at("else").empty(),"Unsupported underground light direction cache");
    Json directions=Json::object();const std::array<std::pair<int,const char*>,4> values{{{2,"down"},{4,"left"},{6,"right"},{8,"up"}}};
    for(int i=0;i<4;++i){
        const auto& offset=steps.at(3+i);const auto& compare=offset.at("condition");const auto [value,name]=values.at(i);
        need(offset.at("kind")=="branch"&&compare.at("kind")=="domain_compare"&&compare.at("field")==facing&&compare.at("operator")=="equal"&&compare.at("operand").at("constant")==value&&offset.at("else").empty(),"Unsupported underground light offset branch");
        std::array<int,2> xy{};
        for(const auto& operation:offset.at("then")){
            const auto field=operation.at("field");const auto kind=operation.at("operation");
            need(operation.at("kind")=="domain_value"&&(field==x||field==y)&&(kind=="add"||kind=="subtract"),"Unsupported underground light offset");
            xy[field==x?0:1]+=(kind=="add"?1:-1)*operation.at("value").get<int>();
        }
        const auto& change=changed.at("then").at(i);const auto& test=change.at("condition");const auto& show=change.at("then");
        need(change.at("kind")=="branch"&&test.at("field")==facing&&test.at("operator")=="equal"&&test.at("operand").at("constant")==value&&change.at("else").empty()&&show.size()==3,"Unsupported underground light graphic branch");
        need(show.at(2).at("kind")=="domain_value"&&show.at(2).at("field")==condition.at("operand").at("field")&&show.at(2).at("operation")=="set"&&show.at(2).at("value")==value,"Unsupported underground light cache commit");
        Json cues=Json::array();for(int n=0;n<2;++n){need(show.at(n).at("kind")=="cue","Unsupported underground light picture");const auto id=show.at(n).at("cue").get<std::string>();need(c.cue(id).at("kind")=="show_picture","Missing underground light picture");cues.push_back(id);}
        directions[name]={{"offset_pixels",xy},{"show_cues",cues}};
    }
    Json moves=Json::array();for(int i=8;i<10;++i){const auto id=steps.at(i).at("cue").get<std::string>();need(steps.at(i).at("kind")=="cue"&&c.cue(id).at("kind")=="move_picture","Missing underground light movement");moves.push_back(id);}
    return {{"object",object},{"page",row.at("page")},{"actor",lightActor(c,c.objects.at(object).map,anchor)},{"directions",directions},{"move_cues",moves},{"source",row.at("source")}};
}
void saveNames(const Json& steps,std::map<std::string,std::string>& names,int depth=0){
    need(depth<8,"Underground save metadata is too deep");
    for(const auto& step:steps){const auto kind=step.at("kind");
        if(kind=="save_location_name"){
            need(step.at("actor")==1,"Unsupported underground save identity");const std::string name=step.at("value");
            for(const auto* difficulty:{"EASY","NORMAL","HARD"}){if(name.find(std::string("/")+difficulty+"/")!=std::string::npos){need(names.emplace(difficulty,name).second,"Duplicate underground save label");}}
        }else if(kind=="branch"){saveNames(step.at("then"),names,depth+1);saveNames(step.at("else"),names,depth+1);}
    }
}
void validateEnvironment(const CombatContent& c,const Json& flow){
    for(const auto& step:flow.at("steps")){
        const std::string kind=step.at("kind");
        if(kind=="cue"){need(c.cues.contains(step.at("cue").get<std::string>()),"Missing underground environment cue");}
        else if(kind=="flag"){need(c.facts.switches.contains(step.at("id").get<int>())&&step.at("value").is_boolean(),"Invalid underground environment fact");}
        else if(kind=="combine_skill"){const int skill=step.at("skill");need(step.at("actor").get<int>()>0&&(skill==3||skill==4)&&c.skills.contains(skill)&&step.at("learn").is_boolean(),"Invalid underground environment skill");}
        else if(kind=="branch"){
            const auto& condition=step.at("condition");
            need(condition.at("kind")=="fact"&&c.facts.switches.contains(condition.at("id").get<int>())&&condition.at("value").is_boolean(),"Unsupported underground environment condition");
            for(const auto* arm:{"then","else"}){
                const auto& body=step.at(arm);
                need(body.size()==1&&body.at(0).at("kind")=="cue"&&c.cue(body.at(0).at("cue").get<std::string>()).at("kind")=="bgm","Unsupported underground environment music branch");
            }
        }else{need(kind=="erase_event_once","Unsupported underground environment operation");}
    }
}
void validateBodyCache(const CombatContent& c,const std::string& id,const Json& row){
    const auto& profile=row.at("runtime_profile");
    if(!profile.contains("body_cache")){return;}
    const auto& cache=profile.at("body_cache");const std::string manager=cache.at("source_object");
    need(c.objects.contains(manager)&&c.objects.at(manager).map==c.objects.at(id).map,"Unbound underground canine body manager");
    need(cache.at("register_event")==c.objects.at(id).variants.front().definition.sourceEventId,"Invalid underground canine body event");
    for(const auto& number:cache.at("source_pages")){page(c,manager,number);}
    need(cache.at("source_pages").size()==3,"Invalid underground canine manager pages");
    // CE3's eight source workspaces are visit-local scratch, deliberately absent
    // from persistent facts. Validate the exact declared slot instead of promoting it.
    const auto& slots=row.at("source_slots");const int slot=row.at("source_slot"),base=slots.at("base");
    need(slot>=1&&slot<=8&&base==81+14*(slot-1),"Unbound underground canine body slot");
    need(cache.at("register_variable")==slots.at("event_id")&&slots.at("event_id")==base+9,"Unbound underground canine body variable");
    need(cache.at("tile_variables")==Json::array({base,base+1})&&cache.at("screen_variables")==Json::array({base+2,base+3})&&cache.at("half_variables")==Json::array({base+6,base+7}),"Unbound underground canine body variable");
    need(cache.at("death_clear_screen")==cache.at("screen_variables"),"Invalid underground canine death clearing");
    const auto& offset=cache.at("center_offset_pixels");need(offset.is_array()&&offset.size()==2&&offset.at(0).is_number_integer()&&offset.at(1).is_number_integer(),"Invalid underground canine body offset");
    for(const auto* name:{"roam","pursuit"}){
        const auto pageNumber=std::to_string(profile.at("page_roles").at(name).at("page").get<int>());
        const auto& half=cache.at("half_pixels_by_page").at(pageNumber);
        need(half.is_array()&&half.size()==2&&half.at(0).is_number_integer()&&half.at(1).is_number_integer()&&half.at(0)>0&&half.at(1)>0,"Invalid underground canine body half size");
    }
    for(const auto* name:{"normal_death","head_death"}){
        const auto& source=cache.at("death_clear_source").at(name);
        need(source.at("page")==profile.at("page_roles").at(name).at("page")&&source.at("commands").size()==2,"Invalid underground canine death source");
    }
}
}
void installUndergroundContent(CombatContent& c,const nlohmann::json& underground){
    const auto& ordinary=underground.at("ordinary");
    for(const auto& [id,cue]:ordinary.at("presentation_cues").items()){
        need(cue.at("kind")=="show_picture"||cue.at("kind")=="move_picture"||cue.at("kind")=="erase_picture"||cue.at("kind")=="wait","Unsupported underground blood presentation cue");
        need(c.cues.emplace(id,std::make_shared<const Json>(cue)).second,"Conflicting underground presentation cue");
    }
    for(const auto& [id,effect]:ordinary.at("effects").items()){
        need(c.media.at("images").contains(effect.at("image").get<std::string>()),"Missing underground blood image");
        const auto& offsets=effect.at("stage_offsets_px");const auto& waits=effect.at("wait_zero_count_after_stage");const auto& tones=effect.at("stage_tone_percent");
        need(offsets.size()==4&&waits.size()==offsets.size()&&tones.size()==offsets.size(),"Invalid underground blood stages");
        for(const auto& tone:tones){need(tone.size()==4,"Invalid underground blood tone");for(const auto& value:tone){need(value.is_number_integer()&&value>=0&&value<=200,"Unsupported underground blood tone value");}}
        for(const auto& wait:waits){need(wait.is_number_integer()&&wait>0&&wait<=60,"Invalid underground blood wait");}
        need(c.media["effects"].emplace(id,effect).second,"Conflicting underground blood effect");
    }
    for(const auto& [id,row]:underground.at("encounters").items()){
        need(row.at("object")==id&&c.objects.at(id).map==row.at("map"),"Unbound underground encounter");
        const std::string type=row.at("type");need(type=="pursuer"||type=="zombie"||type=="dog"||type=="spider","Unsupported underground encounter type");
        // Earlier cumulative packages retain unimplemented typed rows and their map gate.
        if(!row.contains("runtime_profile")||(type=="dog"&&!row.at("runtime_profile").contains("page_roles"))||(type=="spider"&&!row.at("runtime_profile").contains("pages"))){continue;}
        const auto& profile=row.at("runtime_profile");Json rules;
        if(type=="pursuer"){
            if(!row.contains("stop_fact")){continue;}
            rules={{"behavior","pursuer"},{"attack",row.at("normal_attributes").at("attack")},{"defense",row.at("normal_attributes").at("defense")},{"activation_fact",row.at("activation_fact")},{"stop_fact",row.at("stop_fact")},{"completion_flow",row.at("completion_flow")},{"hitbox_half_pixels",{8,15}},{"runtime_profile",profile}};
        }else if(type=="zombie"){
            validateBranchEnemyProfile(c,id,profile);rules=c.rules.at("enemy");rules["runtime_profile"]=profile;
            rules["bite_attack"]=row.at("normal_attributes").at("attack");rules["defense"]=row.at("normal_attributes").at("defense");rules["death_damage_threshold"]=row.at("termination_damage_threshold");rules["activation_fact"]=row.at("activation_fact");
            rules["grab_max_bites"]=row.at("grab").at("maximum_damage_cycles");rules["grab_escape_direction_inputs"]=row.at("grab").at("escape_input_count");rules["grab_bite_wait_tenths"]=row.at("grab").at("wait_tenths");
        }else{
            rules=row.at("rules");rules["behavior"]=type=="dog"?"cerberus":"spider";rules["normal_attributes"]=row.at("normal_attributes");rules["runtime_profile"]=profile;rules["activation_fact"]=row.at("activation_fact");rules["attack_switch"]=row.at("attack_switch");
            if(type=="spider"){rules["blood_effects"]=profile.at("blood_effects");for(const auto& [kind,effect]:rules.at("blood_effects").items()){need(c.media.at("effects").contains(effect.get<std::string>()),"Missing underground spider blood effect");}}
            if(type=="dog"){
                validateBodyCache(c,id,row);
                for(const auto* key:{"page_roles","cue_commands","retained_defense_on_blow","blow_blood_type","blood_effects","grab_start_sound_cue"}){if(profile.contains(key)){rules[key]=profile.at(key);}}
                for(const auto& [name,role]:rules.at("page_roles").items()){page(c,id,role.at("page"));for(const auto& cue:role.at("cues")){need(c.cues.contains(cue.at("id").get<std::string>()),"Missing underground canine cue");}}
                for(const auto& [kind,effect]:rules.at("blood_effects").items()){need(c.media.at("effects").contains(effect.get<std::string>()),"Missing underground canine blood effect");}
            }
        }
        for(const auto* key:{"source_slots","dead_fact","source_slot"}){if(row.contains(key)){rules[key]=row.at(key);}}
        const int dead=type=="pursuer"?row.at("completion_flag").get<int>():row.at("dead_fact").get<int>();
        need(c.enemies.emplace(id,EnemyDefinition{id,row.at("map"),row.at("source_order"),dead,false,std::move(rules)}).second,"Duplicate underground encounter");
    }
    const auto& flows=underground.at("flows");const auto& save=underground.at("legal_save");const std::string saveObject=save.at("object");
    for(const auto& [id,row]:flows.items()){
        if(row.at("implementation_ticket")!=96||row.at("object")==saveObject){continue;}
        if(row.at("trigger")=="environment"){validateEnvironment(c,row);}
        page(c,row.at("object"),row.at("page"));auto& installed=c.catalog.at("flows").at(id);
        need(installed.at("object")==row.at("object")&&installed.at("page")==row.at("page"),"Underground flow binding differs");installed["runtime_enabled"]=true;
    }
    // Conversion areas may request a source page on a different event.
    for(const auto& [id,row]:flows.items()){
        const auto& steps=row.at("steps");const int ticket=row.at("implementation_ticket");
        if((ticket!=96&&ticket!=97)||row.at("trigger")!="contact"||steps.size()!=2||steps.at(0).at("kind")!="remove"||steps.at(1).at("kind")!="grant"){continue;}
        const int from=steps.at(0).at("item"),to=steps.at(1).at("item");const std::string origin=row.at("object");const auto& original=c.objects.at(origin);
        Json uses=Json::object();std::string target;std::map<int,bool> requestFlags;
        for(const auto& [key,candidate]:flows.items()){
            if(candidate.at("trigger")!="autorun"||c.objects.at(candidate.at("object").get<std::string>()).map!=original.map){continue;}
            const bool removes=std::any_of(candidate.at("steps").begin(),candidate.at("steps").end(),[&](const auto& step){return step.at("kind")=="remove"&&step.at("item")==to;});
            if(!removes){continue;}const std::string object=candidate.at("object");need(target.empty()||target==object,"Ambiguous underground tool target");
            const auto& usePage=page(c,object,candidate.at("page"));
            if(target.empty()){requestFlags=usePage.flags;}else{std::erase_if(requestFlags,[&](const auto& flag){const auto found=usePage.flags.find(flag.first);return found==usePage.flags.end()||found->second!=flag.second;});}
            target=object;need(uses.emplace(std::to_string(usePage.page),key).second,"Duplicate underground tool source page");
        }
        if(target.empty()){continue;}
        const auto& inactive=page(c,origin,row.at("page"));need(inactive.inventoryItem==from,"Missing underground tool inventory page");
        int usablePage=inactive.page;for(const auto& candidate:original.variants){if(candidate.inventoryItem==to){usablePage=candidate.page;}}
        need(requestFlags.size()==1&&requestFlags.begin()->second,"Missing underground tool request flag");
        const auto& complete=original.variants.back();need(complete.flags.size()==1&&complete.flags.begin()->second,"Missing underground tool completion flag");
        Json exits=Json::array();for(const auto& [key,exit]:flows.items()){
            const auto& actions=exit.at("steps");const auto& object=c.objects.at(exit.at("object").get<std::string>());
            if(object.map==original.map&&exit.at("trigger")=="contact"&&actions.size()==2&&actions.at(0).at("kind")=="remove"&&actions.at(1).at("kind")=="grant"&&actions.at(0).at("item")==to&&actions.at(1).at("item")==from){exits.push_back({object.tile.x,object.tile.y});}
        }
        need(!exits.empty()&&c.items.contains(from)&&c.items.contains(to),"Incomplete underground tool binding");
        const std::string useId=uses.begin().value();const int effectivePage=std::stoi(uses.begin().key());
        c.catalog["context_tools"].push_back({{"map",original.map},{"object",target},{"origin_object",origin},{"origin_requires_inactive_item",usablePage==inactive.page},{"request_origin_page",usablePage},{"inactive_origin_page",inactive.page},{"request_effective_page",effectivePage},{"inactive_item",from},{"usable_item",to},{"use_tile",{original.tile.x,original.tile.y}},{"exit_tiles",exits},{"request_switch",requestFlags.begin()->first},{"completed_switch",complete.flags.begin()->first},{"flow",useId},{"flows_by_page",uses}});
    }
    installSourceSavePoint(c,save,flows);
    installSourceLighting(c,underground.at("lighting"));
    for(auto it=c.pendingMapTickets.begin();it!=c.pendingMapTickets.end();){
        const bool enemiesReady=std::all_of(underground.at("encounters").begin(),underground.at("encounters").end(),[&](const auto& row){const std::string id=row.at("object");return row.at("map")!=it->first||c.enemies.contains(id)||c.expansion.enemies.contains(id);});
        if(it->second==96&&enemiesReady){it=c.pendingMapTickets.erase(it);}else{++it;}
    }
}

void installSourceSavePoint(CombatContent& c,const Json& save,const Json& flows){
    const std::string saveObject=save.at("object");
    need(save.at("profile")=="f3.save_flow"&&save.at("party")==Json::array({1}),"Unsupported underground save policy");
    for(const auto& number:save.at("pages")){
        page(c,saveObject,number);const auto found=std::find_if(flows.begin(),flows.end(),[&](const auto& row){return row.at("object")==saveObject&&row.at("page")==number;});need(found!=flows.end(),"Missing underground save metadata");
        std::map<std::string,std::string> names;saveNames(found->at("steps"),names);need(names.size()==3,"Incomplete underground save labels");
        const auto& normal=names.at("NORMAL");const auto location=normal.substr(normal.rfind('/')+1);
        c.catalog["save_points"].push_back({{"object",saveObject},{"page",number},{"facing",save.at("facing")},{"location",location},{"slot_label",normal},{"slot_label_by_difficulty",names},{"allowed_party",save.at("party")},{"source",save.at("source")}});
    }
}

void installSourceLighting(CombatContent& c,const Json& lighting){for(const auto& [id,row]:lighting.items()){need(!c.catalog["underground_lighting"].contains(id),"Duplicate source lighting");c.catalog["underground_lighting"][id]=compileLight(c,row);}}
