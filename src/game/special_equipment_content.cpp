#include "full_content.h"
#include "content_hash.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
using Json=nlohmann::json;
namespace{
void need(bool ok,const char* text){if(!ok){throw std::runtime_error(text);}}
std::string verified(const std::filesystem::path& dir,const std::string& name,const std::string& sha){
 need(std::filesystem::path(name).filename()==name,"Invalid equipment resource path");const auto file=dir/name;
 need(!std::filesystem::is_symlink(file)&&std::filesystem::is_regular_file(file)&&std::filesystem::file_size(file)<=128*1024*1024,"Missing equipment resource");
 std::ifstream in(file,std::ios::binary);need(bool(in),"Cannot read equipment resource");const auto bytes=readContentBytes(in);need(contentSha256(bytes)==sha,"Changed equipment resource");return bytes;
}
}
CombatContent loadSpecialEquipmentContent(const std::filesystem::path& path,const Json& data){
 need(data.size()==4&&data.at("schema")=="nobihaza.g7equipment.v1"&&data.at("content_compatibility")=="post-f5-g7-normal-6","Unknown equipment identity");
 const auto& source=data.at("source");need(source==Json{{"file","g7-05-source.json"},{"sha256","cb70c4bd43248f1bf6ca0bec361d773a6115d06bc3f80329b819a7fc07879164"}},"Unknown equipment predecessor");
 const auto& p=data.at("projection");need(contentSha256(p.dump())=="6dc1a57b7751449c8b74824a8760d5050a3624d639ec388b6768e5dfee563bc8","Unreviewed equipment projection");
 std::istringstream in(verified(path.parent_path(),source.at("file"),source.at("sha256")));auto old=std::make_shared<const CombatContent>(loadCombatContentDefinition(path.parent_path()/source.at("file").get<std::string>(),parseCombatContentDefinition(in)));
 auto c=*old;c.legacySource=old;c.compatibility=data.at("content_compatibility");
 for(const auto* group:{"images","audio","cues","animations"}){for(const auto& [id,row]:p.at(group).items()){need(!c.hasMediaDefinition(group,id),"Equipment shadows delivered media");c.setMediaDefinition(group,id,row);}}
 auto sprites=*c.maps.begin()->second.sprites;
 const auto added=readSceneDefinitions(c,Json{{"font",Json{{"file",c.maps.begin()->second.fontPath.filename().string()}}},{"sprites",p.at("sprites")},{"maps",Json::object()},{"objects",Json::object()}});
 for(const auto& [id,row]:added){need(!sprites.contains(id),"Equipment shadows delivered sprite");sprites.emplace(id,row);}const auto sharedSprites=std::make_shared<const SpriteDefinitions>(sprites);for(auto& [id,map]:c.maps){map.sprites=sharedSprites;}
 for(const auto& [id,row]:p.at("cues").items()){if(row.at("kind")=="forced_route"){c.routes.emplace(id,readRouteDefinition(row));}}
 for(const auto& row:p.at("items")){const auto item=readItemDefinition(row);need(!c.items.contains(item.id),"Equipment shadows delivered item");c.items.emplace(item.id,item);}
 for(const auto& [name,row]:p.at("profiles").items()){need(!c.actionProfiles.contains(name),"Equipment shadows delivered profile");c.actionProfiles[name]=row;}
 for(const auto& [id,row]:p.at("bindings").items()){
  auto b=row;if(b.contains("base")){b=c.catalog.at("weapon_bindings").at(std::to_string(row.at("base").get<int>()));for(const auto& [key,v]:row.items()){if(key!="base"){b[key]=v;}}}
  if(id=="56"){b["quick_reload"]=p.at("reload_equipment").at(id).at("profile");}
  c.catalog["weapon_bindings"][id]=b;
 }
 // CE42 has no I56 distance loss; I58 retains its explicit receiver5.
 c.expansion.attacks.emplace(58,c.expansion.attacks.at(21));
 for(const auto& [id,row]:p.at("reload_equipment").items()){c.expansion.reloadEquipment[id]=row;}
 for(const auto& [map,rows]:p.at("map_profiles").items()){if(c.maps.contains(std::stoi(map))){for(const auto& [item,row]:rows.items()){c.catalog["weapon_map_profiles"][map][item]=row;}}}
 c.requiredMagazines.emplace(56,6);c.magazineSeedBindings.emplace(64,56);
 for(auto& [id,entry]:c.entries){entry.session.magazines.emplace(56,6);}
 const auto& overdraw=p.at("lc9_overdraw");need(overdraw.at("item")==27&&overdraw.at("accessory")==55&&overdraw.at("minimum")==-999999&&overdraw.at("capacity")==7&&overdraw.at("box_debit")==7&&c.items.at(27).capacity==7,"Invalid LC9 signed contract");
 c.items.at(27).magazineMinimum=overdraw.at("minimum");c.items.at(27).overdrawAccessory=overdraw.at("accessory");c.items.at(27).boxReloadDebit=overdraw.at("box_debit");
 c.catalog["special_equipment"]={{"glasses",55},{"menu_common_event",89},{"source",p.at("source_contracts")}};
 for(const auto& [name,sha]:p.at("resources").items()){verified(c.directory,name,sha);}
 validateSceneContent(c,sprites);return c;
}
