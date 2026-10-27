#include "title_navigation.h"
#include "content_hash.h"
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace {
namespace fs=std::filesystem;
using Json=nlohmann::json;
void need(bool ok,const std::string& message){if(!ok){throw std::runtime_error(message);}}
std::string bytes(const fs::path& path){std::ifstream file(path,std::ios::binary);need(bool(file),"Missing input: "+path.string());return {std::istreambuf_iterator<char>(file),{}};}
InputFrame key(InputAction action=InputAction::Confirm){InputFrame input;input.buttons[int(action)]={true,true,false};return input;}
void tick(GameApplication& game){game.update({});game.takePresentation();}
void load(GameApplication& game,int slot=1){finishTitleOpening(game);game.update(key());need(game.mode()==ApplicationMode::Loading,"Public title Load list absent");for(int n=1;n<slot;++n){game.update(key(InputAction::Down));}game.update(key());need(game.mode()==ApplicationMode::Playing&&game.hasGame(),"Public title Load failed");}
void sameActor(const SessionEntry& actual,const SessionEntry& saved){need(actual.hp==saved.hp&&actual.maxHp==saved.maxHp&&actual.tp==saved.tp&&actual.inventory==saved.inventory&&actual.magazines==saved.magazines&&actual.skills==saved.skills&&actual.difficulty==saved.difficulty&&actual.saveCount==saved.saveCount,"Loaded owned actor/inventory/count differs");}
void publicSave(GameApplication& game,const SaveSnapshot& source,const fs::path& root){
 load(game);sameActor(game.session().exportState(),source.session);
 for(int n=0;n<1000&&(game.foreground()!=ForegroundKind::None||game.dialogue().active()||game.world().player().motion.moving());++n){game.update(n%8==0?key():InputFrame{});game.takePresentation();}
 // The genuine ordinary and won-Warehouse source disks face their original save object.
 const int count=game.session().saveCount(),statistic=game.session().variable(399);
 game.update(key());for(int n=0;n<2000&&!game.fileMenuView();++n){game.update(n%8==0?key():InputFrame{});game.takePresentation();}
 need(game.mode()==ApplicationMode::Saving&&game.fileMenuView(),"Original source save object did not open file menu");
 game.update(key(InputAction::Down));need(game.fileMenuView()->selection==1,"Isolated slot2 not selected");game.update(key());
 need(game.mode()==ApplicationMode::Playing&&game.session().saveCount()==count+1,"Public save did not commit exactly once");
 const auto saved=SaveFiles(game.content(),root).read(2);need(saved.continuation.kind==source.continuation.kind&&saved.session.saveCount==count+1,"Public disk kind/count differs");
 need(game.session().variable(399)==statistic+(source.continuation.kind=="ordinary"?1:0),"Public save changed the wrong statistic");
}
Json boundary(const CombatContent& content,const SaveSnapshot& source,const fs::path& root){
 const auto encoded=encodeSave(content,source);SaveFiles files(content,root);
 need(files.write(1,source).outcome==WriteOutcome::Committed,"Boundary seed failed");const auto before=bytes(root/"slot-01.json");
 std::ofstream(root/"slot-02.json")<<"broken";need(files.list()[1].state==SlotState::Corrupt,"Damaged save accepted");
 auto unknown=Json::parse(encoded);unknown["after_save"]["kind"]="future_unknown";std::ofstream(root/"slot-03.json")<<unknown.dump();need(files.list()[2].state!=SlotState::Readable,"Unknown kind accepted");
 {std::ofstream oversize(root/"slot-04.json");oversize<<std::string(8*1024*1024+1,'!');}need(files.list()[3].state==SlotState::Corrupt,"Oversize accepted");
 fs::create_symlink(root/"slot-01.json",root/"slot-05.json");need(files.write(5,source).outcome==WriteOutcome::Failed,"Symlink write followed");
 std::ofstream(root/"slot-06.foreign")<<"protected";need(files.write(6,source).outcome==WriteOutcome::Failed&&bytes(root/"slot-06.foreign")=="protected","Foreign artifact overwritten");
 const auto readonly=root/"readonly";fs::create_directory(readonly);SaveFiles protectedFiles(content,readonly);need(protectedFiles.write(1,source).outcome==WriteOutcome::Committed,"Readonly seed failed");const auto protectedBytes=bytes(readonly/"slot-01.json");
 const auto permission=fs::status(readonly).permissions();fs::permissions(readonly,fs::perms::owner_read|fs::perms::owner_exec);
 const auto outcome=protectedFiles.write(1,source).outcome;fs::permissions(readonly,permission);
 need(outcome!=WriteOutcome::Committed&&bytes(readonly/"slot-01.json")==protectedBytes,"Actual unwritable directory damaged old bytes or reported Committed");
 GameApplication live(content,"title");std::string error;need(live.restore(source,error),"Valid active restore failed: "+error);const auto state=live.session().exportState();const auto generation=live.generation();auto bad=source;bad.contentCompatibility="unknown";need(!live.restore(bad,error)&&live.generation()==generation&&live.session().exportState()==state,"Bad candidate replaced active game");
 need(bytes(root/"slot-01.json")==before,"Protection changed good slot");
 return {{"corrupt_unknown_oversize_rejected",true},{"symlink_and_foreign_preserved",true},{"actual_readonly_write_not_Committed",true},{"active_game_preserved",true}};
}
}
int main(int argc,char** argv){try{
 std::cout.setf(std::ios::unitbuf);need(argc==5,"release_save_checks package output source publish|save|load|load2|wake|boundary|crash|recover");
 const auto content=loadCombatContent(argv[1]);const fs::path root=fs::absolute(argv[2]);const auto raw=bytes(argv[3]);const auto source=decodeSave(content,raw);const std::string mode=argv[4];Json report={{"mode",mode},{"source_sha256",contentSha256(raw)},{"source_kind",source.continuation.kind},{"source_map",source.scene.map},{"source_count",source.session.saveCount},{"default_save_directory",defaultSaveDirectory().string()}};
 if(mode=="load"||mode=="load2"||mode=="wake"){
  const int slot=mode=="load2"?2:1;SaveFiles files(content,root);const auto expected=files.read(slot);const auto path=root/(slot==1?"slot-01.json":"slot-02.json");const auto before=bytes(path);GameApplication game(content,"title");game.configureSaveDirectory(root);load(game,slot);sameActor(game.session().exportState(),expected.session);
  need(game.mapId()==expected.scene.map,"Load installed wrong map");const bool ordinary=expected.continuation.kind=="ordinary";need(game.session().variable(399)==expected.continuation.statisticBefore+(ordinary?1:0),"Load statistic did not follow saved kind");
  for(int n=0;n<5;++n){tick(game);}need(game.session().saveCount()==expected.session.saveCount&&game.session().variable(399)==expected.continuation.statisticBefore+(ordinary?1:0),"Load continuation counted twice");
  if(mode=="wake"){
   need(expected.continuation.kind=="chapter_handoff_after_file","Wake requires real Chapter source");
   for(int n=0;n<30000&&!(game.session().flag(360)&&game.foreground()==ForegroundKind::None&&!game.dialogue().active());++n){game.update(n%8==0?key():InputFrame{});game.takePresentation();}
   need(game.mapId()==127&&game.world().player().motion.tile()==TilePosition{11,7}&&game.session().flag(360)&&game.session().flag(1)&&game.session().flag(18)&&game.foreground()==ForegroundKind::None,"Current complete wake did not restore original position/control");
   need(game.session().saveCount()==expected.session.saveCount&&game.session().variable(399)==expected.continuation.statisticBefore&&game.session().exportState().magazines==expected.session.magazines&&game.session().exportState().inventory==expected.session.inventory,"Chapter wake changed save count/statistic/inventory/ammunition");
   game.update(key(InputAction::Cancel));need(game.mode()==ApplicationMode::Equipment,"Current wake did not enable public menu");game.update(key(InputAction::Cancel));need(game.mode()==ApplicationMode::Playing,"Current wake menu did not close");report["wake_completed_to_control"]=true;
  }
  need(bytes(path)==before,"Load rewrote disk");report["loaded_count"]=game.session().saveCount();report["statistic"]=game.session().variable(399);report["after_five_ticks_map"]=game.mapId();report["independent_public_Load"]=true;
 }else if(mode=="crash"){
  need(fs::exists(root/"slot-01.json"),"Crash fixture requires committed source");SaveFiles interrupted(content,root,[](std::string_view stage){if(stage=="replace"){std::_Exit(73);}});(void)interrupted.write(1,source);throw std::runtime_error("Process did not exit at real replace boundary");
 }else if(mode=="recover"){
  SaveFiles files(content,root);need(files.list()[0].state==SlotState::RecoveryRequired,"Abrupt process residue not visible");const auto choices=files.recoveryChoices(1);need(!choices.empty(),"Abrupt process lost all whole candidates");auto chosen=std::find_if(choices.begin(),choices.end(),[&](const auto& choice){return choice.snapshot==source;});need(chosen!=choices.end(),"Exact interrupted candidate not recoverable");need(files.recover(1,*chosen).outcome==WriteOutcome::Committed&&files.read(1)==source,"Explicit real-process recovery did not commit exact candidate");report["actual_process_exit_recovery"]=true;
 }else{
  need(!fs::exists(root),"Output must be fresh; never overwrite source progress");fs::create_directories(root);
  if(mode=="boundary"){report["boundary"]=boundary(content,source,root);}
  else{need(mode=="publish"||mode=="save","Unknown release save mode");SaveFiles files(content,root);need(files.write(1,source).outcome==WriteOutcome::Committed&&files.read(1)==source,"Current package new-file transaction rejected genuine disk");need(files.write(1,source).outcome==WriteOutcome::Committed&&files.read(1)==source,"Current package overwrite changed genuine disk");report["genuine_snapshot_new_and_overwrite"]=true;
   if(mode=="save"){need(source.continuation.kind=="ordinary"||source.continuation.kind=="direct_unscored","Public resave only supports source-facing ordinary/direct disk");GameApplication game(content,"title");game.configureSaveDirectory(root);game.setPresentationState(source.presentation);publicSave(game,source,root);report["public_save_from_legal_disk"]=true;report["saved_count"]=files.read(2).session.saveCount;}}
 }
 need(bytes(argv[3])==raw,"Source file changed");report["source_unchanged"]=true;report["result"]="PASS";std::ofstream(root/("release-"+mode+"-result.json"))<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
