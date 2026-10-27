#include "full_content.h"
#include "content_hash.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
using Json=nlohmann::json;
namespace{
void need(bool value,const char* text){if(!value){throw std::runtime_error(text);}}
std::string verified(const std::filesystem::path& dir,const std::string& name,const std::string& sha){
 need(std::filesystem::path(name).filename()==name,"Invalid Control resource path");const auto path=dir/name;
 need(!std::filesystem::is_symlink(path)&&std::filesystem::is_regular_file(path)&&std::filesystem::file_size(path)<=128*1024*1024,"Missing Control resource");
 std::ifstream input(path,std::ios::binary);need(bool(input),"Cannot open Control source/media");const auto raw=readContentBytes(input);need(contentSha256(raw)==sha,"Changed Control source/media");return raw;
}
}
CombatContent loadControlContent(const std::filesystem::path& path,const Json& data){
 need(data.size()==4&&data.at("schema")=="nobihaza.g5.v1"&&data.at("content_compatibility")=="post-f5-g5-normal-3","Unknown Control identity");
 const auto& source=data.at("source");need(source.size()==2&&source.at("file")=="g5-02-source.json"&&source.at("sha256")=="36cdbd3b7dd37da02f023b8646bca98f3334f0945d3c91f4370deb82209fe8be","Unknown Control source");
 const auto& p=data.at("projection");need(contentSha256(p.dump())=="9e2cf24d3e5e1ca1ca883740e381e9a0371f5e3cc1533799faae9b14f03258a0","Unreviewed Control projection");
 std::istringstream input(verified(path.parent_path(),source.at("file"),source.at("sha256")));auto old=std::make_shared<const CombatContent>(loadCombatContentDefinition(path.parent_path()/source.at("file").get<std::string>(),parseCombatContentDefinition(input)));
 auto c=*old;c.legacySource=old;c.compatibility=data.at("content_compatibility");
 for(const auto& [id,name]:p.at("facts").items()){need(c.facts.switches.emplace(std::stoi(id),name.get<std::string>()).second,"Duplicate Control fact");c.catalog["g5"]["facts"][id]=name;}
 for(const auto& [id,name]:p.at("values").items()){need(c.facts.variables.emplace(std::stoi(id),name.get<std::string>()).second,"Duplicate Control value");c.catalog["g5"]["values"][id]=name;}
 for(const auto* group:{"images","audio","cues","animations"}){for(const auto& [id,row]:p.at(group).items()){c.setMediaDefinition(group,id,row);}}
 for(const auto& [id,row]:p.at("maps").items()){need(!c.maps.contains(std::stoi(id)),"Control shadows delivered map");}
 for(const auto& [id,row]:p.at("objects").items()){if(c.objects.contains(id)){need(id.starts_with("m195-e")&&row.at("map")==195&&row.at("initial_tile")==Json::array({c.objects.at(id).tile.x,c.objects.at(id).tile.y}),"Control shadows delivered object");c.objects.erase(id);}}
 auto sprites=*c.maps.begin()->second.sprites;const auto added=readSceneDefinitions(c,Json{{"font",p.at("font")},{"sprites",p.at("scene_sprites")},{"maps",p.at("maps")},{"objects",p.at("objects")}});
 for(const auto& [id,row]:added){sprites.insert_or_assign(id,row);}const auto sharedSprites=std::make_shared<const SpriteDefinitions>(sprites);for(auto& [id,map]:c.maps){map.sprites=sharedSprites;}
 for(const auto& [id,row]:p.at("items").items()){need(c.items.contains(std::stoi(id)),"Control invents archive item");}
 for(const auto& [id,row]:p.at("messages").items()){c.messages.insert_or_assign(id,readCombatMessage(row,true));}
 for(const auto& [id,row]:p.at("cues").items()){need(!row.contains("runtime_support"),"Unsupported Control cue");if(row.at("kind")=="forced_route"){c.routes.insert_or_assign(id,readRouteDefinition(row));}}
 for(const auto& [id,row]:p.at("flows").items()){if(c.catalog["flows"].contains(id)){need(c.catalog["flows"].at(id).value("handoff_only",false)||!c.catalog["flows"].at(id).value("runtime_enabled",true),"Control changes delivered flow");}c.catalog["flows"][id]=row;}
 c.catalog["g5"]["delivered"]=3;c.catalog["g5"]["control_contract"]=p.at("control_contract");
 for(const auto& point:p.at("save_points")){c.catalog["save_points"].push_back(point);}
 for(const auto& [id,row]:p.at("map_music").items()){need(!c.expansion.mapMusic.contains(id),"Control shadows music");c.expansion.mapMusic[id]=row;}
 for(auto& [id,entry]:c.entries){initializeFullSession(c,entry.session);}
 for(const auto& [name,sha]:p.at("resources").items()){verified(c.directory,name,sha);}
 validateSceneContent(c,sprites);return c;
}
