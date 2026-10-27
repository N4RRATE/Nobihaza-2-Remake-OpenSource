#include "combat_scene.h"
#include <iostream>
#include <stdexcept>
using Json=nlohmann::json;
void require(bool ok,const char* message){if(!ok){throw std::runtime_error(message);}}
InputFrame press(InputAction action=InputAction::Confirm){InputFrame input;input.buttons[static_cast<int>(action)].pressed=true;return input;}
int main(int argc,char** argv){try{
    if(argc!=2){return 2;}const auto original=loadCombatContent(argv[1]);
    // CN1.00 M41E2P3 C10/24 and M29E10P3 C10/15: last-use question only at the second door.
    for(bool reverse:{false,true}){for(bool discard:{false,true}){
        auto content=original;auto entry=content.entries.at("defeated-claimed");entry.map=6;entry.playerTile={10,8};
        entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();entry.session.inventory[73]=1;
        GameSession session(content.items,entry.session,content.facts);session.setFlag(174,false);session.setFlag(175,false);
        for(int pass=0;pass<2;++pass){
            const int own=(reverse!=(pass!=0))?175:174,other=own==174?175:174;
            const std::string actor=pass==0?"m6-e4":"m6-e1";
            content.objects.at(actor).variants.front().flags[own]=false;
            content.catalog["flows"]["last-key-domain"]={{"object",actor},{"page",1},{"trigger","autorun"},
                {"steps",Json::array({{{"kind","if_flag"},{"flag",other},{"steps",Json::array({
                    {{"kind","discard_key"},{"item",73},{"choice","f3-ce100-c4"}}})}},
                    {{"kind","flag"},{"id",own},{"value",true}}})}};
            CombatScene room(content,session,entry);room.update({});
            if(pass==0){
                require(session.flag(own)&&session.count(73)==1,"The first door opens without discarding its still-needed key");
                require(room.inputPromptView().phase==DialoguePhase::Hidden,"The first door has no last-use question");
            }else{
                require(!session.flag(own)&&room.inputPromptView().phase!=DialoguePhase::Hidden,"The second door waits for the last-use answer before unlocking");
                room.update(press(InputAction::Cancel));
                require(!session.flag(own)&&session.count(73)==1,"Uncancellable original key question commits nothing on X");
                if(!discard){room.update(press(InputAction::Down));}room.update(press());room.update({});
                require(session.flag(174)&&session.flag(175)&&session.count(73)==(discard?0:1),"Either key answer unlocks both doors in either visitation order");
                room.update(press());require(session.count(73)==(discard?0:1),"Repeated confirmation cannot discard another key");
            }
        }
        auto failed=original;failed.catalog["flows"]["invalid-conditional-domain"]={{"object","m6-e4"},{"page",1},{"trigger","autorun"},
            {"steps",Json::array({{{"kind","flag"},{"id",174},{"value",true}},
                {{"kind","if_flag"},{"flag",175},{"steps",Json::array({{{"kind","erase_event_once"},{"object","m4-e1"}}})}}})}};
        GameSession untouched(failed.items,entry.session,failed.facts);untouched.setFlag(174,false);untouched.setFlag(175,false);
        CombatScene invalid(failed,untouched,entry);bool rejected=false;try{invalid.update({});}catch(const std::runtime_error&){rejected=true;}
        require(rejected&&!untouched.flag(174),"Nested conditional resources are checked before any state is committed, even on the skipped branch");
    }}
    std::cout<<"Conditional last-use key flow checks passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
