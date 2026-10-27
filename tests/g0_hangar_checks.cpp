#include "combat_scene.h"
#include "enemy_controller.h"
#include "enemy_host.h"
#include "route_player.h"
#include "hangar_gas_rules.h"
#include "scene_presentation.h"
#include "combat_presentation.h"
#include "scene_renderer.h"
#include "g0_natural_driver.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
namespace{
void need(bool ok,const std::string& why){if(!ok){throw std::runtime_error(why);}}
EnemyView body(const CombatScene& scene){for(const auto& e:scene.enemyStates()){if(e.id=="m120-e13"){return e;}}throw std::runtime_error("Missing hangar body");}
CombatEntry battle(const CombatContent& c){auto e=c.entries.at("post-pickup");e.map=120;e.playerTile={33,8};e.direction=Direction::Left;e.pages.clear();e.positions={{"m120-e13",{27,7}}};e.directions={{"m120-e13",Direction::Down}};e.through.clear();e.session.flags[346]=true;e.session.flags[347]=true;e.session.flags[348]=true;e.session.flags[349]=true;e.session.flags[350]=false;e.session.flags[1]=true;e.session.flags[18]=true;e.session.flags[21]=false;e.session.variables[91]=0;e.session.variables[89]=0;e.session.variables[280]=2;e.session.hp=e.session.maxHp=100;return e;}
int safe(int lo,int hi){if(lo==0&&hi==10){return 5;}if(lo==0&&hi==15){return 9;}return hi;}
AttackResolution hit(CombatScene& scene,int page,std::uint64_t id=0,AttackPhase phase=AttackPhase::Impact){
 std::set<std::string> resolved;auto a=findCharacter(scene.world(),"m120-e13");auto p=a.motion.position();return scene.resolveAttack({{(p.x+.5)*16,(p.y+1)*16-23},{9,9},AttackKind::Handgun,page,0,false,id,phase},resolved);
}
void playtestFixes(const CombatContent& c){
 int failures=0;
 try{
  auto e=battle(c);e.playerTile={48,55};e.session.flags[346]=false;e.session.flags[347]=false;e.session.flags[348]=false;e.session.flags[349]=false;
  GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,safe);
  scene.update({});need(scene.foreground()==ForegroundKind::Content,"First hangar arrival omitted the original Suneo autorun");
  for(int n=0;n<800;++n){InputFrame f;if(n%8==0){f.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;}scene.update(f);scene.takePresentation();}
  const auto actor=findCharacter(scene.world(),"m120-e9");
  need(!actor.active&&scene.foreground()==ForegroundKind::None&&s.flag(1)&&s.flag(18),"First hangar arrival leaves the non-interactive Suneo at the aircraft tail");
  need(!actor.active,"First hangar arrival did not retire its one-shot actor");
  std::cout<<"First arrival actor/story passed\n";
 }catch(const std::exception& e){++failures;std::cout<<"Arrival RED: "<<e.what()<<'\n';}
 need(failures==0,"G0 playtest symptoms remain");
}
void arrivalRoute(const CombatContent& c,const char* recovery,const char* ordinary,const char* chapter,const char* framePath){
 GameApplication app(c,"title",safe);std::string error;
 need(app.restore(decodeSave(c,g0natural::read(recovery)),error),error);need(app.mapId()==121&&!app.session().flag(346),"Expected old M121 recovery before the first visit");
 std::set<MessageRequestId> arrivalMessages;int ticks=0;bool called=false,aircraft=false,osprey=false,cameraMoved=false;
 sf::Font font;std::unique_ptr<CombatPresentation> presentation;if(framePath){need(font.openFromFile(c.maps.at(120).fontPath),"Missing arrival font");presentation=std::make_unique<CombatPresentation>(c,font);}
 const auto tick=[&](InputFrame input=InputFrame{}){const auto camera=app.cameraCenter();app.update(input);++ticks;auto events=app.takePresentation();if(presentation){presentation->update(events);}
  if(app.mapId()==120&&!app.session().flag(346)){const auto& view=app.dialogue().view();if(app.dialogue().active()){arrivalMessages.insert(view.request);}for(const auto& line:view.lines){called|=line.find(U"喂")!=std::u32string::npos;aircraft|=line.find(U"直升机")!=std::u32string::npos;osprey|=line.find(U"V-22")!=std::u32string::npos;}cameraMoved|=app.cameraCenter().x!=camera.x||app.cameraCenter().y!=camera.y;}
 };
 const auto walk=[&](TilePosition goal){const int map=app.mapId();for(int n=0;n<8000;++n){
  if(app.mapId()!=map){return;}if(app.foreground()!=ForegroundKind::None||app.dialogue().active()){tick(n%8==0?g0natural::key():InputFrame{});continue;}
  if(app.world().player().motion.moving()){tick();continue;}if(app.world().player().motion.tile()==goal){return;}
  InputFrame input;input.movement=g0natural::next(app.world(),goal);need(input.movement.has_value(),"First-arrival route is blocked");tick(input);
 }throw std::runtime_error("First-arrival walk timed out");};
 walk({9,5});InputFrame face;face.movement=Direction::Up;tick(face);tick(g0natural::key());
 for(int n=0;n<1000&&app.mapId()==121;++n){tick(n%8==0?g0natural::key():InputFrame{});}
 need(app.mapId()==120&&app.world().player().motion.tile()==TilePosition{48,55},"Original M121 door did not reach its first-arrival location");
 for(int n=0;n<2000;++n){tick(n%8==0?g0natural::key():InputFrame{});if(app.foreground()==ForegroundKind::None&&!findCharacter(app.world(),"m120-e9").active){break;}}
 need(!findCharacter(app.world(),"m120-e9").active&&app.foreground()==ForegroundKind::None&&app.session().flag(1)&&app.session().flag(18),"Natural first-arrival story did not retire the actor and return control");
 need(arrivalMessages.size()==3&&called&&aircraft&&osprey&&cameraMoved&&findCharacter(app.world(),"m120-e9").motion.tile()==TilePosition{27,47},"Natural arrival missed its dialogue, camera or actor route");
 walk({27,52});
 if(framePath){
  SceneRenderer renderer(app.definition());sf::RenderTexture frame({320,240});
  auto view=makeSceneView({320,240},{320,240});const auto center=app.cameraCenter();view.setCenter({float(center.x),float(center.y)});frame.setView(view);frame.clear(sf::Color::Black);presentation->drawBackground(frame,app);renderer.draw(frame,app.world(),presentation->characterFlashes(),app.playerVisible(),presentation->saturation());presentation->draw(frame,app);frame.display();need(frame.getTexture().copyToImage().saveToFile(framePath),"Could not save arrival frame");
 }
 walk({27,44});InputFrame enter;enter.movement=Direction::Up;tick(enter);
 for(int n=0;n<2000&&app.mapId()==120;++n){tick(n%8==0?g0natural::key():InputFrame{});}
 need(app.mapId()==122&&findCharacter(app.world(),"m122-e11").active&&findCharacter(app.world(),"m122-e11").spriteId,"Aircraft Suneo disappeared with the tail actor");
 bool returned=false;
 for(int n=0;n<14000;++n){tick(n%8==0?g0natural::key():InputFrame{});if(app.mapId()==120){returned=true;need(!findCharacter(app.world(),"m120-e9").active,"Completed first visit revived the tail actor");}if(returned&&app.mapId()==122&&app.session().flag(347)&&app.foreground()==ForegroundKind::None){break;}}
 need(returned&&app.mapId()==122&&app.session().flag(346)&&app.session().flag(347)&&findCharacter(app.world(),"m122-e14").active,"Natural visit/return lost the aircraft actor or safe window");
 for(const char* path:{ordinary,chapter}){
  auto saved=decodeSave(c,g0natural::read(path));auto prepared=prepareRestore(c,saved);const auto before=prepared.session->exportState();
  auto e=battle(c);e.session=before;e.session.flags[349]=false;e.playerTile={48,55};GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,safe);
  need(!findCharacter(scene.world(),"m120-e9").active,"Old saved progress replayed the omitted tail actor");
  need(encodeSave(c,saved)==encodeSave(c,decodeSave(c,encodeSave(c,saved))),"Old save codec changed its state");
 }
 need(arrivalMessages.size()==3,"Return replayed first-arrival dialogue");
 std::cout<<"Old M121 public door -> first arrival -> aircraft -> return, aircraft actors and old ordinary/Chapter saves passed: ticks="<<ticks<<'\n';
}
}
int main(int argc,char** argv){try{
 std::cout.setf(std::ios::unitbuf);
 if(argc>=3&&std::string(argv[2])=="--playtest-fixes"){auto c=loadCombatContent(argv[1]);playtestFixes(c);if(argc>=6){arrivalRoute(c,argv[3],argv[4],argv[5],argc>=7?argv[6]:nullptr);}return 0;}
 need(argc>=3,"g0_hangar_checks package source-predicate-cases [frame]");auto c=loadCombatContent(argv[1]);need(c.catalog.at("g0").at("delivered")==5&&c.enemies.contains("m120-e13"),"G0 battle body is not registered");
 need(c.enemies.at("m120-e13").rules.at("behavior")=="hangar"&&!c.enemies.contains("m125-e13"),"Hangar registration leaks into aviation story");
 std::ifstream in(argv[2]);nlohmann::json cases;in>>cases;int gates=0;
 for(const auto& row:cases.at("active_cases")){
  std::array<bool,3> occupied{};for(int n=0;n<3;++n){occupied[n]=row.at("occupied_switches").at(std::to_string(23+2*n));}
  const auto r=activeHangarGases(row.at("level"),occupied);need(r.callsEmitter==row.at("calls_E32_P1"),"Active gas invocation differs from source");std::vector<int> actual;for(int n=0;n<3;++n){if(r.slots[n]){actual.push_back(n==0?5:n==1?7:8);}}
  std::vector<int> expected;for(const auto& p:row.at("initializes_event_pages")){expected.push_back(p[0]);}need(actual==expected,"Active gas slot selection differs from source");++gates;
 }
 for(int mask=0;mask<8;++mask){std::array<bool,3> occupied{};for(int n=0;n<3;++n){occupied[n]=mask&(1<<n);}for(const auto& row:cases.at("hard_passive_cases")){const int before=row.at("V91_before_wait"),after=row.at("V91_after_wait");const bool expected=row.at("body_after_wait_reached");const auto r=passiveHangarGases(before,after,occupied);need(r.callsEmitter==expected,"Passive gas before/after gate changed");for(int n=0;n<3;++n){need(r.slots[n]==(expected&&!occupied[n]),"Passive slot changed");}}}
 std::cout<<gates<<" original active cases and passive before/after gates passed\n";
 auto e=battle(c);
 {GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,safe);scene.update({});scene.takePresentation();need(body(scene).mode!=EnemyMode::Dormant,"Source battle never activated");auto bad=c.enemies.at("m120-e13");bad.rules["behavior"]="hangar-typo";bool rejected=false;try{auto unknown=makeEnemyBehavior(scene,bad);}catch(const std::runtime_error&){rejected=true;}need(rejected,"Unknown named behavior defaulted to a zombie");
  std::set<std::string> resolved;need(scene.resolveAttack({{0,0},{9,9},AttackKind::Handgun,2},resolved).contacts==0,"Distant contact accepted");
  auto first=hit(scene,2,44);need(first.processed==1&&first.damage>0&&body(scene).damage==s.variable(89),"Normal receiver omitted damage");need(hit(scene,2,44).processed==0,"Same attack processed twice with a fresh collision set");
  const int oldHp=s.pursuerHealth(),oldDamage=s.variable(89);s.setFlag(47,true);const auto critical=hit(scene,3,45);need(critical.damage>0&&s.pursuerHealth()==oldHp&&s.variable(89)==oldDamage+critical.damage,"P3 critical flattened actor32 to actor30");
  need(hit(scene,3,46).damage>0&&s.pursuerHealth()<oldHp,"P3 normal actor30 mutation missing");need(hit(scene,2,44).processed==0,"Interleaved attack resurrected processed identity");
 }
 {auto z=e;z.session.baseAttack=0;GameSession s(c.items,z.session,c.facts);CombatScene scene(c,s,z,safe);scene.update({});auto r=hit(scene,1,100);need(r.processed==1&&r.damage==0&&s.variable(89)==0,"Zero damage not acknowledged or inflated");need(hit(scene,1,100).processed==0,"Zero damage identity not retained");}
 std::cout<<"Receiver damage, actor targets, interleaved identity and zero-damage retreat passed\n";
 {auto a=e;a.session.flags[13]=true;GameSession s(c.items,a.session,c.facts);CombatScene scene(c,s,a,safe);scene.update({});const auto pos=findCharacter(scene.world(),"m120-e13").motion.position();const GridPosition center{(pos.x+.5)*16,(pos.y+1)*16-23};std::set<std::string> resolved;const int hp=s.pursuerHealth();auto flight=scene.resolveAttack({center,{10,10},AttackKind::GlNormal,1,0,false,71,AttackPhase::Flight},resolved);need(flight.contacts==1&&flight.processed==0&&s.variable(89)==0,"GL flight mutated/consumed receiver");auto burn=scene.resolveAttack({center,{10,10},AttackKind::GlFlame,2,0,false,72,AttackPhase::Burning},resolved);need(burn.processed==1&&burn.damage==0,"Flame callback did not start source burn");std::set<std::string> repeated;need(scene.resolveAttack({center,{10,10},AttackKind::GlFlame,3,0,false,73,AttackPhase::Impact},repeated).processed==0,"Active source flame accepted another impact");for(int n=0;n<180;++n){scene.update({});scene.takePresentation();}need(s.variable(89)>=4&&s.pursuerHealth()==hp,"CE24 burn did not accumulate independently of actor HP");}
 // Public weapon posture/projectile path reaches the new registered receiver.
 {auto f=c;for(auto& p:f.objects.at("m120-e13").variants){p.autonomousRoute.steps.clear();}auto a=e;a.playerTile={27,11};a.direction=Direction::Up;a.session.flags[13]=true;GameSession s(f.items,a.session,f.facts);need(s.equip(11),"Handgun fixture could not equip admitted item");CombatScene scene(f,s,a,safe);InputFrame raised;raised.buttons[static_cast<int>(InputAction::Weapon)]={true,true,false};scene.update(raised);for(int n=0;n<150;++n){scene.update({});scene.takePresentation();}need(scene.raised(),"Battle input did not raise source handgun");const int ammo=s.magazine(11);InputFrame fire;fire.buttons[static_cast<int>(InputAction::Attack)]={true,true,false};scene.update(fire);for(int n=0;n<80&&s.variable(89)==0;++n){scene.update({});scene.takePresentation();}need(s.magazine(11)==ammo-1&&s.variable(89)>0,"Actual handgun projectile did not damage hangar receiver");}
 std::cout<<"Actual handgun input/projectile passed\n";
 // Each NORMAL source action can run naturally through all its finite waits/routes.
 for(int choice=1;choice<=5;++choice){auto a=e;GameSession s(c.items,a.session,c.facts);CombatScene scene(c,s,a,safe);scene.update({});const int cacheX=s.variable(81),cacheY=s.variable(82);s.setVariable(91,choice);s.setFlag(21,true);bool began=false,ended=false;int swings=0;bool cacheStopped=false,cacheResumed=false;
  for(int tick=0;tick<1000;++tick){scene.update({});for(const auto& event:scene.takePresentation()){swings+=event.kind==PresentationEvent::Kind::Sound&&event.sound.resource==readSound(c.cue("m120-e13-p13-c9").at("sound")).resource;}
   began|=body(scene).sourceState==choice;if(choice==4&&body(scene).sourceState==4){need(s.variable(81)==cacheX&&s.variable(82)==cacheY,"Action4 updated paused E26 tile cache");cacheStopped|=findCharacter(scene.world(),"m120-e13").motion.tile()!=TilePosition{cacheX,cacheY};}if(choice==5&&body(scene).sourceState==5){cacheResumed|=s.variable(87)==12&&s.variable(88)==16;}
   if(began&&body(scene).sourceState==0){ended=true;break;}
  }
  need(began&&ended,"Source action did not finish: "+std::to_string(choice));std::cout<<"Action "<<choice<<" finite waits/routes passed\n";if(choice<=3){need(swings==(choice==3?2:1),"Slash sound/second landing omitted");}if(choice==5){need(cacheResumed,"Action5 did not restore E26 sampling");need(s.flag(23)&&s.flag(25)&&s.flag(27),"Three independent gases not live");}
  if(choice==4){need(cacheStopped,"Jump did not move the unique actor while E26 was paused");}
 }
 for(int choice=1;choice<=5;++choice){auto a=e;GameSession s(c.items,a.session,c.facts);CombatScene scene(c,s,a,safe);scene.update({});s.setVariable(91,choice);s.setFlag(21,true);for(int n=0;n<3;++n){scene.update({});}need(body(scene).sourceState==choice,"Interruption fixture action missing");const auto h=hit(scene,4,200+choice);need(h.processed==1&&h.damage>0&&body(scene).sourceState==0&&!s.flag(21),"Receiver did not interrupt active action");const int committed=s.variable(89);scene.deactivate();scene.takePresentation();for(int n=0;n<80;++n){scene.update({});}need(s.variable(89)==committed&&scene.takePresentation().empty(),"Retired action refunded committed hit or emitted late stages");}
 {auto a=e;GameSession s(c.items,a.session,c.facts);CombatScene scene(c,s,a,safe);scene.update({});s.setVariable(91,4);s.setFlag(21,true);for(int n=0;n<13;++n){scene.update({});scene.takePresentation();}const int cachedX=s.variable(81),cachedY=s.variable(82);const auto foot=scene.sampleActorScreen("m120-e13");const double visualY=s.variable(84);need(visualY<foot.y-23,"Jump arc did not update its independent source screen workspace");const auto actual=findCharacter(scene.world(),"m120-e13").motion.position();const GridPosition center{(actual.x+.5)*16,(actual.y+1)*16-23-(foot.y-23-visualY)};std::set<std::string> resolved;need(scene.resolveAttack({center,{0,0},AttackKind::Handgun,4,0,false,280},resolved).processed==1,"Jump collision reused E26's pre-jump screen position");need(s.variable(81)==cachedX&&s.variable(82)==cachedY,"Jump screen sample overwrote paused tile cache");}
 // Double slash gets a fresh actor/facing sample; cancellation between hits keeps the first damage.
 {auto a=e;a.playerTile={27,7};a.session.variables[91]=3;a.session.flags[21]=true;GameSession s(c.items,a.session,c.facts);CombatScene scene(c,s,a,safe);int swings=0;int committed=0;
  for(int tick=0;tick<300&&swings<1;++tick){scene.update({});for(const auto& event:scene.takePresentation()){if(event.kind==PresentationEvent::Kind::Sound&&event.sound.resource==readSound(c.cue("m120-e13-p15-c9").at("sound")).resource){++swings;}}}
  need(swings==1&&s.hp()<100,"Double first hit not submitted");committed=s.hp();EnemyHost host(scene,"m120-e13");auto actor=host.actor();actor.motion=CharacterMotion({35,7},Direction::Right,actor.motion.speed());host.setActor(actor);
  // The public Host placement seam changes the canonical actor; Sue occupies the adjacent right tile, so a forward-route fixture would be blocked.
  s.setFlag(13,false);for(int tick=0;tick<200&&swings<2;++tick){scene.update({});for(const auto& event:scene.takePresentation()){if(event.kind==PresentationEvent::Kind::Sound&&event.sound.resource==readSound(c.cue("m120-e13-p15-c9").at("sound")).resource){++swings;}}}
  need(swings==2&&s.hp()==committed&&std::abs(s.variable(85))>20&&findCharacter(scene.world(),"m120-e13").motion.facing()==Direction::Right,"Second slash sample: swings="+std::to_string(swings)+" HP="+std::to_string(s.hp())+" committed="+std::to_string(committed)+" dx="+std::to_string(s.variable(85))+" dy="+std::to_string(s.variable(86))+" facing="+std::to_string(static_cast<int>(findCharacter(scene.world(),"m120-e13").motion.facing())));
 }
 {auto a=e;a.playerTile={27,7};a.session.variables[91]=3;a.session.flags[21]=true;GameSession s(c.items,a.session,c.facts);CombatScene scene(c,s,a,safe);int swings=0;for(int tick=0;tick<100&&swings==0;++tick){scene.update({});for(const auto& v:scene.takePresentation()){swings+=v.kind==PresentationEvent::Kind::Sound&&v.sound.resource==readSound(c.cue("m120-e13-p15-c9").at("sound")).resource;}}const int hp=s.hp();s.setFlag(21,false);s.setVariable(91,0);for(int n=0;n<70;++n){scene.update({});for(const auto& v:scene.takePresentation()){swings+=v.kind==PresentationEvent::Kind::Sound&&v.sound.resource==readSound(c.cue("m120-e13-p15-c9").at("sound")).resource;}}need(swings==1&&s.hp()==hp,"Cancelled double slash submitted/refunded damage");}
 // The real source waiting route cancels each slot independently, even if the stop page suppresses cancel pages.
 {auto a=e;a.session.variables[91]=5;a.session.flags[21]=true;GameSession s(c.items,a.session,c.facts);CombatScene scene(c,s,a,safe);for(int n=0;n<45;++n){scene.update({});scene.takePresentation();}need(s.flag(23)&&s.flag(25)&&s.flag(27),"Source roar did not launch gases");s.setFlag(257,true);scene.update({});need(!s.flag(23)&&s.flag(25)&&s.flag(27),"One gas cancellation stopped another slot");s.setFlag(350,true);scene.update({});need(!s.flag(23)&&!s.flag(25)&&!s.flag(27),"Highest stop page retained live gases");auto events=scene.takePresentation();for(int n:{5,7,8}){need(std::any_of(events.begin(),events.end(),[&](const auto& v){return v.id=="m120-e"+std::to_string(n)+"-p3-c1";} )||n==5,"Stop omitted gas picture erase");}}
 {auto a=e;a.session.variables[91]=5;a.session.flags[21]=true;GameSession s(c.items,a.session,c.facts);CombatScene scene(c,s,a,safe);for(int n=0;n<500;++n){scene.update({});scene.takePresentation();}need(!s.flag(23)&&!s.flag(25)&&!s.flag(27),"Gas source route never expired");}
 {auto f=c;f.catalog["flows"]["retire-map"]={{"object","m120-e19"},{"page",2},{"trigger","autorun"},{"steps",nlohmann::json::array({{{"kind","cue"},{"cue","m120-e13-p13-c106"}},{{"kind","relocate"},{"destination",{{"map",122},{"tile",nlohmann::json::array({11,7})}}}}})}};auto a=e;a.session.variables[91]=5;a.session.flags[21]=true;GameSession s(f.items,a.session,f.facts);CombatScene scene(f,s,a,safe);for(int n=0;n<45;++n){scene.update({});scene.takePresentation();}need(s.flag(23)&&s.flag(25)&&s.flag(27),"Transfer fixture did not launch three gases");const auto generation=scene.generation();for(int n=0;n<100&&scene.mapId()==120;++n){scene.update({});scene.takePresentation();}need(scene.mapId()==122&&scene.generation()!=generation&&!s.flag(23)&&!s.flag(25)&&!s.flag(27),"Committed map transfer retained old gas lifetime");s.setVariable(89,800);for(int n=0;n<80;++n){scene.update({});}need(!s.flag(350),"Retired map submitted old encounter");}
 {auto a=e;a.session.variables[91]=5;a.session.flags[21]=true;GameSession s(c.items,a.session,c.facts);CombatScene scene(c,s,a,safe);for(int n=0;n<45;++n){scene.update({});scene.takePresentation();}scene.deactivate();need(!s.flag(23)&&!s.flag(25)&&!s.flag(27),"Retired scene retained gas flags");scene.takePresentation();for(int n=0;n<60;++n){scene.update({});}need(scene.takePresentation().empty(),"Retired scene emitted gas work");}
 // Production05 keeps06 pending. A tiny test flow proves the Host's actual retry/identity contract.
 {GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,safe);scene.update({});s.setVariable(89,799);scene.update({});need(body(scene).mode!=EnemyMode::Ending,"799 triggered ending");s.setVariable(89,800);scene.update({});need(body(scene).mode==EnemyMode::Ending&&scene.foreground()==ForegroundKind::None&&!s.flag(350),"05 forged06 completion");scene.deactivate();scene.takePresentation();for(int n=0;n<80;++n){scene.update({});}need(!s.flag(350)&&scene.takePresentation().empty(),"Inactive scene produced late victory");}
 {auto f=c;f.catalog["flows"]["busy"]={{"object","m120-e19"},{"page",2},{"trigger","autorun"},{"steps",nlohmann::json::array({{{"kind","cue"},{"cue","m120-e13-p13-c106"}},{{"kind","erase_event_once"}}})}};f.catalog["flows"]["g0-hangar-victory"]["runtime_enabled"]=true;f.catalog["flows"]["g0-hangar-victory"]["steps"]=nlohmann::json::array({{{"kind","flag"},{"id",355},{"value",true}}});GameSession s(f.items,e.session,f.facts);CombatScene scene(f,s,e,safe);scene.update({});need(scene.foreground()==ForegroundKind::Content,"Busy foreground missing");s.setVariable(89,800);for(int n=0;n<30;++n){scene.update({});}need(!s.flag(355),"Foreground busy did not reject completion");for(int n=0;n<100;++n){scene.update({});}need(s.flag(355),"Rejected completion was not retried");}
 {auto f=c;f.catalog["flows"]["g0-hangar-victory"]["runtime_enabled"]=true;f.catalog["flows"]["g0-hangar-victory"]["steps"]=nlohmann::json::array({{{"kind","flag"},{"id",355},{"value",true}}});GameSession s(f.items,e.session,f.facts);CombatScene scene(f,s,e,safe);scene.update({});s.setVariable(89,800);s.loseHp(100);scene.update({});need(!s.flag(355)&&scene.foreground()==ForegroundKind::Death,"Dead actor submitted victory");}
 std::cout<<"NORMAL actions, receiver/identity, two landings, three lifetimes, 799/800 retry/death/retirement passed\n";
 if(argc==4){sf::Font font;need(font.openFromFile(c.maps.at(120).fontPath),"Missing font");CombatPresentation presentation(c,font);auto a=e;a.session.variables[91]=5;a.session.flags[21]=true;GameSession s(c.items,a.session,c.facts);c.entries["hangar-frame"]=a;GameApplication scene(c,"hangar-frame",safe);SceneRenderer renderer(scene.definition());sf::RenderTexture frame({320,240});for(int n=0;n<40;++n){scene.update({});presentation.update(scene.takePresentation());}auto view=makeSceneView({320,240},{320,240});const auto center=scene.cameraCenter();view.setCenter({float(center.x),float(center.y)});frame.setView(view);frame.clear(sf::Color::Black);presentation.drawBackground(frame,scene);renderer.draw(frame,scene.world(),presentation.characterFlashes(),scene.playerVisible(),presentation.saturation());presentation.draw(frame,scene);frame.display();need(frame.getTexture().copyToImage().saveToFile(argv[3]),"Could not save hangar frame");}
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
