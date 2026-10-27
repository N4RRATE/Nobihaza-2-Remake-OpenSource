#pragma once
#include "world_geometry.h"

// Grid movement only; callers decide collision and who owns the current action.
class CharacterMotion{
public:
    CharacterMotion(TilePosition tile, Direction facing, int subunitsPerUpdate,
                    int subunitsPerTile = 256);
    TilePosition tile() const;
    GridPosition position() const;
    Direction facing() const;
    bool moving() const;
    int remainingSubunits() const;
    void face(Direction direction);
    bool beginStep(Direction direction,bool preserveFacing=false);
    bool beginDiagonal(Direction vertical,Direction horizontal,bool preserveFacing=false);
    bool beginJump(TilePosition offset);
    bool jumping() const{return jumping_;}
    double jumpHeight() const;
    void advance();
    void setSpeed(int subunitsPerUpdate);
    int speed() const;
private:
    friend class WarehouseSaveState;
    TilePosition tile_;
    TilePosition origin_;
    Direction facing_;
    int stepSubunits_;
    int tileSubunits_;
    int remaining_{};bool jumping_{};
};
