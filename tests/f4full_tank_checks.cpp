#include "combat_scene.h"
#include "scene_presentation.h"
#include <array>
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* message){if(!ok){throw std::runtime_error(message);}}
InputFrame press(InputAction action){InputFrame input;input.buttons[static_cast<int>(action)].pressed=true;return input;}
int main(int argc,char** argv){try{
    if(argc!=2){return 2;}const auto content=loadCombatContent(argv[1]);
    // CN2022-07-05 translation1.00 M51 E9 P3 C33-39.
    const std::array<std::pair<const char*,TilePosition>,7> tankParts{{
        {"m51-e9",{43,22}},{"m51-e11",{42,22}},{"m51-e12",{41,22}},
        {"m51-e14",{42,21}},{"m51-e13",{41,21}},{"m51-e17",{42,20}},{"m51-e23",{41,20}}
    }};
    for(int mode:{0,1,2}){
        // Start at the real tank with explicit preconditions; this is not a rescue-to-tank playthrough.
        auto entry=content.entries.at("defeated-claimed");entry.map=51;entry.playerTile={43,23};entry.direction=Direction::Up;
        entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();
        GameSession session(content.items,entry.session,content.facts);
        session.setFlag(198,false);session.setFlag(201,false);session.setFlag(1,true);session.setFlag(18,true);session.setFlag(13,false);
        if(mode){session.grant(74,1);}
        CombatScene scene(content,session,entry);scene.update(press(InputAction::Confirm));
        require(scene.foreground()==ForegroundKind::Content,"Real tank page starts through investigation");
        std::map<std::string,GridPosition> origins;for(int part=9;part<=18;++part){const auto id="m51-e"+std::to_string(part);origins[id]=findCharacter(scene.world(),id).motion.position();}
        bool chose=false,observedMotion=false;
        for(int tick=0;tick<20000&&scene.foreground()!=ForegroundKind::None;++tick){
            auto input=press(InputAction::Confirm);
            if(!chose&&scene.inputPromptView().phase!=DialoguePhase::Hidden){chose=true;if(mode==1){input=press(InputAction::Cancel);}}
            scene.update(input);
            if(mode==2&&!session.flag(198)){
                double delta=0;for(const auto& [id,origin]:origins){const auto current=findCharacter(scene.world(),id).motion.position();if(current.x!=origin.x){delta=current.x-origin.x;break;}}
                if(delta!=0){observedMotion=true;for(const auto& [id,origin]:origins){const auto current=findCharacter(scene.world(),id).motion.position();
                    if(std::abs(current.x-origin.x-delta)>.00001||current.y!=origin.y){throw std::runtime_error("Tank intermediate-frame split: "+id+" dx="+std::to_string(current.x-origin.x)+" head="+std::to_string(delta));}}}
            }
        }
        require(scene.foreground()==ForegroundKind::None&&session.flag(1)&&session.flag(18)&&!session.flag(13),"Tank branch restores control after its source presentation");
        require(session.flag(198)==(mode==2),"Only accepting with I74 starts the tank");
        require(session.count(74)==(mode==1?1:0),"Declining preserves I74; accepted discard removes it once");
        require(!session.flag(201)&&scene.mapId()==51,"Starting the tank does not invent underground first-visit completion");
        if(mode!=2){for(const auto& [id,origin]:origins){const auto current=findCharacter(scene.world(),id).motion.position();require(current.x==origin.x&&current.y==origin.y,"No-key and cancellation leave every original tank part in place");}}
        if(mode==2){
            require(observedMotion,"Tank source movement has actual intermediate frames");
            for(const auto& [object,tile]:tankParts){require(findCharacter(scene.world(),object).motion.tile()==tile,"Tank body parts finish at the seven original placements");}
            if(content.catalog.contains("underground_lighting")){
                // P4 is a below-player blank page; enter its tile after the tank moves.
                InputFrame walk;walk.movement=Direction::Up;scene.update(walk);
                for(int tick=0;tick<120&&scene.world().player().motion.moving();++tick){scene.update({});}
                require(scene.world().player().motion.tile()==TilePosition{43,22},"Started tank opens its original same-tile entrance");
                scene.update(press(InputAction::Confirm));bool declined=false;
                for(int tick=0;tick<6000&&scene.foreground()!=ForegroundKind::None;++tick){
                    auto input=press(InputAction::Confirm);
                    if(!declined&&scene.inputPromptView().phase!=DialoguePhase::Hidden){declined=true;input=press(InputAction::Cancel);}
                    scene.update(input);
                }
                require(declined&&scene.mapId()==51&&!session.flag(201)&&session.flag(1)&&session.flag(18),"Declining first underground entry preserves the first-visit fact and restores control");
                auto& undergroundEntry=scene;undergroundEntry.update(press(InputAction::Confirm));bool visited=false;std::string lastCue;
                for(int tick=0;tick<40000&&(!session.flag(201)||undergroundEntry.mapId()!=51||undergroundEntry.foreground()!=ForegroundKind::None);++tick){
                    undergroundEntry.update(press(InputAction::Confirm));visited|=undergroundEntry.mapId()==66;
                    for(const auto& event:undergroundEntry.takePresentation()){if(!event.id.empty()){lastCue=event.id;}}
                }
                if(!(visited&&session.flag(201)&&undergroundEntry.mapId()==51&&undergroundEntry.foreground()==ForegroundKind::None)){
                    std::cerr<<"Underground entry: map="<<undergroundEntry.mapId()<<" visited="<<visited<<" S201="<<session.flag(201)<<" foreground="<<static_cast<int>(undergroundEntry.foreground())<<" transfer="<<undergroundEntry.transferError()<<" last cue="<<lastCue<<'\n';
                }
                require(visited&&session.flag(201)&&undergroundEntry.mapId()==51&&undergroundEntry.foreground()==ForegroundKind::None,"First unlit underground visit runs its story and returns while committing S201");
                require(session.flag(1)&&session.flag(18)&&session.count(75)==0,"Unlit return restores control without inventing a flashlight");
                auto& refused=scene;refused.update(press(InputAction::Confirm));
                for(int tick=0;tick<6000&&refused.foreground()!=ForegroundKind::None;++tick){refused.update(press(InputAction::Confirm));}
                require(refused.mapId()==51&&refused.foreground()==ForegroundKind::None&&session.flag(201),"A later tank visit without I75 refuses entry instead of replaying the intro");
                std::cout<<"Natural same-tile entry, declined entry, unlit intro return and repeat rejection passed\n";
                for(bool powered:{false,true}){
                    session.grant(75,1);session.setFlag(236,powered);session.setFlag(200,false);
                    scene.update(press(InputAction::Confirm));
                    for(int tick=0;tick<6000&&scene.foreground()!=ForegroundKind::None;++tick){scene.update(press(InputAction::Confirm));scene.takePresentation();}
                    require(scene.mapId()==66&&scene.world().player().motion.tile()==TilePosition{27,5}&&scene.foreground()==ForegroundKind::None,"Later flashlight entry uses the original post-intro landing");
                    require(session.flag(201)&&session.flag(200)==!powered&&session.flag(1)&&session.flag(18),"Post-intro flashlight entry preserves first visit and uses the current power state");
                    InputFrame face;face.movement=Direction::Right;scene.update(face);
                    scene.update(press(InputAction::Confirm));
                    for(int tick=0;tick<6000&&scene.foreground()!=ForegroundKind::None;++tick){scene.update(press(InputAction::Confirm));scene.takePresentation();}
                    require(scene.mapId()==51&&scene.world().player().motion.tile()==TilePosition{43,22},"Ordinary underground return restores the same tank entrance across scene lifetimes");
                    session.discard(75,1);
                }
            }
        }
    }
    for(auto direction:{Direction::Right,Direction::Down}){for(bool blocked:{false,true}){
        auto c=content;c.enemies.clear();auto e=c.entries.at("defeated-claimed");e.map=16;e.playerTile={21,5};e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();
        GameSession session(c.items,e.session,c.facts);CombatScene scene(c,session,e);
        auto& world=const_cast<ExplorationScene&>(scene.world());
        for(auto actor:world.objects()){actor.active=false;world.setObjectState(actor);}
        auto follower=world.objects().at(0),leader=world.objects().at(1),wall=world.objects().at(2);leader.active=true;leader.through=false;leader.layer=1;
        bool found=false;
        for(int y=2;y<c.maps.at(16).height-3&&!found;++y){for(int x=2;x<c.maps.at(16).width-3&&!found;++x){
            leader.motion=CharacterMotion({x,y},direction,16);auto ahead=leader;
            if(!world.canMove(leader,direction)){continue;}ahead.motion=CharacterMotion(adjacentTile(leader.motion.tile(),direction),direction,16);
            if(!world.canMove(ahead,direction)){continue;}
            follower=leader;follower.id=world.objects().at(0).id;follower.eventId=world.objects().at(0).eventId;follower.motion=CharacterMotion(adjacentTile(leader.motion.tile(),static_cast<Direction>((static_cast<int>(direction)+2)%4)),direction,16);
            if(!world.canMove(follower,direction)){continue;}found=true;
        }}require(found,"Different-size scene supplies a real traversable translation corridor");
        wall.active=blocked;wall.through=false;wall.layer=1;wall.motion=CharacterMotion(adjacentTile(leader.motion.tile(),direction),direction,16);world.setObjectState(wall);world.setObjectState(leader);world.setObjectState(follower);
        const auto originFollower=follower.motion.position(),originLeader=leader.motion.position();
        RouteDefinition route;route.frequency=8;route.skipBlocked=true;route.target=follower.eventId;route.steps={RouteStep{std::string(directionName(direction))},RouteStep{std::string(directionName(direction))}};c.routes["variation-follower"]=route;route.target=leader.eventId;c.routes["variation-leader"]=route;
        ScenePresentation presentation(scene);presentation.route("variation-follower");presentation.route("variation-leader");
        for(int tick=0;tick<120;++tick){presentation.advanceRoutes();world.advanceCharacters();const auto a=findCharacter(world,follower.id).motion.position(),b=findCharacter(world,leader.id).motion.position();
            require(std::abs((a.x-originFollower.x)-(b.x-originLeader.x))<.00001&&std::abs((a.y-originFollower.y)-(b.y-originLeader.y))<.00001,"Different map and heading keep same-batch actors aligned");
        }
        const auto end=findCharacter(world,leader.id).motion.tile();require(end==(blocked?leader.motion.tile():adjacentTile(adjacentTile(leader.motion.tile(),direction),direction)),"Static occupancy still blocks source skipBlocked routes");
    }}
    std::cout<<"Different-size scene right/down translation and static-blocked variations passed\n";
    if(content.full->at("runtime").contains("tank_return")){
        auto entry=content.entries.at("defeated-claimed");entry.map=61;entry.playerTile={17,8};entry.direction=Direction::Up;
        entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();
        GameSession session(content.items,entry.session,content.facts);
        for(int flag:{1,18,178,180,193,194}){session.setFlag(flag,true);}session.setFlag(195,false);session.setFlag(13,false);
        CombatScene scene(content,session,entry);bool visitedReturn=false;
        for(int tick=0;tick<80000&&(scene.mapId()!=25||scene.foreground()!=ForegroundKind::None);++tick){
            scene.update(press(InputAction::Confirm));visitedReturn|=scene.mapId()==182;
        }
        require(visitedReturn&&scene.mapId()==25&&scene.foreground()==ForegroundKind::None,"Original rescue completion runs through Map182 before returning to the conference room");
        require(session.flag(195)&&session.count(74)==1&&session.flag(1)&&session.flag(18),"Rescue hands over exactly one tank key and restores control after returning");
        for(const auto* actor:{"m25-e12","m25-e13"}){require(!findCharacter(scene.world(),actor).spriteId,"Source S195 highest empty pages retire old conference actors");}
        const auto retired12=findCharacter(scene.world(),"m25-e12").motion.tile(),retired13=findCharacter(scene.world(),"m25-e13").motion.tile();
        for(int tick=0;tick<30;++tick){scene.update({});}
        require(findCharacter(scene.world(),"m25-e12").motion.tile()==retired12&&findCharacter(scene.world(),"m25-e13").motion.tile()==retired13,"Retired conference pages do not keep an old actor route alive");
        require(session.count(74)==1,"Settled return does not repeat the tank reward");
        std::cout<<"Sena reward, Map182 story and retired conference actors passed\n";
    }
    std::cout<<"Tank no-key, cancel, source movement and key discard passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
