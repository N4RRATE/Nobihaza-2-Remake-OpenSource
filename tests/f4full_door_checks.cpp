#include "combat_scene.h"
#include "enemy_controller.h"
#include "full_content.h"
#include "save_snapshot.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void need(bool value,const char* message){if(!value){throw std::runtime_error(message);}}
InputFrame key(InputAction action=InputAction::Confirm){InputFrame input;input.buttons[static_cast<int>(action)].pressed=true;return input;}
CombatEntry fixture(const CombatContent& content,const std::string& object,const SessionEntry& state){
    auto entry=content.entries.at("defeated-claimed");const auto& door=content.objects.at(object);
    entry.map=door.map;entry.playerTile={door.tile.x,door.tile.y+1};entry.direction=Direction::Up;
    entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();entry.session=state;return entry;
}
void finish(CombatScene& scene,int choice=0){
    std::uint64_t request{};int firstLine{};
    for(int n=0;n<5000;++n){
        if(scene.foreground()==ForegroundKind::None&&!scene.dialogue().active()&&!scene.world().player().motion.moving()){return;}
        auto input=n%12==0?key():InputFrame{};const auto view=scene.inputPromptView();
        if(view.cursor){if(request!=view.request){request=view.request;firstLine=view.cursor->line;}
            input=key(view.cursor->line==firstLine+choice?InputAction::Confirm:InputAction::Down);}
        scene.update(input);
    }
    throw std::runtime_error("Door foreground did not finish");
}
void waitForQuestion(CombatScene& scene){
    for(int n=0;n<5000;++n){if(scene.inputPromptView().cursor){return;}scene.update(n%12==0?key():InputFrame{});}
    throw std::runtime_error("Source last-use question missing");
}
void enter(CombatScene& scene,int map){
    for(int n=0;n<120&&scene.mapId()!=map;++n){InputFrame input;input.movement=Direction::Up;scene.update(input);}
    if(scene.mapId()!=map){scene.update(key());}finish(scene);need(scene.mapId()==map,"Unlocked real door transfers to its source room");
}
}
int main(int argc,char** argv){try{
    need(argc==2||argc==3,"usage: f4full_door_checks full-package [natural-pre-fuse-save]");const auto content=loadCombatContent(argv[1]);
    if(argc==3&&std::string(argv[2])=="--screenshot-door"){
        for(int scenario=0;scenario<5;++scenario){
            auto state=content.entries.at("defeated-claimed").session;
            state.flags[305]=true;state.flags[174]=scenario>=3;state.flags[175]=scenario==1||scenario==2;
            state.flags[170]=scenario==4;state.flags[1]=state.flags[18]=true;
            state.inventory[71]=1;state.inventory.erase(73);if(scenario==1||scenario==2){state.inventory[73]=1;}
            const auto entry=fixture(content,"m138-e2",state);GameSession session(content.items,state,content.facts);CombatScene door(content,session,entry);
            const auto* page=content.activeVariant(content.objects.at("m138-e2"),session);
            std::cout<<"Screenshot M138 E2 (27,4) page "<<(page?page->page:0)<<"; Confirm/Z scenario "<<scenario<<"\n"<<std::flush;
            door.update(key());
            if(scenario==0){
                for(int n=0;n<1000&&!door.dialogue().active();++n){door.update({});}
                need(door.dialogue().active(),"Locked screenshot door displays its real source refusal message");
            }
            if(scenario==1||scenario==2){waitForQuestion(door);door.update(key(InputAction::Cancel));need(!session.flag(174)&&session.count(73)==1,"Last-use cancel cannot unlock or discard");}
            finish(door,scenario==2?1:0);
            if(scenario==0){need(!session.flag(174)&&door.mapId()==138&&session.count(71)==1,"Missing diamond key reports refusal without using club key");door.update(key());finish(door);continue;}
            if(scenario==1||scenario==2){
                need(session.flag(174)&&session.count(73)==(scenario==1?0:1),"Both real last-use answers unlock exactly once");
                const auto events=door.takePresentation();
                need(std::count_if(events.begin(),events.end(),[](const auto& e){return e.kind==PresentationEvent::Kind::SystemGraphic;})==2,"Last-use question restores system graphic");
                door.update(key());finish(door);
            }
            need(door.mapId()==45&&session.flag(174)&&session.flag(1)&&session.flag(18),"Public Z enters source room and restores control");
            need(session.variable(91)==(scenario==4?9:0),"Only real S170 branch primes source licker mode");
            const auto events=door.takePresentation();need(std::any_of(events.begin(),events.end(),[](const auto& e){return e.kind==PresentationEvent::Kind::Sound;}),"Source door sound is emitted");
            auto back=fixture(content,"m45-e6",session.exportState());CombatScene returning(content,session,back);returning.update(key());finish(returning);
            need(returning.mapId()==138&&session.flag(174)&&session.count(73)==(scenario==2?1:0),"Actual source return preserves unlocked door and chosen key state");
            auto revisit=fixture(content,"m138-e2",session.exportState());CombatScene reopened(content,session,revisit);reopened.update(key());finish(reopened);need(reopened.mapId()==45,"Revisited unlocked screenshot door accepts Z again");
        }
        for(bool active:{false,true}){
            auto state=content.entries.at("defeated-claimed").session;state.flags[305]=true;state.flags[170]=active;state.flags[169]=true;
            const auto entry=fixture(content,"m138-e4",state);GameSession session(content.items,state,content.facts);CombatScene wooden(content,session,entry);
            wooden.update(key());finish(wooden);need(wooden.mapId()==46,"Other real old-zone mode caller transfers through central wooden door");
            need(session.variable(91)==(active?9:0),"Central door respects source S170 branch in another room");
        }
        auto bad=content;auto state=content.entries.at("defeated-claimed").session;state.flags[174]=state.flags[305]=true;
        const auto transfer=std::find_if(content.transitions.begin(),content.transitions.end(),[](const auto& link){return link.at("object")=="m138-e2"&&link.at("page")==4;});
        need(transfer!=content.transitions.end(),"Screenshot door has an actual public transfer flow");
        const auto flow=transfer.key();
        for(bool missingCue:{false,true}){
            auto& steps=bad.catalog["flows"][flow]["steps"];
            if(missingCue){steps[2]["cue"]="missing-door-resource";}else{steps[6]["then"][0]["field"]="unknown_licker_mode";}
            const auto entry=fixture(bad,"m138-e2",state);GameSession session(bad.items,state,bad.facts);CombatScene rejected(bad,session,entry);const auto before=session.exportState();
            bool failed=false;try{rejected.update(key());}catch(const std::exception&){failed=true;}
            const auto after=session.exportState();
            need(failed&&rejected.mapId()==138&&session.flag(174)&&session.flag(1)==state.flags[1]&&session.flag(18)==state.flags[18]&&after.inventory==before.inventory&&session.variable(91)==0,"Invalid field or missing cue rejects before flags, inventory or transfer commit");
            steps=content.catalog.at("flows").at(flow).at("steps");
        }
        std::cout<<"Screenshot door refusal, last-use answers, source transfer/return, central variant and rejection checks passed\n";return 0;
    }
    {
        std::ifstream input(argv[1]);const auto source=nlohmann::json::parse(input);
        for(bool supplement:{false,true}){
            auto changed=source;
            if(supplement){changed["f4full"]["runtime"]["door_closure"]["unknown"]=true;}
            else{changed["f4full"]["runtime"]["flows"]["ce100"]["steps"][0]["kind"]="unknown";}
            bool rejected=false;try{loadFullContent(argv[1],changed);}catch(const std::runtime_error& error){
                rejected=std::string(error.what()).find(supplement?"Unreviewed original door closure":"Unreviewed training projection")!=std::string::npos;
            }
            need(rejected,"Loader rejects unknown supplement and still checks the inherited projection");
        }
    }
    const std::set<int> naturalMaps{40,41,45,51,52,53,54,55,56,58,59,60,61};
    for(const auto& [id,link]:content.transitions.items()){
        const auto& object=content.objects.at(link.at("object").get<std::string>());
        if(!naturalMaps.contains(object.map)||!link.value("runtime_enabled",true)){continue;}
        need(link.contains("flow")||content.catalog.at("flows").contains(id),"Released return link has a reviewed finite consumer");
    }
    if(argc==3&&std::string(argv[2])=="--dormant"){
        for(bool active:{false,true}){for(bool dead:{false,true}){
            auto state=content.entries.at("defeated-claimed").session;state.flags[180]=active;state.flags[166]=false;state.flags[187]=dead;
            const auto entry=fixture(content,"m27-e2",state);GameSession session(content.items,state,content.facts);CombatScene scene(content,session,entry);
            scene.update(key());finish(scene);need(scene.mapId()==26,"Actual deferred door now consumes its reviewed licker state");
            const auto states=scene.enemyStates();const auto actor=std::find_if(states.begin(),states.end(),[](const auto& e){return e.id=="m26-e4";});
            need(actor!=states.end(),"Dormant licker has a real controller");
            need(dead?actor->mode==EnemyMode::Corpse:active?actor->sourceState==9&&actor->mode!=EnemyMode::Dormant:actor->mode==EnemyMode::Dormant,"Source entry keeps inactive, ceiling-drop and dead states distinct");
            need(!session.flag(166),"Entering via S180 does not invent the old S166 activation fact");
        }}
        std::ifstream input(argv[1]);auto changed=nlohmann::json::parse(input);changed["f4full"]["runtime"]["dormant_licker"]["unknown"]=true;
        bool rejected=false;try{loadFullContent(argv[1],changed);}catch(const std::runtime_error& e){rejected=std::string(e.what()).find("Unreviewed dormant licker binding")!=std::string::npos;}need(rejected,"Unknown dormant binding is rejected before stripping");
        std::cout<<"Dormant source entry, inactive/dead variation and unknown profile rejection passed\n";return 0;
    }
    if(argc==3){
        std::ifstream input(argv[2]);const std::string bytes{std::istreambuf_iterator<char>(input),{}};const auto original=decodeSave(content,bytes);
        auto cleared=original;GameSession state(content.items,cleared.session,content.facts);
        for(int fact:{188,189,190,191,192}){state.setFlag(fact,true);}state.setFlag(202,true);state.grant(78,1);cleared.session=state.exportState();
        const auto encoded=encodeSave(content,cleared);need(decodeSave(content,encoded).session==cleared.session,"Source fuse clearance survives save and Load without activation injection");
        if(content.expansion.enemies.contains("m26-e4")){
            auto dormant=original;GameSession activated(content.items,dormant.session,content.facts);activated.setFlag(187,true);activated.setFlag(166,false);activated.setFlag(180,true);dormant.session=activated.exportState();
            need(decodeSave(content,encodeSave(content,dormant)).session==dormant.session,"Reviewed S180 entry permits its licker death save without inventing S166");
            activated.setFlag(180,false);dormant.session=activated.exportState();bool rejected=false;try{encodeSave(content,dormant);}catch(const std::runtime_error&){rejected=true;}need(rejected,"Licker death without either real source activation stays rejected");
        }
        for(int variation=0;variation<3;++variation){auto invalid=cleared;GameSession changed(content.items,invalid.session,content.facts);
            if(variation==0){changed.setFlag(202,false);}else if(variation==1){changed.setFlag(188,false);}else{changed.setFlag(119,true);changed.setFlag(88,false);}
            invalid.session=changed.exportState();bool rejected=false;try{encodeSave(content,invalid);}catch(const std::runtime_error&){rejected=true;}need(rejected,"No pickup, incomplete clearance and unrelated unactivated canine stay rejected");
        }
    }
    // Explicit domain fixtures; these checks are separate from the natural-route evidence.
    for(bool discard:{false,true}){
        auto state=content.entries.at("defeated-claimed").session;state.flags[171]=false;state.inventory.erase(80);
        auto entry=fixture(content,"m14-e21",state);GameSession locked(content.items,state,content.facts);CombatScene refusal(content,locked,entry);
        refusal.update(key());finish(refusal);need(!locked.flag(171)&&refusal.mapId()==14,"Missing hot-water key preserves locked room");
        state.inventory[80]=1;entry=fixture(content,"m14-e21",state);GameSession owned(content.items,state,content.facts);CombatScene opening(content,owned,entry);
        opening.update(key());waitForQuestion(opening);opening.update(key(InputAction::Cancel));
        need(!owned.flag(171)&&owned.count(80)==1,"Uncancellable key question cannot commit on X");finish(opening,discard?0:1);
        need(owned.flag(171)&&owned.count(80)==(discard?0:1)&&owned.flag(1)&&owned.flag(18),"Real hot-water door commits answer and restores control");enter(opening,59);
    }
    for(bool reverse:{false,true}){for(bool discard:{false,true}){
        auto state=content.entries.at("defeated-claimed").session;state.inventory[73]=1;state.flags[174]=false;state.flags[175]=false;
        for(int pass=0;pass<2;++pass){
            const bool diamond=(pass==0)!=reverse;const std::string object=diamond?"m29-e10":"m41-e2";const int own=diamond?175:174;
            auto entry=fixture(content,object,state);GameSession session(content.items,state,content.facts);CombatScene door(content,session,entry);
            door.update(key());
            if(pass==1){waitForQuestion(door);door.update(key(InputAction::Cancel));need(!session.flag(own)&&session.count(73)==1,"Second diamond door rejects X without consuming key");}
            finish(door,discard?0:1);need(session.flag(own)&&session.count(73)==(pass==0||!discard?1:0),"Diamond key is offered only after its other door is open");
            enter(door,diamond?42:45);state=session.exportState();
        }
    }}
    auto state=content.entries.at("defeated-claimed").session;auto entry=fixture(content,"m40-e10",state);
    GameSession returning(content.items,state,content.facts);CombatScene north(content,returning,entry);
    north.update(key());finish(north);need(north.mapId()==60,"North partition of M40 has its actual return to M60");
    state.flags[305]=false;entry=fixture(content,"m45-e6",state);
    GameSession flashlightSession(content.items,state,content.facts);CombatScene flashlight(content,flashlightSession,entry);
    flashlight.update(key());finish(flashlight);need(flashlight.mapId()==41,"Flashlight room returns by its source M45E6P2 door");
    std::cout<<"Original hot-water/diamond doors and both source room returns passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
