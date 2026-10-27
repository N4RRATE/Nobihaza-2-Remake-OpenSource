#pragma once

#include <string_view>
#include <vector>

enum class Direction{ Up, Right, Down, Left };

struct TilePosition{
    int x{};
    int y{};
    bool operator==(const TilePosition&) const = default;
};

struct GridPosition{
    double x{};
    double y{};
};

TilePosition adjacentTile(TilePosition tile, Direction direction);
Direction directionFromName(std::string_view name);
std::string_view directionName(Direction direction);

class TerrainMap{
public:
    TerrainMap(int width, int height, std::vector<int> passageMasks);
    bool contains(TilePosition tile) const;
    int passage(TilePosition tile) const;
    void setPassage(TilePosition tile,int mask);
    bool canTraverse(TilePosition from, Direction direction,bool loopX=false) const;
private:
    int width_;
    int height_;
    std::vector<int> passageMasks_;
};
