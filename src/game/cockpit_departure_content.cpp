#include "full_content.h"
#include "content_hash.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
using Json=nlohmann::json;
namespace{
void need(bool ok,const char* message){if(!ok){throw std::runtime_error(message);}}
std::string verified(const std::filesystem::path& dir,const std::string& name,const std::string& sha){
 need(std::filesystem::path(name).filename()==name,"Invalid cockpit resource path");const auto file=dir/name;
 need(!std::filesystem::is_symlink(file)&&std::filesystem::is_regular_file(file)&&std::filesystem::file_size(file)<=128*1024*1024,"Missing cockpit resource");
 std::ifstream input(file,std::ios::binary);need(bool(input),"Cannot open cockpit resource");const auto raw=readContentBytes(input);need(contentSha256(raw)==sha,"Changed cockpit resource");return raw;
}
}
CombatContent loadCockpitDepartureContent(const std::filesystem::path& path,const Json& data){
 need(data.size()==4&&data.at("schema")=="nobihaza.g7departure.v1"&&data.at("content_compatibility")=="post-f5-g7-normal-1","Unknown cockpit identity");
 const auto& source=data.at("source");need(source==Json{{"file","g7-01-source.json"},{"sha256","03789b0cf7ecfc225738e5ebc2ebe92dc81b51a5e4ff9aebc517cc1045007628"}},"Unknown cockpit predecessor");
 const auto& p=data.at("projection");need(contentSha256(p.dump())=="54764ac75b78592582107c85b127e8ded195c7da18e6777d35c66ae038d8e894","Unreviewed cockpit projection");
 std::istringstream input(verified(path.parent_path(),source.at("file"),source.at("sha256")));auto c=loadCombatContentDefinition(path.parent_path()/source.at("file").get<std::string>(),parseCombatContentDefinition(input));
 // All durable facts and legitimate save windows are unchanged from G7-01.
 for(const auto* group:{"images","audio","cues","animations"}){for(const auto& [id,row]:p.at(group).items()){c.setMediaDefinition(group,id,row);}}
 for(const auto& [id,row]:p.at("maps").items()){need(!c.maps.contains(std::stoi(id)),"Cockpit shadows delivered map");}
 for(const auto& [id,row]:p.at("objects").items()){need(!c.objects.contains(id),"Cockpit shadows delivered object");}
 auto sprites=*c.maps.begin()->second.sprites;const auto added=readSceneDefinitions(c,Json{{"font",p.at("font")},{"sprites",p.at("scene_sprites")},{"maps",p.at("maps")},{"objects",p.at("objects")}});
 for(const auto& [id,row]:added){sprites.insert_or_assign(id,row);}const auto sharedSprites=std::make_shared<const SpriteDefinitions>(sprites);for(auto& [id,map]:c.maps){map.sprites=sharedSprites;}
 for(const auto& [id,row]:p.at("messages").items()){c.messages.insert_or_assign(id,readCombatMessage(row,true));}
 for(const auto& [id,row]:p.at("cues").items()){need(!row.contains("runtime_support"),"Unsupported cockpit cue");if(row.at("kind")=="forced_route"){c.routes.insert_or_assign(id,readRouteDefinition(row));}}
 for(const auto& [id,row]:p.at("flows").items()){need(!c.catalog["flows"].contains(id),"Cockpit shadows delivered flow");c.catalog["flows"][id]=row;}
 c.catalog["g7"]["departure"]=p.at("departure");c.catalog["g7"]["delivered"]=2;
 for(const auto& [id,row]:p.at("map_music").items()){need(!c.expansion.mapMusic.contains(id),"Cockpit shadows music");c.expansion.mapMusic[id]=row;}
 for(int map:{206,207,208}){c.catalog["pending_maps"].erase(std::to_string(map));}
 auto entry=c.entries.at("final-capability");entry.map=206;entry.playerTile={30,8};entry.direction=Direction::Right;entry.session.worldFlags[c.facts.switches.at(529)]=true;
 c.entries.emplace("departure-capability",entry);
 for(const auto& [name,sha]:p.at("resources").items()){verified(c.directory,name,sha);}
 validateSceneContent(c,sprites);return c;
}
