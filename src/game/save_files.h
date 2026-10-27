#pragma once
#include "save_snapshot.h"
#include <functional>

enum class SlotState{Empty,Readable,Corrupt,Incompatible,RecoveryRequired};
enum class WriteOutcome{Committed,Failed,RecoveryRequired};
struct WriteResult{WriteOutcome outcome;std::string message;};
struct SlotInfo{int slot;SlotState state;std::optional<SaveSnapshot> snapshot;std::string message;};
struct RecoveryChoice{std::string source;std::string bytes;SaveSnapshot snapshot;};
std::filesystem::path defaultSaveDirectory();
// File transactions never mutate the active game. Directory injection is also used by isolated checks.
class SaveFiles{
public:
    using Fault=std::function<void(std::string_view)>;
    SaveFiles(const CombatContent&,std::filesystem::path directory=defaultSaveDirectory(),Fault fault={});
    std::vector<SlotInfo> list() const;
    bool hasArtifacts() const;
    SaveSnapshot read(int slot) const;
    WriteResult write(int slot,const SaveSnapshot&);
    std::vector<RecoveryChoice> recoveryChoices(int slot) const;
    WriteResult recover(int slot,const RecoveryChoice& confirmed);
    const std::filesystem::path& directory() const{return directory_;}
private:
    std::filesystem::path path(int slot,const std::string& suffix=".json") const;
    SlotInfo inspect(int slot) const;
    void protect(int slot) const;
    void fault(std::string_view stage) const;
    const CombatContent& content_;
    std::filesystem::path directory_;
    Fault fault_;
};
