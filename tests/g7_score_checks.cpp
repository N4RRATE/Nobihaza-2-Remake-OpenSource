#include "combat_content.h"
#include "game_application.h"
#include "save_snapshot.h"
#include "full_content.h"
namespace {void need(bool,const std::string&);}
#include "g4_dora_render.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
namespace {
void need(bool ok,const std::string& s){if(!ok){throw std::runtime_error(s);}}
template<class F> void rejected(F action){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}need(rejected,"Invalid score state accepted");}
std::string bytes(const std::filesystem::path& p){std::ifstream in(p,std::ios::binary);need(bool(in),"Missing private test input");std::ostringstream out;out<<in.rdbuf();return out.str();}
CombatEntry scoreEntry(const CombatContent& c,const EndingScoreInput& in){
    auto e=c.entries.at("post-pickup");e.map=213;e.playerTile={9,7};e.positions.clear();e.directions.clear();e.pages.clear();e.through.clear();
    e.session.party.clear();e.session.flags[1]=false;e.session.flags[2]=false;e.session.flags[18]=false;e.session.difficulty=in.difficultyLevel==3?"hard":in.difficultyLevel==1?"easy":"normal";
    e.session.variables[382]=in.sprayCount;e.session.variables[630]=in.retryCount;e.session.variables[399]=in.saveCount;
    for(const auto [id,n]:{std::pair{390,in.hours},{391,in.minutes},{392,in.seconds},{395,in.guardFirstScore}}){if(c.facts.variables.contains(id)){e.session.worldValues[c.facts.variables.at(id)]=n;}else{e.session.variables[id]=n;}}
    e.session.worldFlags[c.facts.switches.at(584)]=in.specialItemUsed;for(int id=601;id<=605;++id){e.session.worldFlags[c.facts.switches.at(id)]=in.unlocked[id-601];}
    return e;
}
void clock(const CombatContent& c){
    auto e=c.entries.at("post-pickup").session;GameSession s(c.items,e,c.facts);s.setVariable(390,1);s.setVariable(391,59);s.setVariable(392,59);s.setFlag(18,true);
    for(int n=0;n<59;++n){s.tickPlayClock();}need(s.variable(392)==59,"CE3 ticked before its second elapsed");s.tickPlayClock();need(s.variable(390)==2&&s.variable(391)==0&&s.variable(392)==0,"CE3 rollover failed");
    for(int n=0;n<40;++n){s.tickPlayClock();}s.setFlag(18,false);for(int n=0;n<100;++n){s.tickPlayClock();}need(s.variable(392)==0,"S18 OFF counted wall time");s.setFlag(18,true);for(int n=0;n<19;++n){s.tickPlayClock();}need(s.variable(392)==0,"S18 paused wait advanced early");s.tickPlayClock();need(s.variable(392)==1,"S18 resume lost pending wait");
    GameSession load(c.items,s.exportState(),c.facts);for(int n=0;n<60;++n){load.tickPlayClock();}need(load.variable(392)==2,"Load did not preserve completed seconds");
    need(!s.flag(584),"Ordinary I23 must not be special penalty");rejected([&]{s.recordEndingSpecialAction(82);});s.recordEndingSpecialAction(89);need(s.flag(584),"Explicit CE89 writer missing");
    std::cout<<"G7-04 actual Session CE3/S18/rollover/Load wait and bounded S584 writers PASS\n";
}
void sequence(CombatContent c,const EndingScoreInput& in,const std::filesystem::path& root,bool render){
    c.entries["score-check"]=scoreEntry(c,in);GameApplication game(std::move(c),"score-check",[](int lo,int){return lo;});game.configureSaveDirectory(root/"isolated-unused-slots");
    std::unique_ptr<DoraRender> pixels;if(render){pixels=std::make_unique<DoraRender>(game,game.content(),root);pixels->pending=game.takePresentation();pixels->tick();}else{game.takePresentation();}
    const auto expected=endingScore(in);std::vector<int> commits;auto unlocked=in.unlocked;std::vector<std::string> cues;int messages=0,lastMap=213;bool visible=false;
    int n=0;for(;n<10000&&!game.endingClearRequest();++n){
        InputFrame noisy;for(auto action:{InputAction::Confirm,InputAction::Cancel}){noisy.buttons[int(action)]={true,true,false};}game.update(noisy);
        const auto events=game.takePresentation();for(const auto& event:events){if(event.kind==PresentationEvent::Kind::Cue&&event.id.starts_with("m213-e1-p1")){cues.push_back(event.id);}}
        for(int id=601;id<=605;++id){const bool on=game.session().flag(id);if(on&&!unlocked[id-601]){commits.push_back(id);unlocked[id-601]=true;need(game.mapId()==214,"Reward committed before214");}}
        if(game.mapId()==214&&lastMap==213){need(game.world().player().motion.tile()==TilePosition{10,8},"M213 transfer differs from original214(10,8)");need(commits.empty(),"Reward flags precommitted before first message");lastMap=214;}
        if(game.mapId()==214&&game.dialogue().active()){++messages;}
        if(pixels){pixels->pending=events;pixels->tick();if(game.mapId()==213&&std::find(cues.begin(),cues.end(),"m213-e1-p1-c149")!=cues.end()&&!visible){pixels->frame("result-rank-save-time");visible=true;}if(game.mapId()==214&&game.dialogue().view().lines.size()==4&&game.dialogue().view().lines.back().size()>15){pixels->frame("unlock-original-message");}}
        need(game.session().variable(390)==in.hours&&game.session().variable(391)==in.minutes&&game.session().variable(392)==in.seconds,"Scoring presentation continued playtime");
    }
    need(n<10000&&commits==expected.unlocks,"Automatic reward sequence did not submit original ordered flags");need(game.session().variable(381)==expected.rank,"Rank display differs from Session result");need(game.session().exportState().party.empty(),"Score invented a combat party");
    const auto request=game.endingClearRequest();need(request&&request->scene==game.generation()&&request->map==215&&request->tile==TilePosition{9,7},"C98 request is not the real pending destination");
    if(!expected.unlocks.empty()){need(messages>120,"Source timed reward messages were skipped");}
    need(std::find(cues.begin(),cues.end(),"m213-e1-p1-c147")!=cues.end()&&std::find(cues.begin(),cues.end(),"m213-e1-p1-c150")!=cues.end(),"Final picture waits omitted");
    for(int i=0;i<100;++i){game.update({});game.takePresentation();}need(game.mapId()==214&&game.session().variable(381)==expected.rank,"Missing Clear must keep completed rewards and source scene");
    static int caseNumber=0;
    std::ofstream(root/("case-"+std::to_string(++caseNumber)+".json"))<<nlohmann::json{{"kind","isolated-runtime-score-not-natural-ending"},{"steps",n},{"rank",expected.rank},{"commits",commits},{"message_updates",messages},{"clear_success",false},{"cues",cues}}.dump(2)<<'\n';
    std::cout<<"G7-04 source automatic track/ordered timed commits rank="<<expected.rank<<" steps="<<n<<" PASS\n";
}
void preservation(const CombatContent& c){
    auto e=scoreEntry(c,EndingScoreInput{});GameSession session(c.items,e.session,c.facts);bool reject=true;
    CombatScene scene(c,session,e,{},[&](const SceneDefinition& map){if(map.width==20&&map.backgroundPath.filename()=="map-214-background.png"&&reject){throw std::runtime_error("isolated score target failure");}});
    for(int n=0;n<1500&&scene.mapId()==213;++n){scene.update({});scene.takePresentation();}need(scene.mapId()==213&&!scene.transferError().empty(),"Failed reward target published partial world");need(!session.flag(604)&&session.variable(381)==28,"Transfer failure changed uncommitted awards");reject=false;
    for(int n=0;n<3000&&!scene.endingClearRequest();++n){scene.update({});scene.takePresentation();}need(scene.endingClearRequest()&&session.flag(604)&&session.flag(605),"Retry did not finish actual messages");
    e=scoreEntry(c,session.endingScoreInput());e.map=214;CombatScene again(c,session,e);for(int n=0;n<3;++n){again.update({});again.takePresentation();}need(again.endingClearRequest()&&!again.dialogue().active(),"Existing unlocks replayed prompts");again.deactivate();need(!again.endingClearRequest(),"Retired generation exposed Clear request");
    std::cout<<"G7-04 failed target/retry/already-owned replay/retired request PASS\n";
}
void saves(const CombatContent& c,const std::filesystem::path& old){
    const auto original=bytes(old);auto s=decodeSave(c,original);need(s.contentCompatibility==c.compatibility,"Old source not adapted");need(s.session.worldValues.at("score.hours")==0&&s.session.worldValues.at("score.minutes")==0&&s.session.worldValues.at("score.seconds")==0,"Missing history was fabricated");
    const auto oldSave=s.session.saveCount,oldSpray=s.session.variables.at(382),oldRetry=s.session.variables.at(630),oldCount=s.session.variables.at(399);
    s.session.worldValues["score.hours"]=2;s.session.worldValues["score.minutes"]=15;s.session.worldValues["score.seconds"]=59;s.session.worldFlags["score.special-used"]=true;
    const auto raw=encodeSave(c,s);const auto roundtrip=decodeSave(c,raw);need(roundtrip==s,"Actual ordinary score facts roundtrip differs");
    need(roundtrip.session.saveCount==oldSave&&roundtrip.session.variables.at(382)==oldSpray&&roundtrip.session.variables.at(630)==oldRetry&&roundtrip.session.variables.at(399)==oldCount,"Adaptation altered existing statistics");
    for(const auto& key:{"score.hours","score.minutes","score.seconds","score.rank"}){auto bad=s;bad.session.worldValues.erase(key);rejected([&]{validateSave(c,bad);});}
    for(const auto [key,value]:{std::pair{"score.minutes",60},std::pair{"score.seconds",-1},std::pair{"score.hours",INT_MAX},std::pair{"score.rank",28}}){auto bad=s;bad.session.worldValues[key]=value;rejected([&]{validateSave(c,bad);});}
    auto bad=s;bad.session.worldFlags["score.unlocked-605"]=true;rejected([&]{validateSave(c,bad);});
    auto forged=nlohmann::json::parse(original);forged["session"]["world_values"].push_back({"score.hours",123});rejected([&]{decodeSave(c,forged.dump());});
    need(bytes(old)==original,"Old isolated input was modified");std::cout<<"G7-04 strict old identity/time-default/actual ordinary roundtrip/unknown and malformed rejection PASS\n";
}
}
int main(int argc,char** argv){try{need(argc==4,"usage: g7_score_checks content output old-slot");auto c=loadCombatContent(argv[1]);const std::filesystem::path root=argv[2];std::filesystem::create_directories(root);clock(c);saves(c,argv[3]);preservation(c);
    EndingScoreInput in;in.guardFirstScore=40;sequence(c,in,root,true);in.seconds=1;in.hours=1;sequence(c,in,root,false);in.hours=2;in.minutes=15;sequence(c,in,root,false);in={};in.saveCount=7;sequence(c,in,root,false);in.saveCount=100;sequence(c,in,root,false);in.saveCount=370;sequence(c,in,root,false);in={};in.specialItemUsed=true;sequence(c,in,root,false);in={};in.difficultyLevel=1;sequence(c,in,root,false);in.difficultyLevel=3;in.unlocked.fill(true);sequence(c,in,root,false);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
