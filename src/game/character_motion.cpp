#include "character_motion.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

CharacterMotion::CharacterMotion(TilePosition tile, Direction facing, int subunitsPerUpdate,
                                 int subunitsPerTile)
    : tile_(tile), origin_(tile), facing_(facing), stepSubunits_(subunitsPerUpdate),
      tileSubunits_(subunitsPerTile){
    if(tileSubunits_ <= 0 || stepSubunits_ <= 0 || stepSubunits_ > tileSubunits_){
        throw std::runtime_error("Invalid movement units");
    }
}
TilePosition CharacterMotion::tile() const{ return tile_; }
GridPosition CharacterMotion::position() const{
    const double progress = 1.0 - static_cast<double>(remaining_) / tileSubunits_;
    return {origin_.x + (tile_.x - origin_.x) * progress,
            origin_.y + (tile_.y - origin_.y) * progress};
}
Direction CharacterMotion::facing() const{ return facing_; }
bool CharacterMotion::moving() const{ return remaining_ > 0; }
int CharacterMotion::remainingSubunits() const{ return remaining_; }
void CharacterMotion::face(Direction direction){ if(!moving()){ facing_ = direction; } }
bool CharacterMotion::beginStep(Direction direction,bool preserveFacing){
    if(moving()){ return false; }
    origin_ = tile_;
    tile_ = adjacentTile(tile_,direction);
    if(!preserveFacing){facing_ = direction;}
    remaining_ = tileSubunits_;
    return true;
}
bool CharacterMotion::beginDiagonal(Direction vertical,Direction horizontal,bool preserveFacing){
    if((vertical!=Direction::Up&&vertical!=Direction::Down)||(horizontal!=Direction::Left&&horizontal!=Direction::Right)){throw std::runtime_error("Invalid diagonal axes");}
    if(moving()){return false;}origin_=tile_;tile_=adjacentTile(adjacentTile(tile_,vertical),horizontal);
    if(!preserveFacing){
        if(facing_==static_cast<Direction>((static_cast<int>(vertical)+2)%4)){facing_=vertical;}
        else if(facing_==static_cast<Direction>((static_cast<int>(horizontal)+2)%4)){facing_=horizontal;}
    }
    remaining_=tileSubunits_;return true;
}
bool CharacterMotion::beginJump(TilePosition offset){
    if(moving()){return false;}origin_=tile_;tile_.x+=offset.x;tile_.y+=offset.y;remaining_=tileSubunits_;jumping_=true;return true;
}
double CharacterMotion::jumpHeight() const{if(!jumping_){return 0;}const int h=std::min(remaining_,tileSubunits_-remaining_)/8;return h<5?2*h:h<13?h+4:16;}
void CharacterMotion::advance(){
    static constexpr int jumps[]={8,12,16,24,32,64};const int level=std::clamp(static_cast<int>(std::log2(stepSubunits_))-1,1,6);
    remaining_=std::max(0,remaining_-(jumping_?jumps[level-1]:stepSubunits_));if(!remaining_){jumping_=false;}
}

void CharacterMotion::setSpeed(int speed){
    if(speed<=0||speed>tileSubunits_){throw std::runtime_error("Invalid movement speed");}
    stepSubunits_=speed;
}

int CharacterMotion::speed() const{return stepSubunits_;}
