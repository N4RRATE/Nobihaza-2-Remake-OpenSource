#include "RoomMap.h"

#include <nlohmann/json.hpp>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>

namespace{
    using Json = nlohmann::json;

    const Json& field(const Json& data, const std::string& path){
        try{
            return data.at(Json::json_pointer(path));
        }catch(const Json::exception& error){
            throw std::runtime_error(path + ": " + error.what());
        }
    }

    int integer(const Json& value, const std::string& path, int minimum, int maximum){
        if(!value.is_number_integer()){
            throw std::runtime_error(path + ": expected an integer");
        }

        // 先检查原始整数范围，避免大数转换成int时被截断。
        if(value.is_number_unsigned()){
            const auto number = value.get<std::uint64_t>();
            if(number > static_cast<std::uint64_t>(maximum)){
                throw std::runtime_error(path + ": integer out of range");
            }
        }else{
            const auto number = value.get<std::int64_t>();
            if(number < minimum || number > maximum){
                throw std::runtime_error(path + ": integer out of range");
            }
        }

        const int result = value.get<int>();
        if(result < minimum){
            throw std::runtime_error(path + ": integer out of range");
        }
        return result;
    }

    void expectText(const Json& data, const std::string& path, const char* expected){
        const auto& value = field(data, path);
        if(!value.is_string() || value != expected){
            throw std::runtime_error(path + ": expected " + expected);
        }
    }

    TilePosition tilePosition(const Json& data, const std::string& path, int width, int height){
        const auto& value = field(data, path);
        if(!value.is_array() || value.size() != 2){
            throw std::runtime_error(path + ": expected [x, y]");
        }
        return {
            integer(value.at(0), path + "/0", 0, width - 1),
            integer(value.at(1), path + "/1", 0, height - 1)
        };
    }
}

RoomMap::RoomMap(const std::filesystem::path& filePath){
    try{
        std::ifstream file(filePath);
        if(!file){
            throw std::runtime_error("cannot open file");
        }
        const Json data = Json::parse(file);
        expectText(data, "/schema", "nobihaza.first-room.v1");
        expectText(data, "/grid/order", "row-major");
        expectText(data, "/grid/origin", "top-left");

        // 本读取器的尺寸上限用于约束索引和内存；实际宽高仍来自JSON。
        width_ = integer(field(data, "/grid/width"), "/grid/width", 1, 1024);
        height_ = integer(field(data, "/grid/height"), "/grid/height", 1, 1024);
        tileSize_ = integer(field(data, "/grid/tile_size"), "/grid/tile_size", 16, 16);

        const auto& pixelSize = field(data, "/grid/pixel_size");
        if(!pixelSize.is_array() || pixelSize.size() != 2){
            throw std::runtime_error("/grid/pixel_size: expected [width, height]");
        }
        integer(pixelSize.at(0), "/grid/pixel_size/0", width_ * tileSize_, width_ * tileSize_);
        integer(pixelSize.at(1), "/grid/pixel_size/1", height_ * tileSize_, height_ * tileSize_);

        integer(field(data, "/tiles/direction_bits/down"), "/tiles/direction_bits/down", 1, 1);
        integer(field(data, "/tiles/direction_bits/left"), "/tiles/direction_bits/left", 2, 2);
        integer(field(data, "/tiles/direction_bits/right"), "/tiles/direction_bits/right", 4, 4);
        integer(field(data, "/tiles/direction_bits/up"), "/tiles/direction_bits/up", 8, 8);

        const auto& masks = field(data, "/tiles/passage_masks");
        const auto cellCount = static_cast<std::size_t>(width_) * height_;
        if(!masks.is_array() || masks.size() != cellCount){
            throw std::runtime_error("/tiles/passage_masks: length must equal width * height");
        }
        passageMasks_.reserve(cellCount);
        for(std::size_t i = 0; i < cellCount; ++i){
            passageMasks_.push_back(integer(masks.at(i),
                "/tiles/passage_masks/" + std::to_string(i), 0, 15));
        }

        playerSpawn_ = tilePosition(data, "/player/spawn_tile", width_, height_);
        soldierTile_ = tilePosition(data, "/soldier/tile", width_, height_);
        const auto& blocks = field(data, "/soldier/blocks_tile");
        if(!blocks.is_boolean()){
            throw std::runtime_error("/soldier/blocks_tile: expected a boolean");
        }
        soldierBlocks_ = blocks.get<bool>();

        const auto& text = field(data, "/interaction/text");
        if(!text.is_string()){
            throw std::runtime_error("/interaction/text: expected a string");
        }

        interactionText_ = text.get<std::string>();
        if(interactionText_.empty()){
            throw std::runtime_error("/interaction/text: text must not be empty");
        }

        if(passageAt(playerSpawn_) == 0 || isOccupied(playerSpawn_)){
            throw std::runtime_error("/player/spawn_tile: spawn is blocked");
        }
    }catch(const std::exception& error){
        throw std::runtime_error(filePath.string() + ": " + error.what());
    }
}

bool RoomMap::isInside(TilePosition tile) const{
    return tile.x >= 0 && tile.x < width_
        && tile.y >= 0 && tile.y < height_;
}

bool RoomMap::isOccupied(TilePosition tile) const{
    return soldierBlocks_ && tile.x == soldierTile_.x && tile.y == soldierTile_.y;
}

int RoomMap::passageAt(TilePosition tile) const{
    return passageMasks_[static_cast<std::size_t>(tile.y) * width_ + tile.x];
}

bool RoomMap::canMove(TilePosition from, TilePosition to) const{
    // 必须先验证范围，随后才能用格坐标访问数组。
    if(!isInside(from) || !isInside(to) || isOccupied(from) || isOccupied(to)){
        return false;
    }

    const int dx = to.x - from.x;
    const int dy = to.y - from.y;
    int exitBit = 0;
    int entryBit = 0;

    if(dx == 0 && dy == -1){
        exitBit = 8;
        entryBit = 1;
    }else if(dx == 0 && dy == 1){
        exitBit = 1;
        entryBit = 8;
    }else if(dx == -1 && dy == 0){
        exitBit = 2;
        entryBit = 4;
    }else if(dx == 1 && dy == 0){
        exitBit = 4;
        entryBit = 2;
    }else{
        return false;
    }

    // 起点允许出去，终点也必须允许从相反方向进入。
    return (passageAt(from) & exitBit) != 0
        && (passageAt(to) & entryBit) != 0;
}
