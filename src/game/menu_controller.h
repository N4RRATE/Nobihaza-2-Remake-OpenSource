#pragma once
#include "combat_content.h"
#include "input_buffer.h"
#include "tool_request.h"
#include "input_prompt.h"
enum class MenuPage{Root,Items,Skills,Slots,Equipment,Target,TitleConfirm,Notice};
struct MenuRow{int id{};std::string text;bool enabled{true};};
struct MenuView{
    MenuPage page{MenuPage::Root};std::string title,description,notice;
    std::optional<EquipmentStats> currentStats,previewStats;
    std::vector<MenuRow> equipmentSlots;
    std::vector<MenuRow> rows;int selection{};int columns{1};
};
struct MenuResult{enum class Kind{Close,Title,Read,Tool,Mix};Kind kind;int item{};std::optional<ToolUseRequest> tool;std::optional<PromptIdentity> mix;};
// Owns navigation only. Definitions and current quantities are read, never copied into UI state.
class MenuController{
public:
    MenuController(const CombatContent& content,GameSession& session,std::optional<ToolUseRequest> tool={},std::optional<PromptIdentity> mix={});
    MenuController(const CombatContent&,GameSession&,std::vector<ToolUseRequest>,std::optional<PromptIdentity>);
    void update(const InputFrame& input);
    MenuView view() const;
    std::optional<MenuResult> takeResult();
    std::string takeSound();
    std::optional<SoundDefinition> takeActionSound();
    void returnFromReading();
    void refreshTools(std::vector<ToolUseRequest> tools){tools_=std::move(tools);}
private:
    void enter(MenuPage page,int selection=0);
    void notice(std::string text);
    const CombatContent& content_;GameSession& session_;
    MenuPage page_{MenuPage::Root},previous_{MenuPage::Root};
    int selection_{},slot_{1},item_{},listSelection_{};
    std::string notice_,sound_;
    std::optional<SoundDefinition> actionSound_;
    bool suppressConfirm_{};
    std::optional<MenuResult> result_;
    std::vector<ToolUseRequest> tools_;
    std::optional<PromptIdentity> mix_;
};
