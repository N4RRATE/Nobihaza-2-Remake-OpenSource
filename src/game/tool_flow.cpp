#include "combat_scene.h"
#include "scene_presentation.h"
#include <algorithm>
namespace{
bool inToolArea(const nlohmann::json& tool,TilePosition p){
    if(tool.contains("inside_rect")){const auto& r=tool.at("inside_rect");return p.x>=r[0]&&p.x<=r[2]&&p.y>=r[1]&&p.y<=r[3];}
    const auto& xy=tool.at("use_tile");return p==TilePosition{xy[0],xy[1]};
}
bool onToolExit(const nlohmann::json& tool,TilePosition p){
    if(tool.contains("inside_rect")){const auto& v=tool.at("exit_vertical");const auto& h=tool.at("exit_horizontal");return ((p.x==v[0]||p.x==v[1])&&p.y>=v[2]&&p.y<=v[3])||((p.y==h[0]||p.y==h[1])&&p.x>=h[2]&&p.x<=h[3]);}
    return std::any_of(tool.at("exit_tiles").begin(),tool.at("exit_tiles").end(),[&](const auto& xy){return p==TilePosition{xy[0],xy[1]};});
}
}


bool CombatScene::atToolPosition() const{
    const auto& tool=content_.expansion.tool;if(tool.is_null()||!active_||session_.hp()<=0||foreground_!=ForegroundKind::None||!session_.flag(1)||!session_.flag(18)){return false;}
    const auto& target=content_.objects.at(tool.at("vent_object").get<std::string>());
    if(target.map!=map_||session_.flag(tool.at("removed_flag"))){return false;}
    const auto& p=world_->player();if(p.motion.moving()||p.through){return false;}
    const auto ladderId=tool.at("ladder_object").get<std::string>();
    const auto& ladders=content_.catalog.at("ladders");const auto ladder=std::find_if(ladders.begin(),ladders.end(),[&](const auto& v){return v.at("object")==ladderId;});
    if(ladder==ladders.end()||!session_.flag(ladder->at("mounted_flag"))){return false;}
    const auto area=findCharacter(*world_,tool.at("active_area"));const auto vent=findCharacter(*world_,target.id);const auto support=findCharacter(*world_,ladderId);
    // The audited tool flow turns up before using the wrench. Standing on the
    // source ladder's northern landing, not merely carrying I63, grants access.
    return area.active&&vent.active&&support.active&&support.spriteId&&p.motion.tile()==area.motion.tile()&&
        adjacentTile(p.motion.tile(),Direction::Up)==vent.motion.tile()&&adjacentTile(p.motion.tile(),Direction::Down)==support.motion.tile();
}
const nlohmann::json* CombatScene::contextToolAtPosition(int item) const{
    if(!active_||session_.hp()<=0||foreground_!=ForegroundKind::None||!session_.flag(1)||!session_.flag(18)||!content_.catalog.contains("context_tools")){return nullptr;}
    const auto& player=world_->player();if(player.motion.moving()||player.through){return nullptr;}
    for(const auto& tool:content_.catalog.at("context_tools")){
        if((item&&tool.at("usable_item")!=item)||map_!=tool.at("map")||session_.flag(tool.at("completed_switch"))||!inToolArea(tool,player.motion.tile())){continue;}
        if(tool.value("origin_requires_inactive_item",false)&&session_.count(tool.at("inactive_item"))==0&&session_.count(tool.at("usable_item"))==0){continue;}
        const auto origin=tool.value("origin_object",tool.at("object").get<std::string>());
        bool page=isCurrentPage(origin,tool.at("request_origin_page"));
        if(tool.contains("request_origin_pages")){page=std::any_of(tool.at("request_origin_pages").begin(),tool.at("request_origin_pages").end(),[&](const auto& p){return isCurrentPage(origin,p);});}
        else if(tool.contains("inactive_origin_page")){page|=isCurrentPage(origin,tool.at("inactive_origin_page"));}
        if(!page&&tool.value("origin_requires_inactive_item",false)&&session_.count(tool.at("usable_item"))>0){
            GameSession candidate(content_.items,session_.exportState(),content_.facts);
            if(candidate.convertTool(tool.at("usable_item"),tool.at("inactive_item"))){const auto* source=content_.activeVariant(content_.objects.at(origin),candidate);page=source&&source->page==tool.at("inactive_origin_page");}
        }
        if(page){return &tool;}
    }
    return nullptr;
}
std::optional<ToolUseRequest> CombatScene::requestToolUse(){
    const auto requests=requestToolUses();return requests.empty()?std::nullopt:std::optional<ToolUseRequest>{requests.front()};
}
std::vector<ToolUseRequest> CombatScene::requestToolUses(){
    toolRequests_.clear();
    if(content_.catalog.contains("context_tools")){for(const auto& candidate:content_.catalog.at("context_tools")){
        const int item=candidate.at("usable_item");const auto* tool=contextToolAtPosition(item);
        if(tool!=&candidate||!session_.canSelectItem(item)||!content_.catalog.at("flows").at(tool->at("flow").get<std::string>()).value("runtime_enabled",true)){continue;}
        toolRequests_.push_back({generation_,++nextToolRequest_,item,tool->at("request_origin_page"),tool->at("object"),world_->player().motion.tile(),{},world_->player().motion.facing()});
    }}
    if(toolRequests_.empty()&&content_.catalog.contains("deferred_key")&&active_&&session_.hp()>0&&foreground_==ForegroundKind::None&&session_.flag(1)&&session_.flag(18)&&!world_->player().motion.moving()&&!world_->player().through){
        const auto& source=content_.catalog.at("deferred_key");const int item=source.at("item");
        if(session_.canSelectItem(item)){toolRequests_.push_back({generation_,++nextToolRequest_,item,0,"source-key",world_->player().motion.tile(),{},world_->player().motion.facing()});}
    }
    if(!toolRequests_.empty()||!atToolPosition()){return toolRequests_;}
    const auto& tool=content_.expansion.tool;const int item=tool.at("active_item");
    if(!session_.flag(tool.at("area_flag"))||!session_.canSelectItem(item)){return {};}
    const auto target=tool.at("vent_object").get<std::string>();const auto* page=content_.activeVariant(content_.objects.at(target),session_);
    if(!page||page->definition.trigger!=0||page->definition.layer!=1){return {};}
    toolRequests_.push_back({generation_,++nextToolRequest_,item,page->page,target,world_->player().motion.tile(),findCharacter(*world_,tool.at("ladder_object")).motion.tile(),world_->player().motion.facing()});
    return toolRequests_;
}
bool CombatScene::useTool(const ToolUseRequest& request){
    if(std::find(toolRequests_.begin(),toolRequests_.end(),request)==toolRequests_.end()){return false;}toolRequests_.clear();
    if(request.scene!=generation_){return false;}
    if(request.target=="source-key"){
        if(!content_.catalog.contains("deferred_key")||!active_||session_.hp()<=0||foreground_!=ForegroundKind::None||!session_.flag(1)||!session_.flag(18)||world_->player().motion.moving()||world_->player().through||request.player!=world_->player().motion.tile()||request.facing!=world_->player().motion.facing()){return false;}
        const auto& source=content_.catalog.at("deferred_key");if(request.item!=source.at("item")||!session_.canSelectItem(request.item)){return false;}
        session_.setFlag(source.at("request_switch"),true);refreshObjectPages();return true;
    }
    if(const auto* tool=contextToolAtPosition(request.item);tool&&request.item==tool->at("usable_item")){
        if(request.target!=tool->at("object").get<std::string>()||request.page!=tool->at("request_origin_page")||request.player!=world_->player().motion.tile()||request.facing!=world_->player().motion.facing()||!session_.canSelectItem(request.item)){return false;}
        std::string flow=tool->at("flow");int effective=tool->at("request_effective_page");
        if(tool->contains("flows_by_page")){
            // Select the actual highest source page before committing its request flag.
            GameSession candidate(content_.items,session_.exportState(),content_.facts);candidate.setFlag(tool->at("request_switch"),true);
            const auto* page=content_.activeVariant(content_.objects.at(request.target),candidate);
            if(!page||!tool->at("flows_by_page").contains(std::to_string(page->page))){return false;}
            effective=page->page;flow=tool->at("flows_by_page").at(std::to_string(effective));
        }
        if(!content_.catalog.at("flows").at(flow).value("runtime_enabled",true)){return false;}
        ScenePresentation(*this).validateSequence(content_.catalog.at("flows").at(flow).at("steps"),request.target);
        session_.setFlag(tool->at("request_switch"),true);refreshObjectPages();
        if(!isCurrentPage(request.target,effective)){session_.setFlag(tool->at("request_switch"),false);refreshObjectPages();return false;}
        startContentFlow(flow);return true;
    }
    if(!atToolPosition()||!isCurrentPage(request.target,request.page)||world_->player().motion.facing()!=request.facing){return false;}
    const auto& tool=content_.expansion.tool;
    if(request.player!=world_->player().motion.tile()||request.ladder!=findCharacter(*world_,tool.at("ladder_object")).motion.tile()||!session_.flag(tool.at("area_flag"))||!session_.canSelectItem(request.item)){return false;}
    session_.setFlag(tool.at("request_flag"),true);refreshObjectPages();startContentFlow("f4-tool-remove");return true;
}
void CombatScene::updateToolAreas(){
    if(foreground_==ForegroundKind::None&&!world_->player().motion.moving()&&content_.catalog.contains("context_tools")){
        const auto* active=contextToolAtPosition();
        for(const auto& tool:content_.catalog.at("context_tools")){
            if(tool.value("conversion_in_flow",false)){continue;}
            const int from=tool.at("inactive_item"),to=tool.at("usable_item");
            if(tool.value("cleanup_duplicate_inactive",false)&&map_==tool.at("map")&&session_.count(to)>0&&session_.count(from)>0){session_.discard(from,1);refreshObjectPages();}
            if(active==&tool){if(session_.convertTool(from,to)){refreshObjectPages();}}
            else if(map_==tool.at("map")&&!session_.flag(tool.at("completed_switch"))){
                const auto tile=world_->player().motion.tile();
                if(onToolExit(tool,tile)&&session_.convertTool(to,from)){if(tool.value("clear_request_on_exit",true)){session_.setFlag(tool.at("request_switch"),false);}refreshObjectPages();}
            }
        }
    }
    const auto& tool=content_.expansion.tool;if(tool.is_null()||foreground_!=ForegroundKind::None||world_->player().motion.moving()){return;}
    const auto active=tool.at("active_area").get<std::string>();if(content_.objects.at(active).map!=map_){return;}
    const int from=tool.at("inactive_item"),to=tool.at("active_item"),flag=tool.at("area_flag");
    const auto* entryPage=content_.activeVariant(content_.objects.at(active),session_);
    if(entryPage&&entryPage->definition.trigger==2&&entryPage->inventoryItem==from&&atToolPosition()&&!session_.flag(flag)&&session_.convertTool(from,to)){session_.setFlag(flag,true);refreshObjectPages();return;}
    for(const auto& id:tool.at("return_areas")){
        const auto object=id.get<std::string>();const auto actor=findCharacter(*world_,object);
        const auto* exitPage=content_.activeVariant(content_.objects.at(object),session_);
        if(exitPage&&exitPage->definition.trigger==2&&exitPage->inventoryItem==to&&actor.active&&actor.motion.tile()==world_->player().motion.tile()&&session_.flag(flag)&&session_.convertTool(to,from)){
            session_.setFlag(flag,false);refreshObjectPages();return;
        }
    }
}

void CombatScene::prepareAccessKeyMenu(){
    if(!active_||!content_.catalog.contains("access_keys")){return;}
    cancelToolUse();session_.normalizeAccessKeys();refreshObjectPages();updateToolAreas();
}

void CombatScene::updateHeatKeyAreas(){
    if(!content_.catalog.contains("heat_room")||foreground_!=ForegroundKind::None||world_->player().motion.moving()){return;}
    const auto& room=content_.catalog.at("heat_room");if(map_!=room.at("map")){return;}
    // E12's submitted request owns its autorun before E13/E14 can reverse the carried form.
    if(session_.flag(content_.catalog.at("deferred_key").at("request_switch"))){return;}
    const auto tile=world_->player().motion.tile();const auto& rect=room.at("conversion_rect");
    if(isCurrentPage(room.at("conversion_object"),1)&&tile.x>=rect[0]&&tile.x<=rect[2]&&tile.y>=rect[1]&&tile.y<=rect[3]){if(session_.convertTool(105,106)){refreshObjectPages();}}
    if(isCurrentPage(room.at("return_object"),1)&&tile.y>room.at("return_y").get<int>()){if(session_.convertTool(106,105)){refreshObjectPages();}}
}
