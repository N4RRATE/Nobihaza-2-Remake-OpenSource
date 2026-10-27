#include "full_content.h"
#include "content_hash.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
using Json=nlohmann::json;
namespace {
void need(bool ok,const char* text){if(!ok){throw std::runtime_error(text);}}
std::string verified(const std::filesystem::path& dir,const std::string& name,const std::string& sha){
    need(std::filesystem::path(name).filename()==name,"Invalid epilogue resource path");const auto file=dir/name;
    need(!std::filesystem::is_symlink(file)&&std::filesystem::is_regular_file(file)&&std::filesystem::file_size(file)<=128*1024*1024,"Missing epilogue resource");
    std::ifstream in(file,std::ios::binary);need(bool(in),"Cannot read epilogue resource");const auto bytes=readContentBytes(in);need(contentSha256(bytes)==sha,"Changed epilogue resource");return bytes;
}
}
CombatContent loadEpilogueContent(const std::filesystem::path& path,const Json& data){
    need(data.size()==4&&data.at("schema")=="nobihaza.g7epilogue.v1"&&data.at("content_compatibility")=="post-f5-g7-normal-4","Unknown epilogue identity");
    const auto& source=data.at("source");need(source==Json{{"file","g7-04-source.json"},{"sha256","8d84565447bd9d59bfcaf4fa1934a4deaf56e588f84c34a73e6e78037c26e2d1"}},"Unknown epilogue predecessor");
    const auto& p=data.at("projection");need(contentSha256(p.dump())=="bc21db6bb48f34676ef898ace82b1ec2c3f751e4953d83b8f4ed8f663e2d992f","Unreviewed epilogue projection");
    std::istringstream in(verified(path.parent_path(),source.at("file"),source.at("sha256")));auto old=std::make_shared<const CombatContent>(loadCombatContentDefinition(path.parent_path()/source.at("file").get<std::string>(),parseCombatContentDefinition(in)));
    auto c=*old;c.legacySource=old;
    for(const auto* group:{"images","audio","cues","animations"}){for(const auto& [id,row]:p.at(group).items()){
        const bool replacement=std::string(group)=="cues"&&(id.starts_with("m209-e10-p10-")||id.starts_with("m207-e2-p3-"));
        need(!c.hasMediaDefinition(group,id)||replacement,"Epilogue shadows delivered media");c.setMediaDefinition(group,id,row);
    }}
    for(const auto& [id,row]:p.at("maps").items()){need(!c.maps.contains(std::stoi(id)),"Epilogue shadows delivered map");}
    for(const auto& [id,row]:p.at("objects").items()){need(!c.objects.contains(id),"Epilogue shadows delivered object");}
    auto sprites=*c.maps.begin()->second.sprites;const auto added=readSceneDefinitions(c,Json{{"font",p.at("font")},{"sprites",p.at("scene_sprites")},{"maps",p.at("maps")},{"objects",p.at("objects")}});
    for(const auto& [id,row]:added){sprites.insert_or_assign(id,row);}const auto sharedSprites=std::make_shared<const SpriteDefinitions>(sprites);for(auto& [id,map]:c.maps){map.sprites=sharedSprites;}
    for(const auto& [id,row]:p.at("messages").items()){need(c.messages.emplace(id,readCombatMessage(row,true)).second,"Epilogue shadows delivered message");}
    for(const auto& [id,row]:p.at("cues").items()){need(!row.contains("runtime_support"),"Unsupported epilogue cue");if(row.at("kind")=="forced_route"){c.routes.insert_or_assign(id,readRouteDefinition(row));}}
    for(const auto& [id,row]:p.at("flows").items()){
        if(c.catalog["flows"].contains(id)){need(id=="m207-e2-p3"&&!c.catalog["flows"].at(id).value("runtime_enabled",true),"Epilogue shadows delivered flow");}
        c.catalog["flows"][id]=row;
    }
    c.catalog["epilogue"]=p.at("epilogue");
    for(const auto& [id,row]:p.at("map_music").items()){need(!c.expansion.mapMusic.contains(id),"Epilogue shadows delivered map music");c.expansion.mapMusic[id]=row;}
    for(int map:{210,211,212}){c.catalog["pending_maps"].erase(std::to_string(map));c.pendingMapTickets.erase(map);}
    for(const auto& [name,sha]:p.at("resources").items()){verified(c.directory,name,sha);}
    validateSceneContent(c,sprites);return c;
}
