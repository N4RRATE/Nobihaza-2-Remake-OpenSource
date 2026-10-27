#include "game_application.h"
#include "enemy_controller.h"
#include <algorithm>
#include <iostream>
#include <queue>
#include <stdexcept>
namespace{
void need(bool ok,const std::string& why){if(!ok){throw std::runtime_error(why);}}
InputFrame key(InputAction action){InputFrame f;f.buttons[static_cast<int>(action)]={true,true,false};return f;}
CombatEntry entry(const CombatContent& c,int map,TilePosition tile){
    auto e=c.entries.at("defeated-claimed");e.map=map;e.playerTile=tile;e.direction=Direction::Up;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();
    for(int flag:{10,13,15,21,22,37,45,160,199,201,202,226,227,228,235,236,237,275,276,277,585,587}){e.session.flags[flag]=false;}
    e.session.flags[1]=e.session.flags[18]=e.session.flags[200]=true;e.session.pursuerHealth=500;e.session.eventTimer={};
    e.session.hp=e.session.maxHp=5000;return e;
}
void finish(CombatScene& scene,bool decline=false){
    for(int n=0;n<20000&&scene.foreground()!=ForegroundKind::None;++n){scene.update(key(decline&&scene.inputPromptView().phase!=DialoguePhase::Hidden?InputAction::Cancel:InputAction::Confirm));scene.takePresentation();}
    need(scene.foreground()==ForegroundKind::None,"Finite source presentation stuck in map "+std::to_string(scene.mapId()));
}
void install(CombatScene& scene,bool decline=false){scene.update({});const auto r=scene.requestToolUse();need(r&&scene.useTool(*r),"Current source tool request installs fuse");finish(scene,decline);}
void defeat(CombatScene& scene,const std::string& id){
    for(int n=0;n<200;++n){scene.update({});const auto states=scene.enemyStates();auto e=std::find_if(states.begin(),states.end(),[&](const auto& e){return e.id==id;});
        if(e!=states.end()&&e->mode!=EnemyMode::Dormant){break;}}
    for(int n=0;n<600;++n){const auto p=findCharacter(scene.world(),id).motion.position();std::set<std::string> resolved;
        const auto hit=scene.resolveAttack({{p.x*16+8,p.y*16-5},{0,0},AttackKind::GlAcid,6,0,false},resolved);
        const auto states=scene.enemyStates();const auto e=std::find_if(states.begin(),states.end(),[&](const auto& e){return e.id==id;});
        need(e!=states.end(),"Installed pursuer exists");if(e->damage>=500){return;}need(hit.processed,"Original pursuer accepts finite impact");}
    throw std::runtime_error("Pursuer never reached original500 threshold");
}
std::optional<Direction> next(const CombatScene& scene,TilePosition target){
    const auto& w=scene.world();const auto start=w.player().motion.tile();std::queue<TilePosition> q;q.push(start);std::map<std::pair<int,int>,Direction> first;first[{start.x,start.y}]=Direction::Up;
    while(!q.empty()){const auto pos=q.front();q.pop();auto actor=w.player();actor.motion=CharacterMotion(pos,Direction::Down,32);
        for(int n=0;n<4;++n){const auto d=static_cast<Direction>(n);const auto tile=adjacentTile(pos,d);const auto k=std::pair{tile.x,tile.y};
            if(first.contains(k)||!w.canMove(actor,d)){continue;}first[k]=pos==start?d:first.at({pos.x,pos.y});if(tile==target){return first[k];}q.push(tile);}}
    return {};
}
void walk(CombatScene& scene,TilePosition target){
    const int map=scene.mapId();
    for(int n=0;n<12000;++n){if(scene.mapId()!=map){finish(scene);return;}if(scene.foreground()!=ForegroundKind::None){finish(scene);continue;}if(scene.world().player().motion.moving()){scene.update({});continue;}
        if(scene.world().player().motion.tile()==target){return;}InputFrame f;f.movement=next(scene,target);need(bool(f.movement),"No route to "+std::to_string(target.x)+","+std::to_string(target.y)+" in "+std::to_string(scene.mapId()));scene.update(f);scene.takePresentation();}
    throw std::runtime_error("Walking did not settle");
}
void interact(CombatScene& scene,const CombatContent& c,const std::string& id,int destination){
    const int origin=scene.mapId();const auto& actor=findCharacter(scene.world(),id);const auto object=actor.motion.tile();const auto from=scene.world().player().motion.tile();
    const auto* page=c.activeVariant(c.objects.at(id),scene.session());need(page,"Object has a current page");
    if(page->definition.layer!=1){walk(scene,object);}else{
        bool arrived=false;for(int n=0;n<4&&!arrived;++n){const auto d=static_cast<Direction>(n);const auto target=adjacentTile(object,d);
            if(from!=target&&!next(scene,target)){continue;}walk(scene,target);if(scene.mapId()!=origin){need(scene.mapId()==destination,"Touch transfer reaches expected destination");return;}InputFrame turn;turn.movement=static_cast<Direction>((n+2)%4);turn.buttons[static_cast<int>(InputAction::Cancel)].held=true;scene.update(turn);arrived=true;}
        need(arrived,"No adjacent interaction route to "+id);
    }
    if(scene.mapId()!=origin){need(scene.mapId()==destination,"Contact transfer reaches expected destination");return;}scene.update(key(InputAction::Confirm));finish(scene);need(scene.mapId()==destination,"Wrong destination after "+id+" got "+std::to_string(scene.mapId()));
}
}
int main(int argc,char** argv){try{
    if(argc!=2){return 2;}const auto c=loadCombatContent(argv[1]);
    // Integration fixtures start after the accepted underground prerequisites. Extended player HP only bounds incidental damage; this is not NORMAL playtest acceptance.
    for(bool beforeDefeat:{false,true}){
        auto e=entry(c,69,c.objects.at("m69-e12").tile);e.session.flags[226]=beforeDefeat;e.session.inventory[78]=1;
        GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e);install(scene,true);
        need(s.flag(237)&&!s.flag(236)&&s.count(78)==0&&s.count(79)==0&&!s.eventTimer().running,"Decline commits installed fuse, consumes it and does not start power");
        scene.deactivate();auto retry=entry(c,69,{7,6});CombatScene returned(c,s,retry);returned.update(key(InputAction::Confirm));finish(returned);
        need(beforeDefeat?s.flag(236)&&!s.eventTimer().running:s.flag(227)&&s.eventTimer().running,"Rebuilt scene retries without another fuse and respects prior defeat");
        if(beforeDefeat){need(!s.flag(200)&&s.count(83)==0,"Already defeated power path clears darkness without replaying reward");}
    }
    std::cout<<"Fuse decline/retry and prior defeat passed\n"<<std::flush;
    // Exact end-frame fixture uses the public impact receiver; the source E9 completion runs before E10 timeout, including its timer-dependent branch.
    for(bool expire:{false,true}){
        auto e=entry(c,69,{7,6});e.session.flags[227]=e.session.flags[237]=true;e.session.eventTimer={EventTimerSource::Power,600,true,false,false};
        GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,[](int,int hi){return hi;});defeat(scene,"m69-e9");
        if(expire){s.setEventTimer(EventTimerSource::Power,1);for(int n=0;n<59;++n){s.tickEventTimer();}}
        scene.takePresentation();scene.update({});auto events=scene.takePresentation();
        need(std::any_of(events.begin(),events.end(),[](const auto& e){return e.id=="m69-e9-p17-c4";}),"Defeat completion wins the same update as timer expiration");
        finish(scene);need(s.flag(236)&&s.flag(226)&&s.flag(585)&&!s.flag(200)&&!s.eventTimer().running&&s.count(83)==1,"Defeat completion powers room and grants its single card");
        for(int n=0;n<20;++n){scene.update({});}need(s.count(83)==1,"Higher completion page prevents repeated card grant");
        scene.deactivate();auto r=entry(c,69,{7,6});CombatScene rebuilt(c,s,r);for(int n=0;n<20;++n){rebuilt.update({});}need(s.count(83)==1&&rebuilt.foreground()==ForegroundKind::None,"Reward fact suppresses AI and autorun on scene reconstruction");
    }
    std::cout<<"Defeat and expiration ordering passed\n"<<std::flush;
    {
        auto e=entry(c,69,{7,6});e.session.inventory[78]=1;auto appContent=c;appContent.entries["power-fixture"]=e;
        // Public menu tool selection is verified separately; start the approved active90 interval to check Application pause without mutating its session.
        auto active=e;active.session.flags[227]=active.session.flags[237]=true;active.session.inventory.erase(78);active.session.eventTimer={EventTimerSource::Power,5459,true,false,false};appContent.entries["active-power-fixture"]=active;
        GameApplication running(appContent,"active-power-fixture",[](int,int hi){return hi;});running.update(key(InputAction::Cancel));need(running.mode()==ApplicationMode::Equipment,"Pursuit permits original menu entry");
        const auto timer=running.session().eventTimer();for(int n=0;n<200;++n){running.update({});}need(running.session().eventTimer()==timer,"Equipment mode pauses the Session90 timer");
    }
    {
        auto e=entry(c,76,{22,12});e.session.flags[231]=true;e.session.inventory[75]=1;GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,[](int,int hi){return hi;});
        interact(scene,c,"m76-e5",76);need(s.count(78)==1&&s.flag(202)&&s.flag(227),"Fuse is obtained through original pickup");
        for(const auto& [id,map]:std::vector<std::pair<std::string,int>>{{"m76-e7",75},{"m75-e2",74},{"m74-e9",73},{"m73-e10",68},{"m68-e10",66},{"m66-e12",67},{"m67-e10",69}}){interact(scene,c,id,map);std::cout<<id<<" -> "<<map<<"\n"<<std::flush;}
        walk(scene,c.objects.at("m69-e12").tile);install(scene);need(s.eventTimer().running&&s.flag(237),"Picked fuse starts the real90 second flow after continuous map travel");
        need(c.activeVariant(c.objects.at("m69-e2"),s)->page==4,"Active source interval selects the sealed exit page");
        // Keep the actual enemy present; original higher player guards suppress only its attack while waiting.
        s.setFlag(13,true);int elapsed=0;while(!s.flag(236)&&elapsed++<6500){scene.update(key(InputAction::Confirm));scene.takePresentation();}
        finish(scene);s.setFlag(13,false);need(s.flag(236)&&!s.flag(226)&&!s.flag(585)&&s.count(83)==0&&!s.eventTimer().running,"Timeout restores power while preserving optional pursuit and reward");
        need(c.activeVariant(c.objects.at("m69-e2"),s)->page==5,"Power completion opens the source exit independently of reward");
        defeat(scene,"m69-e9");scene.update({});finish(scene);need(s.flag(226)&&s.count(83)==1,"Pursuer can still be defeated for its card after timeout");
        interact(scene,c,"m69-e2",67);interact(scene,c,"m67-e8",66);
        walk(scene,c.objects.at("m66-e22").tile);scene.update({});const auto card=scene.requestToolUse();need(card&&scene.useTool(*card),"Reward card is usable at original powered door");finish(scene);need(s.flag(276)&&s.count(83)==0&&s.count(84)==0,"Card commits original door unlock and consumption");
        interact(scene,c,"m66-e19",85);interact(scene,c,"m85-e2",85);interact(scene,c,"m85-e11",85);
        need(s.count(19)==1&&s.count(25)==1&&s.equip(19)&&s.equip(25,2),"Newly picked AR and survival knife are immediately usable equipment");
        const int rounds=s.magazine(19);for(int n=0;n<90;++n){InputFrame f;f.buttons[static_cast<int>(InputAction::Weapon)].held=true;scene.update(f);}scene.update(key(InputAction::Attack));for(int n=0;n<30;++n){scene.update({});}
        need(s.magazine(19)<rounds,"Naturally acquired AR fires through real controller");
        for(int n=0;n<8;++n){InputFrame f;f.buttons[static_cast<int>(InputAction::Weapon)].held=true;scene.update(f);}for(int n=0;n<60;++n){scene.update({});}
        need(!scene.raised(),"Original short posture release lowers newly acquired AR before interaction");
        scene.update(key(InputAction::AuxiliaryWeapon));for(int n=0;n<150;++n){scene.update({});}need(scene.raised(),"Newly picked survival knife raises through real controller");scene.takePresentation();
        scene.update(key(InputAction::Attack));for(int n=0;n<40;++n){scene.update({});}const auto knifeEvents=scene.takePresentation();
        const auto sound=readSound(c.cue(c.actionProfiles.at("survival-knife-fire").at("sound_cue").get<std::string>()).at("sound"));
        need(std::any_of(knifeEvents.begin(),knifeEvents.end(),[&](const auto& event){return event.kind==PresentationEvent::Kind::Sound&&event.sound==sound;}),"Naturally acquired I25 executes its distinct original strike");
        for(int n=0;n<8;++n){InputFrame f;f.buttons[static_cast<int>(InputAction::AuxiliaryWeapon)].held=true;scene.update(f);}for(int n=0;n<60;++n){scene.update({});}need(!scene.raised(),"Original auxiliary posture release restores exploration");
        interact(scene,c,"m85-e19",66);interact(scene,c,"m66-e19",85);
        walk(scene,{13,6});InputFrame face;face.movement=Direction::Up;face.buttons[static_cast<int>(InputAction::Cancel)].held=true;scene.update(face);scene.update(key(InputAction::Confirm));finish(scene);
        walk(scene,{20,8});face.movement=Direction::Right;scene.update(face);scene.update(key(InputAction::Confirm));finish(scene);
        need(s.count(19)==0&&s.count(25)==0,"Original completed pickup pages reject adjacent investigation when equipped on return");
        interact(scene,c,"m85-e19",66);interact(scene,c,"m66-e12",67);const bool pending=c.pendingMapTickets.contains(70);interact(scene,c,"m67-e9",pending?67:70);
        if(pending){need(scene.exitRequest()&&scene.exitRequest()->map==70&&scene.takeApplicationRequest()==ApplicationMode::Boundary,"Undelivered parking keeps explicit98 boundary without committing departure");need(scene.mapId()==67&&s.flag(1)&&s.flag(18),"Pending parking preserves playable source and control");}
        else{need(scene.mapId()==70&&!scene.exitRequest()&&scene.takeApplicationRequest()!=ApplicationMode::Boundary,"Delivered first-guard consumer receives the original powered exit");}
        std::cout<<"Continuous fuse pickup, seven transfers, timeout, optional card, armory return and parking boundary passed\n";
    }
    std::cout<<"Power cancellation, retry, defeat ordering, timer menu pause and armory integration passed; no NORMAL playtest claim\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
