#include "local_text_fixtures.h"
#include "combat_scene.h"
#include "game_application.h"
#include "save_snapshot.h"
#include <iostream>
#include <stdexcept>
#include <set>
namespace{
void need(bool value,const std::string& message){if(!value){throw std::runtime_error(message);}}
InputFrame key(InputAction action=InputAction::Confirm){InputFrame f;f.buttons[static_cast<int>(action)].pressed=true;return f;}
CombatEntry entry(const CombatContent& c,int map,TilePosition tile){auto e=c.entries.at("defeated-claimed");e.map=map;e.playerTile=tile;e.direction=Direction::Up;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.hp=e.session.maxHp=500;e.session.baseDefense=999;return e;}
void settle(CombatScene& scene){for(int n=0;n<15000;++n){scene.update(n%2?key():InputFrame{});scene.takePresentation();if(scene.foreground()==ForegroundKind::None&&!scene.dialogue().active()){scene.update({});if(scene.foreground()==ForegroundKind::None){return;}}}throw std::runtime_error("Scene did not settle on map "+std::to_string(scene.mapId()));}
void menu(GameApplication& game,int item){game.update(key(InputAction::Cancel));need(game.mode()==ApplicationMode::Equipment,"Actual app opens menu");game.update(key());for(int n=0;n<100&&game.menuView().rows.at(game.menuView().selection).id!=item;++n){game.update(key(InputAction::Right));}need(game.menuView().rows.at(game.menuView().selection).id==item&&game.menuView().rows.at(game.menuView().selection).enabled,"Chosen color is enabled in actual menu");game.update(key());need(game.mode()==ApplicationMode::Playing,"Menu submits contextual item");}
void settle(GameApplication& app){for(int n=0;n<15000;++n){app.update(n%2?key():InputFrame{});app.takePresentation();if(app.foreground()==ForegroundKind::None&&!app.dialogue().active()){app.update({});if(app.foreground()==ForegroundKind::None){return;}}}throw std::runtime_error("App did not settle");}
}
int main(int argc,char** argv){try{
const bool trapOnly=argc==3&&std::string(argv[2])=="--trap";
const bool roomsOnly=argc==3&&std::string(argv[2])=="--rooms";need(argc==2||trapOnly||roomsOnly,"Usage: truth checks package [--trap|--rooms]");const auto c=loadCombatContent(argv[1]);need(c.fullStage=="truth"||c.fullStage=="evacuation","Reviewed cumulative truth package");
for(int map:{37,101,102,104,138}){need(!c.pendingMapTickets.contains(map),"Current truth map delivered");}need(!c.maps.contains(120),"Forbidden map absent");
if(!trapOnly&&!roomsOnly){
for(bool redFirst:{false,true}){
    auto content=c;auto e=entry(c,37,{10,5});e.session.inventory[89]=e.session.inventory[91]=1;e.session.flags[310]=e.session.flags[316]=e.session.flags[318]=e.session.flags[274]=e.session.flags[213]=e.session.flags[338]=true;content.entries["eyes"]=e;GameApplication app(std::move(content),"eyes");settle(app);
    need(app.session().count(90)==1&&app.session().count(92)==1&&app.session().variable(278)==2,"Source contact converts both independent colors");
    menu(app,redFirst?90:92);settle(app);need(app.mapId()==37&&!app.session().flag(315),"Only one installed eye cannot open chamber");
    need(app.session().flag(redFirst?313:314)&&!app.session().flag(redFirst?314:313)&&app.session().variable(278)==1,"First eye commits once without spending other color");
    menu(app,redFirst?92:90);settle(app);need(app.mapId()==102&&app.session().flag(313)&&app.session().flag(314)&&app.session().flag(315)&&app.session().variable(278)==0,"Either actual menu order opens and completes original chamber presentation");
    need(app.session().count(89)+app.session().count(90)+app.session().count(91)+app.session().count(92)==0,"Exactly two gems consumed");
    auto saved=entry(c,16,{10,21});saved.session=app.session().exportState();GameSession storage(c.items,saved.session,c.facts);CombatScene crow(c,storage,saved);
    const auto snapshot=captureSave(c,crow,{crow.generation(),"m16-e5","BSAA 1F入口"},{});auto restored=prepareRestore(c,decodeSave(c,encodeSave(c,snapshot)));
    need(restored.session->flag(313)&&restored.session->flag(314)&&restored.session->flag(315)&&restored.session->variable(278)==0&&!restored.session->exportState().variables.contains(278),"Legal save/reconstruction preserves two eyes without a writable count");
    auto before=app.session().exportState();for(int n=0;n<60;++n){app.update({});}need(before==app.session().exportState(),"Completed chamber does not replay");
}
{
    auto e=entry(c,37,{10,5});e.session.inventory[89]=1;GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);settle(room);auto r=room.requestToolUse();need(r&&r->item==90,"Single red available");room.cancelToolUse();need(!room.useTool(*r)&&!s.flag(313)&&s.count(90)==1,"Cancelled request leaves inventory intact");auto all=room.requestToolUses();auto forged=all.front();forged.item=92;need(!room.useTool(forged),"Cannot change color in request");need(room.useTool(all.front())&&!room.useTool(all.front()),"Fresh current request consumed once");settle(room);need(s.flag(313)&&!s.flag(315)&&room.mapId()==37&&!room.requestToolUse(),"Single gem stays in current room without another use");
}
{
    auto e=entry(c,37,{10,5});e.session.flags[314]=true;e.session.inventory[89]=1;GameSession s(c.items,e.session,c.facts);CombatScene oldBlue(c,s,e);settle(oldBlue);auto r=oldBlue.requestToolUse();need(r&&r->item==90&&oldBlue.useTool(*r),"Old BC blue eye accepts red through its real highest page7");settle(oldBlue);need(s.flag(315)&&oldBlue.mapId()==102&&!s.count(92),"Old blue is preserved and never awarded twice");
}
for(TilePosition tile:{TilePosition{9,5},TilePosition{10,6},TilePosition{11,5}}){
    auto e=entry(c,37,tile);e.session.inventory[90]=e.session.inventory[92]=1;GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);settle(room);need(s.count(89)==1&&s.count(91)==1&&s.variable(278)==0&&!room.requestToolUse(),"Every exit pad restores both unused gems");
}
{
    auto e=entry(c,37,{10,5});e.session.inventory[89]=1;GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);settle(room);const auto r=room.requestToolUse();room.deactivate();need(r&&!room.useTool(*r)&&!s.flag(313),"Discarded scene cannot consume an old result");for(int n=0;n<300;++n){room.update(key());}need(!s.flag(313),"Retired scene cannot run delayed eye installation");
}
for(const std::string difficulty:{"easy","normal"}){
    auto e=entry(c,39,adjacentTile(c.objects.at("m39-e16").tile,Direction::Down));e.session.difficulty=difficulty;e.session.flags[305]=true;e.session.inventory.erase(71);GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update(key());settle(room);need(room.mapId()==(difficulty=="normal"?138:41),"Original entrance selects difficulty without owning clover key");
}
{
    auto e=entry(c,97,adjacentTile(c.objects.at("m97-e18").tile,Direction::Down));GameSession s(c.items,e.session,c.facts);need(c.activeVariant(c.objects.at("m97-e18"),s)->page==3,"Unconditional higher monitor page covers video reward");CombatScene room(c,s,e);room.update(key());settle(room);need(s.count(85)==0,"Effective faulty monitor never awards unattainable video");
}
std::cout<<"Actual two-color menu orders, rejection, cancellation, old BC blue, exits and invalidation passed\n"<<std::flush;
}
if(!trapOnly){
    auto e=entry(c,102,adjacentTile(c.objects.at("m102-e7").tile,Direction::Up));e.direction=Direction::Down;e.session.flags[315]=true;
    GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update(key());settle(room);
    need(room.mapId()==101&&room.world().player().motion.tile()==TilePosition{9,4},"Transport chamber reaches the real chief room through its source portal");
    e=entry(c,101,adjacentTile(c.objects.at("m101-e5").tile,Direction::Down));e.session=s.exportState();CombatScene back(c,s,e);back.update(key());settle(back);
    need(back.mapId()==102&&back.world().player().motion.tile()==TilePosition{10,22},"Chief room source exit returns to the transport chamber");
    std::cout<<"Transport and chief room source portal round trip passed\n"<<std::flush;
}
if(roomsOnly){return 0;}
{
    auto e=entry(c,101,c.objects.at("m101-e17").tile);e.session.flags[313]=e.session.flags[314]=e.session.flags[315]=true;GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e);scene.update(key());need(scene.foreground()==ForegroundKind::Content,"Computer starts real source trap flow");
    int filePages=0;std::set<std::string> seen,fileMessages;bool factBeforeDrop=false,postFile=false;const auto& reading=c.catalog.at("readings").at("truth-file");
    for(int n=0;n<15000;++n){
        if(scene.dialogue().view().phase==DialoguePhase::AwaitingInput){const auto& lines=scene.dialogue().view().lines;for(const auto& step:reading.at("body")){if(step.at("kind")!="message"){continue;}const auto& raw=c.messages.at(step.at("message").get<std::string>()).message.lines;std::vector<std::u32string> expected;for(const auto& line:raw){std::u32string text;for(const auto& token:line.tokens){if(token.kind==MessageTokenKind::Text){text+=token.text;}}expected.push_back(text);}if(lines==expected){fileMessages.insert(step.at("message"));filePages=static_cast<int>(fileMessages.size());break;}}}
        InputFrame input=n%2?key():InputFrame{};input.buttons[static_cast<int>(InputAction::Cancel)].pressed=n%2;input.movement=Direction::Down;scene.update(input);
        if(scene.foreground()!=ForegroundKind::None){need(!scene.takeApplicationRequest()&&!scene.takeSaveRequest(),"Trap/file cannot expose menu, save or title interruption");}
        for(const auto& event:scene.takePresentation()){if(event.kind==PresentationEvent::Kind::Cue){seen.insert(event.id);}if(event.kind==PresentationEvent::Kind::Sound&&event.sound.resource==c.cue("m101-e17-p1-c75").at("sound").at("resource").get<std::string>()){postFile=true;}}
        factBeforeDrop|=s.flag(319)&&scene.mapId()==101;
        if(scene.mapId()==104&&s.flag(320)&&scene.foreground()==ForegroundKind::None){break;}
    }
    need(filePages==11&&postFile&&factBeforeDrop,"CE157 file reaches eleven source message groups then resumes trap caller; pages="+std::to_string(filePages)+" map="+std::to_string(scene.mapId()));need(seen.contains("f4tr-ce157-c1")&&seen.contains("f4tr-ce157-c67"),"Source file picture opens and restores its caller tone");
    need(scene.mapId()==104&&s.flag(319)&&s.flag(320)&&s.flag(1)&&s.flag(18)&&!s.flag(13),"Trap ends at stable M104 after source waking presentation");
    need(scene.world().player().motion.tile()==TilePosition{10,6},"Original destination is preserved through waking scene");
    bool rejected=false;try{s.setFlag(319,false);}catch(const std::logic_error&){rejected=true;}need(rejected&&s.flag(319),"Committed trap fact cannot revert in the same session");
    auto awake=entry(c,104,{10,6});awake.session=s.exportState();CombatScene rebuilt(c,s,awake);for(int n=0;n<100;++n){rebuilt.update({});}need(rebuilt.foreground()==ForegroundKind::None&&s.flag(319)&&rebuilt.mapId()==104,"Stable scene rebuild cannot repeat or revert trap");
}
std::cout<<"Truth reading and complete trap-to-M104 source presentations passed\n"<<std::flush;
if(!trapOnly){
    need(c.catalog.at("readings").contains("item-185"),"Delivered report has a formal menu reading");
    auto content=c;auto e=entry(c,16,{10,21});e.session.inventory[185]=1;e.session.flags[310]=true;content.entries["report"]=e;GameApplication app(std::move(content),"report");
    const std::vector<std::vector<std::u32string>> expected{
        {decodeMessageText(localSourceText("f4full_truth_checks.0")),std::u32string(5,U'\u3000')+decodeMessageText(localSourceText("f4full_truth_checks.1"))},
        {decodeMessageText(localSourceText("f4full_truth_checks.2")),decodeMessageText(localSourceText("f4full_truth_checks.3")),decodeMessageText(localSourceText("f4full_truth_checks.4")),decodeMessageText(localSourceText("f4full_truth_checks.5"))},
        {decodeMessageText(localSourceText("f4full_truth_checks.6")),decodeMessageText(localSourceText("f4full_truth_checks.7")),decodeMessageText(localSourceText("f4full_truth_checks.8"))},
        {decodeMessageText(localSourceText("f4full_truth_checks.9")),decodeMessageText(localSourceText("f4full_truth_checks.10")),decodeMessageText(localSourceText("f4full_truth_checks.11")),decodeMessageText(localSourceText("f4full_truth_checks.12"))},
        {decodeMessageText(localSourceText("f4full_truth_checks.13")),decodeMessageText(localSourceText("f4full_truth_checks.14")),decodeMessageText(localSourceText("f4full_truth_checks.15"))},
        {decodeMessageText(localSourceText("f4full_truth_checks.16")),decodeMessageText(localSourceText("f4full_truth_checks.17")),decodeMessageText(localSourceText("f4full_truth_checks.18")),std::u32string(8,U'\u3000')+decodeMessageText(localSourceText("f4full_truth_checks.19"))}};
    app.update(key(InputAction::Cancel));const auto before=app.session().exportState();bool opened=false,closed=false;
    for(int repeat=0;repeat<2;++repeat){
        need(app.mode()==ApplicationMode::Equipment&&app.menuView().page==MenuPage::Root,"Report starts or returns to actual root menu");app.update(key());for(int n=0;n<100&&app.menuView().rows.at(app.menuView().selection).id!=185;++n){app.update(key(InputAction::Right));}
        need(app.menuView().rows.at(app.menuView().selection).id==185&&app.menuView().rows.at(app.menuView().selection).enabled,"Report remains selectable");app.update(key());need(app.mode()==ApplicationMode::Reading,"Source report enters actual reading mode");std::vector<std::vector<std::u32string>> pages;
        for(int n=0;n<6000&&app.mode()==ApplicationMode::Reading;++n){
            if(app.dialogue().view().phase==DialoguePhase::AwaitingInput){pages.push_back(app.dialogue().view().lines);app.update(key());}else{app.update({});}
            for(const auto& event:app.takePresentation()){if(event.kind==PresentationEvent::Kind::Cue){const auto& cue=c.cue(event.id);opened|=cue.at("kind")=="show_picture";}}
        }
        need(pages==expected&&app.mode()==ApplicationMode::Equipment&&app.menuView().page==MenuPage::Root,"Report displays six independent source pages and returns to root");need(app.session().exportState()==before&&app.session().count(185)==1,"Reading disposal prose never consumes or grants a report");
    }
    app.update(key(InputAction::Cancel));for(int n=0;n<600&&app.mode()==ApplicationMode::Reading;++n){app.update({});for(const auto& event:app.takePresentation()){if(event.kind==PresentationEvent::Kind::Cue){closed|=c.cue(event.id).at("kind")=="erase_picture";}}}
    need(opened&&closed&&app.mode()==ApplicationMode::Playing&&app.session().count(185)==1,"Closing repeated report erases source file picture and returns to play");std::cout<<"I185 actual menu repeat, all six source pages and picture close passed\n";
}
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
