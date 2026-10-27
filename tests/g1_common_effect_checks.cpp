#include "save_files.h"
#include "enemy_host.h"
#include <fstream>
#include <iostream>
#include <sstream>
void need(bool ok,const char* message){if(!ok){throw std::runtime_error(message);}}
template<class F> void rejects(F run){bool failed=false;try{run();}catch(const std::exception&){failed=true;}need(failed,"Malformed common effects accepted");}
std::string read(const std::filesystem::path& path){std::ifstream in(path);need(bool(in),"Missing source save");std::ostringstream out;out<<in.rdbuf();return out.str();}
int main(int argc,char** argv){try{
    std::cout.setf(std::ios::unitbuf);
    need(argc==5,"package source isolated-root write|read");const auto c=loadCombatContent(argv[1]);const std::filesystem::path root=argv[3];SaveFiles files(c,root);
    if(std::string(argv[4])=="read"){
        auto disk=files.read(1);auto loaded=prepareRestore(c,disk);
        need(disk.formatVersion==4&&!loaded.session->conditions().bleedingActive&&loaded.session->conditions().bleedingTicks==600,"Independent Load lost cured pending CE34");
        need(!loaded.session->flag(48)&&loaded.session->quickshotTicks()==0,"OFF quickshot Load restored an old interpreter cursor");
        need(loaded.session->variable(301)==24&&loaded.session->variable(308)==8&&loaded.session->burnWaitRemaining(24)==0,"Independent Load lost counters or restored fire cursor");
        loaded.session->startBleeding(1800);for(int n=0;n<600;++n){loaded.session->tickMapCommonEffects(false);}need(!loaded.session->conditions().bleedingActive&&loaded.session->conditions().bleedingTicks==0,"Old CE34 tail did not cure newly applied State3");
        need(loaded.session->castSkill(c.skills.at(9))==SkillUseResult::Applied&&loaded.session->quickshotTicks()==900,"OFF Load did not rebuild fresh CE60 on recast");
        std::cout<<"Independent v4 cured-wait/burn counters/paused-quick Load passed\n";return 0;
    }
    std::cout<<"source-read\n";
    const auto old=decodeSave(c,read(argv[2]));auto disk=old;disk.formatVersion=4;disk.session.commonEffectsKnown=true;disk.session.conditions.bleedingTicks=600;disk.session.conditions.bleedingActive=true;disk.session.quickshotTicks=0;disk.session.quickshotActive=false;
    auto entry=c.entries.at("post-pickup");entry.map=141;entry.playerTile={8,7};entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();entry.session=disk.session;entry.session.flags[1]=entry.session.flags[18]=true;
    std::cout<<"CE12-fixture\n";
    GameSession state(c.items,entry.session,c.facts);CombatScene scene(c,state,entry,[](int,int){return 0;});EnemyHost enemy(scene,"m141-e2");
    state.setVariable(41,9);need(enemy.receiveSourceDamage(1)&&state.conditions().bleedingTicks==600&&state.conditions().bleedingActive,"Repeated CE12 refreshed CE34");
    state.grant(6,2);need(state.useMedicine(6)==ItemUseResult::Applied&&!state.conditions().bleedingActive&&state.conditions().bleedingTicks==600,"I6 cancelled CE34");
    const int hp=state.hp();state.walkedStep(1,1);need(state.hp()==hp,"Cured State3 still dealt map damage");
    state.setVariable(41,9);need(enemy.receiveSourceDamage(1)&&state.conditions().bleedingTicks==600&&state.conditions().bleedingActive,"Post-cure CE12 refreshed old CE34");state.restoreActor();need(!state.conditions().bleedingActive&&state.conditions().bleedingTicks==600,"FullHeal cancelled CE34");
    std::cout<<"medicine/fullheal passed\n";
    auto common=state.exportState();common.tp=common.maxTp=100;GameSession skill(c.items,common,c.facts);need(skill.castSkill(c.skills.at(9))==SkillUseResult::Applied,"Quick skill rejected");for(int n=0;n<243;++n){skill.tickMapCommonEffects(false);}need(skill.quickshotTicks()==657,"Quick wait setup failed");skill.resetRoomFacts();need(!skill.flag(48)&&skill.quickshotTicks()==657,"CE55 destroyed wait");
    for(int n=0;n<100;++n){skill.tickMapCommonEffects(false);}need(skill.quickshotTicks()==657,"OFF CE60 failed to pause");const auto paused=skill.exportState();GameSession copy(c.items,paused,c.facts);need(copy.castSkill(c.skills.at(9))==SkillUseResult::Applied&&copy.quickshotTicks()==657&&copy.tp()==90,"Copied room recast refreshed wait or skipped TP");copy.tickMapCommonEffects(false);need(copy.quickshotTicks()==656&&skill.quickshotTicks()==657,"Prepared copy changed original");need(copy.castSkill(c.skills.at(9))==SkillUseResult::Applied&&copy.quickshotTicks()==656,"Same-room repeat refreshed wait");
    std::cout<<"same-session quick passed\n";
    disk.session.conditions=state.conditions();disk.session.quickshotTicks=657;disk.session.quickshotActive=false;disk.session.worldValues[c.facts.variables.at(301)]=24;disk.session.worldValues[c.facts.variables.at(308)]=8;const auto encoded=encodeSave(c,disk);const auto current=decodeSave(c,encoded);need(current==disk,"V4 stable values did not roundtrip");
    std::cout<<"v4 roundtrip passed\n";
    auto stable=prepareRestore(c,old);std::cout<<"stable prepared\n";stable.session->setVariable(301,24);stable.session->setVariable(308,8);const auto captured=captureSave(c,*stable.scene,{stable.scene->generation(),old.continuation.point,old.summary.location},old.presentation);need(!captured.session.variables.contains(301)&&captured.session.worldValues.at(c.facts.variables.at(301))==24&&captured.session.worldValues.at(c.facts.variables.at(308))==8,"Legal save capture filtered burn counters");
    std::cout<<"capture passed\n";
    auto on=disk;on.session.quickshotActive=true;need(prepareRestore(c,on).session->quickshotTicks()==900,"Positive ON Load contract changed");
    auto legacy=nlohmann::json::parse(encoded);legacy["schema"]="nobihaza.save.v3";legacy["version"]=3;legacy["session"].erase("common_effects");legacy["session"]["variables"]=nlohmann::json::array();const auto json=nlohmann::json::parse(encoded);for(const auto& row:json["session"]["variables"]){if(row[0].get<int>()<301||row[0].get<int>()>308){legacy["session"]["variables"].push_back(row);}}
    for(auto& row:legacy["session"]["world_values"]){if(row[0]==c.facts.variables.at(301)||row[0]==c.facts.variables.at(308)){row[1]=0;}}
    for(int ticks:{0,600}){legacy["session"]["conditions"][0]=ticks;legacy["session"]["quickshot_ticks"]=ticks?657:0;const auto adapted=decodeSave(c,legacy.dump());auto loaded=prepareRestore(c,adapted);need(loaded.session->conditions().bleedingTicks==ticks&&loaded.session->conditions().bleedingActive==(ticks>0)&&loaded.session->quickshotTicks()==(ticks?900:0)&&loaded.session->saveCount()==disk.session.saveCount,"Known v3 changed old facts or fabricated history");need(loaded.session->variable(301)==0,"Old source fabricated fire history");}
    for(int variation=0;variation<10;++variation){auto bad=nlohmann::json::parse(encoded);auto& s=bad["session"];if(variation==0){s["common_effects"].erase("bleeding_active");}if(variation==1){s["common_effects"]["quickshot_active"]=1;}if(variation==2){s["common_effects"]["unknown"]=false;}if(variation==3){s["common_effects"]["bleeding_active"]=true;s["conditions"][0]=0;}if(variation==4){s["common_effects"]["quickshot_active"]=true;s["quickshot_ticks"]=0;}if(variation==5||variation==6){for(auto& row:s["world_values"]){if(row[0]==c.facts.variables.at(302)){row[1]=variation==5?25:-1;}}}if(variation==7){bad["schema"]="nobihaza.save.v3";bad["version"]=3;}if(variation==8){bad["content"]="post-f5-g1-normal-future";}if(variation==9){s["variables"].push_back({309,1});}rejects([&]{decodeSave(c,bad.dump());});}
    auto bad=legacy;bad["session"]["variables"].push_back({301,1});rejects([&]{decodeSave(c,bad.dump());});rejects([&]{skill.startBleeding(0);});
    need(files.write(1,disk).outcome==WriteOutcome::Committed,"Isolated v4 disk write failed");
    std::cout<<"CE12/I6/FullHeal/old-wait tail/657-copy-recast/strict v3+v4/bounded burn/invalid combinations passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
