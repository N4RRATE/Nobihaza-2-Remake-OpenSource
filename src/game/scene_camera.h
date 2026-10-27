#pragma once
#include "world_geometry.h"
// Scene-local viewport. Pan advances the actual clipped view; its scheduled wait
// is independent of hitting a map edge (fixed Player compatibility semantics).
class SceneCamera{
public:
    void reset(GridPosition player,int width,int height,bool loopX=false);
    void tick(GridPosition player);
    void lock(bool value);
    int pan(Direction direction,int tiles,int speed);
    int restore(int speed);
    bool moving() const;
    GridPosition center() const{return center_;}
private:
    friend class WarehouseSaveState;
    int schedule(int speed);
    GridPosition clamp(GridPosition value) const;
    GridPosition player_,center_,offset_,target_;
    bool loopX_{};
    int width_{20},height_{15};double step_{8};bool locked_{};
};
