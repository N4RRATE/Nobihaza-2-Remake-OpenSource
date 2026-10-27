#include "combat_scene.h"
#include "enemy_controller.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace{
void require(bool ok,const char* message){if(!ok){throw std::runtime_error(message);}}
void tick(CombatScene& scene,int count){while(count--){scene.update({});}}
EnemyView enemy(const CombatScene& scene,const std::string& id){const auto states=scene.enemyStates();const auto found=std::find_if(states.begin(),states.end(),[&](const auto& e){return e.id==id;});require(found!=states.end(),"Expected real rescue enemy");return *found;}
CombatEntry entry(const CombatContent& content,int map){auto e=content.entries.at("post-pickup");e.map=map;e.playerTile={1,1};e.positions.clear();e.pages.clear();e.through.clear();e.directions.clear();e.session.flags[18]=false;e.session.flags[1]=false;return e;}
AttackResolution hit(CombatScene& scene,const std::string& id,AttackKind kind=AttackKind::Handgun,int page=2,AttackPhase phase=AttackPhase::Impact){const auto p=findCharacter(scene.world(),id).motion.position();std::set<std::string> resolved;return scene.resolveAttack({{(p.x+.5)*16,(p.y+.5)*16},{8,8},kind,page,0,false,1,phase},resolved);}
}
int main(int argc,char** argv){
    if(argc!=2){return 2;}
    try{
        const auto c=loadCombatContent(argv[1]);require(c.full->at("runtime").contains("rescue"),"Check needs admitted real rescue content");
        {
            auto e=entry(c,61);e.session.flags[179]=false;e.session.flags[178]=false;
            GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e);
            require(hit(scene,"m61-e3",AttackKind::GlNormal,1,AttackPhase::Flight).processed==0&&!s.flag(179),"GL flight contacts a dormant target without activating it");
            tick(scene,104);require(!s.flag(179)&&enemy(scene,"m61-e3").damage==0,"M61 E3 P8 stand-up cycles never set S179 or damage");
            const auto events=scene.takePresentation();const auto soundCue=c.enemies.at("m61-e3").rules.at("dormant_presentation").at("cues").at(5).get<std::string>();const auto sound=readSound(c.cue(soundCue).at("sound"));
            require(std::any_of(events.begin(),events.end(),[&](const auto& event){return event.kind==PresentationEvent::Kind::Sound&&event.sound==sound;}),"Dormant stand-up emits its original source sound before receiving a hit");
            require(hit(scene,"m61-e3").processed==1&&s.flag(179)&&enemy(scene,"m61-e3").damage>0,"M61 E3 P2 first actual received hit activates S179");
        }
        {
            auto e=entry(c,42);e.session.flags[181]=true;e.session.flags[498]=false;e.session.flags[193]=false;
            for(int fact:{176,177,182,183,184,185,186}){e.session.flags[fact]=true;}
            e.session.flags[401]=false;GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e);
            for(int i=0;i<425&&!s.flag(498);++i){scene.update({});}
            require(s.flag(498)&&enemy(scene,"m42-e21").sourceState==200,"Real reinforcement delay submits mode200 before pursuit");
            require(hit(scene,"m42-e21").processed==0,"Arrival has no invented combat receiver during its falling introduction");
            for(int i=0;i<300&&enemy(scene,"m42-e21").sourceState==200;++i){scene.update({});}
            require(enemy(scene,"m42-e21").sourceState==0&&findCharacter(scene.world(),"m42-e21").motion.tile()==TilePosition{25,6},"M42 E21 P18 finishes two down steps and its source waits before mode0");
            const auto before=s.exportState();scene.deactivate();tick(scene,40);require(s.exportState()==before,"Retired rescue scene cannot continue arrival effects");
        }
        {
            auto e=entry(c,42);e.session.baseAttack=63;e.session.flags[181]=true;e.session.flags[498]=true;e.session.flags[185]=false;
            for(int fact:{176,177,182,183,184,186,401}){e.session.flags[fact]=true;}
            {GameSession stats(c.items,e.session,c.facts);e.session.baseAttack+=63-stats.attack();}
            GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,[](int low,int high){return low<=5&&high>=5?5:low;});
            hit(scene,"m42-e17");const int damage=enemy(scene,"m42-e17").damage;
            if(damage!=60){throw std::runtime_error("Original Actor20 defense3 expected local damage60, actual "+std::to_string(damage)+", attack "+std::to_string(s.attack()));}
            s.setFlag(18,true);tick(scene,2);require(enemy(scene,"m42-e17").mode!=EnemyMode::Fallen&&enemy(scene,"m42-e17").damage==60,"M42 E17 has no fabricated >=50 fall branch");
            require(c.enemies.at("m42-e17").rules.at("source_slots").at("state")==161,"Original encounter source-slot identity remains data");
        }
        std::cout<<"Real rescue enemy arrival, dormant activation and profile variation checks passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
