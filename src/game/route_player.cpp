#include "route_player.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
CharacterState findCharacter(const ExplorationScene& world,const std::string& id){
    if(id=="player"){return world.player();}
    for(const auto& a:world.objects()){if(a.id==id){return a;}}
    throw std::runtime_error("Missing route actor: "+id);
}
void replaceCharacter(ExplorationScene& world,const CharacterState& a){if(a.id=="player"){world.setPlayerState(a);}else{world.setObjectState(a);}}
Direction faceToward(TilePosition a,TilePosition b){
    if(std::abs(b.x-a.x)>std::abs(b.y-a.y)){return b.x>a.x?Direction::Right:Direction::Left;}
    return b.y>=a.y?Direction::Down:Direction::Up;
}
RoutePlayer::RoutePlayer(RouteDefinition d,std::string actor,std::string source):definition_(std::move(d)),actor_(std::move(actor)),source_(std::move(source)),frequency_(definition_.frequency){}
const std::string& RoutePlayer::actor() const{return actor_;}
const std::string& RoutePlayer::source() const{return source_;}
void RoutePlayer::refreshDefinition(RouteDefinition definition){
    if(definition.target!=definition_.target||definition.steps.size()!=definition_.steps.size()||
       !std::equal(definition.steps.begin(),definition.steps.end(),definition_.steps.begin(),[](const auto& a,const auto& b){return a.kind==b.kind;})){
        throw std::invalid_argument("Cannot preserve route progress across different commands");
    }
    definition_=std::move(definition);frequency_=definition_.frequency;speed_=0;
}
std::optional<Direction> RoutePlayer::translationDirection() const{
    if(definition_.repeat||definition_.steps.empty()){return {};}
    const auto& kind=definition_.steps.front().kind;
    if(kind!="up"&&kind!="right"&&kind!="down"&&kind!="left"){return {};}
    if(!std::all_of(definition_.steps.begin(),definition_.steps.end(),[&](const auto& step){return step.kind==kind;})){return {};}
    return directionFromName(kind);
}
bool RoutePlayer::tick(ExplorationScene& world,GameSession& session,std::vector<PresentationEvent>& events,const std::function<int(int,int)>& random,const std::function<void(int,bool)>& setSwitch){
    contactedPlayer_=false;auto a=findCharacter(world,actor_);
    if(!speed_){speed_=std::clamp(static_cast<int>(std::log2(a.motion.speed()))-1,1,6);}
    if(a.motion.moving()){return false;}
    if(wait_>0){--wait_;return false;}
    // A completed movement already yielded while it advanced. Restart a repeating
    // route now, without inserting an extra stopped frame at every tile boundary.
    if(cursor_==definition_.steps.size()){firstPassComplete_=true;if(definition_.repeat){cursor_=0;}}
    // All-immediate repeat routes still execute at most one pass per update.
    while(cursor_<definition_.steps.size()){
        const auto& s=definition_.steps[cursor_];const auto& k=s.kind;
        const int moveWait=frequency_==8?0:1<<(9-frequency_);
        const int turnWait=frequency_==8?0:1<<(8-frequency_);
        std::optional<Direction> move;
        if(k=="begin_jump"){
            TilePosition offset{};std::size_t end=cursor_+1;
            for(;end<definition_.steps.size()&&definition_.steps[end].kind!="end_jump";++end){
                const auto& step=definition_.steps[end];
                if(step.kind=="up"||step.kind=="right"||step.kind=="down"||step.kind=="left"){offset=adjacentTile(offset,directionFromName(step.kind));}
                else if(step.kind=="up_left"||step.kind=="up_right"||step.kind=="down_left"||step.kind=="down_right"){
                    offset=adjacentTile(offset,step.kind.starts_with("up")?Direction::Up:Direction::Down);
                    offset=adjacentTile(offset,step.kind.ends_with("left")?Direction::Left:Direction::Right);
                }
                else if(step.kind=="forward"){offset=adjacentTile(offset,a.motion.facing());}
                else if(step.kind=="away_player"||step.kind=="toward_player"){
                    auto direction=faceToward(a.motion.tile(),world.player().motion.tile());
                    if(step.kind=="away_player"){direction=static_cast<Direction>((static_cast<int>(direction)+2)%4);}
                    a.face(direction);offset=adjacentTile(offset,direction);
                }
                // The source jump scanner skips through toggles inside its body.
                else if(step.kind!="through_on"&&step.kind!="through_off"){throw std::runtime_error("Unsupported jump body: "+step.kind);}
            }
            if(end==definition_.steps.size()){throw std::runtime_error("Unclosed route jump");}
            const auto origin=a.motion.tile();
            if(!world.containsTile({origin.x+offset.x,origin.y+offset.y})){
                if(!definition_.skipBlocked){return false;}
                cursor_=end+1;continue;
            }
            a.motion.beginJump(offset);cursor_=end+1;replaceCharacter(world,a);return false;
        }
        if(k=="up"||k=="right"||k=="down"||k=="left"){move=directionFromName(k);}
        if(k=="toward_player"){move=faceToward(a.motion.tile(),world.player().motion.tile());}
        if(k=="random"){move=static_cast<Direction>(random(0,3));}
        if(k=="forward"){move=a.motion.facing();}
        if(k=="away_player"){move=static_cast<Direction>((static_cast<int>(faceToward(a.motion.tile(),world.player().motion.tile()))+2)%4);}
        if(k=="up_left"||k=="up_right"||k=="down_left"||k=="down_right"){
            const auto vertical=k.starts_with("up")?Direction::Up:Direction::Down;
            const auto horizontal=k.ends_with("left")?Direction::Left:Direction::Right;
            if(world.canMoveDiagonal(a,vertical,horizontal)){
                a.motion.beginDiagonal(vertical,horizontal,locked_||a.directionLocked);++cursor_;wait_=moveWait;replaceCharacter(world,a);return false;
            }
            if(!definition_.skipBlocked){return false;}
        }else if(move){
            if(!locked_&&!a.directionLocked){a.motion.face(*move);}
            const auto next=adjacentTile(a.motion.tile(),*move);const int margin=std::min(definition_.outsideMapMargin,a.outsideMapMargin);
            const bool outside=margin>0&&actor_!="player"&&a.through&&world.withinTileMargin(next,margin);
            if(world.canMove(a,*move)||outside){a.motion.beginStep(*move,locked_||a.directionLocked);++cursor_;wait_=moveWait;replaceCharacter(world,a);return false;}
            contactedPlayer_|=actor_!="player"&&adjacentTile(a.motion.tile(),*move)==world.player().motion.tile();
            if(!definition_.skipBlocked){replaceCharacter(world,a);return false;}
        }else if(k.starts_with("face_")){
            // Explicit route turns change the displayed row even when movement facing is fixed.
            const auto d=k=="face_away"?static_cast<Direction>((static_cast<int>(faceToward(a.motion.tile(),world.player().motion.tile()))+2)%4):k=="face_player"?faceToward(a.motion.tile(),world.player().motion.tile()):k=="face_random"?static_cast<Direction>(random(0,3)):directionFromName(k.substr(5));a.face(d);
            wait_=turnWait;
        }else if(k=="turn_left"||k=="turn_right"||k=="turn_around"){
            a.face(static_cast<Direction>((static_cast<int>(a.motion.facing())+(k=="turn_around"?2:k=="turn_right"?1:3))%4));wait_=turnWait;
        }else if(k=="wait"){wait_=20+turnWait;}
        else if(k=="opacity_down"||k=="opacity_up"){a.transparency=std::clamp(a.transparency+(k=="opacity_down"?1:-1),0,7);}
        else if(k=="graphic"){a.spriteId=s.sprite;}
        else if(k=="sound"){events.push_back({PresentationEvent::Kind::Sound,"",s.sound,{},{}});}
        else if(k=="frequency_up"){frequency_=std::min(8,frequency_+1);}
        else if(k=="frequency_down"){frequency_=std::max(1,frequency_-1);}
        else if(k=="speed_up"||k=="speed_down"){
            speed_=std::clamp(speed_+(k=="speed_up"?1:-1),1,6);a.setSpeedLevel(speed_);
        }else if(k=="lock_direction"){locked_=true;a.directionLocked=true;}
        else if(k=="unlock_direction"){locked_=false;a.directionLocked=false;}
        else if(k=="through_on"||k=="through_off"){a.routeThrough=a.through=k=="through_on";}
        else if(k=="start_animation"||k=="stop_animation"){a.animation.setPaused(k=="stop_animation");}
        else if(k=="switch_on"||k=="switch_off"){if(setSwitch){setSwitch(s.switchId,k=="switch_on");}else{session.setFlag(s.switchId,k=="switch_on");}}
        else{throw std::runtime_error("Unsupported active route step: "+k);}
        ++cursor_;replaceCharacter(world,a);
        if(wait_){return false;}
    }
    firstPassComplete_=true;
    if(definition_.repeat){cursor_=0;return false;}
    return true;
}
