#include "game_application.h"
#include "full_content.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace{
void need(bool v,const char* why){if(!v){throw std::runtime_error(why);}}
InputFrame key(InputAction a=InputAction::Confirm){InputFrame f;f.buttons[static_cast<int>(a)].pressed=true;return f;}
std::string read(const std::filesystem::path& p){std::ifstream f(p);need(f.is_open(),"Missing saved route evidence");return {std::istreambuf_iterator<char>(f),{}};}
template<class F>void reject(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}need(failed,"Invalid candidate was accepted");}
void question(GameApplication& g){g.update(key());need(g.mode()==ApplicationMode::Saving,"Public save point did not open");for(int n=0;n<200;++n){g.update({});}}
}
int main(int argc,char** argv){try{
    need(argc==4,"g0_save_checks package natural-save isolated-root");auto c=loadCombatContent(argv[1]);const auto saved=decodeSave(c,read(argv[2]));
    need(saved.scene.map==122&&saved.continuation.version==2&&saved.continuation.kind=="ordinary","Natural route has no ordinary G0 snapshot");
    const auto encoded=encodeSave(c,saved);const auto data=nlohmann::json::parse(encoded);need(data.at("schema")=="nobihaza.save.v4"&&data.at("after_save").at("kind")=="ordinary","Explicit v4 ordinary format");
    for(const auto* identity:{"future","","f4-normal-2"}){auto bad=data;bad["content"]=identity;reject([&]{decodeSave(c,bad.dump());});}
    for(const auto* field:{"schema","kind"}){auto bad=data;if(std::string(field)=="schema"){bad[field]="nobihaza.save.v9";}else{bad["after_save"][field]="chapter_handoff_after_file";}reject([&]{decodeSave(c,bad.dump());});}
    auto bad=data;bad["after_save"].erase("kind");reject([&]{decodeSave(c,bad.dump());});
    for(const auto& [id,name]:c.catalog.at("g0").at("facts").items()){
        bad=data;auto& facts=bad["session"]["world_flags"];std::erase_if(facts.get_ref<nlohmann::json::array_t&>(),[&](const auto& pair){return pair.at(0)==name;});reject([&]{decodeSave(c,bad.dump());});
    }
    bad=data;for(auto& pair:bad["session"]["world_flags"]){if(pair.at(0)=="hangar.first-visit"){pair[1]=false;}}reject([&]{decodeSave(c,bad.dump());});
    bad=data;for(auto& pair:bad["session"]["world_flags"]){if(pair.at(0)=="hangar.first-visit"){pair[1]="true";}}reject([&]{decodeSave(c,bad.dump());});
    auto candidate=prepareRestore(c,saved);const int count=candidate.session->saveCount(),stats=candidate.session->variable(399);
    need(candidate.session->flag(346)&&candidate.session->flag(347)&&candidate.session->flag(355)&&candidate.session->flag(501),"Snapshot lost persistent source facts");
    need(candidate.scene->world().player().motion.tile()==TilePosition{11,7},"Restored source approach is incorrect");
    auto next=captureSave(c,*candidate.scene,{candidate.scene->generation(),"m122-e16",""},saved.presentation);
    finishSaveContinuation(*candidate.session,next);finishSaveContinuation(*candidate.session,next);need(candidate.session->saveCount()==count+1&&candidate.session->variable(399)==stats+1,"Ordinary continuation is not idempotent");
    reject([&]{captureSave(c,*candidate.scene,{candidate.scene->generation()+1,"m122-e16",""},saved.presentation);});
    auto wrong=saved;wrong.scene.direction=Direction::Left;reject([&]{validateSave(c,wrong);});wrong=saved;wrong.scene.playerTile={20,7};reject([&]{validateSave(c,wrong);});
    auto moving=prepareRestore(c,saved);for(int n=0;n<12;++n){moving.scene->update({});}InputFrame motion;for(const auto direction:{Direction::Down,Direction::Left,Direction::Right}){if(moving.scene->world().canPlayerMove(direction)){motion.movement=direction;break;}}moving.scene->update(motion);need(moving.scene->world().player().motion.moving(),"Movement test needs an actual moving player");reject([&]{captureSave(c,*moving.scene,{moving.scene->generation(),"m122-e16",""},saved.presentation);});
    GameApplication app(c,"title");std::string error;need(app.restore(saved,error),"Application failed to prepare valid route snapshot");app.configureSaveDirectory(argv[3]);auto initial=app.session().exportState();
    question(app);app.update(key(InputAction::Down));app.update(key());need(app.mode()==ApplicationMode::Playing&&app.session().saveCount()==initial.saveCount&&app.session().variable(399)==stats,"Question no changed counters");
    question(app);app.update(key());need(app.fileMenuView().has_value(),"Yes did not open slots");app.update(key(InputAction::Cancel));need(app.mode()==ApplicationMode::Playing&&app.session().saveCount()==initial.saveCount,"Slot cancel changed count");
    question(app);app.update(key());app.update(key());need(app.mode()==ApplicationMode::Playing&&app.session().saveCount()==initial.saveCount+1&&app.session().variable(399)==stats+1,"Successful ordinary save did not commit both continuations");
    app.update(key());need(app.mode()==ApplicationMode::Saving,"Repeated real action should request a new save");for(int n=0;n<200;++n){app.update({});}app.update(key(InputAction::Down));app.update(key());need(app.session().saveCount()==initial.saveCount+1,"Repeated confirmation/cancel duplicated prior transaction");
    SaveFiles files(c,argv[3]);const auto disk=files.read(1);need(disk.session.saveCount==initial.saveCount+1&&disk.session.variables.at(399)==stats,"Disk must retain pre-continuation V399");
    auto changed=c;changed.objects.at("m122-e16").variants.front().flags[350]=true;reject([&]{prepareRestore(changed,saved);});
    reject([&]{prepareRestore(c,saved,[](const SceneDefinition&){throw std::runtime_error("missing texture");});});need(app.session().saveCount()==initial.saveCount+1&&app.mapId()==122,"Failed restore replaced live game");
    // Another original size uses the same world preparation; this is an explicit geometry fixture, not a chapter route.
    auto e=c.entries.at("post-pickup");e.map=127;e.playerTile={13,7};e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();auto geometry=c;geometry.pendingMapTickets.erase(127);GameSession session(geometry.items,e.session,geometry.facts);CombatScene smaller(geometry,session,e);need(smaller.definition().width==20&&smaller.definition().height==15,"World assumes hangar dimensions");
    std::cout<<"G0 v3 strict schema/kind/facts, ordinary cancel/commit/idempotence, safe restore and map variation passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
