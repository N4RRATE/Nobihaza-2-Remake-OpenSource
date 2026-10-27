#include "combat_scene.h"
#include "scene_presentation.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
using Json=nlohmann::json;
void require(bool value,const char* reason){if(!value){throw std::runtime_error(reason);}}
Json readJson(const char* path){std::ifstream input(path);return Json::parse(input);}
int main(int argc,char** argv){try{
    if(argc!=2&&argc!=3&&argc!=4){return 2;}auto content=loadCombatContent(argv[1]);
    if(argc==3){
        const auto evidence=readJson(argv[2]);
        auto fullDefinition=*content.full;fullDefinition["runtime"]["player_recovery"].update(evidence.at("player_recovery"));
        content.full=std::make_shared<const Json>(std::move(fullDefinition));
        for(const auto& [id,cue]:evidence.at("cues").items()){
            content.setMediaDefinition("cues",id,cue);
            if(cue.at("kind")=="forced_route"){content.routes[id]=readRouteDefinition(cue);}
        }
    }
    if(argc==4){
        // Source-backed development fixture; the original package and natural gates stay unchanged.
        auto fullDefinition=*content.full;fullDefinition["runtime"]["player_recovery"]=readJson(argv[2]).at("player_recovery");
        content.full=std::make_shared<const Json>(std::move(fullDefinition));
        const auto evidence=readJson(argv[3]);
        for(const auto& [id,cue]:evidence.at("cues").items()){
            content.setMediaDefinition("cues",id,cue);
            if(cue.at("kind")=="forced_route"){content.routes[id]=readRouteDefinition(cue);}
        }
    }
    if(content.full->at("runtime").at("player_recovery").contains("ce113")){
        const auto entry=[&]{
            auto e=content.entries.at("defeated-claimed");e.map=6;e.playerTile={6,4};
            e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();
            e.session.flags[15]=e.session.flags[45]=e.session.flags[17]=e.session.flags[3]=false;
            e.session.flags[148]=true;e.session.flags[149]=false;e.session.conditions.neuralParalysis=true;
            return e;
        };
        const auto flash=content.full->at("runtime").at("player_recovery").at("ce112").at("flash").get<std::string>();
        const auto flashes=[&](CombatScene& scene){const auto events=scene.takePresentation();return std::count_if(events.begin(),events.end(),[&](const auto& event){return event.id==flash;});};
        for(const bool locked:{false,true}){
            auto e=entry();GameSession session(content.items,e.session,content.facts);CombatScene scene(content,session,e);
            for(int tick=1;tick<=240;++tick){
                scene.update({});const auto count=flashes(scene);
                require(session.flag(149)&&session.conditions().neuralParalysis,"CE113 entry sets149 and retains State8 throughout its240-update wait");
                if(tick==1){require(count==0,"CE112 precedes CE113 and sees activation on the following update");}
                if(tick==2||tick==9){require(count==1,"CE112 flashes on first execution and again after its separate end yield");}
                if(tick>2&&tick<9){require(count==0,"CE112 does not flash during its wait or event-end update");}
                if(tick==2){require(scene.world().player().motion.speed()==8,"CE112 source route reduces the actual actor to speed level2");}
            }
            // CE113 reads S3 when its wait completes, not when gas first hits.
            // A message owns input here, so CE77 does not release the held direction mode.
            ChoicePrompt prompt;prompt.options.push_back({"继续",{{MessageTokenKind::Text,U"继续",0}}});scene.requestInput(prompt);
            session.setFlag(3,locked);scene.update({});
            require(!session.conditions().neuralParalysis&&!session.flag(148)&&!session.flag(149),"CE113 removes State8 and clears both source facts once on update241");
            if(scene.world().player().motion.speed()!=(locked?16:32)){
                std::cerr<<"Recovery locked="<<locked<<" speed="<<scene.world().player().motion.speed()<<" S3="<<session.flag(3)<<" raised="<<scene.raised()<<'\n';
            }
            require(scene.world().player().motion.speed()==(locked?16:32),"CE113 uses the distinct direction-lock and ordinary recovery routes");
        }
        {
            auto e=entry();e.map=4;e.playerTile={8,5};GameSession session(content.items,e.session,content.facts);CombatScene scene(content,session,e);
            scene.update({});scene.update({});
            InputFrame walk;
            for(const auto direction:{Direction::Left,Direction::Right,Direction::Up,Direction::Down}){
                if(scene.world().canMove(scene.world().player(),direction)){walk.movement=direction;break;}
            }
            require(walk.movement.has_value(),"Movement fixture starts beside an actually passable tile");scene.update(walk);
            if(scene.world().player().motion.speed()!=8||!scene.world().player().motion.moving()){
                std::cerr<<"Neural walk speed="<<scene.world().player().motion.speed()<<" moving="<<scene.world().player().motion.moving()<<" foreground="<<static_cast<int>(scene.foreground())<<'\n';
            }
            require(scene.world().player().motion.speed()==8&&scene.world().player().motion.moving(),"WASD moves at the source route speed instead of overwriting neural slowdown every input");
        }
        {
            auto e=entry();e.session.flags[148]=false;e.session.flags[149]=true;e.session.flags[17]=true;
            GameSession session(content.items,e.session,content.facts);CombatScene scene(content,session,e);scene.update({});
            require(flashes(scene)==1&&scene.world().player().motion.speed()==32,"CE112 flashes while action17 prevents its slow route");
            session.setFlag(17,false);
            for(int n=0;n<6;++n){scene.update({});}
            require(scene.world().player().motion.speed()==32,"Clearing the action guard does not execute a route between source common cycles");
            scene.update({});require(scene.world().player().motion.speed()==8,"The next CE112 cycle observes the cleared guard and applies slowdown");
        }
        {
            auto e=entry();e.map=4;e.playerTile={8,5};e.session.flags[148]=false;e.session.conditions.neuralParalysis=false;
            e.session.magazines[11]=15;GameSession session(content.items,e.session,content.facts);CombatScene scene(content,session,e);
            InputFrame raise;raise.buttons[static_cast<std::size_t>(InputAction::Weapon)]={true,true,false};scene.update(raise);
            raise.buttons[static_cast<std::size_t>(InputAction::Weapon)].pressed=false;
            for(int n=0;n<44;++n){scene.update(raise);}for(int n=0;n<5;++n){scene.update({});}
            require(scene.raised(),"Direction-lock variation uses the real raised weapon posture");
            InputFrame lock;lock.buttons[static_cast<std::size_t>(InputAction::Cancel)].held=true;scene.update(lock);
            require(session.flag(3)&&scene.world().player().motion.speed()==16,"Entering direction lock writes the original S3 and speed3 once");
            session.setFlag(149,true);scene.update(lock);scene.update(lock);
            require(session.flag(3)&&scene.world().player().motion.speed()==8,"Holding the same direction-lock mode cannot overwrite a later source slow route");
            scene.update({});
            require(!session.flag(3)&&scene.world().player().motion.speed()==32,"Leaving direction lock executes its source speed4 transition, even before the next neural effect cycle");
        }
        {
            auto e=entry();GameSession session(content.items,e.session,content.facts);CombatScene scene(content,session,e);
            for(int n=0;n<10;++n){scene.update({});}
            auto candidate=session;candidate.resetRoomFacts();e.map=4;e.playerTile={8,5};
            CombatScene next(content,candidate,e);ChoicePrompt prompt;prompt.options.push_back({"继续",{{MessageTokenKind::Text,U"继续",0}}});next.requestInput(prompt);
            for(int n=0;n<230;++n){next.update({});}
            require(candidate.conditions().neuralParalysis,"Ordinary scene replacement retains the remaining CE113 wait during a message");
            next.update({});require(!candidate.conditions().neuralParalysis&&session.conditions().neuralParalysis,"A candidate completes without advancing the original session");
            GameSession loaded(content.items,session.exportState(),content.facts);CombatScene loadScene(content,loaded,e);
            for(int n=0;n<240;++n){loadScene.update({});}
            require(loaded.conditions().neuralParalysis,"Load reconstructs the complete wait rather than restoring its old execution cursor");
            loadScene.update({});require(!loaded.conditions().neuralParalysis,"Load finishes its new recovery wait once");
        }
        {
            auto e=entry();GameSession session(content.items,e.session,content.facts);CombatScene scene(content,session,e);
            for(int n=0;n<10;++n){scene.update({});}session.setFlag(148,false);session.setFlag(149,false);
            for(int n=0;n<30;++n){scene.update({});}session.setFlag(148,true);
            for(int n=0;n<230;++n){scene.update({});}
            require(session.conditions().neuralParalysis&&!session.flag(149),"OFF pauses an entered common event; resuming does not replay its initial149 write");
            scene.update({});require(!session.conditions().neuralParalysis,"The paused recovery resumes its original remaining wait");
        }
        std::cout<<"CE112/113 source-backed scene recovery checks passed\n";
        // Isolated source monitor, with a real message pausing autonomous combat.
        // Only the development copy opens Map52; original content gates are untouched.
        auto training=content;training.pendingMapTickets.erase(52);
        const auto gasEntry=[&]{
            auto e=entry();e.map=52;e.playerTile={18,12};e.positions["m52-e4"]={16,10};
            e.session.hp=e.session.maxHp=100;e.session.baseDefense=0;
            e.session.mainWeapon=e.session.auxiliaryWeapon=e.session.armor=e.session.accessory=e.session.otherEquipment=0;
            e.session.flags[18]=e.session.flags[156]=true;
            for(int id:{13,15,21,37,45,148,149,158,160,162}){e.session.flags[id]=false;}
            e.session.conditions={};return e;
        };
        const auto holdMessage=[](CombatScene& scene){ChoicePrompt p;p.options.push_back({"继续",{{MessageTokenKind::Text,U"继续",0}}});scene.requestInput(p);};
        const auto animation=training.full->at("runtime").at("gas_monitors").at("52").at("animation_cue").get<std::string>();
        const auto animations=[&](CombatScene& scene){const auto events=scene.takePresentation();return std::count_if(events.begin(),events.end(),[&](const auto& event){return event.id==animation;});};
        for(const int mode:{0,1,2,3,4,5}){
            auto c=training;auto e=gasEntry();if(mode==1){e.playerTile.x=19;}if(mode==2){e.session.flags[13]=true;}
            GameSession session(c.items,e.session,c.facts);CombatScene scene(c,session,e,[](int,int hi){return hi;});holdMessage(scene);
            for(int n=0;n<420;++n){
                if(mode==4&&n==331){session.startRecovery(0,90);}
                if(mode==5&&n==408){session.setFlag(45,true);session.setVariable(29,10);}
                scene.update({});
            }
            require(animations(scene)==0&&session.hp()==100&&!session.conditions().neuralParalysis,"Gas Wait70 does not animate or damage during its first420 updates");
            if(mode==4){require(session.flag(13)&&session.conditions().invulnerableTicks==1,"Protection expires on the same update as the gas box check");}
            if(mode==5){require(session.flag(45)&&!session.flag(13),"Rise Wait2 completes on the same update as the gas box check");}
            if(mode==3){
                c.setMediaDefinition("cues","domain-move-gas-source",{{"kind","place_object"},{"object","m52-e4"},{"tile",{23,10}}});
                ScenePresentation(scene).cue("domain-move-gas-source");
            }
            scene.update({});
            require(animations(scene)==1,"Gas animation is shown after Wait70 even when its tile box misses or the player is protected");
            if(mode==5){require(!session.flag(45)&&session.flag(13)&&session.conditions().invulnerableTicks==90,"Completed rise grants its protection before the same-step map hazard");}
            const bool hit=mode==0||mode==4;
            require(session.conditions().neuralParalysis==hit&&session.flag(148)==hit&&session.hp()==(hit?88:100),"Gas uses live inclusive plus/minus2 tiles, protection13 and original attack10 damage");
            require(!session.flag(149),"Map gas requests recovery after the common-event stage has already run");
            if(hit){scene.update({});require(session.flag(149)&&session.conditions().neuralParalysis,"The next common update begins the recovery requested by the actual gas monitor");}
        }
        {
            auto e=gasEntry();GameSession session(training.items,e.session,training.facts);CombatScene scene(training,session,e,[](int,int hi){return hi;});holdMessage(scene);
            for(int n=0;n<200;++n){scene.update({});}session.setFlag(158,true);scene.update({});session.setFlag(158,false);
            for(int n=0;n<420;++n){scene.update({});}
            require(animations(scene)==0&&!session.conditions().neuralParalysis,"An empty highest page retires the map monitor wait; returning starts a full Wait70");
            scene.update({});require(animations(scene)==1&&session.conditions().neuralParalysis,"The reactivated monitor executes exactly after its rebuilt420-update wait");
        }
        std::cout<<"Original gas monitor timing, geometry and page lifecycle checks passed\n";
    }
    const auto& recovery=content.full->at("runtime").at("player_recovery").at("ce36");
    // A second audited graphic makes the CE13 timing observable independently of the normal sprite.
    const auto& protection=content.full->at("runtime").at("player_recovery").at("ce13");
    {auto cueVariant=content.cue(protection.at("graphic_pairs").at(0).at(0).get<std::string>());cueVariant["sprite"]="player-7";content.setMediaDefinition("cues",protection.at("graphic_pairs").at(0).at(0).get<std::string>(),cueVariant);}
    const auto firstProtectionRoute=protection.at("graphic_pairs").at(0).at(1).get<std::string>();
    // Both source commands select the same graphic; keep the observable variant consistent.
    {auto cueVariant=content.cue(firstProtectionRoute);cueVariant["steps"][0]["sprite"]="player-7";content.setMediaDefinition("cues",firstProtectionRoute,cueVariant);}
    content.routes[firstProtectionRoute]=readRouteDefinition(content.cue(firstProtectionRoute));
    for(const auto& [hp,required]:{std::pair{20,50},std::pair{21,20},std::pair{50,20},std::pair{51,10}}){
        auto entry=content.entries.at("defeated-claimed");entry.map=6;entry.playerTile={6,4};
        entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();
        entry.session.hp=hp;entry.session.flags[15]=false;entry.session.flags[45]=true;
        entry.session.flags[1]=false;entry.session.flags[10]=false;entry.session.variables[29]=0;
        GameSession session(content.items,entry.session,content.facts);session.grant(158,1);session.setFlag(159,true);
        CombatScene scene(content,session,entry);scene.update({});
        require(session.count(158)==1&&session.flag(159),"CE36 activation preserves aiming aid until the source CE5 cleanup point");
        const auto downSprite=content.cue(recovery.at("down").at("graphic").get<std::string>()).at("sprite");
        require(scene.world().player().spriteId==downSprite,"CE36 uses the actual knocked-down Actor1 graphic");
        InputFrame held;held.movement=Direction::Right;held.buttons[static_cast<std::size_t>(InputAction::Right)].held=true;
        for(int tick=0;tick<30;++tick){scene.update(held);}
        require(session.variable(29)==0&&scene.world().player().motion.tile()==entry.playerTile,"Holding a direction cannot accumulate struggle inputs or move the downed player");
        auto edge=held;edge.buttons[static_cast<std::size_t>(InputAction::Right)].pressed=true;
        for(int n=0;n<required-1;++n){scene.update(edge);scene.update({});}
        require(session.variable(29)==required-1&&session.flag(45)&&!session.flag(1),"HP boundaries use the original struggle threshold before restoring control");
        scene.update(edge);
        const auto riseSprite=content.cue(recovery.at("rise").at("graphic").get<std::string>()).at("sprite");
        require(scene.world().player().spriteId==riseSprite&&session.flag(45)&&!session.flag(1),"The last direction edge starts the rise presentation without releasing control early");
        for(int tick=0;tick<11;++tick){scene.update({});}
        require(session.flag(45)&&!session.flag(1),"Original Wait2 keeps control until all twelve logic steps finish");
        scene.update({});
        require(!session.flag(45)&&session.flag(1)&&session.flag(13)&&session.variable(29)==0,"CE36 restores control through CE5 once and starts separate protection");
        require(session.count(158)==0&&!session.flag(159),"CE5 commits aiming aid cleanup only after the rise wait finishes");
        const auto normalSprite=scene.world().player().spriteId;
        for(int tick=0;tick<59;++tick){scene.update({});}
        require(scene.world().player().spriteId==normalSprite&&session.flag(13),"CE13 does not publish its first graphic before Wait10 finishes");
        scene.update({});
        require(scene.world().player().spriteId=="player-7"&&session.flag(13),"CE13 applies its first source graphic after sixty separate logic steps");
        for(int tick=0;tick<29;++tick){scene.update({});}
        require(session.flag(13),"CE13 keeps protection for the separate Wait5");
        scene.update({});
        require(!session.flag(13)&&scene.world().player().spriteId==normalSprite,"CE13 applies the second source graphic and retires protection after ninety steps");
        const auto completed=session.exportState();scene.deactivate();
        for(int n=0;n<4;++n){scene.update(edge);}
        require(session.exportState()==completed,"A retired scene cannot accumulate inputs or finish an old response");
    }
    {
        auto entry=content.entries.at("defeated-claimed");entry.map=6;entry.playerTile={6,4};entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();
        entry.session.hp=51;entry.session.flags[15]=false;entry.session.flags[45]=true;entry.session.flags[1]=false;entry.session.variables[29]=0;
        GameSession session(content.items,entry.session,content.facts);CombatScene scene(content,session,entry);
        InputFrame edge;edge.buttons[static_cast<std::size_t>(InputAction::Up)].pressed=true;
        for(int n=0;n<10;++n){scene.update(edge);scene.update({});}
        session.loseHp(51);
        for(int tick=0;tick<14;++tick){scene.update({});}
        require(session.hp()==0&&session.flag(15)&&!session.flag(1),"Death cancels the pending rise without restoring live player control");
    }
    {
        auto entry=content.entries.at("defeated-claimed");entry.map=6;entry.playerTile={6,4};entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();
        entry.session.hp=51;entry.session.flags[15]=false;entry.session.flags[45]=false;
        GameSession session(content.items,entry.session,content.facts);CombatScene scene(content,session,entry);
        session.startRecovery(0,90);session.loseHp(51);scene.update({});
        require(session.flag(15)&&!session.flag(13),"CE13 clears protection on death rather than retaining the living wait");
    }
    {
        auto entry=content.entries.at("defeated-claimed");entry.map=6;entry.playerTile={6,4};entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();
        entry.session.hp=51;entry.session.flags[15]=false;entry.session.flags[45]=true;entry.session.flags[1]=false;entry.session.variables[29]=0;
        GameSession session(content.items,entry.session,content.facts);CombatScene scene(content,session,entry);
        ChoicePrompt prompt;prompt.options.push_back({"继续",{{MessageTokenKind::Text,U"继续",0}}});const auto request=scene.requestInput(prompt);
        InputFrame edge;edge.buttons[static_cast<std::size_t>(InputAction::Right)].pressed=true;scene.update(edge);
        const auto downSprite=content.cue(recovery.at("down").at("graphic").get<std::string>()).at("sprite");
        require(scene.world().player().spriteId==downSprite,"Parallel CE36 starts its source presentation even while the message input owns direction keys");
        require(session.variable(29)==0,"Message input prevents CE32 from counting the same direction edge as a struggle");
        require(scene.cancelInput(request),"The current scene can retire its own message input");scene.update(edge);
        require(session.variable(29)==1,"CE32 resumes new direction input after the message request closes");
    }
    std::cout<<"Source-backed player recovery domain checks passed; natural gate unchanged\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
