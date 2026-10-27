#include "full_content.h"
#include "content_hash.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
using Json=nlohmann::json;
namespace{
void need(bool value,const char* text){if(!value){throw std::runtime_error(text);}}
std::string verified(const std::filesystem::path& dir,const std::string& name,const std::string& sha){
 need(std::filesystem::path(name).filename()==name,"Invalid Dora resource path");const auto path=dir/name;
 need(!std::filesystem::is_symlink(path)&&std::filesystem::is_regular_file(path)&&std::filesystem::file_size(path)<=128*1024*1024,"Missing Dora resource");
 std::ifstream input(path,std::ios::binary);std::string raw{std::istreambuf_iterator<char>(input),{}};need(contentSha256(raw)==sha,"Changed Dora source/media");return raw;
}
}
CombatContent loadDoraContent(const std::filesystem::path& path,const Json& data){
 need(data.size()==4&&data.at("schema")=="nobihaza.g4.v1"&&data.at("content_compatibility")=="post-f5-g4-normal-2","Unknown Dora identity");
 const auto& source=data.at("source");need(source.size()==2&&source.at("file")=="g4-01-source.json"&&source.at("sha256")=="268dd15d535a824ea3a897e0f60fbdc15dfacb984eb03e17f033e009612e21bc","Unknown Dora source");
 const auto& p=data.at("projection");need(contentSha256(p.dump())=="c847034602898e17c109419283fb55850b403f865aa07a0859c8a2ed0b2ef3a1","Unreviewed Dora projection");
 std::istringstream input(verified(path.parent_path(),source.at("file"),source.at("sha256")));auto old=std::make_shared<const CombatContent>(loadCombatContentDefinition(path.parent_path()/source.at("file").get<std::string>(),parseCombatContentDefinition(input)));
 auto c=*old;c.legacySource=old;c.compatibility=data.at("content_compatibility");
 for(const auto& [id,name]:p.at("facts").items()){need(c.facts.switches.emplace(std::stoi(id),name.get<std::string>()).second,"Duplicate Dora fact");c.catalog["g4"]["facts"][id]=name;}
 for(const auto* group:{"images","audio","cues","animations"}){for(const auto& [id,row]:p.at(group).items()){c.setMediaDefinition(group,id,row);}}
 for(const auto& [id,row]:p.at("maps").items()){need(!c.maps.contains(std::stoi(id)),"Dora shadows delivered map");}
 for(const auto& [id,row]:p.at("objects").items()){if(c.objects.contains(id)){need(id=="m190-e8"&&row.at("map")==190&&row.at("initial_tile")==Json::array({c.objects.at(id).tile.x,c.objects.at(id).tile.y}),"Dora shadows delivered object");c.objects.erase(id);}}
 auto sprites=*c.maps.begin()->second.sprites;const auto added=readSceneDefinitions(c,Json{{"font",p.at("font")},{"sprites",p.at("scene_sprites")},{"maps",p.at("maps")},{"objects",p.at("objects")}});
 for(const auto& [id,row]:added){sprites.insert_or_assign(id,row);}const auto sharedSprites=std::make_shared<const SpriteDefinitions>(sprites);for(auto& [id,map]:c.maps){map.sprites=sharedSprites;}
 for(const auto& [id,row]:p.at("messages").items()){c.messages.insert_or_assign(id,readCombatMessage(row,true));}
 for(const auto& [id,row]:p.at("cues").items()){need(!row.contains("runtime_support"),"Unsupported Dora cue");if(row.at("kind")=="forced_route"){c.routes.insert_or_assign(id,readRouteDefinition(row));}}
 for(const auto& [id,row]:p.at("flows").items()){if(c.catalog["flows"].contains(id)){need(c.catalog["flows"].at(id).value("handoff_only",false)||!c.catalog["flows"].at(id).value("runtime_enabled",true),"Dora changes delivered flow");}c.catalog["flows"][id]=row;}
 c.catalog["pending_maps"].erase("192");c.catalog["g4"]["delivered"]=2;c.catalog["dora"]=p.at("dora");
 c.catalog["continue"]["restore_clear_flags"].push_back(504);
 for(const auto& [id,row]:p.at("map_music").items()){need(!c.expansion.mapMusic.contains(id),"Dora shadows music");c.expansion.mapMusic[id]=row;}
 for(auto& [id,entry]:c.entries){initializeFullSession(c,entry.session);}
 for(const auto& [name,sha]:p.at("resources").items()){verified(c.directory,name,sha);}
 validateSceneContent(c,sprites);return c;
}
