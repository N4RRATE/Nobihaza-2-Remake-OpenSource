#include "full_content.h"
#include "content_hash.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
using Json=nlohmann::json;
namespace {
void need(bool ok,const char* text){if(!ok){throw std::runtime_error(text);}}
std::string verified(const std::filesystem::path& dir,const std::string& name,const std::string& sha){
    need(std::filesystem::path(name).filename()==name,"Invalid score resource path");const auto file=dir/name;
    need(!std::filesystem::is_symlink(file)&&std::filesystem::is_regular_file(file)&&std::filesystem::file_size(file)<=128*1024*1024,"Missing score resource");
    std::ifstream in(file,std::ios::binary);need(bool(in),"Cannot read score resource");const auto bytes=readContentBytes(in);need(contentSha256(bytes)==sha,"Changed score resource");return bytes;
}
}
CombatContent loadScoreContent(const std::filesystem::path& path,const Json& data){
    need(data.size()==4&&data.at("schema")=="nobihaza.g7score.v1"&&data.at("content_compatibility")=="post-f5-g7-normal-4","Unknown score identity");
    const auto& source=data.at("source");need(source==Json{{"file","g6-06-source.json"},{"sha256","321c74219dc40643f60c250367b8043ecafefd40c2199e1f2feaeeb3d2d0cbfa"}},"Unknown score predecessor");
    const auto& p=data.at("projection");need(contentSha256(p.dump())=="638de2153b6a07c3f5c0e3fdf02573d9f182db06343e6ff73077646741d3c20d","Unreviewed score projection");
    std::istringstream in(verified(path.parent_path(),source.at("file"),source.at("sha256")));auto old=std::make_shared<const CombatContent>(loadCombatContentDefinition(path.parent_path()/source.at("file").get<std::string>(),parseCombatContentDefinition(in)));
    auto c=*old;c.legacySource=old;c.compatibility=data.at("content_compatibility");
    for(const auto* group:{"images","audio","cues","animations"}){for(const auto& [id,row]:p.at(group).items()){need(!c.hasMediaDefinition(group,id),"Score shadows delivered media");c.setMediaDefinition(group,id,row);}}
    for(const auto& [id,row]:p.at("maps").items()){need(!c.maps.contains(std::stoi(id)),"Score shadows delivered map");}
    for(const auto& [id,row]:p.at("objects").items()){need(!c.objects.contains(id),"Score shadows delivered object");}
    auto sprites=*c.maps.begin()->second.sprites;const auto added=readSceneDefinitions(c,Json{{"font",p.at("font")},{"sprites",p.at("scene_sprites")},{"maps",p.at("maps")},{"objects",p.at("objects")}});
    for(const auto& [id,row]:added){sprites.insert_or_assign(id,row);}const auto sharedSprites=std::make_shared<const SpriteDefinitions>(sprites);for(auto& [id,map]:c.maps){map.sprites=sharedSprites;}
    for(const auto& [id,row]:p.at("messages").items()){need(c.messages.emplace(id,readCombatMessage(row,true)).second,"Score shadows delivered message");}
    c.catalog["score"]=p.at("score");
    for(const auto& [id,name]:p.at("score").at("facts").items()){need(c.facts.switches.emplace(std::stoi(id),name.get<std::string>()).second,"Duplicate score fact");}
    for(const auto& [id,name]:p.at("score").at("values").items()){need(c.facts.variables.emplace(std::stoi(id),name.get<std::string>()).second,"Duplicate score value");}
    for(const auto& [id,row]:p.at("flows").items()){need(!c.catalog["flows"].contains(id),"Score shadows delivered flow");c.catalog["flows"][id]=row;}
    for(int map:{213,214}){c.catalog["pending_maps"].erase(std::to_string(map));c.pendingMapTickets.erase(map);}
    c.catalog["pending_maps"]["215"]=5;
    for(auto& [id,entry]:c.entries){initializeFullSession(c,entry.session);}
    for(const auto& [name,sha]:p.at("resources").items()){verified(c.directory,name,sha);}
    validateSceneContent(c,sprites);return c;
}
