#include "full_content.h"
#include "content_hash.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
using Json=nlohmann::json;
namespace {
void need(bool ok,const char* text){if(!ok){throw std::runtime_error(text);}}
std::string verified(const std::filesystem::path& dir,const std::string& name,const std::string& sha){
    need(std::filesystem::path(name).filename()==name,"Invalid clear resource path");const auto file=dir/name;
    need(!std::filesystem::is_symlink(file)&&std::filesystem::is_regular_file(file)&&std::filesystem::file_size(file)<=128*1024*1024,"Missing clear resource");
    std::ifstream in(file,std::ios::binary);need(bool(in),"Cannot read clear resource");const auto bytes=readContentBytes(in);need(contentSha256(bytes)==sha,"Changed clear resource");return bytes;
}
}
CombatContent loadClearContent(const std::filesystem::path& path,const Json& data){
    need(data.size()==4&&data.at("schema")=="nobihaza.g7clear.v1"&&data.at("content_compatibility")=="post-f5-g7-normal-4","Unknown clear identity");
    const auto& source=data.at("source");need(source==Json{{"file","g7-03-source.json"},{"sha256","0ae0bb2420d190425102afcdba58d034bed622398441b6b3ec2355531a14958c"}},"Unknown clear predecessor");
    const auto& p=data.at("projection");need(contentSha256(p.dump())=="83b55da30aae5a53b5762194428b43727c3614b6892e977cb547f95edd5dfb48","Unreviewed clear projection");
    std::istringstream in(verified(path.parent_path(),source.at("file"),source.at("sha256")));auto old=std::make_shared<const CombatContent>(loadCombatContentDefinition(path.parent_path()/source.at("file").get<std::string>(),parseCombatContentDefinition(in)));
    auto c=*old;c.legacySource=old;
    for(const auto* group:{"images","audio","cues","animations"}){for(const auto& [id,row]:p.at(group).items()){
        need(!c.hasMediaDefinition(group,id),"Clear shadows delivered media");c.setMediaDefinition(group,id,row);
    }}
    for(const auto& [id,row]:p.at("maps").items()){need(!c.maps.contains(std::stoi(id)),"Clear shadows delivered map");}
    for(const auto& [id,row]:p.at("objects").items()){need(!c.objects.contains(id),"Clear shadows delivered object");}
    auto sprites=*c.maps.begin()->second.sprites;const auto added=readSceneDefinitions(c,Json{{"font",p.at("font")},{"sprites",p.at("scene_sprites")},{"maps",p.at("maps")},{"objects",p.at("objects")}});
    for(const auto& [id,row]:added){sprites.insert_or_assign(id,row);}const auto sharedSprites=std::make_shared<const SpriteDefinitions>(sprites);for(auto& [id,map]:c.maps){map.sprites=sharedSprites;}
    for(const auto& [id,row]:p.at("messages").items()){need(c.messages.emplace(id,readCombatMessage(row,true)).second,"Clear shadows delivered message");}
    for(const auto& [id,row]:p.at("cues").items()){need(!row.contains("runtime_support"),"Unsupported clear cue");if(row.at("kind")=="forced_route"){c.routes.insert_or_assign(id,readRouteDefinition(row));}}
    for(const auto& [id,row]:p.at("flows").items()){
        need(!c.catalog["flows"].contains(id),"Clear shadows delivered flow");
        c.catalog["flows"][id]=row;
    }
    c.catalog["clear"]=p.at("clear");
    for(const auto& [id,row]:p.at("map_music").items()){need(!c.expansion.mapMusic.contains(id),"Clear shadows delivered map music");c.expansion.mapMusic[id]=row;}
    for(int map:{215,216}){c.catalog["pending_maps"].erase(std::to_string(map));c.pendingMapTickets.erase(map);}
    for(const auto& [name,sha]:p.at("resources").items()){verified(c.directory,name,sha);}
    validateSceneContent(c,sprites);return c;
}
