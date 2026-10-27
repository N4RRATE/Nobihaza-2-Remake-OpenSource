#include "file_menu.h"
#include "save_platform.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
using Json=nlohmann::json;
using Clock=std::chrono::steady_clock;
namespace fs=std::filesystem;
void need(bool ok,const char* message){if(!ok){throw std::runtime_error(message);}}
InputFrame key(InputAction action){InputFrame in;in.buttons[std::size_t(action)]={true,true,false};return in;}
void put(const fs::path& path,const std::string& bytes){std::ofstream out(path,std::ios::binary);out<<bytes;need(bool(out),"Fixture write failed");}
std::string slotName(int slot){std::ostringstream out;out<<"slot-"<<std::setw(2)<<std::setfill('0')<<slot<<".json";return out.str();}
template<class F> double timed(F&& action){const auto start=Clock::now();action();return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
}
int main(int argc,char** argv){try{
    need(argc>=5,"package new-isolated-output-directory repeats source-save [source-save ...]");
    const fs::path root=fs::absolute(argv[2]);need(!fs::exists(root),"Output directory must not exist (protect source saves)");fs::create_directories(root);
    const int repeats=std::stoi(argv[3]);need(repeats>=1&&repeats<=3,"Use 1..3 bounded samples");
    Json report={{"scope","save pipeline CPU/file transaction phases; excludes renderer/audio and native input-to-frame latency"},{"samples",Json::array()}};
    const auto flush=[&]{put(root/"measurements.json",report.dump(2)+"\n");};
    std::optional<CombatContent> content;report["content_load_ms"]=timed([&]{content.emplace(loadCombatContent(argv[1]));});flush();
    auto& c=*content;
    for(int source=4;source<argc;++source){
        const auto bytes=save_platform::read(argv[source]);const auto original=Json::parse(bytes);
        Json result={{"source",argv[source]},{"bytes",bytes.size()},{"content",original.at("content")},{"map",original.at("scene").at("map")},{"difficulty",original.at("session").at("difficulty")},{"save_count",original.at("session").at("save_count")},{"kind",original.at("after_save").value("kind","ordinary")},{"runs",Json::array()}};
        const auto sourceIdentity=original.at("content").get<std::string>();int generations=0;for(const CombatContent* sourceContent=&c;sourceContent&&sourceContent->compatibility!=sourceIdentity;sourceContent=sourceContent->legacySource.get()){++generations;}result["adaptation_generations"]=generations;
        const fs::path fixture=root/("source-"+std::to_string(source-4));fs::create_directory(fixture);
        for(int count:{1,15}){const auto dir=fixture/std::to_string(count);fs::create_directory(dir);for(int slot=1;slot<=count;++slot){put(dir/slotName(slot),bytes);}}
        SaveSnapshot snapshot;
        for(int run=0;run<repeats;++run){
            Json sample;sample["decode_adapt_ms"]=timed([&]{snapshot=decodeSave(c,bytes);});
            sample["validate_ms"]=timed([&]{validateSave(c,snapshot);});
            sample["prepare_candidate_no_media_ms"]=timed([&]{auto candidate=prepareRestore(c,snapshot);need(candidate.session->saveCount()==snapshot.session.saveCount,"Prepare changed save count");need(candidate.session->variable(399)==snapshot.continuation.statisticBefore+(snapshot.continuation.kind=="ordinary"?1:0),"Prepare changed statistic continuation");});
            for(int count:{1,15}){SaveFiles files(c,fixture/std::to_string(count));sample["list_"+std::to_string(count)+"_ms"]=timed([&]{auto slots=files.list();need(slots.size()==15&&slots[count-1].state==SlotState::Readable,"Real save list rejected");});}
            SaveFiles files(c,fixture/"1");
            std::optional<FileMenu> menu;sample["menu_open_ms"]=timed([&]{menu.emplace(files);});
            sample["menu_confirm_ms"]=timed([&]{menu->update(key(InputAction::Confirm));auto loaded=menu->takeResult();need(loaded&&loaded->kind==FileMenuResult::Kind::Loaded&&loaded->snapshot==snapshot,"Public file menu Load changed snapshot");});
            SaveFiles output(c,fixture/("write-"+std::to_string(run)));
            sample["write_new_ms"]=timed([&]{need(output.write(1,snapshot).outcome==WriteOutcome::Committed,"New save failed");});
            sample["write_overwrite_ms"]=timed([&]{need(output.write(1,snapshot).outcome==WriteOutcome::Committed,"Overwrite save failed");});
            result["runs"].push_back(sample);
            std::cout<<"source "<<source-4<<" run "<<run<<' '<<sample.dump()<<'\n'<<std::flush;
            report["current_sample"]=result;flush();
        }
        // A displayed summary is never trusted after an external file mutation.
        SaveFiles files(c,fixture/"1");
        const auto rejected=[&](const std::string& replacement,bool remove=false){put(fixture/"1"/"slot-01.json",bytes);FileMenu menu(files);if(remove){fs::remove(fixture/"1"/"slot-01.json");}else{put(fixture/"1"/"slot-01.json",replacement);}menu.update(key(InputAction::Confirm));need(!menu.takeResult(),"Changed slot accepted stale displayed snapshot");};
        rejected("broken");rejected("",true);auto unknown=original;unknown["content"]="unknown-source";rejected(unknown.dump());
        put(fixture/"1"/"slot-01.json",bytes);{FileMenu menu(files);auto changed=original;changed["summary"]["saved_at_unix"]=changed["summary"]["saved_at_unix"].get<std::int64_t>()+1;const auto stamp=fs::last_write_time(fixture/"1"/"slot-01.json");put(fixture/"1"/"slot-01.json",changed.dump(2)+"\n");fs::last_write_time(fixture/"1"/"slot-01.json",stamp);menu.update(key(InputAction::Confirm));const auto loaded=menu.takeResult();need(loaded&&loaded->snapshot&&loaded->snapshot->summary.savedAtUnix==snapshot.summary.savedAtUnix+1,"Valid external replacement returned stale snapshot");}
        put(fixture/"1"/"slot-01.json",bytes);{FileMenu menu(files);put(fixture/"1"/"slot-01.transaction.json","{}");menu.update(key(InputAction::Confirm));need(!menu.takeResult(),"Transaction residue accepted stale displayed snapshot");fs::remove(fixture/"1"/"slot-01.transaction.json");}
        {FileMenu menu(files);menu.update(key(InputAction::Cancel));const auto cancelled=menu.takeResult();need(cancelled&&cancelled->kind==FileMenuResult::Kind::Cancelled,"Cancel changed");}
        auto bad=snapshot;bad.session.worldFlags["unknown.fact"]=true;bool refused=false;try{validateSave(c,bad);}catch(const std::exception&){refused=true;}need(refused,"Unknown world flag accepted");
        bad=snapshot;bad.session.worldValues["unknown.value"]=1;refused=false;try{validateSave(c,bad);}catch(const std::exception&){refused=true;}need(refused,"Unknown world value accepted");
        auto badSource=original;badSource["session"]["world_flags"].push_back({"unknown.source-fact",true});refused=false;try{(void)decodeSave(c,badSource.dump());}catch(const std::exception&){refused=true;}need(refused,"Source contract unknown flag accepted during adaptation");
        SaveFiles failed(c,fixture/"failure",[](std::string_view point){if(point=="begin"){throw std::runtime_error("injected before transaction");}});need(failed.write(1,snapshot).outcome==WriteOutcome::Failed&&!fs::exists(fixture/"failure"/"slot-01.json"),"Failed save published candidate");
        result["safety_checks"]="PASS: valid-replaced-with-same-mtime/corrupt/deleted/incompatible/residue/unknown-member/cancel/pre-transaction-failure";
        report["samples"].push_back(result);report.erase("current_sample");flush();
    }
    const auto mixed=root/"mixed-15";fs::create_directory(mixed);for(int slot=1;slot<=15;++slot){put(mixed/slotName(slot),save_platform::read(argv[4+(slot-1)%(argc-4)]));}
    SaveFiles mixedFiles(c,mixed);report["mixed_15_ms"]=Json::array();for(int run=0;run<repeats;++run){report["mixed_15_ms"].push_back(timed([&]{const auto slots=mixedFiles.list();for(const auto& slot:slots){need(slot.state==SlotState::Readable,"Mixed current/old list rejected");}}));}
    report["result"]="PASS";flush();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
