#include "character_animation.h"
#include <stdexcept>

CharacterAnimation::CharacterAnimation(WalkingAnimationDefinition definition,int initialColumn,int animationType)
    : definition_(definition), phase_(initialColumn), type_(animationType){
    if(initialColumn < 0 || initialColumn > 2 || animationType < 0 || animationType > 5 ||
       definition.spinThresholdUpdates < 1 || definition.movingThresholdUpdates < 1 || definition.continuousThresholdUpdates < definition.movingThresholdUpdates ||
       definition.logicalFrameColumns != std::array<int,4>{0,1,2,1}){
        throw std::runtime_error("Invalid or unsupported character animation definition");
    }
}
void CharacterAnimation::update(bool movedThisUpdate,bool paused){
    if(type_ == 5){if(++counter_>=definition_.spinThresholdUpdates){rotation_=(rotation_+1)%4;counter_=0;}return;}
    if(paused_ || paused){ if(type_ != 4){phase_ = 1;} counter_ = 0; return; }
    if(type_ == 4){ return; }
    const bool continuous = type_ == 1 || type_ == 3;
    if(continuous || movedThisUpdate || phase_ == 0 || phase_ == 2 ||
       counter_ < definition_.movingThresholdUpdates - 1){ ++counter_; }
    if(counter_ >= definition_.continuousThresholdUpdates ||
       (movedThisUpdate && counter_ >= definition_.movingThresholdUpdates)){
        phase_ = (phase_ + 1) % 4; counter_ = 0;
    }
}
bool CharacterAnimation::directionFixed() const{ return type_ == 2 || type_ == 3 || type_ == 4 || type_ == 5; }
int CharacterAnimation::column() const{ return definition_.logicalFrameColumns.at(phase_); }
WalkingAnimationDefinition animationForSpeed(int speed){
    // Pinned compatible-engine thresholds; see exploration-rules-audit EX05.
    constexpr std::array<int,6> moving{12,10,8,6,5,4};
    constexpr std::array<int,6> continuous{16,12,10,8,7,6};
    if(speed < 1 || speed > 6){ throw std::runtime_error("Animation speed must be 1..6"); }
    constexpr std::array<int,6> spin{24,16,12,8,6,4};
    return {moving[speed-1],continuous[speed-1],{0,1,2,1},spin[speed-1]};
}

void CharacterAnimation::setSpeedLevel(int speed){definition_=animationForSpeed(speed);}
