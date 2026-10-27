#include "local_text_fixtures.h"
#include "combat_content.h"
#include "combat_scene.h"
#include "reading_controller.h"
#include "menu_controller.h"
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* reason){if(!ok){throw std::runtime_error(reason);}}
void realReadings(CombatContent& content){
    InputFrame confirm;confirm.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;
    InputFrame right;right.buttons[static_cast<int>(InputAction::Right)].pressed=true;
    auto entry=content.entries.at("natural").session;entry.inventory.clear();entry.inventory[183]=1;entry.inventory[184]=1;
    GameSession session(content.items,entry,content.facts);session.setVariable(388,1234);
    const std::vector<std::vector<std::u32string>> passwordPages{
        {decodeMessageText(localSourceText("f4full_message_checks.0")),decodeMessageText(localSourceText("f4full_message_checks.1"))},
        {decodeMessageText(localSourceText("f4full_message_checks.2")),decodeMessageText(localSourceText("f4full_message_checks.3")),decodeMessageText(localSourceText("f4full_message_checks.4"))},
        {decodeMessageText(localSourceText("f4full_message_checks.5")),decodeMessageText(localSourceText("f4full_message_checks.6")),decodeMessageText(localSourceText("f4full_message_checks.7")),decodeMessageText(localSourceText("f4full_message_checks.8"))},
        {decodeMessageText(localSourceText("f4full_message_checks.9")),decodeMessageText(localSourceText("f4full_message_checks.10")),decodeMessageText(localSourceText("f4full_message_checks.11")),decodeMessageText(localSourceText("f4full_message_checks.12"))}};
    const std::vector<std::vector<std::u32string>> medicalPages{
        {decodeMessageText(localSourceText("f4full_message_checks.13")),decodeMessageText(localSourceText("f4full_message_checks.14"))},
        {decodeMessageText(localSourceText("f4full_message_checks.15")),decodeMessageText(localSourceText("f4full_message_checks.16")),decodeMessageText(localSourceText("f4full_message_checks.17")),decodeMessageText(localSourceText("f4full_message_checks.18"))},
        {decodeMessageText(localSourceText("f4full_message_checks.19")),decodeMessageText(localSourceText("f4full_message_checks.20")),decodeMessageText(localSourceText("f4full_message_checks.21")),decodeMessageText(localSourceText("f4full_message_checks.22"))},
        {decodeMessageText(localSourceText("f4full_message_checks.23")),decodeMessageText(localSourceText("f4full_message_checks.24")),decodeMessageText(localSourceText("f4full_message_checks.25")),decodeMessageText(localSourceText("f4full_message_checks.26"))},
        {decodeMessageText(localSourceText("f4full_message_checks.27")),decodeMessageText(localSourceText("f4full_message_checks.28")),decodeMessageText(localSourceText("f4full_message_checks.29"))}};
    for(int item:{183,184}){for(int repeat=0;repeat<2;++repeat){
        MenuController menu(content,session);menu.update(confirm);
        for(int n=0;n<2&&menu.view().rows.at(menu.view().selection).id!=item;++n){menu.update(right);}
        require(menu.view().rows.at(menu.view().selection).id==item&&menu.view().rows.at(menu.view().selection).enabled,"Real file is selectable when held");
        menu.update(confirm);const auto request=menu.takeResult();require(request&&request->kind==MenuResult::Kind::Read&&request->item==item,"Real menu requests the selected file");
        const auto before=session.exportState();const std::string id="item-"+std::to_string(item);
        ReadingController reading(content,session,id,31);std::vector<std::vector<std::u32string>> pages;
        for(int tick=0;tick<2000&&!reading.atMenu();++tick){
            if(reading.dialogue().view().phase==DialoguePhase::AwaitingInput){pages.push_back(reading.dialogue().view().lines);reading.update(confirm,31);}
            else{reading.update({},31);}
        }
        require(reading.atMenu()&&pages==(item==183?passwordPages:medicalPages),"All original file pages, blank lines and bound password are displayed");
        menu.returnFromReading();require(menu.view().page==MenuPage::Root,"Reading returns to the menu root");
        reading.close();for(int n=0;n<100&&!reading.finished();++n){reading.update({},31);}
        require(reading.finished()&&reading.restore()->display==MessageDisplay{false,2,true,true},"Closing restores original message options");
        bool opened=false,erased=false;for(const auto& e:reading.takePresentation()){
            if(e.kind==PresentationEvent::Kind::Cue){const auto& cue=content.cue(e.id);opened|=cue.at("kind")=="show_picture";erased|=cue.at("kind")=="erase_picture";}
        }
        require(opened&&erased&&session.exportState()==before,"Real picture lifecycle completes without consuming files or changing session state");
    }}
}
int main(int argc,char** argv){try{
    if(argc!=2&&argc!=3){return 2;}auto content=loadCombatContent(argv[1]);
    if(argc==3){require(std::string(argv[2])=="--readings","Unknown check option");
        require(content.catalog.at("readings").contains("item-183")&&content.catalog.at("readings").contains("item-184"),"Reviewed real files must reach the reading catalog");realReadings(content);}
    // CN1.00 CE154 C24, resolved from the current session when each page opens.
    nlohmann::json source={{"options","inherit"},{"face","inherit"},{"lines",nlohmann::json::array({
        {{"raw","\\>No.『\\V[388]』"},{"tokens",nlohmann::json::array({
            {{"type","instant_on"}},{{"type","text"},{"text","No.『"}},
            {{"type","variable"},{"value",388}},{{"type","text"},{"text","』"}}})}}})}};
    bool rejected=false;try{readCombatMessage(source);}catch(const std::exception&){rejected=true;}
    require(rejected,"Existing message contracts keep rejecting variable controls");
    auto wrong=source;wrong["lines"][0]["tokens"][2]["value"]=390;
    rejected=false;try{readCombatMessage(wrong,true);}catch(const std::exception&){rejected=true;}
    require(rejected,"Full messages reject unreviewed variable IDs");
    content.messages["binding-domain"]=readCombatMessage(source,true);
    require(content.messages.at("binding-domain").message.lines.front().raw=="\\>No.『\\V[388]』","Original control text is preserved");
    // CE154 C2 establishes the inherited, fixed middle message window.
    content.setMediaDefinition("cues","binding-domain-options",{{"kind","message_options"},
        {"value",{{"transparent",true},{"position",1},{"fixed",true},{"continue_events",false}}}});
    content.catalog["readings"]["binding-domain"]={{"open",nlohmann::json::array({{{"kind","cue"},{"cue","binding-domain-options"}}})},
        {"body",nlohmann::json::array({{{"kind","message"},{"message","binding-domain"}}})},
        {"return_to_menu",nlohmann::json::array()},{"close",nlohmann::json::array()}};
    auto entry=content.entries.at("defeated-claimed");GameSession session(content.items,entry.session,content.facts);
    for(int value:{0,1234,9999}){
        session.setVariable(388,value);ReadingController reading(content,session,"binding-domain",17);
        for(int n=0;n<25;++n){reading.update({},17);}
        require(reading.dialogue().view().lines==std::vector<std::u32string>{decodeMessageText(localSourceText("f4full_message_checks.30"))+decodeMessageText(std::to_string(value))+decodeMessageText(localSourceText("f4full_message_checks.31"))},"Repeated reading uses current password without changing it");
        require(session.variable(388)==value,"Reading never writes the password");
        reading.update({},18);require(reading.finished(),"A replaced scene invalidates the reading before accessing its session");
    }
    entry.map=6;entry.playerTile={10,8};entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();
    entry.session.flags[156]=false;entry.session.variables[388]=5678;
    content.objects.at("m6-e4").variants.front().flags[156]=false;
    content.catalog["flows"]["binding-domain"]={{"object","m6-e4"},{"page",1},{"trigger","autorun"},
        {"steps",nlohmann::json::array({{{"kind","cue"},{"cue","binding-domain-options"}},{{"kind","message"},{"message","binding-domain"}}})}};
    GameSession speaking(content.items,entry.session,content.facts);speaking.setVariable(388,5678);CombatScene scene(content,speaking,entry);
    for(int n=0;n<25;++n){scene.update({});}
    require(scene.dialogue().view().lines==std::vector<std::u32string>{decodeMessageText(localSourceText("f4full_message_checks.32"))},"Scene requests use the same session binding as file reading");
    speaking.setVariable(388,1111);scene.update({});
    require(scene.dialogue().view().lines==std::vector<std::u32string>{decodeMessageText(localSourceText("f4full_message_checks.33"))},"An open page keeps its request snapshot");
    scene.deactivate();require(!scene.dialogue().active(),"Destroying a scene retires its bound page");
    std::cout<<"Full message binding checks passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
