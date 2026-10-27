#include "game_application.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <stdexcept>
void require(bool b,const char* message){if(!b){throw std::runtime_error(message);}}
int main(int argc,char** argv){
    if(argc!=2){return 2;}auto content=loadCombatContent(argv[1]);
    require(content.maps.at(12).width==40&&content.maps.at(16).height==40,"real non-room dimensions");
    for(int side=0;side<2;++side){
        auto c=content;auto e=c.entries.at("defeated-claimed");e.session.flags[55]=true;e.map=6;e.pages.clear();e.positions.clear();e.through.clear();e.directions.clear();e.playerTile={20+side,3};e.direction=Direction::Up;c.entries["transfer-fixture"]=e;
        GameApplication game(std::move(c),"transfer-fixture");
        const auto generation=game.generation();for(int i=0;i<240&&game.generation()==generation;++i){game.update({});}
        require(game.definition().width==20&&game.world().player().motion.tile()==TilePosition{4+side,7},"distinct Map6 exits land in Map9");
        for(int i=0;i<180;++i){game.update({});}
        require(game.session().flag(52)&&game.session().magazine(11)==e.session.magazines.at(11),"persistent facts survive transfer");
        InputFrame down;down.movement=Direction::Down;for(int i=0;i<16;++i){game.update(down);}
        for(int i=0;i<180;++i){game.update({});}
        require(game.definition().width==25&&game.world().player().motion.tile()==TilePosition{20+side,6},"Map9 returns through matching reviewed exit");
    }
    // Representative variation: IDs and coordinates are content, not shared transfer rules.
    auto changed=content;auto entry=changed.entries.at("defeated-claimed");
    entry.session.flags[55]=true;entry.map=6;entry.pages.clear();entry.positions.clear();entry.through.clear();entry.directions.clear();entry.playerTile={9,7};entry.direction=Direction::Left;
    auto object=changed.objects.at("m6-e13");changed.objects.erase("m6-e13");object.id="alternate-door";object.tile={9,7};
    for(auto& v:object.variants){v.definition.id=object.id;v.definition.tileX=9;v.definition.tileY=7;}
    changed.objects.emplace(object.id,object);changed.transitions["m6-next-left"]["object"]=object.id;
    entry.session.flags[159]=true;entry.session.inventory[158]=1;changed.entries["variation"]=entry;
    GameApplication variation(changed,"variation");for(int i=0;i<160;++i){variation.update({});}
    require(variation.definition().width==20&&variation.world().player().motion.facing()==Direction::Left,"changed object identity/location preserves facing");
    require(variation.session().count(158)==0&&!variation.session().flag(159),"transition retires temporary aiming aid before clearing its owner flag");
    // An actual application resource-preparation failure must leave the live model intact.
    GameApplication failed(changed,"variation",{},[](const SceneDefinition& candidate){if(candidate.width==20){throw std::runtime_error("simulated texture preparation failure");}});
    const auto before=failed.generation();failed.update({});
    require(failed.generation()==before&&failed.definition().width==25&&failed.world().player().motion.tile()==TilePosition{9,7},"failed candidate retains map and position");
    require(failed.session().flag(1)&&failed.session().flag(18)&&failed.session().magazine(11)==entry.session.magazines.at(11),"failure precedes control locks and room resets");
    require(failed.foreground()==ForegroundKind::None,"failure leaves foreground unclaimed");
    GameSession state(content.items,entry.session);state.setFlag(48,true);state.setVariable(89,80);state.setVariable(35,7);state.setVariable(41,3);state.startBleeding(1000);
    state.resetRoomFacts();require(!state.flag(48)&&state.variable(89)==0&&state.variable(35)==7&&state.variable(41)==3&&state.conditions().bleedingTicks==1000,"CE55 clears transient work and preserves statistics and bleeding");
    require(!state.knowsSkill(3)&&!state.knowsSkill(4),"fresh state has no combine seed");state.replaceKnownSkill(4,3);require(!state.knowsSkill(3),"conditional conversion does not teach fresh combine");
    auto seeded=entry.session;seeded.skills.insert(4);GameSession learned(content.items,seeded);learned.replaceKnownSkill(4,3);require(learned.knowsSkill(3)&&!learned.knowsSkill(4),"known combine variant switches");
    auto boundary=content;auto edge=entry;edge.session.flags[55]=true;edge.map=9;edge.playerTile=adjacentTile(boundary.objects.at("m9-e6").tile,Direction::Left);edge.direction=Direction::Right;
    boundary.entries["unimplemented-flow"]=edge;GameApplication next(boundary,"unimplemented-flow");InputFrame confirm;confirm.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;next.update(confirm);
    for(int n=0;n<240;++n){next.update({});}require(next.mode()==ApplicationMode::Playing&&next.definition().width==28&&next.session().flag(1),"reviewed door presentation transfers to Map10 and restores control");
    const auto folder=std::filesystem::temp_directory_path()/("nobi-content-check-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(folder);
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::filesystem::remove_all(path);}}cleanup{folder};
    for(const auto& f:std::filesystem::directory_iterator(content.directory)){
        if(f.path().filename()!="f3.json"){std::filesystem::create_symlink(std::filesystem::absolute(f.path()),folder/f.path().filename());}
    }
    std::ifstream input(argv[1]);const auto original=nlohmann::json::parse(input);
    for(int fault=0;fault<9;++fault){
        auto j=original;
        if(fault==0){j.erase("f3");}
        if(fault==1){j["transitions"]["m6-next-left"]["destination"]["tile"]={999,0};}
        if(fault==2){j["transitions"]["m6-next-left"]["flow"][2]["cue"]="missing";}
        if(fault==3){j["transitions"]["m6-next-left"]["flow"][0]["kind"]="arbitrary_script";}
        if(fault==4){j["f3"]["enemies"].push_back(j["f3"]["enemies"].at(0));}
        if(fault==5){j["f3"]["flows"]["m14-e4-p2"]["steps"][0]["kind"]="arbitrary_script";}
        if(fault==6){j["f3"]["ladders"][0]["helpers"]["up"]["object"]="missing";}
        if(fault==7){j["f3"]["save_points"][0]["page"]=999;}
        if(fault==8){j["f3"]["flows"]["m14-e4-p2"]["steps"][4]["cue"]="missing";}
        {std::ofstream output(folder/"input.json");output<<j;}
        bool rejected=false;try{loadCombatContent(folder/"input.json");}catch(const std::exception&){rejected=true;}
        require(rejected,"malformed F3 schema or transfer must fail at read boundary");
    }
    std::cout<<"F3 transfer checks passed\n";
}
