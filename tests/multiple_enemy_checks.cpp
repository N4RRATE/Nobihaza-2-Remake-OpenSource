#include "game_application.h"
#include "enemy_controller.h"
#include <algorithm>
#include <stdexcept>
#include <iostream>
void require(bool b,const char* message){if(!b){throw std::runtime_error(message);}}
int main(int argc,char** argv){
    if(argc!=2){return 2;}auto content=loadCombatContent(argv[1]);
    require(content.enemies.size()==(content.maps.contains(17)?7u:5u),"cumulative A/B keep first soldier and every applicable ordinary enemy");
    auto entry=content.entries.at("post-pickup");entry.map=10;entry.playerTile={5,20};entry.direction=Direction::Up;
    entry.pages.clear();entry.positions.clear();entry.through.clear();entry.directions.clear();content.entries["dual-fixture"]=entry;
    GameSession session(content.items,entry.session);CombatScene scene(content,session,"dual-fixture",[](int lo,int){return lo;});
    auto states=scene.enemyStates();require(states.size()==2&&states[0].id=="m10-e3"&&states[1].id=="m10-e5","original slot order selects two independent instances");
    std::set<std::string> resolved;
    require(scene.resolveShot({(5+.5)*16,(18+1)*16-8},true,0,false,resolved),"real first target receives attack");
    states=scene.enemyStates();require(states[0].damage>0&&states[1].damage==0,"damage belongs only to targeted instance");
    require(!scene.resolveShot({(5+.5)*16,(18+1)*16-8},true,0,false,resolved),"attack target set prevents duplicate application");
    // A sampled overlap is ordered by source slot, never short-circuited at target one.
    auto overlap=content;auto& o=overlap.entries.at("dual-fixture");
    o.positions["m10-e3"]={8,18};o.positions["m10-e5"]={8,18};o.pages["m10-e3"]=8;o.pages["m10-e5"]=8;
    o.through["m10-e3"]=true;o.through["m10-e5"]=true;o.directions["m10-e3"]=Direction::Up;o.directions["m10-e5"]=Direction::Up;
    o.session.flags[52]=false;o.session.flags[12]=false;o.session.baseAttack=30;
    GameSession bothSession(overlap.items,o.session);CombatScene both(overlap,bothSession,"dual-fixture",[](int,int hi){return hi;});
    std::set<std::string> targets;
    require(both.resolveShot({136,296},false,0,false,targets)&&targets.size()==2,"same sample visits both overlapping targets");
    const auto pair=both.enemyStates();require(pair[0].damage==pair[1].damage&&pair[0].damage>0,"each target reads independent defense and damage");
    require(pair[0].mode==EnemyMode::HitRecovery&&pair[1].mode==EnemyMode::HitRecovery,"knockback callback frozen before V35 changes");
    require(bothSession.variable(35)==2,"shared hit counter applied in slot order");
    auto recoil=content;auto& r=recoil.entries.at("dual-fixture");r.session.baseAttack=2;
    for(auto& page:recoil.objects.at("m10-e3").variants){if(page.page==8){page.autonomousRoute.steps.clear();}}
    GameSession recoilSession(recoil.items,r.session);CombatScene recoiling(recoil,recoilSession,"dual-fixture",[](int,int hi){return hi;});
    std::set<std::string> recoilShot;recoiling.resolveShot({88,296},false,0,false,recoilShot);
    for(int n=0;n<90;++n){recoiling.update({});}
    require(findCharacter(recoiling.world(),"m10-e3").motion.tile()==TilePosition{5,17},"P3 completes exactly one away step, without extra attack-recovery sidestep");
    // Normal death waits before committing, without the first soldier's foreground/reward.
    auto kill=content;auto& k=kill.entries.at("dual-fixture");k.session.baseAttack=1000;k.session.flags[12]=false;
    GameSession killedSession(kill.items,k.session);CombatScene killed(kill,killedSession,"dual-fixture",[](int,int hi){return hi;});
    std::set<std::string> lethal;require(killed.resolveShot({88,296},true,0,false,lethal),"lethal real target sample");
    require(!killedSession.flag(102)&&!killedSession.flag(101)&&killed.foreground()==ForegroundKind::None,"ordinary death neither commits early nor starts story");
    const auto secondOrigin=findCharacter(killed.world(),"m10-e5").motion.position();
    for(int n=0;n<30;++n){killed.update({});}
    require(!killedSession.flag(102),"Wait5 completes before death marker statement");killed.update({});
    require(killedSession.flag(102)&&!killedSession.flag(103)&&killedSession.flag(18),"only dead target commits marker; battle remains enabled");
    require(killed.enemyStates()[1].mode!=EnemyMode::Dormant,"second enemy progresses during first death");
    require(killed.foreground()==ForegroundKind::None&&killedSession.count(24)==session.count(24),"ordinary death has no first-soldier reward");
    CombatScene returned(kill,killedSession,"dual-fixture",[](int lo,int){return lo;});
    require(!findCharacter(returned.world(),"m10-e3").active&&returned.enemyStates()[1].damage==0,"recreated room hides dead ordinary enemy and resets live damage");
    auto head=kill;head.entries.at("dual-fixture").session.flags[12]=true;head.entries.at("dual-fixture").session.variables[35]=2;
    GameSession headSession(head.items,head.entries.at("dual-fixture").session);
    CombatScene decapitated(head,headSession,"dual-fixture",[](int lo,int hi){return hi==10?5:lo;});
    std::set<std::string> headShot;decapitated.resolveShot({88,296},true,0,false,headShot);
    require(findCharacter(decapitated.world(),"m10-e3").spriteId==head.enemies.at("m10-e3").rules.at("runtime_profile").at("sprites").at("head_transition"),"critical uses this enemy's head transition");
    for(int n=0;n<500&&!headSession.flag(102);++n){decapitated.update({});}
    require(headSession.flag(102)&&!headSession.flag(101)&&decapitated.foreground()==ForegroundKind::None,"parallel head animation commits only its ordinary marker");
    // Both attacks submit a foreground request on the same update. Only one owns the player.
    auto grabs=overlap;auto& g=grabs.entries.at("dual-fixture");g.playerTile={8,17};g.session.hp=1000;g.session.maxHp=1000;g.through["m10-e3"]=false;g.through["m10-e5"]=false;
    GameSession grabSession(grabs.items,g.session);CombatScene grabbed(grabs,grabSession,"dual-fixture",[](int lo,int hi){return hi==2?2:lo;});
    bool pending=false,firstOwner=false,secondOwner=false;
    for(int n=0;n<700&&!secondOwner;++n){
        grabbed.update({});const auto v=grabbed.enemyStates();
        const int owners=static_cast<int>(std::count_if(v.begin(),v.end(),[](const auto& e){return e.holdsPlayer;}));
        require(owners<=1,"only one foreground grab owner");
        firstOwner=firstOwner||v[0].holdsPlayer;pending=pending||v[1].mode==EnemyMode::PendingGrab;
        secondOwner=v[1].holdsPlayer;
    }
    require(firstOwner&&pending&&secondOwner,"queued grab survives another owner and later takes over");
    for(int n=0;n<35;++n){grabbed.update({});}
    require(grabbed.enemyStates()[1].holdsPlayer&&grabSession.flag(10)&&grabbed.world().player().spriteId==grabs.enemies.at("m10-e5").rules.at("runtime_profile").at("sprites").at("player_grab"),"old recovery cannot overwrite new grab owner");
    auto yield=grabs;auto& y=yield.entries.at("dual-fixture");y.positions["m10-e5"]={8,19};y.directions["m10-e5"]=Direction::Right;
    for(auto& page:yield.objects.at("m10-e5").variants){if(page.page==8){page.autonomousRoute.steps.clear();}}
    // Reviewed geometry variation: clear terrain isolates P1 direction from map walls.
    std::fill(yield.maps.at(10).passageMasks.begin(),yield.maps.at(10).passageMasks.end(),15);std::fill(yield.terrainIds.at(10).begin(),yield.terrainIds.at(10).end(),1);
    GameSession yieldSession(yield.items,y.session);CombatScene pushed(yield,yieldSession,"dual-fixture",[](int lo,int hi){return hi==2?2:lo;});
    for(int n=0;n<600&&yieldSession.conditions().recoveryTicks==0;++n){pushed.update({});}
    require(yieldSession.conditions().recoveryTicks>0,"collision-yield fixture reaches actual push");
    for(int n=0;n<20;++n){pushed.update({});}
    require(findCharacter(pushed.world(),"m10-e5").motion.tile().x<8,"P1 uses called enemy facing: right-facing target yields left, not pusher down");
    // The source order survives changed stable IDs and container order.
    auto varied=overlap;auto object=varied.objects.extract("m10-e3");object.key()="z-enemy";object.mapped().id="z-enemy";
    for(auto& page:object.mapped().variants){page.definition.id="z-enemy";}varied.objects.insert(std::move(object));
    auto enemy=varied.enemies.extract("m10-e3");enemy.key()="z-enemy";enemy.mapped().id="z-enemy";varied.enemies.insert(std::move(enemy));
    auto& v=varied.entries.at("dual-fixture");v.pages.clear();v.positions.clear();v.through.clear();v.directions.clear();
    GameSession variedSession(varied.items,v.session);CombatScene variation(varied,variedSession,"dual-fixture");
    require(variation.enemyStates()[0].id=="z-enemy"&&variation.enemyStates()[1].id=="m10-e5","source order independent of object string sorting");
    std::cout<<"multiple enemy checks passed\n";
}
