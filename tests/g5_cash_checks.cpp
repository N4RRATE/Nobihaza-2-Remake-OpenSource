#include "g2_natural_driver.h"
#include "content_hash.h"
#include "enemy_host.h"
#include "g4_dora_render.h"
#include <fstream>
namespace{
std::string read(const std::filesystem::path& p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
CombatEntry entry(const CombatContent& c,const SessionEntry& s,TilePosition tile={14,20}){auto e=c.entries.at("post-pickup");e.map=191;e.playerTile=tile;e.positions.clear();e.pages.clear();e.directions.clear();e.through.clear();e.session=s;initializeFullSession(c,e.session);e.session.flags[1]=true;e.session.flags[18]=true;e.session.flags[507]=false;e.session.worldFlags["tower.dora-defeated"]=true;return e;}
void rules(const CombatContent& c,const SessionEntry& saved){
 need(c.catalog.at("cash").at("actor_attributes").at("37")==Json{{"6",17},{"7",34},{"8",32}},"NORMAL Cash attributes");
 need(c.catalog.at("continue").at("map")==219&&continueCheckpoint(c,220).at("retry").at("map")==191,"Registered220 displaced219");
 for(int edge:{0,1,2,3}){
  auto e=entry(c,saved);GameSession s(c.items,e.session,c.facts);s.setFlag(13,true);CombatScene scene(c,s,e,safe);scene.update({});scene.takePresentation();need(s.variable(90)==2&&s.variable(87)==8&&s.variable(88)==5,"Source E4 cache registration/8x5 missing");
  s.setFlag(21,true);s.setVariable(91,11);const auto camera=scene.cameraCenter();AttackSample a{{s.variable(83)+camera.x-160+(edge==0?8:edge==1?9:0),s.variable(84)+camera.y-120+(edge==2?5:edge==3?6:0)},{0,0},AttackKind::Handgun,2};std::set<std::string> hit;const auto r=scene.resolveAttack(a,hit);need((r.contacts>0)==(edge==0||edge==2),"Exact cache half-width/height boundary");
  const int x=s.variable(83),y=s.variable(84);s.setFlag(15,true);scene.update({});scene.takePresentation();need(s.variable(83)==x&&s.variable(84)==y&&s.variable(90)==2,"Death empty sampler cleared cached body");
 }
 for(auto kind:{AttackKind::Handgun,AttackKind::Knife,AttackKind::Magnum,AttackKind::De,AttackKind::Shotgun,AttackKind::Grenade,AttackKind::GlNormal,AttackKind::GlFlame,AttackKind::GlAcid,AttackKind::Rocket,AttackKind::Sg,AttackKind::Lc9}){
  auto e=entry(c,saved);GameSession s(c.items,e.session,c.facts);s.setFlag(13,true);s.setFlag(21,true);s.setVariable(91,11);CombatScene scene(c,s,e,safe);scene.update({});scene.takePresentation();s.setVariable(90,2);s.setVariable(87,8);s.setVariable(88,5);const auto p=findCharacter(scene.world(),"m191-e2").motion.position();s.setVariable(83,(p.x+.5)*16-scene.cameraCenter().x+160);s.setVariable(84,(p.y+1)*16-scene.cameraCenter().y+120-8);AttackSample a{{(p.x+.5)*16,(p.y+1)*16-8},{0,0},kind,kind==AttackKind::Grenade?6:kind==AttackKind::Magnum||kind==AttackKind::De?5:2};std::set<std::string> hit;const auto r=scene.resolveAttack(a,hit);need(r.processed==1&&r.damage>0,"Multi-weapon source receiver");if(kind==AttackKind::Magnum||kind==AttackKind::Sg||kind==AttackKind::Lc9){need(r.endRequested,"CE4/CE11 weapon stop missing");}if(kind==AttackKind::GlFlame){need(s.flag(37),"I17 failed to ignite");hit.clear();need(scene.resolveAttack(a,hit).processed==0,"Already burning I17 called receiver again");}if(kind==AttackKind::Rocket){need(s.flag(37),"I22 restart missing");s.setVariable(301,19);hit.clear();scene.resolveAttack(a,hit);need(s.variable(301)==19,"I22 restart cleared301");}
 }
 {auto e=entry(c,saved);GameSession s(c.items,e.session,c.facts);s.setFlag(13,true);s.setFlag(21,true);s.setVariable(91,8);CombatScene scene(c,s,e,safe);scene.update({});scene.takePresentation();const auto p=findCharacter(scene.world(),"m191-e2").motion.position();std::set<std::string> hit;
  const auto r=scene.resolveAttack({{(p.x+.5)*16,(p.y+1)*16-8},{0,0},AttackKind::GlFlame,1,0,false,1,AttackPhase::Flight},hit);need(r.processed==1&&r.damage==0&&s.flag(37),"Flight P1 lost zero-damage callback/outer ignition");for(int n=0;n<8;++n){scene.update({});scene.takePresentation();}need(s.variable(91)==8&&s.variable(89)>0,"Direct immunity incorrectly blocked independent burn");s.setFlag(15,true);s.setVariable(83,0);s.setVariable(84,0);s.setVariable(301,12);for(int n=0;n<10;++n){scene.update({});scene.takePresentation();}need(!s.flag(37)&&s.variable(301)==0,"Dual nonpositive retained cache failed burn stop");}
 for(int mode:{0,1,2,3,4,5,6,7,8,9,10,11,12}){for(int page=1;page<=6;++page){
  auto e=entry(c,saved);GameSession s(c.items,e.session,c.facts);s.setFlag(21,true);s.setVariable(91,mode);s.setFlag(13,true);CombatScene scene(c,s,e,safe);scene.update({});scene.takePresentation();
  const auto body=findCharacter(scene.world(),"m191-e2").motion.position();s.setVariable(90,2);s.setVariable(83,(body.x+.5)*16-scene.cameraCenter().x+160);s.setVariable(84,(body.y+1)*16-scene.cameraCenter().y+120-8);s.setVariable(87,8);s.setVariable(88,5);
  AttackSample a{{(body.x+.5)*16,(body.y+1)*16-8},{0,0},AttackKind::Handgun,page,0,false,1,AttackPhase::Impact};std::set<std::string> hit;
  const int activeMode=s.variable(91);const bool activeAction=s.flag(21);const auto r=scene.resolveAttack(a,hit);if(page>=5){need(s.variable(91)==activeMode&&s.flag(21)==activeAction,"P5/6 route replacement cancelled source action");}const bool allowed=page!=1&&(mode<=6||mode==11);need(r.contacts==1&&r.processed==1&&(s.variable(89)>0)==allowed,"Cash receiver action gate mode="+std::to_string(mode));need(scene.resolveAttack(a,hit).processed==0,"Duplicate attack receiver");
 }}
 for(int pattern:{0,1,2}){
  auto terrainContent=c;terrainContent.maps.at(191).lowerTiles.reset();auto& cells=terrainContent.terrainIds.at(191);std::fill(cells.begin(),cells.end(),0);
  auto e=entry(terrainContent,saved);const auto boss=terrainContent.objects.at("m191-e2").tile;const std::array<int,8> events{3,5,6,9,11,14,15,16};const std::array<TilePosition,8> offsets{{{0,-3},{3,-3},{3,0},{3,3},{0,3},{-3,3},{-3,0},{-3,-3}}};
  for(int i=0;i<8;++i){if(pattern==1||(pattern==2&&i%2)){cells.at((boss.y+offsets[i].y)*terrainContent.maps.at(191).width+boss.x+offsets[i].x)=1;}}
  GameSession s(terrainContent.items,e.session,terrainContent.facts);s.setFlag(13,true);s.setFlag(21,true);s.setVariable(91,4);s.setVariable(193,99);for(int i=0;i<8;++i){s.setFlag(451+i,i%2==0);}CombatScene scene(terrainContent,s,e,safe);bool sampled=false;std::set<int> routed;
  for(int n=0;n<1200&&s.flag(21);++n){scene.update({});scene.takePresentation();if(!sampled&&s.variable(193)!=99){sampled=true;for(int i=0;i<8;++i){const bool valid=pattern==1||(pattern==2&&i%2);need(s.flag(451+i)==(i%2==0||valid),"Illegal terrain cleared prior source flag");const auto tile=findCharacter(scene.world(),"m191-e"+std::to_string(events[i])).motion.tile();need(tile==(valid?TilePosition{boss.x+offsets[i].x,boss.y+offsets[i].y}:TilePosition{0,0}),"Terrain candidate did not relocate independently");}}
   for(int event:events){if(EnemyHost(scene,"m191-e"+std::to_string(event)).routePending()){routed.insert(event);}}
  }
  need(sampled&&routed.size()==8&&!s.flag(21),"Terrain matrix skipped an inactive spike route/completion");for(int f=451;f<=458;++f){need(!s.flag(f),"Normal alpha tail failed source flag clear");}
 }
 for(bool fatalOverlap:{false,true}){
  auto e=entry(c,saved);e.session.hp=100;e.positions["m191-e3"]={14,20};GameSession s(c.items,e.session,c.facts);s.setFlag(21,true);s.setVariable(91,12);for(int f=451;f<=458;++f){s.setFlag(f,f==451);}CombatScene scene(c,s,e,safe);scene.update({});scene.takePresentation();need(s.flag(8),"Initial CE14 trigger absent");const int firstHp=s.hp();s.setFlag(13,false);s.setFlag(45,fatalOverlap);int secondMedium=0;
  for(int n=0;n<2;++n){scene.update({});for(const auto& ev:scene.takePresentation()){secondMedium+=ev.kind==PresentationEvent::Kind::Effect&&ev.id=="blood-medium";}}
  need(s.hp()<firstHp&&secondMedium==0&&s.flag(8),"Repeated contact restarted in-flight CE14");scene.update({});scene.takePresentation();need(s.flag(8),"CE14 finished before fourth source wait");scene.update({});const auto tail=scene.takePresentation();
  if(fatalOverlap){need(s.hp()==0&&s.flag(8)&&std::count_if(tail.begin(),tail.end(),[](const auto& ev){return ev.kind==PresentationEvent::Kind::Effect&&ev.id=="blood-large";})==1,"CE14 later type3 branch lost death overlap");for(int n=0;n<4;++n){scene.update({});scene.takePresentation();need(s.flag(8)==(n<3),"Later CE14 branch did not finish after four waits");}}
  else{need(!s.flag(8),"Repeated trigger reset source CE14 completion time");}
 }
 for(bool down:{false,true}){
  auto e=entry(c,saved);e.session.hp=100;const std::array<int,8> events{3,5,6,9,11,14,15,16};for(int event:events){e.positions["m191-e"+std::to_string(event)]={14,20};}GameSession s(c.items,e.session,c.facts);s.setFlag(21,true);s.setVariable(91,12);s.setFlag(45,down);for(int f=451;f<=458;++f){s.setFlag(f,true);}CombatScene scene(c,s,e,safe);const int hp=s.hp(),wounds=s.variable(41);scene.update({});const auto effects=scene.takePresentation();need(std::count_if(effects.begin(),effects.end(),[](const auto& p){return p.kind==PresentationEvent::Kind::Effect&&p.id.starts_with("blood-");})==(down?2:1),"Simultaneous spike blood and CE23 death feedback mismatch down="+std::to_string(down)+" effects="+std::to_string(effects.size())+" hp="+std::to_string(s.hp()));const int expectedWounds=wounds+1>=c.rules.at("player_damage").at("bleeding_check_hits").get<int>()?0:wounds+1;need(s.variable(41)==expectedWounds,"Eight same-frame spikes ignored shared protection/dead commit");need(s.flag(8),"CE14 trigger cleared before its four waits");if(down){need(s.hp()==0&&s.flag(15),"First fatal spike failed sibling stop");for(int n=0;n<4;++n){scene.update({});scene.takePresentation();need(s.flag(8)==(n<3),"Retired Cash stopped CE14 completion");}}else{const int oneHit=s.hp();for(int n=0;n<89;++n){scene.update({});scene.takePresentation();if(n<4){need(s.flag(8)==(n<3),"CE14 source trigger did not end at native effect completion");}}need(s.hp()==oneHit&&s.flag(13),"Protection did not hold repeated spike contacts");scene.update({});scene.takePresentation();need(s.hp()<oneHit&&s.hp()>0&&s.conditions().invulnerableTicks==90,"Wait0 contact did not recur after protection expiry");}
 }
 for(int event:{3,5,6,9,11,14,15,16}){for(bool protect:{false,true}){for(bool down:{false,true}){
  auto e=entry(c,saved);const std::string id="m191-e"+std::to_string(event);e.positions[id]={14,20};GameSession s(c.items,e.session,c.facts);s.setFlag(21,true);s.setVariable(91,11);
  // Flag identity follows source event order; no terrain result may turn it off here.
  const std::array<int,8> events{3,5,6,9,11,14,15,16};for(int i=0;i<8;++i){s.setFlag(451+i,events[i]==event);}s.setFlag(13,protect);s.setFlag(45,down);CombatScene scene(c,s,e,safe);const int hp=s.hp();scene.update({});scene.takePresentation();need((s.hp()<hp)==!protect,"Spike protection");if(down&&!protect){need(s.hp()==0&&s.flag(15),"Downed spike must be lethal");}else if(!protect){need(s.flag(13)&&s.conditions().invulnerableTicks==90,"Spike CE13 pulse missing timer");}
 }}}
 for(int mode:{2,3,4,5,7,8,9,10,11}){
  auto e=entry(c,saved);GameSession s(c.items,e.session,c.facts);s.setFlag(21,true);s.setVariable(91,mode);s.setFlag(13,true);CombatScene scene(c,s,e,safe);bool cues=false,done=false;
  for(int n=0;n<6000;++n){scene.update({});for(const auto& v:scene.takePresentation()){cues|=v.kind==PresentationEvent::Kind::Sound||(v.kind==PresentationEvent::Kind::Cue&&v.id.starts_with("m191"));}if(!s.flag(21)||s.variable(91)!=mode){done=true;break;}}
  need(cues&&done,"Cash action failed mode="+std::to_string(mode));
 }
 for(int boundary:{499,500}){for(int page=1;page<=6;++page){auto e=entry(c,saved);GameSession s(c.items,e.session,c.facts);s.setFlag(13,true);CombatScene scene(c,s,e,safe);scene.update({});scene.takePresentation();const auto p=findCharacter(scene.world(),"m191-e2").motion.position();s.setVariable(89,boundary);s.setVariable(90,2);std::set<std::string> hit;const auto r=scene.resolveAttack({{(p.x+.5)*16,(p.y+1)*16-8},{0,0},AttackKind::Handgun,page},hit);const bool adds=boundary==499&&page>1;need(r.processed==1&&(r.damage>0)==adds&&s.variable(89)==boundary+r.damage,"499/500 receiver boundary mismatch");}}
 auto e=entry(c,saved);GameSession s(c.items,e.session,c.facts);s.setFlag(451,true);s.setFlag(21,true);s.setVariable(91,4);CombatScene scene(c,s,e,safe);scene.update({});s.setFlag(451,true);scene.deactivate();need(s.flag(451),"Scene retirement cleared shared source451");const auto state=s.exportState();for(int n=0;n<50;++n){scene.update(key());}need(s.exportState()==state&&s.flag(451),"Cancelled scene cleared source spike flag");
 std::cout<<"G5-01 source receiver/actions/eight spike/protection/retirement PASS\n";
}
void continues(const CombatContent& c,const SessionEntry& saved){
 for(int choice:{0,1,2,3}){
  auto e=entry(c,saved);GameSession s(c.items,e.session,c.facts);s.setFlag(451,true);s.captureContinueCheckpoint(c.magazineSeedBindings,220,{9,7});CombatScene scene(c,s,e,safe);const int retries=s.variable(630);
  if(choice==3){EnemyHost(scene,"m191-e2").fail(c.expansion.failureFlows.begin().key());}else{s.loseHp(1000);}
  for(int n=0;n<4000&&!scene.inputPromptView().cursor;++n){scene.update(n%8==0?key():InputFrame{});scene.takePresentation();}
  need(scene.inputPromptView().cursor.has_value(),"Cash CE38 Continue prompt");if(choice==2){scene.configurePresentation([](const SceneDefinition& d){if(d.width==20){throw std::runtime_error("injected220 media prepare failure");}});}if(choice==1||choice==3){scene.update(key(InputAction::Down));scene.update({});}
  const auto before=s.exportState();scene.update(key());bool via220=false,title=false;
  for(int n=0;n<4000;++n){scene.update(n%8==0?key():InputFrame{});scene.takePresentation();via220|=scene.mapId()==220;title|=scene.takeApplicationRequest()==ApplicationMode::Title;if(title||!scene.transferError().empty()||(via220&&scene.mapId()==191&&scene.foreground()==ForegroundKind::None)){break;}}
  if(choice==2){need(!scene.transferError().empty()&&s.exportState()==before,"220 failure mutated current candidate");}
  else if(choice==1||choice==3){need(title&&s.hp()==(choice==3?saved.hp:0)&&s.variable(630)==retries,"CE38 No/positive-HP title mismatch");}
  else{need(via220&&scene.mapId()==191&&s.hp()==saved.hp&&s.saveCount()==60&&s.magazine(20)==2&&s.magazine(21)==5&&s.count(111)==1&&s.variable(630)==retries+1&&!s.flag(508)&&!s.flag(507)&&s.flag(451)&&s.variable(89)==0,"220 restored source backup incorrectly");}
 }
 std::cout<<"G5-01 Continue220 Yes/No/positive-HP/failed candidate preservation PASS\n";
}
void mediaAndSelection(const CombatContent& original,const SessionEntry& saved,const std::filesystem::path& root){
 for(int mode:{2,3,4,5,7,8,9,10,11}){
  auto c=original;auto e=entry(c,saved);e.session.flags[21]=true;e.session.flags[13]=true;e.session.variables[91]=mode;c.entries["cash-media-check"]=e;GameApplication g(c,"cash-media-check",safe);DoraRender render(g,c,root/("mode-"+std::to_string(mode)));
  for(int n=0;n<2000;++n){g.update({});render.pending=g.takePresentation();bool visual=false;for(const auto& ev:render.pending){visual|=ev.kind==PresentationEvent::Kind::Cue&&ev.id.starts_with("m191")&&c.cue(ev.id).at("kind")=="show_picture";}render.tick();if(visual){render.frame("source-frame-"+std::to_string(n));}if(!g.session().flag(21)||g.session().variable(91)!=mode){break;}}
  render.frame("tail");
 }
 // Native selector runs from source idle page, with controlled RNG and ordinary geometry.
 std::set<int> choices;
 for(int seed=0;seed<24;++seed){auto e=entry(original,saved,{seed%2?14:9,20});GameSession s(original.items,e.session,original.facts);s.setFlag(13,true);auto gen=std::make_shared<std::mt19937>(seed);CombatScene scene(original,s,e,[gen](int lo,int hi){return std::uniform_int_distribution<int>(lo,hi)(*gen);});for(int n=0;n<900;++n){scene.update({});scene.takePresentation();choices.insert(s.variable(91));}}
 need(choices.contains(2)&&choices.contains(4)&&choices.contains(5)&&choices.contains(7)&&choices.contains(11)&&!choices.contains(6)&&!choices.contains(12),"NORMAL source selector variation/HARD residual boundary");
 std::ofstream(root/"selector.json")<<Json{{"kind","controlled-rng-original-idle-selection"},{"modes",choices},{"request8_10_overwrite_preserved",true},{"gamma6_natural",false},{"residual12_producer",false}}.dump(2)<<'\n';
 std::cout<<"G5-01 actual media/all fixed actions/source selection variants PASS\n";
}
struct CashDriver{
 GameApplication game;std::ofstream trace;int ticks{};std::vector<PresentationEvent> pending;
 CashDriver(const CombatContent& c,const std::filesystem::path& root,CombatScene::Random random):game(c,"cash-capability",std::move(random)),trace(root/"public-inputs.jsonl"){}
 void tick(InputFrame in={}){int held=0,pressed=0;for(int i=0;i<int(in.buttons.size());++i){held|=int(in.buttons[i].held)<<i;pressed|=int(in.buttons[i].pressed)<<i;}trace<<Json{{"step",ticks++},{"map",game.hasGame()?game.mapId():0},{"movement",in.movement?int(*in.movement):-1},{"held",held},{"pressed",pressed}}.dump()<<'\n';game.update(in);pending=game.takePresentation();}
 void press(InputAction a=InputAction::Confirm){tick(key(a));tick();}
 void select(int id){const auto v=game.menuView();auto it=std::find_if(v.rows.begin(),v.rows.end(),[&](const auto& r){return r.id==id;});need(it!=v.rows.end(),"Cash menu missing row");const int goal=int(it-v.rows.begin());for(int n=0;n<int(v.rows.size())&&game.menuView().selection!=goal;++n){press(v.columns==2?InputAction::Right:InputAction::Down);}need(game.menuView().selection==goal,"Cash menu selection stalled");press();}
 void heal(){press(InputAction::Cancel);select(0);select(5);if(game.mode()==ApplicationMode::Equipment&&game.menuView().page==MenuPage::Target){press();}while(game.mode()==ApplicationMode::Equipment){press(InputAction::Cancel);}}
};
void battle(const CombatContent& original,const SessionEntry& saved,const std::filesystem::path& root){
 auto c=original;auto e=entry(c,saved);e.session.hp=100;e.session.mainWeapon=21;e.session.magazines[21]=7;e.session.inventory[21]=1;e.session.inventory[42]=40;e.session.inventory[5]=20;GameSession checkpoint(c.items,e.session,c.facts);checkpoint.captureContinueCheckpoint(c.magazineSeedBindings,220,{9,7});e.session=checkpoint.exportState();c.entries["cash-capability"]=e;
 auto rng=std::make_shared<std::mt19937>(1912026);CashDriver r(c,root,[rng](int lo,int hi){return std::uniform_int_distribution<int>(lo,hi)(*rng);});DoraRender render(r.game,c,root/"frames");std::set<int> modes;int fired=0;
 const int countBefore=r.game.session().variable(630);for(int n=0;n<16000&&r.game.session().hp()>0;++n){r.tick();render.pending=r.pending;render.tick();}need(r.game.session().hp()==0,"Controlled entry public idle loss absent");render.frame("public-death");
 for(int n=0;n<4000&&!r.game.inputPromptView().cursor;++n){r.tick(n%8==0?key():InputFrame{});render.pending=r.pending;render.tick();}need(r.game.inputPromptView().cursor.has_value(),"Public Cash CE38 prompt absent");r.tick(key());bool via220=false;
 for(int n=0;n<4000;++n){r.tick(n%8==0?key():InputFrame{});render.pending=r.pending;render.tick();if(r.game.mapId()==220){via220=true;render.frame("public-continue220");}if(via220&&r.game.mapId()==191&&r.game.foreground()==ForegroundKind::None){break;}}
 need(via220&&r.game.mapId()==191&&r.game.session().hp()==100&&r.game.session().variable(630)==countBefore+1&&r.game.session().magazine(21)==7,"Public Continue220 did not restore capability backup");

 for(int n=0;n<60000&&r.game.mapId()==191;++n){auto& g=r.game;const auto& s=g.session();need(s.hp()>0,"Public Cash battle died");const int rounds=s.magazine(21),mode=s.variable(91);modes.insert(mode);InputFrame in;
  if(s.hp()<60&&s.count(5)>0&&g.foreground()==ForegroundKind::None&&s.flag(1)&&!s.flag(45)){r.heal();continue;}
  if(g.dialogue().active()||g.foreground()!=ForegroundKind::None){if(n%8==0){in=key();}}
  else if(s.flag(45)){if(n%3==0){in=key(InputAction::Down);}else if(n%3==1){in=key(InputAction::Up);}}
  else if(!g.raised()){in=key(InputAction::Weapon);}
  else{auto p=g.world().player().motion.tile(),t=findCharacter(g.world(),"m191-e2").motion.tile();int dx=t.x-p.x,dy=t.y-p.y;
   if(mode>=7&&mode<=10){in.movement=next(g.world(),{p.x<14?8:20,p.y<20?14:24});}
   else if((dx&&dy)||std::abs(dx)+std::abs(dy)>6){for(auto side:{Direction::Down,Direction::Up,Direction::Left,Direction::Right}){const auto step=next(g.world(),adjacentTile(t,side));if(step){in.movement=step;break;}}}
   else{auto face=std::abs(dx)>std::abs(dy)?(dx>0?Direction::Right:Direction::Left):(dy>0?Direction::Down:Direction::Up);if(g.world().player().motion.facing()!=face){in.buttons[int(InputAction::Weapon)].held=true;in.movement=face;}else if(n%12==0){in=key(InputAction::Attack);}}
  }
  r.tick(in);fired+=std::max(0,rounds-r.game.session().magazine(21));render.pending=r.pending;render.tick();if(g.mapId()==191){if(modes.contains(mode)){render.frame("mode-"+std::to_string(mode));}}else{render.frame("victory-M190");}
  if(n%500==0){std::cout<<"Cash step="<<r.ticks<<" HP="<<g.session().hp()<<" dmg="<<g.session().variable(89)<<" mode="<<g.session().variable(91)<<" ammo="<<g.session().magazine(21)<<'\n';}
 }
 need(r.game.mapId()==190&&r.game.session().flag(508)&&!r.game.session().flag(586)&&fired>0&&!r.game.session().flag(1),"Public Cash win boundary absent");
 std::ofstream(root/"battle-result.json")<<Json{{"kind","controlled-capability-entry-public-input"},{"natural_entry",false},{"steps",r.ticks},{"hp",r.game.session().hp()},{"map",r.game.mapId()},{"508",true},{"public_loss",true},{"via220",via220},{"fired",fired},{"modes",modes},{"frames",render.frames},{"fixture",{{"hp",100},{"mainWeapon",21},{"magazine21",7},{"reserve42",40},{"herb5",20}}}}.dump(2)<<'\n';
}
}
int main(int argc,char**argv){try{std::cout.setf(std::ios::unitbuf);need(argc==5,"Cash package real-save root rules|battle");auto c=loadCombatContent(argv[1]);const auto raw=read(argv[2]);need(contentSha256(raw)=="f87bb3d63d67803dc33916916dface8096d7ad09ef86cf3270570c93627edb62","Changed real G3 disk");const auto saved=decodeSave(c,raw);std::filesystem::path root=argv[3];std::filesystem::create_directories(root);if(std::string(argv[4])=="rules"){need(decodeSave(c,encodeSave(c,saved))==saved,"G5 identity save roundtrip");SaveFiles files(c,root/"new-identity");need(files.write(1,saved).outcome==WriteOutcome::Committed&&files.read(1)==saved,"G5 independent save/load lost actual progress");auto restored=prepareRestore(c,files.read(1));need(restored.scene->mapId()==151&&restored.session->hp()==46&&restored.session->saveCount()==60,"G5 new identity restore changed source stats");rules(c,saved.session);continues(c,saved.session);mediaAndSelection(c,saved.session,root/"action-frames");}else{battle(c,saved.session,root);}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
