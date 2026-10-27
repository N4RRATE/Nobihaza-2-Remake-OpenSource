#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct TilePosition{
    int x{};
    int y{};
};

// 提供首房数据和跨格通行判断；不持有窗口或人物精灵。
class RoomMap{
public:
    explicit RoomMap(const std::filesystem::path& filePath);

    int width() const { return width_; }
    int height() const { return height_; }
    int tileSize() const { return tileSize_; }
    TilePosition playerSpawn() const { return playerSpawn_; }
    TilePosition soldierTile() const { return soldierTile_; }
    std::string interactionText() const { return interactionText_; }

    bool canMove(TilePosition from, TilePosition to) const;

private:
    bool isInside(TilePosition tile) const;
    bool isOccupied(TilePosition tile) const;
    int passageAt(TilePosition tile) const;

    int width_{};
    int height_{};
    int tileSize_{};
    std::vector<int> passageMasks_;
    TilePosition playerSpawn_;
    TilePosition soldierTile_;
    bool soldierBlocks_{};
    std::string interactionText_;
};
