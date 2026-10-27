#pragma once
#include "game_session.h"
#include "expanded_content.h"
#include "scene_definition.h"
#include <nlohmann/json.hpp>
#include <memory>
struct SoundDefinition{bool operator==(const SoundDefinition&) const=default;std::string resource; int volume{100}; int pitch{100}; int balance{50};};
struct RouteStep{
    std::string kind;
    std::string sprite;
    SoundDefinition sound;
    int switchId{};
};
struct RouteDefinition{
    int target{};
    int frequency{6};
    bool repeat{};
    bool skipBlocked{};
    std::vector<RouteStep> steps;
    int outsideMapMargin{}; // Through routes may enter from a bounded offscreen spawn. Default keeps map bounds.

};
struct CombatMessage{
    MessageDefinition message;
    bool inheritDisplay{};
    bool inheritFace{};
};
struct CombatEntry{
    SessionEntry session;
    int map{};
    TilePosition playerTile;
    Direction direction{};
    std::string sprite;
    std::map<std::string,int> pages;
    std::map<std::string,TilePosition> positions;
    std::map<std::string,bool> through;
    std::map<std::string,Direction> directions;
};
struct ObjectVariant{
    int page{};
    std::map<int,bool> flags;
    std::optional<std::pair<int,int>> variableMinimum;
    std::optional<int> inventoryItem;
    std::optional<int> actor;
    std::optional<int> timerMaximumSeconds;
    SceneObjectDefinition definition;
    RouteDefinition autonomousRoute;
};
struct CombatObject{int map{}; std::string id; TilePosition tile; std::vector<ObjectVariant> variants;};
struct EnemyDefinition{
    std::string id; int map{}; int sourceOrder{}; int deadFlag{}; bool story{};
    nlohmann::json rules;
};
struct CombatContent{
    ExpandedContent expansion;
    std::map<int,int> pendingMapTickets;
    WorldFactDefinitions facts{f3WorldFacts()};
    std::map<int,int> magazineSeedBindings{{51,11},{60,20}};
    std::map<int,SkillDefinition> skills;
    std::map<std::string,EnemyDefinition> enemies;
    ItemDefinitions items;
    std::map<int,SceneDefinition> maps;
    std::map<int,std::vector<int>> terrainIds;
    std::map<std::string,CombatEntry> entries;
    std::map<std::string,CombatObject> objects;
    std::map<std::string,CombatMessage> messages;
    std::map<std::string,RouteDefinition> routes;
    // Reviewed finite media/rule definitions; original event command arrays are never executed.
    nlohmann::json catalog;
    std::string compatibility{"f3-normal-1"};
    std::shared_ptr<const CombatContent> legacySource;
    std::map<int,int> requiredMagazines;
    nlohmann::json branch;
    // Frozen full-stage definitions may be shared by strict historical content versions.
    std::shared_ptr<const nlohmann::json> full=std::make_shared<const nlohmann::json>();
    nlohmann::json enemyBurnSlots=nlohmann::json::object();
    std::optional<nlohmann::json> fatalDeath;
    std::string fullStage;
    nlohmann::json media;
    // Each generation owns its ID table; inherited immutable cue leaves retain their lifetime.
    std::map<std::string,std::shared_ptr<const nlohmann::json>> cues;
    nlohmann::json rules;
    nlohmann::json interactions;
    nlohmann::json transitions;
    nlohmann::json actionProfiles;
    std::filesystem::path directory;
    void setFullDefinition(nlohmann::json definition);
    nlohmann::json resolvedFullDefinition() const;
    const nlohmann::json& enemyBurnSources() const;
    const nlohmann::json& cue(const std::string& id) const;
    bool hasMediaDefinition(const std::string& group,const std::string& id) const;
    void setMediaDefinition(const std::string& group,const std::string& id,const nlohmann::json& definition);
    void replaceMediaDefinitions(const std::string& group,const nlohmann::json& definitions);
    nlohmann::json resolvedMedia() const;
    const ObjectVariant* activeVariant(const CombatObject& object,const GameSession& session,std::optional<int> overridePage=std::nullopt) const;
    SceneDefinition makeScene(int map,TilePosition tile,Direction facing,const GameSession& session,
                              const CombatEntry* entry=nullptr) const;
};
CombatContent loadCombatContent(const std::filesystem::path& path);
SoundDefinition readSound(const nlohmann::json& data);

ItemDefinition readItemDefinition(const nlohmann::json& data);

std::map<std::string,SpriteDefinition> readSceneDefinitions(CombatContent& content,const nlohmann::json& data);
CombatMessage readCombatMessage(const nlohmann::json& data,bool allowVariables=false);
MessageDefinition resolveCombatMessage(const CombatMessage&,const GameSession&);
RouteDefinition readRouteDefinition(const nlohmann::json& data);
void validateSceneContent(const CombatContent& content,const std::map<std::string,SpriteDefinition>& sprites);
bool supportsEnemyBloodSlot(const CombatContent& content,int slot);
