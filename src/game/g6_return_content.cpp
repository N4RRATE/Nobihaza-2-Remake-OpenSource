#include "full_content.h"
#include "content_hash.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
using Json=nlohmann::json;
namespace{
void need(bool value,const char* text){if(!value){throw std::runtime_error(text);}}
std::string verified(const std::filesystem::path& dir,const std::string& name,const std::string& sha){
 need(std::filesystem::path(name).filename()==name,"Invalid Return resource path");const auto path=dir/name;
 need(!std::filesystem::is_symlink(path)&&std::filesystem::is_regular_file(path)&&std::filesystem::file_size(path)<=128*1024*1024,"Missing Return resource");
 std::ifstream input(path,std::ios::binary);need(bool(input),"Cannot open Return source/media");const auto raw=readContentBytes(input);need(contentSha256(raw)==sha,"Changed Return source/media");return raw;
}
}
CombatContent loadReturnContent(const std::filesystem::path& path,const Json& data){
 if(data.at("content_compatibility")=="post-f5-g6-normal-5"){
  need(data.size()==4&&data.at("schema")=="nobihaza.g6.v1"&&data.at("source")==Json{{"file","g6-04-source.json"},{"sha256","1ee8819360d9e155bc633df51c1512d2ba61b9d4626c2b0fb01f57141118b459"}},"Unknown direct source");
  const auto& p=data.at("projection");need(contentSha256(p.dump())=="be0dbf2e927d7fc853be7af6091f13c14b10cff6416cb94917275cca0eb585de","Unreviewed direct profile");
  std::istringstream input(verified(path.parent_path(),"g6-04-source.json",data.at("source").at("sha256")));auto old=std::make_shared<const CombatContent>(loadCombatContentDefinition(path.parent_path()/"g6-04-source.json",parseCombatContentDefinition(input)));auto c=*old;c.legacySource=old;c.compatibility="post-f5-g6-normal-5";c.catalog["warehouse_direct"]=p.at("warehouse_direct");c.catalog["g6"]["delivered"]=5;for(const auto& point:p.at("save_points")){c.catalog["save_points"].push_back(point);}return c;
 }
 const bool warehouse=data.at("content_compatibility")=="post-f5-g6-normal-4";
 const bool memorial=data.at("content_compatibility")=="post-f5-g6-normal-3";
 const bool terminal=data.at("content_compatibility")=="post-f5-g6-normal-2";
 need(data.size()==4&&data.at("schema")=="nobihaza.g6.v1"&&(warehouse||memorial||terminal||data.at("content_compatibility")=="post-f5-g6-normal-1"),"Unknown Return identity");
 const auto& source=data.at("source");need(source.size()==2&&source.at("file")== (warehouse?"g6-03-source.json":memorial?"g6-02-source.json":terminal?"g6-01-source.json":"g5-03-source.json")&&source.at("sha256")== (warehouse?"82e84d61e59acf0e4705bc19b33c8cad7ce4c99e090599cb0b98f7316291e29c":memorial?"c5488d260f603239f4ade665640f32d8baa0490e5b90fd079da381b007d820dd":terminal?"082d02d2fe7bcbec7a50d1ac4fc856a53b875c06295042c38076640e27f7c543":"4d8fe8f4facd71a0cf94b99e239394b4f99745ae13d7a440aed4bbf030e90156"),"Unknown Return source");
 const auto& p=data.at("projection");need(contentSha256(p.dump())== (warehouse?"50dc2a6438825aff2c9f12a0d2e2a20312f62ba9f524d61fc38880cf28c62c9e":memorial?"6f557bec26c608458792a86586175e0ec2d6dc9a0047a71a3e9efbfcf6410778":terminal?"e3ade74186991a46446d4414bd8b3bbf340cbe9700043351d8d15b4e6d6caad3":"38a4da6d9c7907d55f12f1b685bd4191a0e2941cc529bba4c2e0a52a5ec0d7c2"),"Unreviewed Return projection");
 std::istringstream input(verified(path.parent_path(),source.at("file"),source.at("sha256")));auto old=std::make_shared<const CombatContent>(loadCombatContentDefinition(path.parent_path()/source.at("file").get<std::string>(),parseCombatContentDefinition(input)));
 auto c=*old;c.legacySource=old;c.compatibility=data.at("content_compatibility");

 for(const auto& [id,name]:p.at("facts").items()){need(c.facts.switches.emplace(std::stoi(id),name.get<std::string>()).second,"Duplicate Return fact");c.catalog["g6"]["facts"][id]=name;}
 for(const auto& [id,name]:p.at("values").items()){need(c.facts.variables.emplace(std::stoi(id),name.get<std::string>()).second,"Duplicate Return value");c.catalog["g6"]["values"][id]=name;}
 for(const auto* group:{"images","audio","cues","animations"}){for(const auto& [id,row]:p.at(group).items()){c.setMediaDefinition(group,id,row);}}
 for(const auto& [id,row]:p.at("maps").items()){need(!c.maps.contains(std::stoi(id)),"Return shadows delivered map");}
 for(const auto& [id,row]:p.at("objects").items()){if(c.objects.contains(id)){need(row.at("map")==c.objects.at(id).map&&row.at("initial_tile")==Json::array({c.objects.at(id).tile.x,c.objects.at(id).tile.y}),"Return shadows delivered object");c.objects.erase(id);}}
 if(terminal){for(const auto& [id,row]:p.at("objects").items()){if(!old->objects.contains(id)){c.catalog["legacy_scene_objects"].push_back(id);}}}
 auto sprites=*c.maps.begin()->second.sprites;const auto added=readSceneDefinitions(c,Json{{"font",p.at("font")},{"sprites",p.at("scene_sprites")},{"maps",p.at("maps")},{"objects",p.at("objects")}});
 for(const auto& [id,row]:added){sprites.insert_or_assign(id,row);}const auto sharedSprites=std::make_shared<const SpriteDefinitions>(sprites);for(auto& [id,map]:c.maps){map.sprites=sharedSprites;}
 if(p.contains("lower_tiles")){installLowerTileContent(c,p.at("lower_tiles"));}
 if(terminal||warehouse){for(const auto& [map,rows]:p.at("tile_substitutions").items()){for(const auto& row:rows){
  UpperTileReplacementDefinition replacement;replacement.fromIndex=row.at("from_index");replacement.toIndex=row.at("to_index");
  replacement.image=c.directory/c.media.at("images").at(row.at("image").get<std::string>()).at("file").get<std::string>();replacement.underlay=c.directory/c.media.at("images").at(row.at("underlay").get<std::string>()).at("file").get<std::string>();
  for(const auto& cell:row.at("cells")){replacement.tiles.push_back(cell.at("tile").get<std::array<int,2>>());replacement.passageMasks.push_back(cell.at("mask"));if(warehouse){replacement.cellUnderlays.push_back(c.directory/c.media.at("images").at(cell.at("underlay").get<std::string>()).at("file").get<std::string>());}}c.maps.at(std::stoi(map)).upperTileReplacements.push_back(std::move(replacement));
 }}}
 for(const auto& [id,row]:p.at("items").items()){need(c.items.contains(std::stoi(id)),"Return invents archive item");}
 for(const auto& [id,row]:p.at("messages").items()){c.messages.insert_or_assign(id,readCombatMessage(row,true));}
 for(const auto& [id,row]:p.at("cues").items()){need(!row.contains("runtime_support"),"Unsupported Return cue");if(row.at("kind")=="forced_route"){c.routes.insert_or_assign(id,readRouteDefinition(row));}}
 for(const auto& [id,row]:p.at("flows").items()){if(c.catalog["flows"].contains(id)){need(c.catalog["flows"].at(id).at("object")==row.at("object")&&c.catalog["flows"].at(id).at("page")==row.at("page"),"Return changes flow identity");}c.catalog["flows"][id]=row;}
 c.catalog["g6"]["delivered"]=warehouse?4:memorial?3:terminal?2:1;
 if(terminal){c.catalog["g6"]["terminal_contract"]=p.at("terminal_contract");c.catalog["g6"]["alarm"]=p.at("alarm");
  need(p.at("alarm").at("flag")==529&&p.at("alarm").at("steps").size()==27,"Incomplete finite alarm");
  for(const auto& step:p.at("alarm").at("steps")){if(step.contains("sound")){const auto sound=readSound(step.at("sound"));need(c.media.at("audio").contains(sound.resource),"Missing alarm sound");}else{need(step.size()==1&&step.at("wait_ticks")==60,"Changed alarm wait");}}
 }else if(warehouse){c.catalog["warehouse_g4"]=p.at("warehouse_g4");c.catalog["pending_maps"].erase("204");auto& effects=c.enemyBurnSlots[c.full->at("runtime").contains("combat_effects")?"combat_effects":"weapon_actions"];effects[p.at("warehouse_g4").at("object").get<std::string>()]=1;
 }else if(memorial){c.catalog["g6"]["memorial_contract"]=p.at("memorial_contract");c.catalog["g6"]["save_mixing"]=p.at("save_mixing");need(p.at("save_mixing").at("after_flag")==530&&p.at("save_mixing").at("variant")=="preserved","Unknown return save mixing boundary");}
 else{c.catalog["g6"]["return_contract"]=p.at("return_contract");}
 for(const auto* group:{"chimeras","zombies"}){if(!p.contains(group)){continue;}for(const auto& [id,row]:p.at(group).items()){
  const auto& rules=row.at("rules");need(c.objects.contains(id)&&rules.at("behavior")== (std::string(group)=="chimeras"?"chimera":"zombie"),"Invalid return enemy");
  need(c.enemies.emplace(id,EnemyDefinition{id,row.at("map"),row.at("event"),row.at("dead_fact"),false,rules}).second,"Duplicate return enemy");
  c.enemyBurnSlots["weapon_actions"][id]=rules.at("burn_slot");
 }}

 if(p.contains("acid_projectiles")){for(const auto& [id,row]:p.at("acid_projectiles").items()){need(!c.catalog["acid_projectiles"].contains(id)||!c.catalog["acid_projectiles"].at(id).value("runtime_enabled",true),"Return changes delivered acid");c.catalog["acid_projectiles"][id]=row;}}
 if(!memorial&&!warehouse){for(auto& point:c.catalog["save_points"]){if(point.at("object")=="m196-e2"){need(point.at("absent_flags")== (terminal?Json::array({528}):Json::array({528,530})),"Changed inherited control save phase");point["absent_flags"]=terminal?Json::array():Json::array({528});}}}
 for(const auto& point:p.value("save_points",Json::array())){c.catalog["save_points"].push_back(point);}
 for(const auto& [id,row]:p.at("map_music").items()){need(!c.expansion.mapMusic.contains(id),"Return shadows music");c.expansion.mapMusic[id]=row;}
 for(auto& [id,entry]:c.entries){initializeFullSession(c,entry.session);}
 for(const auto& [name,sha]:p.at("resources").items()){verified(c.directory,name,sha);}
 validateSceneContent(c,sprites);return c;
}
