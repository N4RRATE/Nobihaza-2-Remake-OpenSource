#include "game_application.h"
#include "full_content.h"
#include <cmath>
#include <queue>
#include <map>
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace{
void need(bool value,const std::string& text){if(!value){throw std::runtime_error(text);}}
InputFrame key(InputAction a=InputAction::Confirm){InputFrame f;f.buttons[static_cast<int>(a)].pressed=true;f.buttons[static_cast<int>(a)].held=true;return f;}
CombatEntry entry(const CombatContent& c,int map=109,TilePosition tile={9,8}){auto e=c.entries.at("post-pickup");e.map=map;e.playerTile=tile;e.direction=Direction::Up;e.sprite=c.maps.at(map).player.spriteId;e.pages.clear();e.positions.clear();e.through.clear();e.directions.clear();initializeFullSession(c,e.session);e.session.flags[1]=e.session.flags[18]=true;return e;}
void tick(CombatScene& s,int n){for(int i=0;i<n;++i){s.update({});s.takePresentation();}}
GridPosition world(const CombatScene& scene,GridPosition screen){auto c=scene.cameraCenter();return {screen.x+c.x-160,screen.y+c.y-120};}
AttackResolution attack(CombatScene& scene,GridPosition point,int page=2,AttackKind kind=AttackKind::Handgun,std::array<int,2> half={0,0},AttackPhase phase=AttackPhase::Impact){std::set<std::string> hits;return scene.resolveAttack({world(scene,point),half,kind,page,0,false,1,phase},hits);}
CombatScene::Random mid=[](int lo,int hi){return lo+(hi-lo)/2;};
void settle(CombatScene& s){for(int i=0;i<60000;++i){s.update(key());s.takePresentation();if(s.foreground()==ForegroundKind::None&&!s.dialogue().active()&&!s.world().player().motion.moving()){return;}}throw std::runtime_error("Content did not settle");}
using Cell=std::pair<int,int>;
bool path(const CombatScene& s,TilePosition target,std::vector<Direction>& result){
 const auto origin=s.world().player().motion.tile();std::queue<Cell> q;std::map<Cell,std::pair<Cell,Direction>> previous;q.push({origin.x,origin.y});previous[{origin.x,origin.y}]={{origin.x,origin.y},Direction::Up};
 while(!q.empty()){auto at=q.front();q.pop();if(at==Cell{target.x,target.y}){for(auto x=at;x!=Cell{origin.x,origin.y};x=previous.at(x).first){result.push_back(previous.at(x).second);}std::reverse(result.begin(),result.end());return true;}
  for(auto d:{Direction::Up,Direction::Right,Direction::Down,Direction::Left}){auto p=s.world().player();p.motion=CharacterMotion({at.first,at.second},d,p.motion.speed());const auto next=adjacentTile(p.motion.tile(),d);Cell cell{next.x,next.y};if(!previous.contains(cell)&&s.world().canMove(p,d)){previous[cell]={at,d};q.push(cell);}}
 }return false;
}
void walk(CombatScene& s,TilePosition tile){std::vector<Direction> route;need(path(s,tile,route),"No source terrain path map "+std::to_string(s.mapId())+" to "+std::to_string(tile.x)+","+std::to_string(tile.y));const int map=s.mapId();for(auto d:route){InputFrame f;f.movement=d;s.update(f);for(int n=0;n<100&&s.world().player().motion.moving();++n){s.update({});}if(s.foreground()!=ForegroundKind::None){settle(s);}if(s.mapId()!=map){return;}}need(s.world().player().motion.tile()==tile,"Movement reaches destination");}
void action(const CombatContent& c,CombatScene& s,const std::string& object){auto tile=c.objects.at(object).tile;for(auto d:{Direction::Down,Direction::Left,Direction::Right,Direction::Up}){std::vector<Direction> route;auto from=adjacentTile(tile,d);if(path(s,from,route)){walk(s,from);InputFrame face;face.movement=d==Direction::Down?Direction::Up:d==Direction::Up?Direction::Down:d==Direction::Left?Direction::Right:Direction::Left;face.buttons[static_cast<int>(InputAction::Weapon)].pressed=false;// Facing through a blocked object uses the real movement request.
 s.update(face);while(s.world().player().motion.moving()){s.update({});}s.update(key());settle(s);return;}}throw std::runtime_error("No interaction path");}
}
int main(int argc,char** argv){try{
need(argc==2,"usage crocodile package");auto c=loadCombatContent(argv[1]);std::cout<<"Reviewed candidate loaded\n"<<std::flush;for(int m:{108,109,110}){need(!c.pendingMapTickets.contains(m),"Natural boss maps delivered");}need(!c.pendingMapTickets.contains(113),"103 delivers post-boss route");need(c.pendingMapTickets.contains(117)!=c.catalog.contains("guard_second"),"Guard map gate matches installed cumulative capability");
{
 auto e=entry(c,111,{21,25});e.direction=Direction::Down;e.session.flags[321]=true;GameSession state(c.items,e.session,c.facts);
 for(const auto& [id,enemy]:c.enemies){if(enemy.map==111){if(enemy.rules.contains("activation_fact")&&!enemy.rules.at("activation_fact").is_null()){state.setFlag(enemy.rules.at("activation_fact"),true);}state.setFlag(enemy.deadFlag,true);}}
 CombatScene scene(c,state,e,mid);action(c,scene,"m111-e5");need(scene.mapId()==108&&!state.flag(323),"Actual drained water exit reaches pre-boss map");
 action(c,scene,"m108-e2");need(state.flag(322)&&scene.mapId()==108&&!state.flag(323),"Locked vent investigation commits322 before boss initialization");
 walk(scene,{28,10});walk(scene,{27,10});settle(scene);need(scene.mapId()==109&&state.flag(323)&&state.variable(89)==0,"Real contact starts full NPC intro and enters boss without victory");
 need(scene.fileReadingLocked()&&!state.flag(13)&&state.flag(18),"Intro locks only file actions and restores combat");
}
std::cout<<"Natural111 to108 vent/NPC/full intro to109 passed\n"<<std::flush;
{
 need(c.catalog.at("flows").at("m107-e7-p1").at("steps").at(0).at("kind")=="enable_combine_if_known","102 preserves 101 normalized environment");
 need(!c.catalog.at("flows").at("m107-e3-p2").at("runtime_enabled").get<bool>(),"102 preserves disabled raw legal-save flow");
 auto crow=c.objects.at("m107-e3").tile;auto e=entry(c,107,adjacentTile(crow,Direction::Left));e.direction=Direction::Right;e.session.flags[321]=true;c.entries["crow-wrong-facing"]=e;GameApplication app(c,"crow-wrong-facing",mid);
 for(int i=0;i<30;++i){app.update({});app.takePresentation();}app.update(key());need(app.mode()==ApplicationMode::Playing&&!app.takeSaveRequest(),"Actual loaded catalog starts water environment and rejects crow wrong facing");
}


{
 auto e=entry(c);GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e,mid);need(state.flag(323)&&scene.mapId()==109&&state.variable(89)==0,"S323 initializes and never completes battle");
 need(state.variable(87)==18&&state.variable(88)==92&&state.variable(101)==20&&state.variable(102)==25&&state.variable(115)==25&&state.variable(116)==25,"Source three receiver half extents");
 const int expected=state.attack()-15;need(attack(scene,{159,-90}).damage==std::max(1,expected),"Weakpoint CE11 reads effective attack and NORMAL defense15");
 state.setFlag(47,true);const auto crit=attack(scene,{159,-90});need(crit.damage==std::max(1,expected)*2,"Weakpoint critical multiplies damage2");state.setFlag(47,true);need(attack(scene,{204,-100}).damage==std::max(1,expected)/2,"Side divides2 even critical");
 auto before=state.variable(89);need(attack(scene,{204,-100},4,AttackKind::Shotgun).damage==0&&state.variable(89)==before,"Empty side P4 acknowledges without invented damage");
 need(attack(scene,{159+18+0.1,-90}).contacts==0,"Weakpoint outside x18 misses");need(attack(scene,{159+18,-90}).contacts==1,"Weakpoint x18 includes boundary");
 
 std::set<std::string> resolved;auto sample=AttackSample{world(scene,{159,-90}),{0,0},AttackKind::Handgun,2};auto one=scene.resolveAttack(sample,resolved);need(one.processed==1&&scene.resolveAttack(sample,resolved).processed==0,"Same attack cannot hit same slot twice");
 scene.deactivate();before=state.variable(89);tick(scene,100);need(state.variable(89)==before&&attack(scene,{159,-90}).contacts==0,"Retired scene ignores delayed damage");
}
{
 auto e=entry(c);GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e,mid);state.setVariable(98,40);state.setFlag(13,true);
 auto burn=attack(scene,{204,40},2,AttackKind::GlFlame,{0,0},AttackPhase::Burning);need(burn.processed==1&&state.flag(38)&&state.variable(89)==0,"Actual side burn activates CE25 only");
 state.setVariable(84,50);state.setVariable(98,40);tick(scene,7);need(state.variable(89)==0&&state.variable(103)>=4,"Complete CE25 cycle adds4 reaction without HP damage");
}

std::cout<<"Initialization, exact slots, critical/side empty source pages, attack identity and retirement passed\n"<<std::flush;
{
 auto e=entry(c);GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e,mid);state.setFlag(13,true);state.setVariable(84,40);state.setVariable(91,1);tick(scene,10);need(state.variable(91)==10&&state.variable(117)==45,"Open10 iterations sets retreat45");tick(scene,45);need(state.variable(91)==0&&!state.flag(21),"Retreat closes mouth and returns idle");
 state.setVariable(84,-90);state.setVariable(103,20);tick(scene,13);need(state.variable(91)==2||state.variable(91)==3,"Reaction20 chooses shake/charge");tick(scene,40);need(state.variable(91)==3||state.variable(91)==10,"Shake35 reaches charge");
}
{
 auto e=entry(c);GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e,mid);const auto p=scene.sampleActorScreen("player");state.setVariable(97,int(p.x));state.setVariable(84,int(p.y)-6);state.setVariable(194,16);scene.update({});scene.update({});need(scene.foreground()==ForegroundKind::Death&&state.hp()>0&&!state.flag(18)&&!state.flag(17),"Idle left peak16+1 causes real immediate death independent of HP");for(int i=0;i<6000;++i){scene.update({});if(scene.takeApplicationRequest()==ApplicationMode::Title){break;}}need(scene.foreground()==ForegroundKind::None,"Crush completes actual title failure");
}
{
 auto e=entry(c);GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e,mid);const auto p=scene.sampleActorScreen("player");state.setVariable(111,int(p.x));state.setVariable(84,int(p.y)-6);state.setVariable(195,16);state.setVariable(119,2);scene.update({});scene.update({});need(scene.foreground()!=ForegroundKind::Death&&state.hp()==state.maxHp(),"Right peak shakes without symmetric crush");
}
{
 auto e=entry(c);e.session.flags[13]=false;e.session.armor=e.session.accessory=e.session.otherEquipment=0;e.session.baseDefense=0;e.session.hp=e.session.maxHp=100;GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e,[](int lo,int){return lo;});const auto player=scene.sampleActorScreen("player");state.setVariable(84,int(player.y)-40);state.setVariable(91,1);tick(scene,11);
 need(state.hp()==75&&!state.flag(17)&&!state.flag(18),"Bite reads actual defense, commits CE12 and starts source displacement");for(int i=0;i<1000&&!state.flag(18);++i){tick(scene,1);}need(state.flag(18)&&!state.flag(17)&&scene.foreground()==ForegroundKind::None,"Living bite route restores battle control hp"+std::to_string(state.hp())+" mode"+std::to_string(state.variable(91))+" y"+std::to_string(state.variable(84)));
}
{
 auto e=entry(c);e.session.flags[13]=false;e.session.armor=e.session.accessory=e.session.otherEquipment=0;e.session.baseDefense=0;e.session.hp=1;GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e,[](int lo,int){return lo;});const auto player=scene.sampleActorScreen("player");state.setVariable(84,int(player.y)-40);state.setVariable(91,1);tick(scene,11);need(state.hp()==0&&scene.foreground()==ForegroundKind::Death,"Lethal bite enters existing real death flow");
}
for(int weapon:{11,12,14,16,17,18,19,20,21}){
 auto e=entry(c);e.session.inventory[weapon]=1;e.session.mainWeapon=weapon;e.session.magazines[weapon]=weapon==19?30:1;GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e,mid);state.setFlag(13,true);
 scene.update(key(InputAction::Weapon));tick(scene,150);need(scene.raised(),"Actual weapon posture completes item"+std::to_string(weapon));state.setVariable(84,80);state.setVariable(91,10);state.setVariable(117,1000);
 const int prior=state.magazine(weapon);scene.update(key(InputAction::Attack));tick(scene,150);need(state.magazine(weapon)<prior&&state.variable(89)>0,"Actual source weapon commits and reaches crocodile item"+std::to_string(weapon));
 const int spent=state.magazine(weapon);scene.deactivate();tick(scene,100);need(state.magazine(weapon)==spent,"Retiring weapon cannot commit a late second shot");
}
for(int weapon:{24,25,27,29}){
 auto e=entry(c);e.session.inventory[weapon]=2;e.session.auxiliaryWeapon=weapon;if(weapon==27){e.session.magazines[27]=1;}GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e,mid);state.setFlag(13,true);
 scene.update(key(InputAction::AuxiliaryWeapon));tick(scene,150);need(scene.raised(),"Actual auxiliary posture completes item"+std::to_string(weapon));state.setVariable(84,80);state.setVariable(91,10);state.setVariable(117,1000);scene.update(key(InputAction::Attack));tick(scene,300);
 need(state.variable(89)>0,"Actual auxiliary strike reaches crocodile item"+std::to_string(weapon));need(state.count(weapon)==(weapon==29?1:2),"Auxiliary commit preserves knife or consumes one grenade");
}
for(bool shoe:{false,true}){
 auto e=entry(c);GameSession state(c.items,e.session,c.facts);if(shoe){state.grant(53,1);need(state.equip(53,4),"I53 uses actual head equipment");}const int agility=state.auxiliaryAttack();CombatScene scene(c,state,e,mid);InputFrame input;input.movement=Direction::Left;scene.update(input);need(scene.world().player().motion.speed()==(shoe?32:16)&&state.auxiliaryAttack()==agility,"Boss special movement applies fixed speed3/4 without agility bonus");
}

std::cout<<"Body FSM, left immediate death/title and right harmless peak passed\n"<<std::flush;
{
 auto e=entry(c);GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e,[](int lo,int){return lo;});state.setFlag(1,false);tick(scene,400);need(!state.flag(29),"Disabled battle does not spawn gas");state.setFlag(1,true);state.setFlag(13,true);tick(scene,359);need(!state.flag(29),"Minimum gas wait remains until6 seconds");tick(scene,1);need(state.flag(29),"Gas spawns at6 seconds");auto gas=scene.sampleActorScreen("m109-e6");need(scene.world().player().motion.tile()==e.playerTile,"Gas placement does not move player");need(std::abs(gas.x-104)<.1,"Gas source lower x6");
 state.setVariable(84,int(gas.y)-100);state.setVariable(91,10);state.setVariable(117,1000);const int hp=state.hp();auto hit=attack(scene,{gas.x,gas.y+20});need(hit.processed>=1&&state.variable(147)==10,"Weapon breaks gas at source contact center");tick(scene,2);const int before=state.variable(89);tick(scene,1);need(state.variable(89)>before&&state.variable(89)-before==80,"Frame2 gas blast100-5-15 affects weakpoint only");need(state.hp()==hp,"Gas does not call player CE12");tick(scene,15);need(!state.flag(29),"Gas clears and starts next generation");
}
{
 auto e=entry(c);GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e,[](int,int hi){return hi;});state.setFlag(13,true);tick(scene,539);need(!state.flag(29),"Maximum gas wait remains until9 seconds");tick(scene,1);need(state.flag(29),"Gas maximum wait9 seconds");const auto gas=scene.sampleActorScreen("m109-e6");need(std::abs(gas.x-216)<.1,"Gas source upper x13");state.setVariable(84,int(gas.y)-100);state.setVariable(91,10);state.setVariable(117,1000);attack(scene,{gas.x,gas.y+20});state.setFlag(12,true);tick(scene,2);const int before=state.variable(89);tick(scene,1);need(state.variable(89)-before==200,"Source M109 E9P7 C18 gas critical multiplies2 independently of side divisor");
}
std::cout<<"Gas6 seconds, contact/blast source frame, enemy-only damage and regeneration passed\n"<<std::flush;
{
 auto e=entry(c);GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e,mid);state.setFlag(13,true);state.setVariable(89,999);scene.update({});need(scene.mapId()==109,"999 is not victory");state.setVariable(89,1000);scene.update({});need(scene.foreground()==ForegroundKind::Content,"1000 owns victory foreground");settle(scene);need(scene.mapId()==110&&scene.world().player().motion.tile()==TilePosition{9,26},"Actual victory relocates only to110(9,26)");need(!scene.fileReadingLocked()&&state.flag(323),"Victory unlocks file actions and preserves initialization");need(state.variable(89)==0,"CE55 resets local damage after actual relocate");
}
{
 auto e=entry(c);c.entries["crocodile-test"]=e;GameApplication app(c,"crocodile-test",mid);app.update(key(InputAction::Cancel));need(app.mode()==ApplicationMode::Equipment,"Real menu opens in boss");auto before=app.session().variable(84);for(int i=0;i<900;++i){app.update({});}need(app.session().variable(84)==before,"Menu pauses body and gas simulation");app.update(key(InputAction::Cancel));for(int i=0;i<15;++i){app.update({});}need(app.mode()==ApplicationMode::Playing&&app.session().variable(84)!=before,"Menu exit resumes actual body");
}
{
 auto e=entry(c);e.session.baseAttack=100;GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e,mid);
 for(int i=0;i<20&&state.variable(89)<1000;++i){attack(scene,{159,-90});}need(state.variable(89)>=1000,"Public receiver damage reaches source victory threshold for a changed attack attribute");scene.update({});settle(scene);need(scene.mapId()==110&&scene.world().player().motion.tile()==TilePosition{9,26},"Actual accumulated attack damage completes victory transfer");
}
std::cout<<"Actual victory to110 and Application menu pause/resume passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
