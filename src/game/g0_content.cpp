#include "full_content.h"
#include "content_hash.h"
#include <fstream>
#include <sstream>
#include <set>
#include <stdexcept>
using Json=nlohmann::json;
namespace{
void need(bool ok,const char* why){if(!ok){throw std::runtime_error(why);}}
CombatContent loadG0ContentDefinition(const std::filesystem::path& path,const Json& data,const Json* sourceDefinition,std::shared_ptr<const CombatContent> old){
    need(data.size()==4&&data.at("schema")=="nobihaza.g0.v1"&&(data.at("content_compatibility")=="post-f5-g0-normal-1"||data.at("content_compatibility")=="post-f5-g0-normal-2"),"Unknown G0 content identity");
    const auto& source=data.at("source");
    need(source.size()==2&&source.at("file")=="g0-f4-source.json"&&source.at("sha256")=="c4f607688c2dbaabfe910381feec1d9a3333d1acbf1536c83fbddef264baaed7","Unknown G0 source contract");
    const auto sourcePath=path.parent_path()/"g0-f4-source.json";
    const auto& p=data.at("projection");
    const auto hash=contentSha256(p.dump());
    need(hash=="35b2508b2b3a59acb6d3152905e420da4de7452ae16c170ea462cca21c113b44"||hash=="c445756bb80895f0c8018b78468e34963de3ecdbd558b7ea166ea6d629c32ca0"||hash=="0fe12a93a455ebb382686295efcbeea756eb0d04010d2c5aaeb4aab50e5389c5"||hash=="9c9fc76382d4284903c7478fe8e208ee4bb5d8890af15a5954cc76bfcba4e53e"||hash=="efcc389c3315836034e0c5725c0074c06dc765ba70cd9eabda67cba7228989d3"||hash=="3f9e40c0b12e55316513eada4a2abccfe83c885b06aa9ffc3ca0ff67c408ab27"||hash=="cc13c3e4d8a0c6e107b5a7950c6bf3481c11f3c42e51d721d474f2e4d159f4f7"||hash=="950886d8fe00b5256b38447eaa0d9fb1a18a7f42829abfa715dcd1076a886132","Unreviewed G0 projection");
    need((p.value("delivered",3)>=7)==(data.at("content_compatibility")=="post-f5-g0-normal-2"),"G0 identity differs from reviewed delivery");
    Json parsedSource;
    if(!sourceDefinition){
        need(std::filesystem::is_regular_file(sourcePath)&&std::filesystem::file_size(sourcePath)<=128*1024*1024,"Invalid or oversized content file");
        std::ifstream in(sourcePath,std::ios::binary);need(in.is_open(),"Missing G0 source contract");const std::string bytes=readContentBytes(in);
        need(contentSha256(bytes)==source.at("sha256").get<std::string>(),"Changed G0 source contract");
        std::istringstream sourceInput(bytes);parsedSource=parseCombatContentDefinition(sourceInput);sourceDefinition=&parsedSource;
        old=std::make_shared<const CombatContent>(loadCombatContentDefinition(sourcePath,parsedSource));
    }
    auto c=*old;
    c.legacySource=old;c.compatibility=data.at("content_compatibility");
    for(const auto& [id,name]:p.at("facts").items()){
        need(!c.facts.switches.contains(std::stoi(id))&&!c.facts.flagIds().contains(name.get<std::string>()),"Duplicate G0 fact");c.facts.switches.emplace(std::stoi(id),name.get<std::string>());
    }
    for(const auto* group:{"images","audio","cues"}){for(const auto& [id,value]:p.at(group).items()){need(!c.hasMediaDefinition(group,id),"G0 shadows inherited media");c.setMediaDefinition(group,id,value);}}
    if(p.contains("items")){for(const auto& [id,value]:p.at("items").items()){auto item=readItemDefinition(value);need(id==std::to_string(item.id)&&c.items.emplace(item.id,item).second,"G0 shadows inherited item");}}
    if(p.contains("animations")){for(const auto& [id,value]:p.at("animations").items()){need(!c.media["animations"].contains(id),"G0 shadows inherited animation");c.media["animations"][id]=value;}}
    Json sprites=sourceDefinition->at("sprites");for(const auto& [id,value]:p.at("sprites").items()){need(!sprites.contains(id),"G0 shadows inherited sprite");sprites[id]=value;}
    const auto definitions=readSceneDefinitions(c,Json{{"font",sourceDefinition->at("font")},{"sprites",sprites},{"maps",p.at("maps")},{"objects",p.at("objects")}});
    const auto sharedSprites=std::make_shared<const SpriteDefinitions>(definitions);for(auto& [id,map]:c.maps){map.sprites=sharedSprites;}
    for(const auto& [id,value]:p.at("messages").items()){need(c.messages.emplace(id,readCombatMessage(value,true)).second,"G0 shadows inherited message");}
    for(const auto& [id,value]:p.at("cues").items()){
        need(!value.contains("runtime_support"),"Unsupported G0 presentation cue");
        if(value.at("kind")=="forced_route"){need(c.routes.emplace(id,readRouteDefinition(value)).second,"Duplicate G0 route");}
    }
    for(const auto& [id,flow]:p.at("flows").items()){
        const std::string object=flow.at("object");need(c.objects.contains(object),"Missing G0 flow object");
        const auto& variants=c.objects.at(object).variants;need(std::any_of(variants.begin(),variants.end(),[&](const auto& v){return v.page==flow.at("page");}),"Missing G0 flow page");
        need(c.catalog["flows"].emplace(id,flow).second,"G0 shadows inherited flow");
    }
    if(p.contains("arrival")){
        const auto& arrival=p.at("arrival");auto& object=c.objects.at(arrival.at("object").get<std::string>());
        auto page=std::find_if(object.variants.begin(),object.variants.end(),[&](const auto& v){return v.page==arrival.at("page");});
        need(page!=object.variants.end()&&c.facts.switches.contains(arrival.at("retired_by_fact")),"Invalid G0 arrival retirement");
        // Legal old saves already passed the compulsory first visit, without representing S345.
        page->flags[arrival.at("retired_by_fact")]=false;
    }
    for(const auto& row:p.at("tile_substitutions")){
        UpperTileReplacementDefinition r;r.fromIndex=row.at("from_index");r.toIndex=row.at("to_index");
        r.image=c.directory/c.media.at("images").at(row.at("image").get<std::string>()).at("file").get<std::string>();
        r.underlay=c.directory/c.media.at("images").at(row.at("underlay").at("image").get<std::string>()).at("file").get<std::string>();
        for(const auto& cell:row.at("cells")){need(!cell.at("foreground").get<bool>()&&!cell.at("source_foreground").get<bool>()&&!cell.at("lower_foreground").get<bool>(),"Unsupported G0 door tile layer");r.tiles.push_back(cell.at("tile").get<std::array<int,2>>());r.passageMasks.push_back(cell.at("mask"));}
        c.maps.at(121).upperTileReplacements.push_back(std::move(r));
    }
    c.catalog.erase("completion_boundary");
    // Developer recovery is a bounded F5 node, never an ordinary player save point.
    if(c.compatibility=="post-f5-g0-normal-2"&&p.value("delivered",3)>=9){
        c.catalog["development_recovery"]={{"node","f5-m121-before-hangar"},{"map",121},{"tile",{11,28}},{"facing","up"},
            {"required_flags",{320,321,323,341,342,343,344}},{"absent_flags",{346,347,348,349,350,355,356,357,360,501}},
            {"location","机库前走廊（开发恢复）"},{"slot_label","NORMAL"}};
    }
    installSourceSavePoint(c,p.at("save"),p.at("flows"));
    for(auto& point:c.catalog["save_points"]){if(point.at("object")=="m122-e16"){point["required_flags"]=Json::array({346,347});}}
    for(const auto& [id,ticket]:p.at("pending_maps").items()){c.pendingMapTickets.emplace(std::stoi(id),ticket.get<int>());}
    if(p.contains("hangar")){

        const auto& rules=p.at("hangar");c.enemyBurnSlots["weapon_actions"]["m120-e13"]=1;need(rules.at("behavior")=="hangar"&&c.objects.contains("m120-e13"),"Invalid G0 hangar owner");
        need(c.enemies.emplace("m120-e13",EnemyDefinition{"m120-e13",120,13,350,p.value("delivered",3)>=6,rules}).second,"Duplicate G0 hangar enemy");

    }
    if(p.contains("weapon")){
        need(c.compatibility=="post-f5-g0-normal-2","I15 requires distinct strict save identity");
        const auto& prior=p.at("legacy_source");need(prior.at("file")=="g0-06-source.json"&&prior.at("sha256")=="719f5ae2b5b48f55544c2d43d46cf677d91bf2ad0da196495ac8407b74ca4e6f","Unknown previous G0 source");
        std::ifstream f(c.directory/"g0-06-source.json",std::ios::binary);need(f.is_open(),"Missing previous G0 source");const std::string priorBytes{std::istreambuf_iterator<char>(f),{}};
        need(contentSha256(priorBytes)==prior.at("sha256").get<std::string>(),"Changed previous G0 source");
        // This synchronous compatibility branch uses the same verified immutable F4 source.
        std::istringstream priorInput(priorBytes);
        c.legacySource=std::make_shared<const CombatContent>(loadG0ContentDefinition(c.directory/"g0-06-source.json",parseCombatContentDefinition(priorInput),sourceDefinition,old));
        const auto& weapon=p.at("weapon");need(weapon.at("binding").at("damage_kind")=="semi-auto-shotgun"&&weapon.at("initial")==0&&weapon.at("variable")==55&&weapon.at("box_debit")==5&&weapon.at("close_bonus")==Json::array({25,35}),"Unreviewed I15 rules");
        need(c.items.at(15).capacity==7&&c.items.at(15).looseItem==33&&c.items.at(15).boxItem==34,"Invalid I15 magazine");c.items.at(15).boxReloadDebit=weapon.at("box_debit");
        for(const auto& [id,profile]:weapon.at("profiles").items()){need(c.actionProfiles.emplace(id,profile).second,"I15 shadows prior profile");for(const auto& cue:profile.at("cue_order")){need(c.cues.contains(cue.get<std::string>()),"Missing I15 action cue");}}
        need(c.catalog["weapon_bindings"].emplace("15",weapon.at("binding")).second,"Duplicate I15 binding");
        c.expansion.reloadEquipment["15"]=weapon.at("reload_equipment");c.expansion.attacks.emplace(15,c.expansion.attacks.at(14));c.requiredMagazines.emplace(15,0);c.magazineSeedBindings.emplace(55,15);
        c.catalog["aviation"]=p.at("aviation");
    }
    if(p.contains("chapter")){c.catalog["chapter"]=p.at("chapter");}
    if(p.contains("wake")){const auto& wake=p.at("wake");const auto& door=wake.at("door");need(c.objects.contains(wake.at("source_attack_object"))&&c.objects.at(wake.at("source_attack_object")).variants.size()==wake.at("source_attack_pages").get<std::size_t>(),"Missing original wake attack proxy");need(c.transitions.emplace("m127-e3-p2",door).second,"Duplicate post-wake door");c.catalog["wake"]=wake;}
    c.catalog["g0"]={{"facts",p.at("facts")},{"delivered",p.value("delivered",3)}};
    if(p.contains("reading_pairs")){c.catalog["g0"]["reading_pairs"]=p.at("reading_pairs");}
    for(auto& [id,e]:c.entries){initializeFullSession(c,e.session);}
    // Candidate preparation checks every declared resource before replacing a running game.
    for(const auto* group:{"images","audio","sprites"}){for(const auto& [id,value]:p.at(group).items()){need(std::filesystem::is_regular_file(c.directory/value.at("file").get<std::string>()),"Missing G0 resource");}}
    for(const auto& [id,map]:p.at("maps").items()){
        need(c.maps.at(std::stoi(id)).width==map.at("grid").at("width")&&c.maps.at(std::stoi(id)).height==map.at("grid").at("height"),"G0 map dimensions differ");
        for(const auto* layer:{"background","foreground"}){need(std::filesystem::is_regular_file(c.directory/map.at(layer).get<std::string>()),"Missing G0 map layer");}
    }
    need(c.maps.at(120).width==55&&c.maps.at(120).height==61&&c.maps.at(127).width==20&&c.maps.at(127).height==15,"Wrong G0 original map sizes");
    for(int id:{1,33,36}){need(c.items.contains(id),"Missing G0 supply item definition");}
    return c;
}
}
CombatContent loadG0Content(const std::filesystem::path& path,const Json& data){
    return loadG0ContentDefinition(path,data,nullptr,{});
}
