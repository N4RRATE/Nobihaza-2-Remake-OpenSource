#include "g2_natural_driver.h"
#include "full_content.h"
#include "ending_score.h"
#include "content_hash.h"
#include "g3_stage_routes.h"
#include "g3_route_driver.h"
#include "g3_shelves_driver.h"
#include "g3_heat_driver.h"
#include "g3_hunters_driver.h"
#include "g4_truth_driver.h"
#include "g5_control_driver.h"
#include "g6_memorial_driver.h"
#include "g6_cargo_driver.h"
#include <random>
#include <deque>

namespace {
std::string bytes(const std::filesystem::path& path){std::ifstream input(path);need(bool(input),"Missing "+path.string());return {std::istreambuf_iterator<char>(input),{}};}
CombatEntry fixture(const CombatContent& c,int map,const std::string& mode){
    auto e=c.entries.at("post-pickup");e.map=map;e.playerTile={8,9};e.direction=Direction::Up;
    e.positions.clear();e.pages.clear();e.directions.clear();e.through.clear();e.session.difficulty=mode;
    e.session.hp=e.session.maxHp=999;e.session.baseDefense=999;e.session.conditions={};
    e.session.flags[1]=e.session.flags[18]=true;e.session.flags[13]=e.session.flags[15]=false;
    return e;
}
void scoreCases(const std::filesystem::path& path){
    const auto source=Json::parse(bytes(path));int count=0;
    for(const auto& row:source.at("score_cases")){
        const auto& v=row.at("input");EndingScoreInput in;
        in.sprayCount=v.at("sprayCount");in.retryCount=v.at("retryCount");in.saveCount=v.at("saveCount");
        in.hours=v.at("hours");in.minutes=v.at("minutes");in.seconds=v.at("seconds");
        in.guardFirstScore=v.at("guardFirstScore");in.specialItemUsed=v.at("specialItemUsed");
        in.difficultyLevel=v.at("difficultyLevel");in.unlocked=v.at("unlocked").get<std::array<bool,5>>();
        const auto actual=endingScore(in);const auto& expected=row.at("expected");
        need(actual.score==expected.at("score")&&actual.rank==expected.at("rank")&&
             Json(actual.unlocks)==expected.at("unlocks"),"Actual C++ score differs from source case "+row.at("name").get<std::string>());++count;
    }
    need(count==42,"Missing three-difficulty reward boundaries");std::cout<<"LATE_SCORE 42 actual C++ source cases PASS\n";
}
void presets(const CombatContent& c){
    const auto source=Json::parse(bytes("tools/g8-late-game-bindings.json"));
    for(const auto& row:source.at("easy_absent")){
        const auto id=row.at("object").get<std::string>();const auto& d=c.enemies.at(id);
        auto e=fixture(c,d.map,"easy");GameSession s(c.items,e.session,c.facts);s.setFlag(d.deadFlag,true);
        for(const auto& other:row.at("unseeded_neighbors")){s.setFlag(c.enemies.at(other.get<std::string>()).deadFlag,false);}
        s.setVariable(35,7);const auto before=s.exportState();CombatScene scene(c,s,e,safe);
        for(int n=0;n<6;++n){scene.update({});scene.takePresentation();}
        const auto actor=findCharacter(scene.world(),id);const auto tile=row.at("tile");
        need(!actor.active&&actor.motion.tile()==TilePosition{tile.at(0),tile.at(1)},"Preset body remained active/misplaced: "+id);
        need(s.variable(35)==7&&c.activeVariant(c.objects.at(id),s)->page==row.at("terminal_page"),"Preset fabricated a kill or chose a damage-gated page: "+id);
        need(c.activeVariant(c.objects.at(row.at("manager").get<std::string>()),s)->page==row.at("manager_page"),"Preset body cache page remained active: "+id);
        for(const auto& other:row.at("unseeded_neighbors")){need(!s.flag(c.enemies.at(other.get<std::string>()).deadFlag)&&findCharacter(scene.world(),other.get<std::string>()).active,"Preset removed an unseeded neighbor from the actual scene");}
    }
    std::cout<<"LATE_PRESETS five actual scenes/body-cache terminal pages/no kills/neighbor facts PASS\n";
}
void receivers(const CombatContent& c){
    const auto source=Json::parse(bytes("tools/g8-late-game-bindings.json"));int count=0;
    for(const std::string mode:{"easy","normal","hard"}){for(const auto& [id,expected]:source.at("enemy_rules").items()){
        const auto& d=c.enemies.at(id);auto e=fixture(c,d.map,mode);e.positions[id]={8,6};e.directions[id]=Direction::Down;
        GameSession s(c.items,e.session,c.facts);s.setFlag(52,true);s.setFlag(530,true);
        for(const auto& [other,enemy]:c.enemies){if(enemy.map==d.map){s.setFlag(enemy.deadFlag,other!=id);if(enemy.rules.contains("activation_fact")){s.setFlag(enemy.rules.at("activation_fact"),true);}}}
        for(int v:{89,103,117,131,145,159,173,187}){s.setVariable(v,0);}s.setFlag(13,true);
        CombatScene scene(c,s,e,[](int lo,int hi){return lo==-5&&hi==2?0:hi;});scene.update({});scene.takePresentation();
        GridPosition target;const bool chimera=d.rules.value("behavior",std::string{})=="chimera";
        if(chimera){const auto& v=d.rules.at("body_variables");const auto camera=scene.cameraCenter();target={double(s.variable(v[2]))+camera.x-160,double(s.variable(v[3]))+camera.y-120};}
        else{const auto p=findCharacter(scene.world(),id).motion.position();target={(p.x+.5)*16,(p.y+1)*16-8};}
        std::set<std::string> hit;const auto result=scene.resolveAttack({target,{9,9},AttackKind::Handgun,2,1,false,99,AttackPhase::Impact},hit);
        need(result.processed==1&&s.variable(49)==expected.at(mode).at("defense"),"Actual receiver wrong profile "+id+" "+mode+" processed="+std::to_string(result.processed)+" V49="+std::to_string(s.variable(49)));
        scene.deactivate();hit.clear();need(scene.resolveAttack({target,{9,9},AttackKind::Handgun,2,1,false,100,AttackPhase::Impact},hit).processed==0,"Retired scene accepted a late contact");++count;
    }}
    need(count==93,"Incomplete late enemy inventory");std::cout<<"LATE_RECEIVERS 31*3 actual contacts/retirement PASS\n";
}
void mixedActions(const CombatContent& original){
    auto c=original;for(int map:{168,180,202}){std::fill(c.maps.at(map).passageMasks.begin(),c.maps.at(map).passageMasks.end(),15);std::fill(c.terrainIds.at(map).begin(),c.terrainIds.at(map).end(),1);}int cases=0;
    const std::array<std::array<int,2>,3> sourceAttack{{{10,7},{14,10},{25,20}}};int difficulty=0;
    for(const std::string mode:{"easy","normal","hard"}){for(int map:{180,202}){const auto id="m"+std::to_string(map)+"-e9";
        for(int action=0;action<3;++action){auto e=fixture(c,map,mode);e.playerTile=action==2?TilePosition{5,4}:TilePosition{5,6};e.positions[id]={5,5};e.directions[id]=action==2?Direction::Up:Direction::Down;e.session.hp=e.session.maxHp=100;e.session.baseDefense=1;e.session.armor=e.session.otherEquipment=0;
            GameSession state(c.items,e.session,c.facts);state.setFlag(530,true);state.setFlag(10,false);state.setFlag(13,false);for(const auto& [other,enemy]:c.enemies){if(enemy.map==map){state.setFlag(enemy.deadFlag,other!=id);}}
            CombatScene scene(c,state,e,[&](int lo,int hi){if(lo==1&&hi==2){return action==0?1:2;}return lo==-5&&hi==2?0:safe(lo,hi);});for(int n=0;n<1000&&state.hp()==100;++n){scene.update({});scene.takePresentation();}
            const int expected=sourceAttack[difficulty][action==2?1:0];need(state.variable(23)==expected&&state.hp()==100-biteDamage(expected,state.defense(),0),"Mixed source Actor22/20 action reused another page: "+id+" "+mode+" action="+std::to_string(action));
            if(action){need(state.flag(10),"Mixed source grab did not own the player");}scene.deactivate();const auto retired=state.exportState();for(int n=0;n<20;++n){scene.update({});}need(state.exportState()==retired,"Retired mixed action resumed damage");++cases;
        }}++difficulty;
    }need(cases==18,"Incomplete mixed action variation");std::cout<<"LATE_MIXED 2 maps *3 difficulties *bite/front22/back20 actual source damage/retirement PASS\n";
    for(const std::string mode:{"easy","normal","hard"}){auto e=fixture(c,168,mode);e.playerTile={8,5};e.positions["m168-e4"]={5,5};e.directions["m168-e4"]=Direction::Right;e.session.hp=e.session.maxHp=100;e.session.baseDefense=1;e.session.armor=e.session.otherEquipment=0;
        GameSession state(c.items,e.session,c.facts);state.setFlag(397,false);state.setFlag(469,true);state.setFlag(18,false);state.setFlag(451,true);state.setFlag(13,false);CombatScene scene(c,state,e,[](int lo,int hi){return lo==0&&hi==10?6:lo==-5&&hi==2?0:safe(lo,hi);});const int hp=state.hp();for(int n=0;n<80&&state.flag(451);++n){scene.update({});scene.takePresentation();}const int spirit=mode=="easy"?5:mode=="normal"?10:15;
        need(!state.flag(451)&&state.variable(23)==spirit&&state.hp()==hp-biteDamage(spirit,state.defense(),0)&&state.conditions().corroded,"Actual acid projectile lost Actor33 spirit/damage/corrosion: "+mode);scene.deactivate();state.resetRoomFacts();for(int n=0;n<20;++n){scene.update({});}need(!state.conditions().corroded,"Retired acid reapplied corrosion");
    }std::cout<<"LATE_ACID three actual Actor33 projectile contacts/spirit/corrosion/retirement PASS\n";
}

void finalReload(const CombatContent& c){
    bool completed=true;
    for(const std::string mode:{"easy","normal","hard"}){auto e=fixture(c,209,mode);e.playerTile={13,9};e.direction=Direction::Left;e.session.mainWeapon=21;e.session.magazines[21]=0;const auto& gun=c.items.at(21);e.session.inventory[gun.looseItem]=gun.capacity;e.session.inventory[gun.boxItem]=0;
        const auto ready=[&](GameSession& state){state.setFlag(581,true);state.setFlag(13,true);state.setFlag(17,false);state.setFlag(19,false);};
        const auto begin=[&](CombatScene& scene){scene.update(key(InputAction::Weapon));for(int n=0;n<100&&!scene.raised();++n){scene.update({});scene.takePresentation();}need(scene.raised(),"Final reload public raise failed");scene.update(key(InputAction::Attack));};
        GameSession state(c.items,e.session,c.facts);ready(state);CombatScene scene(c,state,e,[](int,int hi){return hi;});begin(scene);for(int n=0;n<600;++n){scene.update({});scene.takePresentation();}
        std::cout<<"LATE_RELOAD "<<mode<<" rounds="<<state.magazine(21)<<" S17="<<state.flag(17)<<" S19="<<state.flag(19)<<'\n';completed&=state.magazine(21)==gun.capacity&&state.count(gun.looseItem)==0&&!state.flag(17)&&!state.flag(19);scene.deactivate();const auto retired=state.exportState();for(int n=0;n<30;++n){scene.update(key(InputAction::Attack));}need(state.exportState()==retired,"Retired final reload resumed");
        GameSession cancelled(c.items,e.session,c.facts);ready(cancelled);CombatScene old(c,cancelled,e,[](int,int hi){return hi;});begin(old);need(cancelled.magazine(21)==gun.capacity,"Reload did not commit actual stock before cancellation");old.deactivate();const auto afterCancel=cancelled.exportState();for(int n=0;n<600;++n){old.update(key(InputAction::Attack));}need(!cancelled.flag(17)&&!cancelled.flag(19)&&cancelled.exportState()==afterCancel,"Cancelled final reload resumed or refunded ammo");
        GameSession guarded(c.items,e.session,c.facts);ready(guarded);guarded.setFlag(19,true);CombatScene blocked(c,guarded,e,[](int,int hi){return hi;});blocked.update(key(InputAction::Weapon));for(int n=0;n<60;++n){blocked.update({});blocked.takePresentation();}need(!blocked.raised()&&guarded.magazine(21)==0&&guarded.flag(19),"Foreign S19 stopped blocking new weapon input");
    }need(completed,"Final dispatch blocked committed reload in one or more difficulties");std::cout<<"LATE_RELOAD all three completion/cancel-after-commit/foreign-lock/retirement PASS\n";
}

void controllers(const CombatContent& c){
    for(const std::string mode:{"easy","normal","hard"}){for(int map:{165,191,192,204,209}){
        auto e=fixture(c,map,mode);if(map==192){e.session.mainWeapon=15;e.session.magazines[15]=2;}GameSession s(c.items,e.session,c.facts);s.setFlag(13,true);s.setFlag(1,true);s.setFlag(18,true);if(map==192){s.setFlag(507,true);}
        CombatScene scene(c,s,e,safe);scene.update({});scene.takePresentation();if(map==192){scene.update(key(InputAction::Weapon));for(int n=0;n<100&&!scene.raised();++n){scene.update({});scene.takePresentation();}need(scene.raised(),"Dora controlled public raise absent");scene.update(key(InputAction::Attack));for(int n=0;n<60;++n){scene.update({});scene.takePresentation();}need(s.magazine(15)==2&&s.flag(225)&&s.variable(89)==0,"Dora S507 failed to intercept natural gun input");}scene.deactivate();const auto before=s.exportState();
        for(int n=0;n<20;++n){scene.update({});}need(s.exportState()==before,"Retired late Boss resumed source continuation");
    }}std::cout<<"LATE_CONTROLLERS 5*3 real constructors/retirement; three Dora S507 gun interceptions PASS\n";
}
void bossDefenses(const CombatContent& c){
    // Independent CN1.00 Actor34/35/36/37/38/39 samples, exercised through real receivers.
    const std::array<std::array<int,6>,3> defense{{{12,5,4,15,24,24},{21,13,5,34,38,38},{25,18,15,42,49,49}}};int difficulty=0,cases=0;
    for(const std::string mode:{"easy","normal","hard"}){
        auto e=fixture(c,165,mode);GameSession s(c.items,e.session,c.facts);s.setFlag(13,true);CombatScene deki(c,s,e,safe);deki.update({});for(int n=0;n<4000&&deki.foreground()!=ForegroundKind::None;++n){deki.update(n%2==0?key():InputFrame{});deki.takePresentation();}need(deki.foreground()==ForegroundKind::None,"Controlled Dekisugi intro did not finish");
        const auto& parts=c.catalog.at("dekisugi").at("parts");for(int selected=0;selected<2;++selected){for(const auto& part:parts){s.setVariable(part.at("registration_variable"),0);s.setVariable(part.at("damage_variable"),0);}const auto& part=parts.at(selected);s.setVariable(part.at("registration_variable"),1);s.setVariable(part.at("x_variable"),150);s.setVariable(part.at("y_variable"),120);s.setVariable(105,1);const auto camera=deki.cameraCenter();std::set<std::string> hit;const auto result=deki.resolveAttack({{150+camera.x-160,120+camera.y-120},{0,0},AttackKind::Handgun,2,0,false},hit);need(result.processed==1&&s.variable(49)==defense[difficulty][selected],"Dekisugi Actor34/35 receiver reused another part/difficulty");++cases;}deki.deactivate();
        // Dora P2/P3 are controlled residual receivers; S507 blocks natural gunfire.
        // Cash P2 C4 requires mode<=6 (or C21 mode11); mode7 is a contact with no damage branch.
        for(int map:{192,191,204,209})for(int page=2;page<=(map==192?3:2);++page){auto entry=fixture(c,map,mode);GameSession state(c.items,entry.session,c.facts);state.setFlag(13,true);state.setFlag(21,true);state.setFlag(581,true);state.setFlag(577,true);state.setVariable(91,7);CombatScene scene(c,state,entry,[](int lo,int){return lo;});scene.update({});scene.takePresentation();state.setVariable(91,(map==192||map==191)?0:7);state.setVariable(89,0);state.setVariable(90,map==209?10:2);state.setVariable(83,150);state.setVariable(84,120);state.setVariable(87,20);state.setVariable(88,20);state.setVariable(105,0);GridPosition target{150,120};if(map==192){const auto a=findCharacter(scene.world(),"m192-e2").motion.position();target={(a.x+.5)*16,(a.y+1)*16-8};}const auto camera=scene.cameraCenter();if(map==192){target.x-=camera.x-160;target.y-=camera.y-120;}std::set<std::string> hit;const auto result=scene.resolveAttack({{target.x+camera.x-160,target.y+camera.y-120},{0,0},AttackKind::Handgun,page,0,false},hit);const int column=map==192?2:map==191?3:map==204?4:5;const int expected=map==192&&page==2?std::array{6,12,18}[difficulty]:defense[difficulty][column];need(result.processed==1&&state.variable(49)==expected,"Actual Boss receiver selected wrong actor defense: "+std::to_string(map)+" "+mode+" processed="+std::to_string(result.processed)+" defense="+std::to_string(state.variable(49)));scene.deactivate();hit.clear();need(scene.resolveAttack({{target.x,target.y},{0,0},AttackKind::Handgun,2},hit).processed==0,"Retired Boss receiver accepted contact");++cases;}
        ++difficulty;
    }need(cases==21,"Missing late Boss defense samples");std::cout<<"LATE_BOSS_DEFENSE Actor34-39 plus Dora residual29 *3 difficulties/part separation/retirement PASS\n";
}
void gates(const CombatContent& c){
    for(const std::string mode:{"easy","normal","hard"}){
        auto e=fixture(c,191,mode);e.playerTile={14,24};e.positions["m191-e2"]={14,5};
        GameSession s(c.items,e.session,c.facts);s.setFlag(13,true);s.setFlag(21,false);s.setFlag(45,false);s.setFlag(5,false);s.setFlag(508,false);
        CombatScene scene(c,s,e,[](int lo,int hi){return lo==0&&hi==650?9:safe(lo,hi);});scene.update({});scene.takePresentation();
        need(s.variable(91)==(mode=="hard"?6:0),"Actual Cash gamma did not keep the HARD-only source gate: "+mode+" mode="+std::to_string(s.variable(91)));
        bool gammaCue=false;for(int n=0;n<120;++n){scene.update({});for(const auto& event:scene.takePresentation()){gammaCue|=event.id.starts_with("m191-e2-p16-");}}need(gammaCue==(mode=="hard"),"Actual Gamma P16 consumer was missing or reachable below HARD: "+mode);if(mode=="hard"){for(int n=0;n<8000&&s.variable(91)==6;++n){scene.update({});scene.takePresentation();}need(s.variable(91)==0&&!s.flag(21),"Hard gamma did not finish original P16 C171-175");for(int id=451;id<=458;++id){need(!s.flag(id),"Hard gamma retained an original beam after completion");}}
        auto hot=fixture(c,181,mode);hot.playerTile={12,6};GameSession h(c.items,hot.session,c.facts);
        h.setFlag(473,true);h.setFlag(474,true);h.setFlag(526,false);h.setVariable(385,12);h.setVariable(386,6);
        CombatScene pickup(c,h,hot,safe);pickup.update({});pickup.takePresentation();pickup.update(key());
        for(int n=0;n<1200&&(pickup.foreground()!=ForegroundKind::None||pickup.dialogue().active());++n){pickup.update(n%8==0?key():InputFrame{});pickup.takePresentation();}
        need(h.count(109)==1&&h.flag(471)&&h.flag(526)==(mode!="easy"),"Actual hot Z pickup did not share526 between NORMAL/HARD: "+mode);
    }
    auto room=c.maps.at(192);room.width=5;room.height=5;room.passageMasks.assign(25,15);room.objects.clear();room.player.tileX=2;room.player.tileY=2;
    ExplorationScene world(room);CharacterState corpse{"bounded-source-parking",999,CharacterMotion({0,24},Direction::Up,128)};
    corpse.active=false;corpse.through=true;corpse.outsideMapMargin=20;world.addTransientActor(corpse);
    bool rejected=false;auto outside=corpse;outside.motion=CharacterMotion({0,25},Direction::Up,128);try{world.setObjectState(outside);}catch(const std::exception&){rejected=true;}need(rejected,"Source parking escaped its explicit margin");
    CharacterState ordinary{"ordinary-outside",998,CharacterMotion({0,5},Direction::Up,128)};rejected=false;try{world.addTransientActor(ordinary);}catch(const std::exception&){rejected=true;}need(rejected,"Default ordinary actor acquired offscreen permission");
    auto unsupported=corpse;unsupported.id="unsupported-margin";unsupported.eventId=997;unsupported.outsideMapMargin=21;unsupported.motion=CharacterMotion({0,0},Direction::Up,128);rejected=false;try{world.addTransientActor(unsupported);}catch(const std::exception&){rejected=true;}need(rejected,"Unreviewed source margin21 accepted");
    std::cout<<"LATE_GATES actual 3 Z526/gamma consumers; alternate-map bounded parking/rejection PASS\n";
}
void rejectLate(const std::filesystem::path& path){
    const auto source=Json::parse(bytes(path));for(int variant=0;variant<4;++variant){auto bad=source;if(variant==0){bad["schema"]="unknown";}if(variant==1){bad["content_compatibility"]="unknown";}if(variant==2){bad["source"]["sha256"]="changed";}if(variant==3){bad["projection"]["controller_rules"]["cash"]["hard"]["actor_attributes"]["6"]=99;}bool rejected=false;try{loadDifficultyLateContent(path,bad);}catch(const std::exception&){rejected=true;}need(rejected,"Unreviewed late wrapper accepted");}std::cout<<"LATE_REJECT four schema/identity/source/projection changes PASS\n";
}
// The full package delivers C98 directly into M215; only an undelivered G7 score slice retains EndingClearRequest.
void rewardScenes(const CombatContent& c,const std::filesystem::path& source,const std::filesystem::path& out){
    const auto raw=bytes(source);const auto saved=decodeSave(c,raw);need(saved.continuation.kind=="clear_after_file","Reward consumer requires a genuine completed Clear source");Json cases=Json::array();
    for(int variant=0;variant<4;++variant){auto e=fixture(c,213,saved.session.difficulty);e.playerTile={9,7};e.session=saved.session;e.session.party.clear();e.session.actor8Name="dammy";GameSession state(c.items,e.session,c.facts);need(state.flag(582),"Reward source lacks its genuine victory");state.setFlag(1,false);state.setFlag(2,false);state.setFlag(18,false);
        for(const auto [id,value]:{std::pair{382,variant==2?1:0},{630,0},{399,variant==2?1:0},{390,variant==2?0:1},{391,variant==2?59:0},{392,variant==1?1:variant==2?59:0},{395,variant==1?39:40}}){state.setVariable(id,value);}state.setFlag(584,variant==1);for(int id=601;id<=605;++id){state.setFlag(id,variant==3);}
        const auto inventory=state.exportState().inventory,magazines=state.exportState().magazines;std::array<bool,5> unlocked{};unlocked.fill(variant==3);std::vector<int> commits;CombatScene scene(c,state,e,safe);int previousMap=213,n=0;for(;n<12000&&scene.mapId()!=215;++n){scene.update(n%2==0?key():InputFrame{});scene.takePresentation();for(int id=601;id<=605;++id){if(state.flag(id)&&!unlocked[id-601]){need(scene.mapId()==214||(previousMap==214&&scene.mapId()==215),"Reward consumer committed outside214 or its same-tick transfer");commits.push_back(id);unlocked[id-601]=true;}}previousMap=scene.mapId();}
        const std::vector<int> expected=variant==3?std::vector<int>{}:saved.session.difficulty=="easy"?std::vector<int>{605}:variant==0?std::vector<int>{604,601,602,603,605}:variant==1?std::vector<int>{602,605}:std::vector<int>{601,603,605};need(scene.mapId()==215&&commits==expected&&state.variable(381)==(variant==1?10:28)&&state.flag(582)&&state.exportState().inventory==inventory&&state.exportState().magazines==magazines,"Actual three-difficulty score/reward consumer differs from original boundaries: "+Json{{"difficulty",saved.session.difficulty},{"variant",variant},{"steps",n},{"map",scene.mapId()},{"request",bool(scene.endingClearRequest())},{"commits",commits},{"expected",expected},{"rank",state.variable(381)},{"inventory_same",state.exportState().inventory==inventory},{"magazines_same",state.exportState().magazines==magazines}}.dump());scene.deactivate();const auto retired=state.exportState();for(int k=0;k<20;++k){scene.update(key());}need(state.exportState()==retired,"Retired reward scene repeated commits");cases.push_back({{"variant",variant},{"rank",state.variable(381)},{"commits",commits},{"steps",n}});
    }need(bytes(source)==raw,"Reward variation rewrote genuine source");std::ofstream(out/("reward-scenes-"+saved.session.difficulty+".json"))<<Json{{"controlled_statistical_variation",true},{"natural_ending",false},{"victory_fact_from_actual_Clear",true},{"source_sha256",contentSha256(raw)},{"difficulty",saved.session.difficulty},{"cases",cases}}.dump(2)<<'\n';std::cout<<"LATE_REWARD_SCENES "<<saved.session.difficulty<<" four actual213/214/C98 consumers/no direct victory write/no stock grant/retirement PASS\n";
}

void loads(const CombatContent& c,int argc,char** argv){
    GameApplication app(c,"post-pickup",safe);
    for(int n=4;n<argc;++n){const std::filesystem::path path=argv[n];const auto raw=bytes(path);const auto disk=decodeSave(c,raw);const auto prepared=prepareRestore(c,disk);std::string error;
        need(app.restore(disk,error),"Actual late independent restore failed: "+error);
        need(app.session().exportState()==prepared.session->exportState()&&app.mapId()==disk.scene.map,"Late source adaptation changed Session/map");
        const auto before=app.session().exportState();const auto generation=app.generation();app.configureRestorePresentation([](const SceneDefinition&,const SavedPresentation&){throw std::runtime_error("missing late media");});
        need(!app.restore(disk,error)&&app.generation()==generation&&app.session().exportState()==before,"Failed late resource candidate replaced live game");app.configureRestorePresentation({});
        need(bytes(path)==raw,"Late independent restore rewrote source disk");std::cout<<"LATE_LOAD "<<path<<" difficulty="<<app.session().difficulty()<<" map="<<app.mapId()<<" count="<<app.session().saveCount()<<" PASS\n";
    }
}
void naturalG3(const CombatContent& c,const std::filesystem::path& out,const std::filesystem::path& initial,const std::string& first,bool observe=false){
    gates(c);
    std::filesystem::create_directories(out);std::mt19937 random(19202026);std::string phase;
    G2NaturalDriver r(c,out,"public-inputs.jsonl",[&](int lo,int hi){
        if(!observe||phase=="boss"){return std::uniform_int_distribution<int>(lo,hi)(random);}
        if(phase=="shelves"&&lo==1&&hi==2){return 1;}
        if((phase=="xy"||phase=="y"||phase=="heat"||phase=="z"||phase=="release")&&lo==0&&hi==400){return 0;}
        if((phase=="xy"||phase=="y"||phase=="heat"||phase=="z"||phase=="release")&&lo==0&&hi==10){return 6;}
        return safe(lo,hi);
    });
    SavedPresentation presentation;int fade=0;std::array<int,6> observedMedicine;observedMedicine.fill(-1);
    r.observe=[&](const PresentationEvent& event){
        if(event.kind==PresentationEvent::Kind::Music){presentation.music=event.sound;fade=0;}
        if(event.kind==PresentationEvent::Kind::StopMusic){presentation.music={};fade=0;}
        if(event.kind==PresentationEvent::Kind::FadeMusic){fade=(event.duration+16)/17;}
        if(event.kind==PresentationEvent::Kind::Cue){const auto& cue=c.cue(event.id);if(cue.at("kind")=="tint"){
            for(int k=0;k<3;++k){presentation.tone[k]=cue.at("tone_percent").at(k);}presentation.saturation=cue.at("tone_percent").at(3);
        }}
    };
    r.afterTick=[&]{if(fade>0&&!--fade){presentation.music={};}r.game.setPresentationState(presentation);if(r.game.hasGame()){for(int id=1;id<=6;++id){const int count=r.game.session().count(id);if(observedMedicine[id-1]>=0&&count!=observedMedicine[id-1]){std::cout<<"LATE_MEDICINE phase="<<phase<<" map="<<r.game.mapId()<<" item="<<id<<" "<<observedMedicine[id-1]<<"->"<<count<<" HP="<<r.game.session().hp()<<" step="<<r.ticks<<'\n';}observedMedicine[id-1]=count;}}};
    std::filesystem::path source=initial;Json stages=Json::array();bool started=first=="boss";
    const auto stage=[&](const std::string& name,int slot,auto action){
        if(!started&&name==first){started=true;}if(!started){return;}
        phase=name;const auto raw=bytes(source);const auto disk=decodeSave(c,raw);presentation=disk.presentation;fade=0;
        const auto folder=out/name;std::filesystem::create_directories(folder);std::ofstream(folder/"slot-01.json")<<raw;
        r.game.returnToTitle();r.game.configureSaveDirectory(folder);if(name!="boss"){r.load();}
        const int begin=r.ticks;action(disk,folder);source=folder/("slot-0"+std::to_string(slot)+".json");
        const auto actual=decodeSave(c,bytes(source));need(actual.scene.map==151&&actual.session.difficulty==disk.session.difficulty&&actual.session.saveCount>disk.session.saveCount,"Natural late G3 stage did not produce a new legal same-difficulty disk");
        stages.push_back(Json{{"stage",name},{"ticks",r.ticks-begin},{"source_sha256",contentSha256(raw)},
            {"output",source.string()},{"output_sha256",contentSha256(bytes(source))},{"save_count",actual.session.saveCount},
            {"difficulty",actual.session.difficulty},{"runtime_state_injection",false}});
        std::ofstream(out/"natural-result.json")<<Json{{"completed_stages",stages},{"steps",r.ticks},{"maps",r.maps},{"last_source",source.string()},{"runtime_state_injection",false},{"presentation","source music/tint/fade events tracked; no native window"},{"controlled_random",observe},{"random_policy",observe?"regional observation: safe/hi and finite acid/attack choices; not ordinary random acceptance":"ordinary uniform mt19937 seed19202026; no event/range overrides"}}.dump(2)<<'\n';
        std::cout<<"LATE_NATURAL "<<name<<" disk="<<source<<" difficulty="<<actual.session.difficulty<<" count="<<actual.session.saveCount<<" PASS\n";
    };
    stage("boss",3,[&](const auto&,const auto&){g3stage::boss(r,true,false);});
    stage("route",3,[&](const auto&,const auto&){r.equipAvailable({15,20,17,19,21});g3route::firstKey(r,false);g3route::returned(r,true);});
    stage("shelves",3,[&](const auto&,const auto& folder){g3shelves::natural(r,folder,true);});
    if(first=="y"){stage("y",3,[&](const auto&,const auto&){g3stage::natural04(r,true,true,observe);});}else{stage("xy",3,[&](const auto&,const auto&){g3stage::natural04(r,false,true,observe);});}
    stage("heat",2,[&](const auto& disk,const auto&){g3heat::natural(r,"natural",disk,true);});
    stage("z",3,[&](const auto& disk,const auto&){g3heat::natural(r,"load-heat",disk,true);});
    stage("release",4,[&](const auto& disk,const auto&){g3heat::natural(r,"load-no",disk,true);});
    stage("hunters",3,[&](const auto&,const auto&){
        const auto before=g3hunters::natural(r,true);
        for(int n=0;n<6000&&(r.game.foreground()!=ForegroundKind::None||r.game.dialogue().active()||!r.game.session().flag(510));++n){
            const bool noise=r.game.dialogue().view().faceImage=="g3-reunion-e304ab60c9b2f547";r.tick(!noise&&n%8==0?key():InputFrame{});
        }
        r.settle();need(r.game.session().flag(510)&&!r.game.session().flag(511),"Natural radio committed reunion early");
        r.actObject("m164-e3");r.receptionBack();need(r.game.session().flag(511)&&r.game.session().count(111)==1&&r.game.session().hp()==before.hp,"Natural reunion changed health/card or did not finish");
        r.monitor(2);r.monitor(3);
    });
    need(started,"Unknown natural G3 starting stage");
}
// Headless observers record rules only; visual/audio timing retains its separate prior evidence.
struct RuleObserver { Json frames=Json::array();void frame(const std::string&){} };
Json doraState(const CombatContent& c,const G2NaturalDriver& r){Json row;const auto& state=r.game.session();const auto& world=r.game.world();const auto* page=c.activeVariant(c.objects.at("m192-e2"),state);row["page"]=page?page->page:0;row["hp"]=state.hp();row["mode"]=state.variable(91);for(int id:{1,10,13,17,18,19,21,45,225,451,452,453,454,455}){row["flags"][std::to_string(id)]=state.flag(id);}for(int id:{1,2,3,4,23,24,29,30,81,82,83,84,85,86,89,90,91,105,119,133,147,161}){row["variables"][std::to_string(id)]=state.variable(id);}const auto record=[&](const std::string& id,const CharacterState& actor){const auto tile=actor.motion.tile();const auto position=actor.motion.position();row["actors"][id]={{"tile",{tile.x,tile.y}},{"position",{position.x,position.y}},{"direction",int(actor.motion.facing())},{"moving",actor.motion.moving()},{"active",actor.active}};};record("player",world.player());for(const auto* id:{"m192-e2","m192-e3","m192-e5","m192-e6","m192-e9","m192-e11"}){record(id,findCharacter(world,id));}return row;}
void doraProbe(const CombatContent& c,const std::filesystem::path& out,const std::filesystem::path& source,const std::filesystem::path& inputTrace){
    const auto raw=bytes(source);const auto disk=decodeSave(c,raw);need(disk.scene.map==151&&disk.session.difficulty=="hard","Dora probe requires the actual HARD M151 source");std::ofstream(out/"slot-01.json")<<raw;
    std::mt19937 random(19202026);G2NaturalDriver r(c,out,"replayed-public-inputs.jsonl",[&](int lo,int hi){return std::uniform_int_distribution<int>(lo,hi)(random);});r.game.configureSaveDirectory(out);r.game.returnToTitle();std::vector<std::string> cues;std::deque<Json> history;
    r.observe=[&](const PresentationEvent& event){if(event.kind==PresentationEvent::Kind::Cue||event.kind==PresentationEvent::Kind::Sound){cues.push_back(event.id);}};
    const auto snapshot=[&]{return doraState(c,r);};
    std::ifstream trace(inputTrace);std::string line;while(std::getline(trace,line)){const auto row=Json::parse(line);need(row.at("map")==(r.game.hasGame()?r.game.mapId():0),"Dora failed trace replay diverged before first hit");InputFrame in;const int movement=row.at("movement"),held=row.at("held"),pressed=row.at("pressed");if(movement>=0){in.movement=Direction(movement);}for(std::size_t i=0;i<in.buttons.size();++i){in.buttons[i].held=held&(1<<i);in.buttons[i].pressed=pressed&(1<<i);}const bool battle=r.game.hasGame()&&r.game.mapId()==192;const auto before=battle?snapshot():Json{};cues.clear();r.tick(in);if(!battle||!r.game.hasGame()||r.game.mapId()!=192){continue;}const auto after=snapshot();history.push_back({{"step",row.at("step")},{"input",row},{"before",before},{"after",after},{"cues",cues}});if(history.size()>90){history.pop_front();}if(after.at("hp").get<int>()<before.at("hp").get<int>()){Json frames=Json::array();for(const auto& frame:history){frames.push_back(frame);}std::ofstream(out/"first-hit.json")<<Json{{"diagnostic_only",true},{"ordinary_Dora_victory",false},{"source_sha256",contentSha256(raw)},{"trace",inputTrace.string()},{"trace_sha256",contentSha256(bytes(inputTrace))},{"random_seed",19202026},{"first_hit_step",row.at("step")},{"frames",frames}}.dump(2)<<'\n';need(bytes(source)==raw,"Dora probe rewrote source");std::cout<<"DORA_FIRST_HIT replay step="<<row.at("step")<<" page="<<before.at("page")<<" mode="<<before.at("mode")<<" HP="<<before.at("hp")<<"->"<<after.at("hp")<<" V23="<<after.at("variables").at("23")<<" diagnostic only\n";return;}}
    throw std::runtime_error("Dora failure trace had no actual first damage");
}
void naturalG4(const CombatContent& c,const std::filesystem::path& out,const std::filesystem::path& source,std::uint32_t seed=19202026){
    const auto raw=bytes(source);const auto disk=decodeSave(c,raw);std::ofstream(out/"slot-01.json")<<raw;
    std::mt19937 random(seed);G2NaturalDriver r(c,out,"public-inputs.jsonl",[&](int lo,int hi){return std::uniform_int_distribution<int>(lo,hi)(random);});r.game.configureSaveDirectory(out);
    SavedPresentation presentation=disk.presentation;int fade=0;RuleObserver observer;std::deque<Json> recent;Json damage=Json::array();int previousHp=-1;
    r.observe=[&](const PresentationEvent& event){if(event.kind==PresentationEvent::Kind::Music){presentation.music=event.sound;fade=0;}if(event.kind==PresentationEvent::Kind::StopMusic){presentation.music={};fade=0;}if(event.kind==PresentationEvent::Kind::FadeMusic){fade=(event.duration+16)/17;}if(event.kind==PresentationEvent::Kind::Cue){const auto& cue=c.cue(event.id);if(cue.at("kind")=="tint"){for(int k=0;k<3;++k){presentation.tone[k]=cue.at("tone_percent").at(k);}presentation.saturation=cue.at("tone_percent").at(3);}}};
    r.afterTick=[&]{if(fade>0&&!--fade){presentation.music={};}r.game.setPresentationState(presentation);if(r.game.hasGame()&&r.game.mapId()==192){auto state=doraState(c,r);state["step"]=r.ticks;recent.push_back(state);if(recent.size()>90){recent.pop_front();}if(previousHp>=0&&r.game.session().hp()<previousHp){Json frames=Json::array();for(const auto& frame:recent){frames.push_back(frame);}damage.push_back({{"step",r.ticks},{"frames",frames}});std::ofstream(out/"dora-damage.json")<<Json{{"diagnostic_only",true},{"source_sha256",contentSha256(raw)},{"random_seed",seed},{"hits",damage}}.dump(2)<<'\n';}previousHp=r.game.session().hp();}else{recent.clear();previousHp=-1;}};
    g4truth::winDora(r,true);for(int n=0;n<20000&&!r.game.inputPromptView().cursor;++n){r.tick(n%8==0?key():InputFrame{});}need(r.game.inputPromptView().cursor.has_value(),"Natural late truth choice absent");r.press();
    for(int n=0;n<30000&&(r.game.mapId()!=191||r.game.foreground()!=ForegroundKind::None);++n){r.tick(n%8==0?key():InputFrame{});}need(r.game.mapId()==191&&r.game.session().flag(504)&&r.game.session().flag(506)&&r.game.session().flag(586)&&r.game.session().variable(283)==220&&r.game.session().variable(627)==5,"Natural late complete truth/220 capture absent");
    g4truth::winCash(r,observer,out,true);r.settle();need(r.game.mapId()==195&&r.game.session().flag(508),"Natural late G5 aftermath absent");g5natural::supplies(r,true);g5natural::saving(r,2,1);g5natural::saving(r,2,2);g5natural::saving(r,2);
    const auto actual=decodeSave(c,bytes(out/"slot-02.json"));need(actual.scene.map==196&&actual.session.difficulty==disk.session.difficulty&&actual.session.saveCount==disk.session.saveCount+1&&bytes(source)==raw,"Natural late G4/G5 save ancestry lost");
    std::ofstream(out/"natural-result.json")<<Json{{"runtime_state_injection",false},{"source_sha256",contentSha256(raw)},{"output_sha256",contentSha256(bytes(out/"slot-02.json"))},{"difficulty",actual.session.difficulty},{"steps",r.ticks},{"maps",r.maps},{"save_count",actual.session.saveCount},{"map",196},{"truth_complete",true},{"No_and_cancel",true},{"rendered",false},{"random_policy","ordinary uniform mt19937 seed"+std::to_string(seed)},{"random_seed",seed}}.dump(2)<<'\n';
    std::cout<<"LATE_NATURAL G4/G5 actual truth/Cash/ordinary save PASS count="<<actual.session.saveCount<<'\n';
}

void naturalG6(const CombatContent& c,const std::filesystem::path& out,const std::filesystem::path& initial,const std::string& first){
    std::mt19937 random(2042026);G2NaturalDriver r(c,out,"public-inputs.jsonl",[&](int lo,int hi){return std::uniform_int_distribution<int>(lo,hi)(random);});
    SavedPresentation presentation;int fade=0,observedHp=-1;
    r.observe=[&](const PresentationEvent& event){if(event.kind==PresentationEvent::Kind::Music){presentation.music=event.sound;fade=0;}if(event.kind==PresentationEvent::Kind::StopMusic){presentation.music={};fade=0;}if(event.kind==PresentationEvent::Kind::FadeMusic){fade=(event.duration+16)/17;}if(event.kind==PresentationEvent::Kind::Cue){const auto& cue=c.cue(event.id);if(cue.at("kind")=="tint"){for(int k=0;k<3;++k){presentation.tone[k]=cue.at("tone_percent").at(k);}presentation.saturation=cue.at("tone_percent").at(3);}}};
    r.afterTick=[&]{if(fade>0&&!--fade){presentation.music={};}r.game.setPresentationState(presentation);if(r.game.hasGame()){const auto& state=r.game.session();if(state.hp()!=observedHp){std::cout<<"LATE_G6_HP map="<<r.game.mapId()<<" hp="<<state.hp()<<" V23="<<state.variable(23)<<" step="<<r.ticks<<'\n';observedHp=state.hp();}}else{observedHp=-1;}};
    std::filesystem::path source=initial;Json stages=Json::array();bool started=false;
    const auto stage=[&](const std::string& name,int slot,int map,auto action){
        if(name==first){started=true;}if(!started){return;}const auto raw=bytes(source);const auto before=decodeSave(c,raw);presentation=before.presentation;fade=0;
        const auto folder=out/name;std::filesystem::create_directories(folder);std::ofstream(folder/"slot-01.json")<<raw;r.game.returnToTitle();r.game.configureSaveDirectory(folder);r.load();const int begin=r.ticks;action();
        source=folder/("slot-0"+std::to_string(slot)+".json");const auto actual=decodeSave(c,bytes(source));need(actual.scene.map==map&&actual.session.difficulty==before.session.difficulty&&actual.session.saveCount>before.session.saveCount,"Natural G6 stage save ancestry lost");
        stages.push_back(Json{{"stage",name},{"steps",r.ticks-begin},{"source_sha256",contentSha256(raw)},{"output",source.string()},{"output_sha256",contentSha256(bytes(source))},{"difficulty",actual.session.difficulty},{"map",map},{"count",actual.session.saveCount}});std::ofstream(out/"natural-result.json")<<Json{{"runtime_state_injection",false},{"random_policy","ordinary uniform mt19937 seed2042026"},{"completed_stages",stages},{"steps",r.ticks},{"maps",r.maps},{"last_source",source.string()}}.dump(2)<<'\n';std::cout<<"LATE_NATURAL G6 "<<name<<" count="<<actual.session.saveCount<<" PASS\n";
    };
    stage("terminal",3,196,[&]{
        need(r.game.mapId()==196&&!r.game.session().flag(528)&&!r.game.session().flag(530),"Natural terminal source not fresh G5 control");const auto inventory=r.game.session().exportState().inventory;
        r.walk({9,6});r.move(Direction::Up);r.tick(key());std::set<std::uint64_t> requests;
        for(int n=0;n<30000;++n){if(r.game.dialogue().active()&&r.game.dialogue().view().display.position==1){requests.insert(r.game.dialogue().view().request);}if(r.game.foreground()==ForegroundKind::None&&!r.game.dialogue().active()&&!r.game.world().player().motion.moving()){break;}r.tick(n%8==0?key(n%16==0?InputAction::Cancel:InputAction::Confirm):InputFrame{});}
        need(r.game.mapId()==195&&r.game.session().flag(528)&&requests.size()==13&&r.game.session().exportState().inventory==inventory,"Natural terminal FILES07 groups/inventory failed");r.actObject("m195-e4");g5natural::saving(r,2);
        r.actObject("m196-e5");r.actObject("m195-e7");need(r.game.mapId()==195&&r.game.session().flag(530)&&r.game.session().flag(529)&&!r.game.session().flag(556),"Natural first-cycle full rescue failed");r.actObject("m195-e4");g5natural::saving(r,3);
    });
    stage("return",2,183,[&]{
        r.healTo(100);r.equipAvailable({20,21,15,19});g6memorial::reception(r);g6memorial::memorial(r);g6memorial::memorialBack(r);
        r.actObject("m159-e5");need(r.game.mapId()==201,"Natural replacement hall missing");g3shelves::fight(r,{},"m201-e4");for(const auto* id:{"m201-e2","m166-e7"}){r.actObject(id);}need(r.game.mapId()==202,"Natural replacement corridor missing");g3shelves::fight(r);
        r.actObject("m202-e24");r.walk({11,5});r.move(Direction::Up);r.tick(key());r.choices(r.game.session().variable(626)==2?2:1);
        for(const auto* id:{"m177-e4","m178-e3","m179-e3","m180-e21"}){r.actObject(id);}need(r.game.mapId()==183,"Natural material return missing");g6memorial::originalSave(r,{23,8},2,1);g6memorial::originalSave(r,{23,8},2,2);g6memorial::originalSave(r,{23,8},2);
    });
    stage("warehouse",2,204,[&]{
        g6cargo::enter(r);g6cargo::prompt(r,1);r.walk({3,6});for(int n=0;n<20;++n){r.press();}r.settle();r.item(1);r.equipAvailable({22,20,15,19,21});g6cargo::prompt(r,0);g6cargo::fight(r);g6cargo::directSave(r);
    });need(started,"Unknown G6 stage");
}

void naturalFinal(const CombatContent& c,const std::filesystem::path& out,const std::filesystem::path& source){
    const auto raw=bytes(source);const auto disk=decodeSave(c,raw);std::ofstream(out/"slot-01.json")<<raw;std::mt19937 random(2092026);G2NaturalDriver r(c,out,"public-inputs.jsonl",[&](int lo,int hi){return std::uniform_int_distribution<int>(lo,hi)(random);});r.game.configureSaveDirectory(out);
    SavedPresentation presentation=disk.presentation;int fade=0,credits=0,victories=0,firstPeak=0,secondPeak=0,firstDamage=0,observedHp=-1;bool reset=false,reloading=false;int reloadStarts=0,previousMap=0;std::optional<MusicPlaybackIdentity> music;std::optional<EndingScoreResult> expected;std::array<bool,5> unlocked{};std::vector<int> commits;std::map<int,int> maps;
    r.observe=[&](const PresentationEvent& event){if(event.musicPlayback){music=event.musicPlayback;}if(event.kind==PresentationEvent::Kind::Music){presentation.music=event.sound;fade=0;}if(event.kind==PresentationEvent::Kind::StopMusic){presentation.music={};fade=0;}if(event.kind==PresentationEvent::Kind::FadeMusic){fade=(event.duration+16)/17;}if(event.kind==PresentationEvent::Kind::Cue){const auto& cue=c.cue(event.id);if(cue.at("kind")=="tint"){for(int k=0;k<3;++k){presentation.tone[k]=cue.at("tone_percent").at(k);}presentation.saturation=cue.at("tone_percent").at(3);}}};
    r.afterTick=[&]{if(fade>0&&!--fade){presentation.music={};}r.game.setPresentationState(presentation);if(r.game.takeFinalBossVictory()){++victories;}if(!r.game.hasGame()){return;}const auto& g=r.game;const auto& state=g.session();
        if(g.mapId()==209){if(state.hp()!=observedHp){std::cout<<"LATE_FINAL_HP hp="<<state.hp()<<" V23="<<state.variable(23)<<" recovery="<<state.flag(45)<<" air="<<state.flag(574)<<" damage="<<state.variable(89)<<" hits="<<state.variable(384)<<" step="<<r.ticks<<'\n';observedHp=state.hp();}const bool currentReload=state.flag(17)&&state.flag(19)&&state.equipped(1)!=23;if(currentReload&&!reloading){++reloadStarts;}reloading=currentReload;if(!reset){firstPeak=std::max(firstPeak,state.variable(384));firstDamage=std::max(firstDamage,state.variable(89));}if(state.flag(574)&&state.variable(384)==0){reset=true;}if(reset&&!state.flag(575)){secondPeak=std::max(secondPeak,state.variable(384));}}
        if(g.mapId()==211&&++credits==9262){need(music&&r.game.reportMusicFirstPass(*music),"Reused source-verified credits completion rejected");}
        if(g.mapId()==215&&music&&music->scene==g.generation()){need(r.game.reportMusicTicks(*music,0),"Clear initial music instance rejected");}
        if(g.mapId()==213&&!expected){const auto in=state.endingScoreInput();expected=endingScore(in);unlocked=in.unlocked;}
        if(expected){for(int id=601;id<=605;++id){if(state.flag(id)&&!unlocked[id-601]){need(g.mapId()==214||(previousMap==214&&g.mapId()==215),"Reward committed outside original map214 or its same-tick transfer");commits.push_back(id);unlocked[id-601]=true;}}}previousMap=g.mapId();
    };
    r.load();need(r.game.mapId()==204&&r.game.session().flag(578),"Natural final source is not won Warehouse disk");r.equipAvailable({21,20,15,13,19});g6cargo::elevator(r,14);g6cargo::cargo(r,8);
    r.walk({15,10});InputFrame raise;raise.buttons[int(InputAction::Weapon)]={true,true,false};r.tick(raise);for(int n=0;n<30;++n){InputFrame turn;turn.buttons[int(InputAction::Weapon)].held=true;turn.movement=Direction::Left;r.tick(turn);}
    int readyTick=r.ticks;for(int n=0;n<50000&&r.game.mode()!=ApplicationMode::Saving;++n){need(r.game.hasGame(),"Natural final/ending lost active scene");need(r.game.session().hp()>0&&r.game.mode()==ApplicationMode::Playing,"Natural final/ending failed map="+std::to_string(r.game.mapId())+" hp="+std::to_string(r.game.session().hp())+" mode="+std::to_string(int(r.game.mode()))+" recovery="+std::to_string(r.game.session().flag(45)));InputFrame input;const bool final=r.game.mapId()==209&&!r.game.session().flag(582);const int weapon=r.game.session().equipped(1),rounds=r.game.session().magazine(weapon);if(final&&weapon!=23&&!r.game.session().flag(574)&&rounds==0&&!r.game.session().hasReserve()){r.lowerForMenu(readyTick);r.equipAvailable({21,20,15,13,19});continue;}if(final&&(r.game.session().flag(45)||r.game.session().flag(10))){if(n%2==0){input=key(n%4==0?InputAction::Up:InputAction::Right);}}else if(final&&!r.game.session().flag(574)&&r.game.session().hp()<70&&r.healingItem()!=0&&r.game.foreground()==ForegroundKind::None&&r.game.session().flag(1)){r.lowerForMenu(readyTick);need(r.healTo(100),"Final public actual medicine absent");continue;}else if(final){input.buttons[int(InputAction::Attack)]={true,true,false};input.buttons[int(InputAction::Weapon)].held=true;input.buttons[int(InputAction::Confirm)].pressed=n%8==0;}else if(n%2==0){input=key();}r.tick(input);if(std::max(0,rounds-r.game.session().magazine(weapon))){readyTick=r.ticks+r.fireRecoveryTicks(weapon);}if(n%1500==0){std::cout<<"LATE_FINAL n="<<n<<" map="<<r.game.mapId()<<" HP="<<r.game.session().hp()<<" damage="<<r.game.session().variable(89)<<" hits="<<r.game.session().variable(384)<<'\n';}}
    need(victories==1&&reset&&firstPeak>=24&&secondPeak>=24&&firstDamage>=1000&&expected&&r.game.mapId()==215&&commits==expected->unlocks&&r.game.session().variable(381)==expected->rank,"Natural two rounds/score/reward boundary missing");need(r.game.mode()==ApplicationMode::Saving&&r.game.fileMenuView(),"Natural Clear actual file menu absent");const auto before=r.game.session().exportState();r.tick(key(InputAction::Down));r.tick(key());need(r.game.mode()==ApplicationMode::Playing,"Natural Clear save failed");const auto actual=decodeSave(c,bytes(out/"slot-02.json"));need(actual.continuation.kind=="clear_after_file"&&actual.session.difficulty==disk.session.difficulty&&actual.session.saveCount==before.saveCount+1&&actual.continuation.statisticBefore==before.variables.at(399),"Natural Clear ancestry/counters wrong");
    for(int n=0;n<400&&!r.game.inputPromptView().cursor;++n){r.tick();}need(r.game.mapId()==216&&r.game.inputPromptView().cursor,"Natural Clear choice absent");need(bytes(source)==raw,"Natural final overwrote source");
    std::ofstream(out/"natural-result.json")<<Json{{"runtime_state_injection",false},{"difficulty",actual.session.difficulty},{"source_sha256",contentSha256(raw)},{"output_sha256",contentSha256(bytes(out/"slot-02.json"))},{"steps",r.ticks},{"maps",r.maps},{"random_policy","ordinary uniform mt19937 seed2092026; no quiet hi selector"},{"ground_firing_tile",{15,10}},{"first_peak",firstPeak},{"second_peak",secondPeak},{"first_damage",firstDamage},{"entry_magazines",disk.session.magazines},{"final_magazines",actual.session.magazines},{"actual_reload_starts",reloadStarts},{"hit_counter_reset",reset},{"victories",victories},{"rank",expected->rank},{"unlocks",commits},{"save_count",actual.session.saveCount},{"audio_feedback","reuse unchanged G7-03 verified credit first-pass tick9262; Clear fresh instance tick0; no new audio timing acceptance"}}.dump(2)<<'\n';std::cout<<"LATE_NATURAL actual two rounds/ending/score/Clear PASS difficulty="<<actual.session.difficulty<<" count="<<actual.session.saveCount<<'\n';
}

}
int main(int argc,char** argv){try{
    std::cout.setf(std::ios::unitbuf);need(argc>=4,"package output rules|load [actual-save...]");std::uint32_t g4Seed=19202026;if(argc>4&&std::string(argv[argc-1]).starts_with("--g4-seed=")){const auto value=std::string(argv[argc-1]).substr(10);std::size_t used=0;const auto seed=std::stoul(value,&used);need(used==value.size()&&seed<=0xffffffffULL,"Invalid ordinary G4 seed");g4Seed=seed;--argc;}const auto c=loadCombatContent(argv[1]);
    need(c.compatibility=="post-f5-g8-late-4","Wrong late-game package");std::filesystem::create_directories(argv[2]);
    const std::string mode=argv[3];
    if(mode=="reload-final"){need(argc==6,"Reload and final requires actual EASY and NORMAL Warehouse disks");finalReload(c);const auto root=std::filesystem::path(argv[2]);std::filesystem::create_directories(root/"easy");naturalFinal(c,root/"easy",argv[4]);std::filesystem::create_directories(root/"normal");naturalFinal(c,root/"normal",argv[5]);}else if(mode=="reload"){finalReload(c);}else if(mode.starts_with("campaign-")){
        need(argc==5||argc==6,"Campaign needs actual source and optional NORMAL final source");std::filesystem::path source=argv[4],root=argv[2];const auto first=mode.substr(9);bool g3=first!="g4"&&first!="terminal"&&first!="return"&&first!="warehouse"&&first!="final";
        if(g3){naturalG3(c,root/"g3",source,first);source=Json::parse(bytes(root/"g3/natural-result.json")).at("last_source").get<std::string>();}
        if(g3||first=="g4"){std::filesystem::create_directories(root/"g4");naturalG4(c,root/"g4",source,g4Seed);source=root/"g4/slot-02.json";}
        if(first!="final"){std::filesystem::create_directories(root/"g6");naturalG6(c,root/"g6",source,g3||first=="g4"?"terminal":first);source=Json::parse(bytes(root/"g6/natural-result.json")).at("last_source").get<std::string>();}
        std::filesystem::create_directories(root/"final");naturalFinal(c,root/"final",source);if(argc==6){std::filesystem::create_directories(root/"normal-final");naturalFinal(c,root/"normal-final",argv[5]);}
    }else if(mode=="dora-probe"){need(argc==6,"Dora probe needs the actual source and failed public trace");doraProbe(c,argv[2],argv[4],argv[5]);}else if(mode.starts_with("controlled-g3-")){need(argc==5,"Controlled G3 observation needs an actual source disk");naturalG3(c,argv[2],argv[4],mode.substr(14),true);}else if(mode=="final"){need(argc==5,"Natural final needs actual Warehouse source");naturalFinal(c,argv[2],argv[4]);}else if(mode.starts_with("g6-")){need(argc==5,"Natural G6 needs actual source");naturalG6(c,argv[2],argv[4],mode.substr(3));}else if(mode=="g4"){need(argc==5,"Natural G4 needs actual G3 source");naturalG4(c,argv[2],argv[4],g4Seed);}else if(mode=="load"){loads(c,argc,argv);}else if(mode.starts_with("g3-")){
        need(argc==5,"Natural G3 needs an actual source disk");naturalG3(c,argv[2],argv[4],mode.substr(3));
    }else{need(mode=="rules"||mode=="verify"||mode=="verify-tail"||mode=="rewards","Unknown late mode");if(mode=="rules"||mode=="verify"){scoreCases("tools/g8-late-game-bindings.json");presets(c);receivers(c);mixedActions(c);controllers(c);gates(c);}if(mode!="rewards"){bossDefenses(c);rejectLate(argv[1]);}if(mode!="rules"){if(mode=="verify"){finalReload(c);}if(mode!="rewards"){loads(c,argc,argv);}std::set<std::string> rewards;for(int n=4;n<argc;++n){const auto saved=decodeSave(c,bytes(argv[n]));if(saved.continuation.kind=="clear_after_file"&&rewards.insert(saved.session.difficulty).second){rewardScenes(c,argv[n],argv[2]);}}need(rewards.size()==3,"Missing actual three-difficulty Clear sources");}}
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
