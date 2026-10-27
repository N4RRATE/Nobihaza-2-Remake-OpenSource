#include "full_content.h"
#include "content_hash.h"
#include <fstream>
#include <algorithm>
#include <sstream>
#include <stdexcept>
using Json=nlohmann::json;
namespace{
void need(bool ok,const char* message){if(!ok){throw std::runtime_error(message);}}
std::string verified(const std::filesystem::path& directory,const std::string& name,const std::string& sha){
    need(std::filesystem::path(name).filename()==name,"Invalid tower resource path");const auto file=directory/name;
    need(!std::filesystem::is_symlink(file)&&std::filesystem::is_regular_file(file)&&std::filesystem::file_size(file)<=128*1024*1024,"Missing tower resource");
    std::ifstream input(file,std::ios::binary);need(input.is_open(),"Cannot read tower resource");std::string bytes=readContentBytes(input);
    need(contentSha256(bytes)==sha,"Changed tower source/media");return bytes;
}
}
const Json& continueCheckpoint(const CombatContent& c,int capturedMap){
    const auto& current=c.catalog.at("continue");
    if(capturedMap==0||capturedMap==current.at("map").get<int>()){return current;}
    if(c.catalog.contains("continue_entries")){return c.catalog.at("continue_entries").at(std::to_string(capturedMap));}
    throw std::runtime_error("Unregistered Continue destination");
}
CombatContent loadTowerContent(const std::filesystem::path& path,const Json& data){
    need(data.size()==4&&data.at("schema")=="nobihaza.g4.v1"&&data.at("content_compatibility")=="post-f5-g4-normal-1","Unknown tower identity");
    const auto& source=data.at("source");need(source.size()==2&&source.at("file")=="g3-07-source.json"&&source.at("sha256")=="de948015f6f1bae9244b45aed3965d127b9086d41c66a26100365bb6fac5bee4","Unknown tower source");
    const auto& p=data.at("projection");need(contentSha256(p.dump())=="56d925df7f1efc3afa295e15e3c151eb9fef73b41a14622bb1b5851d0a847abe","Unreviewed tower projection");
    std::istringstream input(verified(path.parent_path(),source.at("file"),source.at("sha256")));
    auto old=std::make_shared<const CombatContent>(loadCombatContentDefinition(path.parent_path()/source.at("file").get<std::string>(),parseCombatContentDefinition(input)));
    auto c=*old;c.legacySource=old;c.compatibility=data.at("content_compatibility");
    for(const auto& [id,name]:p.at("facts").items()){need(c.facts.switches.emplace(std::stoi(id),name.get<std::string>()).second,"Duplicate tower fact");}
    for(const auto* group:{"images","audio","cues","animations"}){for(const auto& [id,row]:p.at(group).items()){c.setMediaDefinition(group,id,row);}}
    for(const auto& [id,row]:p.at("maps").items()){need(!c.maps.contains(std::stoi(id)),"Tower shadows delivered map");}
    for(const auto& [id,row]:p.at("objects").items()){
        if(c.objects.contains(id)){const auto& oldObject=c.objects.at(id);need(row.at("map")==oldObject.map&&row.at("initial_tile")==Json::array({oldObject.tile.x,oldObject.tile.y}),"Tower moves delivered actor");c.objects.erase(id);}
    }
    auto sprites=*c.maps.begin()->second.sprites;
    const auto added=readSceneDefinitions(c,Json{{"font",p.at("font")},{"sprites",p.at("scene_sprites")},{"maps",p.at("maps")},{"objects",p.at("objects")}});
    for(const auto& [id,row]:added){sprites.insert_or_assign(id,row);}const auto sharedSprites=std::make_shared<const SpriteDefinitions>(sprites);for(auto& [id,map]:c.maps){map.sprites=sharedSprites;}
    for(const auto& [id,row]:p.at("messages").items()){c.messages.insert_or_assign(id,readCombatMessage(row,true));}
    for(const auto& [id,row]:p.at("cues").items()){need(!row.contains("runtime_support"),"Unsupported tower cue");if(row.at("kind")=="forced_route"){c.routes.insert_or_assign(id,readRouteDefinition(row));}}
    for(const auto& [id,row]:p.at("flows").items()){
        if(c.catalog["flows"].contains(id)){need(c.catalog["flows"].at(id).value("handoff_only",false)||!c.catalog["flows"].at(id).value("runtime_enabled",true),"Tower changes a delivered flow");}c.catalog["flows"][id]=row;
    }
    const auto& lower=p.at("lower_tiles");LowerTileDefinition definition;definition.allowed=lower.at("allowed").get<std::vector<std::array<int,2>>>();definition.passageFlags=lower.at("passage_flags").get<std::vector<int>>();definition.terrainIds=lower.at("terrain_ids").get<std::vector<int>>();
    need(definition.passageFlags.size()==162&&definition.terrainIds.size()==162&&std::all_of(definition.passageFlags.begin(),definition.passageFlags.end(),[](int value){return value>=0&&value<=63;}),"Invalid tower lower passage table");
    for(const auto& [id,key]:lower.at("images").items()){need(c.media.at("images").at(key.get<std::string>()).at("size_px")==Json::array({16,16}),"Invalid tower lower pixels");definition.images.emplace(std::stoi(id),c.directory/c.media.at("images").at(key.get<std::string>()).at("file").get<std::string>());}
    for(const auto& cell:lower.at("tiles")){definition.cells.push_back({cell.at("tile").get<std::array<int,2>>(),cell.at("index"),cell.at("upper_passage")});}
    need(!c.maps.at(lower.at("map")).lowerTiles,"Tower shadows delivered lower tile contract");c.maps.at(lower.at("map")).lowerTiles=std::move(definition);
    c.catalog["g4"]={{"facts",p.at("facts")},{"delivered",1}};c.catalog["pending_maps"]=p.at("pending_maps");
    const auto prior=c.catalog.at("continue");c.catalog["continue_entries"][std::to_string(prior.at("map").get<int>())]=prior;
    auto next=prior;for(const auto& [key,value]:p.at("continue").items()){next[key]=value;}c.catalog["continue"]=std::move(next);
    for(const auto& [id,row]:p.at("map_music").items()){need(!c.expansion.mapMusic.contains(id),"Tower shadows delivered music");c.expansion.mapMusic[id]=row;}
    for(auto& [id,entry]:c.entries){initializeFullSession(c,entry.session);}
    for(const auto& [name,sha]:p.at("resources").items()){verified(c.directory,name,sha);}
    validateSceneContent(c,sprites);return c;
}
