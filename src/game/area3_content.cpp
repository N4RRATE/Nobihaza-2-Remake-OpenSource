#include "full_content.h"
#include "content_hash.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
using Json=nlohmann::json;
namespace{
#include "../../tools/g3-route-doors.inc"
#include "../../tools/g3-access-cold-room.inc"
void need(bool ok,const char* message){if(!ok){throw std::runtime_error(message);}}
std::string verified(const std::filesystem::path& directory,const std::string& name,const std::string& sha){
    need(std::filesystem::path(name).filename()==name,"Invalid G3 resource path");const auto file=directory/name;
    need(!std::filesystem::is_symlink(file)&&std::filesystem::is_regular_file(file)&&std::filesystem::file_size(file)<=128*1024*1024,"Missing G3 resource");
    std::ifstream in(file,std::ios::binary);need(in.is_open(),"Cannot read G3 resource");std::string bytes=readContentBytes(in);need(contentSha256(bytes)==sha,"Changed G3 source/media");return bytes;
}
}
CombatContent loadArea3Content(const std::filesystem::path& path,const Json& data){
    if(data.contains("resource_revision")){
        // Explicit self-contained resource revision. The unchanged base still
        // passes its original closed loader and keeps its save identity.
        need(data.size()==4&&data.at("schema")=="nobihaza.g3.v1"&&data.at("resource_revision")=="g3-cold-room-v1","Unknown G3 resource revision");
        const auto& source=data.at("source");
        need(source.size()==2&&source.at("file")=="cold-room-base.json","Invalid cold revision source");
        const auto sha=source.at("sha256").get<std::string>();
        need(sha.size()==64&&sha.find_first_not_of("0123456789abcdef")==std::string::npos,"Invalid cold revision SHA");
        static const auto cold=Json::parse(area3ColdRoomCorrection);
        for(const auto& [name,hash]:cold.at("resources").items()){verified(path.parent_path(),name,hash);}
        std::istringstream input(verified(path.parent_path(),source.at("file"),sha));
        const auto base=parseCombatContentDefinition(input);
        need(!base.contains("resource_revision"),"Recursive cold revision");
        auto c=loadCombatContentDefinition(path.parent_path()/"cold-room-base.json",base);
        need(c.compatibility==data.at("content_compatibility").get<std::string>()&&c.catalog.contains("g3")&&c.catalog.at("g3").at("delivered")>=4,"Cold revision requires delivered G3 access content");
        for(const auto& [id,row]:cold.at("objects").items()){need(!c.objects.contains(id),"Duplicate cold-room object");}
        auto sprites=*c.maps.at(172).sprites;
        const auto added=readSceneDefinitions(c,Json{{"font",Json::object()},{"sprites",cold.at("sprites")},{"maps",Json::object()},{"objects",cold.at("objects")}});
        for(const auto& [id,row]:added){need(sprites.emplace(id,row).second,"Duplicate cold-room sprite");}
        const auto sharedSprites=std::make_shared<const SpriteDefinitions>(sprites);for(auto& [id,map]:c.maps){map.sprites=sharedSprites;}
        for(const auto& [id,row]:cold.at("messages").items()){need(c.messages.emplace(id,readCombatMessage(row,true)).second,"Duplicate cold-room message");}
        for(const auto& [id,row]:cold.at("cues").items()){need(!c.cues.contains(id),"Duplicate cold-room cue");c.setMediaDefinition("cues",id,row);}
        for(const auto& [id,row]:cold.at("flows").items()){need(!c.catalog["flows"].contains(id),"Duplicate cold-room flow");c.catalog["flows"][id]=row;}
        c.catalog["cold_room_revision"]="g3-cold-room-v1";
        validateSceneContent(c,sprites);return c;
    }
    const auto identity=data.at("content_compatibility").get<std::string>();
    const bool reunion=identity=="post-f5-g3-normal-7";
    const bool hunters=reunion||identity=="post-f5-g3-normal-6";
    const bool heat=hunters||identity=="post-f5-g3-normal-5";
    const bool access=heat||identity=="post-f5-g3-normal-4";
    const bool shelves=access||identity=="post-f5-g3-normal-3";
    const bool route=shelves||identity=="post-f5-g3-normal-2";
    need(data.size()==4&&data.at("schema")=="nobihaza.g3.v1"&&(route||data.at("content_compatibility")=="post-f5-g3-normal-1"),"Unknown G3 identity");
    const auto& source=data.at("source");need(source.size()==2&&source.at("file")== (reunion?"g3-06-source.json":hunters?"g3-05-source.json":heat?"g3-04-source.json":access?"g3-03-source.json":shelves?"g3-02-source.json":route?"g3-01-source.json":"g2-06-source.json")&&source.at("sha256")== (reunion?"08f0c2b2dcf56db2fbfd8dbba0e5b596b1abdd9bf72b35c0cefba62f29b73aa8":hunters?"de9f53e9c7791fb92467e9f72057c42b24853b16775101bc734b46c0941ee0db":heat?"b401c8dea23848f8622e593a526f774e19ab6eeb90a68b29aeb74ab972991943":access?"ea351d70e1065a3c3803ea4669f96829d62939751112643854e87663cd363c1c":shelves?"1d3352200ad94e926df2000a37409f3eaaafa85eba93cd1cbed56e4cc464baa3":route?"12243b0b584170a3a7722445e56a428ace73ef08fb39556d455b14c8b024d729":"e001f24317aaa9daa80e105d41ecf33b329daa26d33cd9eeab4b76756d633202"),"Unknown G3 source");
    const auto& p=data.at("projection");need(contentSha256(p.dump())==(reunion?"ab7e75cf263741e71656405df3a6806d8ecb4efac7f4b303769fdfccee1b1ebd":hunters?"8880f11a6e8667585ed4112563dd01499700efea4345e0570663806951043d43":heat?"8ca0feb377e03bb33a068bb728bbe43f952d8c734a2826503d84ed9255253b33":access?"6634e1f9ac49d2ccc33a9031e8c52a32de1049c7912d1928946b22703c84357f":shelves?"2b643c2a44c1ec4306523dadbf401137d5840856359a2f88c200e21244505ac6":route?"a4344319f591e37a43b14dd10f2f4c96280eb76cef98604fc1f001866760afd9":"958a2cb347b7fbf1530cdf5e6101daa98e89b7c7fc0b09550bbda73ca233006f"),"Unreviewed G3 projection");
    std::istringstream in(verified(path.parent_path(),source.at("file"),source.at("sha256")));
    auto old=std::make_shared<const CombatContent>(loadCombatContentDefinition(path.parent_path()/source.at("file").get<std::string>(),parseCombatContentDefinition(in)));
    auto c=*old;c.legacySource=old;c.compatibility=data.at("content_compatibility");


    for(const auto& [id,name]:p.at("facts").items()){need(c.facts.switches.emplace(std::stoi(id),name.get<std::string>()).second,"Duplicate G3 fact");}
    for(const auto& [id,name]:p.at("values").items()){need(c.facts.variables.emplace(std::stoi(id),name.get<std::string>()).second,"Duplicate G3 value");}
    for(const auto* group:{"images","audio","cues","animations"}){for(const auto& [id,row]:p.at(group).items()){c.setMediaDefinition(group,id,row);}}
    // The verified source extends M164 event pages; every inherited map grid stays exact.
    for(const auto& [id,row]:p.at("maps").items()){if(c.maps.contains(std::stoi(id))){need(std::stoi(id)==164,"G3 changes another inherited map");c.maps.erase(std::stoi(id));c.terrainIds.erase(std::stoi(id));}}
    for(const auto& [id,row]:p.at("objects").items()){if(c.objects.contains(id)){need(c.objects.at(id).map==164||(access&&row.at("map")==c.objects.at(id).map&&row.at("initial_tile")==Json::array({c.objects.at(id).tile.x,c.objects.at(id).tile.y})),"G3 changes another inherited object");c.objects.erase(id);}}
    auto sprites=*c.maps.begin()->second.sprites;
    const auto added=readSceneDefinitions(c,Json{{"font",p.at("font")},{"sprites",p.at("scene_sprites")},{"maps",p.at("maps")},{"objects",p.at("objects")}});
    for(const auto& [id,row]:added){sprites.insert_or_assign(id,row);}const auto sharedSprites=std::make_shared<const SpriteDefinitions>(sprites);for(auto& [id,map]:c.maps){map.sprites=sharedSprites;}
    for(const auto& [id,row]:p.at("messages").items()){c.messages.insert_or_assign(id,readCombatMessage(row,true));}
    for(const auto& [id,row]:p.at("cues").items()){need(!row.contains("runtime_support"),"Unsupported G3 cue");if(row.at("kind")=="forced_route"){c.routes.insert_or_assign(id,readRouteDefinition(row));}}
    for(const auto& [id,row]:p.at("flows").items()){
        if(c.catalog["flows"].contains(id)){need((access&&(id=="m176-e5-p1"||id=="m176-e6-p1"||id=="m176-e7-p1"))||c.catalog["flows"].at(id).value("handoff_only",false)||!c.catalog["flows"].at(id).value("runtime_enabled",true),"G3 changes a delivered flow");}c.catalog["flows"][id]=row;
    }
    if(route){
        for(const auto& [id,name]:p.at("facts").items()){c.catalog["g3"]["facts"][id]=name;}
        for(const auto& [id,name]:p.at("values").items()){c.catalog["g3"]["values"][id]=name;}
        c.catalog["g3"]["delivered"]=reunion?7:hunters?6:heat?5:access?4:shelves?3:2;
        for(const auto& [id,row]:p.at("items").items()){c.items.insert_or_assign(std::stoi(id),readItemDefinition(row));}
        if(!shelves){for(const auto& [id,row]:p.at("readings").items()){need(c.catalog["readings"].emplace(id,row).second,"Duplicate G3 reading");}
        for(const auto& row:p.at("ladders")){c.catalog["ladders"].push_back(row);}}
        for(const auto& [id,row]:p.at("zombies").items()){
            const auto& rules=row.at("rules");need(rules.at("behavior")=="zombie"&&c.objects.contains(id),"Invalid G3 zombie");
            need(c.enemies.emplace(id,EnemyDefinition{id,row.at("map"),row.at("event"),row.at("dead_fact"),false,rules}).second,"Duplicate G3 zombie");
            c.enemyBurnSlots["weapon_actions"][id]=rules.at("burn_slot");
        }
        if(reunion){
            c.catalog["music_memory"]=p.at("music_memory");
            c.catalog["legacy_scene_objects"]=p.at("legacy_scene_objects");
            for(auto& [id,flow]:c.catalog["flows"].items()){
                if(flow.contains("object")&&std::find(p.at("entry_events").begin(),p.at("entry_events").end(),flow.at("object"))!=p.at("entry_events").end()){flow["natural_entry_only"]=true;}
            }
        }else if(hunters){
            for(const auto& [id,row]:p.at("hunters").items()){
                const auto& rules=row.at("rules");need(rules.at("behavior")=="hunter"&&c.objects.contains(id),"Invalid B1 Hunter binding");
                need(c.enemies.emplace(id,EnemyDefinition{id,row.at("map"),row.at("event"),row.at("dead_fact"),false,rules}).second,"Duplicate B1 Hunter");
                auto& effects=c.enemyBurnSlots[c.full->at("runtime").contains("combat_effects")?"combat_effects":"weapon_actions"];effects[id]=rules.at("burn_slot");
            }
        }else if(heat){
            c.catalog["heat_room"]=p.at("heat_room");c.catalog["deferred_key"]=p.at("deferred_key");
            for(const auto& tool:p.at("context_tools")){c.catalog["context_tools"].push_back(tool);}
        }else if(access){
            c.expansion.failureFlows["chimera-fatal"]=p.at("failure");c.catalog["access_keys"]=p.at("access_keys");c.catalog["acid_projectiles"]=p.at("acid_projectiles");
            for(const auto& tool:p.at("context_tools")){c.catalog["context_tools"].push_back(tool);}
            for(const auto& [id,row]:p.at("chimeras").items()){
                const auto& rules=row.at("rules");need(rules.at("behavior")=="chimera"&&c.objects.contains(id),"Invalid G3 chimera");
                need(c.enemies.emplace(id,EnemyDefinition{id,row.at("map"),row.at("event"),row.at("dead_fact"),false,rules}).second,"Duplicate G3 chimera");
                c.enemyBurnSlots["weapon_actions"][id]=rules.at("burn_slot");
            }
        }
    }else{
        c.catalog["g3"]={{"facts",p.at("facts")},{"values",p.at("values")},{"delivered",1}};
        c.catalog["dekisugi"]=p.at("dekisugi");c.catalog["continue"]=p.at("continue");c.catalog["continue"]["restore_clear_flags"]=Json::array({441,442});c.catalog["weapon_map_profiles"]=p.at("weapon_map_profiles");
        auto& effects=c.enemyBurnSlots[c.full->at("runtime").contains("combat_effects")?"combat_effects":"weapon_actions"];
        for(const auto& part:p.at("dekisugi").at("parts")){effects[part.at("object").get<std::string>()]=part.at("burn_slot");}
    }
    if(route&&!shelves){
        // Correct the fixed 02 content once; later cumulative sources inherit it.
        static const auto doors=Json::parse(area3DoorCorrection);
        for(const auto& [id,row]:doors.at("objects").items()){
            need(c.objects.at(id).map==row.at("map")&&Json::array({c.objects.at(id).tile.x,c.objects.at(id).tile.y})==row.at("initial_tile"),"Changed G3 door identity");c.objects.erase(id);
        }
        readSceneDefinitions(c,Json{{"font",p.at("font")},{"sprites",doors.at("sprites")},{"maps",Json::object()},{"objects",doors.at("objects")}});
        for(const auto& [id,row]:doors.at("messages").items()){need(c.messages.emplace(id,readCombatMessage(row,true)).second,"Duplicate corrected door message");}
        for(const auto& [id,row]:doors.at("cues").items()){need(!c.cues.contains(id),"Duplicate corrected door cue");c.setMediaDefinition("cues",id,row);}
        for(const auto& [id,row]:doors.at("flows").items()){
            if(c.catalog["flows"].contains(id)&&c.catalog["flows"].at(id).value("runtime_enabled",true)){need(c.catalog["flows"].at(id).at("steps")==row.at("steps"),"Door correction changes delivered flow");}
            else{c.catalog["flows"][id]=row;}
        }
    }

    for(auto& [id,e]:c.entries){initializeFullSession(c,e.session);}
    for(const auto& [name,sha]:p.at("resources").items()){verified(c.directory,name,sha);}
    validateSceneContent(c,sprites);return c;
}
