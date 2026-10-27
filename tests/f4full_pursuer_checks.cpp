#include "combat_scene.h"
#include "enemy_controller.h"
#include "enemy_host.h"
#include "scene_presentation.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* reason){if(!ok){throw std::runtime_error(reason);}}
EnemyView state(const CombatScene& scene,const std::string& id){for(const auto& enemy:scene.enemyStates()){if(enemy.id==id){return enemy;}}throw std::runtime_error("Missing pursuer");}
AttackResolution hit(CombatScene& scene,const std::string& id,int page=2){const auto p=findCharacter(scene.world(),id).motion.position();std::set<std::string> resolved;return scene.resolveAttack({{p.x*16+8,p.y*16-5},{0,0},AttackKind::Handgun,page,0,false},resolved);}
bool sampledAt(const std::vector<PresentationEvent>& events,const std::string& cue,double x,double y){
    return std::any_of(events.begin(),events.end(),[&](const auto& event){return event.id==cue&&event.sampledScreenPosition&&std::abs(event.sampledScreenPosition->x-x)<.001&&std::abs(event.sampledScreenPosition->y-y)<.001;});
}
int main(int argc,char** argv){try{
    if(argc!=2&&argc!=3&&argc!=4){return 2;}auto content=loadCombatContent(argv[1]);
    if(argc==3&&std::string(argv[2])=="--underground-cache"){
        auto c=content;for(auto& flow:c.catalog.at("flows")){if(flow.value("trigger",std::string{})=="autorun"){flow["trigger"]="call-only";}}
        for(const std::string id:{"m67-e13","m69-e9"}){
            require(c.enemies.contains(id)&&c.enemies.at(id).rules.at("stop_fact")==585,"Cache consumer uses a production-installed underground pursuer");
            c.pendingMapTickets.erase(c.enemies.at(id).map);for(auto& page:c.objects.at(id).variants){page.autonomousRoute.steps.clear();}
            auto e=c.entries.at("defeated-claimed");e.map=c.enemies.at(id).map;e.playerTile={15,12};e.positions.clear();e.pages.clear();e.directions.clear();e.through.clear();e.positions[id]={6,6};e.directions[id]=Direction::Right;
            for(int flag:{15,21,37,45,160,226,228,585}){e.session.flags[flag]=false;}e.session.flags[18]=e.session.flags[227]=e.session.flags[13]=true;e.session.pursuerHealth=500;e.session.variables[81]=e.session.variables[82]=0;
            // M76 E5P2 clears the original dogs before enabling underground pursuit.
            // Their source managers must not overwrite the shared slot during a guarded picture.
            for(const auto& [enemy,d]:c.enemies){if(d.map==e.map&&enemy!=id){e.session.flags[d.deadFlag]=true;}}
            GameSession session(c.items,e.session,c.facts);CombatScene room(c,session,e);const auto request=room.requestInput(NumberPrompt{});room.update({});
            require(session.variable(81)==6&&session.variable(82)==6&&session.variable(90)==c.enemies.at(id).sourceOrder,"Original idle body samples tile and source event before its first picture guard");
            EnemyHost host(room,id);auto actor=host.actor();actor.motion=CharacterMotion({8,6},Direction::Right,actor.motion.speed());host.setActor(actor);room.update({});
            require(session.variable(81)==6,"Blocked picture retry cannot resample the later World tile");require(room.cancelInput(request),"Cache fixture cancels its public input identity");
            room.update({});room.update({});require(!session.flag(21)&&session.variable(81)==8&&session.variable(82)==6,"Next idle source body iteration commits the new cached tile outside attack range");
        }
        std::cout<<"Installed underground pursuer tile cache and native picture guard lifetime passed\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--underground-gas"){
        auto c=content;
        for(auto& flow:c.catalog.at("flows")){if(flow.value("trigger",std::string{})=="autorun"){flow["trigger"]="call-only";}}
        // Isolate the environmental receiver; enemy movement is covered by the seven-profile checks.
        std::erase_if(c.enemies,[](const auto& row){return row.second.map>=66&&row.second.map<=77;});
        for(int map:{66,76}){
            c.pendingMapTickets.erase(map);const auto& monitor=c.full->at("runtime").at("underground").at("gas_monitors").at(std::to_string(map));
            auto e=c.entries.at("defeated-claimed");e.map=map;e.playerTile={6,6};e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();
            e.positions[monitor.at("box_object").get<std::string>()]={6,6};e.session.hp=100;e.session.maxHp=100;e.session.armor=0;e.session.accessory=0;e.session.otherEquipment=0;e.session.baseDefense=0;e.session.conditions={};
            for(int flag:{13,15,160,227,585}){e.session.flags[flag]=false;}e.session.flags[18]=true;e.session.flags[228]=true;
            GameSession session(c.items,e.session,c.facts);CombatScene room(c,session,e,[](int low,int){return low;});
            if(map==76){
                room.update({});session.setFlag(227,true);
                for(int tick=0;tick<60;++tick){room.update({});}
                require(session.hp()==100&&!session.flag(148),"M76 finishes its already entered inactive Wait10 without emitting gas");
            }
            for(int tick=0;tick<420;++tick){room.update({});}
            require(session.hp()==100,"Gas retains its complete original Wait70 before damage");room.update({});
            require(session.hp()==95&&session.conditions().neuralParalysis&&session.flag(148),"Underground gas applies CE12 and State8 through its reviewed source box");
            session.setFlag(585,true);const int hp=session.hp();for(int tick=0;tick<500;++tick){room.update({});}
            require(session.hp()==hp,"Higher source stop page cancels the local gas loop");
        }
        std::cout<<"Underground gas source wait, conditional sampling, damage and page cancellation passed\n";return 0;
    }
    if(argc==3&&std::string(argv[2])=="--underground"){
        // Reviewed underground domain fixture: keep story gates separate from the enemy consumer.
        auto c=content;const auto& rows=c.full->at("runtime").at("underground").at("encounters");
        for(auto& flow:c.catalog.at("flows")){if(flow.value("trigger",std::string{})=="autorun"){flow["trigger"]="call-only";}}
        std::erase_if(c.enemies,[](const auto& row){return row.second.map>=66&&row.second.map<=77;});
        std::vector<std::string> ids;
        for(const auto& [id,row]:rows.items()){
            if(row.at("type")!="pursuer"){continue;}ids.push_back(id);c.pendingMapTickets.erase(row.at("map").get<int>());
            auto rules=nlohmann::json{{"behavior","pursuer"},{"attack",row.at("normal_attributes").at("attack")},{"defense",row.at("normal_attributes").at("defense")},
                {"activation_fact",row.at("activation_fact")},{"completion_flow",row.at("completion_flow")},{"hitbox_half_pixels",{8,15}},{"runtime_profile",row.at("runtime_profile")}};
            c.enemies[id]={id,row.at("map").get<int>(),row.at("source_order").get<int>(),row.at("completion_flag").get<int>(),false,std::move(rules)};
            for(auto& page:c.objects.at(id).variants){page.autonomousRoute.steps.clear();}
        }
        const auto entryFor=[&](const std::string& id){auto e=c.entries.at("defeated-claimed");e.map=c.enemies.at(id).map;
            e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.playerTile={11,6};e.positions[id]={6,6};e.directions[id]=Direction::Right;
            for(int fact:{13,15,18,21,22,37,45,160,226,227,228,585}){e.session.flags[fact]=false;}
            e.session.flags[18]=true;e.session.flags[227]=true;e.session.flags[13]=true;e.session.hp=5000;e.session.maxHp=5000;e.session.mainWeapon=0;
            e.session.baseAttack=80;e.session.pursuerHealth=500;e.session.variables[89]=0;e.session.variables[91]=0;e.session.variables[193]=0;return e;};
        std::vector<std::string> failures;
        const auto check=[&](const std::string& name,const auto& action){try{action();std::cout<<"Underground pursuer checked: "<<name<<'\n'<<std::flush;}
            catch(const std::exception& error){failures.push_back(name+": "+error.what());std::cerr<<failures.back()<<'\n';}};
        for(const auto& id:ids){check(id+" original manager and Wait13",[&]{auto e=entryFor(id);e.playerTile={9,9};e.session.flags[160]=true;e.session.pursuerHealth=317;
            GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});
            for(int tick=0;tick<77;++tick){room.update({});}require(s.flag(160),"Original Wait13 lasts78 fixed updates after entry");room.update({});
            require(!s.flag(160)&&state(room,id).damage==183&&findCharacter(room.world(),id).motion.tile()==TilePosition{9,9},"Underground reappearance rebuilds local damage and captured tile for its own manager");
            room.deactivate();const int hp=s.pursuerHealth();require(hit(room,id).processed==0&&s.pursuerHealth()==hp,"Departed scene cannot commit old enemy damage");
        });}
        for(int choice:{0,1,2}){check("training shared selector "+std::to_string(choice),[&]{auto e=entryFor("m52-e4");e.session.flags[156]=true;e.session.flags[157]=false;e.session.flags[158]=false;e.session.flags[162]=false;
            e.playerTile={8,6};GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e,[choice](int lo,int hi){return lo==0&&hi==2?choice:hi;});room.update({});
            require(s.flag(21)&&s.variable(91)==(choice?1:0),"Shared random mapping preserves training0/1/2 to0/1/1 including zero attack fact");
        });}
        for(int choice:{0,5,8,9,10}){check("M66 source random "+std::to_string(choice),[&]{auto e=entryFor("m66-e17");e.playerTile={8,6};
            GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e,[choice](int lo,int hi){return lo==0&&hi==10?choice:hi;});room.update({});
            require(s.variable(91)==(choice<5?1:choice<8?2:choice)&&s.flag(21)==(choice<8),"Random8..10 retains original V91 and leaves S21 OFF");
        });}
        for(const auto* id:{"m67-e13","m76-e4"}){check(std::string(id)+" proximity entry",[&]{auto e=entryFor(id);
            GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});
            require(s.flag(228)&&!s.flag(21),"Original +/-80px entry sets228 without attack selection");
            const auto events=room.takePresentation();const auto& extra=c.enemies.at(id).rules.at("runtime_profile").at("extra_modes");
            const auto mode=std::find_if(extra.begin(),extra.end(),[](const auto& row){return row.at("role")=="proximity-entry";});
            const auto music=readSound(c.cue(mode->at("proximity_commit").at("music").get<std::string>()).at("sound"));
            require(std::any_of(events.begin(),events.end(),[&](const auto& event){return event.kind==PresentationEvent::Kind::Music&&event.sound==music;}),"Entry emits its own original music cue");
        });}
        check("M67 outside proximity rejects entry",[&]{auto e=entryFor("m67-e13");e.playerTile={12,6};GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});require(!s.flag(228),"Beyond80px preserves entry state");});
        check("M69 corridor status page",[&]{auto e=entryFor("m69-e9");e.session.variables[193]=2;GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});
            require(s.variable(193)==0&&!s.flag(21),"Higher corridor page clears source193 without attack selection");});
        check("M67 environment receiver",[&]{auto e=entryFor("m67-e13");e.session.flags[18]=false;GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});
            require(state(room,"m67-e13").damage==0,"Environment receiver is never an automatic per-frame damage action");
            require(hit(room,"m67-e13",9).damage==100&&s.pursuerHealth()==400&&s.variable(92)==3&&s.flag(22),"Original P9 commits fixed100 to local and persistent health with large blood");
        });
        require(failures.empty(),"Underground pursuer focused cases failed");return 0;
    }
    if(argc==3&&std::string(argv[2])=="--doors"){
        auto c=content;for(auto& flow:c.catalog.at("flows")){if(flow.value("trigger",std::string{})=="autorun"){flow["trigger"]="call-only";}}
        const auto entryFor=[&](int map){auto e=c.entries.at("defeated-claimed");e.map=map;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();
            for(int fact:{13,15,18,21,22,23,37,45,156,157,158,160,161,162,164}){e.session.flags[fact]=false;}
            e.session.hp=5000;e.session.maxHp=5000;e.session.baseDefense=0;e.session.armor=0;e.session.accessory=0;e.session.otherEquipment=0;e.session.pursuerHealth=500;return e;};
        for(int map:{54,56}){
            c.pendingMapTickets.erase(map);auto e=entryFor(map);const std::string door=map==54?"m54-e5":"m56-e5",monitor=map==54?"m54-e9":"m56-e7";
            e.positions[door]={5,5};e.positions[monitor]={8,8};e.playerTile={8,8};e.session.flags[23]=true;
            GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e,[](int,int hi){return hi;});room.update({});require(s.hp()==5000,"Door damage reads E5 position, not the monitor tile");
            c.setMediaDefinition("cues","domain-door-position",{{"kind","place_object"},{"object",door},{"tile",{8,8}}});ScenePresentation(room).cue("domain-door-position");room.update({});
            require(s.hp()==3999&&s.variable(24)==2&&s.variable(23)==999&&s.flag(8),"Door contact uses Actor30 attack, CE12 mode2 and S8 pulse");
            s.setFlag(15,true);room.update({});require(s.hp()==3999,"Higher source empty page retires the active door monitor");
        }
        {
            auto e=entryFor(54);e.playerTile={2,5};e.session.flags[156]=true;e.session.flags[160]=true;e.session.flags[18]=true;e.session.flags[13]=true;
            GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});
            for(int tick=1;tick<180;++tick){room.update({});}require(!s.flag(23),"Special M54 retains original Wait30 before breaking the door");room.update({});
            require(s.flag(23)&&room.foreground()==ForegroundKind::None,"Special door animation stays parallel and starts its original damage monitor");
            InputFrame move;move.movement=Direction::Right;room.update(move);require(room.world().player().motion.moving(),"Player can move during the parallel door action");
            for(int tick=181;tick<360;++tick){room.update({});}require(!s.flag(160)&&findCharacter(room.world(),"m54-e4").motion.tile()==TilePosition{2,5},"M54 skips defaultWait15 and uses entry-captured reappearance tile after Wait5");
        }
        {
            auto e=entryFor(56);e.playerTile={25,10};e.session.flags[156]=true;e.session.flags[160]=true;e.session.flags[18]=true;e.session.flags[13]=true;
            GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});require(findCharacter(room.world(),"m56-e4").motion.tile()==TilePosition{25,10},"M56 places pursuer before the first special wait");
            for(int tick=1;tick<60;++tick){room.update({});}room.update({});require(findCharacter(room.world(),"m56-e8").motion.tile()==TilePosition{14,9}&&s.flag(23),"M56 first Wait10 positions source object8 and invokes the live door sequence");
            for(int tick=0;tick<2400&&s.flag(160);++tick){room.update({});}
            require(!s.flag(160)&&s.flag(161)&&!s.flag(23),"All four M56 door actions complete once and set161 without defaultWait15/sound");
            require(findCharacter(room.world(),"m56-e9").motion.tile()==TilePosition{25,10}&&findCharacter(room.world(),"m56-e10").motion.tile()==TilePosition{25,10}&&findCharacter(room.world(),"m56-e11").motion.tile()==TilePosition{25,10},"Later special actions retain their independently captured player tile");
        }
        std::cout<<"Source door monitors and special reappearance checks passed\n";return 0;
    }
    std::vector<std::string> bodyFailures;
    const auto checkBody=[&](const std::string& name,const auto& body){
        try{body();std::cout<<"Pursuer E2 checked: "<<name<<'\n'<<std::flush;}
        catch(const std::exception& error){bodyFailures.push_back(name+": "+error.what());std::cerr<<"Pursuer E2 failure: "<<bodyFailures.back()<<'\n';}
    };
    if(argc==4){
        // Audited source-backed interface fixture; production content and gates stay intact.
        std::ifstream interfaceInput(argv[2]),cueInput(argv[3]);const auto interface=nlohmann::json::parse(interfaceInput),media=nlohmann::json::parse(cueInput);
        auto fullDefinition=*content.full;fullDefinition["runtime"]["player_recovery"]=interface.at("player_recovery");fullDefinition["runtime"]["player_damage"]=interface.at("player_damage");
        content.full=std::make_shared<const nlohmann::json>(std::move(fullDefinition));
        for(const auto& [id,cue]:media.at("cues").items()){content.setMediaDefinition("cues",id,cue);if(cue.at("kind")=="forced_route"){content.routes[id]=readRouteDefinition(cue);}}
    }
    {
        auto entry=content.entries.at("defeated-claimed");entry.map=27;entry.playerTile={1,1};entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();entry.session.flags[93]=true;
        GameSession session(content.items,entry.session,content.facts);CombatScene oldScene(content,session,entry);oldScene.update({});oldScene.deactivate();
        const auto p=findCharacter(oldScene.world(),"m27-e3").motion.position();std::set<std::string> resolved;
        require(oldScene.resolveAttack({{(p.x+.5)*16,(p.y+.5)*16},{0,0},AttackKind::Handgun,2,0,false},resolved).processed==0,"Deactivated scene rejects old attack");
    }
    if(content.enemies.contains("m52-e4")){
        // Development/domain entry only: clone the reviewed real M52 package;
        // production Map52 remains gated until 93 closes its natural flow.
        auto training=content;training.pendingMapTickets.erase(52);
        // Domain enemy checks exclude unrelated story autoruns even after93 opens naturally.
        for(auto& flow:training.catalog.at("flows")){if(flow.value("trigger",std::string{})=="autorun"){flow["trigger"]="call-only";}}
        if(training.enemies.at("m52-e4").rules.at("runtime_profile").contains("autonomy")){
            // Original P10 C20-25 maps random 0/1/2 to selector 0/1/1.
            for(int choice=0;choice<3;++choice){
                auto autonomous=training.entries.at("defeated-claimed");autonomous.map=52;autonomous.playerTile={24,16};
                autonomous.pages.clear();autonomous.positions.clear();autonomous.directions.clear();autonomous.through.clear();
                autonomous.positions["m52-e4"]={24,14};autonomous.directions["m52-e4"]=Direction::Down;
                autonomous.session.flags[18]=true;autonomous.session.flags[156]=true;autonomous.session.flags[157]=false;
                autonomous.session.flags[158]=false;autonomous.session.flags[160]=false;autonomous.session.flags[162]=false;
                autonomous.session.flags[21]=false;autonomous.session.flags[45]=false;autonomous.session.flags[15]=false;
                autonomous.session.variables[91]=0;autonomous.session.pursuerHealth=500;
                GameSession selected(training.items,autonomous.session,training.facts);
                CombatScene roam(training,selected,autonomous,[choice](int lo,int hi){return lo==0&&hi==2?choice:hi;});roam.update({});
                require(selected.flag(21)&&selected.variable(91)==(choice?1:0),"Real P10 autonomous selector maps0/1/2 to0/1/1 and sets S21 even for0");
                roam.update({});
                require(state(roam,"m52-e4").mode==(choice?EnemyMode::Windup:EnemyMode::Pursuit),"Selector0 keeps original autonomous page while1/2 both choose slash");
            }
            std::cout<<"Real M52 selector0/1/2 maps0/1/1 checked\n"<<std::flush;
        }
        auto entry=training.entries.at("defeated-claimed");entry.map=52;entry.playerTile={24,16};
        entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();
        entry.session.flags[156]=true;entry.session.flags[158]=false;entry.session.flags[160]=true;entry.session.flags[162]=false;
        entry.session.flags[47]=false;entry.session.mainWeapon=0;entry.session.baseAttack=80;entry.session.pursuerHealth=317;
        if(training.enemies.at("m52-e4").rules.at("runtime_profile").contains("blood")){
            auto bloodEntry=entry;bloodEntry.session.flags[18]=false;bloodEntry.session.flags[160]=false;
            bloodEntry.session.flags[21]=false;bloodEntry.session.flags[22]=false;bloodEntry.session.flags[47]=true;
            bloodEntry.session.pursuerHealth=500;bloodEntry.positions["m52-e4"]={24,14};
            GameSession bloodSession(training.items,bloodEntry.session,training.facts);
            CombatScene bleeding(training,bloodSession,bloodEntry,[](int lo,int hi){return lo==0&&hi==10?5:hi;});bleeding.update({});bleeding.takePresentation();
            require(hit(bleeding,"m52-e4",2).damage==100&&bloodSession.flag(22)&&bloodSession.variable(92)==1,"Original receiver starts CE15 small blood with S22 and V92 rather than only sound");
            bleeding.update({});auto bloodEvents=bleeding.takePresentation();
            require(std::any_of(bloodEvents.begin(),bloodEvents.end(),[](const auto& e){return e.id=="f4full-ce15-c25"&&e.actor=="m52-e4"&&std::abs(e.position.y-41.125)<.001;}),"CE15 first small blood frame resolves world foot plus434px from original body");
            bloodSession.setFlag(47,false);hit(bleeding,"m52-e4",2);bleeding.update({});bloodEvents=bleeding.takePresentation();
            require(std::any_of(bloodEvents.begin(),bloodEvents.end(),[](const auto& e){return e.id=="f4full-ce15-c31";})&&std::none_of(bloodEvents.begin(),bloodEvents.end(),[](const auto& e){return e.id=="f4full-ce15-c24";}),"Repeated S22 pulse resumes the current CE15 wait instead of restarting small blood");
            bleeding.update({});bleeding.update({});require(bloodSession.flag(22),"CE15 retains its fourth original Wait0 before final erase");
            bleeding.update({});bloodEvents=bleeding.takePresentation();
            require(!bloodSession.flag(22)&&std::any_of(bloodEvents.begin(),bloodEvents.end(),[](const auto& e){return e.id=="f4full-ce15-c44";}),"CE15 erases picture24 after four waits and clears S22 at its source ending");
        }
        GameSession session(training.items,entry.session,training.facts);
        CombatScene scene(training,session,entry,[](int lo,int hi){return lo==0&&hi==10?5:hi;});scene.update({});
        require(state(scene,"m52-e4").mode==EnemyMode::Dormant,"Real M52 reappearance uses the source wait");
        for(int n=0;n<240;++n){scene.update({});}
        require(state(scene,"m52-e4").damage==183&&!session.flag(160),"Real M52 source profile rebuilds local183 after Wait40");
        // P17 C5 moves after E2 has sampled this update; CE4 needs the next body loop.
        scene.update({});scene.takePresentation();
        require(hit(scene,"m52-e4").damage==50&&session.pursuerHealth()==267,"Real M52 receiver uses its source defense30 and full raw50");
        const auto events=scene.takePresentation();
        const auto expected=readSound(training.cue("m52-e4-p2-c5").at("sound"));
        require(std::any_of(events.begin(),events.end(),[&](const auto& e){return e.kind==PresentationEvent::Kind::Sound&&e.sound==expected;}),"Real M52 receiver emits its original C5 hit sound");
        session.setFlag(47,true);require(hit(scene,"m52-e4").damage==100,"Real M52 precision receiver gets the original doubled damage");
        const auto criticalEvents=scene.takePresentation();const auto criticalSound=readSound(training.cue("m52-e4-p2-c11").at("sound"));
        require(std::count_if(criticalEvents.begin(),criticalEvents.end(),[&](const auto& e){return e.kind==PresentationEvent::Kind::Sound&&e.sound==criticalSound;})==1,"Real M52 critical receiver emits original C11 Gdamage sound once");
        struct ReceiverSoundCase{int page;bool precision;int sounds;};
        for(const auto sample:std::array<ReceiverSoundCase,7>{{{2,false,0},{3,true,1},{3,false,0},{4,true,0},{5,false,1},{5,true,1},{6,true,0}}}){
            auto soundEntry=entry;soundEntry.session.flags[160]=false;soundEntry.session.flags[21]=false;
            soundEntry.session.flags[47]=sample.precision;soundEntry.session.pursuerHealth=500;
            GameSession soundSession(training.items,soundEntry.session,training.facts);
            CombatScene receiver(training,soundSession,soundEntry,[](int lo,int hi){return lo==0&&hi==10?5:hi;});receiver.update({});receiver.takePresentation();
            require(hit(receiver,"m52-e4",sample.page).processed==1,"Source receiver variation accepts one real hit");
            const auto soundEvents=receiver.takePresentation();
            require(std::count_if(soundEvents.begin(),soundEvents.end(),[&](const auto& e){return e.kind==PresentationEvent::Kind::Sound&&e.sound==criticalSound;})==sample.sounds,"Source P2/P3 critical, P5 unconditional and P4/P6 absent Gdamage sound branches");
        }
        std::cout<<"Real M52 reviewed-data domain entry checked; production gate retained\n";
        if(training.enemies.at("m52-e4").rules.at("runtime_profile").contains("attack_stages")){
            {
                entry.session.flags[160]=false;entry.session.flags[21]=false;entry.session.pursuerHealth=500;
                entry.positions["m52-e4"]=training.objects.at("m52-e4").tile;entry.directions["m52-e4"]=Direction::Down;
                auto idleEntry=entry;idleEntry.session.flags[18]=false;idleEntry.session.flags[157]=false;idleEntry.session.flags[45]=false;
                auto stationary=training;
                // Domain motion variation isolates the original idle cycle;
                // its presentation profile and source expectations stay intact.
                for(auto& page:stationary.objects.at("m52-e4").variants){page.autonomousRoute.steps.clear();}
                GameSession idleSession(stationary.items,idleEntry.session,stationary.facts);CombatScene idle(stationary,idleSession,idleEntry);idle.update({});
                auto pictures=idle.takePresentation();
                require(std::any_of(pictures.begin(),pictures.end(),[](const auto& e){return e.id=="m52-e2-p1-c85"&&e.actor=="m52-e4"&&std::abs(e.position.x-23.1875)<.001&&std::abs(e.position.y-12.6875)<.001;}),"Source initial nonzero tile starts idle walk pulse32 with down offset-13,-21");
                for(int n=1;n<16;++n){idle.update({});}pictures=idle.takePresentation();
                require(std::any_of(pictures.begin(),pictures.end(),[](const auto& e){return e.id=="m52-e2-p1-c82";}),"Idle cycle renders the second source picture at counter16");
                for(int n=16;n<32;++n){idle.update({});}idle.takePresentation();idle.update({});pictures=idle.takePresentation();
                require(std::any_of(pictures.begin(),pictures.end(),[](const auto& e){return e.id=="m52-e2-p1-c95";}),"Source pulse decays while stationary and renders rest after32 ticks");
            }
            checkBody("source action ending clears idle pulse after windup",[&]{
                auto stationary=training;for(auto& page:stationary.objects.at("m52-e4").variants){page.autonomousRoute.steps.clear();}
                auto pulseEntry=entry;pulseEntry.playerTile={1,1};pulseEntry.positions["m52-e4"]={20,12};pulseEntry.directions["m52-e4"]=Direction::Down;
                pulseEntry.session.flags[18]=false;pulseEntry.session.flags[157]=false;pulseEntry.session.flags[45]=false;
                pulseEntry.session.flags[160]=false;pulseEntry.session.flags[21]=false;pulseEntry.session.pursuerHealth=500;
                GameSession pulseSession(stationary.items,pulseEntry.session,stationary.facts);CombatScene pulse(stationary,pulseSession,pulseEntry);
                pulse.update({});pulse.takePresentation();pulseSession.setFlag(21,true);pulseSession.setVariable(91,1);pulse.update({});
                for(int n=0;n<166;++n){pulse.update({});}pulse.takePresentation();require(!pulseSession.flag(21),"Original down slash ends after166 updates without hitting a distant player");
                pulse.update({});const auto resumed=pulse.takePresentation();
                require(std::any_of(resumed.begin(),resumed.end(),[](const auto& e){return e.id=="m52-e2-p1-c95";})&&std::none_of(resumed.begin(),resumed.end(),[](const auto& e){return e.id=="m52-e2-p1-c85";}),"M52 E4 P14 C325-328 clear202/205/206 before S21OFF, so stationary first idle is rest");
            });
            {
                auto varied=training;auto& grid=varied.maps.at(52);grid.width=33;grid.height=25;grid.passageMasks.assign(33*25,15);varied.terrainIds.at(52).assign(33*25,0);
                for(auto& page:varied.objects.at("m52-e4").variants){page.definition.through=true;}
                auto& marker=varied.objects.at("m52-e34");auto nextPage=marker.variants.front();nextPage.page=2;nextPage.flags[799]=true;nextPage.definition.through=true;
                nextPage.autonomousRoute={10005,8,false,true,{{"right"}}};marker.variants.push_back(nextPage);
                // A domain autorun uses the public flow/page refresh seam. The
                // pursuer's original route and its nine-up/two-left oracle stay intact.
                varied.catalog["flows"]["domain-autonomy-refresh"]={{"object","m52-e34"},{"page",1},{"trigger","autorun"},{"steps",nlohmann::json::array({
                    {{"kind","flag"},{"id",18},{"value",true}},{{"kind","flag"},{"id",156},{"value",true}},{{"kind","flag"},{"id",799},{"value",true}},
                    {{"kind","cue"},{"cue","m52-e2-p1-c99"}}})}};
                auto routeEntry=entry;routeEntry.playerTile={1,1};routeEntry.positions["m52-e4"]={26,18};routeEntry.directions["m52-e4"]=Direction::Down;
                routeEntry.session.flags[18]=false;routeEntry.session.flags[156]=false;routeEntry.session.flags[157]=false;routeEntry.session.flags[799]=false;
                routeEntry.session.flags[45]=false;routeEntry.session.flags[160]=false;routeEntry.session.flags[21]=false;routeEntry.session.pursuerHealth=500;
                GameSession routed(varied.items,routeEntry.session,varied.facts);CombatScene routeScene(varied,routed,routeEntry,[](int lo,int hi){return hi;});
                int transitionTick=0;
                for(int tick=1;tick<=500;++tick){
                    const bool before=routed.flag(157);routeScene.update({});
                    if(!before&&routed.flag(157)){transitionTick=tick;break;}
                }
                const auto checkpoint=findCharacter(routeScene.world(),"m52-e4");
                if(!transitionTick||checkpoint.motion.tile()!=TilePosition{24,9}){
                    std::cerr<<"Owned-route first S157 tick="<<transitionTick<<" tile="<<checkpoint.motion.tile().x<<","<<checkpoint.motion.tile().y<<" facing="<<directionName(checkpoint.motion.facing())<<" moving="<<checkpoint.motion.moving()<<" remaining="<<checkpoint.motion.remainingSubunits()<<'\n';
                }
                require(transitionTick>0&&checkpoint.motion.tile()==TilePosition{24,9},"Page refresh preserves the original nine-up/two-left endpoint when S157 first turns on");
                require(findCharacter(routeScene.world(),"m52-e34").motion.tile()==TilePosition{6,15},"Claiming pursuer movement keeps another actor's ordinary page route on a larger grid");
            }
            {
                // Public Scene/World/presentation checks for one complete source FSM slice.
                // Explicit content variations isolate movement parameters and media;
                // original highest-page predicates and source profiles remain intact.
                std::vector<std::string> failures;
                const auto check=[&](const std::string& name,const auto& body){
                    try{body();std::cout<<"Pursuer FSM checked: "<<name<<'\n'<<std::flush;}
                    catch(const std::exception& error){failures.push_back(name+": "+error.what());std::cerr<<"Pursuer FSM failure: "<<failures.back()<<'\n';}
                };
                const auto sourceEntry=[&]{
                    auto e=entry;e.playerTile={1,1};e.positions["m52-e4"]={16,10};e.directions["m52-e4"]=Direction::Down;
                    for(int fact:{15,13,21,45,157,158,160,162}){e.session.flags[fact]=false;}
                    e.session.flags[18]=true;e.session.flags[156]=true;e.session.variables[91]=0;
                    e.session.hp=100;e.session.pursuerHealth=500;return e;
                };
                const auto mediaOnly=[&]{
                    auto c=training;for(auto& page:c.objects.at("m52-e4").variants){page.autonomousRoute.steps.clear();page.definition.through=true;}return c;
                };
                check("same command codes preserve cursor and use new page parameters",[&]{
                    auto c=mediaOnly();
                    for(auto& page:c.objects.at("m52-e4").variants){
                        if(page.page==10||page.page==11){
                            const int base=page.page==10?701:703;
                            page.autonomousRoute={10005,8,false,false,{{"up"},{"switch_on",{},{},base},{"left"},{"switch_on",{},{},base+1}}};
                        }
                    }
                    auto e=sourceEntry();for(int fact:{701,702,703,704}){e.session.flags[fact]=false;}
                    GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});s.setFlag(157,true);
                    for(int tick=0;tick<100&&!s.flag(704)&&!s.flag(702);++tick){room.update({});}
                    require(s.flag(703)&&s.flag(704)&&!s.flag(701)&&!s.flag(702),"Future switch_on parameters come from P11 while its first completed Up is not repeated");
                    require(findCharacter(room.world(),"m52-e4").motion.tile()==TilePosition{15,9},"Same-code page refresh keeps exactly one completed Up and one Left");
                });
                check("different command codes restart the new route",[&]{
                    auto c=mediaOnly();for(auto& page:c.objects.at("m52-e4").variants){
                        if(page.page==10){page.autonomousRoute={10005,8,false,false,{{"up"},{"switch_on",{},{},705}}};}
                        if(page.page==11){page.autonomousRoute={10005,8,false,false,{{"right"},{"switch_on",{},{},706}}};}
                    }
                    auto e=sourceEntry();e.session.flags[705]=false;e.session.flags[706]=false;
                    GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});s.setFlag(157,true);
                    for(int tick=0;tick<100&&!s.flag(706);++tick){room.update({});}
                    require(s.flag(706)&&!s.flag(705)&&findCharacter(room.world(),"m52-e4").motion.tile()==TilePosition{17,9},"Different-code P11 starts Right after the already committed Up finishes");
                });
                for(bool duringAttack:{false,true}){
                    check(duringAttack?"highest P17 interrupts slash with original Wait40":"highest P17 interrupts active pursuit with original Wait40",[&]{
                        auto c=mediaOnly();auto e=sourceEntry();e.positions["m52-e4"]={24,14};e.playerTile={24,16};
                        e.session.flags[21]=duringAttack;e.session.variables[91]=duringAttack?1:0;
                        GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e,[](int,int hi){return hi;});room.update({});
                        s.setFlag(160,true);room.update({});room.takePresentation();
                        for(int tick=0;tick<239;++tick){room.update({});}
                        require(s.hp()==100&&s.flag(160),"P17 retains all240 source Wait updates and old slash cannot damage during its wait");
                        room.update({});const auto events=room.takePresentation();
                        require(!s.flag(160)&&findCharacter(room.world(),"m52-e4").motion.tile()==TilePosition{24,16},"Higher P17 places the actor at its captured player tile after Wait40");
                        const auto sound=readSound(c.cue("m52-e4-p17-c4").at("sound"));
                        require(std::count_if(events.begin(),events.end(),[&](const auto& event){return event.kind==PresentationEvent::Kind::Sound&&event.sound==sound;})==1,"P17 commits its source reappearance sound once");
                    });
                }
                check("empty P19 and manager P4 keep pictures then restart setup",[&]{
                    auto c=mediaOnly();auto e=sourceEntry();e.session.flags[21]=true;e.session.variables[91]=1;e.playerTile={16,12};
                    GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});room.takePresentation();
                    s.setFlag(162,true);for(int tick=0;tick<80;++tick){room.update({});}const auto stopped=room.takePresentation();
                    require(s.hp()==100&&state(room,"m52-e4").mode==EnemyMode::Dormant,"Highest empty page retires the old attack before its damage stage");
                    require(std::none_of(stopped.begin(),stopped.end(),[&](const auto& event){return event.kind==PresentationEvent::Kind::Cue&&c.cue(event.id).at("kind")=="erase_picture";}),"Source empty manager P4 does not erase existing pictures");
                    s.setFlag(162,false);room.update({});const auto resumed=room.takePresentation();
                    require(std::any_of(resumed.begin(),resumed.end(),[](const auto& event){return event.id=="m52-e2-p2-c9";}),"Leaving empty P4 restarts same-direction slash Show rather than retaining old interpreter setup");
                });
                struct PictureDirection{Direction direction;int frameX;const char* slashShow;const char* sweepShow;};
                for(const auto sample:std::array<PictureDirection,4>{{{Direction::Up,13,"m52-e2-p2-c35","m52-e2-p3-c36"},{Direction::Right,-10,"m52-e2-p2-c26","m52-e2-p3-c26"},{Direction::Down,-13,"m52-e2-p2-c9","m52-e2-p3-c6"},{Direction::Left,10,"m52-e2-p2-c17","m52-e2-p3-c16"}}}){
                    for(int selector:{1,2}){
                        check("source setup then frame offset "+std::string(directionName(sample.direction))+" selector"+std::to_string(selector),[&]{
                            auto c=mediaOnly();auto e=sourceEntry();e.directions["m52-e4"]=sample.direction;e.session.flags[21]=true;e.session.variables[91]=selector;
                            GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});const auto events=room.takePresentation();
                            const std::string show=selector==1?sample.slashShow:sample.sweepShow,move=selector==1?"m52-e2-p2-c72":"m52-e2-p3-c76";
                            require(std::any_of(events.begin(),events.end(),[&](const auto& event){return event.id==show&&std::abs(event.position.x-16)<.001&&std::abs(event.position.y-8.6875)<.001;}),"Attack Show uses source world foot offset0,-21 before any directional frame shift");
                            require(std::any_of(events.begin(),events.end(),[&](const auto& event){return event.id==move&&std::abs(event.position.x-(16+sample.frameX/16.0))<.001&&std::abs(event.position.y-8.6875)<.001;}),"Immediate attack Move uses the independently sourced directional offset");
                        });
                    }
                }
                for(Direction facing:{Direction::Up,Direction::Right,Direction::Down,Direction::Left}){
                    for(int comparison:{-1,0,1}){
                        check("terrain3 strict comparison "+std::string(directionName(facing))+" "+std::to_string(comparison),[&]{
                            auto c=mediaOnly();auto& grid=c.maps.at(52);grid.width=33;grid.height=25;grid.passageMasks.assign(33*25,15);c.terrainIds.at(52).assign(33*25,0);
                            auto e=sourceEntry();e.directions["m52-e4"]=facing;
                            const bool vertical=facing==Direction::Up||facing==Direction::Down;
                            e.playerTile=vertical?TilePosition{16-comparison*8,comparison?10:20}:TilePosition{comparison?16:24,10-comparison*8};
                            const auto front=adjacentTile({16,10},facing);c.terrainIds.at(52)[front.y*33+front.x]=3;
                            GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});room.update({});
                            const auto actor=findCharacter(room.world(),"m52-e4");
                            require(comparison?actor.motion.moving():!actor.motion.moving(),"Terrain sidestep requires strict actor/player axis inequality; equality produces no route");
                            if(comparison){const auto expected=vertical?(comparison<0?Direction::Right:Direction::Left):(comparison<0?Direction::Down:Direction::Up);require(actor.motion.facing()==expected,"Terrain turn moves along source comparison axis for all four starting facings");}
                        });
                    }
                }
                for(Direction facing:{Direction::Up,Direction::Right,Direction::Down,Direction::Left}){
                    for(bool commonAxis:{false,true}){
                        check("P12 common-axis constant1 "+std::string(directionName(facing))+(commonAxis?" aligned":" diagonal"),[&]{
                            auto c=mediaOnly();auto e=sourceEntry();e.directions["m52-e4"]=facing;e.session.flags[45]=true;
                            auto p=TilePosition{16,10};for(int n=0;n<2;++n){p=adjacentTile(p,facing);}
                            if(!commonAxis){if(facing==Direction::Up||facing==Direction::Down){++p.x;}else{++p.y;}}e.playerTile=p;
                            GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e,[](int lo,int){return lo;});room.update({});
                            require(s.flag(21)==commonAxis&&(!commonAxis||s.variable(91)==1),"Original approach P12 requires the shared axis and commits constant1 independently of random0");
                        });
                    }
                }
                if(!failures.empty()){throw std::runtime_error("Pursuer complete FSM slice has "+std::to_string(failures.size())+" failed public scenarios");}
            }
            if(training.enemies.at("m52-e4").rules.at("runtime_profile").contains("burn")){
                std::vector<std::string> failures;
                const auto check=[&](const std::string& name,const auto& body){try{body();std::cout<<"Pursuer burn checked: "<<name<<'\n'<<std::flush;}catch(const std::exception& error){failures.push_back(name+": "+error.what());std::cerr<<"Pursuer burn failure: "<<failures.back()<<'\n';}};
                const auto sourceContent=[&]{
                    auto c=training;c.facts.variables[301]="burn.slot-1.lifetime";
                    for(auto& page:c.objects.at("m52-e4").variants){page.autonomousRoute.steps.clear();page.definition.through=true;}
                    auto& grid=c.maps.at(52);grid.width=33;grid.height=25;grid.passageMasks.assign(33*25,15);c.terrainIds.at(52).assign(33*25,0);return c;
                };
                const auto sourceEntry=[&](int counter){
                    auto e=entry;e.playerTile={1,1};e.positions["m52-e4"]={16,10};e.directions["m52-e4"]=Direction::Down;
                    for(int fact:{13,15,21,45,157,158,160,162}){e.session.flags[fact]=false;}
                    e.session.flags[18]=true;e.session.flags[156]=true;e.session.flags[37]=true;e.session.variables[91]=0;
                    e.session.variables.erase(301);e.session.worldValues["burn.slot-1.lifetime"]=counter;e.session.hp=100;e.session.pursuerHealth=500;return e;
                };
                // These domain fixtures begin after the preceding E2 sample.
                // Runtime source work is deliberately not a SessionEntry/Load field.
                const auto primeSourceBody=[](GameSession& s,const CombatEntry& e){
                    if(e.positions.at("m52-e4")==TilePosition{16,10}){s.setVariable(83,264);s.setVariable(84,155);}
                };
                for(int initial:{23,24}){
                    check("persistent lifetime "+std::to_string(initial)+" plus2 ends at25/26 then zero",[&]{
                        auto c=sourceContent();auto e=sourceEntry(initial);GameSession s(c.items,e.session,c.facts);primeSourceBody(s,e);CombatScene room(c,s,e,[](int,int hi){return hi;});room.update({});
                        for(int tick=0;tick<5;++tick){room.update({});}
                        require(s.variable(301)==initial&&s.flag(37)&&state(room,"m52-e4").damage==0,"Six source waits retain the previous persistent lifetime before damage");
                        room.update({});const auto events=room.takePresentation();
                        require(s.variable(301)==0&&!s.flag(37)&&state(room,"m52-e4").damage==4&&s.pursuerHealth()==500,"CE24 adds4 only locally and resets the sole persistent counter after25/26");
                        require(std::count_if(events.begin(),events.end(),[](const auto& event){return event.id=="f4full-ce24-c81";})==1,"Terminal CE24 erases picture32 once");
                    });
                }
                check("counter24 remains active until the next source cycle",[&]{
                    auto c=sourceContent();auto e=sourceEntry(23);GameSession s(c.items,e.session,c.facts);primeSourceBody(s,e);CombatScene room(c,s,e,[](int lo,int){return lo;});
                    for(int tick=0;tick<7;++tick){room.update({});}
                    require(s.variable(301)==24&&s.flag(37)&&state(room,"m52-e4").damage==4,"23 plus1 must remain24 and active rather than ending or clamping a terminal value");
                    for(int tick=0;tick<6;++tick){room.update({});}
                    require(s.variable(301)==0&&!s.flag(37)&&state(room,"m52-e4").damage==8,"The next six waits reach25 then reset0");
                });
                check("three fire frames follow original two-wait boundaries",[&]{
                    auto c=sourceContent();auto e=sourceEntry(23);GameSession s(c.items,e.session,c.facts);primeSourceBody(s,e);CombatScene room(c,s,e,[](int,int hi){return hi;});
                    const std::array<const char*,3> moves{"f4full-ce24-c15","f4full-ce24-c38","f4full-ce24-c61"};const std::array<double,3> expectedY{29.75,14.75,-.25};
                    for(int frame=0;frame<3;++frame){
                        room.update({});const auto events=room.takePresentation();
                        require(std::count_if(events.begin(),events.end(),[&](const auto& event){return event.id==moves[frame]&&event.actor=="m52-e4"&&std::abs(event.position.x-16)<.001&&std::abs(event.position.y-expectedY[frame])<.001;})==1,"Each source fire frame samples World actor foot with its independent316/76/-164 pixel offset");
                        if(frame==0){require(std::count_if(events.begin(),events.end(),[](const auto& event){return event.id=="f4full-ce24-c1";})==1,"CE24 establishes its invisible source fire picture at entry");}
                        room.update({});room.takePresentation();
                    }
                    room.update({});const auto ended=room.takePresentation();require(std::any_of(ended.begin(),ended.end(),[](const auto& event){return event.id=="f4full-ce24-c81";}),"Six waits end with original erase32 instead of another visible frame");
                });
                check("ordinary session candidate and OFF retain the remaining three waits",[&]{
                    auto c=sourceContent();auto e=sourceEntry(3);GameSession original(c.items,e.session,c.facts);primeSourceBody(original,e);CombatScene first(c,original,e,[](int,int hi){return hi;});
                    for(int tick=0;tick<4;++tick){first.update({});}first.deactivate();auto candidate=original;candidate.resetRoomFacts();
                    CombatScene next(c,candidate,e,[](int,int hi){return hi;});for(int tick=0;tick<20;++tick){next.update({});}
                    require(candidate.variable(301)==3&&state(next,"m52-e4").damage==0,"CE55 room reset preserves V301 and OFF pauses pending waits without local damage");
                    candidate.setFlag(37,true);next.update({});next.update({});require(candidate.variable(301)==3&&state(next,"m52-e4").damage==0,"Ordinary candidate cannot restart or shorten its remaining three waits");
                    next.update({});require(candidate.variable(301)==5&&state(next,"m52-e4").damage==4&&original.variable(301)==3,"Third resumed wait commits only the new scene's local damage and persistent counter");
                    first.update({});require(original.variable(301)==3,"Deactivated old Scene cannot advance candidate burn progress");
                });
                check("Load preserves lifetime but reconstructs six waits",[&]{
                    auto c=sourceContent();auto e=sourceEntry(3);GameSession original(c.items,e.session,c.facts);primeSourceBody(original,e);CombatScene first(c,original,e,[](int,int hi){return hi;});
                    for(int tick=0;tick<4;++tick){first.update({});}first.deactivate();GameSession loaded(c.items,original.exportState(),c.facts);primeSourceBody(loaded,e);CombatScene next(c,loaded,e,[](int,int hi){return hi;});
                    for(int tick=0;tick<6;++tick){next.update({});}require(loaded.variable(301)==3&&state(next,"m52-e4").damage==0,"Load does not restore the original interpreter's pending three waits");
                    next.update({});require(loaded.variable(301)==5&&state(next,"m52-e4").damage==4,"Fresh finite entry commits after all six reconstructed waits");
                });
                check("both body coordinates nonpositive terminate and reset counter",[&]{
                    auto c=sourceContent();auto e=sourceEntry(23);e.positions["m52-e4"]={0,0};e.playerTile={30,22};GameSession s(c.items,e.session,c.facts);primeSourceBody(s,e);CombatScene room(c,s,e);room.update({});const auto events=room.takePresentation();
                    require(s.variable(301)==0&&!s.flag(37)&&state(room,"m52-e4").damage==0,"CE24 C6-9 resets V301 before its early BreakLoop without applying damage");
                    require(std::any_of(events.begin(),events.end(),[](const auto& event){return event.id=="f4full-ce24-c81";}),"Early BreakLoop still reaches erase32 and S37OFF");
                });
                check("parallel CE24 continues through authoritative foreground once per update",[&]{
                    auto c=sourceContent();auto e=sourceEntry(23);e.session.pursuerHealth=317;
                    c.catalog["flows"]["domain-burn-foreground"]={{"object","m52-e34"},{"page",1},{"trigger","autorun"},{"steps",nlohmann::json::array({{{"kind","cue"},{"cue","m52-e4-p17-c3"}}})}};
                    GameSession s(c.items,e.session,c.facts);primeSourceBody(s,e);CombatScene room(c,s,e,[](int,int hi){return hi;});for(int tick=0;tick<6;++tick){room.update({});}
                    require(room.foreground()==ForegroundKind::Content&&s.variable(301)==23,"Foreground does not authorize autonomy and six parallel waits still do not commit early");
                    room.update({});require(s.variable(301)==0&&!s.flag(37)&&state(room,"m52-e4").damage==187&&s.pursuerHealth()==317,"Parallel burn adds its single4 to the initialized local183 while authoritative foreground remains active");
                    require(findCharacter(room.world(),"m52-e4").motion.tile()==TilePosition{16,10},"Parallel update does not also advance the enemy's autonomous route");
                });
                const auto openMessage=[&](CombatContent& c,CombatScene& room){
                    c.catalog["flows"]["domain-picture-message"]={{"object","m52-e34"},{"page",1},{"trigger","autorun"},{"steps",nlohmann::json::array({{{"kind","message"},{"message","m4-e4-p7-c3"}}})}};
                    room.update({});c.catalog["flows"]["domain-picture-message"]["trigger"]="call-only";
                    require(room.dialogue().view().phase!=DialoguePhase::Hidden,"The public foreground opens an existing audited source message");room.takePresentation();
                };
                const auto closeMessage=[&](CombatScene& room){
                    for(int tick=0;tick<1000&&room.dialogue().view().phase!=DialoguePhase::AwaitingInput;++tick){room.update({});}
                    require(room.dialogue().view().phase==DialoguePhase::AwaitingInput,"The source message reaches its user confirmation boundary");
                    InputFrame confirm;confirm.buttons[static_cast<std::size_t>(InputAction::Confirm)]={true,true,false};room.update(confirm);
                    for(int tick=0;tick<1000&&room.dialogue().view().phase!=DialoguePhase::Hidden;++tick){room.update({});}
                    require(room.dialogue().view().phase==DialoguePhase::Hidden,"Confirm and the source closing/cooldown release the actual message window");room.takePresentation();
                };
                const auto cueCount=[](const auto& events,const std::string& cue){return std::count_if(events.begin(),events.end(),[&](const auto& event){return event.id==cue;});};
                const auto cueAt=[](const auto& events,const std::string& cue,double x,double y){return std::any_of(events.begin(),events.end(),[&](const auto& event){return event.id==cue&&std::abs(event.position.x-x)<.001&&std::abs(event.position.y-y)<.001;});};
                check("native initial Show waits before any source Wait",[&]{
                    auto c=sourceContent();auto e=sourceEntry(3);e.session.flags[37]=false;GameSession s(c.items,e.session,c.facts);primeSourceBody(s,e);CombatScene room(c,s,e);
                    openMessage(c,room);s.setVariable(93,80);s.setVariable(94,96);s.setFlag(37,true);
                    for(int tick=0;tick<12;++tick){room.update({});}
                    const auto stalled=room.takePresentation();
                    require(cueCount(stalled,"f4full-ce24-c1")==0&&cueCount(stalled,"f4full-ce24-c15")==0&&s.burnWaitRemaining(24)==0&&s.variable(301)==3&&state(room,"m52-e4").damage==0,"Native C1 guard stalls before Show, sampling and all six Wait0 commands");
                    closeMessage(room);room.update({});const auto resumed=room.takePresentation();
                    require(cueAt(resumed,"f4full-ce24-c1",4.5,5)&&cueAt(resumed,"f4full-ce24-c15",16,29.75)&&s.burnWaitRemaining(24)==6,"Unblocked C1 reads old global scratch, then the loop samples frame1 and starts its waits");
                    require(sampledAt(resumed,"f4full-ce24-c1",80,96),"CE24 initial Show publishes original pixel80/96 after its native guard");
                });
                check("ordinary Wait finishes before pending Move2, which rereads scratch",[&]{
                    auto c=sourceContent();auto e=sourceEntry(3);GameSession s(c.items,e.session,c.facts);primeSourceBody(s,e);CombatScene room(c,s,e);
                    room.update({});room.takePresentation();openMessage(c,room);room.update({});
                    require(s.burnWaitRemaining(24)==4&&s.variable(301)==3&&state(room,"m52-e4").damage==0&&cueCount(room.takePresentation(),"f4full-ce24-c38")==0,"C24/25 Wait0 complete during message before C38 becomes blocked");
                    s.setVariable(93,80);s.setVariable(94,96);for(int tick=0;tick<10;++tick){room.update({});}
                    require(s.burnWaitRemaining(24)==4&&s.variable(93)==80&&s.variable(94)==96,"A pending Move does not rerun its already committed ControlVariables or advance another Wait");
                    closeMessage(room);room.update({});const auto resumed=room.takePresentation();
                    require(cueAt(resumed,"f4full-ce24-c38",4.5,5)&&s.burnWaitRemaining(24)==4,"Move parameter reads occur after native guard and use the latest shared source work values");
                    require(sampledAt(resumed,"f4full-ce24-c38",80,96),"CE24 pending Move publishes overwritten global work80/96 in pixels rather than stale sampled actor coordinates");
                });
                check("real CE15 writes shared scratch before its own blocked Show",[&]{
                    auto c=sourceContent();auto e=sourceEntry(3);GameSession s(c.items,e.session,c.facts);primeSourceBody(s,e);CombatScene room(c,s,e);
                    room.update({});openMessage(c,room);room.update({});room.takePresentation();s.setVariable(92,1);s.setFlag(22,true);room.update({});
                    const auto blocked=room.takePresentation();
                    require(s.variable(93)==264&&s.variable(94)==610&&s.flag(22)&&cueCount(blocked,"f4full-ce15-c24")==0&&cueCount(blocked,"f4full-ce24-c38")==0,"CE15 C19-23 overwrite CE24 work93/94 but both picture commands remain blocked");
                    closeMessage(room);room.update({});const auto resumed=room.takePresentation();
                    require(cueAt(resumed,"f4full-ce24-c38",16,37.125)&&cueCount(resumed,"f4full-ce15-c24")==1,"CE24 pending Move observes the real parallel CE15 write rather than a frozen actor anchor");
                    require(sampledAt(resumed,"f4full-ce15-c24",264,610)&&sampledAt(resumed,"f4full-ce24-c38",264,610),"Both released common pictures publish real CE15 shared work264/610 in screen pixels");
                });
                check("terminal damage and counter precede pending Erase and S37OFF",[&]{
                    auto c=sourceContent();auto e=sourceEntry(23);GameSession s(c.items,e.session,c.facts);primeSourceBody(s,e);CombatScene room(c,s,e,[](int,int hi){return hi;});
                    for(int tick=0;tick<5;++tick){room.update({});}room.takePresentation();openMessage(c,room);room.update({});const auto blocked=room.takePresentation();
                    require(s.variable(301)==0&&state(room,"m52-e4").damage==4&&s.flag(37)&&cueCount(blocked,"f4full-ce24-c81")==0,"C72-76 commit once before native C81 guard while C82 S37OFF remains pending");
                    for(int tick=0;tick<12;++tick){room.update({});}const auto held=room.takePresentation();
                    require(s.flag(37)&&state(room,"m52-e4").damage==4&&s.variable(301)==0&&cueCount(held,"f4full-ce24-c1")==0&&cueCount(held,"f4full-ce24-c81")==0,"Pending terminal Erase cannot restart the loop or repeat local damage");
                    closeMessage(room);room.update({});const auto ended=room.takePresentation();
                    require(!s.flag(37)&&state(room,"m52-e4").damage==4&&cueCount(ended,"f4full-ce24-c81")==1&&cueCount(ended,"f4full-ce24-c1")==0,"Only the released Erase commits before S37OFF");
                });
                check("OFF and ordinary candidate retain pending Move but CE55 clears scratch",[&]{
                    auto c=sourceContent();auto e=sourceEntry(3);GameSession original(c.items,e.session,c.facts);primeSourceBody(original,e);CombatScene first(c,original,e);
                    first.update({});openMessage(c,first);first.update({});first.deactivate();auto candidate=original;candidate.resetRoomFacts();c.catalog["flows"].erase("domain-picture-message");
                    CombatScene next(c,candidate,e);for(int tick=0;tick<12;++tick){next.update({});}next.takePresentation();
                    require(candidate.variable(301)==3&&candidate.burnWaitRemaining(24)==4&&candidate.variable(93)==0&&candidate.variable(94)==0,"CE55 clears work values and OFF pauses the copied pending Move independently of its wait");
                    candidate.setFlag(37,true);next.update({});const auto resumed=next.takePresentation();
                    require(cueAt(resumed,"f4full-ce24-c38",-.5,-1)&&cueCount(resumed,"f4full-ce24-c1")==0&&candidate.burnWaitRemaining(24)==4,"Copied source cursor resumes Move with cleared global work instead of resampling or reShow");
                    first.update({});require(original.variable(301)==3,"Retired source Scene cannot modify copied progress");
                });
                check("Load reconstructs initial Show rather than pending Move",[&]{
                    auto c=sourceContent();auto e=sourceEntry(3);GameSession original(c.items,e.session,c.facts);primeSourceBody(original,e);CombatScene first(c,original,e);
                    first.update({});openMessage(c,first);first.update({});first.deactivate();c.catalog["flows"].erase("domain-picture-message");
                    GameSession loaded(c.items,original.exportState(),c.facts);primeSourceBody(loaded,e);CombatScene next(c,loaded,e);next.update({});const auto events=next.takePresentation();
                    require(cueAt(events,"f4full-ce24-c1",-.5,-1)&&cueAt(events,"f4full-ce24-c15",16,29.75)&&cueCount(events,"f4full-ce24-c38")==0&&loaded.burnWaitRemaining(24)==6&&loaded.variable(301)==3,"Load keeps the persistent lifetime but recreates C1, zero work and all six waits");
                });
                check("nonterminal source Loop never repeats entry Show",[&]{
                    auto c=sourceContent();auto e=sourceEntry(3);GameSession s(c.items,e.session,c.facts);primeSourceBody(s,e);CombatScene room(c,s,e,[](int lo,int){return lo;});
                    for(int tick=0;tick<13;++tick){room.update({});}const auto events=room.takePresentation();
                    require(cueCount(events,"f4full-ce24-c1")==1&&cueCount(events,"f4full-ce24-c15")==3&&state(room,"m52-e4").damage==8&&s.variable(301)==5,"C1 is outside Loop; two completed cycles only repeat sampled Move1");
                });
                check("native pictures also wait for the actual choice window",[&]{
                    auto c=sourceContent();auto e=sourceEntry(3);e.session.flags[37]=false;
                    c.catalog["flows"]["domain-picture-choice"]={{"object","m52-e34"},{"page",1},{"trigger","autorun"},{"steps",nlohmann::json::array({{{"kind","choice"},{"cancel_option",0},{"options",nlohmann::json::array({{{"label","继续"},{"steps",nlohmann::json::array()}}})}}})}};
                    GameSession s(c.items,e.session,c.facts);primeSourceBody(s,e);CombatScene room(c,s,e);room.update({});c.catalog["flows"]["domain-picture-choice"]["trigger"]="call-only";room.takePresentation();
                    require(room.inputPromptView().phase==DialoguePhase::AwaitingInput,"Public foreground starts a real choice window");s.setFlag(37,true);
                    for(int tick=0;tick<8;++tick){room.update({});}const auto blocked=room.takePresentation();
                    require(s.burnWaitRemaining(24)==0&&state(room,"m52-e4").damage==0&&cueCount(blocked,"f4full-ce24-c1")==0,"The message pool choice state also guards C1 before its ordinary waits");
                    InputFrame confirm;confirm.buttons[static_cast<std::size_t>(InputAction::Confirm)]={true,true,false};room.update(confirm);room.takePresentation();room.update({});
                    require(cueCount(room.takePresentation(),"f4full-ce24-c1")==1&&s.burnWaitRemaining(24)==6,"A hidden completed choice permits C1 even before its old request identity is consumed");
                });
                check("CE15 final Erase retains S22 while message blocks it",[&]{
                    auto c=sourceContent();auto e=sourceEntry(3);e.session.flags[37]=false;e.session.flags[22]=true;e.session.variables[92]=1;
                    GameSession s(c.items,e.session,c.facts);primeSourceBody(s,e);CombatScene room(c,s,e);for(int tick=0;tick<3;++tick){room.update({});}room.takePresentation();openMessage(c,room);room.update({});
                    const auto blocked=room.takePresentation();require(s.flag(22)&&cueCount(blocked,"f4full-ce15-c44")==0,"CE15 fourth Wait completes but native Erase guard preserves the source activation flag");
                    for(int tick=0;tick<8;++tick){room.update({});}require(s.flag(22)&&cueCount(room.takePresentation(),"f4full-ce15-c24")==0,"A blocked CE15 ending does not restart its entry picture");
                    closeMessage(room);room.update({});const auto ended=room.takePresentation();
                    require(!s.flag(22)&&cueCount(ended,"f4full-ce15-c44")==1,"CE15 only clears S22 after its original Erase succeeds");
                });
                check("CE15 OFF and candidate preserve pending frame, Load rebuilds entry",[&]{
                    auto c=sourceContent();auto e=sourceEntry(3);e.session.flags[37]=false;e.session.flags[22]=true;e.session.variables[92]=1;
                    GameSession original(c.items,e.session,c.facts);primeSourceBody(original,e);CombatScene first(c,original,e);first.update({});openMessage(c,first);first.update({});first.deactivate();auto candidate=original;candidate.resetRoomFacts();c.catalog["flows"].erase("domain-picture-message");
                    CombatScene next(c,candidate,e);for(int tick=0;tick<8;++tick){next.update({});}next.takePresentation();candidate.setVariable(92,1);candidate.setFlag(22,true);next.update({});const auto resumed=next.takePresentation();
                    require(cueAt(resumed,"f4full-ce15-c36",-.5,-1)&&cueCount(resumed,"f4full-ce15-c24")==0,"Ordinary candidate resumes the pending CE15 frame with CE55-cleared global scratch rather than another Show");
                    GameSession loaded(c.items,original.exportState(),c.facts);primeSourceBody(loaded,e);CombatScene afterLoad(c,loaded,e);afterLoad.update({});const auto fresh=afterLoad.takePresentation();
                    require(cueCount(fresh,"f4full-ce15-c24")==1&&cueCount(fresh,"f4full-ce15-c25")==1&&cueCount(fresh,"f4full-ce15-c36")==0,"Load restarts the finite CE15 entry and does not save its original interpreter cursor");
                });
                check("CE15 later blood type first Show inherits the previous branch scratch",[&]{
                    auto c=sourceContent();auto e=sourceEntry(3);e.session.flags[37]=false;e.session.flags[22]=true;e.session.variables[92]=1;
                    GameSession s(c.items,e.session,c.facts);primeSourceBody(s,e);CombatScene room(c,s,e);room.update({});room.takePresentation();s.setVariable(92,2);
                    for(int tick=0;tick<3;++tick){room.update({});}room.takePresentation();room.update({});const auto promoted=room.takePresentation();
                    require(cueCount(promoted,"f4full-ce15-c44")==1&&cueAt(promoted,"f4full-ce15-c50",16,20.5625)&&s.variable(94)==345&&s.flag(22),"C47-50 reuse type1 frame4 work94=-110 then subtract25/add480, rather than rerunning C19/20");
                    require(sampledAt(promoted,"f4full-ce15-c50",264,345),"Promoted CE15 Show publishes inherited work264/345 in pixels");
                    room.update({});const auto second=room.takePresentation();
                    require(cueAt(second,"f4full-ce15-c57",16,22.125)&&s.variable(94)==370,"The later type's second frame resumes its own source body sample before moving");
                });
                if(!failures.empty()){throw std::runtime_error("Pursuer complete burn slice has "+std::to_string(failures.size())+" failed public scenarios");}
            }
            if(training.enemies.at("m52-e4").rules.at("runtime_profile").contains("presentation_lifecycle")){
                const auto sourceContent=[&]{
                    auto c=training;
                    for(auto& page:c.objects.at("m52-e4").variants){page.autonomousRoute.steps.clear();page.definition.through=true;}
                    auto& grid=c.maps.at(52);grid.width=33;grid.height=25;grid.passageMasks.assign(33*25,15);c.terrainIds.at(52).assign(33*25,0);
                    c.facts.variables[301]="burn.slot-1.lifetime";return c;
                };
                const auto sourceEntry=[&]{
                    auto e=entry;e.playerTile={1,1};e.positions["m52-e4"]={16,10};e.directions["m52-e4"]=Direction::Down;
                    for(int fact:{13,15,21,22,37,45,157,158,160,162}){e.session.flags[fact]=false;}
                    e.session.flags[18]=true;e.session.flags[156]=true;e.session.variables[91]=0;e.session.hp=100;e.session.pursuerHealth=500;return e;
                };
                const auto cueCount=[](const auto& events,const std::string& cue){return std::count_if(events.begin(),events.end(),[&](const auto& event){return event.id==cue;});};
                const auto cueAt=[](const auto& events,const std::string& cue,double x,double y){return std::any_of(events.begin(),events.end(),[&](const auto& event){return event.id==cue&&std::abs(event.position.x-x)<.001&&std::abs(event.position.y-y)<.001;});};
                const auto messageFlow=[&](CombatContent& c,bool move){
                    auto steps=nlohmann::json::array();
                    if(move){
                        c.routes["domain-body-right"]={4,8,false,true,{{"right"}}};
                        c.setMediaDefinition("cues","domain-body-right",{{"kind","forced_route"}});
                        steps.push_back({{"kind","cue"},{"cue","domain-body-right"}});
                    }
                    steps.push_back({{"kind","message"},{"message","m4-e4-p7-c3"}});
                    c.catalog["flows"]["domain-body-message"]={{"object","m52-e34"},{"page",1},{"trigger","autorun"},{"steps",steps}};
                };
                const auto closeMessage=[](CombatScene& room){
                    for(int tick=0;tick<1000&&room.dialogue().view().phase!=DialoguePhase::AwaitingInput;++tick){room.update({});}
                    require(room.dialogue().view().phase==DialoguePhase::AwaitingInput,"Body source message reaches confirmation");
                    InputFrame confirm;confirm.buttons[static_cast<std::size_t>(InputAction::Confirm)]={true,true,false};room.update(confirm);
                    for(int tick=0;tick<1000&&room.dialogue().view().phase!=DialoguePhase::Hidden;++tick){room.update({});}
                    require(room.dialogue().view().phase==DialoguePhase::Hidden,"Body source message closes without crossing first Hidden");room.takePresentation();
                };
                // CN1.00 V201 is the E2 source sample, distinct from World facing.
                // A changed manager's empty highest page isolates inherited work;
                // no enemy page, action profile or permission guard is replaced.
                const auto stopBody=[](CombatContent& c){
                    auto empty=c.objects.at("m52-e2").variants.back();empty.page=5;empty.flags={{777,true}};
                    c.objects.at("m52-e2").variants.push_back(empty);
                };
                const auto faceActor=[](CombatContent& c,CombatScene& room,Direction direction){
                    c.routes["domain-cached-facing"]={4,8,false,true,{{"face_"+std::string(directionName(direction))}}};
                    c.setMediaDefinition("cues","domain-cached-facing",{{"kind","forced_route"}});
                    ScenePresentation(room).cue("domain-cached-facing");
                };
                for(const auto sample:std::array<std::pair<Direction,int>,4>{{{Direction::Down,2},{Direction::Left,4},{Direction::Right,6},{Direction::Up,8}}}){
                    checkBody("E2 source direction sample "+std::string(directionName(sample.first)),[&]{
                        auto c=sourceContent();auto e=sourceEntry();e.directions["m52-e4"]=sample.first;
                        GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});
                        require(s.variable(201)==sample.second,"E2 P1 C11 samples original2/4/6/8 before picture commands and E4 autonomy");
                    });
                }
                checkBody("source direction is transient across ordinary copy CE55 and Load",[&]{
                    auto c=sourceContent();auto e=sourceEntry();GameSession s(c.items,e.session,c.facts);s.setVariable(201,6);
                    auto candidate=s;require(candidate.variable(201)==6,"Ordinary candidate copy retains the already sampled source direction");
                    const auto saved=s.exportState();require(!saved.variables.contains(201),"V201 is not exported as persistent legacy state");
                    GameSession loaded(c.items,saved,c.facts);require(loaded.variable(201)==0,"Load reconstructs unsampled direction0");
                    candidate.resetRoomFacts();require(candidate.variable(201)==0&&s.variable(201)==6,"CE55 C27 clears201 in the candidate without changing the old Session");
                    auto invalid=e.session;invalid.variables[201]=2;bool rejected=false;
                    try{GameSession restored(c.items,invalid,c.facts);}catch(const std::exception&){rejected=true;}
                    require(rejected,"Persistent201 input must be rejected rather than restored as an E2 sample");
                });
                checkBody("blocked idle Move retains direction until the next source loop",[&]{
                    auto c=sourceContent();auto e=sourceEntry();GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});
                    messageFlow(c,true);room.update({});c.catalog["flows"]["domain-body-message"]["trigger"]="call-only";
                    for(int tick=0;tick<40;++tick){room.update({});}
                    require(findCharacter(room.world(),"m52-e4").motion.facing()==Direction::Right&&s.variable(201)==2,"Blocked C64 retains Down201 while a real separate route turns the actor Right");
                    closeMessage(room);room.update({});require(s.variable(201)==2,"The first released Move cannot rerun earlier C11");
                    room.update({});require(s.variable(201)==6,"Only the next idle outer loop samples Right201");
                });
                if(training.full->at("runtime").contains("player_recovery")){
                    for(int page:{7,8}){
                        checkBody("P"+std::to_string(page)+" side route reads blocked attack-entry direction",[&]{
                            auto c=sourceContent();auto e=sourceEntry();e.playerTile={24,10};e.session.flags[21]=true;e.session.variables[91]=1;
                            GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});
                            messageFlow(c,true);room.update({});c.catalog["flows"]["domain-body-message"]["trigger"]="call-only";
                            for(int tick=0;tick<40;++tick){room.update({});}closeMessage(room);
                            for(int tick=0;tick<4&&room.foreground()!=ForegroundKind::None;++tick){room.update({});}
                            require(room.foreground()==ForegroundKind::None&&findCharacter(room.world(),"m52-e4").motion.facing()==Direction::Right,"Public message releases while P2 keeps its entry Down sample and live actor faces Right");
                            const auto before=room.world().player().motion.tile();EnemyHost host(room,"m52-e4");require(host.requestPlayerReaction(page),"The existing bounded reaction API accepts the source page");
                            for(int tick=0;tick<40;++tick){room.update({});}
                                                        require(room.world().player().motion.tile()==TilePosition{before.x+(page==7?1:-1),before.y}&&s.hp()==100,"P7/P8 execute original cached-Down horizontal side route instead of live-Right vertical route");
                            require(s.variable(201)==2,"P2 inner body loop does not resample direction after startup");
                        });
                    }
                    checkBody("unmatched source direction0 omits only the side route",[&]{
                        auto c=sourceContent();auto e=sourceEntry();e.session.flags[156]=false;e.session.flags[1]=true;
                        GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});
                        require(s.variable(201)==0,"Inactive E2 leaves original unsampled direction0");EnemyHost host(room,"m52-e4");
                        require(host.requestPlayerReaction(7)&&s.flag(45)&&!s.flag(1)&&s.variable(31)==3,"P7 C3/CE6/CE9/C23 still commit when none of the four direction branches matches");
                        const auto before=room.world().player().motion.tile();for(int tick=0;tick<40;++tick){room.update({});}
                        require(room.world().player().motion.tile()==before&&s.hp()==100,"Unknown0 does not default to Down or publish a displacement route");
                    });
                }
                for(int page:{10,11,12}){
                    checkBody("P"+std::to_string(page)+" attack bounds use inherited cached direction",[&]{
                        auto c=sourceContent();stopBody(c);auto e=sourceEntry();GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e,[](int,int hi){return hi;});room.update({});
                        s.setFlag(777,true);if(page==11){s.setFlag(157,true);}if(page==12){s.setFlag(45,true);}
                        c.setMediaDefinition("cues","domain-cached-player",{{"kind","place_object"},{"object","player"},{"tile",{16,12}}});ScenePresentation(room).cue("domain-cached-player");faceActor(c,room,Direction::Right);room.update({});
                        require(findCharacter(room.world(),"m52-e4").motion.facing()==Direction::Right&&s.flag(21)&&s.variable(91)==1,"Original Down bounds select attack1 while live Right bounds do not contain the player");
                        require(s.variable(201)==2,"Highest empty manager keeps the preceding E2 sample");
                    });
                }
                checkBody("terrain turn reads inherited cached front rather than live front",[&]{
                    auto c=sourceContent();stopBody(c);auto e=sourceEntry();GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});
                    s.setFlag(777,true);c.terrainIds.at(52)[11*33+16]=3;
                    c.setMediaDefinition("cues","domain-cached-player",{{"kind","place_object"},{"object","player"},{"tile",{24,18}}});ScenePresentation(room).cue("domain-cached-player");faceActor(c,room,Direction::Right);room.update({});room.update({});
                    require(findCharacter(room.world(),"m52-e4").motion.moving()&&findCharacter(room.world(),"m52-e4").motion.facing()==Direction::Right,"Cached Down front terrain3 and actorX<playerX publish the source Right sidestep");
                });
                checkBody("unsampled autonomy skips direction branches but advances its own route",[&]{
                    auto c=sourceContent();stopBody(c);auto e=sourceEntry();e.playerTile={16,12};e.session.flags[777]=true;e.session.flags[778]=false;
                    for(auto& page:c.objects.at("m52-e4").variants){if(page.page==10){page.autonomousRoute={10005,8,false,true,{{"up"},{"switch_on",{},{},778}}};}}
                    GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});
                    require(s.variable(201)==0&&!s.flag(21),"P10 no201 branch matches before an E2 sample, even with player in live Down bounds");
                    for(int tick=0;tick<40&&!s.flag(778);++tick){room.update({});}
                    require(s.flag(778)&&findCharacter(room.world(),"m52-e4").motion.tile()==TilePosition{16,9},"An unmatched direction does not suppress the source autonomous route or invent a turn");
                });
                checkBody("sweep damage reads entry cache after a real later turn",[&]{
                    auto c=sourceContent();auto e=sourceEntry();e.playerTile={16,13};e.session.flags[21]=true;e.session.variables[91]=2;e.session.hp=2000;e.session.maxHp=2000;e.session.baseDefense=0;e.session.armor=0;e.session.accessory=0;e.session.otherEquipment=0;
                    GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e,[](int,int hi){return hi;});room.update({});
                    for(int tick=1;tick<30;++tick){room.update({});}faceActor(c,room,Direction::Up);
                    for(int tick=30;tick<=97;++tick){room.update({});}require(s.hp()==2000,"Cached Down front point remains unharmed through sweep tick97");room.update({});
                                        require(findCharacter(room.world(),"m52-e4").motion.facing()==Direction::Up&&s.hp()==999,"P15 frame4 uses sampled Down201 despite a real World Up turn");
                    room.update({});require(s.hp()==999,"The committed sweep frame never repeats its damage next update");
                });
                checkBody("sweep unmatched201 skips damage without stopping its source ending",[&]{
                    auto c=sourceContent();stopBody(c);auto e=sourceEntry();e.playerTile={16,13};e.session.flags[21]=true;e.session.variables[91]=2;e.session.hp=2000;e.session.maxHp=2000;e.session.baseDefense=0;e.session.armor=0;e.session.accessory=0;e.session.otherEquipment=0;
                    GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e,[](int,int hi){return hi;});room.update({});
                    // Domain inherited-work variation: manager stops and source201 is0.
                    s.setFlag(777,true);s.setVariable(201,0);for(int tick=1;tick<=180;++tick){room.update({});}
                    require(s.hp()==2000&&!s.flag(21),"P15 unmatched four directional branches omit damage but still reach C630-633 cleanup");
                });
                for(const bool cachedHorizontal:{false,true}){
                    checkBody(cachedHorizontal?"slash horizontal cache delays live Down hit to72":"slash vertical cache permits live Right hit at70",[&]{
                        auto c=sourceContent();auto e=sourceEntry();e.playerTile=cachedHorizontal?TilePosition{16,13}:TilePosition{19,10};e.directions["m52-e4"]=cachedHorizontal?Direction::Right:Direction::Down;
                        e.session.flags[21]=true;e.session.variables[91]=1;e.session.hp=2000;e.session.maxHp=2000;e.session.baseDefense=0;e.session.armor=0;e.session.accessory=0;e.session.otherEquipment=0;
                        GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e,[](int,int hi){return hi;});room.update({});
                        for(int tick=1;tick<30;++tick){room.update({});}faceActor(c,room,cachedHorizontal?Direction::Down:Direction::Right);
                        for(int tick=30;tick<70;++tick){room.update({});}room.update({});
                        require(s.hp()==(cachedHorizontal?2000:999),"P14 C65/66 select prep timing by201 while C86/142/198/254 choose actual live hit direction");
                        if(cachedHorizontal){room.update({});require(s.hp()==2000,"Cached horizontal branch retains its two Wait0 updates");room.update({});require(s.hp()==999,"Cached horizontal prep commits the live Down branch at72");}
                    });
                }
                if(training.full->at("runtime").contains("training_target_contacts")){
                    checkBody("real target parallel uses selected page and survives messages",[&]{
                        auto c=sourceContent();auto e=sourceEntry();e.positions["m52-e6"]={6,6};e.playerTile={6,6};e.session.flags[164]=false;
                        GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});require(!s.flag(45),"Target P1 is not the parallel contact page");
                        messageFlow(c,false);room.update({});c.catalog["flows"]["domain-body-message"]["trigger"]="call-only";
                        s.setFlag(164,true);room.update({});const auto events=room.takePresentation();
                        require(s.flag(45)&&!s.flag(1)&&s.hp()==100&&s.variable(31)==3,"Target P2 calls E4P7 during the real message without applying HP damage");
                        require(cueCount(events,"m52-e6-p2-c8")==1&&std::count_if(events.begin(),events.end(),[&](const auto& event){return event.kind==PresentationEvent::Kind::Sound&&event.sound.resource==c.cue("m52-e6-p2-c7").at("sound").at("resource").get<std::string>();})==1,"Contact emits original flash and sound exactly once before S45 prevents repetition");
                        room.update({});require(cueCount(room.takePresentation(),"m52-e6-p2-c8")==0,"S45 rejects a repeated contact on the same tile");
                    });
                    checkBody("target rejection and changed identity use their own tile",[&]{
                        auto c=sourceContent();auto e=sourceEntry();e.playerTile={6,6};e.positions["m52-e7"]={6,6};e.session.flags[164]=true;e.session.flags[13]=true;
                        GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});require(!s.flag(45),"Protection13 rejects target contact");
                        s.setFlag(13,false);room.update({});require(s.flag(45),"Another source target identity uses its changed position and the shared response");
                    });
                }
                checkBody("idle publishes sampled body83/84 before autonomy",[&]{
                    auto c=sourceContent();auto e=sourceEntry();GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});
                    require(s.variable(83)==264&&s.variable(84)==155,"M52 E2 P1 C3-5 publish foot264,176 minus21 to the unique transient source body pair");
                });
                checkBody("common event reads previous zero body before first map sample",[&]{
                    auto c=sourceContent();auto e=sourceEntry();e.session.flags[37]=true;e.session.worldValues["burn.slot-1.lifetime"]=23;
                    GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});const auto events=room.takePresentation();
                    require(!s.flag(37)&&s.variable(301)==0&&state(room,"m52-e4").damage==0&&cueCount(events,"f4full-ce24-c81")==1,"CE24 first sees unsampled0/0 and exits before E2 writes the nonzero body");
                    require(s.variable(83)==264&&s.variable(84)==155,"The same step then executes the independent E2 body thread");
                });
                checkBody("common picture copies prior body instead of resampling actor",[&]{
                    auto c=sourceContent();auto e=sourceEntry();e.session.flags[37]=true;e.session.flags[156]=false;
                    GameSession s(c.items,e.session,c.facts);s.setVariable(83,80);s.setVariable(84,96);CombatScene room(c,s,e);room.update({});
                    require(s.variable(93)==80&&s.variable(94)==433&&cueAt(room.takePresentation(),"f4full-ce24-c15",4.5,26.0625),"CE24 C3-4 read the preceding shared80/96 plus337 even when its actor is at16,10");
                    require(s.variable(83)==80&&s.variable(84)==96,"An inactive manager does not overwrite inherited body work");
                });
                checkBody("idle pending Move retains sampled body and direction while another owned route moves",[&]{
                    auto c=sourceContent();auto e=sourceEntry();GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});room.takePresentation();
                    messageFlow(c,true);room.update({});c.catalog["flows"]["domain-body-message"]["trigger"]="call-only";
                    require(room.dialogue().view().phase!=DialoguePhase::Hidden,"Public source message blocks the parallel Move");
                    // This update ran E2 before checkAutorun opened the message.
                    require(cueCount(room.takePresentation(),"m52-e2-p1-c85")==1,"The opening update completes its legal source Move before the actual message window exists");
                    for(int tick=0;tick<40;++tick){room.update({});}const auto stalled=room.takePresentation();
                    require(findCharacter(room.world(),"m52-e4").motion.tile()==TilePosition{17,10},"A separate forced route finishes one Right while its picture instruction waits");
                    require(s.variable(83)==264&&s.variable(84)==155&&cueCount(stalled,"m52-e2-p1-c85")==0,"Pending C64 does not repeat C3-5 sampling or continue to C85");
                    closeMessage(room);room.update({});const auto resumed=room.takePresentation();
                    require(cueAt(resumed,"m52-e2-p1-c85",15.1875,8.6875)&&cueCount(resumed,"m52-e2-p1-c43")==0,"Unblocked idle continues with the old Down sample before reevaluating the now Right-facing actor");
                    require(sampledAt(resumed,"m52-e2-p1-c85",251,155),"Released unfixed body Move publishes original oldDown sample251/155 while actor has moved Right");
                    room.update({});const auto next=room.takePresentation();
                    require(s.variable(83)==280&&s.variable(84)==155&&cueCount(next,"m52-e2-p1-c43")==1,"Only the next source loop samples17,10 and sets up the new Right pictures");
                });
                // CN1.00 CE4 C3-16 reads the preceding E2 body83/84, not live motion.
                // Public world-pixel attacks use the current camera origin for that screen cache.
                for(const bool panCamera:{false,true}){
                    checkBody(panCamera?"receiver converts stale screen body through a genuinely panned camera":"receiver hits old cached body then misses after the next E2 loop",[&]{
                        auto c=sourceContent();auto e=sourceEntry();GameSession s(c.items,e.session,c.facts);
                        CombatScene room(c,s,e,[](int lo,int hi){return lo==0&&hi==10?5:hi;});room.update({});room.takePresentation();
                        messageFlow(c,true);
                        if(panCamera){
                            c.setMediaDefinition("cues","domain-receiver-camera",{{"kind","camera"},{"operation","pan"},{"direction_original",static_cast<int>(Direction::Right)},{"distance_tiles",4},{"speed_effective",6},{"wait",false}});
                        }
                        room.update({});c.catalog["flows"]["domain-body-message"]["trigger"]="call-only";
                        for(int tick=0;tick<40;++tick){room.update({});}room.takePresentation();
                        if(panCamera){
                            // Start the public camera cue only after E2 has reached its blocked Move.
                            ScenePresentation presentation(room);presentation.cue("domain-receiver-camera");
                            for(int tick=0;tick<8;++tick){room.update({});}
                        }
                        require(room.dialogue().view().phase!=DialoguePhase::Hidden&&findCharacter(room.world(),"m52-e4").motion.tile()==TilePosition{17,10},"Receiver fixture retains an actual message blocker while a forced Right finishes");
                        require(s.variable(83)==264&&s.variable(84)==155,"Pending E2 Move retains the original body264/155 without resampling live17,10");
                        require(room.cameraCenter().x==(panCamera?224:160)&&room.cameraCenter().y==120,"Actual Scene camera is panned64 pixels only in the camera variation");
                        const AttackSample sample{{panCamera?320.0:256.0,155},{0,0},AttackKind::Handgun,2,0,false};std::set<std::string> resolved;
                        const auto contact=room.resolveAttack(sample,resolved);
                        require(contact.processed==1&&contact.damage==50&&s.pursuerHealth()==450&&state(room,"m52-e4").damage==50,"CE4 inclusive left boundary uses cached body, current camera and zero attack half-size");
                        require(room.resolveAttack(sample,resolved).processed==0&&s.pursuerHealth()==450&&state(room,"m52-e4").damage==50,"The same attack's resolved actor is not damaged twice");
                        closeMessage(room);room.update({});
                        require(s.variable(83)==264&&s.variable(84)==155,"First unblocked E2 step only completes its pending picture command");
                        room.update({});require(s.variable(83)==(panCamera?216:280)&&s.variable(84)==155,"Next body loop samples live17,10 using the current camera origin");
                        s.setFlag(47,true);std::set<std::string> missed;
                        require(room.resolveAttack(sample,missed).processed==0&&missed.empty()&&s.flag(47)&&s.pursuerHealth()==450,"After the next sample the same world point misses without consuming the precision latch or health");
                        std::set<std::string> fresh;
                        require(room.resolveAttack({{288,155},{0,0},AttackKind::Handgun,2,0,false},fresh).damage==100&&s.pursuerHealth()==350,"The new cached right boundary hits once and consumes the original doubled damage latch");
                    });
                }
                checkBody("receiver preserves the vertical cached boundary while its Up-facing actor moves",[&]{
                    auto c=sourceContent();auto e=sourceEntry();e.directions["m52-e4"]=Direction::Up;GameSession s(c.items,e.session,c.facts);
                    CombatScene room(c,s,e,[](int lo,int hi){return lo==0&&hi==10?5:hi;});room.update({});room.takePresentation();
                    messageFlow(c,true);c.routes["domain-body-right"].steps={{"up"}};room.update({});c.catalog["flows"]["domain-body-message"]["trigger"]="call-only";
                    for(int tick=0;tick<40;++tick){room.update({});}room.takePresentation();
                    require(findCharacter(room.world(),"m52-e4").motion.tile()==TilePosition{16,9}&&s.variable(83)==264&&s.variable(84)==155,"Up route changes World tile while the pending E2 body remains264/155");
                    std::set<std::string> resolved;
                    require(room.resolveAttack({{264,170},{0,0},AttackKind::Handgun,2,0,false},resolved).damage==50&&s.pursuerHealth()==450,"CE4 cached bottom boundary155+15 accepts a zero-half attack after moving Up");
                    closeMessage(room);room.update({});room.update({});
                    require(s.variable(83)==264&&s.variable(84)==139,"Next Up body loop publishes the source center139, already including minus21");
                    std::set<std::string> missed;
                    require(room.resolveAttack({{264,170},{0,0},AttackKind::Handgun,2,0,false},missed).processed==0&&s.pursuerHealth()==450,"Old bottom point misses after the next Up body sample");
                    std::set<std::string> fresh;
                    require(room.resolveAttack({{264,124},{0,0},AttackKind::Handgun,2,0,false},fresh).damage==50&&s.pursuerHealth()==400,"New top boundary139-15 hits without subtracting body offset21 twice");
                });
                for(int selector:{1,2}){
                    checkBody(selector==1?"slash samples body before blocked startup Show":"sweep retains body until blocked startup Show completes",[&]{
                        auto c=sourceContent();auto e=sourceEntry();GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});
                        c.messages.at("m4-e4-p7-c3").message.lines={{"缓存",{{MessageTokenKind::Text,U"缓存",0}}}};
                        messageFlow(c,false);room.update({});c.catalog["flows"]["domain-body-message"]["trigger"]="call-only";
                        s.setVariable(83,80);s.setVariable(84,96);s.setVariable(91,selector);s.setFlag(21,true);room.update({});
                        require(s.variable(83)==(selector==1?264:80)&&s.variable(84)==(selector==1?155:96),"Original P2 C1-3 precede Show; original P3 C47-49 follow all startup Shows");
                        closeMessage(room);room.update({});const auto events=room.takePresentation();
                        const auto setup=selector==1?"m52-e2-p2-c9":"m52-e2-p3-c6";
                        const int expectedBodyX=selector==1?264:248;
                        require(s.variable(83)==expectedBodyX&&s.variable(84)==155&&cueAt(events,setup,16,8.6875),"Released setup retains its original foot sample while the first body loop reads the actor after the source sweep forward route");
                        if(selector==2){require(findCharacter(room.world(),"m52-e4").motion.tile()==TilePosition{15,10},"P15 C2 faces the player and moves Left while its E2 startup Show remains blocked");}
                        require(sampledAt(events,setup,264,155),"Slash/sweep startup publishes source setup sample264/155 pixels independently of later frame offsets");
                    });
                }
                checkBody("action entry preserves idle movement pulse before source finish clears it",[&]{
                    auto c=sourceContent();auto e=sourceEntry();GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});
                    s.setVariable(91,1);s.setFlag(21,true);for(int tick=0;tick<6;++tick){room.update({});}room.takePresentation();s.setFlag(21,false);room.update({});room.update({});
                    require(cueCount(room.takePresentation(),"m52-e2-p1-c85")>0,"Interrupting before P14 C326 retains the previously accumulated idle pulse205");
                });
                checkBody("body thread continues during authoritative foreground",[&]{
                    auto c=sourceContent();auto e=sourceEntry();c.routes["domain-body-right"]={4,8,false,true,{{"right"}}};c.setMediaDefinition("cues","domain-body-right",{{"kind","forced_route"}});
                    c.catalog["flows"]["domain-body-wait"]={{"object","m52-e34"},{"page",1},{"trigger","autorun"},{"steps",nlohmann::json::array({{{"kind","cue"},{"cue","domain-body-right"}},{{"kind","cue"},{"cue","m52-e4-p17-c3"}}})}};
                    GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);for(int tick=0;tick<40;++tick){room.update({});}
                    require(room.foreground()==ForegroundKind::Content&&findCharacter(room.world(),"m52-e4").motion.tile()==TilePosition{17,10}&&s.variable(83)==280&&s.variable(84)==155,"E2 keeps sampling the source actor during an independent foreground Wait40");
                });
                checkBody("E4 terminal Erase waits for the native message guard before cleanup",[&]{
                    auto c=sourceContent();auto e=sourceEntry();e.session.flags[21]=true;e.session.variables[91]=2;
                    GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});for(int tick=1;tick<110;++tick){room.update({});}
                    messageFlow(c,false);room.update({});c.catalog["flows"]["domain-body-message"]["trigger"]="call-only";room.takePresentation();
                    for(int tick=0;tick<120;++tick){room.update({});}const auto blocked=room.takePresentation();
                    require(s.flag(21)&&cueCount(blocked,"m52-e4-p15-c625")==0,"Native Erase blocks the E4 cursor after its rest-frame write and before cleanup");
                    closeMessage(room);room.update({});const auto released=room.takePresentation();require(cueCount(released,"m52-e4-p15-c625")==1,"The released E4 Erase commits once");
                    for(int tick=0;tick<5;++tick){room.update({});}require(s.flag(21),"Final Wait1 is measured from released Erase");room.update({});require(!s.flag(21),"E4 cleans up after the retained final Wait1");
                });
                checkBody("source E4 reappearance wait continues through a real blocking message",[&]{
                    auto c=sourceContent();auto e=sourceEntry();e.session.flags[160]=true;messageFlow(c,false);
                    GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});c.catalog["flows"]["domain-body-message"]["trigger"]="call-only";
                    for(int tick=1;tick<240;++tick){room.update({});}require(s.flag(160),"Parallel E4 retains full Wait40 while message blocks only picture commands");room.update({});
                    require(!s.flag(160)&&room.foreground()==ForegroundKind::Content&&findCharacter(room.world(),"m52-e4").motion.tile()==e.playerTile,"E4 completes exactly once without waiting for message confirmation");
                });
                checkBody("body thread samples before death early return",[&]{
                    auto c=sourceContent();auto e=sourceEntry();e.session.hp=0;GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});
                    require(s.variable(83)==264&&s.variable(84)==155,"Player death does not suppress source E2 parallel before Scene handles death");
                });
                checkBody("changed manager ID selects highest empty page without erasing or sampling",[&]{
                    auto c=sourceContent();auto manager=c.objects.at("m52-e2");c.objects.erase("m52-e2");manager.id="domain-body-manager";
                    for(auto& page:manager.variants){page.definition.id=manager.id;}
                    auto higher=manager.variants.back();higher.page=5;higher.flags={{777,true}};manager.variants.push_back(higher);c.objects[manager.id]=manager;
                    c.enemies.at("m52-e4").rules["runtime_profile"]["presentation_lifecycle"]["manager_object"]=manager.id;
                    auto e=sourceEntry();e.session.flags[777]=true;GameSession s(c.items,e.session,c.facts);s.setVariable(83,80);s.setVariable(84,96);CombatScene room(c,s,e);room.update({});const auto empty=room.takePresentation();
                    require(s.variable(83)==80&&s.variable(84)==96&&std::none_of(empty.begin(),empty.end(),[](const auto& event){return event.id.starts_with("m52-e2-");}),"Highest empty changed manager suppresses the old body and all original manager picture operations");
                    s.setFlag(777,false);room.update({});const auto active=room.takePresentation();
                    require(s.variable(83)==264&&s.variable(84)==155&&cueCount(active,"m52-e2-p1-c31")==1,"Returning to changed manager P1 starts its finite thread on the larger grid");
                });
                for(const std::string binding:{"missing-body-manager","m4-e11"}){
                    checkBody("reject wrong manager binding "+binding,[&]{
                        auto c=sourceContent();c.enemies.at("m52-e4").rules["runtime_profile"]["presentation_lifecycle"]["manager_object"]=binding;
                        auto e=sourceEntry();GameSession s(c.items,e.session,c.facts);s.setVariable(83,80);s.setVariable(84,96);bool rejected=false;
                        try{CombatScene room(c,s,e);room.update({});}catch(const std::exception&){rejected=true;}
                        require(rejected&&s.variable(83)==80&&s.variable(84)==96&&!s.flag(21)&&s.hp()==100,"Unknown or foreign-map manager is rejected before body, selector or damage commit");
                    });
                }
                checkBody("reject undeclared manager suppression page before source writes",[&]{
                    auto c=sourceContent();c.enemies.at("m52-e4").rules["runtime_profile"]["presentation_lifecycle"]["stop_page"]=99;
                    auto e=sourceEntry();GameSession s(c.items,e.session,c.facts);s.setVariable(83,80);s.setVariable(84,96);bool rejected=false;
                    try{CombatScene room(c,s,e);room.update({});}catch(const std::exception&){rejected=true;}
                    require(rejected&&s.variable(83)==80&&s.variable(84)==96&&!s.flag(21),"A missing typed stop page is rejected before body or selector writes");
                });
            }
            if(training.full->at("runtime").contains("player_recovery")){
                for(const int side:{-1,1}){
                    auto sideEntry=entry;sideEntry.playerTile={24+side,16};sideEntry.positions["m52-e4"]={24,14};sideEntry.directions["m52-e4"]=Direction::Down;
                    sideEntry.session.flags[160]=false;sideEntry.session.flags[21]=true;sideEntry.session.variables[91]=1;
                    sideEntry.session.flags[13]=false;sideEntry.session.flags[15]=false;sideEntry.session.flags[45]=false;sideEntry.session.flags[1]=true;
                    sideEntry.session.hp=100;sideEntry.session.pursuerHealth=500;
                    GameSession knocked(training.items,sideEntry.session,training.facts);CombatScene sideSlash(training,knocked,sideEntry);sideSlash.update({});
                    for(int n=1;n<70;++n){sideSlash.update({});}require(!knocked.flag(45)&&knocked.hp()==100,"Source slash side contact waits through69 updates without central damage");
                    InputFrame heldMovement;heldMovement.movement=Direction::Up;sideSlash.update(heldMovement);
                    require(knocked.flag(45)&&!knocked.flag(1)&&knocked.hp()==100&&!state(sideSlash,"m52-e4").holdsPlayer,"Original slash side P7/P8 commits CE9 knocked-down response and S45 at70 without damage or grab");
                    require(sideSlash.world().player().motion.tile().y==sideEntry.playerTile.y,"The source side response prevents the same update from applying sampled Up movement, including a blocked horizontal route");
                    const auto sprite=training.cue("f4full-ce9-c18").at("sprite").get<std::string>();
                    require(sideSlash.world().player().spriteId==sprite,"Side response publishes the source CE9 mode3 Actor1 graphic");
                }
            }
            // The source rules/profile remain unchanged; only the domain entry
            // starts in the original slash facts, facing and player position.
            entry.session.flags[160]=false;entry.session.flags[21]=true;entry.session.variables[91]=1;
            entry.session.flags[13]=false;entry.session.flags[15]=false;entry.session.pursuerHealth=500;
            entry.session.hp=100;entry.session.maxHp=100;
            entry.positions["m52-e4"]=training.objects.at("m52-e4").tile;entry.directions["m52-e4"]=Direction::Down;
            GameSession target(training.items,entry.session,training.facts);
            CombatScene slash(training,target,entry,[](int lo,int hi){return hi;});slash.update({});
            require(state(slash,"m52-e4").mode==EnemyMode::Windup&&target.hp()==100,"Real M52 slash commits windup without immediate damage");
            for(int n=1;n<70;++n){slash.update({});}
            require(target.hp()==100,"Vertical slash retains its full70-step windup");
            slash.update({});require(target.hp()==0,"Original Actor30 attack999 reaches the player at vertical tick70");
            const auto impactEvents=slash.takePresentation();
            require(std::none_of(impactEvents.begin(),impactEvents.end(),[](const auto& e){return e.id=="m52-e2-p2-c188";}),"E2 precedes E4 C80, so frame4 is not visible during the tick70 damage commit");
            slash.update({});const auto pictureEvents=slash.takePresentation();
            require(std::any_of(pictureEvents.begin(),pictureEvents.end(),[](const auto& e){
                return e.kind==PresentationEvent::Kind::Cue&&e.id=="m52-e2-p2-c188"&&e.actor=="m52-e4"&&
                    std::abs(e.position.x-24.125)<.001&&std::abs(e.position.y-14.125)<.001;
            }),"Source E2 renders frame4 on tick71 death parallel after E4 tick70, at down offset2,2");
            // Independent source literals choose a front point after the original
            // face-player/forward route. First impact here is stage4, not stage3.
            // each facing; no damage or presentation rule is changed in data.
            const std::array<Direction,4> facings{Direction::Up,Direction::Right,Direction::Down,Direction::Left};
            const std::array<TilePosition,4> targets{TilePosition{24,11},TilePosition{27,14},TilePosition{24,17},TilePosition{21,14}};
            const std::array<TilePosition,4> origins{TilePosition{24,13},TilePosition{25,14},TilePosition{24,15},TilePosition{23,14}};
            for(std::size_t d=0;d<facings.size();++d){
                entry.playerTile=targets[d];entry.directions["m52-e4"]=facings[d];
                entry.session.variables[91]=2;entry.session.hp=2000;entry.session.maxHp=2000;entry.session.baseDefense=0;
                entry.session.mainWeapon=0;entry.session.auxiliaryWeapon=0;entry.session.armor=0;entry.session.accessory=0;entry.session.otherEquipment=0;
                GameSession swept(training.items,entry.session,training.facts);CombatScene sweep(training,swept,entry,[](int lo,int hi){return hi;});sweep.update({});
                auto sweepEvents=sweep.takePresentation();
                const auto startSound=readSound(training.cue("m52-e4-p15-c4").at("sound"));
                require(std::none_of(sweepEvents.begin(),sweepEvents.end(),[&](const auto& e){return e.kind==PresentationEvent::Kind::Sound&&e.sound==startSound;}),"Sweep start sound waits24 steps instead of playing at commit");
                for(int n=1;n<=24;++n){sweep.update({});}sweepEvents=sweep.takePresentation();
                require(std::count_if(sweepEvents.begin(),sweepEvents.end(),[&](const auto& e){return e.kind==PresentationEvent::Kind::Sound&&e.sound==startSound;})==1,"Sweep start sound plays once after original Wait4");
                require(findCharacter(sweep.world(),"m52-e4").motion.tile()==origins[d],"Sweep original start route faces the player and advances one tile");
                for(int n=25;n<98;++n){sweep.update({});}
                require(swept.hp()==2000,"Front point in all sweep directions remains unharmed through97 steps");
                sweep.update({});
                if(swept.hp()!=999){const auto actor=findCharacter(sweep.world(),"m52-e4").motion.tile();const auto player=sweep.world().player().motion.tile();std::cerr<<"Sweep direction="<<d<<" hp="<<swept.hp()<<" defense="<<swept.defense()<<" actor="<<actor.x<<","<<actor.y<<" player="<<player.x<<","<<player.y<<" facing="<<directionName(findCharacter(sweep.world(),"m52-e4").motion.facing())<<" flags13/15="<<swept.flag(13)<<"/"<<swept.flag(15)<<"\n";}
                require(swept.hp()==999,"Source sweep front point receives attack999 plus fluctuation2 at stage4 tick98");
                sweep.update({});require(swept.hp()==999,"A sweep stage does not repeat its committed damage next tick");
            }
            std::cout<<"Real M52 source sweep four-facing domain entry checked\n";
            std::cout<<"Real M52 source slash domain entry checked\n";
        }
    }
    // Domain fixture: an existing actor hosts CN1.00 M52E4 direct receivers.
    // This checks the public scene seam and does not claim a natural training run.
    // This direct-receiver fixture has no E2 manager. Supply its explicit preceding
    // CN body sample in screen pixels; production never synthesizes a live fallback.
    const auto primeReceiverBody=[](GameSession& s,const CombatScene& room,const std::string& actor){
        const auto p=findCharacter(room.world(),actor).motion.position(),camera=room.cameraCenter();
        s.setVariable(83,static_cast<int>((p.x+.5)*16-camera.x+160));
        s.setVariable(84,static_cast<int>((p.y+1)*16-camera.y+120)-21);
    };
    const std::string id="m27-e3";
    auto& enemy=content.enemies.at(id);enemy.story=false;enemy.rules["behavior"]="pursuer";
    enemy.rules["defense"]=30;enemy.rules["activation_fact"]=nullptr;
    enemy.rules["hitbox_offset_pixels"]={8,-5};enemy.rules["hitbox_half_pixels"]={8,15};
    enemy.rules["completion_flow"]="pursuer-domain-completion";
    content.catalog["flows"]["pursuer-domain-completion"]={{"object",id},{"page",1},{"trigger","requested"},{"steps",nlohmann::json::array({{{"kind","flag"},{"id",enemy.deadFlag},{"value",true}}})}};
    auto entry=content.entries.at("defeated-claimed");entry.map=enemy.map;entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();entry.playerTile={1,1};
    entry.session.mainWeapon=0;entry.session.baseAttack=80;entry.session.flags[47]=true;entry.session.flags[enemy.deadFlag]=false;
    GameSession session(content.items,entry.session,content.facts);CombatScene scene(content,session,entry,[](int lo,int hi){return lo==0&&hi==10?5:lo==0&&hi==100?0:hi;});scene.update({});primeReceiverBody(session,scene,id);
    {
        const auto p=findCharacter(scene.world(),id).motion.position();std::set<std::string> resolved;
        require(scene.resolveAttack({{p.x*16+8,p.y*16+11},{0,0},AttackKind::Handgun,2,0,false},resolved).processed==0,"Source body bottom+16 is outside halfheight15");
        require(session.pursuerHealth()==500&&session.flag(47),"Miss does not consume health or CE11 critical latch");
    }
    {
        const auto p=findCharacter(scene.world(),id).motion.position();std::set<std::string> resolved;bool rejected=false;
        try{scene.resolveAttack({{p.x*16+8,p.y*16-5},{0,0},static_cast<AttackKind>(999),2,0,false},resolved);}
        catch(const std::invalid_argument&){rejected=true;}
        require(rejected&&session.pursuerHealth()==500&&session.flag(47),"Unsupported weapon kind is rejected before consuming CE11 state");
    }
    require(hit(scene,id).damage==100,"M52E4 training multiplies CE11 critical-marked50 by2");
    require(session.pursuerHealth()==400&&state(scene,id).damage==100,"Direct damage updates persistent HP and independent local sum");
    scene.deactivate();require(hit(scene,id).processed==0,"Deactivated pursuer scene rejects old attack");
    {
        GameSession precise(content.items,entry.session,content.facts);int criticalRolls=0;
        CombatScene room(content,precise,entry,[&](int lo,int hi){if(lo==0&&hi==100){++criticalRolls;}return lo==0&&hi==10?5:hi;});room.update({});primeReceiverBody(precise,room,id);
        require(hit(room,id).damage==100&&precise.flag(12)&&!precise.flag(47),"S47 jumps directly to CE11 Label5 and guarantees S12");
        require(criticalRolls==0&&precise.variable(35)==0,"CE11 precision skips critical randomness and clears successful hit count");
    }
    entry.session.flags[47]=false;entry.session.pursuerHealth=1;entry.positions[id]={6,5};
    {
        GameSession worn(content.items,entry.session,content.facts);CombatScene room(content,worn,entry,[](int lo,int hi){return lo==0&&hi==10?5:hi;});room.update({});primeReceiverBody(worn,room,id);
        require(state(room,id).damage==499,"Scene rebuild restores500 minus persistent HP");
        const auto received=hit(room,id,5);
        require(received.damage==50,"Receiver5 uses actual damage rather than zombie150 terminal value");
        require(received.endRequested,"Receiver5 requests the current player attack end from original S16");
        require(worn.pursuerHealth()==1&&state(room,id).damage==549,"Full raw50 goes past499 despite zero persistent HP delta");
        require(state(room,id).mode==EnemyMode::Ending,"Threshold requests completion and retires pursuit");
        require(hit(room,id).processed==0,"Threshold prevents another direct callback");
        room.update({});require(worn.flag(enemy.deadFlag),"Next update requests the bound finite completion flow");
    }
    {
        GameSession preserved(content.items,entry.session,content.facts);CombatScene next(content,preserved,entry);next.update({});
        require(state(next,id).damage==499&&preserved.pursuerHealth()==1,"A new scene rebuilds persistent damage rather than old local excess");
    }
    {
        // Domain sample of complete CN1.00 CE24 damage/lifespan arithmetic;
        // fire media and the later GL projectile activation are not claimed.
        enemy.rules["runtime_profile"]["burn"]={{"common_event",24},{"counter_source_variable",301},{"activation_fact",37},{"interval_ticks",6},{"damage",4},{"lifetime_threshold",25},{"counter_increment",{1,2}}};
        auto burnedEntry=entry;burnedEntry.session.pursuerHealth=50;burnedEntry.session.flags[37]=true;
        GameSession burned(content.items,burnedEntry.session,content.facts);CombatScene room(content,burned,burnedEntry,[](int lo,int hi){return hi;});room.update({});
        for(int n=0;n<5;++n){room.update({});}require(state(room,id).damage==450,"CE24 retains six source zero-waits before local damage");
        room.update({});require(state(room,id).damage==454&&burned.pursuerHealth()==50,"CE24 adds4 locally without damaging persistent Actor30 HP");
        for(int n=0;n<72;++n){room.update({});}
        require(state(room,id).damage==502&&state(room,id).mode==EnemyMode::Ending&&burned.pursuerHealth()==50,"Local DOT reaches completion while persistent HP remains above1");
        require(!burned.flag(37),"CE24 counter increases2 per cycle and stops at26 crossing original25");
        room.update({});require(burned.flag(enemy.deadFlag),"DOT completion requests the same bound finite flow");
        burnedEntry.session.pursuerHealth=500;
        GameSession paused(content.items,burnedEntry.session,content.facts);CombatScene pauseRoom(content,paused,burnedEntry,[](int lo,int hi){return hi;});pauseRoom.update({});
        for(int n=0;n<15;++n){pauseRoom.update({});}require(state(pauseRoom,id).damage==8,"Pause sample commits two DOT cycles and retains three partial waits");
        paused.setFlag(37,false);for(int n=0;n<20;++n){pauseRoom.update({});}require(state(pauseRoom,id).damage==8,"CommonEvent switch OFF pauses local DOT without extra damage");
        paused.setFlag(37,true);pauseRoom.update({});pauseRoom.update({});require(state(pauseRoom,id).damage==8,"Resumed CE24 still retains its remaining three waits");
        pauseRoom.update({});require(state(pauseRoom,id).damage==12,"CommonEvent resumes remaining wait instead of starting a new cycle");
        for(int n=0;n<60;++n){pauseRoom.update({});}
        require(state(pauseRoom,id).damage==52&&!paused.flag(37),"CommonEvent pause preserves its lifespan counter until original26 endpoint");
        enemy.rules["runtime_profile"].erase("burn");
    }
    // Domain timing variation preserves M52E4P17C1-10's captured tile and Wait40.
    enemy.rules["runtime_profile"]["reappear"]={{"flag",160},{"wait","pursuer-domain-reappear-wait"}};
    content.setMediaDefinition("cues","pursuer-domain-reappear-wait",{{"kind","wait"},{"tenths",40}});
    std::fill(content.maps.at(enemy.map).passageMasks.begin(),content.maps.at(enemy.map).passageMasks.end(),15);
    entry.playerTile={5,4};entry.session.pursuerHealth=317;entry.session.flags[160]=true;
    {
        GameSession waiting(content.items,entry.session,content.facts);CombatScene room(content,waiting,entry);room.update({});
        require(state(room,id).mode==EnemyMode::Dormant&&hit(room,id).processed==0,"Reappearance cannot receive attacks before original wait");
        InputFrame moving;moving.movement=Direction::Left;room.update(moving);
        for(int n=1;n<239;++n){room.update({});}
        require(state(room,id).mode==EnemyMode::Dormant,"Wait40 retains all240 fixed updates");
        room.update({});
        require(state(room,id).damage==183&&waiting.pursuerHealth()==317,"Original recovery reads500 minus current HP after wait");
        require(findCharacter(room.world(),id).motion.tile()==TilePosition{5,4},"Reappearance uses captured entry player tile rather than initial enemy or later player position");
        require(!waiting.flag(160),"Reappearance consumes only its original transient flag");
    }
    // CN1.00 M52E4P19 is the highest empty stop page: its event wait is retired.
    enemy.rules["stop_fact"]=162;entry.session.flags[162]=false;
    {
        GameSession interrupted(content.items,entry.session,content.facts);CombatScene room(content,interrupted,entry);room.update({});
        for(int n=0;n<60;++n){room.update({});}
        interrupted.setFlag(162,true);room.update({});
        InputFrame moving;moving.movement=Direction::Left;room.update(moving);
        for(int n=0;n<30;++n){room.update({});}
        const auto captured=room.world().player().motion.tile();
        require(captured!=entry.playerTile,"Cancellation variation moves the player before restart");
        interrupted.setFlag(162,false);room.update({});
        for(int n=0;n<239;++n){room.update({});}
        require(state(room,id).mode==EnemyMode::Dormant,"Stopped reappearance retires its old wait and restarts full Wait40");
        room.update({});
        require(findCharacter(room.world(),id).motion.tile()==captured,"Restart captures a fresh player tile after stop rather than stale entry tile");
    }
    std::cout<<"Pursuer direct receiver checks passed\n";
    if(!bodyFailures.empty()){throw std::runtime_error("Pursuer E2 body slice has "+std::to_string(bodyFailures.size())+" failed public scenarios");}
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
