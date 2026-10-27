#include "save_files.h"
#include "enemy_controller.h"
#include "save_controller.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
void need(bool ok,const std::string& why){if(!ok){throw std::runtime_error(why);}}
InputFrame confirm(){InputFrame f;f.buttons.at(static_cast<int>(InputAction::Confirm)).pressed=true;return f;}
SaveSnapshot read(const CombatContent& c,const char* path){std::ifstream f(path);std::string bytes{std::istreambuf_iterator<char>(f),{}};return decodeSave(c,bytes);}
CombatEntry at(const CombatContent& c,int map,TilePosition tile){auto e=c.entries.at("defeated-claimed");e.map=map;e.playerTile=tile;e.direction=Direction::Up;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();return e;}
}
int main(int argc,char** argv){try{
    if(argc!=6){return 2;}const auto content=loadCombatContent(argv[1]);const std::string mode=argv[5];
    auto source=read(content,(mode=="initial"||mode=="parking"||mode=="repeated-fuse")?argv[2]:argv[3]);auto restored=prepareRestore(content,source);auto& session=*restored.session;
    if(mode=="parking"){
        need(!session.flag(236),"Natural first-visit source is unpowered");
        auto e=at(content,67,{5,34});e.direction=Direction::Left;CombatScene scene(content,session,e);
        scene.update(confirm());for(int n=0;n<6000&&scene.foreground()!=ForegroundKind::None;++n){scene.update(n%12==0?confirm():InputFrame{});}
        need(scene.mapId()==70&&scene.world().player().motion.tile()==TilePosition{22,12},"Public input enters original unpowered parking room");
        InputFrame face;face.movement=Direction::Right;scene.update(face);need(!scene.world().player().motion.moving(),"Source return door blocks its adjacent tile");
        scene.update(confirm());for(int n=0;n<6000&&scene.foreground()!=ForegroundKind::None;++n){scene.update(n%12==0?confirm():InputFrame{});}
        need(scene.mapId()==67&&scene.world().player().motion.tile()==TilePosition{5,34}&&session.flag(1)&&session.flag(18),"Unpowered public return door resumes source map and control");
        need(!session.flag(236)&&!session.flag(238)&&!session.flag(239),"Return does not publish power or guard completion");
        std::cout<<"Natural-source unpowered M67 to M70 to M67 public door input passed\n";return 0;
    }
    if(mode=="repeated-fuse"){
        const auto finish=[](CombatScene& room){for(int n=0;n<6000&&room.foreground()!=ForegroundKind::None;++n){room.update(n%12==0?confirm():InputFrame{});}need(room.foreground()==ForegroundKind::None,"Original source flow settles");};
        const auto pickup=[&]{const auto tile=content.objects.at("m76-e5").tile;auto e=at(content,76,{tile.x,tile.y+1});CombatScene room(content,session,e);room.update(confirm());finish(room);need(session.flag(227)&&session.count(78)>0,"Original fuse pickup grants item and activates pursuit");};
        const auto leave=[&]{auto e=at(content,67,{18,19});CombatScene room(content,session,e);room.update(confirm());finish(room);need(room.mapId()==69&&!session.flag(227),"Original power-room entrance clears live pursuit");};
        pickup();need(!session.flag(228)&&!session.flag(236),"First source pickup has no proximity reveal or power");
        auto e=at(content,77,{8,8});CombatScene fight(content,session,e);
        for(int n=0;n<200;++n){fight.update({});const auto states=fight.enemyStates();const auto target=std::find_if(states.begin(),states.end(),[](const auto& value){return value.id=="m77-e4";});if(target!=states.end()&&target->mode!=EnemyMode::Dormant){break;}}
        for(int n=0;n<600;++n){const auto p=findCharacter(fight.world(),"m77-e4").motion.position();std::set<std::string> resolved;fight.resolveAttack({{p.x*16+8,p.y*16-5},{0,0},AttackKind::GlAcid,6,0,false},resolved);const auto states=fight.enemyStates();const auto target=std::find_if(states.begin(),states.end(),[](const auto& value){return value.id=="m77-e4";});if(target!=states.end()&&target->damage>=500){break;}}
        fight.update({});finish(fight);need(session.flag(226)&&session.flag(585)&&!session.flag(228),"Actual M77 defeat commits permanent S226 before proximity S228");
        leave();pickup();need(session.flag(226)&&!session.flag(585)&&!session.flag(228),"Repeated original pickup resets only save stop, retaining permanent defeat");leave();
    }else if(mode=="initial"){
        // A legal first-visit variation: ordinary M68 E4 dies before the fuse is touched.
        auto c=content;const auto definition=c.enemies.at("m68-e4");c.enemies.clear();c.enemies.emplace(definition.id,definition);
        for(auto& flow:c.catalog.at("flows")){if(flow.value("trigger",std::string{})=="autorun"){flow["trigger"]="call-only";}}
        auto e=at(c,68,{10,10});e.positions[definition.id]={6,6};session.setFlag(202,false);
        CombatScene fight(c,session,e);auto enemy=makeEnemyBehavior(fight,definition);enemy->update({});
        for(int n=0;n<6000&&!session.flag(202);++n){const auto p=findCharacter(fight.world(),definition.id).motion.position();enemy->receiveAttack({{(p.x+.5)*16,(p.y+.5)*16},{0,0},AttackKind::Handgun,500});enemy->update({});}
        need(session.flag(202)&&session.count(78)==0&&!session.flag(237),"Actual ordinary enemy kill publishes S202 before fuse pickup");
    }else{
        // CN1.00 M67 E10 P2 C8 clears temporary pursuit while permanent death survives.
        auto e=at(content,67,{18,19});CombatScene door(content,session,e);
        door.update(confirm());for(int n=0;n<6000&&door.foreground()!=ForegroundKind::None;++n){door.update(n%12==0?confirm():InputFrame{});}
        need(door.mapId()==69&&!session.flag(227),"Actual M67 E10 P2 clears live pursuit while entering power room");need(session.flag(226)&&session.flag(585)&&session.flag(237),"Natural powered source retains permanent defeat/installed facts");
    }
    const auto crow=content.objects.at("m77-e6").tile;auto e=at(content,77,{crow.x,crow.y+1});CombatScene scene(content,session,e);
    scene.update(confirm());const auto request=scene.takeSaveRequest();need(bool(request),"Original M77 source page produces save request");
    const auto snapshot=captureSave(content,scene,*request,{});
    SaveFiles files(content,argv[4]);need(files.write(1,snapshot).outcome==WriteOutcome::Committed,"Legal state commits in isolated directory");
    auto disk=files.read(1);auto loaded=prepareRestore(content,disk);
    need(loaded.scene->mapId()==77&&loaded.session->flag(202)==session.flag(202)&&loaded.session->flag(226)==session.flag(226)&&loaded.session->flag(227)==session.flag(227),"Load preserves ordinary death and temporary/permanent pursuit facts");
    for(int variation=0;variation<4;++variation){
        auto bad=snapshot;GameSession state(content.items,bad.session,content.facts);
        if(variation==0){state.grant(78,1);state.setFlag(188,false);}
        if(variation==1){state.setFlag(202,false);state.grant(78,1);}
        if(variation==2){state.setFlag(119,true);state.setFlag(88,false);}
        if(variation==3){state.setFlag(158,true);state.setFlag(156,false);}
        bad.session=state.exportState();bool rejected=false;try{validateSave(content,bad);}catch(const std::exception&){rejected=true;}
        need(rejected,"Rejected-state variation failed: "+std::to_string(variation));
    }
    if(mode=="initial"){
        auto bad=snapshot;GameSession state(content.items,bad.session,content.facts);
        state.setFlag(226,true);state.setFlag(585,true);for(int fact:{188,189,190,191,192}){state.setFlag(fact,true);}bad.session=state.exportState();
        bool rejected=false;try{validateSave(content,bad);}catch(const std::exception&){rejected=true;}
        need(rejected,"Underground defeat plus stop cannot invent fuse acquisition");
    }
    const auto current=session.exportState();
    SaveFiles failed(content,argv[4],[](std::string_view stage){if(stage=="begin"){throw std::runtime_error("injected before-write failure");}});
    need(failed.write(1,snapshot).outcome==WriteOutcome::Failed&&files.read(1)==snapshot&&session.exportState()==current,"Rejected before-write preserves original isolated slot and live session");
    SaveController cancelled(content,session,scene,files,*request,{});
    for(int n=0;n<600&&!cancelled.promptView().cursor;++n){cancelled.update({});}
    need(bool(cancelled.promptView().cursor),"Legal source opens original save choice");
    cancelled.update(confirm());for(int n=0;n<200&&!cancelled.fileView();++n){cancelled.update({});}
    need(bool(cancelled.fileView()),"Original affirmative choice opens cancellable slots");
    InputFrame cancel;cancel.buttons.at(static_cast<int>(InputAction::Cancel)).pressed=true;cancelled.update(cancel);
    need(cancelled.finished(),"Cancelling slots finishes the source save flow");
    auto resumed=current;resumed.flags[1]=true;resumed.flags[18]=true;
    need(session.exportState()==resumed&&files.read(1)==snapshot,"Cancelling save restores control and preserves current facts and prior isolated slot");
    std::cout<<mode<<" natural-source M77 capture, isolated write/read and restore passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
