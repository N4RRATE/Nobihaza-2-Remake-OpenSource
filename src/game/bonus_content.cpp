#include "full_content.h"
#include "content_hash.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
using Json=nlohmann::json;
CombatContent loadBonusContent(const std::filesystem::path& path,const Json& data){
    const auto need=[](bool ok,const char* text){if(!ok){throw std::runtime_error(text);}};
    need(data.size()==4&&data.at("schema")=="nobihaza.g7bonus.v1"&&data.at("content_compatibility")=="post-f5-g7-normal-7","Unknown bonus identity");
    const auto& source=data.at("source");const auto& p=data.at("projection");
    const auto verified=[&](const std::string& name,const std::string& sha){
        need(std::filesystem::path(name).filename()==name,"Invalid bonus resource path");const auto file=path.parent_path()/name;
        need(!std::filesystem::is_symlink(file)&&std::filesystem::is_regular_file(file)&&std::filesystem::file_size(file)<=128*1024*1024,"Missing bonus resource");
        std::ifstream in(file,std::ios::binary);const auto bytes=readContentBytes(in);need(contentSha256(bytes)==sha,"Changed bonus resource");return bytes;
    };
    need(source==Json{{"file","g7-07-source.json"},{"sha256","ae6177127e7c6a8fa9c77b5ec8c6c8bd9b8cc3abcb408fb99015b027d153cf9f"}},"Unknown bonus predecessor");
    need(contentSha256(p.dump())=="3f2af7181d4b75de52361e6ecbf8c7087339b25786935079a7ede18fcf90762b","Unreviewed bonus projection");
    std::istringstream in(verified(source.at("file"),source.at("sha256")));auto old=std::make_shared<const CombatContent>(loadCombatContentDefinition(path.parent_path()/source.at("file").get<std::string>(),parseCombatContentDefinition(in)));
    auto c=*old;c.legacySource=old;
    for(const auto* group:{"images","audio","cues","animations"}){for(const auto& [id,row]:p.at(group).items()){
        need(!c.hasMediaDefinition(group,id),"Bonus shadows delivered media");c.setMediaDefinition(group,id,row);
    }}
    for(const auto& [id,row]:p.at("maps").items()){need(!c.maps.contains(std::stoi(id)),"Bonus shadows delivered map");}
    for(const auto& [id,row]:p.at("objects").items()){need(!c.objects.contains(id),"Bonus shadows delivered object");}
    auto sprites=*c.maps.begin()->second.sprites;const auto added=readSceneDefinitions(c,Json{{"font",p.at("font")},{"sprites",p.at("scene_sprites")},{"maps",p.at("maps")},{"objects",p.at("objects")}});
    for(const auto& [id,row]:added){sprites.insert_or_assign(id,row);}const auto sharedSprites=std::make_shared<const SpriteDefinitions>(sprites);for(auto& [id,map]:c.maps){map.sprites=sharedSprites;}
    for(const auto& [id,row]:p.at("messages").items()){need(c.messages.emplace(id,readCombatMessage(row,true)).second,"Bonus shadows delivered message");}
    for(const auto& [id,row]:p.at("cues").items()){need(!row.contains("runtime_support"),"Unsupported bonus cue");if(row.at("kind")=="forced_route"){c.routes.insert_or_assign(id,readRouteDefinition(row));}}
    for(const auto& [id,row]:p.at("flows").items()){need(!c.catalog["flows"].contains(id),"Bonus shadows delivered flow");c.catalog["flows"][id]=row;}
    c.catalog["bonus"]=p.at("bonus");
    for(const auto& [id,row]:p.at("map_music").items()){need(!c.expansion.mapMusic.contains(id),"Bonus shadows delivered music");c.expansion.mapMusic[id]=row;}
    c.catalog["pending_maps"].erase("217");c.pendingMapTickets.erase(217);
    for(const auto& [name,sha]:p.at("resources").items()){verified(name,sha);}
    validateSceneContent(c,sprites);return c;
}
