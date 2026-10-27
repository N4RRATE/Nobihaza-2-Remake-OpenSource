#include "g0_natural_driver.h"
#include "game_application.h"
#include "enemy_host.h"
#include "scene_presentation.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <queue>
#include <stdexcept>
namespace{
void need(bool ok,const std::string& why){if(!ok){throw std::runtime_error(why);}}
InputFrame key(InputAction a=InputAction::Confirm){InputFrame f;f.buttons[static_cast<int>(a)]={true,true,false};return f;}
std::string read(const char* p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int safe(int lo,int hi){if(lo==0&&hi==10){return 5;}if(lo==0&&hi==15){return 9;}return hi;}
CombatEntry battle(const CombatContent& c){auto e=c.entries.at("post-pickup");e.map=120;e.playerTile={33,8};e.direction=Direction::Left;e.pages.clear();e.positions={{"m120-e13",{27,7}}};e.directions.clear();e.through.clear();e.session.flags[346]=e.session.flags[347]=e.session.flags[348]=e.session.flags[349]=true;e.session.flags[350]=false;e.session.flags[1]=e.session.flags[18]=true;e.session.flags[21]=false;e.session.variables[91]=e.session.variables[89]=0;e.session.variables[280]=2;e.session.hp=e.session.maxHp=100;return e;}
std::optional<Direction> next(const ExplorationScene& w,TilePosition goal){
 auto start=w.player().motion.tile();std::queue<TilePosition> q;q.push(start);std::map<std::pair<int,int>,std::optional<Direction>> first;first[{start.x,start.y}]={};
 while(!q.empty()){auto at=q.front();q.pop();if(at==goal){return first.at({at.x,at.y});}for(auto d:{Direction::Up,Direction::Right,Direction::Down,Direction::Left}){auto p=w.player();p.motion=CharacterMotion(at,d,p.motion.speed());auto t=adjacentTile(at,d);auto cell=std::pair{t.x,t.y};if(first.contains(cell)||!w.canMove(p,d)){continue;}first[cell]=first.at({at.x,at.y}).value_or(d);q.push(t);}}return {};
}
void tick(CombatScene& scene,int n=1){for(int k=0;k<n;++k){scene.update(k%8==0?key():InputFrame{});scene.takePresentation();}}
}
int main(int argc,char** argv){try{
 std::cout.setf(std::ios::unitbuf);need(argc==3,"g0_victory_checks package natural03-save");auto c=loadCombatContent(argv[1]);const auto& victory=c.catalog.at("flows").at("g0-hangar-victory");need(victory.at("runtime_enabled")&&victory.at("next_flow")=="m122-e8-p4","Natural E13P18 victory is still pending");
 need(c.enemies.at("m120-e13").story,"Victory hides its source corpse event");auto e=battle(c);
 nlohmann::json conversions=nlohmann::json::array();for(const auto& s:victory.at("steps")){if(s.at("kind")=="branch"){conversions.push_back(s);}}need(conversions.size()==10,"CE167 inverse pair count differs");
 for(int source:{0,1,2}){for(int held:{0,1,99}){
  auto f=c;f.catalog["flows"]=nlohmann::json{{"inverse",{{"object","m122-e16"},{"page",1},{"trigger","autorun"},{"steps",conversions}}}};auto a=e;a.map=122;a.playerTile={11,7};a.positions.clear();a.session.flags[350]=false;
  for(const auto& pair:c.catalog.at("g0").at("reading_pairs")){a.session.inventory[pair[1]]=source;a.session.inventory[pair[0]]=held;}
  GameSession s(f.items,a.session,f.facts);CombatScene scene(f,s,a);scene.update({});for(const auto& pair:c.catalog.at("g0").at("reading_pairs")){int original=pair[0],disabled=pair[1];need(s.count(disabled)==std::max(0,source-1)&&s.count(original)==std::min(99,held+(source>0)),"CE167 replaced more than one item or ignored cap");if(s.count(original)){need(s.canSelectItem(original),"Restored file lost field permission");}need(!s.canSelectItem(disabled),"Disabled remaining ID gained field permission");}
  scene.deactivate();auto inv=s.exportState().inventory;tick(scene,30);need(s.exportState().inventory==inv,"Retired inverse mutated inventory");
 }}
 {auto f=c;f.catalog["flows"]=nlohmann::json{{"inverse",{{"object","m22-e2"},{"page",1},{"trigger","autorun"},{"steps",nlohmann::json::array({conversions[1]})}}}};auto a=e;a.map=22;a.playerTile={10,5};a.positions.clear();a.session.inventory[197]=2;a.session.inventory[182]=1;GameSession s(f.items,a.session,f.facts);f.catalog["flows"]["inverse"]["page"]=f.activeVariant(f.objects.at("m22-e2"),s)->page;CombatScene scene(f,s,a);scene.update({});need(s.count(197)==1&&s.count(182)==2,"Inverse assumes hangar geometry");}
 std::cout<<"Ten CE167 pairs source0/1/2, destination0/1/99, field permission, retirement and alternate map passed\n";
 // Full source victory runs once, including same-map rebuild and post-transfer CE55.
 {auto a=e;a.session.flags[355]=a.session.flags[501]=a.session.flags[357]=true;a.session.inventory[195]=2;a.session.inventory[180]=1;a.session.variables[330]=5;a.session.variables[372]=9;GameSession s(c.items,a.session,c.facts);CombatScene scene(c,s,a,safe);const auto original=s.exportState();scene.update({});scene.takePresentation();s.setVariable(89,800);int reloads=0;bool corpse=false,anchor=false,pending=false;std::map<std::string,int> cues;
  for(int n=0;n<5000&&!pending;++n){scene.update(n%8==0?key():InputFrame{});for(const auto& v:scene.takePresentation()){reloads+=v.kind==PresentationEvent::Kind::SceneChanged;pending|=v.kind==PresentationEvent::Kind::Notice&&v.id.find("任务 7 ")!=std::string::npos; if(v.kind==PresentationEvent::Kind::Sound&&v.sound.resource==readSound(c.cue("m120-e13-p18-c4").at("sound")).resource){++cues["m120-e13-p18-c4"];}if(v.kind==PresentationEvent::Kind::Cue){++cues[v.id];if(v.id=="m120-e13-p18-c29"){anchor=v.sampledScreenPosition.has_value();const auto tile=findCharacter(scene.world(),"m120-e13").motion.tile();corpse=tile==TilePosition{27,6};}}}}
  need(pending&&scene.mapId()==122&&scene.world().player().motion.tile()==TilePosition{9,21}&&scene.foreground()==ForegroundKind::Content,"Victory did not reach occupied M122");need(reloads==2&&corpse&&anchor&&cues["m120-e13-p18-c4"]==1&&cues["m120-e13-p18-c40"]==1,"Victory omitted/duplicated reload, body or picture sequence: reloads="+std::to_string(reloads)+" corpse="+std::to_string(corpse)+" anchor="+std::to_string(anchor)+" c4="+std::to_string(cues["m120-e13-p18-c4"])+" c40="+std::to_string(cues["m120-e13-p18-c40"]));need(s.flag(349)&&s.flag(350)&&s.flag(357)&&s.flag(355)&&s.flag(501),"CE55 cleared permanent/supply facts");need(!s.flag(1)&&!s.flag(18)&&s.variable(89)==0&&s.variable(330)==0&&s.variable(372)==0,"Post-transfer CE55 did not clear source local work");need(s.count(195)==1&&s.count(180)==2,"Natural CE167 not applied once");need(s.exportState().mainWeapon==original.mainWeapon&&s.exportState().auxiliaryWeapon==original.auxiliaryWeapon&&s.exportState().magazines==original.magazines&&s.count(33)==(original.inventory.contains(33)?original.inventory.at(33):0),"Victory changed equipment/ammunition");auto at=scene.world().player().motion.position();auto inv=s.exportState().inventory;for(int n=0;n<90;++n){InputFrame in=key(InputAction::Cancel);in.movement=Direction::Up;scene.update(in);}need(scene.world().player().motion.position().x==at.x&&scene.world().player().motion.position().y==at.y&&!scene.takeApplicationRequest()&&!scene.takeSaveRequest()&&s.exportState().inventory==inv,"M122 released false menu/save/pickup window");scene.deactivate();tick(scene,30);
  auto visit=e;visit.session=s.exportState();GameSession revisit(c.items,visit.session,c.facts);CombatScene room(c,revisit,visit,safe);tick(room,30);need(c.activeVariant(c.objects.at("m120-e13"),revisit)->page==19,"Revisit did not select victory corpse page");need(revisit.variable(89)==0&&revisit.count(180)==2,"Revisit restarted battle/reward");
 }
 std::cout<<"Full79C victory, two transfers, corpse anchor, CE55, persistent facts and occupied M122 passed\n";
 // Admission rejects missing destination resources before any persistent completion.
 {GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,safe);scene.configurePresentation([](const SceneDefinition& d){if(d.width==20&&d.height==23){throw std::runtime_error("destination resources missing");}});bool rejected=false;try{ScenePresentation(scene).validateSequence(victory.at("steps"),"m120-e13");}catch(const std::exception&){rejected=true;}need(rejected&&!s.flag(350),"Missing destination committed completion");}
 // A late resource failure retains the current relocation command; inverse is already committed once.
 {GameSession s(c.items,e.session,c.facts);s.grant(195,2);CombatScene scene(c,s,e,safe);bool reject=false;scene.configurePresentation([&](const SceneDefinition& d){if(reject&&d.width==20&&d.height==23){throw std::runtime_error("late target failure");}});scene.update({});s.setVariable(89,800);bool failed=false;
  for(int n=0;n<5000&&!failed;++n){if(s.flag(350)){reject=true;}try{scene.update(n%8==0?key():InputFrame{});}catch(const std::exception&){failed=true;}scene.takePresentation();}
  need(failed&&scene.mapId()==120&&s.count(195)==1&&s.count(180)==2,"Late transfer failed before inverse/changed map");reject=false;tick(scene,90);need(scene.mapId()==122&&s.count(195)==1&&s.count(180)==2&&s.variable(89)==0&&scene.foreground()==ForegroundKind::Content,"Transfer retry skipped relocation or duplicated inverse");
 }
 {GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,safe);scene.update({});s.setVariable(89,800);for(int n=0;n<50&&!s.flag(350);++n){scene.update({});scene.takePresentation();}need(s.flag(350),"Cancellation did not reach actual commit");scene.deactivate();auto snapshot=s.exportState();tick(scene,1000);need(s.exportState()==snapshot&&scene.takePresentation().empty(),"Retired victory committed later commands");}
 {GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,safe);scene.update({});s.setVariable(89,800);s.loseHp(s.hp());scene.update({});need(!s.flag(350)&&scene.foreground()==ForegroundKind::Death,"Dead actor submitted completion");}
 std::cout<<"Destination admission, late transfer retry, cancellation and death passed\n";
 // Shared public-input driver also feeds the07 consumer from this real victory.
 auto natural=g0natural::reachVictory(c,decodeSave(c,read(argv[2])));
 need(natural.scene->mapId()==122&&natural.scene->foreground()==ForegroundKind::Content,"Natural victory handoff differs");
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
