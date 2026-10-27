#include "combat_scene.h"
#include "enemy_controller.h"
#include <iostream>
#include <stdexcept>
namespace{
void need(bool v,const char* message){if(!v){throw std::runtime_error(message);}}
CombatEntry entry(const CombatContent& c,int map=72){auto e=c.entries.at("defeated-claimed");e.map=map;e.playerTile={14,12};e.direction=Direction::Up;e.positions.clear();e.directions.clear();e.pages.clear();e.through.clear();e.session.flags[1]=e.session.flags[18]=true;e.session.flags[13]=e.session.flags[15]=false;e.session.hp=e.session.maxHp=100;e.session.baseAttack=200;e.session.mainWeapon=19;e.session.inventory[19]=1;e.session.conditions={};e.session.variables[267]=e.session.variables[269]=0;e.session.variables[247]=30;e.session.variables[261]=0;e.session.variables[47]=e.session.variables[48]=8;for(int flag=218;flag<=225;++flag){e.session.flags[flag]=false;}for(const auto& [n,slot]:c.full->at("runtime").at("parking").at("first_guard").at("runtime_profile").at("slots").items()){e.session.variables[slot.at("source_slots").at("damage").get<int>()]=0;e.session.variables[slot.at("source_slots").at("mode").get<int>()]=0;e.session.flags[slot.at("dead_fact").get<int>()]=false;}return e;}
void acknowledge(CombatScene& scene){InputFrame input;input.buttons[static_cast<std::size_t>(InputAction::Confirm)].pressed=true;scene.update(input);}
EnemyView enemy(const CombatScene& scene,const std::string& id){for(const auto& e:scene.enemyStates()){if(e.id==id){return e;}}throw std::runtime_error("Missing slot");}
GridPosition center(const CombatScene& scene,const std::string& id){const auto p=findCharacter(scene.world(),id).motion.position();return {(p.x+.5)*16,(p.y+1)*16-8};}
}
int main(int argc,char** argv){try{
 if(argc!=2&&argc!=3){return 2;}auto c=loadCombatContent(argv[1]);
 if(argc==3&&std::string(argv[2])=="--legacy"){need(!c.full->at("runtime").at("parking").at("first_guard").contains("runtime_profile")&&c.pendingMapTickets.contains(72),"Legacy cache remains admitted without pretending guard implementation");GameSession session(c.items,c.entries.at("natural").session,c.facts);CombatScene scene(c,session,"natural");need(scene.mapId()==c.entries.at("natural").map,"Legacy cache starts its delivered scene");std::cout<<"Reviewed legacy next-parking-cache admission and scene construction passed\n";return 0;}
if(argc==3&&std::string(argv[2])=="--projectile-edge"){
 for(Direction direction:{Direction::Up,Direction::Left}){
  auto e=entry(c);const TilePosition npc=direction==Direction::Up?TilePosition{10,2}:TilePosition{2,10};const TilePosition target=direction==Direction::Up?TilePosition{10,0}:TilePosition{0,10};
  e.positions["m72-e6"]=npc;e.positions["m72-e7"]=target;e.session.variables[261]=5;e.session.variables[c.full->at("runtime").at("parking").at("first_guard").at("runtime_profile").at("slots").at("1").at("source_slots").at("mode").get<int>()]=2;
  GameSession session(c.items,e.session,c.facts);CombatScene scene(c,session,e,[](int lo,int){return lo;});scene.update({});
  need(session.variable(247)==29,"Edge regression launches one real NPC projectile");
  session.setVariable(261,20);session.setFlag(221,true);
  for(const auto& slot:c.full->at("runtime").at("parking").at("first_guard").at("runtime_profile").at("slots")){session.setVariable(slot.at("source_slots").at("damage"),150);}
  for(int tick=0;tick<100;++tick){scene.update({});need(scene.world().containsTile(findCharacter(scene.world(),"m72-e2").motion.tile()),"Projectile always preserves valid World bounds");}
  need(findCharacter(scene.world(),"m72-e2").motion.tile()==TilePosition{1,0}&&!session.flag(215)&&!session.flag(216),"Blocked source forward slots complete and reset projectile");
 }
 std::cout<<"NPC projectile upward/left boundary miss completes original skip-blocked route safely\n";return 0;
}
const auto& g=c.full->at("runtime").at("parking").at("first_guard");
 need(!c.pendingMapTickets.contains(70)&&!c.pendingMapTickets.contains(72)&&(!c.pendingMapTickets.contains(78)||c.pendingMapTickets.at(78)==99),"First guard maps open; later stage may deliver parking");
 std::cerr<<"Guard loader ready\n";
 // CE11 must distinguish low-damage normal, third-hit and guaranteed main criticals.
 for(bool guaranteed:{false,true}){
  auto e=entry(c);e.session.baseAttack=1;e.session.mainWeapon=11;e.positions["m72-e7"]={10,9};GameSession session(c.items,e.session,c.facts);session.setVariable(35,guaranteed?0:2);session.setFlag(47,guaranteed);CombatScene scene(c,session,e,[](int lo,int){return lo;});std::set<std::string> hit;
  scene.resolveAttack({center(scene,"m72-e7"),{0,0},AttackKind::Handgun,2},hit);
  need(enemy(scene,"m72-e7").damage==150&&session.variable(269)==1&&session.flag(12)&&session.variable(35)==0&&!session.flag(47),"Low-damage third/guaranteed CE11 critical sets cumulative150, counts kill and consumes S47/V35");
 }
 // Auxiliary knife third-hit changes damage without consuming a main-weapon guaranteed critical.
 {auto e=entry(c);e.session.baseAttack=1;e.session.auxiliaryWeapon=24;e.session.inventory[24]=1;GameSession session(c.items,e.session,c.facts);session.setVariable(35,2);session.setFlag(47,true);CombatScene scene(c,session,e,[](int lo,int){return lo;});std::set<std::string> hit;scene.resolveAttack({center(scene,"m72-e7"),{0,0},AttackKind::Knife,2},hit);need(!session.flag(12)&&session.flag(47)&&session.variable(35)==3&&enemy(scene,"m72-e7").damage==session.auxiliaryAttack(),"Auxiliary knife third-hit retains S47 and uses its damage branch rather than cumulative150");}
 // A living unheld receiver owns its full source route, including P4's three cells.
 for(int page:{3,4}){
  auto e=entry(c);e.playerTile={10,12};e.positions["m72-e6"]={22,20};e.positions["m72-e7"]={10,9};e.directions["m72-e7"]=Direction::Down;e.session.baseAttack=1;e.session.mainWeapon=11;e.session.variables[35]=0;e.session.variables[261]=20;e.session.flags[221]=true;GameSession session(c.items,e.session,c.facts);CombatScene scene(c,session,e,[](int lo,int){return lo;});std::set<std::string> hit;scene.resolveAttack({center(scene,"m72-e7"),{0,0},AttackKind::Handgun,page,0,page==4},hit);
  need(enemy(scene,"m72-e7").damage<100&&session.variable(267)==0,"Nonlethal unheld hit never invents a rescue");for(int tick=0;tick<50;++tick){scene.update({});}if(findCharacter(scene.world(),"m72-e7").motion.tile()!=TilePosition{10,page==4?6:8}){const auto tile=findCharacter(scene.world(),"m72-e7").motion.tile();std::cerr<<"Reaction page "<<page<<" tile "<<tile.x<<","<<tile.y<<" mode "<<enemy(scene,"m72-e7").sourceState<<" damage "<<enemy(scene,"m72-e7").damage<<"\n";}need(findCharacter(scene.world(),"m72-e7").motion.tile()==TilePosition{10,page==4?6:8},"P3/P4 source reaction moves one/three cells away from the player");
 }
 {auto e=entry(c);e.playerTile={10,3};e.positions["m72-e6"]={22,20};e.positions["m72-e7"]={10,0};e.directions["m72-e7"]=Direction::Down;e.session.baseAttack=1;e.session.mainWeapon=11;e.session.variables[35]=0;e.session.variables[261]=20;e.session.flags[221]=true;GameSession session(c.items,e.session,c.facts);CombatScene scene(c,session,e,[](int lo,int){return lo;});std::set<std::string> hit;scene.resolveAttack({center(scene,"m72-e7"),{0,0},AttackKind::Handgun,4,0,true},hit);for(int tick=0;tick<20;++tick){scene.update({});}need(findCharacter(scene.world(),"m72-e7").motion.tile()==TilePosition{10,0}&&session.variable(267)==0,"P4 source skip-blocked allows shorter wall reaction without invalid position or fake rescue");}
 std::cerr<<"CE11 critical and nonlethal receiver routes ready\n";
 if(argc==3&&std::string(argv[2])=="--receivers"){return 0;}

 // Public attack ownership and complete visit-local regeneration, including slot8.
 {auto e=entry(c);GameSession s(c.items,e.session,c.facts);s.setVariable(261,20);s.setFlag(221,true);s.startEventTimer(EventTimerSource::GuardFirst,120,false,false);CombatScene scene(c,s,e,[](int lo,int){return lo;});
 need(scene.enemyStates().size()==8&&scene.companionHp()==100,"Eight independent enemy slots and independent companion HP");
 for(const auto& [number,slot]:g.at("runtime_profile").at("slots").items()){const std::string id=slot.at("object");std::set<std::string> hit;const auto result=scene.resolveAttack({center(scene,id),{0,0},AttackKind::Ar,6},hit);need(result.processed>=1,"Player attack reaches a real guard receiver");}
 need(s.variable(269)==8,"Only player receiver threshold crossings count once for all8");
 for(int tick=0;tick<500;++tick){scene.update({});}
 need(s.variable(269)==8,"Regeneration does not add player quota");for(const auto& v:scene.enemyStates()){need(v.damage<100,"Every killed slot respawns");}
 need(scene.session().exportState().party==std::vector<int>{1},"Encounter NPC never changes durable party");}
 std::cerr<<"Player and regeneration ready\n";
 // NPC kills use independent receiver and retain the source attack workspace.
 {auto e=entry(c);e.positions["m72-e6"]={10,9};e.positions["m72-e7"]={10,8};e.directions["m72-e6"]=Direction::Up;GameSession s(c.items,e.session,c.facts);s.setVariable(261,11);s.setVariable(47,8);s.setVariable(48,7);s.setVariable(89,99);s.setFlag(18,true);CombatScene scene(c,s,e,[](int lo,int){return lo;});
 for(int tick=0;tick<12;++tick){scene.update({});}need(enemy(scene,"m72-e7").damage>=100&&s.variable(269)==0,"NPC knife kills without player quota");need(s.variable(47)==8&&s.variable(48)==7&&s.variable(250)!=0,"NPC melee samples independent250/251 and preserves47/48");}
 std::cerr<<"NPC melee ready\n";
 // Short fire has one ammunition commit and a World-owned cursor with shared half1.
 {auto e=entry(c);e.positions["m72-e6"]={10,9};e.positions["m72-e7"]={10,5};e.session.variables[261]=5;e.session.variables[17]=e.session.variables[18]=100;GameSession session(c.items,e.session,c.facts);CombatScene scene(c,session,e,[](int lo,int){return lo;});scene.update({});scene.update({});need(session.variable(247)==29&&session.variable(47)==1&&session.variable(48)==1,"NPC fire commits ammo once and writes source projectile half");need(session.variable(18)==92&&session.variable(17)==100,"Projectile init retains source player-center offset anomaly");need(findCharacter(scene.world(),"m72-e2").motion.tile()!=TilePosition{10,9}&&findCharacter(scene.world(),"m72-e2").motion.tile()!=TilePosition{1,0},"NPC projectile actually travels in World");}
 // Kick arms the source delayed grenade; its actual explosion kills without player quota.
 {auto e=entry(c);e.positions["m72-e6"]={10,9};e.positions["m72-e7"]={10,8};e.directions["m72-e6"]=Direction::Up;e.session.variables[261]=14;e.session.variables[91]=10;GameSession session(c.items,e.session,c.facts);CombatScene scene(c,session,e,[](int lo,int){return lo;});bool armed=false,explosion=false;for(int tick=0;tick<250;++tick){scene.update({});armed|=session.variable(266)>0;for(const auto& event:scene.takePresentation()){explosion|=event.id=="m72-e21-p2-c47";}}need(armed&&explosion&&session.variable(266)==0,"Kick uses the real delayed grenade and clears occupied slot");need(session.variable(269)==0,"NPC grenade never adds player quota");}
 std::cerr<<"NPC projectile and grenade ready\n";
 // Hurt hit <=2 and dodge >2 use the real NPC receiver; rebuilding restores transient HP.
 for(int roll:{2,3}){auto e=entry(c);e.positions["m72-e6"]={10,9};e.positions["m72-e7"]={10,8};e.session.variables[91]=2;GameSession session(c.items,e.session,c.facts);CombatScene scene(c,session,e,[roll](int lo,int hi){return lo==0&&hi==10?roll:lo;});scene.update({});need(scene.companionHp()==(roll==2?99:100),"NPC receiver hits at2 and dodges at3");need(session.hp()==100,"NPC hurt never changes player HP");CombatScene rebuilt(c,session,e);need(rebuilt.companionHp()==100,"Scene lifetime owns independent NPC HP");}
 // The real enemy prepares a companion grab, and player contact rescues once.
 {auto e=entry(c);e.positions["m72-e6"]={10,9};e.positions["m72-e7"]={10,8};e.session.variables[91]=3;GameSession session(c.items,e.session,c.facts);CombatScene scene(c,session,e,[](int lo,int){return lo;});scene.update({});need(session.flag(221)&&enemy(scene,"m72-e7").sourceState==8,"Target choice creates real companion grab");session.setFlag(17,true);std::set<std::string> resolved;scene.resolveAttack({center(scene,"m72-e7"),{0,0},AttackKind::Ar,3},resolved);need(session.variable(267)==1&&!session.flag(221),"Player rescue increments267 and releases NPC");scene.resolveAttack({center(scene,"m72-e7"),{0,0},AttackKind::Ar,3},resolved);need(session.variable(267)==1,"Repeated contact cannot duplicate a rescue");}
 std::cerr<<"NPC receiver and rescue ready\n";
 // Ready map autorun performs the original intro into combat, retaining the work counters.
 {auto e=entry(c,70);e.playerTile={22,12};need(c.transitions.at("m67-e9-p2").at("runtime_enabled")&&c.catalog.at("flows").at("m67-e9-p2").at("runtime_enabled"),"Delivered natural parking entrance opens");e.session.flags[236]=true;e.session.flags[238]=e.session.flags[239]=false;e.session.variables[267]=2;e.session.variables[269]=3;GameSession session(c.items,e.session,c.facts);CombatScene scene(c,session,e);for(int tick=0;tick<10000&&!(scene.mapId()==72&&session.eventTimer().source==EventTimerSource::GuardFirst&&session.flag(18));++tick){acknowledge(scene);for(const auto& event:scene.takePresentation()){if(event.kind==PresentationEvent::Kind::Notice){std::cerr<<"Intro notice: "<<event.id<<"\n";}}}need(scene.mapId()==72&&scene.companionHp()==100,"Natural first guard intro enters real map72");need(session.variable(267)==2&&session.variable(269)==3,"First intro retains240-269 counters");need(session.eventTimer().source==EventTimerSource::GuardFirst&&session.eventTimer().remainingTicks>0,"Natural intro starts120-second Session timer");need(session.exportState().party==std::vector<int>{1},"Natural intro uses encounter-local companion");}
 std::cerr<<"Natural intro ready\n";
 // Companion death returns Title while player HP remains positive.
 {auto e=entry(c);GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e);s.setFlag(224,true);for(int tick=0;tick<200&&scene.foreground()!=ForegroundKind::Death;++tick){scene.update({});}need(scene.foreground()==ForegroundKind::Death&&s.hp()==100,"Companion death enters actual failure with live player");bool title=false;for(int tick=0;tick<800&&!title;++tick){acknowledge(scene);title=scene.takeApplicationRequest()==ApplicationMode::Title;}need(title,"Companion failure reaches real Title");}
 std::cerr<<"Failure ready\n";
 // Source highest ending page wins simultaneous timer/quota; full reward bundle and arithmetic.
 for(const auto counts:std::array<std::array<int,2>,4>{{{15,10},{10,2},{5,2},{0,0}}}){
 auto e=entry(c);GameSession s(c.items,e.session,c.facts);s.setVariable(269,counts[0]);s.setVariable(267,counts[1]);s.startEventTimer(EventTimerSource::GuardFirst,120,false,false);for(int tick=0;tick<7200;++tick){s.tickEventTimer();}CombatScene scene(c,s,e);
 for(int tick=0;tick<16000&&!(s.flag(239)&&s.flag(1)&&s.flag(18)&&scene.foreground()==ForegroundKind::None);++tick){acknowledge(scene);}
 if(!(scene.mapId()==70&&s.flag(238)&&s.flag(239))){std::cerr<<"Reward counts "<<counts[0]<<","<<counts[1]<<" map "<<scene.mapId()<<" fg "<<static_cast<int>(scene.foreground())<<" flags "<<s.flag(238)<<s.flag(239)<<s.flag(1)<<s.flag(18)<<" score "<<s.variable(395)<<"\n";for(const auto& event:scene.takePresentation()){if(event.kind==PresentationEvent::Kind::Notice){std::cerr<<"Reward notice "<<event.id<<"\n";}}}need(scene.mapId()==70&&s.flag(238)&&s.flag(239),"Timer or quota exits and rewards complete exactly once");need(s.variable(395)==counts[0]*2+counts[1]*10&&s.variable(269)==counts[0]*2&&s.variable(267)==counts[1]*10,"Original reward arithmetic mutates counters before score");
 if(counts[0]==5){need(s.count(5)>=1,"Middle reward retains healing branch");}
 if(counts[1]>=4){need(s.count(52)==1&&s.count(32)>=1&&s.count(33)>=14&&s.count(35)>=5&&s.count(36)>=5&&s.count(37)>=5,"Highest stock reward retains all source ammunition");}
 need(s.exportState().party==std::vector<int>{1}&&s.flag(1)&&s.flag(18),"Reward cleanup restores control and stable party");}
 std::cout<<"First guard8 slots, player/NPC quota, melee workspace, regeneration, live-player failure, timer/quota rewards and score passed\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
