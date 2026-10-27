#pragma once
#include "combat_content.h"
#include <istream>
// Loader internals: strict SAX key checks and DOM construction share one parse.
nlohmann::json parseCombatContentDefinition(std::istream&);
CombatContent loadCombatContentDefinition(const std::filesystem::path&,const nlohmann::json&);
CombatContent loadFullContent(const std::filesystem::path& path,const nlohmann::json& data);
void initializeFullSession(const CombatContent& content,SessionEntry& entry);
void validateFullSession(const CombatContent& content,const SessionEntry& entry,bool clearBoundary=false);
void installUndergroundContent(CombatContent& content,const nlohmann::json& underground);

void installParkingContent(CombatContent&,const nlohmann::json&);
void installSourceSavePoint(CombatContent&,const nlohmann::json&,const nlohmann::json&);

void installTruthContent(CombatContent&,const nlohmann::json&);
void installDrainageContent(CombatContent&,const nlohmann::json&);

void installCrocodileContent(CombatContent&,const nlohmann::json&);
void installPostCrocodileContent(CombatContent&,const nlohmann::json&);

CombatContent loadG0Content(const std::filesystem::path&,const nlohmann::json&);

CombatContent loadG1Content(const std::filesystem::path&,const nlohmann::json&);

void installSourceLighting(CombatContent&,const nlohmann::json&);

CombatContent loadArea2Content(const std::filesystem::path&,const nlohmann::json&);

CombatContent loadArea3Content(const std::filesystem::path&,const nlohmann::json&);

CombatContent loadTowerContent(const std::filesystem::path&,const nlohmann::json&);
const nlohmann::json& continueCheckpoint(const CombatContent&,int capturedMap);

CombatContent loadControlContent(const std::filesystem::path&,const nlohmann::json&);
CombatContent loadAftermathContent(const std::filesystem::path&,const nlohmann::json&);
CombatContent loadTruthContent(const std::filesystem::path&,const nlohmann::json&);
CombatContent loadDoraContent(const std::filesystem::path&,const nlohmann::json&);

CombatContent loadCashContent(const std::filesystem::path&,const nlohmann::json&);

CombatContent loadReturnContent(const std::filesystem::path&,const nlohmann::json&);

void installLowerTileContent(CombatContent&,const nlohmann::json&);

CombatContent loadCockpitDepartureContent(const std::filesystem::path&,const nlohmann::json&);
CombatContent loadFinalContent(const std::filesystem::path&,const nlohmann::json&);

CombatContent loadCargoPlatformContent(const std::filesystem::path&,const nlohmann::json&);
CombatContent loadScoreContent(const std::filesystem::path&,const nlohmann::json&);

CombatContent loadEpilogueContent(const std::filesystem::path&,const nlohmann::json&);

CombatContent loadClearContent(const std::filesystem::path&,const nlohmann::json&);

CombatContent loadSpecialEquipmentContent(const std::filesystem::path&,const nlohmann::json&);

CombatContent loadCycleContent(const std::filesystem::path&,const nlohmann::json&);

CombatContent loadBonusContent(const std::filesystem::path&,const nlohmann::json&);
CombatContent loadBackgroundContent(const std::filesystem::path&,const nlohmann::json&);
CombatContent loadExFontContent(const std::filesystem::path&,const nlohmann::json&);

CombatContent loadDifficultyInitializationContent(const std::filesystem::path&,const nlohmann::json&);
bool isDifficultyPreset(const CombatContent&,const GameSession&,int);

CombatContent loadDifficultyFirstPartContent(const std::filesystem::path&,const nlohmann::json&);
CombatContent loadDifficultyLateContent(const std::filesystem::path&,const nlohmann::json&);
CombatContent loadDifficultyRegionContent(const std::filesystem::path&,const nlohmann::json&);
const nlohmann::json* difficultyEnemyRules(const CombatContent&,const GameSession&,const std::string&);
const nlohmann::json* difficultyControllerRules(const CombatContent&,const GameSession&,const std::string&);
