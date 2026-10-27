#include "local_text_fixtures.h"
#include "RoomMap.h"

#include <nlohmann/json.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace{
    using Json = nlohmann::json;
    int checks = 0;

    void require(bool condition, const std::string& message){
        if(!condition){
            throw std::runtime_error(message);
        }
        ++checks;
    }

    struct TemporaryDirectory{
        std::filesystem::path path;

        TemporaryDirectory(){
            const auto id = std::chrono::steady_clock::now().time_since_epoch().count();
            path = std::filesystem::temp_directory_path()
                / ("nobihaza-room-checks-" + std::to_string(id));
            if(!std::filesystem::create_directory(path)){
                throw std::runtime_error("Could not create temporary test directory");
            }
        }

        ~TemporaryDirectory(){
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    };

    Json fixture(){
        return {
            {"schema", "nobihaza.first-room.v1"},
            {"grid", {{"width", 3}, {"height", 3}, {"tile_size", 16},
                {"order", "row-major"}, {"origin", "top-left"}, {"pixel_size", {48, 48}}}},
            {"tiles", {{"passage_masks", std::vector<int>(9, 15)},
                {"direction_bits", {{"down", 1}, {"left", 2}, {"right", 4}, {"up", 8}}}}},
            {"player", {{"spawn_tile", {1, 1}}}},
            {"soldier", {{"tile", {2, 2}}, {"blocks_tile", true}}},
            {"interaction", {{"text", "Inspect target."}}}
        };
    }

    void save(const std::filesystem::path& path, const Json& data){
        std::ofstream file(path);
        file << data.dump();
        if(!file){
            throw std::runtime_error("Could not write test fixture");
        }
    }

    void reject(const std::filesystem::path& path, const Json& data, const char* field){
        save(path, data);
        try{
            const RoomMap room(path);
        }catch(const std::exception& error){
            const std::string message = error.what();
            require(message.find(path.string()) != std::string::npos, "Error must identify file");
            require(message.find(field) != std::string::npos, "Error must identify field");
            return;
        }
        throw std::runtime_error(std::string("Accepted invalid field: ") + field);
    }

    void checkRules(){
        TemporaryDirectory directory;
        const auto path = directory.path / "room.json";
        const auto valid = fixture();
        save(path, valid);
        const RoomMap room(path);
        require(room.width() == 3 && room.height() == 3 && room.tileSize() == 16,
            "Dimensions must come from JSON");
        require(room.interactionText() == "Inspect target.", "Dialogue must come from JSON");
        for(const auto target : {TilePosition{1, 0}, {1, 2}, {0, 1}, {2, 1}}){
            require(room.canMove({1, 1}, target), "Open floor must allow four directions");
        }
        require(!room.canMove({0, 0}, {-1, 0}), "Left boundary");
        require(!room.canMove({0, 0}, {0, -1}), "Top boundary");
        require(!room.canMove({2, 0}, {3, 0}), "Right boundary");
        require(!room.canMove({0, 2}, {0, 3}), "Bottom boundary");
        require(!room.canMove({-1, 0}, {0, 0}), "Invalid origin");
        require(!room.canMove({1, 1}, {2, 0}), "Diagonal step");
        require(!room.canMove({0, 1}, {2, 1}), "Two-cell jump");
        require(!room.canMove({1, 1}, {1, 1}), "Zero-length step");
        require(!room.canMove({2, 1}, {2, 2}), "Soldier blocks passable terrain");

        auto data = valid;
        // Unicode转义使此测试源码不依赖编译器的源字符集设置。
        data["interaction"]["text"] = Json::parse(R"("\u68c0\u67e5\u2026")");
        save(path, data);
        require(RoomMap(path).interactionText() == "\xE6\xA3\x80\xE6\x9F\xA5\xE2\x80\xA6",
            "Reloaded dialogue must preserve UTF-8 characters and punctuation");

        data = valid;
        data["tiles"]["passage_masks"][5] = 0;
        save(path, data);
        require(!RoomMap(path).canMove({1, 1}, {2, 1}), "Destination wall");

        // 两边都不是墙，也必须分别检查正确的方向位。
        data = valid;
        data["tiles"]["passage_masks"][4] = 4;
        data["tiles"]["passage_masks"][5] = 2;
        save(path, data);
        require(RoomMap(path).canMove({1, 1}, {2, 1}), "Matching exit and entry bits");
        data["tiles"]["passage_masks"][5] = 8;
        save(path, data);
        require(!RoomMap(path).canMove({1, 1}, {2, 1}), "Wrong destination entry bit");
        data["tiles"]["passage_masks"][4] = 2;
        data["tiles"]["passage_masks"][5] = 15;
        save(path, data);
        require(!RoomMap(path).canMove({1, 1}, {2, 1}), "Wrong origin exit bit");

        data = valid;
        data["soldier"]["blocks_tile"] = false;
        save(path, data);
        require(RoomMap(path).canMove({2, 1}, {2, 2}), "Disabled object blocking");
        data["soldier"]["tile"] = {0, 1};
        data["soldier"]["blocks_tile"] = true;
        save(path, data);
        const RoomMap movedSoldier(path);
        require(!movedSoldier.canMove({1, 1}, {0, 1})
            && movedSoldier.canMove({2, 1}, {2, 2}), "Object position must come from JSON");

        data = valid;
        data["grid"]["width"] = 4;
        data["grid"]["pixel_size"] = {64, 48};
        data["tiles"]["passage_masks"] = std::vector<int>(12, 15);
        save(path, data);
        require(RoomMap(path).canMove({2, 1}, {3, 1}), "Changed width must change boundary");

        data = valid; data["schema"] = "unknown";
        reject(path, data, "/schema");
        data = valid; data["grid"].erase("width");
        reject(path, data, "/grid/width");
        data = valid; data["grid"]["width"] = 3.5;
        reject(path, data, "/grid/width");
        data = valid; data["grid"]["height"] = 0;
        reject(path, data, "/grid/height");
        data = valid; data["grid"]["width"] = 18446744073709551615ULL;
        reject(path, data, "/grid/width");
        data = valid; data["grid"]["tile_size"] = 32;
        reject(path, data, "/grid/tile_size");
        data = valid; data["grid"]["pixel_size"] = {47, 48};
        reject(path, data, "/grid/pixel_size/0");
        data = valid; data["grid"]["order"] = "column-major";
        reject(path, data, "/grid/order");
        data = valid; data["tiles"]["passage_masks"].erase(0);
        reject(path, data, "/tiles/passage_masks");
        data = valid; data["tiles"]["passage_masks"][0] = 16;
        reject(path, data, "/tiles/passage_masks/0");
        data = valid; data["tiles"]["passage_masks"][0] = -1;
        reject(path, data, "/tiles/passage_masks/0");
        data = valid; data["tiles"]["passage_masks"][0] = 15.0;
        reject(path, data, "/tiles/passage_masks/0");
        data = valid; data["tiles"]["direction_bits"]["right"] = 8;
        reject(path, data, "/tiles/direction_bits/right");
        data = valid; data["soldier"]["blocks_tile"] = 1;
        reject(path, data, "/soldier/blocks_tile");
        data = valid; data["soldier"]["tile"] = {3, 1};
        reject(path, data, "/soldier/tile/0");
        data = valid; data["player"]["spawn_tile"] = {1};
        reject(path, data, "/player/spawn_tile");
        data = valid; data["player"]["spawn_tile"] = {2, 2};
        reject(path, data, "/player/spawn_tile");
        data = valid; data["tiles"]["passage_masks"][4] = 0;
        reject(path, data, "/player/spawn_tile");

        data = valid; data["interaction"].erase("text");
        reject(path, data, "/interaction/text");
        data = valid; data["interaction"]["text"] = 42;
        reject(path, data, "/interaction/text");
        data = valid; data["interaction"]["text"] = "";
        reject(path, data, "/interaction/text");

        { std::ofstream file(path); file << "{broken json"; }
        bool failed = false;
        try{ const RoomMap broken(path); }
        catch(const std::exception& error){
            failed = std::string(error.what()).find(path.string()) != std::string::npos;
        }
        require(failed, "Malformed JSON must report its file");
    }

    void checkFirstRoom(const std::filesystem::path& path){
        const RoomMap room(path);
        require(room.width() == 20 && room.height() == 15, "First-room dimensions");
        require(room.playerSpawn().x == 9 && room.playerSpawn().y == 9, "Spawn position");
        require(room.soldierTile().x == 7 && room.soldierTile().y == 5, "Soldier position");
        require(room.interactionText() == localSourceText("room_map_checks.original"),
            "First-room dialogue must match the prepared Chinese text");
        const TilePosition route[] = {{9, 9}, {9, 8}, {9, 7}, {9, 6}, {8, 6}, {7, 6}};
        for(int i = 1; i < 6; ++i){
            require(room.canMove(route[i - 1], route[i]), "Documented route must be walkable");
        }
        require(room.canMove({9, 9}, {9, 10}), "Floor below spawn");
        require(!room.canMove({6, 5}, {5, 5}), "Documented wall must block");
        require(!room.canMove({7, 6}, {7, 5}), "Soldier must block from below");
        require(!room.canMove({6, 5}, {7, 5}), "Soldier must block from left");
        require(!room.canMove({8, 5}, {7, 5}), "Soldier must block from right");
        require(!room.canMove({9, 0}, {9, -1}), "Documented outer-boundary check");
    }
}

int main(int argc, char** argv){
    try{
        if(argc == 1){
            checkRules();
        }else if(argc == 2){
            checkFirstRoom(argv[1]);
        }else{
            throw std::runtime_error("Usage: room_map_checks [room.json]");
        }
        std::cout << "PASS: " << checks << " checks\n";
        return 0;
    }catch(const std::exception& error){
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
