#pragma once
#include <array>

struct WalkingAnimationDefinition{
    int movingThresholdUpdates{6};
    int continuousThresholdUpdates{8};
    std::array<int,4> logicalFrameColumns{0,1,2,1};
    int spinThresholdUpdates{8};
};

// No keyboard, positions or textures: one 60 Hz update describes the actual action.
class CharacterAnimation{
public:
    CharacterAnimation(WalkingAnimationDefinition definition = {}, int initialColumn = 1,
                       int animationType = 0);
    void update(bool movedThisUpdate, bool paused = false);
    void setPaused(bool value){paused_=value;}
    bool paused() const{return paused_;}
    int column() const;
    bool directionFixed() const;
    int rotation() const{return rotation_;}
    void setSpeedLevel(int speed);
private:
    friend class WarehouseSaveState;
    WalkingAnimationDefinition definition_;
    int phase_{};
    int counter_{};
    int type_{};
    int rotation_{};
    bool paused_{};
};
WalkingAnimationDefinition animationForSpeed(int speed);
