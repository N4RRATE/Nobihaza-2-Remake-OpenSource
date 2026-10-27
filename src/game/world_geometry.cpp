#include "world_geometry.h"
#include <stdexcept>
#include <utility>

TilePosition adjacentTile(TilePosition tile, Direction direction){
    switch(direction){
    case Direction::Up: --tile.y; break;
    case Direction::Right: ++tile.x; break;
    case Direction::Down: ++tile.y; break;
    case Direction::Left: --tile.x; break;
    }
    return tile;
}
Direction directionFromName(std::string_view name){
    if(name == "up"){ return Direction::Up; }
    if(name == "right"){ return Direction::Right; }
    if(name == "down"){ return Direction::Down; }
    if(name == "left"){ return Direction::Left; }
    throw std::runtime_error("Unknown direction");
}
std::string_view directionName(Direction direction){
    switch(direction){
    case Direction::Up: return "up";
    case Direction::Right: return "right";
    case Direction::Down: return "down";
    case Direction::Left: return "left";
    }
    throw std::runtime_error("Unknown direction");
}
TerrainMap::TerrainMap(int width, int height, std::vector<int> passageMasks)
    : width_(width), height_(height), passageMasks_(std::move(passageMasks)){
    if(width <= 0 || height <= 0 || width > 1024 || height > 1024 ||
       passageMasks_.size() != static_cast<std::size_t>(width) * height){
        throw std::runtime_error("Invalid terrain dimensions or passage count");
    }
    for(int mask : passageMasks_){
        if(mask < 0 || mask > 15){ throw std::runtime_error("Invalid terrain passage mask"); }
    }
}
bool TerrainMap::contains(TilePosition tile) const{
    return tile.x >= 0 && tile.y >= 0 && tile.x < width_ && tile.y < height_;
}
bool TerrainMap::canTraverse(TilePosition from, Direction direction,bool loopX) const{
    if(!contains(from)){ return false; }
    auto to = adjacentTile(from, direction);
    if(loopX){to.x=(to.x%width_+width_)%width_;}
    if(!contains(to)){ return false; }
    int outgoing{};
    int incoming{};
    switch(direction){
    case Direction::Up: outgoing = 8; incoming = 1; break;
    case Direction::Right: outgoing = 4; incoming = 2; break;
    case Direction::Down: outgoing = 1; incoming = 8; break;
    case Direction::Left: outgoing = 2; incoming = 4; break;
    }
    return (passageMasks_[from.y * width_ + from.x] & outgoing) != 0 &&
           (passageMasks_[to.y * width_ + to.x] & incoming) != 0;
}

void TerrainMap::setPassage(TilePosition tile,int mask){
    if(!contains(tile)||mask<0||mask>15){throw std::runtime_error("Invalid replacement tile passage");}
    passageMasks_.at(tile.y*width_+tile.x)=mask;
}

int TerrainMap::passage(TilePosition tile) const{
    if(!contains(tile)){throw std::runtime_error("Passage query outside terrain");}
    return passageMasks_.at(tile.y*width_+tile.x);
}
