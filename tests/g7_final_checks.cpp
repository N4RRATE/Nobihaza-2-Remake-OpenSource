#include "combat_content.h"
#include "combat_scene.h"
#include "full_content.h"
#include "route_player.h"
#include "game_application.h"
#include "save_snapshot.h"
namespace { void need(bool,const std::string&); }
#include "g4_dora_render.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace{
void need(bool value,const std::string& message){if(!value){throw std::runtime_error(message);}}
int quiet(int low,int high){return high;}
InputFrame attack(){InputFrame input;input.buttons[int(InputAction::Attack)]={true,true,false};return input;}
InputFrame confirm(){InputFrame input;input.buttons[int(InputAction::Confirm)]={true,true,false};return input;}
CombatEntry battle(const CombatContent& content,bool started=true){
 auto entry=content.entries.at("post-pickup");entry.map=209;entry.playerTile={13,9};entry.direction=Direction::Left;entry.positions.clear();entry.pages.clear();entry.directions.clear();entry.through.clear();
 initializeFullSession(content,entry.session);entry.session.hp=100;entry.session.maxHp=100;entry.session.mainWeapon=21;entry.session.armor=0;entry.session.inventory[23]=0;entry.session.inventory[49]=0;entry.session.flags[1]=true;entry.session.flags[18]=true;entry.session.flags[586]=false;
 entry.session.worldFlags[content.facts.switches.at(581)]=started;entry.session.variables[81]=150;entry.session.variables[203]=150;entry.session.variables[211]=121;entry.session.variables[111]=50;entry.session.variables[112]=130;entry.session.variables[125]=50;entry.session.variables[126]=130;entry.session.variables[139]=50;entry.session.variables[140]=130;return entry;
}
void tick(CombatScene& scene,int count=1){for(int n=0;n<count;++n){scene.update({});scene.takePresentation();}}
void thresholds(const CombatContent& c){
 for(int damage:{999,1000})for(int body:{149,150})for(int hits:{24,25}){
  auto entry=battle(c);GameSession s(c.items,entry.session,c.facts);s.setFlag(13,true);s.setVariable(89,damage);s.setVariable(203,body);s.setVariable(384,hits);CombatScene scene(c,s,entry,quiet);tick(scene);
  need(s.flag(574)==(damage>=1000&&body>=150&&hits>=25),"First threshold lost damage/body/25-hit conjunction");need(!s.flag(582),"First1000 incorrectly won");
 }
 for(bool dead:{false,true})for(int body:{149,150})for(int damage:{999,1000}){
  auto entry=battle(c);GameSession s(c.items,entry.session,c.facts);s.setFlag(13,true);s.setFlag(576,true);s.setFlag(15,dead);s.setVariable(89,damage);s.setVariable(384,0);s.setVariable(203,body);CombatScene scene(c,s,entry,quiet);tick(scene);need(s.flag(582)==(!dead&&body==150&&damage==1000),"Second1000 ignored awakening/death");
 }
 std::cout<<"G7 both thresholds/body150/24-25/death gates PASS\n";
}
void receivers(const CombatContent& c){
 for(int page=2;page<=6;++page){
  auto entry=battle(c);GameSession s(c.items,entry.session,c.facts);s.setFlag(13,true);CombatScene scene(c,s,entry,[](int low,int){return low;});tick(scene);s.setVariable(105,0);s.setVariable(89,0);s.setVariable(90,10);s.setVariable(83,150);s.setVariable(84,120);s.setVariable(87,20);s.setVariable(88,120);
  const auto camera=scene.cameraCenter();AttackSample sample{{150+camera.x-160,120+camera.y-120},{0,0},AttackKind::De,page,0,false,91};std::set<std::string> hit;const auto result=scene.resolveAttack(sample,hit);need(result.processed==1&&result.damage>0,"Missing explicit E10 receiver "+std::to_string(page));need(s.variable(105)==(page==2?0:4),"P2 inherited extra head reaction or P3-6 lost it");need(scene.resolveAttack(sample,hit).processed==0,"Repeated same projectile hit");scene.deactivate();hit.clear();need(scene.resolveAttack(sample,hit).processed==0,"Old scene received late attack");
 }
 std::cout<<"G7 explicit five receiver pages/P2 distinction/duplicate/retirement PASS\n";
}
void parts(const CombatContent& c){
 for(int state:{1,2,3,4,5}){
  auto entry=battle(c);GameSession s(c.items,entry.session,c.facts);s.setFlag(13,true);s.setVariable(105,state);CombatScene scene(c,s,entry,quiet);for(int n=0;n<1200&&s.variable(105);++n){tick(scene);}need(s.variable(105)==0,"Head sequence did not finish state "+std::to_string(state));if(state==5){need(s.flag(23),"Triple head swing did not open assistance gate");}
 }
 for(int variable:{91,161}){
  auto entry=battle(c);GameSession s(c.items,entry.session,c.facts);s.setFlag(13,true);s.setVariable(variable,1);CombatScene scene(c,s,entry,quiet);std::set<int> modes;for(int n=0;n<4000&&s.variable(variable);++n){modes.insert(s.variable(variable==91?214:198));tick(scene);}need(s.variable(variable)==0&&modes.size()>1,"Mouth or ceiling action did not progress independently: "+std::to_string(variable));
 }
 for(const auto pair:{std::pair{119,117},std::pair{133,131},std::pair{147,145}}){
  auto entry=battle(c);GameSession s(c.items,entry.session,c.facts);s.setFlag(13,true);s.setVariable(pair.first,6);s.setVariable(pair.second,23);CombatScene scene(c,s,entry,quiet);tick(scene);need(s.variable(pair.first)==7&&s.variable(pair.second)==1,"Independent if failed same-cycle tentacle6->7 progression");
 }
 std::cout<<"G7 five head actions/mouth/ceiling/three same-cycle tentacles PASS\n";
}
void assistance(const CombatContent& c,const std::filesystem::path& output,bool stocked=false){
 auto entry=battle(c);if(stocked){entry.session.armor=45;entry.session.inventory[45]=99;entry.session.inventory[21]=99;entry.session.inventory[23]=2;entry.session.inventory[49]=3;}GameSession s(c.items,entry.session,c.facts);s.setVariable(89,1000);s.setVariable(384,0);CombatScene scene(c,s,entry,quiet);
 int firstPeak=0,secondPeak=0,scriptShots=0,missileStarts=0,missileFlights=0,missileHits=0;bool previousMissile=false,flightCounted=false;int lastDamage=s.variable(89);bool sawReset=false,waitingSecond=false;int steps=0;
 for(;steps<24000&&!s.flag(576);++steps){
  scene.update(steps%8==0?confirm():InputFrame{});for(const auto& event:scene.takePresentation()){if(event.id=="m209-e10-p8-c29"||event.id=="m209-e10-p8-c43"||event.id=="m209-e10-p8-c58"||event.id=="m209-e10-p8-c73"||event.id=="m209-e10-p8-c88"||event.id=="m209-e10-p8-c103"||event.id=="m209-e10-p8-c121"){++scriptShots;}}
  const bool missile=s.flag(6);if(missile&&!previousMissile){++missileStarts;flightCounted=false;}if(missile&&!flightCounted&&findCharacter(scene.world(),"m209-e1").motion.tile().x<11){++missileFlights;flightCounted=true;}if(missile&&s.variable(89)>lastDamage){++missileHits;}previousMissile=missile;lastDamage=s.variable(89);
  if(!s.flag(574)){firstPeak=std::max(firstPeak,s.variable(384));}else if(s.variable(384)==0){sawReset=true;}if(sawReset){secondPeak=std::max(secondPeak,s.variable(384));waitingSecond|=scene.foreground()==ForegroundKind::Boss&&!s.flag(575)&&s.variable(384)>0;}
  need(s.hp()>0,"Assistance capability fixture died");need(!s.flag(582),"Assistance first1000 won before second battle");
 }
 need(s.flag(574)&&s.flag(575)&&s.flag(576)&&sawReset&&waitingSecond&&secondPeak>=25,"Actual second25 hits did not advance foreground");need(scriptShots==7&&missileStarts==7&&missileFlights==7&&missileHits==7,"Seven real scripted missiles absent: "+std::to_string(missileStarts)+"/"+std::to_string(missileFlights)+"/"+std::to_string(missileHits));need(s.equipped(1)==23&&s.equipped(3)==49&&s.count(23)==(stocked?1:0)&&s.count(49)==(stocked?2:0)&&s.count(21)==(stocked?99:1)&&(!stocked||s.count(45)==99),"Zero-stock plot equipment/old inventory incorrect");need(s.magazine(23)==0&&s.variable(8)==999999&&s.variable(89)==0&&s.hp()==s.maxHp(),"Awakening fabricated magazine/missed restore");
 const auto inventory=s.exportState().inventory;tick(scene,80);need(s.exportState().inventory==inventory,"Assistance empty high page repeated equipment reward");
 std::ofstream(output/(stocked?"assistance-stock-result.json":"assistance-result.json"))<<nlohmann::json{{"kind","controlled-real-Scene-capability"},{"natural_entry",false},{"steps",steps},{"first_peak",firstPeak},{"second_peak",secondPeak},{"script_shots",scriptShots},{"missile_starts",missileStarts},{"missile_flights",missileFlights},{"missile_hits",missileHits},{"hp",s.hp()},{"main",s.equipped(1)},{"armor",s.equipped(3)},{"magazine23",s.magazine(23)}}.dump(2)<<'\n';
 std::cout<<"G7 actual two25-hit rounds/seven missiles/zero-stock equipment PASS steps="<<steps<<'\n';
}
void air(const CombatContent& c){
 for(int gate:{1,2,19,5}){
  auto entry=battle(c);entry.session.mainWeapon=23;GameSession s(c.items,entry.session,c.facts);s.setFlag(13,true);s.setFlag(574,true);s.setFlag(576,true);s.setFlag(2,true);s.setFlag(1,false);s.setFlag(19,false);s.setFlag(5,false);if(gate==2){s.setFlag(2,false);}if(gate==19){s.setFlag(19,true);}if(gate==5){s.setFlag(5,true);}
  CombatScene scene(c,s,entry,quiet);const int before=s.variable(89);for(int n=0;n<180;++n){scene.update(attack());scene.takePresentation();}need((s.variable(89)>before)==(gate==1),"S574 air input bypassed an additional gate "+std::to_string(gate));need(s.magazine(23)==0,"Air acquired phantom magazine");
 }
 {auto e=battle(c);e.session.mainWeapon=23;GameSession s(c.items,e.session,c.facts);s.setFlag(13,true);s.setFlag(576,true);CombatScene scene(c,s,e,[](int low,int){return low;});tick(scene);for(int count:{2,5,20}){s.setVariable(35,count);const auto camera=scene.cameraCenter();std::set<std::string> hit;const auto result=scene.resolveAttack({{s.variable(83)+camera.x-160.0,s.variable(84)+camera.y-120.0},{0,0},AttackKind::Rocket,6},hit);need(result.damage==std::max(1,s.attack()-38-1)&&!s.flag(12)&&s.variable(35)==count+1,"I23 inherited I22 critical rule");}}
 auto entry=battle(c);GameSession strict(c.items,entry.session,c.facts);need(!strict.equip(23)&&!strict.equip(49,3),"Menu equipment accepts zero-stock plot items");
 std::cout<<"G7 no-ammo I23 input/S574 only-S1/menu rejection PASS\n";
}
void lifecycle(const CombatContent& c){
 {auto e=battle(c);e.session.mainWeapon=23;GameSession s(c.items,e.session,c.facts);s.setFlag(13,true);s.setFlag(574,true);s.setFlag(576,true);s.setFlag(2,true);CombatScene scene(c,s,e,quiet);scene.update(attack());scene.update(attack());scene.takePresentation();const auto frame6=scene.world().player().spriteId;need(frame6==c.cue("ce82-c29").at("sprite").get<std::string>(),"Air frame6 source pose missing");s.setFlag(17,false);tick(scene);need(scene.world().player().spriteId==frame6,"S17 interrupt cut source Wait1 early");tick(scene,5);need(scene.world().player().spriteId==c.cue("ce82-c41").at("sprite").get<std::string>()&&!s.flag(17)&&s.magazine(23)==0,"S17 checkpoint did not cancel after frame7 without ammo");}

 {auto e=battle(c);e.session.mainWeapon=23;GameSession s(c.items,e.session,c.facts);s.setFlag(13,true);s.setFlag(574,true);s.setFlag(576,true);s.setFlag(2,true);CombatScene scene(c,s,e,quiet);scene.update(attack());scene.update(attack());scene.takePresentation();need(s.flag(17)&&s.flag(6),"Air cancellation fixture did not begin action");scene.deactivate();const auto before=s.exportState();tick(scene,120);need(s.exportState()==before&&!scene.takeFinalBossVictory(),"Cancelled scene resumed air or committed victory");}
 {auto e=battle(c);GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,quiet);s.loseHp(s.hp());bool title=false;for(int n=0;n<1000&&!title;++n){scene.update(n%8==0?confirm():InputFrame{});scene.takePresentation();if(auto mode=scene.takeApplicationRequest()){need(*mode!=ApplicationMode::Equipment,"Dead final player opened equipment");title=*mode==ApplicationMode::Title;}}need(title&&!s.flag(586)&&!scene.takeFinalBossVictory(),"Final death did not end at title without Continue");}
 std::cout<<"G7 active air cancel/no late submit/final death/title/no Continue PASS\n";
}

void routes(const CombatContent& c){
 auto e=battle(c);GameSession session(c.items,e.session,c.facts);auto definition=c.makeScene(e.map,e.playerTile,e.direction,session,&e);definition.width=9;definition.height=6;definition.passageMasks.assign(54,15);definition.objects.clear();definition.player.tileX=4;definition.player.tileY=3;ExplorationScene world(definition);std::vector<PresentationEvent> events;
 RouteDefinition repeat;repeat.frequency=8;repeat.repeat=true;repeat.steps={{"lock_direction"},{"face_left"},{"wait"}};RoutePlayer route(repeat,"player","first-instance");need(!route.firstPassComplete(),"New repeat already complete");
 for(int n=0;n<21;++n){route.tick(world,session,events,quiet);world.advanceCharacters();need(!route.firstPassComplete(),"Repeat completed before its first wait");}route.tick(world,session,events,quiet);need(route.firstPassComplete()&&world.player().directionLocked,"Repeat first pass did not release wait or retain unique direction lock");
 RoutePlayer replacement(repeat,"player","replacement");need(!replacement.firstPassComplete(),"Replacement inherited completed identity");
 RouteDefinition unlock;unlock.frequency=8;unlock.steps={{"unlock_direction"}};RoutePlayer clear(unlock,"player");clear.tick(world,session,events,quiet);need(!world.player().directionLocked,"Unlock failed to modify unique actor");
 std::cout<<"G7 repeat first-pass/replacement/direction ownership/map variation PASS\n";
}
void fullBattle(const CombatContent& c,const std::filesystem::path& output){
 GameApplication app(c,"final-capability",quiet);DoraRender render(app,c,output/"frames");
 const auto& blankSound=c.media.at("animations").at("65");need(!blankSound.at("timings").at(0).at("sound").is_null()&&blankSound.at("timings").at(0).at("sound").at("resource").is_null(),"Empty animation sound source changed");render.pending={{PresentationEvent::Kind::Animation,"65",{},{},0}};render.tick();for(int n=0;n<int(blankSound.at("frames").size())*blankSound.at("playback").at("logic_updates_per_frame").get<int>();++n){render.tick();}std::cout<<"G7 actual renderer empty-resource animation sound timing PASS\n";

 render.pending=app.takePresentation();render.tick();bool assisted=false,awakened=false,sawIntroTail=false;int initialSteps=0,awakeSteps=0;std::map<std::string,int> cues;std::optional<FinalBossVictory> won;
 for(int n=0;n<16000;++n){
  InputFrame input=attack();input.buttons[int(InputAction::Weapon)]={true,n==0,false};input.buttons[int(InputAction::Confirm)]={false,n%8==0,false};try{app.update(input);}catch(const std::exception& error){throw std::runtime_error("public update step "+std::to_string(n)+": "+error.what());}
  render.pending=app.takePresentation();for(const auto& event:render.pending){++cues[event.id];}try{render.tick();}catch(const std::exception& error){nlohmann::json events=nlohmann::json::array();for(const auto& event:render.pending){events.push_back({{"kind",int(event.kind)},{"id",event.id},{"sound",event.sound.resource}});}throw std::runtime_error("render update step "+std::to_string(n)+" events="+events.dump()+": "+error.what());}
  if(app.foreground()==ForegroundKind::None&&app.session().flag(581)&&!app.session().flag(574)){render.frame("initial-battle");}if(app.session().flag(574)&&!app.session().flag(575)){render.frame("assistance");}if(app.session().flag(575)&&!app.session().flag(576)){render.frame("awakening-visual");}if(app.session().flag(576)){render.frame("awakened-battle");}
  if(n==1000||n==3000||n==8000){std::cout<<"full step="<<n<<" damage="<<app.session().variable(89)<<" body="<<app.session().variable(203)<<" hits="<<app.session().variable(384)<<" flags="<<app.session().flag(581)<<app.session().flag(574)<<app.session().flag(576)<<" foreground="<<int(app.foreground())<<" raised="<<app.raised()<<" ammo="<<app.session().magazine(21)<<" tile="<<app.world().player().motion.tile().x<<","<<app.world().player().motion.tile().y<<" facing="<<int(app.world().player().motion.facing())<<"\n";}
  need(app.mode()!=ApplicationMode::Title&&app.hasGame()&&app.session().hp()>0,"Public full battle died");
  if(app.session().flag(581)&&!app.session().flag(574)&&app.session().flag(1)&&app.session().flag(18)){sawIntroTail=true;}if(app.session().flag(574)){assisted=true;}if(app.session().flag(576)){if(!awakened){initialSteps=n;}awakened=true;++awakeSteps;}
  won=app.takeFinalBossVictory();
  if(won){render.frame("victory");need(won->scene==app.generation()&&won->object=="m209-e10"&&won->page==10&&app.session().flag(582),"Victory has stale identity or uncommitted fact");break;}
 }
 need(assisted&&awakened&&won&&awakeSteps>1,"Public real battle did not reach assistance/awakening/second victory");
 need(sawIntroTail,"E4 foreground lost control tail after S581 commit");
 const int damage=app.session().variable(89),hp=app.session().hp();for(int n=0;n<50;++n){app.update(attack());app.takePresentation();}need(!app.takeFinalBossVictory()&&app.session().variable(89)==damage&&app.session().hp()==hp,"Victory repeated or old combat continued");
 std::ofstream(output/"full-battle-result.json")<<nlohmann::json{{"kind","public-GameApplication-final-capability"},{"natural_cockpit",false},{"initial_steps",initialSteps},{"awake_steps",awakeSteps},{"damage",damage},{"body",app.session().variable(203)},{"hp",hp},{"air_magazine",app.session().magazine(23)},{"scene",won->scene},{"page",won->page},{"cues",cues}}.dump(2)<<'\n';
 std::cout<<"G7 public full battle initial/assistance/awakening/second victory PASS steps="<<initialSteps+awakeSteps<<'\n';
}

void saveCompatibility(const CombatContent& c,const std::filesystem::path& oldPath){
 std::ifstream file(oldPath);const std::string raw{std::istreambuf_iterator<char>(file),{}};need(!raw.empty(),"Missing isolated old save");const auto old=decodeSave(*c.legacySource,raw),next=decodeSave(c,raw);need(next.scene==old.scene&&next.session.hp==old.session.hp&&next.session.inventory==old.session.inventory&&next.session.saveCount==old.session.saveCount&&next.session.conditions==old.session.conditions&&next.warehouse==old.warehouse&&next.presentation==old.presentation,"G7 source upgrade changed existing save state");
 for(int flag:{574,575,576,580,581,582}){need(!next.session.worldFlags.at(c.facts.switches.at(flag)),"Old save invented a final fact");}need(next.session.worldValues.at("final.assistance-hits")==0,"Old save invented hits");
 auto restored=prepareRestore(c,next,{},quiet);need(restored.scene->mapId()==old.scene.map&&restored.session->saveCount()==old.session.saveCount&&restored.session->variable(399)==old.continuation.statisticBefore+(old.continuation.kind=="direct_unscored"?0:1),"G7 restore changed original continuation");need(decodeSave(c,encodeSave(c,next))==next,"G7 legal save roundtrip differs");
 GameApplication live(c,"post-pickup",quiet);const auto before=live.session().exportState();int rejected=0;for(int which=0;which<8;++which){auto bad=next;if(which<6){bad.session.worldFlags[c.facts.switches.at(std::array{574,575,576,580,581,582}[which])]=true;}else if(which==6){bad.contentCompatibility="unknown";}else{bad.session.worldValues["final.assistance-hits"]=-1;}std::string error;if(!live.restore(bad,error)){++rejected;}need(live.session().exportState()==before,"Rejected final snapshot replaced active game");}need(rejected==8,"Unsupported final save or source accepted");
 auto e=battle(c);GameSession session(c.items,e.session,c.facts);CombatScene scene(c,session,e,quiet);bool blocked=false;try{captureSave(c,scene,{scene.generation(),"m209-e10","final"},{});}catch(const std::exception&){blocked=true;}need(blocked,"M209 invented ordinary save point");
 std::cout<<"G7 strict old-source/roundtrip/restore/8 candidate rejections/M209 save rejection PASS\n";
}

}
int main(int argc,char** argv){try{
 std::cout.setf(std::ios::unitbuf);need(argc==3||argc==4,"Expected cumulative G7 package, evidence directory, optional isolated old save");const auto c=loadCombatContent(argv[1]);std::filesystem::path output=argv[2];std::filesystem::create_directories(output);thresholds(c);receivers(c);parts(c);assistance(c,output);assistance(c,output,true);air(c);lifecycle(c);routes(c);fullBattle(c,output);if(argc==4){saveCompatibility(c,argv[3]);}return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
