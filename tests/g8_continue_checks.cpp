#include "combat_content.h"
#include "combat_scene.h"
#include "full_content.h"
#include "g8_continue_natural_driver.h"
#include <iostream>
#include <stdexcept>

namespace {
void need(bool ok,const std::string& message){if(!ok){throw std::runtime_error(message);}}
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}need(rejected,"Invalid Continue accepted");}
SessionEntry fixture(const CombatContent& c,const std::string& difficulty){
    auto e=c.entries.at("post-pickup").session;e.difficulty=difficulty;e.hp=46;e.tp=17;e.saveCount=12;
    e.mainWeapon=13;e.auxiliaryWeapon=27;e.accessory=55;e.magazines[13]=3;e.magazines[27]=-2;
    e.inventory[13]=2;e.inventory[27]=1;e.inventory[55]=1;e.inventory[8]=2;e.itemUses[8]=1;
    e.conditions={};e.commonEffectsKnown=true;return e;
}
void domain(const CombatContent& c){
    for(const auto& difficulty:{"easy","normal","hard"}){for(int map:{218,219,220}){
        const auto& checkpoint=continueCheckpoint(c,map);const auto& xy=checkpoint.at("tile");const TilePosition tile{xy.at(0),xy.at(1)};
        for(int history:{0,2,3}){for(bool poison:{false,true}){for(bool bleed:{false,true}){
            auto e=fixture(c,difficulty);e.conditions.poisoned=poison;e.conditions.bleedingActive=bleed;e.conditions.bleedingTicks=bleed?360000:0;
            GameSession s(c.items,e,c.facts);s.setVariable(272,history);s.setVariable(630,2);s.setVariable(399,9);
            s.setFlag(130,true);s.setVariable(382,4);s.captureContinueCheckpoint(c.magazineSeedBindings,map,tile);
            const auto backup=s.exportState();const int retained=bleed?3:poison?2:history;
            need(s.variable(272)==retained&&s.variable(273)==-2&&s.variable(274)==3,"Capture changed condition history or LC9 slot");
            s.captureContinueCheckpoint(c.magazineSeedBindings,map==218?219:218,{0,0});need(s.exportState()==backup,"Duplicate capture replaced active checkpoint");
            s.discard(8,1);s.consumeRound();s.loseHp(1000);s.setVariable(382,5);s.setFlag(15,true);s.setFlag(45,true);
            const auto dead=s.exportState();auto candidate=s.prepareContinueRestore(c.magazineSeedBindings,map,tile);
            need(s.exportState()==dead,"Continue preparation published partial state");s.commitContinueRestore(candidate);
            need(s.difficulty()==difficulty&&s.variable(280)==(std::string(difficulty)=="easy"?1:std::string(difficulty)=="hard"?3:2),"Continue reset difficulty");
            need(s.hp()==46&&s.tp()==17&&s.equipped(1)==13&&s.equipped(2)==27&&s.equipped(3)==e.armor&&s.equipped(4)==55&&s.equipped(5)==0,"Continue changed vital/equipment domain");
            need(s.magazine(13)==3&&s.magazine(27)==-2&&s.count(13)==1&&s.count(8)==2&&!s.exportState().itemUses.contains(8),"Continue changed inventory/magazine domain");
            need(s.conditions().poisoned==(retained==2)&&s.conditions().bleedingActive==(retained==3),"Continue changed original single condition history");
            need(s.flag(130)&&s.variable(382)==5&&s.variable(630)==3&&s.saveCount()==12&&s.variable(399)==9&&!s.flag(15)&&!s.flag(45),"Continue rolled back world facts or changed save counters");
        }}}
        std::cout<<"CONTINUE_DOMAIN "<<difficulty<<" map="<<map<<" condition cases=12 PASS\n";
        GameSession precision(c.items,fixture(c,difficulty),c.facts);precision.setFlag(20,true);auto before=precision.exportState();
        rejects([&]{precision.captureContinueCheckpoint(c.magazineSeedBindings,map,tile);});need(precision.exportState()==before,"Rejected precision capture changed live state");
        GameSession s(c.items,fixture(c,difficulty),c.facts);rejects([&]{s.prepareContinueRestore(c.magazineSeedBindings,map,tile);});
        s.captureContinueCheckpoint(c.magazineSeedBindings,map,tile);
        for(int field:{272,273,274,283,284,405}){auto damaged=s;damaged.setVariable(field,field==273?-1000000:9999);before=damaged.exportState();rejects([&]{damaged.prepareContinueRestore(c.magazineSeedBindings,map,tile);});need(damaged.exportState()==before,"Rejected Continue changed live state");}
        auto signedOther=s;signedOther.setVariable(291,-1);rejects([&]{signedOther.prepareContinueRestore(c.magazineSeedBindings,map,tile);});
    }}
}
InputFrame key(InputAction action=InputAction::Confirm){InputFrame input;input.buttons[static_cast<int>(action)]={true,true,false};return input;}
void scenes(const CombatContent& c){
    // Controlled live CE38 requests and real retry flows; capture is a fixture, not natural arrival.
    for(const auto& difficulty:{"easy","normal","hard"}){for(int map:{218,219,220}){for(int branch=0;branch<4;++branch){
        const auto& checkpoint=continueCheckpoint(c,map);const auto& xy=checkpoint.at("tile");const TilePosition tile{xy.at(0),xy.at(1)};
        auto entry=c.entries.at("post-pickup");entry.map=16;entry.playerTile={10,21};entry.direction=Direction::Up;
        entry.positions.clear();entry.pages.clear();entry.directions.clear();entry.through.clear();entry.session=fixture(c,difficulty);
        GameSession s(c.items,entry.session,c.facts);s.setFlag(1,true);s.setFlag(18,true);s.setVariable(630,2);s.setVariable(399,9);
        s.captureContinueCheckpoint(c.magazineSeedBindings,map,tile);s.setVariable(382,5);
        CombatScene scene(c,s,entry,[](int lo,int){return lo;});s.loseHp(1000);
        for(int n=0;n<6000&&!scene.inputPromptView().cursor;++n){scene.update(n%8==0?key():InputFrame{});scene.takePresentation();}
        need(scene.inputPromptView().cursor.has_value(),"Controlled Continue prompt missing: "+std::string(difficulty)+"/"+std::to_string(map));
        const PromptIdentity old{scene.generation(),scene.inputPromptView().request};
        int prepared=0;
        if(branch>=2){scene.configurePresentation([&](const SceneDefinition&){if(++prepared==branch-1){throw std::runtime_error("G8 controlled Continue resource failure");}});}
        if(branch==1){scene.update(key(InputAction::Down));scene.update({});}
        const auto dead=s.exportState();scene.update(key());bool title=false,viaCheckpoint=false,returned=false;
        for(int n=0;n<6000;++n){
            scene.update(n%8==0?key():InputFrame{});scene.takePresentation();viaCheckpoint|=scene.mapId()==map;
            title|=scene.takeApplicationRequest()==ApplicationMode::Title;
            returned=viaCheckpoint&&scene.mapId()==checkpoint.at("retry").at("map")&&scene.foreground()==ForegroundKind::None;
            if(title||!scene.transferError().empty()||returned){break;}
        }
        if(branch==0){
            need(returned&&s.hp()==46&&s.tp()==17&&s.variable(630)==3&&s.magazine(13)==3&&s.magazine(27)==-2,"CE38 Yes did not commit original backup once");
            need(s.difficulty()==difficulty&&s.saveCount()==12&&s.variable(399)==9&&s.variable(382)==5,"CE38 Yes reset identity or save/world counters");
            need(!scene.takeInputResult(old)&&!scene.cancelInput(old),"Committed Continue accepted an old request");
            for(int n=0;n<8;++n){scene.update(key());scene.takePresentation();}need(s.variable(630)==3,"Repeated Confirm committed Continue twice");
        }else if(branch==1){need(title&&s.hp()==0&&s.variable(630)==2&&s.saveCount()==12&&s.variable(399)==9,"CE38 No restored state or changed counters");}
        else{need(prepared==branch-1&&!scene.transferError().empty()&&s.exportState()==dead,"Failed Continue preparation published partial state");}
        need(!scene.takeSaveRequest(),"Continue issued a disk save request");scene.deactivate();const auto retired=s.exportState();
        need(!scene.takeInputResult(old)&&!scene.cancelInput(old),"Retired Continue accepted an old request");
        for(int n=0;n<20;++n){scene.update(key());}need(s.exportState()==retired,"Retired Continue mutated the session");
        std::cout<<"CONTINUE_SCENE "<<difficulty<<" map="<<map<<" branch="<<branch<<" PASS\n";
    }}}
    for(const auto& difficulty:{"easy","normal","hard"}){
        auto entry=c.entries.at("post-pickup");entry.map=16;entry.playerTile={10,21};entry.positions.clear();entry.pages.clear();entry.directions.clear();entry.through.clear();entry.session=fixture(c,difficulty);
        GameSession s(c.items,entry.session,c.facts);s.setFlag(1,true);s.setFlag(18,true);s.setVariable(630,2);s.setVariable(399,9);
        CombatScene scene(c,s,entry,[](int lo,int){return lo;});s.loseHp(1000);bool title=false;
        for(int n=0;n<6000&&!title;++n){scene.update(n%8==0?key():InputFrame{});scene.takePresentation();need(!scene.inputPromptView().cursor,"No-checkpoint death offered Continue");title=scene.takeApplicationRequest()==ApplicationMode::Title;}
        need(title&&s.hp()==0&&s.variable(630)==2&&s.saveCount()==12&&s.variable(399)==9&&!scene.takeSaveRequest(),"No-checkpoint death changed save/retry counters");
        std::cout<<"CONTINUE_NO_CHECKPOINT "<<difficulty<<" PASS\n";
    }
}
}
int main(int argc,char** argv){try{
    std::cout.setf(std::ios::unitbuf);g0natural::need(argc>=2,"Expected package [domain|scenes|natural-N source output ...|natural checkpoint source output ...] [--seed=N]");
    std::uint32_t seed=19202026;
    if(std::string(argv[argc-1]).starts_with("--seed=")){
        const auto value=std::string(argv[argc-1]).substr(7);std::size_t used=0;const auto parsed=std::stoul(value,&used);
        g0natural::need(used==value.size()&&parsed<=0xffffffffULL,"Invalid natural random seed");seed=parsed;--argc;
    }
    g0natural::need(argc>=2,"Missing content package");const auto content=loadCombatContent(argv[1]);
    const std::string mode=argc>=3?argv[2]:"domain";
    if(mode=="domain"){g0natural::need(argc<=3,"Unexpected domain arguments");domain(content);}
    else if(mode=="scenes"){g0natural::need(argc==3,"Unexpected Scene arguments");scenes(content);}
    else if(mode=="natural"){
        g0natural::need(argc>=6&&(argc-3)%3==0,"Expected checkpoint/source/output triples");
        for(int n=3;n<argc;n+=3){const std::string id=argv[n];const int checkpoint=id=="218"?218:id=="219"?219:id=="220"?220:0;g0natural::need(checkpoint!=0,"Unknown natural checkpoint");g8continue::natural(content,checkpoint,argv[n+1],argv[n+2],seed);}
    }else{
        const int checkpoint=mode=="natural-218"?218:mode=="natural-219"?219:mode=="natural-220"?220:0;
        g0natural::need(checkpoint!=0&&argc>=5&&(argc-3)%2==0,"Expected natural-N source/output pairs");
        for(int n=3;n<argc;n+=2){g8continue::natural(content,checkpoint,argv[n],argv[n+1],seed);}
    }return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
