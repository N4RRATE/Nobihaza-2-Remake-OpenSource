#include "full_content.h"
#include "content_hash.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
CombatContent loadDifficultyInitializationContent(const std::filesystem::path& path,const nlohmann::json& data){
    using Json=nlohmann::json;const auto need=[](bool ok,const char* message){if(!ok){throw std::runtime_error(message);}};
    need(data.size()==4&&data.at("schema")=="nobihaza.g8initialization.v1"&&data.at("content_compatibility")=="post-f5-g8-initialization-1","Unknown difficulty initialization identity");
    const auto& source=data.at("source");const auto& p=data.at("projection");
    need(source==Json{{"file","g8-g7-source.json"},{"sha256","08b4edb9c2ecd1b1823cab3e8c1d20cda1ff4a829a7fb3438a2a29a277178dc9"}},"Unknown difficulty predecessor");
    need(contentSha256(p.dump())=="e0e8c59db5a58f42d4e7c4c3ffdd66a1ef185c05918ade575ae303a455abe2a9","Unreviewed difficulty projection");
    const auto file=path.parent_path()/source.at("file").get<std::string>();
    need(!std::filesystem::is_symlink(file)&&std::filesystem::is_regular_file(file)&&std::filesystem::file_size(file)<=128*1024*1024,"Missing difficulty predecessor");
    std::ifstream input(file,std::ios::binary);const auto bytes=readContentBytes(input);need(contentSha256(bytes)==source.at("sha256").get<std::string>(),"Changed difficulty predecessor");
    std::istringstream in(bytes);auto old=std::make_shared<const CombatContent>(loadCombatContentDefinition(file,parseCombatContentDefinition(in)));
    auto c=*old;c.legacySource=old;c.compatibility=data.at("content_compatibility");c.catalog["opening"]=p.at("opening");c.catalog["difficulty"]=p;c.catalog["difficulty"].erase("opening");
    for(const auto& id:p.at("easy_preseed")){
        const int flag=id;const auto key="difficulty.easy-preset-"+std::to_string(flag);
        if(!c.facts.switches.contains(flag)){c.facts.switches.emplace(flag,key);}
    }
    // G8-01 exposes a controlled ordinary-save boundary; later region tickets widen it with profiles.
    for(auto& point:c.catalog.at("save_points")){if(point.at("object")=="m16-e5"){
        const std::string label=point.at("slot_label");for(const auto& name:{"EASY","NORMAL","HARD"}){auto text=label;const auto at=text.find("NORMAL");need(at!=std::string::npos,"Unknown early save label");text.replace(at,6,name);point["slot_label_by_difficulty"][name]=text;}
    }}
    for(const auto& stage:c.catalog.at("opening").at("stages")){if(stage.contains("operations")){for(const auto& op:stage.at("operations")){if(op.at("kind")=="message"){need(c.messages.contains(op.at("ref").get<std::string>()),"Missing difficulty message");}else if(op.at("kind")=="cue"){need(c.cues.contains(op.at("ref").get<std::string>()),"Missing difficulty cue");}}}}
    for(auto& [id,entry]:c.entries){initializeFullSession(c,entry.session);}
    return c;
}
bool isDifficultyPreset(const CombatContent& c,const GameSession& session,int flag){
    if(session.difficulty()!="easy"||!c.catalog.contains("difficulty")){return false;}
    const auto& flags=c.catalog.at("difficulty").at("easy_preseed");return std::find(flags.begin(),flags.end(),flag)!=flags.end();
}

const nlohmann::json* difficultyEnemyRules(const CombatContent& c,const GameSession& session,const std::string& id){
    if(!c.catalog.contains("difficulty")){return nullptr;}const auto& d=c.catalog.at("difficulty");
    if(!d.contains("enemy_rules")||!d.at("enemy_rules").contains(id)){return nullptr;}
    return &d.at("enemy_rules").at(id).at(session.difficulty());
}
CombatContent loadDifficultyFirstPartContent(const std::filesystem::path& path,const nlohmann::json& data){
    using Json=nlohmann::json;const auto need=[](bool ok,const char* message){if(!ok){throw std::runtime_error(message);}};
    need(data.size()==4&&data.at("schema")=="nobihaza.g8firstpart.v1"&&data.at("content_compatibility")=="post-f5-g8-first-part-2","Unknown first-part difficulty identity");
    const auto& source=data.at("source");const auto& p=data.at("projection");
    need(source==Json{{"file","g8-initialization-source.json"},{"sha256","ab6c47eb6a482d13f669e875bec20f8de7e3a5ac8bf1476fe4afe04b332d172d"}},"Unknown first-part predecessor");
    need(contentSha256(p.dump())=="44b3caab878bae5b4c6558833692c1e6329bdf4ef21791480e5132ec3bd13b42","Unreviewed first-part difficulty projection");
    const auto file=path.parent_path()/source.at("file").get<std::string>();
    need(!std::filesystem::is_symlink(file)&&std::filesystem::is_regular_file(file)&&std::filesystem::file_size(file)<=128*1024*1024,"Missing first-part predecessor");
    std::ifstream input(file,std::ios::binary);const auto bytes=readContentBytes(input);need(contentSha256(bytes)==source.at("sha256").get<std::string>(),"Changed first-part predecessor");
    std::istringstream in(bytes);auto old=std::make_shared<const CombatContent>(loadCombatContentDefinition(file,parseCombatContentDefinition(in)));
    auto c=*old;c.legacySource=old;c.compatibility=data.at("content_compatibility");
    c.catalog["difficulty"]["enemy_rules"]=p.at("enemy_rules");c.catalog["difficulty"]["reload_profiles"]=p.at("reload_profiles");
    c.catalog["difficulty"]["controller_rules"]=p.at("controller_rules");c.catalog["difficulty"]["rescue"]=p.at("rescue");
    // A new world capability must bind every instantiated enemy family before it opens.
    const auto inRegion=[&](int map){return std::find(p.at("maps").begin(),p.at("maps").end(),map)!=p.at("maps").end();};
    for(const auto& [id,enemy]:c.enemies){if(inRegion(enemy.map)){need(p.at("enemy_rules").contains(id),"Unbound first-part enemy definition");}}
    for(const auto& [id,enemy]:c.expansion.enemies){if(inRegion(enemy.map)){need(p.at("enemy_rules").contains(id),"Unbound first-part enemy blueprint");}}

    for(const auto& name:{"easy","hard"}){c.catalog["difficulty"]["support"]["world"][name]=p.at("maps");c.catalog["difficulty"]["support"]["ordinary"][name]=p.at("save_objects");}
    for(auto& point:c.catalog.at("save_points")){if(p.at("save_labels").contains(point.at("object").get<std::string>())){
        const auto& labels=p.at("save_labels").at(point.at("object").get<std::string>());need(point.at("slot_label")==labels.at("NORMAL"),"Changed inherited first-part save label");point["slot_label_by_difficulty"]=labels;
    }}
    for(const auto& [id,alias]:p.at("object_aliases").items()){
        auto& object=c.objects.at(id);const auto& original=c.objects.at(alias.at("source").get<std::string>());
        for(int page:alias.at("pages")){need(std::none_of(object.variants.begin(),object.variants.end(),[&](const auto& v){return v.page==page;}),"Duplicate shared door page");const auto found=std::find_if(original.variants.begin(),original.variants.end(),[&](const auto& v){return v.page==page;});need(found!=original.variants.end(),"Missing shared door page");auto variant=*found;variant.definition.id=id;object.variants.push_back(std::move(variant));}
    }
    for(const auto& [id,alias]:p.at("flow_aliases").items()){need(!c.catalog.at("flows").contains(id),"Duplicate shared door flow");auto flow=c.catalog.at("flows").at(alias.get<std::string>());flow["object"]=id.substr(0,id.rfind("-p"));c.catalog["flows"][id]=std::move(flow);}
    for(const auto& [id,cue]:p.at("cues").items()){need(!c.cues.contains(id),"Duplicate first-part cue");c.setMediaDefinition("cues",id,cue);if(cue.at("kind")=="forced_route"){c.routes.emplace(id,readRouteDefinition(cue));}}
    for(const auto& [id,profile]:p.at("actions").items()){need(!c.actionProfiles.contains(id),"Duplicate first-part action");for(const auto& cue:profile.at("cue_order")){need(c.cues.contains(cue.get<std::string>()),"Missing first-part action cue");}c.actionProfiles[id]=profile;}
    // Keep old sequences admitted for verified legacy direct saves; new actions use source S17 guards.
    for(const auto& value:p.at("reload_guard_profiles")){
        const std::string id=value,alias="g8-guarded-"+id;auto profile=c.actionProfiles.at(id);Json sequence=Json::array();
        for(const auto& cue:profile.at("cue_order")){sequence.push_back(cue);if(c.cue(cue.get<std::string>()).at("kind")=="forced_route"){sequence.push_back("reload-guard");}}
        sequence.push_back("reload-finish");profile["cue_order"]=std::move(sequence);need(c.actionProfiles.emplace(alias,std::move(profile)).second,"Duplicate guarded reload");c.catalog["difficulty"]["reload_aliases"][id]=alias;
    }
    for(const auto& [id,shape]:p.at("acid_shapes").items()){auto& hit=c.enemies.at(id).rules["runtime_profile"]["acid"]["hit_rule"];hit.erase("signed_screen_sum_bounds");hit["screen_axis_bounds"]=shape;}
    for(auto& [id,entry]:c.entries){initializeFullSession(c,entry.session);}
    return c;
}

const nlohmann::json* difficultyControllerRules(const CombatContent& c,const GameSession& session,const std::string& id){
    if(!c.catalog.contains("difficulty")){return nullptr;}const auto& d=c.catalog.at("difficulty");
    if(!d.contains("controller_rules")||!d.at("controller_rules").contains(id)){return nullptr;}
    return &d.at("controller_rules").at(id).at(session.difficulty());
}

CombatContent loadDifficultyRegionContent(const std::filesystem::path& path,const nlohmann::json& data){
    using Json=nlohmann::json;const auto need=[](bool ok,const char* message){if(!ok){throw std::runtime_error(message);}};
    need(data.size()==4&&data.at("schema")=="nobihaza.g8region.v1"&&data.at("content_compatibility")=="post-f5-g8-region-3","Unknown region difficulty identity");
    const auto& source=data.at("source");const auto& p=data.at("projection");
    need(source==Json{{"file","g8-first-part-source.json"},{"sha256","56d1204486a31d0845fa6c7f6b294d918454010b40acdb8364560ff08b1a3893"}},"Unknown region difficulty predecessor");
    const auto projectionHash=contentSha256(p.dump());
    need(projectionHash=="1eb41c756b6bc1d80a8b4b91980ca199f9be0332ea3ebb40d96a77a1cd697acb"||projectionHash=="3e72422b21536d2bac39a9de5f26fe2f3926f1d94830a094ea720fa888237ada","Unreviewed region difficulty projection");
    const auto file=path.parent_path()/source.at("file").get<std::string>();
    need(!std::filesystem::is_symlink(file)&&std::filesystem::is_regular_file(file)&&std::filesystem::file_size(file)<=128*1024*1024,"Missing region predecessor");
    std::ifstream input(file,std::ios::binary);const auto bytes=readContentBytes(input);need(contentSha256(bytes)==source.at("sha256").get<std::string>(),"Changed region predecessor");
    std::istringstream in(bytes);auto old=std::make_shared<const CombatContent>(loadCombatContentDefinition(file,parseCombatContentDefinition(in)));
    auto c=*old;c.legacySource=old;c.compatibility=data.at("content_compatibility");
    for(const auto& [id,rules]:p.at("enemy_rules").items()){need(!c.catalog.at("difficulty").at("enemy_rules").contains(id),"Duplicate region enemy rule");c.catalog["difficulty"]["enemy_rules"][id]=rules;}
    // The old NORMAL projection froze Actor19's level sample; V280 now derives from Session.
    if(p.contains("level_sample")){
        const auto& sample=p.at("level_sample");
        for(const auto& id:sample.at("flows")){
            auto& steps=c.catalog.at("flows").at(id.get<std::string>()).at("steps");
            const auto matches=[&](const Json& step){return step.value("kind",std::string{})=="domain_value"&&step.value("field",std::string{})==sample.at("field").get<std::string>();};
            need(std::count_if(steps.begin(),steps.end(),matches)==1,"Missing inherited encounter level sample");
            const auto at=std::find_if(steps.begin(),steps.end(),matches);
            need(at->at("operation")=="set"&&at->at("value")==sample.at("legacy_value")&&at->at("source").at("location")==sample.at("legacy_location"),"Changed inherited encounter level sample");
            steps.erase(at);
        }
    }
    c.enemies.at("m120-e13").rules["passive_gas"]=p.at("hangar").at("passive");
    need(c.enemies.at("m120-e13").rules.at("attack")==p.at("hangar").at("attack")&&c.enemies.at("m120-e13").rules.at("receiver_defense")==p.at("hangar").at("receiver_defense"),"Changed original hangar attributes");
    const auto inRegion=[&](int map){return std::find(p.at("maps").begin(),p.at("maps").end(),map)!=p.at("maps").end();};
    for(const auto& [id,enemy]:c.enemies){if(inRegion(enemy.map)&&id!="m120-e13"){need(p.at("enemy_rules").contains(id),"Unbound region enemy");}}
    for(const auto& [id,enemy]:c.expansion.enemies){if(inRegion(enemy.map)){need(p.at("enemy_rules").contains(id),"Unbound region enemy blueprint");}}
    for(const auto& name:{"easy","hard"}){
        for(const auto& map:p.at("maps")){c.catalog["difficulty"]["support"]["world"][name].push_back(map);}
        for(const auto& [object,labels]:p.at("save_labels").items()){c.catalog["difficulty"]["support"]["ordinary"][name].push_back(object);}
        c.catalog["difficulty"]["support"]["chapter"].push_back(name);
    }
    for(auto& point:c.catalog.at("save_points")){const std::string id=point.at("object");if(p.at("save_labels").contains(id)){
        const auto& labels=p.at("save_labels").at(id);need(point.at("slot_label")==labels.at("NORMAL"),"Changed inherited region save label");point["slot_label_by_difficulty"]=labels;
    }}
    need(c.catalog.at("chapter").at("name")==p.at("chapter_names").at("NORMAL"),"Changed inherited Chapter label");
    c.catalog["chapter"]["name_by_difficulty"]=p.at("chapter_names");
    const auto addNames=[&](auto&& self,Json& value)->void{
        if(value.is_object()){
            if(value.value("kind",std::string{})=="actor_display"&&value.at("name")==p.at("chapter_names").at("NORMAL")){value["name_by_difficulty"]=p.at("chapter_names");}
            for(auto& child:value){self(self,child);}
        }else if(value.is_array()){for(auto& child:value){self(self,child);}}
    };
    addNames(addNames,c.catalog["flows"]);addNames(addNames,c.catalog["chapter"]);
    for(auto& [id,entry]:c.entries){initializeFullSession(c,entry.session);}
    return c;
}

CombatContent loadDifficultyLateContent(const std::filesystem::path& path,const nlohmann::json& data){
    using Json=nlohmann::json;const auto need=[](bool ok,const char* message){if(!ok){throw std::runtime_error(message);}};
    need(data.size()==4&&data.at("schema")=="nobihaza.g8late.v1"&&data.at("content_compatibility")=="post-f5-g8-late-4","Unknown late difficulty identity");
    const auto& source=data.at("source");const auto& p=data.at("projection");
    need(source==Json{{"file","g9.json"},{"sha256","76a98613f956a3727abf15b78545b7f48f57cae70dd2a7f36acd068929087718"}},"Unknown late difficulty predecessor");
    need(contentSha256(p.dump())=="74a51a7f39be71739df08ef1156606e49003ef9c078a7c0f9e3269834aa743e2","Unreviewed late difficulty projection");
    const auto file=path.parent_path()/source.at("file").get<std::string>();
    need(!std::filesystem::is_symlink(file)&&std::filesystem::is_regular_file(file)&&std::filesystem::file_size(file)<=128*1024*1024,"Missing late difficulty predecessor");
    std::ifstream input(file,std::ios::binary);const auto bytes=readContentBytes(input);need(contentSha256(bytes)==source.at("sha256").get<std::string>(),"Changed late difficulty predecessor");
    std::istringstream in(bytes);auto old=std::make_shared<const CombatContent>(loadCombatContentDefinition(file,parseCombatContentDefinition(in)));
    auto c=*old;c.legacySource=old;c.compatibility=data.at("content_compatibility");
    auto& difficulty=c.catalog.at("difficulty");
    for(const auto& [id,rules]:p.at("enemy_rules").items()){need(!difficulty.at("enemy_rules").contains(id),"Duplicate late enemy rules");difficulty["enemy_rules"][id]=rules;}
    for(const auto& [id,rules]:p.at("controller_rules").items()){need(!difficulty.at("controller_rules").contains(id),"Duplicate late controller rules");difficulty["controller_rules"][id]=rules;}
    for(const auto& preset:p.at("easy_absent")){
        const auto id=preset.at("object").get<std::string>();auto& enemy=c.enemies.at(id);enemy.rules["death_tile"]=preset.at("tile");
        const int x=preset.at("tile").at(0),y=preset.at("tile").at(1);const auto& map=c.maps.at(enemy.map);
        // CN1.00 M168 is 15 tiles high; its terminal corpse parks at y34 (margin20).
        const int margin=std::max({0,-x,-y,x-map.width+1,y-map.height+1});need(margin<=20,"Late corpse placement exceeds the source bound");
        for(auto& variant:c.objects.at(id).variants){variant.definition.outsideMapMargin=std::max(variant.definition.outsideMapMargin,margin);}
    }
    const auto inRegion=[&](int map){return std::find(p.at("maps").begin(),p.at("maps").end(),map)!=p.at("maps").end();};
    for(const auto& [id,enemy]:c.enemies){if(inRegion(enemy.map)){need(p.at("enemy_rules").contains(id),"Unbound late enemy definition");}}
    for(const auto& [id,enemy]:c.expansion.enemies){if(inRegion(enemy.map)){need(p.at("enemy_rules").contains(id),"Unbound late enemy blueprint");}}
    for(const auto& name:{"easy","hard"}){
        for(int map:p.at("maps")){need(c.maps.contains(map),"Late capability names an undelivered map");difficulty["support"]["world"][name].push_back(map);}
        for(const auto& point:p.at("ordinary")){difficulty["support"]["ordinary"][name].push_back(point);}
    }
    difficulty["support"]["direct"]=p.at("direct");difficulty["support"]["clear"]=p.at("clear");
    for(auto& [id,entry]:c.entries){initializeFullSession(c,entry.session);}
    return c;
}
