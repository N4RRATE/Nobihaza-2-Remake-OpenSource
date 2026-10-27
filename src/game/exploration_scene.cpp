#include "exploration_scene.h"
#include <stdexcept>

ExplorationScene::ExplorationScene(const SceneDefinition& definition)
    : messages_(definition.messages), terrain_(definition.width,definition.height,definition.passageMasks),upperTileReplacements_(definition.upperTileReplacements),
      player_{"player",0,
              CharacterMotion({definition.player.tileX,definition.player.tileY},
                              directionFromName(definition.player.direction),
                              definition.player.subunitsPerUpdate,definition.player.subunitsPerTile),
              definition.player.spriteId,CharacterAnimation(definition.walkingAnimation,definition.player.initialColumn),1,true,false,false,0,{}},
      controls_(definition.controls),loopX_(definition.loopX),width_(definition.width),height_(definition.height){
    lowerTiles_=definition.lowerTiles;for(int i=0;i<144;++i){lowerTileMapping_[i]=i;}
    if(lowerTiles_){
        if(lowerTiles_->passageFlags.size()!=162||lowerTiles_->terrainIds.size()!=162){throw std::runtime_error("Invalid lower passage table");}
        for(const auto& cell:lowerTiles_->cells){if(!terrain_.contains({cell.tile[0],cell.tile[1]})||cell.index<0||cell.index>=144||cell.upperPassage<0||cell.upperPassage>63){throw std::runtime_error("Invalid lower cell");}}
        for(const auto& edge:lowerTiles_->allowed){if(edge[0]<0||edge[0]>=144||edge[1]<0||edge[1]>=144||!lowerTiles_->images.contains(edge[1])){throw std::runtime_error("Invalid finite lower replacement");}}
    }
    for(const auto& [id,graphic] : *definition.sprites){ graphicIds_.insert(id); }
    for(const auto& object : definition.objects){
        if(object.transparency<0||object.transparency>7||object.outsideMapMargin<0||object.outsideMapMargin>20){throw std::runtime_error("Invalid source transparency");}
        objects_.push_back({object.id,object.sourceEventId,
            CharacterMotion({object.tileX,object.tileY},directionFromName(object.direction),
                            1 << (1 + object.moveSpeed)),
            object.spriteId,CharacterAnimation(animationForSpeed(object.moveSpeed),object.initialColumn,object.animationType),object.layer,object.active,object.through,object.overlapForbidden,object.trigger,object.interaction,directionFromName(object.direction),object.renderTiePriority,{},object.transparency,object.tilePassageFlags,object.outsideMapMargin});
    }
}
const CharacterState& ExplorationScene::player() const{ return player_; }
const std::vector<CharacterState>& ExplorationScene::objects() const{ return objects_; }
ControlPermissions ExplorationScene::controls() const{
    auto permissions = controls_;
    if(investigation_){
        permissions.ordinaryMovement = false; permissions.ordinaryAction = false;
        permissions.customMenu = false; permissions.combat = false; permissions.defaultShortcutMenu = false;
    }
    return permissions;
}
const Dialogue& ExplorationScene::dialogue() const{ return dialogue_; }
const MessageDisplay& ExplorationScene::messageOptions() const{ return messageOptions_; }
std::optional<std::string> ExplorationScene::investigationTarget() const{
    if(!controls().ordinaryAction || player_.motion.moving()){return {};}
    const auto tile = player_.motion.tile();
    const auto front = adjacentTile(tile,player_.motion.facing());
    // Same-tile Action events are checked before adjacent events, then by source event ID.
    for(auto geometry : {InteractionGeometry::SameTile,InteractionGeometry::AdjacentFacing}){
        const CharacterState* selected = nullptr;
        for(const auto& object : objects_){
            if(!object.active || object.trigger != 0 || object.interaction.kind != InteractionKind::Message ||
               object.interaction.geometry != geometry){continue;}
            const bool same = geometry == InteractionGeometry::SameTile;
            if((same ? object.layer == 1 : object.layer != 1) ||
               object.motion.tile() != (same ? tile : front)){continue;}
            if(!selected || object.eventId < selected->eventId){selected = &object;}
        }
        if(selected){return selected->id;}
    }
    return {};
}
bool ExplorationScene::canPlayerMove(Direction direction) const{return canMove(player_,direction);}
bool ExplorationScene::canMove(const CharacterState& actor,Direction direction) const{
    const auto destination=normalize(adjacentTile(actor.motion.tile(),direction));
    if(!terrain_.contains(destination)){return false;}
    if(actor.through){return true;}
    const auto source=normalize(actor.motion.tile());
    const auto passage=[&](TilePosition tile,int bit){
        const CharacterState* top=nullptr;
        for(const auto& object:objects_){if(object.active&&!object.through&&object.layer==0&&object.tilePassageFlags&&normalize(object.motion.tile())==tile&&(!top||object.eventId>top->eventId)){top=&object;}}
        if(top&& !(*top->tilePassageFlags&16)){return (*top->tilePassageFlags&bit)!=0;}
        return (terrain_.passage(tile)&bit)!=0;
    };
    const int outgoing=direction==Direction::Up?8:direction==Direction::Right?4:direction==Direction::Down?1:2;
    const int incoming=direction==Direction::Up?1:direction==Direction::Right?2:direction==Direction::Down?8:4;
    if(!passage(source,outgoing)||!passage(destination,incoming)){return false;}
    if(actor.id!=player_.id&&actor.layer==1&&!player_.through&&destination==normalize(player_.motion.tile())){return false;}
    for(const auto& object:objects_){
        if(object.id==actor.id||!object.active||object.through||normalize(object.motion.tile())!=destination){continue;}
        if(actor.id==player_.id){if(object.layer==1){return false;}}
        else if(object.layer==actor.layer||actor.overlapForbidden||object.overlapForbidden){return false;}
    }
    return true;
}
bool ExplorationScene::canMoveDiagonal(const CharacterState& actor,Direction vertical,Direction horizontal) const{
    const auto via=[&](Direction first,Direction second){
        if(!canMove(actor,first)){return false;}auto intermediate=actor;
        intermediate.motion=CharacterMotion(adjacentTile(actor.motion.tile(),first),actor.motion.facing(),actor.motion.speed());
        return canMove(intermediate,second);
    };
    return via(vertical,horizontal)||via(horizontal,vertical);
}
void ExplorationScene::setPlayerState(const CharacterState& player){
    if(player.transparency<0||player.transparency>7||player.id!=player_.id||!terrain_.contains(normalize(player.motion.tile()))||
       (player.spriteId&&!graphicIds_.contains(*player.spriteId))){throw std::runtime_error("Invalid player state");}
    player_=player;
}
void ExplorationScene::update(const InputFrame& input){
    advanceClock();
    const bool confirm = input.buttons[static_cast<std::size_t>(InputAction::Confirm)].pressed;
    const bool cancel = input.buttons[static_cast<std::size_t>(InputAction::Cancel)].pressed;
    if(investigation_){
        dialogue_.update(confirm,cancel);
        if(dialogue_.view().phase == DialoguePhase::Closing || dialogue_.view().phase == DialoguePhase::Cooldown){
            messageOptions_ = investigation_->restore;
        }
        if(const auto completed = dialogue_.takeCompletion()){
            if(completed->request == investigation_->request){investigation_.reset();}
        }
        advanceCharacters();
        return; // The closing confirmation never becomes a second action or ordinary move.
    }
    if(!player_.motion.moving() && controls_.ordinaryMovement && input.movement){
        player_.motion.face(*input.movement);
        if(canPlayerMove(*input.movement)){ player_.motion.beginStep(*input.movement); }
    }
    if(confirm){
        if(const auto target = investigationTarget()){
            for(const auto& object : objects_){
                if(object.id != *target){continue;}
                const auto& message = messages_.at(object.interaction.messageId);
                investigation_ = Investigation{dialogue_.request(message),message.restoreDisplay};
                messageOptions_ = message.display;
                advanceCharacters(); return;
            }
        }
    }
    advanceCharacters();
}
void ExplorationScene::advanceCharacters(){
    const bool playerJumped = player_.motion.jumping();
    const bool playerMoved = player_.motion.moving()&&!playerJumped;
    player_.motion.advance();
    player_.animation.update(playerMoved,playerJumped);
    for(auto& object : objects_){
        if(object.active){
            const bool jumped = object.motion.jumping();
            const bool moved = object.motion.moving()&&!jumped;
            object.motion.advance(); object.animation.update(moved,jumped);
        }
    }
}
void ExplorationScene::setControls(ControlPermissions controls){ controls_ = controls; }
bool ExplorationScene::containsTile(TilePosition tile) const{return terrain_.contains(normalize(tile));}
void ExplorationScene::setObjectState(const CharacterState& object){
    if((object.tilePassageFlags&&(*object.tilePassageFlags<0||*object.tilePassageFlags>63))||object.transparency<0||object.transparency>7||object.outsideMapMargin<0||object.outsideMapMargin>20||!withinTileMargin(normalize(object.motion.tile()),object.outsideMapMargin) || object.layer < 0 || object.layer > 2 ||
       object.animation.column() < 0 || object.animation.column() > 2 ||
       (object.spriteId && !graphicIds_.contains(*object.spriteId)) ||
       (object.interaction.kind == InteractionKind::Message && !messages_.contains(object.interaction.messageId))){
        throw std::runtime_error("Invalid object state: " + object.id);
    }
    for(auto& existing : objects_){
        if(existing.id == object.id){
            if(existing.eventId != object.eventId||existing.outsideMapMargin!=object.outsideMapMargin){ throw std::runtime_error("Cannot change object identity"); }
            existing = object;
            return;
        }
    }
    throw std::runtime_error("Unknown object ID: " + object.id);
}

TilePosition ExplorationScene::normalize(TilePosition p) const{if(loopX_){p.x=(p.x%width_+width_)%width_;}return p;}

void ExplorationScene::addTransientActor(CharacterState actor){
    if(actor.id.empty()||actor.id=="player"||std::any_of(objects_.begin(),objects_.end(),[&](const auto& a){return a.id==actor.id;})){throw std::runtime_error("Duplicate transient actor");}
    // Use the ordinary state validator before publishing this temporary actor.
    objects_.push_back(actor);
    try{setObjectState(actor);}catch(...){objects_.pop_back();throw;}
}

void ExplorationScene::substituteUpperTile(int from,int to){
    const auto found=std::find_if(upperTileReplacements_.begin(),upperTileReplacements_.end(),[&](const auto& r){return r.fromIndex==from&&r.toIndex==to;});
    if(found==upperTileReplacements_.end()||found->tiles.size()!=found->passageMasks.size()){throw std::runtime_error("Invalid finite upper replacement");}
    for(std::size_t n=0;n<found->tiles.size();++n){const auto& tile=found->tiles[n];terrain_.setPassage({tile[0],tile[1]},found->passageMasks[n]);}
    substitutedUpperTiles_.insert(from);
}

void ExplorationScene::substituteLowerTile(int from,int to){
    if(!lowerTiles_||from<0||from>=144||to<0||to>=144||std::find(lowerTiles_->allowed.begin(),lowerTiles_->allowed.end(),std::array<int,2>{from,to})==lowerTiles_->allowed.end()){throw std::runtime_error("Invalid finite lower replacement");}
    for(auto& value:lowerTileMapping_){if(value==from){value=to;}}
    for(const auto& cell:lowerTiles_->cells){
        const int lower=lowerTiles_->passageFlags.at(lowerTileMapping_[cell.index]+18),upper=cell.upperPassage;
        terrain_.setPassage({cell.tile[0],cell.tile[1]},upper&16?(lower&upper&15):upper&15);
    }
}

std::optional<int> ExplorationScene::mappedLowerTerrain(TilePosition tile) const{
    if(lowerTiles_){for(const auto& cell:lowerTiles_->cells){if(cell.tile==std::array<int,2>{tile.x,tile.y}){return lowerTiles_->terrainIds.at(lowerTileMapping_[cell.index]+18);}}}
    return std::nullopt;
}

bool ExplorationScene::withinTileMargin(TilePosition tile,int margin) const{return margin>=0&&tile.x>=-margin&&tile.y>=-margin&&tile.x<width_+margin&&tile.y<height_+margin;}
