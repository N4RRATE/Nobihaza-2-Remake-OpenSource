#include "full_content.h"
#include "content_hash.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
using Json=nlohmann::json;
namespace {
void need(bool ok,const char* message){if(!ok){throw std::runtime_error(message);}}
std::string verified(const std::filesystem::path& dir,const std::string& name,const std::string& sha){
 need(std::filesystem::path(name).filename()==name,"Invalid cargo resource path");const auto file=dir/name;
 need(!std::filesystem::is_symlink(file)&&std::filesystem::is_regular_file(file)&&std::filesystem::file_size(file)<=128*1024*1024,"Missing cargo resource");
 std::ifstream input(file,std::ios::binary);need(bool(input),"Cannot open cargo resource");const auto raw=readContentBytes(input);need(contentSha256(raw)==sha,"Changed cargo resource");return raw;
}
}
CombatContent loadCargoPlatformContent(const std::filesystem::path& path,const Json& data){
 need(data.size()==4&&data.at("schema")=="nobihaza.g6cargo.v1"&&data.at("content_compatibility")=="post-f5-g7-normal-1","Unknown cargo identity");
 const auto& source=data.at("source");need(source==Json{{"file","g7-02-source.json"},{"sha256","20ed575abb9d5dcdf658b784732408a6c023923f740d459f7bd11b4ab0aa1e32"}},"Unknown cargo predecessor");
 const auto& p=data.at("projection");need(contentSha256(p.dump())=="acb11ba6651c170cda784d8cf58ebff1abf6cff590d3aa3b9e9577631db7d9c4","Unreviewed cargo projection");
 std::istringstream input(verified(path.parent_path(),source.at("file"),source.at("sha256")));auto c=loadCombatContentDefinition(path.parent_path()/source.at("file").get<std::string>(),parseCombatContentDefinition(input));
 for(const auto* group:{"images","audio","cues","animations"}){for(const auto& [id,row]:p.at(group).items()){c.setMediaDefinition(group,id,row);}}
 auto sprites=*c.maps.begin()->second.sprites;const auto added=readSceneDefinitions(c,Json{{"font",p.at("font")},{"sprites",p.at("scene_sprites")},{"maps",p.at("maps")},{"objects",p.at("objects")}});
 for(const auto& [id,row]:added){sprites.insert_or_assign(id,row);}const auto sharedSprites=std::make_shared<const SpriteDefinitions>(sprites);for(auto& [id,map]:c.maps){map.sprites=sharedSprites;}
 for(const auto& [id,row]:p.at("messages").items()){c.messages.insert_or_assign(id,readCombatMessage(row,true));}
 for(const auto& [id,row]:p.at("cues").items()){need(!row.contains("runtime_support"),"Unsupported cargo cue");if(row.at("kind")=="forced_route"){c.routes.insert_or_assign(id,readRouteDefinition(row));}}
 for(const auto& [id,row]:p.at("flows").items()){need(!c.catalog["flows"].contains(id),"Cargo shadows delivered flow");c.catalog["flows"][id]=row;}
 installLowerTileContent(c,p.at("lower_tiles"));c.catalog["g6"]["cargo"]=p.at("cargo");c.catalog["g6"]["delivered"]=6;
 for(const auto& [name,sha]:p.at("resources").items()){verified(c.directory,name,sha);}
 validateSceneContent(c,sprites);return c;
}
