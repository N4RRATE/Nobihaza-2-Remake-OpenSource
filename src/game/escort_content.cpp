#include "escort_controller.h"
#include <algorithm>
#include <stdexcept>
namespace{using Json=nlohmann::json;
void need(bool value,const char* message){if(!value){throw std::runtime_error(message);}}
}
void installEscortContent(CombatContent& content,const Json& evacuation){
    const auto& profile=evacuation.at("runtime_profile");const auto& escort=profile.at("escort");const auto& details=escort.at("details");
    need(escort.at("schema")=="nobihaza.escort-presentation.v1"&&escort.at("party_actor").is_null(),"Unsupported escort identity");
    need(details.at("pages").size()==3&&!details.at("fall_steps").empty()&&escort.at("tile_substitutions").size()==6,"Incomplete escort choreography");
    const int map=content.objects.at(escort.at("object").get<std::string>()).map;
    need(content.objects.at(escort.at("skin_object").get<std::string>()).map==map&&content.objects.at(escort.at("fall_object").get<std::string>()).map==map,"Unbound escort actors");
    for(const auto& [number,page]:details.at("pages").items()){
        const auto& variants=content.objects.at(escort.at("skin_object").get<std::string>()).variants;
        need(std::any_of(variants.begin(),variants.end(),[&](const auto& v){return v.page==std::stoi(number);}),"Missing escort costume page");
        for(const auto& cue:page.at("role").at("media")){need(content.cues.contains(cue.at("cue").get<std::string>()),"Missing escort costume cue");}
    }
    auto& scene=content.maps.at(map);
    for(const auto& row:escort.at("tile_substitutions")){
        UpperTileReplacementDefinition replacement;replacement.fromIndex=row.at("from_index");replacement.toIndex=row.at("to_index");
        replacement.image=content.directory/content.media.at("images").at(row.at("image").get<std::string>()).at("file").get<std::string>();replacement.underlay=content.directory/content.media.at("images").at(row.at("underlay").at("image").get<std::string>()).at("file").get<std::string>();
        for(const auto& cell:row.at("cells")){need(!cell.at("foreground").get<bool>()&&!cell.at("source_foreground").get<bool>()&&!cell.at("lower_foreground").get<bool>(),"Unsupported upper replacement layer");const auto tile=cell.at("tile").get<std::array<int,2>>();need(tile[0]>=0&&tile[1]>=0&&tile[0]<scene.width&&tile[1]<scene.height,"Out-of-map replacement tile");replacement.tiles.push_back(tile);replacement.passageMasks.push_back(cell.at("mask"));}
        scene.upperTileReplacements.push_back(std::move(replacement));
    }
    content.catalog["escort"]=escort;
    auto& tail=content.catalog["flows"]["guard-second-story-tail"];need(tail.at("implementation_ticket")==105,"Missing score-to-story continuation");tail["runtime_enabled"]=true;
    for(const auto& [id,row]:evacuation.at("flows").items()){
        if(row.at("implementation_ticket")==105){auto flow=row;flow["runtime_enabled"]=true;content.catalog["flows"][id]=std::move(flow);}
    }
    content.catalog["flows"]["escort-fall"]={{"object",escort.at("fall_object")},{"page",1},{"trigger","invoked"},{"implementation_ticket",105},{"runtime_enabled",true},{"steps",details.at("fall_steps")}};
    auto boundary=profile.at("completion_boundary");need(boundary.at("intercept_before_command")==1&&boundary.at("original_transfer_command")==14&&boundary.at("destination_map")==120&&boundary.at("steps").empty(),"Invalid final boundary intercept");boundary["runtime_enabled"]=true;content.catalog["completion_boundary"]=std::move(boundary);
    for(const auto& tile:escort.at("tile_substitutions")){need(content.media.at("images").at(tile.at("image").get<std::string>()).at("size_px")==Json::array({16,16}),"Missing escort substitution image");}
    for(auto it=content.pendingMapTickets.begin();it!=content.pendingMapTickets.end();){if(it->second==105){it=content.pendingMapTickets.erase(it);}else{++it;}}
    for(auto& [id,transition]:content.transitions.items()){
        const int source=content.objects.at(transition.at("object").get<std::string>()).map,destination=transition.at("destination").at("map");
        if(destination!=120&&transition.value("implementation_ticket",0)==105&&!content.pendingMapTickets.contains(source)&&!content.pendingMapTickets.contains(destination)){transition["runtime_enabled"]=true;transition["handoff_only"]=false;}
    }
}
