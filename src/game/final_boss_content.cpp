#include "full_content.h"
#include "content_hash.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
using Json=nlohmann::json;
namespace{
void need(bool value,const char* text){if(!value){throw std::runtime_error(text);}}
std::string verified(const std::filesystem::path& dir,const std::string& name,const std::string& sha){
 need(std::filesystem::path(name).filename()==name,"Invalid final resource path");const auto path=dir/name;
 need(!std::filesystem::is_symlink(path)&&std::filesystem::is_regular_file(path)&&std::filesystem::file_size(path)<=128*1024*1024,"Missing final source/media");
 std::ifstream input(path,std::ios::binary);need(bool(input),"Cannot open final source/media");const auto raw=readContentBytes(input);need(contentSha256(raw)==sha,"Changed final source/media");return raw;
}
}
CombatContent loadFinalContent(const std::filesystem::path& path,const Json& data){
 need(data.size()==4&&data.at("schema")=="nobihaza.g7.v1"&&data.at("content_compatibility")=="post-f5-g7-normal-1","Unknown final identity");
 const auto& source=data.at("source");need(source==Json{{"file","g6-05-source.json"},{"sha256","f64f34e83aa935c46fdcd9639a53f9dedd506408eada65de28f22a1708af8290"}},"Unknown final inherited source");
 const auto& p=data.at("projection");need(contentSha256(p.dump())=="4145c1b63b7d8db5f0b4940eba200e60a610468ae0846683b42a3975a147bd6b","Unreviewed final projection");
 std::istringstream input(verified(path.parent_path(),source.at("file"),source.at("sha256")));auto old=std::make_shared<const CombatContent>(loadCombatContentDefinition(path.parent_path()/source.at("file").get<std::string>(),parseCombatContentDefinition(input)));
 auto c=*old;c.legacySource=old;c.compatibility=data.at("content_compatibility");
 for(const auto& [id,name]:p.at("facts").items()){const int flag=std::stoi(id);if(c.facts.switches.contains(flag)){need(c.facts.switches.at(flag)==name.get<std::string>(),"Changed inherited final fact");}else{need(c.facts.switches.emplace(flag,name.get<std::string>()).second,"Duplicate final fact");}c.catalog["g7"]["facts"][id]=name;}
 for(const auto& [id,name]:p.at("values").items()){need(c.facts.variables.emplace(std::stoi(id),name.get<std::string>()).second,"Duplicate final value");c.catalog["g7"]["values"][id]=name;}
 for(const auto* group:{"images","audio","cues","animations"}){for(const auto& [id,row]:p.at(group).items()){c.setMediaDefinition(group,id,row);}}
 for(const auto& [id,row]:p.at("maps").items()){need(!c.maps.contains(std::stoi(id)),"Final shadows delivered map");}
 for(const auto& [id,row]:p.at("objects").items()){need(!c.objects.contains(id),"Final shadows delivered object");}
 auto sprites=*c.maps.begin()->second.sprites;const auto added=readSceneDefinitions(c,Json{{"font",p.at("font")},{"sprites",p.at("scene_sprites")},{"maps",p.at("maps")},{"objects",p.at("objects")}});
 for(const auto& [id,row]:added){sprites.insert_or_assign(id,row);}const auto sharedSprites=std::make_shared<const SpriteDefinitions>(sprites);for(auto& [id,map]:c.maps){map.sprites=sharedSprites;}
 for(const auto& [id,row]:p.at("messages").items()){c.messages.insert_or_assign(id,readCombatMessage(row,true));}
 for(const auto& [id,row]:p.at("cues").items()){need(!row.contains("runtime_support"),"Unsupported final cue");if(row.at("kind")=="forced_route"){c.routes.insert_or_assign(id,readRouteDefinition(row));}}
 for(const auto& [id,row]:p.at("items").items()){const auto item=readItemDefinition(row);need((item.id==23&&item.equipmentType==1&&item.capacity==0&&item.attack==89)||(item.id==49&&item.equipmentType==3&&item.defense==100),"Unexpected final equipment");c.items.insert_or_assign(item.id,item);}
 c.catalog["final_boss"]=p.at("final_boss");c.catalog["g7"]["delivered"]=1;c.catalog["pending_maps"].erase("209");
 auto& effects=c.enemyBurnSlots[c.full->at("runtime").contains("combat_effects")?"combat_effects":"weapon_actions"];effects[p.at("final_boss").at("object").get<std::string>()]=1;

 for(const auto& [id,row]:p.at("map_music").items()){need(!c.expansion.mapMusic.contains(id),"Final shadows music");c.expansion.mapMusic[id]=row;}
 for(auto& [id,entry]:c.entries){initializeFullSession(c,entry.session);}
 // Explicit capability entry; the cockpit's natural transition is owned by G7-02.
 auto entry=c.entries.at("post-pickup");entry.map=209;entry.playerTile={10,8};entry.direction=Direction::Left;entry.positions.clear();entry.pages.clear();entry.directions.clear();entry.through.clear();entry.session.hp=entry.session.maxHp;entry.session.mainWeapon=21;entry.session.auxiliaryWeapon=0;entry.session.armor=0;entry.session.flags[1]=true;entry.session.flags[18]=true;entry.session.flags[586]=false;entry.session.magazines[21]=7;entry.session.inventory[21]=0;entry.session.inventory[23]=entry.session.inventory[49]=0;entry.session.inventory[c.items.at(21).looseItem]=24;entry.session.inventory[1]=5;c.entries.emplace("final-capability",entry);
 for(const auto& [name,sha]:p.at("resources").items()){verified(c.directory,name,sha);}
 validateSceneContent(c,sprites);return c;
}
