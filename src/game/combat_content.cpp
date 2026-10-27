#include "combat_content.h"
#include "branch_content.h"
#include "full_content.h"
#include "content_hash.h"
#include "expanded_content_validation.h"
#include <fstream>
#include <algorithm>
#include <functional>
#include <limits>
#include <set>
#include <stdexcept>
using Json=nlohmann::json;
namespace{
    int integer(const Json& j,const char* name,int low,int high){
        const auto& v=j.at(name);
        if(!v.is_number_integer()||v<low||v>high){throw std::runtime_error(std::string(name)+": integer out of range");}
        return v.get<int>();
    }
    std::filesystem::path file(const std::filesystem::path& dir,const Json& value){
        const std::filesystem::path p=value.get<std::string>();
        if(p.empty()||p.is_absolute()){throw std::runtime_error("Expected relative resource file");}
        for(const auto& part:p){if(part==".."){throw std::runtime_error("Resource escapes package");}}
        if(!std::filesystem::is_regular_file(dir/p)){throw std::runtime_error("Missing resource: "+(dir/p).string());}
        return dir/p;
    }
    template<class T> std::map<int,T> idMap(const Json& j){
        std::map<int,T> result;
        for(const auto& [k,v]:j.items()){result.emplace(std::stoi(k),v.get<T>());}
        return result;
    }
    TilePosition tile(const Json& j){if(j.size()!=2){throw std::runtime_error("Expected tile pair");}return {j.at(0).get<int>(),j.at(1).get<int>()};}
    RouteDefinition route(const Json& j){
        RouteDefinition r; r.target=j.value("target",0);r.frequency=j.value("frequency",6);
        r.repeat=j.at("repeat");r.skipBlocked=j.at("skip_blocked");
        r.outsideMapMargin=j.contains("outside_map_margin")?integer(j,"outside_map_margin",0,10):0;
        static const std::set<std::string> supported{"wait","speed_down","speed_up","graphic","frequency_up","frequency_down","up","right","down","left","unlock_direction","lock_direction","face_player","face_random","start_animation","stop_animation","switch_on","switch_off","face_up","face_right","face_down","face_left","turn_left","turn_right","turn_around","face_away","sound","away_player","toward_player","random","through_on","through_off","forward","up_left","up_right","down_left","down_right","begin_jump","end_jump","opacity_down","opacity_up"};
        for(const auto& s:j.at("steps")){
            RouteStep step;step.kind=s.at("kind");
            if(!supported.contains(step.kind)){throw std::runtime_error("Unsupported route step: "+step.kind);}
            step.sprite=s.value("sprite","");step.switchId=s.value("switch_id",0);if(s.contains("sound")){step.sound=readSound(s.at("sound"));}
            r.steps.push_back(step);
        }
        return r;
    }
    MessageDisplay display(const Json& j){return {j.at("transparent"),integer(j,"position",0,2),j.at("fixed"),j.at("continue_events")};}

}
CombatMessage readCombatMessage(const Json& j,bool allowVariables){
    CombatMessage m; m.inheritDisplay=j.at("options")=="inherit"; m.inheritFace=j.at("face")=="inherit";
    m.message.exfontImage=j.value("exfont_image",std::string{});bool hasExFont=false;
    if(!m.inheritDisplay){m.message.display=display(j.at("options"));}
    if(!m.inheritFace&&!j.at("face").is_null()){
        const auto& f=j.at("face");m.message.faceImage=f.at("image");m.message.faceIndex=f.at("index");m.message.faceRight=f.at("right");m.message.faceFlipped=f.at("flipped");
    }
    const std::map<std::string,MessageTokenKind> kinds{{"text",MessageTokenKind::Text},{"instant_on",MessageTokenKind::InstantLine},{"instant_off",MessageTokenKind::InstantOff},{"color",MessageTokenKind::Color},{"short_wait",MessageTokenKind::ShortWait},{"long_wait",MessageTokenKind::LongWait},{"speed",MessageTokenKind::Speed},{"close_without_input",MessageTokenKind::AutoClose},{"wait_input",MessageTokenKind::WaitInput},{"literal_slash",MessageTokenKind::LiteralSlash}};
    for(const auto& line:j.at("lines")){
        MessageLine l;l.raw=line.at("raw");
        for(const auto& t:line.at("tokens")){
            const auto kind=t.at("type").get<std::string>();
            if(kind=="variable"&&allowVariables){
                l.tokens.push_back({MessageTokenKind::Variable,{},integer(t,"value",388,389)});continue;
            }
            if(kind=="exfont"){
                if(t.size()!=2||m.message.exfontImage.empty()){throw std::runtime_error("Invalid ExFont token or missing atlas binding");}
                l.tokens.push_back({MessageTokenKind::ExFont,{},integer(t,"value",0,51)});hasExFont=true;continue;
            }
            if(!kinds.contains(kind)){throw std::runtime_error("Unsupported message token: "+kind);}
            l.tokens.push_back({kinds.at(kind),kind=="text"?decodeMessageText(t.at("text")):U"",t.value("value",0)});
        }
        m.message.lines.push_back(l);
    }
    if(m.message.lines.empty()||m.message.lines.size()>4){throw std::runtime_error("Message page needs 1..4 lines");}
    if(hasExFont!=!m.message.exfontImage.empty()){throw std::runtime_error("ExFont atlas binding needs an explicit glyph token");}
    return m;
}
MessageDefinition resolveCombatMessage(const CombatMessage& source,const GameSession& session){
    auto message=source.message;
    for(auto& line:message.lines){for(auto& token:line.tokens){if(token.kind==MessageTokenKind::Variable){
        if(token.value!=388&&token.value!=389){throw std::runtime_error("Unsupported message variable");}
        token={MessageTokenKind::Text,decodeMessageText(std::to_string(session.variable(token.value))),0};
    }}}
    return message;
}
std::map<std::string,SpriteDefinition> readSceneDefinitions(CombatContent& c,const Json& d){
    std::map<std::string,SpriteDefinition> sprites;
    for(const auto& [id,v]:d.at("sprites").items()){
        SpriteDefinition s;s.texturePath=file(c.directory,v.at("file"));s.textureWidth=v.at("size_px").at(0);s.textureHeight=v.at("size_px").at(1);s.originX=v.at("origin_px").at(0);s.originY=v.at("origin_px").at(1);
        std::vector<SpriteFrameDefinition> frames;
        std::set<std::pair<std::string,int>> seen;
        for(const auto& f:v.at("frames")){
            const auto& r=f.at("rect_px");SpriteFrameDefinition frame{f.at("direction"),f.at("column"),r.at(0),r.at(1),r.at(2),r.at(3)};
            directionFromName(frame.direction);
            if(frame.column<0||frame.column>2||frame.left<0||frame.top<0||frame.width<=0||frame.height<=0||frame.left+frame.width>s.textureWidth||frame.top+frame.height>s.textureHeight||!seen.emplace(frame.direction,frame.column).second){throw std::runtime_error("Invalid frame: "+id);}
            frames.push_back(frame);
        }
        if(seen.size()!=12){throw std::runtime_error("Missing directional frames: "+id);}
        s.frames=std::make_shared<const std::vector<SpriteFrameDefinition>>(std::move(frames));
        sprites.emplace(id,std::move(s));
    }
    const auto sharedSprites=std::make_shared<const SpriteDefinitions>(sprites);
    for(const auto& [id,v]:d.at("maps").items()){
        SceneDefinition s;s.loopX=v.contains("scroll")&&v.at("scroll").value("loop_x",false);s.width=integer(v.at("grid"),"width",1,1024);s.height=integer(v.at("grid"),"height",1,1024);s.tileSize=integer(v.at("grid"),"tile_px",16,16);
        if(v.contains("scroll")&&v.at("scroll").contains("default_panorama")){
            const auto& value=v.at("scroll").at("default_panorama");
            if(value.at("speed_unit")!="original-rpgmaker-speed-setting"){throw std::runtime_error("Unsupported panorama speed unit");}
            if(value.at("enabled").get<bool>()){
                PanoramaDefinition panorama;
                panorama.image=value.at("image").get<std::string>();
                panorama.loopX=value.at("loop_x").get<bool>();panorama.loopY=value.at("loop_y").get<bool>();
                panorama.autoX=value.at("auto_x").get<bool>();panorama.autoY=value.at("auto_y").get<bool>();
                panorama.speedX=integer(value,"speed_x",-8,8);panorama.speedY=integer(value,"speed_y",-8,8);
                if(!c.media.at("images").contains(panorama.image)||(panorama.autoX&&!panorama.loopX)||(panorama.autoY&&!panorama.loopY)){
                    throw std::runtime_error("Invalid default panorama definition");
                }
                s.defaultPanorama=std::move(panorama);
            }
        }
        s.passageMasks=v.at("tiles").at("passage_masks").get<std::vector<int>>();
        TerrainMap validate(s.width,s.height,s.passageMasks);
        s.backgroundPath=file(c.directory,v.at("background"));s.foregroundPath=file(c.directory,v.at("foreground"));s.fontPath=file(c.directory,d.at("font").at("file"));s.sprites=sharedSprites;s.tone={100,100,100};
        s.player={0,0,"down","player-0",1,4,256,32};s.walkingAnimation=animationForSpeed(4);
        c.terrainIds.emplace(std::stoi(id),v.at("tiles").at("terrain_ids").get<std::vector<int>>());
        if(c.terrainIds.at(std::stoi(id)).size()!=s.passageMasks.size()){throw std::runtime_error("Terrain length mismatch");}
        if(v.contains("animated_tiles")){
            std::set<int> ids;std::set<std::array<int,2>> cells;
            for(const auto& a:v.at("animated_tiles")){
                AnimatedTileDefinition tile;tile.id=integer(a,"lower_id",0,3149);if(!((tile.id<=46)||(tile.id>=1000&&tile.id<=1046)||(tile.id>=3000))){throw std::runtime_error("Unsupported animated lower tile");}tile.file=file(c.directory,a.at("file"));
                tile.width=a.at("size_px").at(0);tile.height=a.at("size_px").at(1);tile.frameTicks=integer(a,"frame_duration_ticks",1,1000);
                const auto layer=a.at("layer").get<std::string>();tile.foreground=layer=="foreground";
                tile.frames=a.at("frame_rects_px").get<std::vector<std::array<int,4>>>();tile.order=a.at("frame_order").get<std::vector<int>>();tile.tiles=a.at("tiles").get<std::vector<std::array<int,2>>>();
                if(a.at("source_fps")!=60||a.at("clock")!="game-frame-counter"||(layer!="background"&&layer!="foreground")||!ids.insert(tile.id).second||tile.frames.empty()||tile.frames.size()>4||tile.order.empty()||tile.order.size()>4||tile.tiles.empty()){throw std::runtime_error("Unsupported animated tile definition");}
                for(const auto& r:tile.frames){if(r[0]<0||r[1]<0||r[2]!=16||r[3]!=16||r[0]+16>tile.width||r[1]+16>tile.height){throw std::runtime_error("Animated tile frame outside atlas");}}
                for(int frame:tile.order){if(frame<0||frame>=static_cast<int>(tile.frames.size())){throw std::runtime_error("Unknown animated tile frame");}}
                for(const auto& xy:tile.tiles){if(xy[0]<0||xy[1]<0||xy[0]>=s.width||xy[1]>=s.height||!cells.insert(xy).second){throw std::runtime_error("Invalid animated tile position");}}
                s.animatedTiles.push_back(std::move(tile));
            }
        }
        c.maps.emplace(std::stoi(id),std::move(s));
    }
    for(const auto& [id,v]:d.at("objects").items()){
        CombatObject o;o.id=id;o.map=v.at("map");o.tile=tile(v.at("initial_tile"));
        for(const auto& p:v.at("variants")){
            ObjectVariant variant;variant.page=p.at("original_page");auto& def=variant.definition;
            def.outsideMapMargin=p.contains("outside_map_margin")?integer(p,"outside_map_margin",0,20):0;
            def.id=id;def.sourceEventId=v.at("original_event");def.tileX=o.tile.x;def.tileY=o.tile.y;def.active=true;def.layer=p.at("layer");def.direction=p.at("direction");def.initialColumn=p.at("column");def.moveSpeed=p.at("move_speed");def.animationType=p.at("animation_type");def.trigger=p.at("trigger");def.overlapForbidden=p.at("overlap_forbidden");def.transparency=p.value("translucent",false)?3:0;
            if(p.contains("tile_passage_flags")){def.tilePassageFlags=integer(p,"tile_passage_flags",0,63);}
            def.renderTiePriority=p.contains("render_tie_priority")?integer(p,"render_tie_priority",-16,16):0;
            if(!p.at("sprite").is_null()){def.spriteId=p.at("sprite").get<std::string>();if(!sprites.contains(*def.spriteId)){throw std::runtime_error("Unknown sprite: "+*def.spriteId);}}
            const auto& conditions=p.at("conditions");
            const std::set<std::string> supportedConditions{"switch_a","switch_b","variable_at_least","inventory_item","actor","timer_at_most_seconds"};
            for(const auto& [key,value]:conditions.items()){if(!supportedConditions.contains(key)){throw std::runtime_error("Unsupported object page condition: "+key);}}
            if(conditions.contains("timer_at_most_seconds")){variant.timerMaximumSeconds=integer(conditions,"timer_at_most_seconds",0,120);}
            for(const auto* key:{"switch_a","switch_b"}){if(conditions.contains(key)){variant.flags[conditions.at(key).get<int>()]=true;}}
            if(conditions.contains("variable_at_least")){const auto& a=conditions.at("variable_at_least");variant.variableMinimum={{a.at(0),a.at(1)}};}
            if(conditions.contains("inventory_item")){variant.inventoryItem=conditions.at("inventory_item");}
            if(conditions.contains("actor")){variant.actor=conditions.at("actor");if(*variant.actor!=1){throw std::runtime_error("Unsupported actor page condition");}}
            if(p.contains("autonomous_route")){variant.autonomousRoute=route(p.at("autonomous_route"));variant.autonomousRoute.frequency=p.at("move_frequency");}
            o.variants.push_back(std::move(variant));
        }
        c.objects.emplace(id,std::move(o));
    }
    return sprites;
}
bool supportsEnemyBloodSlot(const CombatContent& content,int slot){
    // CE15 onward defines each source blood workspace; rescue supplies CE21/22.
    return slot>0&&content.media.at("effects").contains("enemy-slot-"+std::to_string(14LL+slot)+"-blood-1");
}
void validateSceneContent(const CombatContent& c,const std::map<std::string,SpriteDefinition>& sprites){
    if(c.catalog.contains("ladders")){for(const auto& l:c.catalog.at("ladders")){
        if(l.contains("push_completed_flag")&&!c.facts.switches.contains(integer(l,"push_completed_flag",1,10000))){throw std::runtime_error("Unknown ladder completion fact");}
        if(l.contains("push_terrain_gate")){l.at("push_terrain_gate").get<bool>();}
        if(l.contains("push_sound")&&(c.cue(l.at("push_sound").get<std::string>()).at("kind")!="sound"||c.cue(l.at("push_sound_wait").get<std::string>()).at("kind")!="wait")){throw std::runtime_error("Invalid ladder move sound");}
        if(l.contains("priority_flow")){const auto& f=c.catalog.at("flows").at(l.at("priority_flow").get<std::string>());if(c.objects.at(f.at("object").get<std::string>()).map!=c.objects.at(l.at("object").get<std::string>()).map||f.at("trigger")!="action"){throw std::runtime_error("Invalid ladder priority flow");}}
    }}
    if(c.catalog.contains("flows")){for(const auto& [id,flow]:c.catalog.at("flows").items()){
        for(const auto& step:flow.at("steps")){if(step.at("kind")=="restore_dead_enemies"){for(const auto& placement:step.at("placements")){if(placement.contains("hidden")){
            const auto id=placement.at("object").get<std::string>();const auto& owner=c.objects.at(flow.at("object").get<std::string>());const auto& map=c.maps.at(owner.map);const auto& xy=placement.at("tile");
            if(placement.at("hidden")!=true||c.objects.at(id).map!=owner.map||c.enemies.at(id).deadFlag!=placement.at("if_flag")||xy.size()!=2||!xy.at(0).is_number_integer()||!xy.at(1).is_number_integer()||(xy.at(0)>=0&&xy.at(0)<map.width&&xy.at(1)>=0&&xy.at(1)<map.height)){throw std::runtime_error("Invalid hidden corpse placement");}
        }}}}
        if(flow.contains("instant_contact_sequence")){
            const auto& steps=flow.at("steps");const auto& owner=c.objects.at(flow.at("object").get<std::string>());
            if(flow.at("instant_contact_sequence")!=true||flow.at("trigger")!="contact"||steps.size()!=3||steps[0].at("kind")!="cue"||c.cue(steps[0].at("cue").get<std::string>()).at("kind")!="sound"||steps[1].at("kind")!="place_object"||steps[2].at("kind")!="flag"||steps[2].at("value")!=true||!c.facts.switches.contains(steps[2].at("id").get<int>())||flow.at("requires_fact_off")!=steps[2].at("id")){
                throw std::runtime_error("Invalid instant contact sequence: "+id);
            }
            const auto& target=c.objects.at(steps[1].at("object").get<std::string>());const auto& map=c.maps.at(owner.map);const auto& xy=steps[1].at("tile");
            if(target.map!=owner.map||xy.size()!=2||!xy[0].is_number_integer()||!xy[1].is_number_integer()||xy[0]<0||xy[0]>=map.width||xy[1]<0||xy[1]>=map.height){throw std::runtime_error("Invalid contact placement: "+id);}
        }
        if(!flow.contains("instant_contact_fact")){continue;}
        const auto& steps=flow.at("steps");
        if(flow.at("instant_contact_fact")!=true||flow.at("trigger")!="contact"||steps.size()!=1||steps.at(0).at("kind")!="flag"||!steps.at(0).at("id").is_number_integer()||!c.facts.switches.contains(steps.at(0).at("id").get<int>())||!steps.at(0).at("value").is_boolean()){
            throw std::runtime_error("Invalid instant contact fact: "+id);
        }
    }}
    for(const auto& [id,enemy]:c.enemies){
        if(!enemy.rules.contains("runtime_profile")){continue;}const auto& profile=enemy.rules.at("runtime_profile");
        const auto bloodSlot=[&](const nlohmann::json& value,const std::string& role){
            if(!value.is_number_integer()||value<1||value>std::numeric_limits<int>::max()||!supportsEnemyBloodSlot(c,value.get<int>())){throw std::runtime_error("Invalid enemy blood slot for "+id+" "+role+": "+value.dump());}
        };
        if(profile.contains("blood_slot")){bloodSlot(profile.at("blood_slot"),"blood_slot");for(const auto& [receiver,slot]:profile.at("blood_write_slots").items()){bloodSlot(slot,"blood_write_slots/"+receiver);}}
        if(enemy.rules.value("behavior",std::string{})=="spider"){bloodSlot(profile.at("source_slot"),"source_slot");}
        if(enemy.rules.contains("attack_sources")){
            const auto& sources=enemy.rules.at("attack_sources");std::set<std::string> pages;
            for(const auto* role:{"bite_page","grab_front_page","grab_back_page"}){
                const int page=profile.at(role);pages.insert(std::to_string(page));
                const auto& variants=c.objects.at(id).variants;
                if(std::none_of(variants.begin(),variants.end(),[&](const auto& v){return v.page==page;})){throw std::runtime_error("Missing attack source page: "+id);}
                const auto& input=sources.at(std::to_string(page));integer(input,"actor",1,999);integer(input,"attack",1,9999);
                if(!input.at("source").is_string()||input.at("source").get<std::string>().empty()){throw std::runtime_error("Missing attack source evidence: "+id);}
            }
            if(pages.size()!=3||sources.size()!=pages.size()){throw std::runtime_error("Unbound attack source: "+id);}
        }
        if(profile.contains("contact_grab_page")){
            const int page=profile.at("contact_grab_page");const auto& variants=c.objects.at(id).variants;
            const auto found=std::find_if(variants.begin(),variants.end(),[&](const auto& v){return v.page==page;});
            if(found==variants.end()||page!=profile.at("grab_approach_page")||found->definition.trigger!=2||found->autonomousRoute.repeat||found->autonomousRoute.steps.empty()){
                throw std::runtime_error("Invalid contact grab route: "+id);
            }
        }
    }
    static const std::set<std::string> cueKinds{"forced_route","wait","sound","move_picture","show_picture","message_options","cancel_routes","face","erase_picture","actor_graphic","erase_screen","show_screen","animation","flash_character","wait_routes","tint","transition_setting","bgm","camera","shake","fade_bgm","choices","player_visibility","flash_screen","panorama","weather","place_object","number_input"};
    for(const auto& [id,cueDefinition]:c.cues){const auto& cue=*cueDefinition;
        if(!cueKinds.contains(cue.at("kind").get<std::string>())){throw std::runtime_error("Unknown media cue: "+id+" "+cue.value("source",Json{}).dump());}
    }
    for(const auto* key:{"images","audio"}){for(const auto& [id,value]:c.media.at(key).items()){
        try{file(c.directory,value.at("file"));}catch(const std::exception& e){throw std::runtime_error(std::string(key)+"/"+id+" "+value.value("source_file",std::string{})+" "+value.value("source",Json{}).dump()+": "+e.what());}
    }}
    for(const auto& [id,o]:c.objects){
        if(!c.maps.contains(o.map)){throw std::runtime_error("Unknown object map: "+id);}
        const auto& map=c.maps.at(o.map);TerrainMap bounds(map.width,map.height,map.passageMasks);
        if(!bounds.contains(o.tile)){throw std::runtime_error("Object outside map: "+id);}
    }
    std::set<std::string> checkedExfonts;
    for(const auto& [id,m]:c.messages){
        if(!m.message.faceImage.empty()){
            const auto& image=c.media.at("images").at(m.message.faceImage);
            if(m.message.faceIndex<0||m.message.faceIndex>=image.at("size_px").at(0).get<int>()/48*(image.at("size_px").at(1).get<int>()/48)){throw std::runtime_error("Face frame out of range: "+id);}
        }
        bool bitmap=false;
        for(const auto& line:m.message.lines){for(const auto& token:line.tokens){if(token.kind==MessageTokenKind::ExFont){
            if(token.value<0||token.value>=52||!token.text.empty()){throw std::runtime_error("Invalid ExFont glyph: "+id);}
            bitmap=true;
        }}}
        if(bitmap!=!m.message.exfontImage.empty()){throw std::runtime_error("Invalid ExFont message binding: "+id);}
        if(bitmap&&checkedExfonts.insert(m.message.exfontImage).second){
            if(!c.media.at("images").contains(m.message.exfontImage)){throw std::runtime_error("Missing ExFont atlas: "+m.message.exfontImage+" "+id);}
            const auto& image=c.media.at("images").at(m.message.exfontImage);
            const auto sha=image.value("sha256",std::string{});
            if(image.at("size_px")!=Json::array({156,48})||sha.size()!=64||sha.find_first_not_of("0123456789abcdef")!=std::string::npos){throw std::runtime_error("Invalid ExFont atlas definition: "+id);}
            const auto path=file(c.directory,image.at("file"));
            if(std::filesystem::is_symlink(path)||std::filesystem::file_size(path)>128*1024){throw std::runtime_error("Unsafe ExFont atlas: "+path.string());}
            std::ifstream input(path,std::ios::binary);
            if(!input||contentSha256(readContentBytes(input))!=sha){throw std::runtime_error("Changed ExFont atlas: "+path.string());}
        }
    }
    const auto validateRoute=[&](const RouteDefinition& r){
        if(r.frequency<1||r.frequency>8){throw std::runtime_error("Route frequency out of range");}
        for(const auto& step:r.steps){
            if(!step.sprite.empty()&&!sprites.contains(step.sprite)){throw std::runtime_error("Missing route sprite: "+step.sprite);}
            if(!step.sound.resource.empty()&&!c.media.at("audio").contains(step.sound.resource)){throw std::runtime_error("Missing route audio");}
        }
    };
    for(const auto& [id,r]:c.routes){validateRoute(r);}
    for(const auto& [id,o]:c.objects){for(const auto& v:o.variants){validateRoute(v.autonomousRoute);}}
    std::function<void(const Json&)> validateReferences=[&](const Json& value){
        if(value.is_object()){
            for(const auto& [key,v]:value.items()){
                if(key=="source"){continue;}
                if(key=="image"&&v.is_string()&&!c.media.at("images").contains(v.get<std::string>())){throw std::runtime_error("Missing image reference: "+v.get<std::string>());}
                if(key=="resource"&&v.is_string()&&!c.media.at("audio").contains(v.get<std::string>())){throw std::runtime_error("Missing audio reference: "+v.get<std::string>());}
                if(key=="sprite"&&v.is_string()&&!sprites.contains(v.get<std::string>())){throw std::runtime_error("Missing sprite reference: "+v.get<std::string>());}
                validateReferences(v);
            }
        }else if(value.is_array()){for(const auto& v:value){validateReferences(v);}}
    };
    validateReferences(c.media);
    for(const auto& [id,definition]:c.cues){validateReferences(*definition);}
    for(const auto& [id,audio]:c.media.at("audio").items()){
        if(!audio.contains("loop_samples")){continue;}
        const auto& format=audio.at("format");const auto& loop=audio.at("loop_samples");
        const int frames=integer(format,"frames",1,std::numeric_limits<int>::max());
        const int end=integer(loop,"end",1,frames);integer(loop,"start",0,end-1);integer(audio,"initial_sample_compatibility",0,end-1);
        if(format.at("codec")!="pcm"||format.at("sample_bytes")!=2){throw std::runtime_error("Loop source is not reviewed PCM");}
        integer(format,"channels",1,8);integer(format,"sample_rate_hz",1,384000);
    }
    for(const auto& [id,image]:c.media.at("images").items()){
        if(image.at("size_px").size()!=2||image.at("size_px").at(0).get<int>()<=0||image.at("size_px").at(1).get<int>()<=0){throw std::runtime_error("Invalid image size: "+id);}
    }
}
RouteDefinition readRouteDefinition(const Json& data){return route(data);}
ItemDefinition readItemDefinition(const Json& v){
    ItemDefinition i;i.id=integer(v,"id",1,10000);i.name=v.at("name");i.description=v.at("description");i.equipmentType=v.at("source_type");i.attack=v.at("attributes").at("attack");i.defense=v.at("attributes").at("defense");i.spirit=v.at("attributes").value("spirit",0);i.agility=v.at("attributes").value("agility",0);
    if(v.contains("field_use")){i.fieldUse=v.at("field_use");for(const auto& allowed:v.at("actor_permissions")){const int value=allowed.get<int>();if(value!=0&&value!=1){throw std::runtime_error("Invalid actor permission");}i.actorPermissions.push_back(value!=0);}i.twoHanded=v.at("two_handed");}
    if(v.contains("medicine")){
        i.uses=integer(v,"uses",0,999);const auto& m=v.at("medicine");auto& effect=i.medicine;
        effect.hpPercent=integer(m,"hp_percent",0,100);effect.hpPoints=integer(m,"hp_points",0,99999);
        effect.tpPercent=integer(m,"tp_percent",0,100);effect.tpPoints=integer(m,"tp_points",0,99999);
        effect.koOnly=m.at("ko_only");effect.cureStates=m.at("cure_states").get<std::vector<int>>();
        for(int state:effect.cureStates){if(state!=2&&state!=3){throw std::runtime_error("Unsupported medicine state cure");}}
    }
    if(v.contains("magazine_capacity")){i.capacity=integer(v,"magazine_capacity",1,999);i.regularReload=v.value("regular_reload",true);if(!i.regularReload){if(v.contains("ammo_items")){throw std::runtime_error("Non-reloadable weapon has reserve ammunition");}return i;}const auto& a=v.at("ammo_items");i.looseItem=integer(a,"loose",1,10000);i.boxItem=integer(a,"box",0,10000);i.boxRounds=integer(a,"rounds_per_box",0,999);if((i.boxItem==0)!=(i.boxRounds==0)){throw std::runtime_error("Incomplete weapon ammo box definition");}}
    return i;
}
SoundDefinition readSound(const Json& j){return {j.at("resource"),integer(j,"volume_percent",0,100),integer(j,"pitch_percent",50,200),integer(j,"balance",0,100)};}
void CombatContent::setFullDefinition(Json definition){
    Json slots=Json::object();std::optional<Json> fatal;
    if(definition.contains("runtime")){
        auto& runtime=definition.at("runtime");
        for(const auto* branch:{"weapon_actions","combat_effects"}){
            if(runtime.contains(branch)&&runtime.at(branch).contains("enemy_sources")){
                slots[branch]=runtime.at(branch).at("enemy_sources");runtime.at(branch).erase("enemy_sources");
            }
        }
        if(runtime.contains("fatal_death")){fatal=runtime.at("fatal_death");runtime.erase("fatal_death");}
    }
    auto frozen=std::make_shared<const Json>(std::move(definition));
    full=std::move(frozen);enemyBurnSlots=std::move(slots);fatalDeath=std::move(fatal);
}
Json CombatContent::resolvedFullDefinition() const{
    auto definition=*full;
    for(const auto& [branch,slots]:enemyBurnSlots.items()){definition["runtime"][branch]["enemy_sources"]=slots;}
    if(fatalDeath){definition["runtime"]["fatal_death"]=*fatalDeath;}
    return definition;
}
const Json& CombatContent::enemyBurnSources() const{
    return enemyBurnSlots.at(full->at("runtime").contains("combat_effects")?"combat_effects":"weapon_actions");
}
const Json& CombatContent::cue(const std::string& id) const{return *cues.at(id);}
bool CombatContent::hasMediaDefinition(const std::string& group,const std::string& id) const{
    return group=="cues"?cues.contains(id):media.at(group).contains(id);
}
void CombatContent::setMediaDefinition(const std::string& group,const std::string& id,const Json& definition){
    if(group=="cues"){cues.insert_or_assign(id,std::make_shared<const Json>(definition));}
    else{media[group][id]=definition;}
}
void CombatContent::replaceMediaDefinitions(const std::string& group,const Json& definitions){
    if(group!="cues"){media[group]=definitions;return;}
    if(!definitions.is_object()){throw std::runtime_error("Invalid cue definitions");}
    decltype(cues) prepared;
    for(const auto& [id,definition]:definitions.items()){prepared.emplace(id,std::make_shared<const Json>(definition));}
    cues=std::move(prepared);
}
Json CombatContent::resolvedMedia() const{
    auto result=media;result["cues"]=Json::object();
    for(const auto& [id,definition]:cues){result["cues"][id]=*definition;}
    return result;
}
Json parseCombatContentDefinition(std::istream& input){
    const auto bytes=readContentBytes(input);
    // shortcut: pinned nlohmann 3.12.0 DOM SAX internals; recheck adapter/tests when upgrading JSON.
    static_assert(NLOHMANN_JSON_VERSION_MAJOR==3&&NLOHMANN_JSON_VERSION_MINOR==12&&NLOHMANN_JSON_VERSION_PATCH==0&&JSON_DIAGNOSTIC_POSITIONS==0);
    using Adapter=decltype(nlohmann::detail::input_adapter(bytes.data(),bytes.data()+bytes.size()));
    using Dom=nlohmann::detail::json_sax_dom_parser<Json,Adapter>;
    struct Strict final : Dom{
        using Dom::Dom;
        std::vector<std::set<std::string>> objects;
        bool start_object(std::size_t length){objects.emplace_back();return Dom::start_object(length);}
        bool key(std::string& value){if(!objects.back().insert(value).second){throw std::runtime_error("Duplicate content JSON key");}return Dom::key(value);}
        bool end_object(){objects.pop_back();return Dom::end_object();}
        bool parse_error(std::size_t,const std::string&,const Json::exception& error){throw std::runtime_error(error.what());}
    };
    Json result;Strict parser(result);
    if(!Json::sax_parse(bytes.data(),bytes.data()+bytes.size(),&parser)){throw std::runtime_error("Invalid content JSON");}
    return result;
}
CombatContent loadCombatContent(const std::filesystem::path& path){
    try{
        std::ifstream in(path);if(!in){throw std::runtime_error("Cannot open combat data");}
        if(!std::filesystem::is_regular_file(path)||std::filesystem::file_size(path)>128*1024*1024){throw std::runtime_error("Invalid or oversized content file");}
        return loadCombatContentDefinition(path,parseCombatContentDefinition(in));
    }catch(const std::exception& e){throw std::runtime_error(path.string()+": "+e.what());}
}
CombatContent loadCombatContentDefinition(const std::filesystem::path& path,const Json& d){
    try{
        if(d.at("schema")=="nobihaza.g9text.v1"){return loadExFontContent(path,d);}
        if(d.at("schema")=="nobihaza.g8late.v1"){return loadDifficultyLateContent(path,d);}
        if(d.at("schema")=="nobihaza.g8region.v1"){return loadDifficultyRegionContent(path,d);}
        if(d.at("schema")=="nobihaza.g8firstpart.v1"){return loadDifficultyFirstPartContent(path,d);}
        if(d.at("schema")=="nobihaza.g8initialization.v1"){return loadDifficultyInitializationContent(path,d);}
        if(d.at("schema")=="nobihaza.backgrounds.v1"){return loadBackgroundContent(path,d);}
        if(d.at("schema")=="nobihaza.g7bonus.v1"){return loadBonusContent(path,d);}
        if(d.at("schema")=="nobihaza.g7cycle.v1"){return loadCycleContent(path,d);}
        if(d.at("schema")=="nobihaza.g7equipment.v1"){return loadSpecialEquipmentContent(path,d);}
        if(d.at("schema")=="nobihaza.g7clear.v1"){return loadClearContent(path,d);}
        if(d.at("schema")=="nobihaza.g7epilogue.v1"){return loadEpilogueContent(path,d);}
        if(d.at("schema")=="nobihaza.g7score.v1"){return loadScoreContent(path,d);}
        if(d.at("schema")=="nobihaza.g6cargo.v1"){return loadCargoPlatformContent(path,d);}
        if(d.at("schema")=="nobihaza.g7departure.v1"){return loadCockpitDepartureContent(path,d);}
        if(d.at("schema")=="nobihaza.g7.v1"){return loadFinalContent(path,d);}
        if(d.at("schema")=="nobihaza.g6.v1"){return loadReturnContent(path,d);}
        if(d.at("schema")=="nobihaza.g5.v1"){return d.at("content_compatibility")=="post-f5-g5-normal-3"?loadControlContent(path,d):d.at("content_compatibility")=="post-f5-g5-normal-2"?loadAftermathContent(path,d):loadCashContent(path,d);}
        if(d.at("schema")=="nobihaza.g4.v1"){return d.at("content_compatibility")=="post-f5-g4-normal-3"?loadTruthContent(path,d):d.at("content_compatibility")=="post-f5-g4-normal-2"?loadDoraContent(path,d):loadTowerContent(path,d);}
        if(d.at("schema")=="nobihaza.g3.v1"){return loadArea3Content(path,d);}
        if(d.at("schema")=="nobihaza.g2.v1"){return loadArea2Content(path,d);}
        if(d.at("schema")=="nobihaza.g1.v1"){return loadG1Content(path,d);}
        if(d.at("schema")=="nobihaza.g0.v1"){return loadG0Content(path,d);}
        if(d.at("schema")=="nobihaza.f4full.v1"){return loadFullContent(path,d);}
        if(d.at("schema")!="nobihaza.combat.v1"&&d.at("schema")!="nobihaza.f3.v1"&&d.at("schema")!="nobihaza.f4.v1"&&d.at("schema")!="nobihaza.f4bc.v1"){throw std::runtime_error("Unsupported combat schema");}
        CombatContent c;
        const bool branchSchema=d.at("schema")=="nobihaza.f4bc.v1";
        const bool f4Schema=d.at("schema")=="nobihaza.f4.v1"||branchSchema;
        const bool f3Schema=d.at("schema")!="nobihaza.combat.v1";
        if(f4Schema){
            if(d.at("content_compatibility")!=(branchSchema?"f4bc-normal-1":"f4a-normal-1")){throw std::runtime_error("Unknown F4 content compatibility");}
            c.compatibility=branchSchema?"f4bc-normal-1":"f4a-normal-1";
            // Fixed source bindings from the audited A contract (M19/M21/M22).
            for(const auto& [id,name]:std::map<int,std::string>{{57,"m19.licker-activated"},{61,"m21-e11.unlocked"},{66,"m22-e4.collected"},{105,"m19-e2.dead"},{108,"m21-e3.dead"},{109,"m21-e5.dead"},{110,"m21-e7.dead"}}){c.facts.switches.emplace(id,name);}
            c.magazineSeedBindings.emplace(54,14);c.magazineSeedBindings.emplace(61,21);
            c.requiredMagazines={{11,15},{20,6},{14,5},{21,8}};
        }
        if(f3Schema){
            c.catalog=d.at("f3");
            for(const auto* group:{"enemies","interactions","sequences","environment"}){
                if(!c.catalog.at(group).is_array()){throw std::runtime_error(std::string("Invalid F3 group: ")+group);}
            }
            if(!c.catalog.at("page_presentations").is_object()||!d.at("batch").is_string()){throw std::runtime_error("Invalid F3 contract");}
        }
        c.directory=path.parent_path();c.rules=d.at("rules");c.interactions=d.at("interactions");c.transitions=d.at("transitions");c.actionProfiles=d.at("action_profiles");
        // Fixed CN1.00 F3 adapter: CE75 C13-15 yields between ending an old I20 bullet and starting the next.
        // Keep the accepted, immutable F3 package usable without rewriting its files.
        if(f3Schema&&c.actionProfiles.contains("magnum-fire")&&!c.actionProfiles.at("magnum-fire").contains("replacement_wait_tenths")){
            c.actionProfiles["magnum-fire"]["replacement_wait_tenths"]=0;
        }
        for(const auto* key:{"images","audio","animations","cues","sounds","ui","terms","actors","effects"}){c.replaceMediaDefinitions(key,d.at(key));}
        // Compatibility adapter for the immutable task-11 package. New F3 packages carry these bindings and flows.
        if(!f3Schema){
            const std::vector<std::tuple<std::string,std::string,int,std::string,int,int>> legacy{
                {"m4-to-m6","m4-e2",4,"m4-e2-p4",4,8},
                {"m6-to-m4","m6-e11",2,"m6-e11-p2",3,7},
                {"m6-next-left","m6-e13",3,"m6-e13-p3",3,7},
                {"m6-next-right","m6-e14",3,"m6-e14-p3",3,7}};
            for(const auto& [id,object,page,prefix,erase,show]:legacy){
                auto& t=c.transitions.at(id);t["object"]=object;t["page"]=page;t["facing"]="preserve";
                auto flow=Json::array({{{"kind","flag"},{"id",1},{"value",false}},{{"kind","flag"},{"id",18},{"value",false}}});
                if(erase==4){flow.push_back({{"kind","cue"},{"cue",prefix+"-c3"}});}
                flow.push_back({{"kind","cue"},{"cue",prefix+"-c"+std::to_string(erase)}});
                flow.push_back({{"kind","room_reset"}});flow.push_back({{"kind","relocate"}});
                flow.push_back({{"kind","cue"},{"cue",prefix+"-c"+std::to_string(show-1)}});
                flow.push_back({{"kind","cue"},{"cue",prefix+"-c"+std::to_string(show)}});
                flow.push_back({{"kind","flag"},{"id",1},{"value",true}});flow.push_back({{"kind","flag"},{"id",18},{"value",true}});t["flow"]=flow;
            }
        }
        const auto sprites=readSceneDefinitions(c,d);
        for(const auto& [id,v]:d.at("items").items()){
            auto i=readItemDefinition(v);
            if(f4Schema&&d.at("capabilities").at("weapons").contains(id)){
                const auto capability=d.at("capabilities").at("weapons").at(id).get<std::string>();
                if(capability!="state-only"&&capability!="executable"){throw std::runtime_error("Unknown weapon capability");}
                i.stateOnly=capability=="state-only";
                if(branchSchema&&(i.id==12||i.id==27||i.id==29)){i.stateOnly=true;i.pendingActionTicket=79;}
            }
            if(std::stoi(id)!=i.id){throw std::runtime_error("Mismatched item ID");}c.items.emplace(i.id,i);
        }
        if(f3Schema){c.media["skills"]=d.at("skills");for(const auto& [id,v]:d.at("skills").items()){
            SkillDefinition skill;skill.id=integer(v,"id",1,10000);skill.type=integer(v,"type",0,3);skill.scope=integer(v,"scope",0,4);
            skill.magicalRate=v.value("magical_rate",skill.id<=2?0:3);skill.tpCost=integer(v,"tp_cost",0,9999);skill.implementationTicket=v.at("action_implementation_ticket");skill.fieldUse=v.at("field_use");skill.name=v.at("name");skill.description=v.at("description");
            if(v.contains("runtime_effect")){
                const auto effect=v.at("runtime_effect").get<std::string>();
                if(effect=="precision"){skill.effect=SkillEffect::Precision;}
                else if(effect=="quickshot"){skill.effect=SkillEffect::Quickshot;skill.durationTicks=integer(v,"duration_tenths",1,10000)*6;}
                else{throw std::runtime_error("Unsupported skill effect");}
            }
            if(std::stoi(id)!=skill.id){throw std::runtime_error("Skill ID mismatch");}c.skills.emplace(skill.id,skill);
        }}
        if(f3Schema){
            const auto cueExists=[&](const std::string& id,const std::string& kind){
                if(!c.cues.contains(id)||(!kind.empty()&&c.cue(id).at("kind")!=kind)){throw std::runtime_error("Missing or invalid weapon cue: "+id);}
            };
            for(const auto& [id,item]:c.items){
                if((item.equipmentType==1||item.equipmentType==2)&&!item.stateOnly&&!c.catalog.at("weapon_bindings").contains(std::to_string(id))){throw std::runtime_error("Missing supported weapon binding");}
            }
            for(const auto& [key,binding]:c.catalog.at("weapon_bindings").items()){
                if(!c.items.contains(std::stoi(key))){throw std::runtime_error("Unknown weapon binding item");}
                const auto kind=binding.at("kind").get<std::string>();
                if(kind!="handgun"&&kind!="knife"&&kind!="magnum"&&!(f4Schema&&(kind=="de"||kind=="shotgun"))){throw std::runtime_error("Unsupported weapon hit strategy");}
                if(!binding.at("aim").is_boolean()){throw std::runtime_error("Invalid weapon aiming qualifier");}
                for(const auto* role:{"raise","lower","fire"}){binding.at(role).get<std::string>();}
                if(kind=="knife"){binding.at("recover").get<std::string>();}
                else{
                    binding.at("reload").get<std::string>();cueExists(binding.at("flash"),"flash_character");
                    const auto prefix=binding.at("effect_prefix").get<std::string>();
                    for(const auto* direction:{"left","right","down"}){if(!c.media.at("effects").contains(prefix+direction)){throw std::runtime_error("Missing weapon effect");}}
                }
                for(const auto& [field,value]:binding.items()){
                    if(field=="kind"||field=="aim"||field=="flash"||field=="effect_prefix"){continue;}
                    if(field!="raise"&&field!="lower"&&field!="fire"&&field!="reload"&&field!="recover"&&field!="quick_fire"){throw std::runtime_error("Unknown weapon binding field");}
                    const auto& profile=c.actionProfiles.at(value.get<std::string>());
                    if(!profile.at("cue_order").is_array()||profile.at("cue_order").empty()){throw std::runtime_error("Empty weapon action");}
                    for(const auto& id:profile.at("cue_order")){cueExists(id,"");}
                    if(kind!="knife"&&(field=="fire"||field=="quick_fire")){cueExists(profile.at("sound_cue"),"sound");}
                    if(profile.contains("sound_cue")){cueExists(profile.at("sound_cue"),"sound");}
                    if(profile.contains("start_cue_order")){for(const auto& cue:profile.at("start_cue_order")){cueExists(cue,"");}}
                    if(profile.contains("replacement_wait_tenths")){integer(profile,"replacement_wait_tenths",0,1000);}
                    if(kind=="de"&&field=="fire"&&(!profile.contains("start_cue_order")||!profile.contains("replacement_wait_tenths"))){throw std::runtime_error("Missing DE attack replacement stages");}
                }
            }
            const auto& effect=c.catalog.at("quickshot_presentation");integer(effect,"period_ticks",1,10000);cueExists(effect.at("cue"),"flash_character");
        }
        std::set<std::string> openingMessages;
        if(f3Schema&&(f4Schema||d.at("batch")=="C")){
            const auto& opening=c.catalog.at("opening");
            if(opening.at("implementation_ticket")!=43||opening.at("runtime_enabled")!=true){throw std::runtime_error("Invalid deferred opening scope");}
            for(const auto& id:opening.at("message_ids")){
                if(!openingMessages.insert(id.get<std::string>()).second||!d.at("messages").contains(id.get<std::string>())){throw std::runtime_error("Invalid deferred opening message ID");}
            }
            c.catalog["opening"]["message_definitions"]=Json::object();
        }
        for(const auto& [id,v]:d.at("messages").items()){
            if(openingMessages.contains(id)){
                if(v.at("implementation_ticket")!=43||v.at("runtime_enabled")!=true){throw std::runtime_error("Invalid deferred opening message");}
                static const std::set<std::string> tokens{"text","instant_on","instant_off","color","short_wait","long_wait","speed","close_without_input","wait_input","literal_slash"};
                if(v.at("lines").empty()||v.at("lines").size()>4){throw std::runtime_error("Invalid opening message lines");}
                for(const auto& line:v.at("lines")){
                    for(const auto& token:line.at("tokens")){if(!tokens.contains(token.at("type").get<std::string>())){throw std::runtime_error("Unknown opening message token");}}
                }
                // Preserve full future content, but do not make it executable by the
                // current dialogue controller. Active flows must resolve c.messages.
                c.catalog["opening"]["message_definitions"][id]=v;
                c.messages.emplace(id,readCombatMessage(v));
            }else{
                if(v.contains("implementation_ticket")||v.value("runtime_enabled",true)==false){throw std::runtime_error("Unregistered deferred message");}
                c.messages.emplace(id,readCombatMessage(v));
            }
        }
        for(const auto& [id,v]:d.at("cues").items()){if(v.at("kind")=="forced_route"&&(!v.contains("runtime_support")||(d.at("schema")=="nobihaza.f4bc.v1"&&id=="m4-e4-p21-c3"&&v.at("runtime_support")=="offline-only-other-weapon-body-transition"))){c.routes.emplace(id,route(v));}}
        for(const auto& [id,v]:d.at("entries").items()){
            CombatEntry e;e.map=v.at("map");e.playerTile=tile(v.at("player").at("tile"));e.direction=directionFromName(v.at("player").at("direction").get<std::string>());e.sprite=v.at("player").at("sprite");
            auto& s=e.session;if(v.contains("skills")){s.skills=v.at("skills").get<std::set<int>>();}s.hp=v.at("player").at("hp");s.maxHp=v.at("player").value("max_hp",100);s.tp=v.at("player").value("tp",30);s.maxTp=v.at("player").value("max_tp",30);s.baseAttack=d.at("actors").at("1").at("attributes").at("attack");s.baseDefense=d.at("actors").at("1").at("attributes").at("defense");s.baseAgility=d.at("actors").at("1").at("attributes").at("agility");s.baseSpirit=d.at("actors").at("1").at("attributes").at("spirit");s.inventory=idMap<int>(v.at("inventory"));s.magazines=idMap<int>(v.at("magazines"));s.flags=idMap<bool>(v.at("switches"));s.variables=idMap<int>(v.at("variables"));
            const auto& equipment=v.at("equipment");s.mainWeapon=equipment.at("main_weapon").is_null()?0:equipment.at("main_weapon").get<int>();s.auxiliaryWeapon=equipment.at("auxiliary_weapon").is_null()?0:equipment.at("auxiliary_weapon").get<int>();s.armor=equipment.at("armor").is_null()?0:equipment.at("armor").get<int>();
            for(const auto& [object,state]:v.at("objects").items()){
                if(!c.objects.contains(object)){throw std::runtime_error("Unknown entry object: "+object);}
                e.pages[object]=state.at("page").is_null()?0:state.at("page").get<int>();e.positions[object]=tile(state.at("tile"));e.through[object]=state.at("through");e.directions[object]=directionFromName(state.at("direction").get<std::string>());
            }
            GameSession validate(c.items,s,c.facts);c.entries.emplace(id,std::move(e));
        }
        for(const auto& [id,e]:c.entries){
            if(!c.maps.contains(e.map)||!sprites.contains(e.sprite)){throw std::runtime_error("Unknown entry map or sprite: "+id);}
            const auto& map=c.maps.at(e.map);TerrainMap bounds(map.width,map.height,map.passageMasks);
            if(!bounds.contains(e.playerTile)){throw std::runtime_error("Entry player outside map: "+id);}
            for(const auto& [object,position]:e.positions){
                if(c.objects.at(object).map!=e.map||!bounds.contains(position)){throw std::runtime_error("Entry object outside map: "+object);}
                const int page=e.pages.at(object);
                if(page&&!std::any_of(c.objects.at(object).variants.begin(),c.objects.at(object).variants.end(),[&](const auto& v){return v.page==page;})){throw std::runtime_error("Unknown entry page: "+object);}
            }
        }
        validateSceneContent(c,sprites);
        const auto executableCue=[&](const std::string& id,const std::string& requiredKind=std::string{}){
            const auto& cue=c.cue(id);
            if(cue.contains("runtime_support")||(!requiredKind.empty()&&cue.at("kind")!=requiredKind)){throw std::runtime_error("Cue is not executable in this role: "+id);}
        };
        for(const auto& [id,p]:c.actionProfiles.items()){
            if(p.contains("cue_order")){for(const auto& cue:p.at("cue_order")){executableCue(cue.get<std::string>());}}
        }
        const auto& profile=c.rules.at("enemy").at("runtime_profile");
        executableCue(profile.at("grab_release_wait"),"wait");
        integer(c.cue(profile.at("grab_release_wait").get<std::string>()),"tenths",0,std::numeric_limits<int>::max()/6);
        const auto& enemy=c.objects.at(c.rules.at("enemy").at("id").get<std::string>());
        const int retreatPage=integer(profile,"retreat_page",1,9999);
        if(std::none_of(enemy.variants.begin(),enemy.variants.end(),[&](const auto& v){return v.page==retreatPage;})){
            throw std::runtime_error("Missing enemy retreat page");
        }
        for(const auto& [role,id]:profile.at("sprites").items()){if(!sprites.contains(id.get<std::string>())){throw std::runtime_error("Missing enemy sprite: "+role);}}
        for(const auto& [role,id]:profile.at("cues").items()){executableCue(id.get<std::string>(),role.ends_with("route")||role.starts_with("approach_")?"forced_route":"sound");}
        for(const auto& [direction,id]:profile.at("retreat_routes").items()){directionFromName(direction);executableCue(id,"forced_route");}
        executableCue(profile.at("retreat_flash"),"flash_character");
        for(const auto& stage:profile.at("feeding")){executableCue(stage.at("route"),"forced_route");if(stage.contains("sound")){executableCue(stage.at("sound"),"sound");}}
        const auto firstId=c.rules.at("enemy").at("id").get<std::string>();
        c.enemies.emplace(firstId,EnemyDefinition{firstId,c.objects.at(firstId).map,81,101,true,Json{}});
        if(f3Schema){for(const auto& e:c.catalog.at("enemies")){
            const std::string id=e.at("id");auto rules=c.rules.at("enemy");
            rules["id"]=id;rules["bite_attack"]=e.at("normal_attack");rules["defense"]=e.at("normal_defense");
            rules["death_damage_threshold"]=e.at("termination_damage_threshold");rules["runtime_profile"]=e.at("runtime_profile");
            const auto& p=rules.at("runtime_profile");
            for(const auto& [role,s]:p.at("sprites").items()){if(!sprites.contains(s.get<std::string>())){throw std::runtime_error("Enemy sprite missing: "+id+role);}}
            for(const auto& [role,q]:p.at("cues").items()){executableCue(q.get<std::string>());}
            for(const auto* group:{"retreat_routes","collision_yield_routes","evade_routes"}){
                for(const auto* direction:{"up","right","down","left"}){executableCue(p.at(group).at(direction),"forced_route");}
            }
            for(const auto* role:{"pursuit_page","retreat_page"}){
                const int page=p.at(role);const auto& variants=c.objects.at(id).variants;
                if(std::none_of(variants.begin(),variants.end(),[&](const auto& v){return v.page==page;})){throw std::runtime_error("Enemy page missing: "+id);}
            }
            executableCue(p.at("grab_release_wait"),"wait");executableCue(p.at("head_delay"),"wait");
            executableCue(p.at("head_animation"),"animation");executableCue(p.at("head_final_route"),"forced_route");
            if(c.objects.at(id).map!=e.at("map")){throw std::runtime_error("Enemy map mismatch");}
            if(!c.enemies.emplace(id,EnemyDefinition{id,e.at("map"),e.at("source_slots").at("base"),e.at("dead_switch"),false,std::move(rules)}).second){throw std::runtime_error("Duplicate enemy identity: "+id);}
        }}
        std::set<std::pair<int,int>> orders;
        for(const auto& [id,e]:c.enemies){if(!orders.emplace(e.map,e.sourceOrder).second){throw std::runtime_error("Duplicate enemy processing order");}}
        for(const auto& [id,t]:c.transitions.items()){
            const auto& object=c.objects.at(t.at("object").get<std::string>());const int page=t.at("page");
            if(std::none_of(object.variants.begin(),object.variants.end(),[&](const auto& v){return v.page==page;})){throw std::runtime_error("Transfer page missing: "+id);}
            const auto& dest=t.at("destination");
            if(!t.value("handoff_only",false)){
                const auto& target=c.maps.at(dest.at("map").get<int>());TerrainMap bounds(target.width,target.height,target.passageMasks);
                if(!bounds.contains(tile(dest.at("tile")))){throw std::runtime_error("Transfer target outside map: "+id);}
            }
            int relocations=0;
            for(const auto& step:t.at("flow")){
                const std::string kind=step.at("kind");
                if(kind=="cue"){executableCue(step.at("cue"));}
                else if(kind=="message"){(void)c.messages.at(step.at("message").get<std::string>());}
                else if(kind=="relocate"){++relocations;}
                else if(kind=="flag"){integer(step,"id",1,10000);step.at("value").get<bool>();}
                else if(branchSchema&&kind=="dormant_licker_entry"){if(step.size()!=5||(step.at("requires_flag")!=170&&step.at("requires_flag")!=180)||step.at("source_slot")!=1||step.at("state")!=9){throw std::runtime_error("Unsupported dormant enemy transfer");}}
                else if(kind!="room_reset"&&kind!="enable_combine_if_known"){throw std::runtime_error("Unsupported transfer step: "+kind);}
            }
            if(relocations!=1){throw std::runtime_error("Transfer must relocate exactly once: "+id);}
        }
        if(f3Schema){
            const auto& menu=c.catalog.at("menu");
            (void)c.media.at("images").at(menu.at("actor_face").at("image").get<std::string>());
            integer(menu.at("actor_face"),"index",0,15);
            for(const auto* group:{"command_terms","confirmation_terms"}){
                for(const auto& [key,term]:menu.at(group).items()){(void)c.media.at("terms").at(std::to_string(term.get<int>()));}
            }
            for(const auto& slot:menu.at("slots")){integer(slot,"slot",1,5);integer(slot,"source_type",1,5);(void)c.media.at("terms").at(std::to_string(slot.at("term").get<int>()));}
            for(const auto& [id,t]:c.transitions.items()){
                if(t.contains("runtime_enabled")){t.at("runtime_enabled").get<bool>();integer(t,"implementation_ticket",1,9999);}
            }
            const auto pageRef=[&](const Json& value){
                const auto& object=c.objects.at(value.at("object").get<std::string>());const int page=value.at("page");
                if(std::none_of(object.variants.begin(),object.variants.end(),[&](const auto& v){return v.page==page;})){throw std::runtime_error("Exploration page missing");}
                return object.map;
            };
            for(const auto& [id,f]:c.catalog.at("flows").items()){
                pageRef(f);const std::string trigger=f.at("trigger");
                if(trigger!="action"&&trigger!="autorun"&&trigger!="environment"){throw std::runtime_error("Unsupported exploration trigger");}
                if(!f.at("steps").is_array()||f.at("steps").empty()){throw std::runtime_error("Empty exploration flow");}
                std::vector<Json> pending=f.at("steps").get<std::vector<Json>>();
                for(std::size_t index=0;index<pending.size();++index){
                    const auto step=pending[index];
                    const std::string kind=step.at("kind");
                    if(trigger=="environment"&&kind!="cue"){throw std::runtime_error("Environment cannot mutate domain state");}
                    if(kind=="cue"){executableCue(step.at("cue"));}
                    else if(kind=="message"){(void)c.messages.at(step.at("message").get<std::string>());}
                    else if(kind=="flag"){integer(step,"id",1,10000);step.at("value").get<bool>();}
                    else if(kind=="grant"){(void)c.items.at(step.at("item").get<int>());integer(step,"count",1,99);}
                    else if(kind=="system_graphic"){(void)c.media.at("images").at(step.at("image").get<std::string>());}
                    else if(kind=="discard_key"){
                        (void)c.items.at(step.at("item").get<int>());const auto& choice=c.cue(step.at("choice").get<std::string>());
                        if(choice.at("kind")!="choices"||choice.at("choices").size()!=2){throw std::runtime_error("Invalid key choice");}
                    }else if(kind=="password_reward"){
                        if(integer(step,"digits",4,4)!=4||integer(step,"password_variable",388,388)!=388){throw std::runtime_error("Invalid password binding");}
                        for(const auto* name:{"success","failure"}){
                            if(!step.at(name).is_array()||step.at(name).empty()){throw std::runtime_error("Empty password branch");}
                            for(const auto& nested:step.at(name)){
                                const std::string k=nested.at("kind");if(k!="flag"&&k!="grant"&&k!="message"&&k!="cue"&&k!="wait_routes"){throw std::runtime_error("Nested return branch unsupported");}
                                pending.push_back(nested);
                            }
                        }
                    }else if(kind!="wait_routes"){throw std::runtime_error("Unsupported exploration operation: "+kind);}
                }
            }
            if(f4Schema||d.at("batch").get<std::string>()>="B"){
                if(!c.catalog.at("readings").contains("manual")){throw std::runtime_error("Missing B reading");}
                for(const auto& [id,reading]:c.catalog.at("readings").items()){
                    (void)c.items.at(reading.at("item").get<int>());
                    const auto& next=reading.at("continuation");
                    if(next.at("repeat")!="open"||next.at("after_reading")!="menu-root"||next.at("after_menu_close")!="close"){throw std::runtime_error("Unsupported reading continuation");}
                    if(reading.at("consumes_item").get<bool>()){throw std::runtime_error("Reading cannot consume its document");}
                    for(const auto* stage:{"open","body","return_to_menu","close"}){
                        if(!reading.at(stage).is_array()||reading.at(stage).empty()){throw std::runtime_error("Empty reading stage");}
                        for(const auto& step:reading.at(stage)){
                            const std::string kind=step.at("kind");
                            if(kind=="message"){(void)c.messages.at(step.at("message").get<std::string>());}
                            else if(kind=="cue"){
                                executableCue(step.at("cue"));
                                const std::string cueKind=c.cue(step.at("cue").get<std::string>()).at("kind");
                                const std::set<std::string> supported{"message_options","show_picture","move_picture","erase_picture","wait","sound"};
                                if(!supported.contains(cueKind)){throw std::runtime_error("Unsupported reading cue");}
                            }
                            else{throw std::runtime_error("Unsupported reading operation");}
                        }
                    }
                }
            }
            for(const auto& l:c.catalog.at("ladders")){
                const int map=pageRef(l);integer(l,"mounted_flag",1,10000);integer(l,"busy_flag",1,10000);
                for(const auto* d:{"up","right","down","left"}){
                    executableCue(l.at("climb").at(d),"forced_route");
                    for(const auto* k:{"object_route","player_route"}){executableCue(l.at("push").at(d).at(k),"forced_route");}
                    const auto& h=l.at("helpers").at(d);if(pageRef(h)!=map){throw std::runtime_error("Ladder helper map mismatch");}
                    for(const auto* k:{"place_route","exit_route"}){executableCue(h.at(k),"forced_route");}
                }
            }
            const auto& save=c.catalog.at("save_flow");
            static const std::set<std::string> saveKeys{"message","choice","question_system","slot_system","restore_system","slot_sounds","restore_sounds","slot_face","restore_options","source"};
            if(save.size()!=saveKeys.size()){throw std::runtime_error("Missing or unknown save-flow field");}for(const auto& [key,value]:save.items()){if(!saveKeys.contains(key)){throw std::runtime_error("Unknown save-flow field");}}
            (void)c.messages.at(save.at("message").get<std::string>());executableCue(save.at("choice"),"choices");
            for(const auto* key:{"question_system","slot_system","restore_system"}){(void)c.media.at("images").at(save.at(key).get<std::string>());}
            for(const auto* group:{"slot_sounds","restore_sounds"}){for(const auto* key:{"cursor","decision","cancel"}){auto sound=readSound(save.at(group).at(key));(void)c.media.at("audio").at(sound.resource);}}
            for(const auto& p:c.catalog.at("save_points")){pageRef(p);directionFromName(p.at("facing").get<std::string>());p.at("location").get<std::string>();}
            for(const auto& b:c.catalog.at("boundaries")){pageRef(b);integer(b,"ticket",1,9999);b.at("label").get<std::string>();}
            for(const auto& p:c.catalog.at("anchored_pictures")){
                const int map=pageRef(p);
                for(const auto& a:p.at("pictures")){
                    executableCue(a.at("cue"),"show_picture");
                    if(c.objects.at(a.at("anchor").get<std::string>()).map!=map){throw std::runtime_error("Picture anchor map mismatch");}
                }
            }
        }
        if(f4Schema){
            const auto& additional=d.at("f4");
            std::set<std::string> groups{"enemies","enemy_behaviors","attacks","flows","encounters","entry_placements","environment","blood_loop","reload_equipment","save_points","failure_flows","map_music"};
            if(d.at("batch")!="A"){groups.insert({"staged_flows","tool","mixing","meeting"});}
            if(branchSchema||d.at("batch")=="C"){groups.insert("meeting_story");}
            if(additional.size()!=groups.size()){throw std::runtime_error("Missing or unknown expanded content group");}
            for(const auto& [key,v]:additional.items()){if(!groups.contains(key)){throw std::runtime_error("Unknown expanded content group");}}
            const std::map<std::string,EnemyBehaviorKind> behaviorKinds{{"licker",EnemyBehaviorKind::Licker},{"running-zombie",EnemyBehaviorKind::RunningZombie},{"falling-zombie",EnemyBehaviorKind::FallingZombie},{"soldier-zombie",EnemyBehaviorKind::SoldierZombie}};
            std::set<std::pair<int,int>> sourceOrders;
            for(const auto& row:additional.at("enemies")){
                EnemyBlueprint e;e.id=row.at("id");e.map=integer(row,"map",1,10000);e.event=integer(row,"event",1,10000);e.sourceOrder=integer(row,"source_order",1,10000);
                e.behavior=behaviorKinds.at(row.at("behavior").get<std::string>());e.sourceSlot=integer(row.at("source_slots"),"base",1,10000);
                e.deadFlag=integer(row,"dead_switch",1,10000);e.attack=integer(row,"normal_attack",1,9999);e.defense=integer(row,"normal_defense",0,9999);e.terminationDamage=integer(row,"termination_damage_threshold",1,99999);
                if(!row.at("activation_fact").is_null()){e.activationFlag=integer(row,"activation_fact",1,10000);}
                const auto& half=row.at("hitbox_half_size_pixels");if(!half.is_array()||half.size()!=2){throw std::runtime_error("Invalid enemy geometry");}
                for(int n=0;n<2;++n){if(!half[n].is_number_integer()||half[n]<=0||half[n]>128){throw std::runtime_error("Invalid enemy geometry");}e.hitboxHalfPixels[n]=half[n];}
                if(!c.objects.contains(e.id)||c.objects.at(e.id).map!=e.map||c.objects.at(e.id).variants.front().definition.sourceEventId!=e.event||!c.facts.switches.contains(e.deadFlag)){throw std::runtime_error("Enemy blueprint references missing content");}
                if(!sourceOrders.emplace(e.map,e.sourceOrder).second||!c.expansion.enemies.emplace(e.id,e).second){throw std::runtime_error("Duplicate expanded enemy identity/order");}
            }
            for(const auto& [id,row]:additional.at("attacks").items()){
                if(row.at("strategy")=="shotgun-distance"){
                    AttackDefinition a;a.strategy=ProjectileStrategy::ShotgunDistance;a.muzzleTest=true;
                    a.forwardOffset=integer(row,"forward_sample_offset_px",0,128);
                    a.sampleLeadPixels=integer(row.at("flight_motion"),"compatibility_sample_lead_px",0,128);a.flightCalls=integer(row.at("flight_motion"),"original_forward_steps",1,100);
                    a.wallTerrain=integer(row,"wall_terrain",1,10000);a.endOnHitFromAdvance=integer(row,"end_on_hit_from_advance",0,100);
                    a.muzzleHalf=row.at("muzzle_half_px").get<std::array<int,2>>();a.verticalHalf=row.at("flight_half_vertical_px").get<std::array<int,2>>();a.horizontalHalf=row.at("flight_half_horizontal_px").get<std::array<int,2>>();
                    a.responseByAdvance=row.at("response_by_advance").get<std::array<int,4>>();
                    int n=0;for(const auto* direction:{"up","right","down","left"}){a.muzzleForward[n++]=integer(row.at("muzzle_forward_px"),direction,0,128);}
                    if(!c.items.contains(std::stoi(id))){throw std::runtime_error("Unknown attack weapon");}c.expansion.attacks.emplace(std::stoi(id),a);continue;
                }
                if(row.at("strategy")!="magnum-flight"){throw std::runtime_error("Unknown projectile strategy");}
                AttackDefinition a;a.muzzleTest=row.at("muzzle_test");a.endOnTarget=row.at("end_on_target");
                a.forwardOffset=integer(row,"forward_sample_offset_px",0,128);a.responsePage=integer(row,"response_page",1,6);a.wallTerrain=integer(row,"wall_terrain",0,1000);
                const auto& half=row.at("projectile_half_px");if(!half.is_array()||half.size()!=2){throw std::runtime_error("Invalid projectile geometry");}
                for(int n=0;n<2;++n){if(!half[n].is_number_integer()||half[n]<=0||half[n]>128){throw std::runtime_error("Invalid projectile geometry");}a.halfPixels[n]=half[n];}
                if(!c.items.contains(std::stoi(id))){throw std::runtime_error("Unknown attack weapon");}c.expansion.attacks.emplace(std::stoi(id),a);

            }
            for(const auto& [name,body]:additional.at("enemy_behaviors").items()){if(!behaviorKinds.contains(name)){throw std::runtime_error("Unknown enemy behavior definition");}}
            auto& x=c.expansion;x.behaviorDefinitions=additional.at("enemy_behaviors");x.flows=additional.at("flows");x.encounters=additional.at("encounters");x.entryPlacements=additional.at("entry_placements");x.environment=additional.at("environment");x.bloodLoop=additional.at("blood_loop");x.reloadEquipment=additional.at("reload_equipment");x.savePoints=additional.at("save_points");x.failureFlows=additional.at("failure_flows");x.mapMusic=additional.at("map_music");
            if(d.at("batch")!="A"){x.stagedFlows=additional.at("staged_flows");x.tool=additional.at("tool");x.mixing=additional.at("mixing");x.meeting=additional.at("meeting");}
            if(branchSchema||d.at("batch")=="C"){x.meetingStory=additional.at("meeting_story");}
            if(branchSchema){auto base=d;base["batch"]="C";base["capabilities"]={{"weapons",{{"14","executable"},{"21","executable"}}},{"delivered_batch","C"}};validateExpandedContent(c,base);}
            else{validateExpandedContent(c,d);}
            if(!x.mixing.is_null()){
                c.facts.switches.emplace(73,"m24-e10.entered");c.facts.switches.emplace(77,"m24-e5.talked");
                MixingRecipe recipe;for(const auto& i:x.mixing.at("ingredients")){recipe.ingredients.emplace(i.at("item"),i.at("count"));}recipe.result=x.mixing.at("result").at("item");recipe.count=x.mixing.at("result").at("count");x.mixingRecipe=std::move(recipe);}
            if(!x.stagedFlows.is_null()){
                // CN1.00 M11 E5P3 C10-11 selects the key required by this event
                // page, calls CE100, then resumes at C12. Reuse the same finite
                // choice operation as the archive door; no map logic in the scene.
                const auto& before=x.stagedFlows.at("wardrobe-before-discard");
                const auto& after=x.stagedFlows.at("wardrobe-after-discard");
                const auto& object=c.objects.at(before.at("object").get<std::string>());
                const auto page=std::find_if(object.variants.begin(),object.variants.end(),[&](const auto& v){return v.page==before.at("page");});
                if(page==object.variants.end()||!page->inventoryItem||after.at("object")!=before.at("object")||after.at("page")!=before.at("page")){
                    throw std::runtime_error("Invalid wardrobe key/page binding");
                }
                auto steps=before.at("steps");
                for(const auto& step:x.tool.at("discard_message_steps")){steps.push_back(step);}
                steps.push_back({{"kind","discard_key"},{"item",*page->inventoryItem},{"choice",x.tool.at("discard_choice")}});
                for(const auto& step:after.at("steps")){steps.push_back(step);}
                const auto install=[&](const std::string& id,const Json& staged,const Json& actions){
                    c.catalog["flows"][id]={{"object",staged.at("object")},{"page",staged.at("page")},{"trigger","action"},{"steps",actions}};
                };
                install("f4-wardrobe-unlock",before,steps);
                const auto& wrench=x.stagedFlows.at("wrench-pickup");install("f4-wrench-pickup",wrench,wrench.at("steps"));
                c.facts.switches.emplace(67,"m11-e5.unlocked");c.facts.switches.emplace(68,"m11-e5.wrench-collected");
                const auto& vent=x.stagedFlows.at("vent-remove");auto ventSteps=vent.at("steps");
                // The adjacent I62 grant/I63 removal represent a single retained tool.
                for(std::size_t i=0;i+1<ventSteps.size();++i){
                    if(ventSteps[i].at("kind")=="grant"&&ventSteps[i].at("item")==x.tool.at("inactive_item")&&ventSteps[i+1].at("kind")=="remove"&&ventSteps[i+1].at("item")==x.tool.at("active_item")){
                        ventSteps[i]={{"kind","convert_tool"},{"from",x.tool.at("active_item")},{"to",x.tool.at("inactive_item")}};ventSteps.erase(i+1);break;
                    }
                }
                install("f4-tool-remove",vent,ventSteps);c.catalog["flows"]["f4-tool-remove"]["trigger"]="request";
                c.facts.switches.emplace(71,"tool.wrench-requested");c.facts.switches.emplace(72,"m12-e14.bars-removed");
                for(const auto& [id,staged]:x.stagedFlows.items()){
                    if(staged.at("implementation_ticket")!=61){continue;}
                    install("f4-"+id,staged,staged.at("steps"));
                    if(id=="meeting-entry"){c.catalog["flows"]["f4-"+id]["trigger"]="autorun";}
                    if(id=="meeting-talk-6-with-shotgun"||id=="meeting-talk-6-without-shotgun"){
                        c.catalog["flows"]["f4-"+id]["owns_item"]=14;c.catalog["flows"]["f4-"+id]["owns"]=id=="meeting-talk-6-with-shotgun";
                    }
                }
                c.facts.switches.emplace(74,"meeting.shotgun-collected");
                for(const auto& [actor,flag]:x.meeting.at("talk_flags").items()){c.facts.switches.emplace(flag.get<int>(),"m24-e"+actor+".talked");}
                if(!x.meetingStory.is_null()){
                    c.facts.switches.emplace(x.meetingStory.at("return").at("completed_flag").get<int>(),"meeting.branch-opened");
                    const auto& returning=x.meetingStory.at("return");
                    c.catalog["flows"]["f4-meeting-return"]={{"object",returning.at("object")},{"page",returning.at("page")},{"trigger","autorun"},{"steps",returning.at("steps")}};
                    for(const auto& refusal:x.meetingStory.at("refusals")){
                        c.catalog["flows"]["f4-refusal-"+refusal.at("object").get<std::string>()]={{"object",refusal.at("object")},{"page",refusal.at("page")},{"trigger","action"},{"steps",refusal.at("steps")}};
                    }
                }


            }
            const auto& legacy=d.at("legacy_source");
            constexpr std::string_view sourceHash="d1ca5ddaa0d29293e0024534da85bc59a88df4638b5b3e45991b38059d68d4b8";
            if(legacy.at("compatibility")!="f3-normal-1"||legacy.at("sha256")!=sourceHash){throw std::runtime_error("Unreviewed F3 source contract");}
            const auto sourceFile=file(c.directory,legacy.at("file"));
            if(std::filesystem::is_symlink(sourceFile)){throw std::runtime_error("Symlink source contract");}
            if(std::filesystem::file_size(sourceFile)>32*1024*1024){throw std::runtime_error("Oversized source contract");}
            std::ifstream source(sourceFile,std::ios::binary);const std::string bytes=readContentBytes(source);
            if(contentSha256(bytes)!=sourceHash){throw std::runtime_error("F3 source contract hash mismatch");}
            c.legacySource=std::make_shared<const CombatContent>(loadCombatContent(sourceFile));
            // Lower audited finite exploration definitions into the shared runtime catalog.
            for(const auto& point:x.savePoints){c.catalog["save_points"].push_back(point);}
            for(const auto& [id,flow]:x.flows.items()){c.catalog["flows"][id]=flow;}
            for(const auto& [id,flow]:x.encounters.items()){c.catalog["flows"][id]=flow;}
            for(const auto& loop:x.environment){
                Json steps=Json::array();for(const auto& cue:loop.at("cue_order")){steps.push_back({{"kind","cue"},{"cue",cue}});}
                c.catalog["flows"][loop.at("object").get<std::string>()+"-environment"]={{"object",loop.at("object")},{"page",loop.at("page")},{"trigger","environment"},{"steps",steps}};
            }
            const auto& blood=x.bloodLoop;Json bloodSteps=Json::array();for(const auto& cue:blood.at("cue_order")){bloodSteps.push_back({{"kind","cue"},{"cue",cue}});}
            c.catalog["flows"][blood.at("object").get<std::string>()+"-blood"]={{"object",blood.at("object")},{"page",blood.at("page")},{"trigger","environment"},{"requires_control",true},{"steps",bloodSteps}};
            for(const auto& [id,map]:c.maps){if(!c.legacySource->maps.contains(id)&&id!=19&&id!=20&&id!=21&&id!=22&&id!=23&&id!=24&&id!=25){c.pendingMapTickets.emplace(id,id==24?61:62);}}
            for(auto& [id,t]:c.transitions.items()){
                if(c.maps.contains(t.at("destination").at("map").get<int>())&&!c.pendingMapTickets.contains(t.at("destination").at("map").get<int>())){t["runtime_enabled"]=true;t["handoff_only"]=false;}
            }
            for(auto& [id,t]:c.transitions.items()){if(c.pendingMapTickets.contains(t.at("destination").at("map").get<int>())){t["runtime_enabled"]=false;t["implementation_ticket"]=c.pendingMapTickets.at(t.at("destination").at("map").get<int>());}}
        }
        if(branchSchema){
            const auto batch=d.at("batch").get<std::string>();
            if(batch!="D"&&batch!="E"&&batch!="F"){throw std::runtime_error("Unknown B/C delivery batch");}
            const std::string weaponCapability=batch=="D"?"state-only":"executable";
            const Json capabilities={{"delivered_batch",batch},{"weapons",{{"14","executable"},{"21","executable"},{"12",weaponCapability},{"27",weaponCapability},{"29",weaponCapability}}}};
            if(d.at("capabilities")!=capabilities){throw std::runtime_error("Unknown B/C weapon capability");}
            const Json sources={{"f3-normal-1",{{"file","f3-source-contract.json"},{"sha256","d1ca5ddaa0d29293e0024534da85bc59a88df4638b5b3e45991b38059d68d4b8"}}},{"f4a-normal-1",{{"file","f4a-source-contract.json"},{"sha256","ee69e4d4c8998f0c6e91758f8b72651d2642f610ab7031e2e7dcd04a09b09b03"}}}};
            if(d.at("source_contracts")!=sources){throw std::runtime_error("Unreviewed B/C source contracts");}
            const auto sourceFile=file(c.directory,"f4a-source-contract.json");
            if(std::filesystem::is_symlink(sourceFile)||std::filesystem::file_size(sourceFile)>32*1024*1024){throw std::runtime_error("Invalid F4-A source contract file");}
            std::ifstream source(sourceFile,std::ios::binary);const std::string bytes=readContentBytes(source);
            if(contentSha256(bytes)!=sources.at("f4a-normal-1").at("sha256").get<std::string>()){throw std::runtime_error("F4-A source contract hash mismatch");}
            c.legacySource=std::make_shared<const CombatContent>(loadCombatContent(sourceFile));
            c.branch=d.at("f4bc");
            const auto& state=c.branch.at("state_contract");
            if(state.at("schema")!="nobihaza.f4bc.state-input.v1"||state.at("target_content_compatibility")!=c.compatibility){throw std::runtime_error("Unknown B/C state contract");}
            // Fixed reviewed semantic bindings; input metadata cannot redefine saved facts.
            const std::map<int,std::string> expectedFacts{
                {87,"power.cable-collected"},
                {88,"dogs.storage-triggered"},
                {89,"dogs.dining-arrival-completed"},
                {90,"dogs.dining-glass-broken"},
                {91,"dogs.shared-dog-jumped"},
                {93,"power.stair-shutter-open"},
                {94,"corridor.ceiling-presence-triggered"},
                {95,"corridor.footsteps-finished"},
                {113,"m30-e3.dead"},
                {114,"m30-e5.dead"},
                {115,"m30-e7.dead"},
                {116,"m30-e9.dead"},
                {117,"m30-e11.dead"},
                {119,"dogs.storage-and-dining-shared.dead"},
                {120,"m39-e2.dead"},
                {121,"m39-e7.dead"},
                {122,"m27-e3.dead"},
                {123,"m27-e5.dead"},
                {124,"m27-e7.dead"},
                {125,"m27-e9.dead"},
                {126,"m31-e6.dead"},
                {127,"m31-e8.dead"},
                {128,"m31-e10.dead"},
                {129,"m39-e4.dead"},
                {130,"m39-e10.dead"},
                {131,"m40-e3.dead"},
                {132,"m39-e14.dead"},
                {133,"m40-e7.dead"},
                {135,"m38-e5.dead"},
                {136,"m38-e2.dead"},
                {286,"m46-e3.collected"},
                {292,"m38-e8.collected"},
                {316,"puzzle.metal-tooth-collected"},
                {487,"m28-e10.collected"},
                {490,"m46-e7.collected"},
                {554,"m40-e13.collected"},
                {569,"m28-e11.collected"},
                {96,"wardrobe.moved-right"},
                {97,"wardrobe.moved-left"},
                {134,"m48-e2.dead"},
                {137,"m44-e2.dead"},
                {138,"m44-e4.dead"},
                {139,"m44-e6.dead"},
                {141,"puzzle.candle-lit"},
                {142,"puzzle.five-elements-matched"},
                {143,"puzzle.five-elements-revealed"},
                {144,"puzzle.spade-collected"},
                {145,"tool.lighter-collected"},
                {147,"training.spade-door-unlocked"},
                {197,"art-room.high-switch-open"},
                {213,"rescue.shared-enemy-dead"},
                {272,"rescue.gian-bsaa-intervened"},
                {273,"rescue.gian-saved-in-time"},
                {274,"rescue.cooperation-established"},
                {336,"rescue.sg-collected"},
                {214,"m81-e2.dead"},
                {242,"m81-e4.dead"},
                {243,"m81-e6.dead"},
                {244,"m83-e2.dead"},
                {245,"m83-e4.dead"},
                {246,"m80-e2.dead"},
                {247,"m80-e4.dead"},
                {248,"m96-e2.dead"},
                {249,"m96-e4.dead"},
                {250,"m96-e6.dead"},
                {255,"m94-e2.dead"},
                {284,"m90-e3.collected"},
                {291,"m94-e3.collected"},
                {300,"m81-e10.collected"},
                {301,"m81-e11.collected"},
                {302,"m81-e12.collected"},
                {314,"puzzle.blue-eye-installed"},
                {318,"puzzle.blue-gem-created"},
                {337,"rescue.suneo-bsaa-intervened"},
                {338,"rescue.suneo-saved-in-time"},
                {339,"rescue.lc9-collected"},
                {358,"m80-e7.collected"},
            };
            std::map<int,std::string> declaredFacts;
            std::set<std::string> names=c.facts.flagIds();
            for(const auto& fact:state.at("facts")){
                const auto key=fact.at("stable_id").get<std::string>();const int id=integer(fact,"source_switch",1,10000);
                if(fact.at("type")!="boolean"||fact.at("initial")!=false||!names.insert(key).second||!c.facts.switches.emplace(id,key).second){throw std::runtime_error("Invalid or duplicate B/C persistent fact");}
            }
            for(const auto& fact:state.at("facts")){declaredFacts.emplace(fact.at("source_switch").get<int>(),fact.at("stable_id").get<std::string>());}
            if(declaredFacts!=expectedFacts){throw std::runtime_error("Incomplete or unreviewed B/C persistent facts");}
            c.requiredMagazines.emplace(12,13);c.requiredMagazines.emplace(27,7);
            c.magazineSeedBindings.emplace(52,12);c.magazineSeedBindings.emplace(63,27);
            c.pendingMapTickets.clear();
            for(const auto& [id,map]:c.maps){if(!c.legacySource->maps.contains(id)){c.pendingMapTickets.emplace(id,72);}}
            for(auto& [id,t]:c.transitions.items()){
                if(c.pendingMapTickets.contains(t.at("destination").at("map").get<int>())){t["runtime_enabled"]=false;t["implementation_ticket"]=72;}
            }
        }
        if(branchSchema){loadBranchExploration(c);loadBranchRescues(c);loadBranchWeapons(c);loadBranchPuzzles(c);loadBranchReturns(c);loadBranchBlue(c);}
        return c;
    }catch(const std::exception& e){throw std::runtime_error(path.string()+": "+e.what());}
}
SceneDefinition CombatContent::makeScene(int map,TilePosition tile,Direction facing,const GameSession& session,const CombatEntry* entry) const{
    if(pendingMapTickets.contains(map)){throw std::runtime_error("Map runtime awaits ticket "+std::to_string(pendingMapTickets.at(map)));}
    auto s=maps.at(map);TerrainMap terrain(s.width,s.height,s.passageMasks);if(!terrain.contains(tile)){throw std::runtime_error("Player outside destination map");}
    s.player.tileX=tile.x;s.player.tileY=tile.y;s.player.direction=std::string(directionName(facing));if(entry){s.player.spriteId=entry->sprite;}
    else if(full->contains("runtime")&&full->at("runtime").contains("player_action_cleanup")){
        const auto& cleanup=full->at("runtime").at("player_action_cleanup").at("ce5");
        if(cleanup.at("actor")==session.actorId()){
            const auto& branch=cleanup.at(session.flag(cleanup.at("injured_flag"))?"injured":"normal");
            s.player.spriteId=cue(branch.at("graphic_cue").get<std::string>()).at("sprite");
        }
    }
    s.controls={true,true,session.flag(1),session.flag(18),false};
    for(const auto& [id,o]:objects){
        if(o.map!=map){continue;}
        const auto* selected=activeVariant(o,session,entry&&entry->pages.contains(id)?std::optional<int>{entry->pages.at(id)}:std::nullopt);
        if(o.variants.empty()){continue;}auto def=selected?selected->definition:o.variants.front().definition;def.active=selected!=nullptr;
        if(entry){
            if(entry->positions.contains(id)){def.tileX=entry->positions.at(id).x;def.tileY=entry->positions.at(id).y;}
            if(entry->through.contains(id)){def.through=entry->through.at(id);}
            if(entry->directions.contains(id)){def.direction=std::string(directionName(entry->directions.at(id)));}
        }
        s.objects.push_back(def);
    }
    // Original event order is numeric; content keys such as e10/e2 are identities,
    // not an update order. Adjacent autonomous actors must vacate in that order.
    std::sort(s.objects.begin(),s.objects.end(),[](const auto& a,const auto& b){return a.sourceEventId<b.sourceEventId;});
    return s;
}

const ObjectVariant* CombatContent::activeVariant(const CombatObject& object,const GameSession& session,std::optional<int> overridePage) const{
    const ObjectVariant* selected=nullptr;
    for(const auto& v:object.variants){
        if(overridePage){if(v.page==*overridePage){return &v;}continue;}
        bool active=true;
        for(const auto& [id,on]:v.flags){active=active&&session.flag(id)==on;}
        if(v.variableMinimum){active=active&&session.variable(v.variableMinimum->first)>=v.variableMinimum->second;}
        if(v.inventoryItem){active=active&&session.count(*v.inventoryItem)>0;}
        if(v.actor){active=active&&session.inParty(*v.actor);}
        if(v.timerMaximumSeconds){active=active&&session.eventTimer().remainingTicks/60<=*v.timerMaximumSeconds;}
        if(active&&(!selected||v.page>selected->page)){selected=&v;}
    }
    return selected;
}
