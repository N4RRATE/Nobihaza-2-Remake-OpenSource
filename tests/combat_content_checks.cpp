#include "combat_content.h"
#include "full_content.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <chrono>
#include <stdexcept>
int main(int argc,char** argv){
    if(argc!=2){return 2;}
    for(const char* bytes:{R"({"a":1,"\u0061":2})",R"([{"a":[{}, {"key":1,"key":2}]}])","{\"a\":{\"id\":1,\"id\":2}}","{\"a\":[{\"id\":1,\"id\":2}]}","{\"a\":1} trailing","{\"a\":\"unterminated}"}){
        std::istringstream input(bytes);bool rejected=false;try{parseCombatContentDefinition(input);}catch(const std::exception&){rejected=true;}
        if(!rejected){throw std::runtime_error("strict parser accepted duplicate or malformed JSON");}
    }
    for(const char* bytes:{"null","true","false","0","-42","18446744073709551615","2.5","-0.0",R"("escaped\n\u4e2d")",R"({"a":[],"b":{},"c":[true,0,"text"]})"}){
        std::istringstream input(bytes);const auto parsed=parseCombatContentDefinition(input),expected=nlohmann::json::parse(bytes);
        if(parsed.type()!=expected.type()||parsed.dump()!=expected.dump()){throw std::runtime_error("strict parser changed a JSON value type");}
    }
    std::istringstream valid("{\"a\":[{\"id\":1},{\"id\":2}],\"b\":[true,null,2.5,\"text\"]}");
    if(parseCombatContentDefinition(valid)!=nlohmann::json::parse("{\"a\":[{\"id\":1},{\"id\":2}],\"b\":[true,null,2.5,\"text\"]}")){throw std::runtime_error("strict parser changed values or key scope");}
    std::istringstream positioned("prefix {\"value\":17}");positioned.seekg(7);
    if(parseCombatContentDefinition(positioned).at("value")!=17){throw std::runtime_error("strict parser changed stream position");}
    struct NonSeeking : std::stringbuf{
        NonSeeking():std::stringbuf("{\"value\":23}"){}
        pos_type seekoff(off_type,std::ios_base::seekdir,std::ios_base::openmode) override{return pos_type(off_type(-1));}
        pos_type seekpos(pos_type,std::ios_base::openmode) override{return pos_type(off_type(-1));}
    } buffer;
    std::istream nonSeeking(&buffer);
    if(parseCombatContentDefinition(nonSeeking).at("value")!=23){throw std::runtime_error("strict parser requires rewind");}
    const auto data=loadCombatContent(argv[1]);
    const auto& e=data.entries.at("natural");
    GameSession session(data.items,e.session);
    const auto scene=data.makeScene(e.map,e.playerTile,e.direction,session,&e);
    if(scene.width!=20||scene.height!=15||scene.sprites->size()!=33||data.items.at(11).capacity!=15){throw std::runtime_error("real content shape");}
    if(data.maps.at(6).width!=25||e.session.inventory.contains(11)||e.session.magazines.at(11)!=15){throw std::runtime_error("natural boundary");}
    if(data.messages.at("m4-e10-p2-c4").message.lines[0].tokens.size()!=6){throw std::runtime_error("full colored message");}
    if(data.routes.at("m4-e10-p2-c10").steps.size()!=9){throw std::runtime_error("complete forced route");}
    const auto temp=std::filesystem::temp_directory_path()/("nobihaza-content-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(temp);
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::filesystem::remove_all(path);}} cleanup{temp};
    for(const auto& file:std::filesystem::directory_iterator(data.directory)){
        if(file.path().filename()!="combat.json"){std::filesystem::create_symlink(std::filesystem::absolute(file.path()),temp/file.path().filename());}
    }
    std::ifstream original(argv[1]);const auto json=nlohmann::json::parse(original);
    auto rejects=[&](auto mutate,const char* why){
        auto changed=json;mutate(changed);{std::ofstream output(temp/"combat.json");output<<changed;}
        bool rejected=false;try{loadCombatContent(temp/"combat.json");}catch(const std::exception&){rejected=true;}
        if(!rejected){throw std::runtime_error(why);}
    };
    rejects([](auto& j){j["entries"]["natural"]["player"]["tile"]={99,0};},"reject out-of-bounds entry");
    rejects([](auto& j){j["sprites"]["player-0"]["frames"][0]["rect_px"]={999,0,24,32};},"reject out-of-bounds frame");
    rejects([](auto& j){j["cues"]["ce61-c3"]["sprite"]="missing";},"reject missing sprite reference");
    rejects([](auto& j){j["cues"]["ce61-c2"]["sound"]["resource"]="missing";},"reject missing sound reference");
    for(const auto* key:{"image","resource","sprite"}){
        rejects([&](auto& j){j["cues"]["ce61-c3"]["prepared_reference"]=nlohmann::json::array({{{key,"missing-nested-cue-reference"}}});},"reject missing nested cue reference");
    }
    rejects([](auto& j){j["messages"]["m4-e10-p2-c4"]["lines"][0]["tokens"][0]["type"]="unknown";},"reject unsupported message token");
    rejects([](auto& j){j["entries"]["natural"]["equipment"]["main_weapon"]=31;},"reject invalid equipped entry");
    rejects([](auto& j){for(const auto& [id,cue]:j["cues"].items()){if(cue.contains("runtime_support")){j["action_profiles"]["fire"]["cue_order"][0]=id;return;}}},"reject action referencing evidence-only route");
    rejects([](auto& j){j["rules"]["enemy"]["runtime_profile"]["feeding"][0]["route"]="ce61-c2";},"reject sound in route slot");
    rejects([](auto& j){j["rules"]["enemy"]["runtime_profile"]["retreat_page"]=999;},"reject missing retreat page before play");
    rejects([](auto& j){j["rules"]["enemy"]["runtime_profile"]["grab_release_wait"]="ce61-c2";},"reject sound used as grab release wait");
    rejects([](auto& j){j["rules"]["enemy"]["runtime_profile"]["retreat_page"]=15.5;},"reject fractional retreat page");
    rejects([](auto& j){j["cues"]["m4-e4-p13-c83"].erase("tenths");},"reject missing release wait duration");
    rejects([](auto& j){j["cues"]["m4-e4-p13-c83"]["tenths"]=-1;},"reject negative release wait duration");
    rejects([](auto& j){j["cues"]["m4-e4-p13-c83"]["tenths"]=3.5;},"reject fractional release wait duration");
    {std::ofstream output(temp/"combat.json");output<<"{\"schema\":\"nobihaza.combat.v1\","<<json.dump().substr(1);}
    bool duplicateRejected=false;
    try{loadCombatContent(temp/"combat.json");}catch(const std::exception& e){duplicateRejected=std::string(e.what()).find("Duplicate content JSON key")!=std::string::npos;}
    if(!duplicateRejected){throw std::runtime_error("reject duplicate content key before definition validation");}
    std::cout<<"combat content checks passed\n";
}
