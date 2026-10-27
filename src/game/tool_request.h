#pragma once
#include "world_geometry.h"
#include <cstdint>
#include <string>
// Value request only. The scene owns validation and consumes it once.
struct ToolUseRequest{
    std::uint64_t scene{},request{};
    int item{},page{};
    std::string target;
    TilePosition player,ladder;
    Direction facing{Direction::Up};
    bool operator==(const ToolUseRequest&) const=default;
};
