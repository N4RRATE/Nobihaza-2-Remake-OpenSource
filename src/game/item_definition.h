#pragma once
#include <map>
#include <string>
#include <vector>
struct MedicineDefinition{int hpPercent{},hpPoints{},tpPercent{},tpPoints{};bool koOnly{};std::vector<int> cureStates;};
struct ItemDefinition{
    int id{};
    std::string name;
    std::string description;
    int equipmentType{};
    int attack{};
    int defense{};
    int capacity{};
    int looseItem{};
    int boxItem{};
    int boxRounds{};
    bool fieldUse{};
    std::vector<bool> actorPermissions;
    bool twoHanded{};
    int spirit{},agility{};
    int uses{1};
    MedicineDefinition medicine;
    bool stateOnly{};
    int pendingActionTicket{};
    int boxReloadDebit{};
    bool regularReload{true};
    int magazineMinimum{};
    int overdrawAccessory{};
};
struct MixingRecipe{std::map<int,int> ingredients;int result{},count{};};
using ItemDefinitions = std::map<int,ItemDefinition>;
