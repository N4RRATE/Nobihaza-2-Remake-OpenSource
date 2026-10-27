#include "save_files.h"
#include "save_platform.h"
#include "content_hash.h"
#include <iostream>
namespace fs=std::filesystem;
namespace{
void need(bool ok,const char* why){if(!ok){throw std::runtime_error(why);}}
void unchanged(const fs::path& target,const std::string& original,const std::string& backup){
    need(save_platform::read(target/"slot-05.json")==original,"Slot5 changed since backup; publication refused");
    need(save_platform::read(target/"slot-05.backup.json")==backup,"Slot5 backup changed; publication refused");
}
}
int main(int argc,char** argv){try{
    need(argc==8,"slot05_recovery publish package expected-original-dir candidate target-save-dir new-independent-archive candidate-sha256");
    const std::string mode=argv[1];need(mode=="publish","Only bounded slot5 publication is supported");
    const fs::path originalDir=argv[3],candidatePath=argv[4],target=argv[5],archive=argv[6];
    const auto original=save_platform::read(originalDir/"slot-05.json"),backup=save_platform::read(originalDir/"slot-05.backup.json"),candidate=save_platform::read(candidatePath);
    need(contentSha256(candidate)==argv[7],"Approved candidate hash changed");
    unchanged(target,original,backup);
    const auto c=loadCombatContent(argv[2]);
    const auto source=decodeSave(c,original);(void)decodeSave(c,backup);const auto restored=decodeSave(c,candidate);
    need(source.scene.map==107&&source.continuation.kind=="ordinary","Expected source is not the reviewed M107 ordinary save");
    need(restored.continuation.kind=="development_f5_recovery","Candidate is not bounded F5 recovery");
    need(restored.session.saveCount==source.session.saveCount&&restored.continuation.statisticBefore==source.continuation.statisticBefore+1,"Candidate/source save counters differ");
    SaveFiles files(c,target);need(files.list().at(4).state==SlotState::Readable,"Slot5 has transaction residue or is unreadable");
    need(contentSha256(candidate)==argv[7],"Approved candidate hash changed");
    need(encodeSave(c,restored)==candidate,"Candidate is not canonical validated bytes");
    const auto expected=original;
    unchanged(target,expected,backup);
    need(!fs::exists(archive)&&!fs::is_symlink(fs::symlink_status(archive)),"Independent archive already exists");
    need(fs::create_directory(archive),"Cannot create independent archive");
    save_platform::archiveCopy(target/"slot-05.json",archive/"slot-05.json");
    save_platform::archiveCopy(target/"slot-05.backup.json",archive/"slot-05.backup.json");
    unchanged(archive,original,backup);
    save_platform::syncDirectory(archive);save_platform::syncDirectory(archive.parent_path());
    unchanged(target,expected,backup);
    SaveFiles transaction(c,target,[&](std::string_view stage){
        if(stage=="begin"||stage=="backup"){unchanged(target,expected,backup);}
        if(stage=="replace"){unchanged(target,expected,expected);}
    });
    const auto& bytes=candidate;
    const auto result=transaction.write(5,restored);
    if(result.outcome!=WriteOutcome::Committed){std::cerr<<"SLOT5_NOT_COMMITTED outcome="<<(result.outcome==WriteOutcome::RecoveryRequired?"RecoveryRequired":"Failed")<<" message="<<result.message<<" archive="<<archive<<"\n";return 1;}
    need(save_platform::read(target/"slot-05.json")==bytes,"Slot5 publication differs");
    (void)files.read(5);
    std::cout<<"SLOT5_"<<mode<<"_COMMITTED sha256="<<contentSha256(bytes)<<" independent_archive="<<archive<<"\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
