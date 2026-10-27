#include "combat_content.h"
#include "combat_scene.h"
#include "enemy_controller.h"
#include <iostream>
#include <stdexcept>
void require(bool value,const char* message){if(!value){throw std::runtime_error(message);}}
int main(int argc,char** argv){
    if(argc!=2){return 2;}
    try{
        auto content=loadCombatContent(argv[1]);content.pendingMapTickets.erase(19);
        auto entry=content.entries.at("defeated-claimed");entry.map=19;entry.playerTile={20,18};entry.direction=Direction::Up;
        entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();
        entry.session.flags[57]=true;entry.session.flags[105]=false;
        GameSession session(content.items,entry.session,content.facts);
        CombatScene scene(content,session,entry,[](int lo,int){return lo;});
        require(scene.enemyStates().size()==1&&scene.enemyStates()[0].id=="m19-e2","Map19 creates its own licker behavior");
        scene.update({});
        require(scene.enemyStates()[0].mode!=EnemyMode::Dormant,"Activated licker leaves dormant state");
        entry.session.baseAttack=11;entry.session.flags[47]=true;entry.session.variables[35]=0;
        GameSession aimedSession(content.items,entry.session,content.facts);
        CombatScene aimedScene(content,aimedSession,entry,[](int low,int high){return low==0&&high==10?2:low;});aimedScene.update({});
        const auto target=findCharacter(aimedScene.world(),"m19-e2").motion.position();
        std::set<std::string> resolved;
        const auto hit=aimedScene.resolveAttack({(target.x+.5)*16,(target.y+.5)*16},true,0,false,resolved);
        require(hit.contacts==1&&hit.processed==1&&hit.damage==42,"Licker critical doubles CE11 damage instead of zombie direct 150");
        require(!aimedSession.flag(47)&&aimedScene.enemyStates()[0].damage==42,"Actual CE11 consumes precision and retains accumulated damage");
        const auto bloodEvents=aimedScene.takePresentation();
        require(std::any_of(bloodEvents.begin(),bloodEvents.end(),[](const auto& e){return e.kind==PresentationEvent::Kind::Effect&&e.id=="enemy-blood-small"&&e.actor=="m19-e2";}),"Critical licker hit still emits CE16 blood following its stable actor");
        const auto duplicate=aimedScene.resolveAttack({(target.x+.5)*16,(target.y+.5)*16},true,0,false,resolved);
        require(duplicate.processed==0&&aimedScene.enemyStates()[0].damage==42,"One attack cannot apply the same target twice");
        std::set<std::string> secondAttack;
        aimedScene.resolveAttack({(target.x+.5)*16,(target.y+.5)*16},false,0,false,secondAttack);
        const int beforeDown=aimedScene.enemyStates()[0].damage;const int hitCount=aimedSession.variable(35);
        std::set<std::string> downAttack;
        const auto down=aimedScene.resolveAttack({(target.x+.5)*16,(target.y+.5)*16},true,0,false,downAttack);
        require(down.processed==1&&down.damage==0&&aimedScene.enemyStates()[0].damage==beforeDown&&aimedSession.variable(35)==hitCount,"Downed licker acknowledges collision but refuses receiver damage and CE11 statistics");
        // Finish knockdown before a new valid hit. Reaching the damage limit
        // starts the death presentation; the durable fact commits after Wait5.
        for(int n=0;n<90;++n){aimedScene.update({});}
        aimedSession.setFlag(47,true);std::set<std::string> lethal;
        const auto atDeath=findCharacter(aimedScene.world(),"m19-e2").motion.position();
        aimedScene.resolveAttack({(atDeath.x+.5)*16,(atDeath.y+.5)*16},true,0,false,lethal);
        require(aimedScene.enemyStates()[0].damage>=100&&!aimedSession.flag(105),"Damage threshold begins ending before durable death commit");
        for(int n=0;n<20;++n){aimedScene.update({});}require(!aimedSession.flag(105),"Death wait cannot commit early");
        for(int n=0;n<20;++n){aimedScene.update({});}require(aimedSession.flag(105),"Death presentation commits its original permanent flag");
        CombatScene returned(content,aimedSession,entry);require(!findCharacter(returned.world(),"m19-e2").active,"Dead licker returns at origin instead of reactivating");
        // A real update-driven encounter must reach the wall/ceiling/strangle
        // path. This fixture changes HP only to observe the non-HP failure.
        entry.session.hp=100;entry.session.maxHp=100;entry.session.baseDefense=999;
        entry.session.flags[47]=false;entry.playerTile={20,16};entry.directions["m19-e2"]=Direction::Down;
        GameSession grappleSession(content.items,entry.session,content.facts);
        CombatScene grapple(content,grappleSession,entry,[](int low,int){return low;});
        bool ceiling=false,held=false,title=false,checkedCeiling=false;
        for(int n=0;n<12000&&!title;++n){
            grapple.update({});const auto enemy=grapple.enemyStates()[0];
            if(enemy.sourceState==7&&!checkedCeiling){
                const auto at=findCharacter(grapple.world(),"m19-e2").motion.position();std::set<std::string> hits;
                const auto unavailable=grapple.resolveAttack({(at.x+.5)*16,(at.y+.5)*16},true,0,false,hits);
                require(unavailable.processed==0,"Ceiling helper clears the hit center; the physical actor position is not a valid target");checkedCeiling=true;
            }
            ceiling|=enemy.sourceState==7;held|=enemy.holdsPlayer;
            if(auto request=grapple.takeApplicationRequest()){title=*request==ApplicationMode::Title;}
        }
        require(ceiling&&held,"Claw retreat, wall climbing and ceiling chase reach strangle");
        require(title&&grappleSession.hp()>0,"Six failed strangle periods run CE57 and return title with positive HP");
        GameSession escapeSession(content.items,entry.session,content.facts);
        CombatScene escapeScene(content,escapeSession,entry,[](int lo,int){return lo;});
        bool caught=false,escaped=false;
        for(int n=0;n<12000&&!escaped;++n){
            InputFrame input;
            if(caught&&n%2==0){input.buttons[static_cast<std::size_t>(InputAction::Up)].pressed=true;}
            escapeScene.update(input);const auto enemy=escapeScene.enemyStates()[0];
            caught|=enemy.holdsPlayer;
            escaped=caught&&!enemy.holdsPlayer&&escapeSession.flag(1)&&!escapeSession.flag(10);
            require(!escapeScene.takeApplicationRequest(),"Successful escape cannot request a title transition");
        }
        require(escaped&&escapeSession.hp()>0,"Fresh directional presses release strangle and return control");
        escapeScene.deactivate();const auto oldHP=escapeSession.hp();
        for(int n=0;n<300;++n){escapeScene.update({});}
        require(escapeSession.hp()==oldHP&&!escapeScene.takeApplicationRequest(),"Destroyed scene does not retain a licker action result");
        auto magnumEntry=entry;magnumEntry.session.mainWeapon=20;magnumEntry.session.flags[47]=false;magnumEntry.session.flags[12]=true;magnumEntry.session.variables[35]=0;
        GameSession magnumSession(content.items,magnumEntry.session,content.facts);CombatScene magnumScene(content,magnumSession,magnumEntry,[](int lo,int hi){return lo==0&&hi==10?2:lo;});magnumScene.update({});
        const auto mp=findCharacter(magnumScene.world(),"m19-e2").motion.position();std::set<std::string> mh;
        const auto magnumHit=magnumScene.resolveAttack({(mp.x+.5)*16,(mp.y+.5)*16},false,12,false,mh,AttackKind::Magnum);
        require(magnumHit.damage==magnumSession.attack()-9&&!magnumSession.flag(12),"I20 against licker has no handgun distance reduction and clears previous critical status");
        auto pounceEntry=entry;pounceEntry.playerTile={20,11};pounceEntry.positions["m19-e2"]={20,15};pounceEntry.directions["m19-e2"]=Direction::Left;
        GameSession pounceSession(content.items,pounceEntry.session,content.facts);CombatScene pounceScene(content,pounceSession,pounceEntry,[](int lo,int){return lo;});
        pounceScene.update({});pounceScene.update({});
        require(pounceScene.enemyStates()[0].mode==EnemyMode::Windup&&findCharacter(pounceScene.world(),"m19-e2").motion.facing()==Direction::Left,"Perpendicular four-tile pounce prepares before original route turns toward player");
        auto mixed=content;
        auto lickerObject=mixed.objects.extract("m19-e2");lickerObject.key()="renamed-licker";lickerObject.mapped().id=lickerObject.key();
        for(auto& variant:lickerObject.mapped().variants){variant.definition.id=lickerObject.key();}
        mixed.objects.insert(std::move(lickerObject));
        auto lickerDefinition=mixed.expansion.enemies.extract("m19-e2");lickerDefinition.key()="renamed-licker";lickerDefinition.mapped().id=lickerDefinition.key();mixed.expansion.enemies.insert(std::move(lickerDefinition));
        auto ordinary=mixed.enemies.at("m10-e3");ordinary.id="z-last-in-container";ordinary.map=19;ordinary.sourceOrder=1;
        auto ordinaryObject=mixed.objects.at("m10-e3");ordinaryObject.id=ordinary.id;ordinaryObject.map=19;
        for(auto& variant:ordinaryObject.variants){variant.definition.id=ordinary.id;}
        mixed.enemies[ordinary.id]=ordinary;mixed.objects[ordinary.id]=ordinaryObject;
        auto mixedEntry=entry;mixedEntry.session.flags[ordinary.deadFlag]=false;mixedEntry.session.flags[47]=false;
        mixedEntry.positions[ordinary.id]={20,15};mixedEntry.positions["renamed-licker"]={20,15};
        mixedEntry.directions.erase("m19-e2");mixedEntry.directions["renamed-licker"]=Direction::Down;
        GameSession mixedSession(mixed.items,mixedEntry.session,mixed.facts);
        CombatScene mixedScene(mixed,mixedSession,mixedEntry,[](int lo,int hi){return lo==0&&hi==10?2:lo;});mixedScene.update({});
        require(mixedScene.enemyStates().size()==2&&mixedScene.enemyStates()[0].id==ordinary.id,"Mixed kinds use original source order rather than lexical IDs");
        const auto overlap=findCharacter(mixedScene.world(),"renamed-licker").motion.position();std::set<std::string> mixedHit;
        const auto both=mixedScene.resolveAttack({(overlap.x+.5)*16,(overlap.y+.5)*16},true,0,false,mixedHit);
        require(both.processed==2&&mixedScene.enemyStates()[0].damage>0&&mixedScene.enemyStates()[1].damage>0,"Overlapping mixed enemies independently receive one attack after stable ID variation");
        auto competing=content;
        auto second=competing.expansion.enemies.at("m19-e2");second.id="second-licker";second.event=202;second.sourceOrder=3;
        auto secondObject=competing.objects.at("m19-e2");secondObject.id=second.id;
        for(auto& v:secondObject.variants){v.definition.id=second.id;v.definition.sourceEventId=202;}
        competing.expansion.enemies[second.id]=second;competing.objects[second.id]=secondObject;
        auto competingEntry=entry;competingEntry.positions["m19-e2"]={20,15};competingEntry.positions[second.id]={20,15};competingEntry.directions[second.id]=Direction::Down;
        GameSession competeSession(competing.items,competingEntry.session,competing.facts);
        CombatScene competeScene(competing,competeSession,competingEntry,[](int lo,int){return lo;});
        bool contested=false,recovered=false;std::set<std::string> observedOwners;std::string previousOwner;
        for(int n=0;n<12000;++n){
            InputFrame input;if(n%2==0){input.buttons[static_cast<std::size_t>(InputAction::Up)].pressed=true;}
            competeScene.update(input);std::string owner;int pending=0;
            for(const auto& e:competeScene.enemyStates()){if(e.holdsPlayer){require(owner.empty(),"Only one grab owner");owner=e.id;}pending+=e.mode==EnemyMode::PendingGrab;}
            contested|=!owner.empty()&&pending>0;
            if(!owner.empty()&&owner!=previousOwner){require(!competeSession.flag(13),"An old ceiling request cannot grab an invulnerable player after the previous owner releases");}
            if(!owner.empty()){observedOwners.insert(owner);}
            recovered|=!previousOwner.empty()&&owner.empty()&&competeSession.flag(13);previousOwner=owner;
        }
        require(observedOwners.size()==2&&recovered,"Two lickers independently capture and release without concurrent control or bypassing recovery immunity");
        auto badRoute=content;
        const std::string approach=badRoute.rules.at("enemy").at("runtime_profile").at("cues").at("approach_bite");
        badRoute.routes.at(approach).target=10001;
        auto badEntry=badRoute.entries.at("post-pickup");badEntry.playerTile={10,8};badEntry.positions["m4-e4"]={10,9};badEntry.directions["m4-e4"]=Direction::Up;
        GameSession protectedSession(badRoute.items,badEntry.session,badRoute.facts);
        CombatScene protectedScene(badRoute,protectedSession,badEntry,[](int lo,int){return lo;});
        bool rejected=false;try{for(int n=0;n<30;++n){protectedScene.update({});}}catch(const std::logic_error&){rejected=true;}
        require(rejected&&protectedScene.world().player().motion.tile()==badEntry.playerTile,"An enemy route cannot bind an unowned player through its source target");
        std::cout<<"Licker checks passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
