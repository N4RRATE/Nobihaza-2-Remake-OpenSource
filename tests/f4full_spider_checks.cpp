#include "combat_scene.h"
#include "enemy_controller.h"
#include "spider_behavior.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace{
void require(bool ok,const char* reason){if(!ok){throw std::runtime_error(reason);}}
void tick(CombatScene& scene,int n){while(n--){scene.update({});}}
EnemyView state(const CombatScene& scene,const std::string& id){for(const auto& e:scene.enemyStates()){if(e.id==id){return e;}}throw std::runtime_error("Missing installed spider");}
CombatEntry entry(const CombatContent& c,const std::string& id,bool active=false){
    auto e=c.entries.at("post-pickup");e.map=c.enemies.at(id).map;e.playerTile={12,6};e.pages.clear();e.positions.clear();e.through.clear();e.directions.clear();e.positions[id]={6,6};e.directions[id]=Direction::Right;
    e.session.hp=e.session.maxHp=1000;e.session.baseDefense=0;e.session.armor=0;e.session.accessory=0;e.session.otherEquipment=0;e.session.conditions={};e.session.baseAttack=20;e.session.mainWeapon=0;
    for(int flag:{1,13,15,18,21,23,24,37,38,45,47,227,228,585}){e.session.flags[flag]=false;}e.session.flags[18]=active;
    for(const auto& [key,d]:c.enemies){if(d.map==e.map){e.session.flags[d.deadFlag]=key!=id;}}
    e.session.variables[35]=0;e.session.variables[41]=0;return e;
}
AttackResolution hit(CombatScene& scene,const std::string& id,int receiver,AttackKind kind=AttackKind::Handgun,AttackPhase phase=AttackPhase::Impact){
    const auto p=findCharacter(scene.world(),id).motion.position();std::set<std::string> resolved;
    return scene.resolveAttack({{(p.x+.5)*16,(p.y+.5)*16},{0,0},kind,receiver,0,false,1,phase},resolved);
}
}
int main(int argc,char** argv){
    if(argc!=2){return 2;}
    try{
        auto c=loadCombatContent(argv[1]);
        for(auto& f:c.catalog.at("flows")){if(f.value("trigger",std::string{})=="autorun"){f["trigger"]="call-only";}}
        std::vector<std::string> ids;
        for(const auto& [id,d]:c.enemies){if(d.rules.value("behavior",std::string{})=="spider"){ids.push_back(id);c.pendingMapTickets.erase(d.map);}}
        const bool water=c.full->at("runtime").contains("evacuation");require(ids.size()==(water?7:3),"Actual installer binds the three underground and four delivered water spiders");
        // Controlled movement variation isolates source actions from unrelated story admission.
        for(const auto& id:ids){for(auto& p:c.objects.at(id).variants){p.autonomousRoute.steps.clear();}std::fill(c.terrainIds.at(c.enemies.at(id).map).begin(),c.terrainIds.at(c.enemies.at(id).map).end(),0);}
        for(const auto& id:ids){
            std::cout<<"Source spider "<<id<<std::endl;
            const auto& rules=c.enemies.at(id).rules;const auto& profile=rules.at("runtime_profile");
            {
                auto e=entry(c,id);GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e,[](int low,int high){return low<=5&&high>=5?5:low;});
                require(hit(room,id,1,AttackKind::GlNormal,AttackPhase::Flight).processed==0&&state(room,id).damage==0,"GL flight contacts the source spider without damage or activation");
                const auto first=hit(room,id,2);require(first.processed==1&&first.damage>0&&state(room,id).sourceState==0,"P2 accumulates CE11 damage without changing movement state");
                const auto events=room.takePresentation();const std::string blood=rules.at("blood_effects").at("101");
                require(std::any_of(events.begin(),events.end(),[&](const auto& event){return event.kind==PresentationEvent::Kind::Effect&&event.id==blood;}),"Source normal spider blood uses its green CE16 effect");
                require(c.media.at("effects").at(blood).at("stage_tone_percent").at(0)==nlohmann::json::array({50,160,100,100}),"Spider source blood keeps its original green tone");
            }
            {
                auto e=entry(c,id);GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);
                EnemyHost host(room,id);auto jumping=host.actor();
                require(jumping.motion.beginJump({1,0}),"Source body sampling jump starts");
                while(jumping.motion.jumpHeight()<16&&jumping.motion.moving()){jumping.motion.advance();}
                require(jumping.motion.jumpHeight()==16,"Source jump reaches its sixteen-pixel apex");host.setActor(jumping);
                require(host.actorScreenFoot().y==room.sampleActorScreen(id).y,"Enemy body foot includes native jump height exactly once");
                const auto foot=host.actorScreenFoot();const auto& offset=profile.at("body_presentation").at("offset_pixels_by_direction").at("right");
                const GridPosition captured{foot.x+offset.at(0).get<int>(),foot.y+offset.at(1).get<int>()};
                room.takePresentation();const auto request=room.requestInput(NumberPrompt{});room.update({});
                const auto blocked=room.takePresentation();
                // Other water-room ambient events do not belong to this spider picture guard.
                require(std::none_of(blocked.begin(),blocked.end(),[&](const auto& event){return event.actor==id;}),"This spider's native picture command waits while an input window is visible");
                auto actor=host.actor();actor.motion=CharacterMotion({8,6},Direction::Right,actor.motion.speed());host.setActor(actor);
                require(room.cancelInput(request),"Picture guard prompt cancels through public identity");room.update({});
                const auto events=room.takePresentation();const auto first=profile.at("body_presentation").at("normal_frames").at("right").at(0).at("show_cue").get<std::string>();
                require(std::any_of(events.begin(),events.end(),[&](const auto& event){return event.id==first&&event.sampledScreenPosition&&event.sampledScreenPosition->x==captured.x&&event.sampledScreenPosition->y==captured.y;}),"Pending picture retry keeps its original screen sample while World position changes");
            }
            for(const auto [receiver,expected,ticks]:{std::array{3,10,18},std::array{4,11,48},std::array{5,10,18},std::array{6,12,60}}){
                auto e=entry(c,id);GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e,[](int low,int high){return low<=5&&high>=5?5:low;});
                require(hit(room,id,receiver).processed==1&&state(room,id).sourceState==expected,"Each source receiver selects its own finite recovery");
                require(state(room,id).damage>0&&state(room,id).damage<100,"Receivers5/6 use CE11 damage, never invented150/2000");
                s.setFlag(18,true);room.update({});tick(room,ticks-1);require(state(room,id).sourceState==expected,"Recovery retains its complete original Wait");room.update({});
                require(state(room,id).sourceState==1,"Recovery resumes original pursuit after its Wait");
            }
            {
                auto e=entry(c,id);e.session.baseAttack=80;e.session.flags[47]=true;GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e,[](int low,int high){return low<=5&&high>=5?5:low;});
                require(hit(room,id,3).damage>=100&&state(room,id).mode==EnemyMode::Ending,"Critical CE11 damage doubles before source death threshold100");
                room.update({});tick(room,29);require(!s.flag(c.enemies.at(id).deadFlag),"Death preserves its original Wait5 before committing dead fact");room.update({});
                require(s.flag(c.enemies.at(id).deadFlag)&&state(room,id).mode==EnemyMode::Corpse&&hit(room,id,2).processed==0,"Completed source corpse rejects damage");
                room.deactivate();const auto dead=s.exportState();tick(room,100);require(s.exportState()==dead,"Retired scene cannot continue its spider actions");
                CombatScene returned(c,s,e);require(state(returned,id).mode==EnemyMode::Corpse,"Death fact rebuilds the source corpse on room re-entry");
            }
            {
                auto e=entry(c,id,true);e.playerTile={9,6};int decisions=0;
                GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e,[&](int low,int high){if(low==0&&(high==15||high==10)){++decisions;}return high;});
                tick(room,2);require(state(room,id).sourceState==4&&decisions==1,"Original random6 is consumed then overwritten by distance+1 acid state");
                tick(room,12);require(!s.flag(profile.at("acid").at("prepare").at("commit_attack_fact")),"Acid startup retains its complete Wait2 before committing attack fact");room.update({});
                require(s.flag(profile.at("acid").at("prepare").at("commit_attack_fact")),"Acid source manager starts only after original preparation Wait");
                for(int i=0;i<80&&s.hp()==1000;++i){room.update({});}
                require(s.hp()<1000&&s.conditions().poisoned,"Acid original jump reaches signed-sum contact and applies CE12 plus State2");
                const int hp=s.hp();s.setFlag(18,false);tick(room,130);require(!s.flag(profile.at("acid").at("prepare").at("commit_attack_fact"))&&s.hp()<=hp,"Single active acid manager clears its source fact after both finish waits");
            }
        }
        // The source overwrites natural state6. Its complete page still supports a real external state request.
        {
            const std::string id=ids.front();auto domain=c;const auto definition=domain.enemies.at(id);auto e=entry(domain,id,true);e.playerTile={7,6};
            domain.enemies.clear();GameSession s(domain.items,e.session,domain.facts);CombatScene room(domain,s,e,[](int low,int){return low;});domain.enemies.emplace(id,definition);
            auto spider=makeEnemyBehavior(room,definition);require(spider->activateState(6),"Source page13 accepts its declared external state6");
            for(int i=0;i<80&&!s.flag(45);++i){spider->updateParallel();spider->update({});room.update({});}
            require(s.flag(45)&&s.hp()<1000,"Swoop checks front tile during15 original iterations and commits player reaction");
            for(int i=0;i<100&&spider->sourceState()==6;++i){spider->updateParallel();spider->update({});room.update({});}
            require(spider->sourceState()==0,"Swoop completes descent waits, picture cleanup and original roam state");
            require(!spider->activateState(9),"Undeclared external state is rejected without changing behavior");
        }
        std::cout<<"Installed underground and water spiders: source damage/recovery, green blood, acid poison, swoop and corpse lifecycle passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
