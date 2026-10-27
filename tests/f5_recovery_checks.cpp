#include "game_application.h"
#include "save_platform.h"
#include <iostream>
namespace fs=std::filesystem;
namespace{
void need(bool ok,const char* why){if(!ok){throw std::runtime_error(why);}}
template<class F>void reject(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}need(failed,"Invalid recovery accepted");}
InputFrame key(InputAction a=InputAction::Confirm){InputFrame f;f.buttons[static_cast<int>(a)].pressed=true;return f;}
}
int main(int argc,char** argv){try{
    need(argc==5,"f5_recovery_checks package original candidate isolated-root");
    auto c=loadCombatContent(argv[1]);const auto sourceBytes=save_platform::read(argv[2]),bytes=save_platform::read(argv[3]);
    const auto source=decodeSave(c,sourceBytes),saved=decodeSave(c,bytes);
    need(saved.continuation.kind=="development_f5_recovery"&&saved.session.saveCount==source.session.saveCount&&saved.continuation.statisticBefore==source.continuation.statisticBefore+1,"Recovery/source counters differ");
    auto bad=saved;bad.continuation.kind="ordinary";reject([&]{validateSave(c,bad);});
    bad=saved;bad.continuation.point="other-node";reject([&]{validateSave(c,bad);});
    bad=saved;bad.scene.map=122;reject([&]{validateSave(c,bad);});
    bad=saved;bad.scene.playerTile={11,27};reject([&]{validateSave(c,bad);});
    bad=saved;bad.scene.direction=Direction::Down;reject([&]{validateSave(c,bad);});
    bad=saved;bad.session.skills.erase(4);bad.session.skills.insert(3);reject([&]{validateSave(c,bad);});
    bad=saved;bad.session.skills.insert(3);reject([&]{validateSave(c,bad);});
    bad=saved;bad.session.saveCount++;reject([&]{validateSave(c,bad);});
    bad=saved;bad.session.flags[18]=false;reject([&]{validateSave(c,bad);});
    bad=saved;bad.session.eventTimer.running=true;reject([&]{validateSave(c,bad);});
    for(const auto& id:c.catalog.at("development_recovery").at("required_flags")){bad=saved;bad.session.worldFlags[c.facts.switches.at(id)]=false;reject([&]{validateSave(c,bad);});}
    for(const auto& id:c.catalog.at("development_recovery").at("absent_flags")){bad=saved;bad.session.worldFlags[c.facts.switches.at(id)]=true;reject([&]{validateSave(c,bad);});}
    bad=saved;bad.scene.positions.begin()->second={1,1};reject([&]{validateSave(c,bad);});
    bad=saved;bad.scene.positions.erase(bad.scene.positions.begin());reject([&]{validateSave(c,bad);});
    bad=saved;bad.contentCompatibility="f4-normal-1";reject([&]{validateSave(*c.legacySource,bad);});
    auto malformed=nlohmann::json::parse(sourceBytes);malformed["after_save"]["count_before"]=0;reject([&]{decodeSave(c,malformed.dump());});
    GameApplication app(c,"title");std::string error;need(app.restore(source,error),"Original ordinary Load failed");
    const auto original=app.session().exportState();const auto generation=app.generation();
    app.configureRestorePresentation([](const SceneDefinition&,const SavedPresentation&){throw std::runtime_error("missing resource");});
    need(!app.restore(saved,error)&&app.session().exportState()==original&&app.generation()==generation,"Failed resource replaced source game");
    app.configureRestorePresentation({});need(app.restore(saved,error),"Bounded recovery Load failed");
    const auto initial=app.session().exportState();need(initial.inventory==saved.session.inventory&&initial.magazines==saved.session.magazines&&initial.saveCount==saved.session.saveCount&&app.session().variable(399)==saved.continuation.statisticBefore,"Recovery changed committed inventory/count/statistic");
    auto restored=prepareRestore(c,saved);finishSaveContinuation(*restored.session,saved);finishSaveContinuation(*restored.session,saved);
    need(restored.session->saveCount()==initial.saveCount&&restored.session->variable(399)==app.session().variable(399),"Recovery continuation duplicated counts");
    reject([&]{captureSave(c,*restored.scene,{generation,"m107-e3",""},saved.presentation);});
    reject([&]{captureSave(c,*restored.scene,{restored.scene->generation(),"m121-e2",""},saved.presentation);});
    need(!restored.scene->takeSaveRequest()&&!restored.scene->takeApplicationRequest(),"Load retained a stale request");
    app.update(key(InputAction::Cancel));need(app.mode()==ApplicationMode::Equipment,"Recovery menu unavailable");app.update(key(InputAction::Cancel));need(app.mode()==ApplicationMode::Playing,"Recovery menu cannot close");
    need(app.restore(saved,error)&&app.session().exportState()==initial,"Second Load repeated rewards or counts");
    fs::path root=argv[4];fs::create_directories(root);SaveFiles files(c,root);
    save_platform::writeExclusive(root/"slot-05.json",sourceBytes);
    WriteResult result;
    need(save_platform::read(root/"slot-05.json")==sourceBytes,"Original v2 bytes were reencoded");
    SaveFiles failing(c,root,[](std::string_view at){if(at=="replace"){throw std::runtime_error("injected publish failure");}});
    result=failing.write(5,saved);need(result.outcome==WriteOutcome::RecoveryRequired&&save_platform::read(root/"slot-05.json")==sourceBytes&&save_platform::read(root/"slot-05.backup.json")==sourceBytes,"Failed transaction lost original");
    need(files.list().at(4).state==SlotState::RecoveryRequired,"Failed transaction not protected");
    const auto choices=files.recoveryChoices(5);const auto old=std::find_if(choices.begin(),choices.end(),[&](const auto& x){return x.bytes==sourceBytes;});need(old!=choices.end(),"Original recovery choice absent");
    need(files.recover(5,*old).outcome==WriteOutcome::Committed,"Original recovery failed");
    result=files.write(5,saved);need(result.outcome==WriteOutcome::Committed&&files.read(5)==saved,"Validated candidate transaction failed");
    bad=saved;bad.scene.map=122;result=files.write(5,bad);need(result.outcome==WriteOutcome::Failed&&save_platform::read(root/"slot-05.json")==bytes,"Invalid snapshot changed committed candidate");
    need(save_platform::read(argv[2])==sourceBytes&&save_platform::read(argv[3])==bytes,"Read-only sources changed");
    std::cout<<"Bounded recovery schema/facts/counters, legacy source rejection, stale requests, two Loads, failed preparation, transaction residue/recovery and invalid write protection passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
