#include "save_files.h"
#include "save_platform.h"
#include <iomanip>
#include <sstream>
#include <set>
namespace fs=std::filesystem;
namespace{
    bool present(const fs::path& p){return fs::symlink_status(p).type()!=fs::file_type::not_found;}
    const std::vector<std::string> residues{".pending.json",".transaction.json",".backup.pending.json",".recover.pending.json"};
}
SaveFiles::SaveFiles(const CombatContent& c,fs::path directory,Fault fault):content_(c),directory_(fs::absolute(std::move(directory)).lexically_normal()),fault_(std::move(fault)){}
fs::path SaveFiles::path(int slot,const std::string& suffix) const{
    if(slot<1||slot>15){throw std::runtime_error("Save slot must be 1..15");}std::ostringstream name;name<<"slot-"<<std::setw(2)<<std::setfill('0')<<slot<<suffix;return directory_/name.str();
}
void SaveFiles::fault(std::string_view stage) const{if(fault_){fault_(stage);}}
void SaveFiles::protect(int slot) const{
    const auto root=fs::symlink_status(directory_);if(root.type()==fs::file_type::symlink){throw std::runtime_error("Save directory symlink refused");}
    if(root.type()==fs::file_type::not_found){return;}if(!fs::is_directory(root)){throw std::runtime_error("Save directory is not a directory");}
    const std::string prefix=path(slot,"").filename().string();std::set<std::string> allowed{prefix+".json",prefix+".backup.json"};for(const auto& suffix:residues){allowed.insert(prefix+suffix);}
    for(const auto& e:fs::directory_iterator(directory_)){const auto name=e.path().filename().string();if(!name.starts_with(prefix)){continue;}
        if(!allowed.contains(name)||!fs::is_regular_file(e.symlink_status())){throw std::runtime_error("Unknown or nonregular slot artifact protected: "+name);}
    }
}
SlotInfo SaveFiles::inspect(int slot) const{
    SlotInfo info{slot,SlotState::Empty,{},""};
    try{protect(slot);for(const auto& suffix:residues){if(present(path(slot,suffix))){info.state=SlotState::RecoveryRequired;info.message="发现未完成写入，请选择恢复版本";return info;}}
        if(!present(path(slot))){if(present(path(slot,".backup.json"))){info.state=SlotState::RecoveryRequired;info.message="正式档缺失，有备份可恢复";}return info;}info.snapshot=decodeSave(content_,save_platform::read(path(slot)));info.state=SlotState::Readable;
    }catch(const IncompatibleSave& e){info.state=SlotState::Incompatible;info.message=e.what();}catch(const std::exception& e){info.state=SlotState::Corrupt;info.message=e.what();}return info;
}
std::vector<SlotInfo> SaveFiles::list() const{std::vector<SlotInfo> out;for(int i=1;i<=15;++i){out.push_back(inspect(i));}return out;}
bool SaveFiles::hasArtifacts() const{
    std::error_code error;const auto root=fs::symlink_status(directory_,error);
    if(root.type()==fs::file_type::not_found){return false;}
    // Keep invalid roots and every slot artifact reachable through the protected Load diagnostics.
    if(error||!fs::is_directory(root)){return true;}
    fs::directory_iterator entries(directory_,error);if(error){return true;}
    for(;entries!=fs::directory_iterator{};entries.increment(error)){
        if(error){return true;}
        for(int slot=1;slot<=15;++slot){if(entries->path().filename().string().starts_with(path(slot,"").filename().string())){return true;}}
    }
    if(error){return true;}
    return false;
}
SaveSnapshot SaveFiles::read(int slot) const{const auto info=inspect(slot);if(info.state!=SlotState::Readable){throw std::runtime_error(info.message.empty()?"该槽位没有可读存档":info.message);}return *info.snapshot;}
WriteResult SaveFiles::write(int slot,const SaveSnapshot& snapshot){
    bool started=false;
    try{
        const auto bytes=encodeSave(content_,snapshot);protect(slot);const auto info=inspect(slot);if(info.state!=SlotState::Empty&&info.state!=SlotState::Readable){return {info.state==SlotState::RecoveryRequired?WriteOutcome::RecoveryRequired:WriteOutcome::Failed,info.message};}
        if(present(path(slot,".backup.json"))){(void)decodeSave(content_,save_platform::read(path(slot,".backup.json")));}
        fs::create_directories(directory_);fault("begin");started=true;
        save_platform::writeExclusive(path(slot,".pending.json"),bytes,fault_);if(save_platform::read(path(slot,".pending.json"))!=bytes){throw std::runtime_error("Candidate readback mismatch");}
        (void)decodeSave(content_,save_platform::read(path(slot,".pending.json")));
        save_platform::writeExclusive(path(slot,".transaction.json"),"{\"transaction\":1}\n");save_platform::syncDirectory(directory_);
        if(info.state==SlotState::Readable){const auto old=save_platform::read(path(slot));(void)decodeSave(content_,old);fault("backup");save_platform::writeExclusive(path(slot,".backup.pending.json"),old);
            if(save_platform::read(path(slot,".backup.pending.json"))!=old){throw std::runtime_error("Backup readback mismatch");}
            save_platform::replace(path(slot,".backup.pending.json"),path(slot,".backup.json"));save_platform::syncDirectory(directory_);
        }
        fault("replace");save_platform::replace(path(slot,".pending.json"),path(slot));fault("after-replace");save_platform::syncDirectory(directory_);
        if(save_platform::read(path(slot))!=bytes){throw std::runtime_error("Published save does not match candidate");}
        fault("commit");fs::remove(path(slot,".transaction.json"));fault("final-sync");save_platform::syncDirectory(directory_);return {WriteOutcome::Committed,"保存成功"};
    }catch(const std::exception& e){
        if(started&&!present(path(slot,".transaction.json"))){try{save_platform::writeExclusive(path(slot,".transaction.json"),"{\"transaction\":1}\n");save_platform::syncDirectory(directory_);}catch(const std::exception&){/* Preserve original failure; disk can refuse all writes. */}}
        return {started?WriteOutcome::RecoveryRequired:WriteOutcome::Failed,e.what()};}
}
std::vector<RecoveryChoice> SaveFiles::recoveryChoices(int slot) const{
    protect(slot);std::vector<RecoveryChoice> choices;
    for(const auto& suffix:{".json",".pending.json",".backup.json",".backup.pending.json",".recover.pending.json"}){
        if(!present(path(slot,suffix))){continue;}try{auto bytes=save_platform::read(path(slot,suffix));auto snapshot=decodeSave(content_,bytes);choices.push_back({suffix,std::move(bytes),std::move(snapshot)});}catch(const std::exception&){/* Invalid alternatives remain untouched. */}
    }return choices;
}
WriteResult SaveFiles::recover(int slot,const RecoveryChoice& confirmed){
    try{
        protect(slot);const auto options=recoveryChoices(slot);const auto selected=std::find_if(options.begin(),options.end(),[&](const auto& c){return c.source==confirmed.source&&c.bytes==confirmed.bytes;});
        if(selected==options.end()){return {WriteOutcome::Failed,"恢复候选已变化，请重新查看"};}
        // Preserve every old artifact before any replacement, including invalid bytes.
        fs::path archive;for(int i=1;;++i){archive=directory_/("recovery-"+std::to_string(slot)+"-"+std::to_string(i));if(fs::create_directory(archive)){break;}}
        for(const auto& suffix:std::vector<std::string>{".json",".backup.json",".pending.json",".transaction.json",".backup.pending.json",".recover.pending.json"}){
            if(present(path(slot,suffix))){save_platform::archiveCopy(path(slot,suffix),archive/path(slot,suffix).filename());}
        }save_platform::syncDirectory(archive);save_platform::syncDirectory(directory_);
        // An earlier recovery candidate was archived; never overwrite it in place.
        if(present(path(slot,".recover.pending.json"))){fs::remove(path(slot,".recover.pending.json"));}
        save_platform::writeExclusive(path(slot,".recover.pending.json"),selected->bytes,fault_);fault("recover-replace");save_platform::replace(path(slot,".recover.pending.json"),path(slot));save_platform::syncDirectory(directory_);
        if(save_platform::read(path(slot))!=selected->bytes){throw std::runtime_error("Recovered save readback mismatch");}
        for(const auto& suffix:residues){fs::remove(path(slot,suffix));}fs::remove(path(slot,".backup.json"));fault("recover-final-sync");save_platform::syncDirectory(directory_);return {WriteOutcome::Committed,"恢复成功；其他版本保留在恢复归档目录"};
    }catch(const std::exception& e){
        if(!present(path(slot,".transaction.json"))){try{save_platform::writeExclusive(path(slot,".transaction.json"),"{\"transaction\":1}\n");save_platform::syncDirectory(directory_);}catch(const std::exception&){}}
        return {WriteOutcome::RecoveryRequired,e.what()};}
}
