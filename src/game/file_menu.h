#pragma once
#include "save_files.h"
struct FileMenuView{std::string title;std::vector<std::string> lines;int selection{};int first{};std::string prompt;std::vector<std::optional<SlotSummary>> summaries;bool confirmation{};int answer{1};};
struct FileMenuResult{enum class Kind{Saved,Loaded,Cancelled};Kind kind;std::optional<SaveSnapshot> snapshot;};
class FileMenu{
public:
    FileMenu(SaveFiles&,std::optional<SaveSnapshot> save={});
    void update(const InputFrame&);
    void showError(std::string message){message_=std::move(message);phase_=Phase::Error;result_.reset();}
    FileMenuView view() const;
    std::optional<FileMenuResult> takeResult();
    std::string takeSound();
private:
    void select();void write();void refresh();
    enum class Phase{Browse,Overwrite,Recovery,ConfirmRecovery,Error,Finished};
    SaveFiles& files_;std::optional<SaveSnapshot> save_;std::vector<SlotInfo> slots_;std::vector<RecoveryChoice> recovery_;
    Phase phase_{Phase::Browse};int selected_{},recoverySelection_{},answer_{1};std::string message_,sound_;
    std::optional<FileMenuResult> result_;
};
