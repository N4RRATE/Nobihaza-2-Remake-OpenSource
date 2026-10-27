#include "save_files.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
void require(bool b,const char* m){if(!b){throw std::runtime_error(m);}}
int main(int argc,char** argv){
 if(argc!=3){return 2;}auto c=loadCombatContent(argv[1]);std::filesystem::path root=argv[2];
 auto e=c.entries.at("defeated-claimed");e.map=16;e.playerTile={10,21};e.direction=Direction::Up;e.pages.clear();e.positions.clear();e.through.clear();e.directions.clear();c.entries["save"]=e;
 GameSession session(c.items,e.session);CombatScene scene(c,session,"save");auto snapshot=captureSave(c,scene,{scene.generation(),"m16-e5","BSAA 1F入口"},{});
 SaveFiles files(c,root);require(files.list().size()==15&&files.list()[14].state==SlotState::Empty,"fifteen empty slots");
 require(files.write(1,snapshot).outcome==WriteOutcome::Committed,"new slot committed");require(files.read(1).session.hp==70,"read independent snapshot");
 auto newer=snapshot;newer.session.hp=90;require(files.write(1,newer).outcome==WriteOutcome::Committed,"overwrite committed");require(files.read(1).session.hp==90&&files.list()[0].state==SlotState::Readable,"new complete progress published");
 for(const auto* stage:{"write","flush","close","backup","replace","after-replace","commit","final-sync"}){
   const auto isolated=root/stage;SaveFiles clean(c,isolated);require(clean.write(1,snapshot).outcome==WriteOutcome::Committed,"fault fixture original");
   SaveFiles faulty(c,isolated,[=](std::string_view point){if(point==stage){throw std::runtime_error("injected failure");}});
   require(faulty.write(1,newer).outcome==WriteOutcome::RecoveryRequired,"uncertain publication requires recovery");
   require(clean.list()[0].state==SlotState::RecoveryRequired,"interrupted transaction is visible");
   auto choices=clean.recoveryChoices(1);bool oldFound=false;for(const auto& choice:choices){oldFound|=choice.snapshot.session.hp==70;}
   require(oldFound,"old complete progress survives each failure");require(!choices.empty(),"recovery alternatives available");
   const auto choice=choices.front();require(clean.recover(1,choice).outcome==WriteOutcome::Committed,"explicit version recovery");
   require(clean.read(1).session.hp==choice.snapshot.session.hp,"selected whole snapshot restored");
 }
 std::ofstream(root/"slot-02.json")<<"broken";require(files.list()[1].state==SlotState::Corrupt,"bad slot distinct");
 auto incompatible=nlohmann::json::parse(encodeSave(c,snapshot));incompatible["version"]=2;std::ofstream(root/"slot-03.json")<<incompatible.dump();require(files.list()[2].state==SlotState::Incompatible,"incompatible slot distinct");
 std::filesystem::create_symlink(root/"slot-01.json",root/"slot-04.json");require(files.write(4,newer).outcome==WriteOutcome::Failed,"symlink protected");
 std::ofstream(root/"slot-05.foreign")<<"keep";require(files.write(5,newer).outcome==WriteOutcome::Failed,"unknown slot artifact protected");
 require(std::filesystem::exists(root/"slot-05.foreign"),"unknown file retained");
 const auto originalCwd=std::filesystem::current_path();std::filesystem::current_path(std::filesystem::temp_directory_path());require(files.read(1).session.hp==90,"directory independent of cwd");std::filesystem::current_path(originalCwd);
 require(defaultSaveDirectory().is_absolute(),"system path is absolute");
 auto damaged=root/"oversize";SaveFiles rescue(c,damaged);require(rescue.write(1,snapshot).outcome==WriteOutcome::Committed,"rescue fixture");require(rescue.write(1,newer).outcome==WriteOutcome::Committed,"rescue backup fixture");
 std::ofstream(damaged/"slot-01.json")<<std::string(8*1024*1024+1,'!');auto alternatives=rescue.recoveryChoices(1);require(!alternatives.empty(),"valid backup for oversized damage");require(rescue.recover(1,alternatives.front()).outcome==WriteOutcome::Committed,"oversize damaged original archived intact");
 std::ofstream(damaged/"slot-01.backup.json")<<"corrupt backup";alternatives=rescue.recoveryChoices(1);require(rescue.recover(1,alternatives.front()).outcome==WriteOutcome::Committed,"bad backup safely archived");require(rescue.write(1,newer).outcome==WriteOutcome::Committed,"recovered slot remains writable");
 SaveFiles recoveryFault(c,damaged,[](std::string_view point){if(point=="recover-final-sync"){throw std::runtime_error("sync failure");}});alternatives=recoveryFault.recoveryChoices(1);require(recoveryFault.recover(1,alternatives.front()).outcome==WriteOutcome::RecoveryRequired,"recovery final sync is uncertain");require(rescue.list()[0].state==SlotState::RecoveryRequired,"recovery final sync retains visible marker");
 std::cout<<"Save files checks passed\n";
}
