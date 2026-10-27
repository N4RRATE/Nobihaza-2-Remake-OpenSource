#include "g2_natural_driver.h"
#include "g3_route_driver.h"
#include "zombie_behavior.h"
#include "enemy_host.h"
namespace {
using g3route::firstKey;
using g3route::naturalFight;
using g3route::closeManual;
using g3route::returned;

EnemyView enemy(const CombatScene& scene,const std::string& id){for(const auto& e:scene.enemyStates()){if(e.id==id){return e;}}throw std::runtime_error("Missing G3 enemy");}
AttackResolution hit(CombatScene& scene,const std::string& id,int receiver,AttackKind kind=AttackKind::Handgun){const auto p=findCharacter(scene.world(),id).motion.position();std::set<std::string> ids;return scene.resolveAttack({{(p.x+.5)*16,(p.y+.5)*16},{0,0},kind,receiver,0,false},ids);}
CombatEntry entry(const CombatContent& c,const SessionEntry& base,int map,TilePosition tile){auto e=c.entries.at("post-pickup");e.map=map;e.playerTile=tile;e.direction=Direction::Up;e.session=base;e.session.flags[1]=true;e.session.flags[18]=true;e.session.flags[10]=false;e.positions.clear();e.directions.clear();e.pages.clear();e.through.clear();return e;}
void eastDoors(const CombatContent& c,const SessionEntry& base){
 need(c.catalog.at("flows").at("m164-e4-p1").value("runtime_enabled",false),"M164 E4P1 still waits for task 1 after G3 delivery");
 for(int event:{2,4,5}){for(int state=0;state<4;++state){
  const std::string id="m164-e"+std::to_string(event);const auto origin=c.objects.at(id).tile;
  auto e=entry(c,base,164,{origin.x-1,origin.y});e.direction=Direction::Right;
  GameSession session(c.items,e.session,c.facts);session.setFlag(510,state>=1);session.setFlag(511,state>=2);session.setFlag(530,state>=3);
  CombatScene scene(c,session,e,safe);scene.update({});scene.takePresentation();
  need(c.activeVariant(c.objects.at(id),session)->page==state+1,"M164 east-door page condition/priority mismatch");
  scene.update(key());bool blockedText=false,pending=false,bgm=false;bool settled=false;
  for(int n=0;n<2000;++n){
   if(scene.dialogue().view().phase==DialoguePhase::AwaitingInput){blockedText=true;scene.update(key());}else{scene.update({});}
   for(const auto& ev:scene.takePresentation()){pending|=ev.kind==PresentationEvent::Kind::Notice;bgm|=ev.kind==PresentationEvent::Kind::Music;}
   if(scene.foreground()==ForegroundKind::None&&!scene.dialogue().active()){settled=true;break;}
  }
  std::cout<<"east-door E"<<event<<" page="<<state+1<<" map="<<scene.mapId()<<" bgm="<<bgm<<"\n";
  need(settled&&!pending&&session.flag(1)&&session.flag(18),"M164 east-door flow stalled/pending/leaked control lock");
  if(state==1){need(scene.mapId()==164&&blockedText&&!bgm,"M164 radio restriction failed");}
  else{need(scene.mapId()==166&&scene.world().player().motion.tile()==TilePosition{3,event==5?18:19},"M164 east-door distinct source landing lost");need(bgm==(state!=3),"M164 later page replayed/stopped source BGM");}
 }}
 std::cout<<"M164 E2/E4/E5 four-page conditions, distinct landing, restriction and music passed for "<<c.compatibility<<"\n";
}
void corpseParking(const CombatContent& c,const SessionEntry& base){
 for(bool dead:{false,true}){
  auto e=entry(c,base,174,{4,8});e.session.flags[385]=dead;e.session.flags[386]=dead;
  GameSession session(c.items,e.session,c.facts);CombatScene scene(c,session,e,safe);
  for(int n=0;n<100;++n){scene.update({});scene.takePresentation();}
  need(findCharacter(scene.world(),"m174-e3").active!=dead&&findCharacter(scene.world(),"m174-e5").active!=dead,"G3 hidden parking removed living enemy or revived corpse");
 }
 for(int example=0;example<4;++example){
  auto bad=c;auto& placement=bad.catalog["flows"]["m174-e7-p2"]["steps"][0]["placements"][0];
  if(example==0){placement["tile"]={4,8};}
  if(example==1){placement["object"]="m173-e3";}
  if(example==2){placement["if_flag"]=468;}
  if(example==3){placement["object"]="m174-e4";}
  bool rejected=false;try{validateSceneContent(bad,*bad.maps.begin()->second.sprites);}catch(const std::exception&){rejected=true;}
  need(rejected,"G3 illegal hidden placement bypassed bounds/map/death/enemy validation");
 }
 auto valid=entry(c,base,174,{4,8});GameSession session(c.items,valid.session,c.facts);ExplorationScene world(c.makeScene(174,{4,8},Direction::Up,session,&valid));
 auto outside=findCharacter(world,"m174-e3");outside.motion=CharacterMotion({0,34},Direction::Up,8);
 bool rejected=false;try{world.setObjectState(outside);}catch(const std::exception&){rejected=true;}
 need(rejected,"G3 ordinary off-map coordinate became legal");
 std::cout<<"G3 hidden corpse/alive/map/death/object/bounds checks passed\n";
}
void focused(const CombatContent& c,const SessionEntry& base){
 for(const auto& id:{"m173-e3","m174-e3","m174-e5"}){
  const auto& d=c.enemies.at(id);const int threshold=d.rules.at("runtime_profile").at("fall").at("damage_threshold");
  auto e=entry(c,base,d.map,{1,1});e.session.mainWeapon=0;e.session.baseAttack=threshold+3;e.session.flags[47]=false;
  GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,[](int low,int high){return low==0&&high==10?2:high;});scene.update({});
  auto initial=hit(scene,id,2);std::cout<<"G3 fall sample "<<id<<" threshold="<<threshold<<" damage="<<initial.damage<<" attack="<<s.attack()<<" defense="<<d.rules.at("defense")<<"\n";need(initial.damage==threshold,"G3 original50/60 receiver damage mismatch");scene.update({});need(enemy(scene,id).mode==EnemyMode::Fallen,"G3 source threshold did not fall");
  const int count=s.variable(35);need(hit(scene,id,3).processed==1&&s.variable(35)==count,"G3 grounded main receiver advanced damage");
  for(int n=0;n<310;++n){scene.update({});scene.takePresentation();}need(enemy(scene,id).mode!=EnemyMode::Fallen&&!s.flag(d.deadFlag),"G3 fall did not recover without death");
  hit(scene,id,5,AttackKind::De);for(int n=0;n<500;++n){scene.update({});scene.takePresentation();}need(s.flag(d.deadFlag),"G3 own zombie death fact missing");
 }
 std::cout<<"G3 P11 check\n";
 {auto varied=c;auto& map=varied.maps.at(173);std::fill(map.passageMasks.begin(),map.passageMasks.end(),15);std::fill(varied.terrainIds.at(173).begin(),varied.terrainIds.at(173).end(),1);auto e=entry(varied,base,173,{12,5});e.positions["m173-e3"]={5,5};e.session.flags[10]=true;GameSession s(varied.items,e.session,varied.facts);CombatScene scene(varied,s,e,safe);scene.update({});const auto initial=findCharacter(scene.world(),"m173-e3").motion.tile();for(int n=0;n<200;++n){scene.update({});scene.takePresentation();}need(findCharacter(scene.world(),"m173-e3").motion.tile()!=initial&&enemy(scene,"m173-e3").mode==EnemyMode::Pursuit,"G3 empty P11 froze autonomous pursuit");s.setFlag(10,false);scene.update({});need(findCharacter(scene.world(),"m173-e3").motion.speed()==8,"G3 P11 release lost pursuit profile");}
 std::cout<<"G3 varied push check\n";
 for(bool blocked:{false,true}){
  auto varied=c;auto& map=varied.maps.at(175);map.width+=2;map.height+=1;map.passageMasks.assign(map.width*map.height,15);varied.terrainIds.at(175).assign(map.width*map.height,1);varied.terrainIds.at(175).at(6*map.width+15)=3;
  if(blocked){map.passageMasks.at(6*map.width+15)=0;}
  auto e=entry(varied,base,175,{15,8});GameSession s(varied.items,e.session,varied.facts);CombatScene scene(varied,s,e,safe);scene.update({});scene.takePresentation();InputFrame push;push.movement=Direction::Up;scene.update(push);bool sound=false;
  for(int n=0;n<300&&scene.foreground()!=ForegroundKind::None;++n){scene.update({});for(const auto& ev:scene.takePresentation()){sound|=ev.kind==PresentationEvent::Kind::Sound;}}
  need(findCharacter(scene.world(),"m175-e9").motion.tile()==(blocked?TilePosition{15,7}:TilePosition{15,6}),"G3 push reused landing terrain gate or ignored passage");need(sound!=blocked&&!s.flag(468)&&s.flag(1)&&!s.flag(19),"G3 push sound/commit/control mismatch");
 }
 std::cout<<"G3 reading check\n";
 {GameSession s(c.items,base,c.facts);ReadingController reader(c,s,"item-191",17);reader.update({},18);need(reader.finished()&&!reader.restore()&&reader.takePresentation().empty(),"G3 retired manual reading emitted stale cues");}
 {GameSession s(c.items,base,c.facts);const auto before=s.exportState();ReadingController reader(c,s,"item-191",31);for(int repeat=0;repeat<2;++repeat){int pages=0;for(int n=0;n<10000&&!reader.atMenu();++n){if(reader.dialogue().view().phase==DialoguePhase::AwaitingInput){++pages;reader.update(key(),31);}else{reader.update({},31);}}need(reader.atMenu()&&pages==9,"G3 CE162 complete nine pages/menu missing");if(repeat==0){reader.repeat();}}reader.close();for(int n=0;n<500&&!reader.finished();++n){reader.update({},31);}bool show=false,erase=false;for(const auto& ev:reader.takePresentation()){if(ev.kind==PresentationEvent::Kind::Cue){const auto& cue=c.cue(ev.id);show|=cue.at("kind")=="show_picture";erase|=cue.at("kind")=="erase_picture";}}need(reader.finished()&&reader.restore()&&show&&erase&&s.exportState()==before,"G3 FILES04 repeat/close lifecycle leaked or mutated Session");}

 std::cout<<"G3 north writer check\n";
 {auto e=entry(c,base,168,{11,8});GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,safe);for(int n=0;n<2000&&!s.flag(461);++n){scene.update({});scene.takePresentation();}need(s.flag(461),"G3 researcher north-route writer absent");}
 std::cout<<"G3 completed lock/exit check\n";
 for(bool terrainBlocked:{false,true}){
  auto varied=c;auto& map=varied.maps.at(175);std::fill(map.passageMasks.begin(),map.passageMasks.end(),15);std::fill(varied.terrainIds.at(175).begin(),varied.terrainIds.at(175).end(),1);if(terrainBlocked){varied.terrainIds.at(175).at(7*map.width+16)=3;}
  auto e=entry(varied,base,175,{13,5});e.session.flags[468]=true;GameSession s(varied.items,e.session,varied.facts);CombatScene scene(varied,s,e,safe);scene.update({});InputFrame push;push.movement=Direction::Up;scene.update(push);need(scene.foreground()==ForegroundKind::None&&findCharacter(scene.world(),"m175-e9").motion.tile()==TilePosition{13,4},"G3 completed vent allowed push");
  // Use the source relocation after completion, retaining climb and directional exit.
  auto e2=entry(varied,base,175,{13,5});e2.session.flags[468]=true;varied.terrainIds.at(175).at(4*map.width+14)=terrainBlocked?3:1;GameSession s2(varied.items,e2.session,varied.facts);CombatScene mounted(varied,s2,e2,safe);mounted.update({});mounted.update(key());for(int n=0;n<1000&&mounted.foreground()!=ForegroundKind::None;++n){mounted.update({});}InputFrame turn;turn.movement=Direction::Right;mounted.update(turn);mounted.update(key());for(int n=0;n<1000&&mounted.foreground()!=ForegroundKind::None;++n){mounted.update({});}need(s2.flag(69)==terrainBlocked&&s2.flag(1)&&s2.flag(18),"G3 down-ladder terrain rejection leaked locks or ignored gate");
 }
 corpseParking(c,base);
 std::cout<<"G3 invalid history check\n";
 {auto bad=base;bad.worldFlags[c.facts.switches.at(468)]=true;bad.worldFlags[c.facts.switches.at(450)]=false;bool rejected=false;try{validateFullSession(c,bad);}catch(const std::exception&){rejected=true;}need(rejected,"G3 invalid vent history accepted");}
 std::cout<<"G3-02 focused falling/death/changed-size/terrain3/passage/sound/rejected history passed\n";
}

}
int main(int argc,char** argv){try{std::cout.setf(std::ios::unitbuf);need(argc==5,"G3 route package source isolated-root natural|finish|load|unpassed-load|rules|corpse");auto c=loadCombatContent(argv[1]);if(std::string(argv[4])=="doors"){eastDoors(c,decodeSave(c,read(argv[2])).session);return 0;}const bool cycle=std::string(argv[4])=="cycle-natural";need(c.compatibility==(cycle?"post-f5-g7-normal-7":"post-f5-g3-normal-2"),"Wrong G3-02 identity");auto saved=decodeSave(c,read(argv[2]));auto root=std::filesystem::path(argv[3]);std::filesystem::create_directories(root);std::string mode=cycle?"natural":argv[4];if(cycle){need(saved.session.worldValues.at(c.facts.variables.at(635))==1,"Cycle route requires real completion count");}if(mode=="rules"){focused(c,saved.session);return 0;}if(mode=="corpse"){corpseParking(c,saved.session);return 0;}
 if(mode=="load"){for(int example=0;example<4;++example){auto bad=saved.session;if(example==0){bad.worldFlags.erase(c.facts.switches.at(468));}if(example==1){bad.worldFlags[c.facts.switches.at(450)]=false;}if(example==2){bad.inventory[191]=2;}if(example==3){bad.inventory[105]=0;bad.inventory[106]=0;}bool rejected=false;try{validateFullSession(c,bad);}catch(const std::exception&){rejected=true;}need(rejected,"G3 malformed saved facts/inventory accepted");}auto unknown=Json::parse(read(argv[2]));unknown["content"]="post-f5-g3-normal-999";bool rejected=false;try{decodeSave(c,unknown.dump());}catch(const std::exception&){rejected=true;}need(rejected,"G3 unknown save identity accepted");std::cout<<"G3 missing-fact/468-prerequisite/manual-sum/X-missing/identity rejected\n";}

 if(mode=="natural"){std::ofstream(root/"slot-01.json")<<read(argv[2]);}G2NaturalDriver r(c,root,"public-inputs-"+mode+".jsonl");r.game.configureSaveDirectory(root);r.load(mode=="load"?3:(mode=="unpassed-load"||mode=="finish")?2:1);if(mode=="natural"){firstKey(r,!cycle);returned(r,cycle);}else if(mode=="finish"){firstKey(r,false);returned(r);}else if(mode=="unpassed-load"){
 need(!r.game.session().flag(468)&&!r.game.session().flag(463),"G3 unpassed Load fabricated completion");r.reception();r.actObject("m159-e5");r.actObject("m164-e2");r.actObject("m166-e21");r.actObject("m170-e2");r.actObject("m169-e5");need(findCharacter(r.game.world(),"m175-e9").motion.tile()==TilePosition{15,7},"G3 unpassed Load retained ladder position");
 }else{need(r.game.session().flag(387)&&!r.game.session().flag(385)&&!r.game.session().flag(386),"G3 independent Load lost actual zombie deaths");need(r.game.session().flag(468)&&r.game.session().flag(463)&&r.game.session().flag(512),"G3 independent Load lost permanent route facts");need(r.game.session().count(105)+r.game.session().count(106)==1&&r.game.session().count(191)+r.game.session().count(206)==1,"G3 independent Load lost key/manual");r.reception();r.actObject("m159-e5");r.actObject("m164-e2");r.actObject("m166-e21");r.actObject("m170-e2");r.actObject("m169-e5");need(findCharacter(r.game.world(),"m175-e9").motion.tile()==TilePosition{13,4},"G3 Load/reentry ladder reset completed fact");r.actObject("m175-e2");r.actObject("m169-e3");r.actObject("m170-e3");need(r.game.mapId()==173&&!findCharacter(r.game.world(),"m173-e3").active,"G3 independent Load revived natural M173 corpse");const auto body=findCharacter(r.game.world(),"m173-e3").motion.tile();for(int n=0;n<120;++n){r.tick();}need(findCharacter(r.game.world(),"m173-e3").motion.tile()==body&&r.game.session().hp()==100&&r.game.session().magazine(20)==4,"G3 Load corpse resumed combat or restored consumed round");}
 std::cout<<"G3-02 "<<mode<<" ticks="<<r.ticks<<" HP="<<r.game.session().hp()<<" save="<<r.game.session().saveCount()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
