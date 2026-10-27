#include "full_content.h"
#include "content_hash.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
using Json=nlohmann::json;
namespace{
void need(bool ok,const char* why){if(!ok){throw std::runtime_error(why);}}
std::string verifiedFile(const std::filesystem::path& directory,const std::string& name,const std::string& hash){
    need(std::filesystem::path(name).filename()==name,"Invalid G1 resource path");const auto path=directory/name;
    need(!std::filesystem::is_symlink(path)&&std::filesystem::is_regular_file(path)&&std::filesystem::file_size(path)<=128*1024*1024,"Missing or oversized G1 resource");
    std::ifstream in(path,std::ios::binary);need(in.is_open(),"Cannot read G1 resource");std::string bytes=readContentBytes(in);
    need(contentSha256(bytes)==hash,"Changed G1 source or media");return bytes;
}
}
static CombatContent loadG1Definition(const std::filesystem::path& path,const Json& data,std::shared_ptr<const CombatContent> old={}){
    const bool monitor=data.at("content_compatibility")=="post-f5-g1-normal-9";
    const bool piano=monitor||data.at("content_compatibility")=="post-f5-g1-normal-8";
    const bool mirror=piano||data.at("content_compatibility")=="post-f5-g1-normal-7";
    const bool puzzles=mirror||data.at("content_compatibility")=="post-f5-g1-normal-6";
    const bool kitchen=puzzles||data.at("content_compatibility")=="post-f5-g1-normal-5";
    const bool hunters=kitchen||data.at("content_compatibility")=="post-f5-g1-normal-4";
    const bool wax=hunters||data.at("content_compatibility")=="post-f5-g1-normal-3";
    const bool stone=data.at("content_compatibility")=="post-f5-g1-normal-2"||wax;
    need(data.size()==4&&data.at("schema")=="nobihaza.g1.v1"&&(data.at("content_compatibility")=="post-f5-g1-normal-1"||stone),"Unknown G1 content identity");
    const auto& source=data.at("source");
    need(source.size()==2&&source.at("file")=="g1-g0-source.json"&&source.at("sha256")=="b77bbcc8ed6853f11f5e7a9859d63cc0b56d00f121b5df4c23842b69f35b2c39","Unknown G1 source contract");
    const auto& p=data.at("projection");
    need(contentSha256(p.dump())==(monitor?"aa760b35dc0823e0231f9d68a55a92f74fdb6e4da40fb5bdf3a8126621f0dd0a":piano?"1504e85269b276a7454ca614b5ea9e312a7e201bd3b93bf3f6f19218bc422c19":mirror?"d0416842b14ff078b32100cb3f9d7052ffafc5477fb7db176276b967875f3ced":puzzles?"9d167f427432a4460b962559df533c4113ca440b314139de430ba2b39ecbb136":kitchen?"3b583b4335ba59336c24d51e438a7a8c867b91a867b24bcbafefce18ee639b8e":hunters?"37b36f76622a7fe89ad71a359310d5f31d0321ff87a7756920914174b1200520":wax?"4f2bafe9faa87610fafcd3b79c3d8e3c8fd42f57b5951331e035a98c6f96c3ee":stone?"a12e0de01e086b23a18be4759bd104ef0d3504314e054d7735d046285037fa61":"a5da5da6383c6575b8f0ad41135fbdd7cc580c269df606c5781b47d3c43a0c5d"),"Unreviewed G1 projection");
    const auto bytes=verifiedFile(path.parent_path(),source.at("file"),source.at("sha256"));std::istringstream input(bytes);
    if(!old){old=std::make_shared<const CombatContent>(loadCombatContentDefinition(path.parent_path()/"g1-g0-source.json",parseCombatContentDefinition(input)));}
    auto c=*old;c.compatibility=data.at("content_compatibility");c.legacySource=old;

    for(const auto& [id,name]:p.at("facts").items()){
        need(!c.facts.switches.contains(std::stoi(id))&&!c.facts.flagIds().contains(name.get<std::string>()),"Duplicate G1 fact");c.facts.switches.emplace(std::stoi(id),name.get<std::string>());
    }
    for(const auto* group:{"images","audio","cues","animations"}){for(const auto& [id,value]:p.at(group).items()){
        need(!c.hasMediaDefinition(group,id),"G1 shadows inherited media");c.setMediaDefinition(group,id,value);
    }}
    need(monitor?p.at("items").size()==11&&p.at("items").contains("22"):piano?p.at("items").size()==10&&p.at("items").contains("100"):mirror?p.at("items").size()==9&&p.at("items").contains("98")&&p.at("items").contains("99"):puzzles?p.at("items").size()==7&&p.at("items").contains("95")&&p.at("items").contains("96")&&p.at("items").contains("97"):kitchen?p.at("items").size()==4&&p.at("items").contains("13")&&p.at("items").contains("102")&&p.at("items").contains("93")&&p.at("items").contains("94"):wax?p.at("items").size()==2&&p.at("items").contains("94")&&p.at("items").contains("93"):stone?p.at("items").size()==1&&p.at("items").contains("93"):p.at("items").empty(),"Unreviewed G1 item definitions");
    for(const auto& [id,value]:p.at("items").items()){need(c.items.emplace(std::stoi(id),readItemDefinition(value)).second,"G1 shadows inherited item");}
    if(monitor){for(const auto& [id,name]:p.at("values").items()){need(c.facts.variables.emplace(std::stoi(id),name.get<std::string>()).second,"Duplicate G1 value");}}
    auto sprites=*c.maps.begin()->second.sprites;
    const auto added=readSceneDefinitions(c,Json{{"font",p.at("font")},{"sprites",p.at("scene_sprites")},{"maps",p.at("maps")},{"objects",p.at("objects")}});
    for(const auto& [id,value]:added){sprites.insert_or_assign(id,value);}const auto sharedSprites=std::make_shared<const SpriteDefinitions>(sprites);for(auto& [id,map]:c.maps){map.sprites=sharedSprites;}
    for(const auto& [id,value]:p.at("messages").items()){need(c.messages.emplace(id,readCombatMessage(value,true)).second,"G1 shadows inherited message");}
    for(const auto& [id,value]:p.at("cues").items()){
        need(!value.contains("runtime_support"),"Unsupported G1 cue");if(value.at("kind")=="forced_route"){need(c.routes.emplace(id,readRouteDefinition(value)).second,"Duplicate G1 route");}
    }
    for(const auto& [id,flow]:p.at("flows").items()){
        const auto& object=c.objects.at(flow.at("object").get<std::string>());
        need(std::any_of(object.variants.begin(),object.variants.end(),[&](const auto& v){return v.page==flow.at("page");}),"Missing G1 flow page");
        need(c.catalog["flows"].emplace(id,flow).second,"G1 shadows inherited flow");
    }
    for(const auto& [id,value]:p.at("transitions").items()){need(c.transitions.emplace(id,value).second,"G1 shadows inherited transfer");}
    auto& wake=c.transitions.at(p.at("wake_door").get<std::string>());need(wake.at("destination").at("map")==128&&c.maps.contains(128),"Missing wake target");wake["runtime_enabled"]=true;wake["handoff_only"]=false;c.catalog["wake"]["door"]=wake;
    for(const auto& [id,value]:p.at("map_music").items()){need(!c.expansion.mapMusic.contains(id),"G1 shadows inherited music policy");c.expansion.mapMusic[id]=value;}
    installSourceSavePoint(c,p.at("save"),p.at("flows"));
    for(auto& point:c.catalog["save_points"]){if(point.at("object")==p.at("save").at("object")){point["required_flags"]=Json::array({360});}}
    if(kitchen){
        installLowerTileContent(c,p.at("lower_tiles"));
        const auto& weapon=p.at("hp_weapon");need(c.items.at(13).attack==27&&c.items.at(13).capacity==13&&weapon.at("magazine")==Json{{"item",13},{"initial",13},{"source_variable",53}},"Invalid HP magazine binding");
        need(!c.requiredMagazines.contains(13)&&!c.magazineSeedBindings.contains(53),"HP shadows existing magazine");c.requiredMagazines.emplace(13,13);c.magazineSeedBindings.emplace(53,13);
        for(const auto& [name,profile]:weapon.at("profiles").items()){need(!c.actionProfiles.contains(name),"HP shadows weapon action");for(const auto& cue:profile.at("cue_order")){need(c.cues.contains(cue.get<std::string>()),"Missing HP weapon cue");}if(profile.contains("restore_facing")){for(const auto& cue:profile.at("restore_facing")){need(c.cues.contains(cue.get<std::string>()),"Missing HP direction restoration cue");}}if(profile.contains("sound_cue")){need(c.cue(profile.at("sound_cue").get<std::string>()).at("kind")=="sound","Invalid HP action sound");}c.actionProfiles[name]=profile;}
        c.catalog["weapon_bindings"]["13"]=weapon.at("binding");c.expansion.reloadEquipment["13"]=weapon.at("reload_equipment");
    }
    if(monitor){
        installSourceSavePoint(c,p.at("monitor_save"),p.at("flows"));
        for(auto& point:c.catalog["save_points"]){if(point.at("object")=="m151-e11"){point["required_flags"]=Json::array({440});}}
        const auto& w=p.at("rocket_weapon");need(w.at("magazine")==Json{{"item",22},{"initial",4},{"source_variable",62}}&&c.items.at(22).attack==399&&!c.items.at(22).regularReload,"Invalid rocket definition");
        c.requiredMagazines.emplace(22,4);c.magazineSeedBindings.emplace(62,22);
        for(const auto& [name,profile]:w.at("profiles").items()){need(!c.actionProfiles.contains(name),"Rocket shadows weapon action");if(profile.contains("sound_cue")){need(c.cue(profile.at("sound_cue").get<std::string>()).at("kind")=="sound","Invalid rocket action sound");}for(const auto& cue:profile.at("cue_order")){need(c.cues.contains(cue.get<std::string>()),"Missing rocket action cue");}if(profile.contains("restore_facing")){for(const auto& cue:profile.at("restore_facing")){need(c.cues.contains(cue.get<std::string>()),"Missing rocket facing cue");}}c.actionProfiles[name]=profile;}
        const auto& b=w.at("binding");need(b.at("kind")=="rocket"&&b.at("projectile").at("steps_forward")==18&&b.at("explosion_stages").size()==1&&b.at("explosion_stages")[0].at("receiver")==6&&b.at("explosion_stages")[0].at("half_pixels")==Json::array({25,25})&&b.at("explosion_stages")[0].at("phase")=="impact"&&b.at("explosion_stages")[0].at("wait_ticks")==7&&b.at("locked_speed")==2&&b.at("projectile").at("speed")==6&&b.at("projectile").at("layer")==2&&!b.at("projectile").at("through").get<bool>()&&b.at("projectile").at("skip_blocked").get<bool>(),"Invalid rocket policy");
        for(const auto& cue:b.at("explosion_stages")[0].at("cue_order")){need(c.cues.contains(cue.get<std::string>()),"Missing rocket receiver cue");}
        for(const auto& cue:b.at("flight_cue_order")){need(c.cues.contains(cue.get<std::string>()),"Missing rocket flight cue");}for(const auto& cue:b.at("explosion_cue_order")){need(c.cues.contains(cue.get<std::string>()),"Missing rocket explosion cue");}
        for(const auto* key:{"flight_move_cue","flight_erase_cue"}){need(c.cues.contains(b.at(key).get<std::string>()),"Missing rocket cleanup cue");}
        c.catalog["weapon_bindings"]["22"]=b;
    }
    c.catalog["g1"]={{"facts",p.at("facts")},{"delivered",monitor?9:piano?8:mirror?7:puzzles?6:kitchen?5:hunters?4:wax?3:stone?2:1}};
    if(wax){const auto& h=p.at("hunter");need(h.at("behavior")=="hunter"&&c.objects.contains("m141-e2"),"Invalid Hunter content binding");c.enemies.emplace("m141-e2",EnemyDefinition{"m141-e2",141,2,270,false,h});c.enemyBurnSlots["weapon_actions"]["m141-e2"]=1;c.fatalDeath=p.at("fatal_death");}
    if(hunters){for(const auto& [id,binding]:p.at("hunters").items()){const auto& h=binding.at("rules");need(h.at("behavior")=="hunter"&&c.objects.contains(id),"Invalid mansion Hunter binding");c.enemies.emplace(id,EnemyDefinition{id,binding.at("map"),binding.at("event"),binding.at("dead_fact"),false,h});c.enemyBurnSlots["weapon_actions"][id]=h.at("burn_slot");}}

    if(stone){const auto& prior=p.at("previous");need(prior.size()==2&&prior.at("file")== (monitor?"g1-07-source.json":piano?"g1-06-source.json":mirror?"g1-05-source.json":puzzles?"g1-09-source.json":kitchen?"g1-04-source.json":hunters?"g1-03-source.json":wax?"g1-02-source.json":"g1-01-source.json")&&prior.at("sha256")== (monitor?"5931ca01284dca9b70331d1a180c9192e76aaa694dc360bd7415161023100189":piano?"ba7d93ba097d4b952167996a442254d45a437b55c2ab0ebf3b734f7c11079bc4":mirror?"92f929c3a9a1710b80a14a842ec3114859a9563b1d8bdfdd2c73a4028931a76d":puzzles?"ae563244da465db195eb62c6d2e323bd0e25deb7c8afc8cb7ba3ebf173dfb7a4":kitchen?"53107b097b5c185417b66296caa271de72550a797fc28d61ff3ab9861caf1adc":hunters?"5cb7cad7df1cc0a1d3d1e505d7b0a8beb867caa0bfbe9c78f535be6f2fe6bf64":wax?"2f4df9c0ab70babb75a6892466818831e51951d2be8bb2d693f1e4c9adb4d916":"10a88681a453e4c356887d9543cf2ac442770b443b92d186187ff0d2f9dc218e"),"Unknown G1 prior source");const auto priorBytes=verifiedFile(path.parent_path(),prior.at("file"),prior.at("sha256"));c.legacySource=std::make_shared<const CombatContent>(loadG1Definition(path.parent_path()/prior.at("file").get<std::string>(),Json::parse(priorBytes),old));}
    if(kitchen){c.catalog["g1"]["delivery_order"]=Json::array({1,2,3,4,9});}
    if(puzzles){
        c.catalog["g1"]["delivery_order"]=Json::array({1,2,3,4,9,5});
        c.catalog["context_tools"].push_back(p.at("clock_tool"));
        c.catalog["readings"]["item-189"]=p.at("note_reading");
        installSourceLighting(c,p.at("lighting"));
    }
    if(mirror){c.catalog["g1"]["delivery_order"]=Json::array({1,2,3,4,9,5,6});c.catalog["context_tools"].push_back(p.at("axe_tool"));c.catalog["environment_targets"].push_back(p.at("mirror_target"));}
    if(piano){c.catalog["g1"]["delivery_order"]=Json::array({1,2,3,4,9,5,6,7});c.catalog["context_tools"].push_back(p.at("piano_tool"));}
    if(monitor){c.catalog["g1"]["delivery_order"]=Json::array({1,2,3,4,9,5,6,7,8});c.catalog["g1"]["values"]=p.at("values");}
    for(auto& [id,e]:c.entries){initializeFullSession(c,e.session);}
    for(const auto& [name,sha]:p.at("resources").items()){verifiedFile(c.directory,name,sha);}
    validateSceneContent(c,sprites);
    return c;
}

CombatContent loadG1Content(const std::filesystem::path& path,const Json& data){return loadG1Definition(path,data);}
