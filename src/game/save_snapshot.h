#pragma once
#include "combat_scene.h"
#include <array>
#include <string_view>
#include <stdexcept>
struct IncompatibleSave : std::runtime_error{using std::runtime_error::runtime_error;};

// Stable values only. Runtime resources, request identities and generic event stacks never cross this boundary.
struct SavedPresentation{bool operator==(const SavedPresentation&) const=default;
    SoundDefinition music;
    std::array<int,3> tone{90,90,90};
    int saturation{100};
    nlohmann::json warehouseVisual;
};
struct AfterSave{bool operator==(const AfterSave&) const=default;int version{1};std::string point;int countBefore{};int statisticBefore{};std::string kind{"ordinary"};};
struct SavedScene{bool operator==(const SavedScene&) const=default;int map{};TilePosition playerTile;Direction direction{};std::string sprite;std::map<std::string,TilePosition> positions;std::map<std::string,Direction> directions;};
struct SlotSummary{bool operator==(const SlotSummary&) const=default;std::int64_t savedAtUnix{};std::string location;std::string face{"face-actor1"};int faceIndex{15};std::string label;};
struct SaveSnapshot{bool operator==(const SaveSnapshot&) const=default;
    int formatVersion{4};
    std::string contentCompatibility{"f3-normal-1"};
    std::string build{"development"};
    SessionEntry session;
    SavedScene scene;
    SavedPresentation presentation;
    AfterSave continuation;
    SlotSummary summary;
    nlohmann::json warehouse;
};
struct PreparedRestore{
    // Scene borrows session; declaration order guarantees the borrower dies first.
    std::unique_ptr<GameSession> session;
    std::unique_ptr<CombatScene> scene;
    SavedPresentation presentation;
};
SaveSnapshot captureSave(const CombatContent&,const CombatScene&,const SaveRequest&,const SavedPresentation&);
SaveSnapshot captureF5Recovery(const CombatContent&,const CombatScene&,const SavedPresentation&);
void validateSave(const CombatContent&,const SaveSnapshot&);
std::string encodeSave(const CombatContent&,const SaveSnapshot&);
SaveSnapshot decodeSave(const CombatContent&,std::string_view);
void finishSaveContinuation(GameSession&,const SaveSnapshot&);
PreparedRestore prepareRestore(const CombatContent&,const SaveSnapshot&,CombatScene::PreparePresentation={},CombatScene::Random={});
