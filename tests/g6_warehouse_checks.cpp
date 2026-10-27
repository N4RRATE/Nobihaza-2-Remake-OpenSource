#include "g2_natural_driver.h"
#include "enemy_host.h"
#include "g4_dora_render.h"
#include <fstream>
namespace {
std::string read(const std::filesystem::path& p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
CombatEntry warehouseEntry(const CombatContent& c,const SessionEntry& base,TilePosition tile={14,12},bool started=true){
 auto e=c.entries.at("post-pickup");e.map=204;e.playerTile=tile;e.direction=Direction::Up;e.positions.clear();e.pages.clear();e.directions.clear();e.through.clear();e.session=base;initializeFullSession(c,e.session);
 e.session.flags[1]=true;e.session.flags[18]=true;e.session.flags[21]=false;e.session.flags[13]=false;e.session.worldFlags[c.facts.switches.at(577)]=started;e.session.worldFlags[c.facts.switches.at(578)]=false;e.session.worldFlags[c.facts.switches.at(527)]=false;e.session.worldFlags[c.facts.switches.at(586)]=false;return e;
}
void tick(CombatScene& scene,int n=1){for(int k=0;k<n;++k){scene.update({});scene.takePresentation();}}
AttackSample atCache(const CombatScene& scene,const GameSession& s,AttackKind kind,int page,std::uint64_t id=1){const auto camera=scene.cameraCenter();return {{s.variable(83)+camera.x-160,s.variable(84)+camera.y-120},{0,0},kind,page,0,false,id,AttackPhase::Impact};}
void sourceRules(const CombatContent& c,const SessionEntry& base){
 need(c.catalog.at("warehouse_g4").at("actor_attributes").at("38")==Json{{"6",17},{"7",38},{"8",32}},"Warehouse NORMAL source actor attributes");
 for(int cacheMode:{6,7}){std::cout<<"cacheMode="<<cacheMode<<'\n';
  auto e=warehouseEntry(c,base);GameSession s(c.items,e.session,c.facts);s.setFlag(21,true);s.setFlag(13,true);s.setVariable(91,cacheMode);s.setVariable(83,31);s.setVariable(84,37);s.setVariable(90,2);CombatScene scene(c,s,e,safe);tick(scene);
  need(cacheMode==6?(s.variable(83)==31&&s.variable(84)==37):(s.variable(83)!=31||s.variable(84)!=37),"Mode6 must retain original cache; mode7 resumes source sampling");
 }
 for(auto kind:{AttackKind::Handgun,AttackKind::Knife,AttackKind::Magnum,AttackKind::De,AttackKind::Shotgun,AttackKind::Grenade,AttackKind::GlNormal,AttackKind::GlFlame,AttackKind::GlAcid,AttackKind::Rocket,AttackKind::Sg,AttackKind::Lc9}){
  auto e=warehouseEntry(c,base);GameSession s(c.items,e.session,c.facts);s.setFlag(13,true);s.setFlag(21,true);s.setVariable(91,7);CombatScene scene(c,s,e,safe);tick(scene);s.setVariable(89,0);std::set<std::string> hit;
  const int page=kind==AttackKind::Grenade?6:kind==AttackKind::Magnum||kind==AttackKind::De?5:2;const auto a=atCache(scene,s,kind,page);const auto r=scene.resolveAttack(a,hit);need(r.contacts==1&&r.processed==1&&r.damage>0,"Warehouse ordinary weapon receiver");need(scene.resolveAttack(a,hit).processed==0,"Warehouse repeated attack pulse");
 }
 for(int damage:{999,1000}){for(int page=1;page<=6;++page){
  auto e=warehouseEntry(c,base);GameSession s(c.items,e.session,c.facts);s.setFlag(13,true);s.setFlag(21,true);s.setVariable(91,7);CombatScene scene(c,s,e,safe);tick(scene);s.setVariable(89,damage);std::set<std::string> hit;const auto r=scene.resolveAttack(atCache(scene,s,AttackKind::Handgun,page),hit);need(r.processed==1&&(r.damage>0)==(damage==999&&page!=1)&&s.variable(89)==damage+r.damage,"Warehouse999/1000 and no-damage geometry contract");
 }}
 for(int mode:{1,2,3,4,6,7,8,9,10}){std::cout<<"actionMode="<<mode<<'\n';
  auto e=warehouseEntry(c,base,{14,14});GameSession s(c.items,e.session,c.facts);s.setFlag(13,true);s.setFlag(21,true);s.setVariable(91,mode);auto rng=std::make_shared<std::mt19937>(20400+mode);CombatScene scene(c,s,e,[mode,rng](int lo,int hi){return mode==9||mode==10?std::uniform_int_distribution<int>(lo,hi)(*rng):mode==8?hi:lo;});bool cues=false,changed=false;
  for(int n=0;n<6000&&s.hp()>0;++n){scene.update({});for(const auto& p:scene.takePresentation()){cues|=p.kind==PresentationEvent::Kind::Cue||p.kind==PresentationEvent::Kind::Sound;}if(s.variable(91)!=mode||!s.flag(21)){changed=true;break;}}
  need(cues&&changed,"Warehouse original action did not complete/transition mode="+std::to_string(mode));
 }
 for(bool fatal:{false,true}){
  auto e=warehouseEntry(c,base);e.session.hp=fatal?1:100;GameSession s(c.items,e.session,c.facts);s.setFlag(21,true);s.setVariable(91,5);CombatScene scene(c,s,e,safe);const int hp=s.hp();bool title=false,continuePrompt=false,sourceTail=false;
  for(int n=0;n<6000;++n){scene.update(n%8==0?key():InputFrame{});for(const auto& p:scene.takePresentation()){sourceTail|=p.id=="m204-e2-p16-c219";}continuePrompt|=scene.inputPromptView().cursor.has_value();title|=scene.takeApplicationRequest()==ApplicationMode::Title;if(title||(!fatal&&scene.foreground()==ForegroundKind::None&&s.variable(91)!=5)){break;}}
  if(fatal){need(title&&s.flag(527)&&s.hp()==hp&&sourceTail&&!continuePrompt&&!s.flag(586),"Source positive-HP squeeze fatal tail must finishCE38/title withoutContinue");}
  else{need(!title&&!s.flag(527)&&s.hp()<hp&&s.hp()>0&&s.variable(91)==7,"Squeeze surviving source damage/recoil/roar");}
 }
 {auto e=warehouseEntry(c,base);e.session.hp=1;GameSession s(c.items,e.session,c.facts);s.setFlag(21,true);s.setVariable(91,5);CombatScene scene(c,s,e,safe);for(int n=0;n<4000&&!s.flag(527);++n){tick(scene);}need(s.flag(527),"Fatal prefix not reached");scene.deactivate();const auto before=s.exportState();for(int n=0;n<500;++n){scene.update(key());}std::set<std::string> ids;need(scene.resolveAttack(atCache(scene,s,AttackKind::Handgun,2),ids).processed==0&&s.exportState()==before&&!scene.takeApplicationRequest(),"Retired squeeze retained old requests/facts");}
 std::cout<<"G6-04 fixed receiver/actions/cache/squeeze/cancellation PASS\n";
}
void entryAndVictory(const CombatContent& c,const SessionEntry& base){
 for(bool yes:{false,true}){
  auto e=warehouseEntry(c,base,{19,6},false);e.session.inventory[180]=1;GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,safe);scene.update(key());scene.takePresentation();bool answered=false;
  for(int n=0;n<12000;++n){InputFrame in;if(scene.inputPromptView().cursor){if(!answered&&!yes){in=key(InputAction::Down);}else{in=key();}answered=true;}else if(n%8==0){in=key();}scene.update(in);scene.takePresentation();if(answered&&scene.foreground()==ForegroundKind::None){break;}}
  need(answered&&s.flag(577)==yes,"Warehouse Yes/No late start commit yes="+std::to_string(yes)+" answered="+std::to_string(answered)+" started="+std::to_string(s.flag(577))+" fg="+std::to_string(int(scene.foreground()))+" x="+std::to_string(scene.world().player().motion.tile().x)+" y="+std::to_string(scene.world().player().motion.tile().y));
  need(scene.world().player().motion.tile()==(yes?TilePosition{14,12}:TilePosition{19,6}),"Entry original11-step route orNo position");need(s.count(180)==(yes?0:1)&&s.count(195)==(yes?1:0),"Entry original file swap");
 }
 {auto e=warehouseEntry(c,base);e.session.inventory[195]=1;GameSession s(c.items,e.session,c.facts);s.setVariable(89,1000);s.setVariable(201,4);CombatScene scene(c,s,e,safe);bool early=false;int substitutions=0;const int stock=s.count(180);
  for(int n=0;n<15000;++n){scene.update(n%8==0?key():InputFrame{});scene.takePresentation();substitutions=int(scene.world().substitutedUpperTiles().size());if(!s.flag(578)){early|=s.count(180)>stock;}if(s.flag(578)&&scene.foreground()==ForegroundKind::None){break;}}
  need(s.flag(578)&&s.flag(577)&&substitutions==24&&s.count(195)==0&&s.count(180)==stock+1&&!early,"Victory127 must commit platform/file/578 in original sequence");
  const int direction=s.variable(201);tick(scene,80);need(s.variable(201)==direction&&s.variable(202)!=s.variable(201),"Corpse must retain201 independently from sampled202");
  const auto snapshot=s.exportState();scene.deactivate();CombatEntry rebuilt=warehouseEntry(c,snapshot);rebuilt.session=snapshot;CombatScene next(c,s,rebuilt,safe);tick(next,80);need(s.count(180)==stock+1&&s.count(195)==0&&next.foreground()==ForegroundKind::None,"Corpse rebuild replayed victory/files");need(next.world().substitutedUpperTiles().size()==24,"Victory fact did not derive platform on rebuilding");
 }
 std::cout<<"G6-04 entry11 steps/No/late victory/platform/corpse reconstruction PASS\n";
}
void criticalAndGeometry(const CombatContent& c,const SessionEntry& base){
 std::vector<int> damage;
 for(bool critical:{false,true}){auto e=warehouseEntry(c,base);GameSession s(c.items,e.session,c.facts);s.setFlag(13,true);s.setFlag(21,true);s.setVariable(91,7);CombatScene scene(c,s,e,safe);tick(scene);s.setVariable(89,0);s.setVariable(35,0);s.setFlag(47,critical);std::set<std::string> hit;const auto r=scene.resolveAttack(atCache(scene,s,AttackKind::Handgun,2),hit);damage.push_back(r.damage);need(r.processed==1,"Critical source receiver missing");}
 need(damage.at(0)>0&&damage.at(1)==2*damage.at(0),"Shared CE11 critical must be applied once before source doubling");
 for(int offset:{20,21}){auto e=warehouseEntry(c,base);GameSession s(c.items,e.session,c.facts);s.setFlag(13,true);s.setFlag(21,true);s.setVariable(91,7);CombatScene scene(c,s,e,safe);tick(scene);auto a=atCache(scene,s,AttackKind::Handgun,2);a.center.x+=offset;std::set<std::string> hit;const auto r=scene.resolveAttack(a,hit);need((r.contacts==1)==(offset==20),"Original20/21 cache boundary");}
 std::cout<<"G6-04 critical once/original cache geometry PASS\n";
}

void landing(const CombatContent& c,const SessionEntry& base){
 auto e=warehouseEntry(c,base,{14,14});e.session.hp=100;GameSession s(c.items,e.session,c.facts);s.setFlag(21,true);s.setFlag(13,false);s.setVariable(91,10);auto rng=std::make_shared<std::mt19937>(20410);int draws=0;CombatScene scene(c,s,e,[rng,&draws](int lo,int hi){if(lo==-3&&hi==3){if(draws++<2){return lo;}}return std::uniform_int_distribution<int>(lo,hi)(*rng);});const int hp=s.hp();
 for(int n=0;n<6000&&s.variable(91)==10;++n){tick(scene);}const auto tile=findCharacter(scene.world(),"m204-e2").motion.tile();need(s.variable(91)!=10&&s.hp()==hp&&draws>=4&&EnemyHost(scene,"m204-e2").terrain(tile)!=3,"Landing must reject first illegal candidate and finish without invented damage");std::cout<<"G6-04 illegal first landing retry/legal terrain/no impact damage PASS draws="<<draws<<'\n';
}

void regularDeath(const CombatContent& c,const SessionEntry& base){
 auto e=warehouseEntry(c,base,{14,8});e.session.hp=1;e.directions["m204-e2"]=Direction::Down;GameSession s(c.items,e.session,c.facts);s.setFlag(21,true);s.setFlag(13,false);s.setVariable(91,2);CombatScene scene(c,s,e,safe);
 for(int n=0;n<600&&s.hp()>0;++n){tick(scene);}need(s.hp()==0&&s.flag(8)&&scene.foreground()==ForegroundKind::Death,"Ordinary Warehouse collision death/blood pulse absent hp="+std::to_string(s.hp())+" blood="+std::to_string(s.flag(8))+" fg="+std::to_string(int(scene.foreground()))+" mode="+std::to_string(s.variable(91))+" dx="+std::to_string(s.variable(85))+" dy="+std::to_string(s.variable(86)));for(int n=0;n<4;++n){tick(scene);need(s.flag(8)==(n<3),"Committed CE14 blood stage stalled after boss retirement");}
 bool title=false,prompt=false;for(int n=0;n<4000&&!title;++n){scene.update(n%8==0?key():InputFrame{});scene.takePresentation();prompt|=scene.inputPromptView().cursor.has_value();title=scene.takeApplicationRequest()==ApplicationMode::Title;}
 need(title&&!prompt&&!s.flag(586)&&!s.flag(578)&&!s.flag(527),"Normal HP0 death invented Continue/win/fatal-squeeze");scene.deactivate();const auto before=s.exportState();tick(scene,20);need(s.exportState()==before,"Retired ordinary death wrote afterTitle");std::cout<<"G6-04 ordinary HP0 collision/committed blood4 waits/noContinue/title/cancel PASS\n";
}

void sourceMedia(const CombatContent& original,const SessionEntry& base,const std::filesystem::path& root){
 int backendSounds=0;std::set<int> seen;
 for(int mode:{1,2,3,4,5,6,7,8,9,10}){
  auto c=original;auto e=warehouseEntry(c,base,{14,12});e.session.hp=100;e.session.flags[13]=mode!=5;e.session.flags[21]=true;e.session.variables[91]=mode;if(mode==9){e.positions["m204-e2"]={13,12};}c.entries["warehouse-media"]=e;auto rng=std::make_shared<std::mt19937>(204000+mode);GameApplication game(c,"warehouse-media",[rng](int lo,int hi){return std::uniform_int_distribution<int>(lo,hi)(*rng);});DoraRender render(game,c,root/("mode-"+std::to_string(mode)));bool pictured=false;
  for(int n=0;n<6000;++n){game.update({});render.pending=game.takePresentation();for(const auto& ev:render.pending){backendSounds+=ev.kind==PresentationEvent::Kind::Sound;if(ev.kind==PresentationEvent::Kind::Cue&&ev.id.starts_with("m204")&&c.cue(ev.id).at("kind")=="show_picture"){pictured=true;}}render.tick();if(pictured){render.frame("first-source-picture");}if(!game.session().flag(21)||game.session().variable(91)!=mode){break;}}
  need(pictured,"Warehouse mode omitted actual source picture");seen.insert(mode);render.frame("tail");
 }
 {auto c=original;auto e=warehouseEntry(c,base,{14,25});e.session.worldFlags[c.facts.switches.at(578)]=true;c.entries["warehouse-platform"]=e;GameApplication game(c,"warehouse-platform",safe);DoraRender render(game,c,root/"platform");for(int n=0;n<30;++n){game.update({});render.pending=game.takePresentation();render.tick();}render.frame("36-cells");need(game.world().substitutedUpperTiles().size()==24,"36-cell renderer world restoration");const auto& rows=game.definition().upperTileReplacements;auto row=std::find_if(rows.begin(),rows.end(),[](const auto& r){return r.fromIndex==15;});need(row!=rows.end()&&row->cellUnderlays.size()==2&&read(row->cellUnderlays[0])!=read(row->cellUnderlays[1]),"Two lower tiles collapsed under one upper mapping");}
 {auto c=original;auto e=warehouseEntry(c,base,{14,5});e.map=195;c.entries["warehouse-old-upper"]=e;GameApplication game(c,"warehouse-old-upper",safe);DoraRender render(game,c,root/"old-single-underlay");for(const auto& row:game.definition().upperTileReplacements){need(row.cellUnderlays.empty(),"Old single underlay representation changed");}render.frame("M195-source");ExplorationScene substituted(game.definition());for(const auto& row:game.definition().upperTileReplacements){substituted.substituteUpperTile(row.fromIndex,row.toIndex);}SceneRenderer renderer(game.definition());render.surface.clear(sf::Color::Black);renderer.draw(render.surface,substituted);render.surface.display();need(render.surface.getTexture().copyToImage().saveToFile(root/"old-single-underlay"/"M195-substituted.png"),"Old single underlay draw failed");need(game.definition().upperTileReplacements.size()==6,"Old six upper substitution consumer missing");}
 std::ofstream(root/"media-result.json")<<Json{{"fixed_modes",seen},{"actual_backend_sound_submissions",backendSounds},{"offscreen",true},{"platform_cells",36},{"old_single_underlay",true},{"original_precise_or_physical_hearing",false}}.dump(2)<<'\n';std::cout<<"G6-04 ten source media actions/36-cell platform/old single underlay PASS\n";
}

void ordinaryCompatibility(const CombatContent& c,const SaveSnapshot& saved,const std::filesystem::path& root){
 need(decodeSave(c,encodeSave(c,saved))==saved,"Warehouse ordinary identity roundtrip");
 for(int flag:{577,578,527}){need(!saved.session.worldFlags.at(c.facts.switches.at(flag)),"Old03 adapter manufactured warehouse progress");}
 auto restore=prepareRestore(c,saved);need(restore.scene->mapId()==183&&restore.session->saveCount()==66,"Actual03 ordinary source failed warehouse restore");
 const auto candidate=captureSave(c,*restore.scene,{restore.scene->generation(),"m183-e4","original"},saved.presentation);need(decodeSave(c,encodeSave(c,candidate))==candidate,"04 original M183 capture/validate/restore");
 for(int badCase=0;badCase<4;++badCase){auto bad=saved;bool rejected=false;
  if(badCase==0){bad.session.worldFlags[c.facts.switches.at(578)]=true;}
  if(badCase==1){bad.session.worldFlags[c.facts.switches.at(527)]=true;}
  if(badCase==2){bad.session.worldFlags["warehouse.unknown"]=false;}
  if(badCase==3){bad.session.worldFlags.erase(c.facts.switches.at(577));}
  try{decodeSave(c,encodeSave(c,bad));}catch(const std::exception&){rejected=true;}need(rejected,"Warehouse ordinary malformed facts accepted");
 }
 SaveFiles files(c,root);need(files.write(1,candidate).outcome==WriteOutcome::Committed&&files.read(1)==candidate,"04 ordinary isolated file commit/load");
 std::ofstream(root/"ordinary-result.json")<<Json{{"kind","actual03 ordinary source adapted/captured"},{"map",183},{"save_count",candidate.session.saveCount},{"new_warehouse_facts",false},{"malformed_rejections",4},{"schema_new",false}}.dump(2)<<'\n';
 std::cout<<"G6-04 ordinary old03 defaults/capture/file load/4 rejections PASS\n";
}
void publicOldLoad(const CombatContent& c,const std::string& raw,const std::filesystem::path& root){
 std::ofstream(root/"slot-01.json")<<raw;const auto disk=decodeSave(c,raw);G2NaturalDriver driver(c,root);driver.game.configureSaveDirectory(root);driver.load();const auto first=driver.game.session().exportState();driver.load();const auto second=driver.game.session().exportState();need(driver.game.mapId()==183&&first.hp==disk.session.hp&&first.saveCount==disk.session.saveCount&&first.magazines==disk.session.magazines&&second.hp==first.hp&&second.saveCount==first.saveCount&&second.magazines==first.magazines,"Independent04 titleLoad changed actual03 resources");for(int flag:{577,578,527}){need(!driver.game.session().flag(flag),"Public old03 Load invented warehouse facts");}std::ofstream(root/"load-result.json")<<Json{{"public_title_load",true},{"repeated_load",true},{"map",183},{"hp",first.hp},{"save_count",first.saveCount},{"ticks",driver.ticks},{"new_warehouse_facts",false}}.dump(2)<<'\n';
}

struct WarehouseDriver {
 GameApplication game;std::ofstream trace;std::vector<PresentationEvent> pending;int ticks{};
 WarehouseDriver(const CombatContent& c,const std::filesystem::path& root,CombatScene::Random random):game(c,"warehouse-capability",std::move(random)),trace(root/"public-inputs.jsonl"){}
 void tick(InputFrame in={}){int held=0,pressed=0;for(int i=0;i<int(in.buttons.size());++i){held|=int(in.buttons[i].held)<<i;pressed|=int(in.buttons[i].pressed)<<i;}trace<<Json{{"step",ticks++},{"map",game.mapId()},{"movement",in.movement?int(*in.movement):-1},{"held",held},{"pressed",pressed}}.dump()<<'\n';game.update(in);pending=game.takePresentation();}
 void press(InputAction a=InputAction::Confirm){tick(key(a));tick();}
 void select(int id){auto v=game.menuView();auto found=std::find_if(v.rows.begin(),v.rows.end(),[&](const auto& r){return r.id==id;});need(found!=v.rows.end(),"Warehouse menu missing item");const int target=int(found-v.rows.begin());for(int n=0;n<int(v.rows.size())&&game.menuView().selection!=target;++n){press(v.columns==2?InputAction::Right:InputAction::Down);}need(game.menuView().selection==target,"Warehouse menu selection stalled");press();}
 void heal(){press(InputAction::Cancel);select(0);select(5);if(game.mode()==ApplicationMode::Equipment&&game.menuView().page==MenuPage::Target){press();}for(int n=0;n<20&&game.mode()==ApplicationMode::Equipment;++n){press(InputAction::Cancel);}need(game.mode()==ApplicationMode::Playing,"Warehouse medicine menu stalled");}
};
void publicBattle(const CombatContent& original,const SessionEntry& base,const std::filesystem::path& root){
 auto c=original;auto e=warehouseEntry(c,base,{19,6},false);e.session.hp=100;e.session.mainWeapon=21;e.session.inventory[21]=1;e.session.magazines[21]=8;e.session.inventory[42]=99;e.session.inventory[5]=20;c.entries["warehouse-capability"]=e;
 auto rng=std::make_shared<std::mt19937>(2042026);WarehouseDriver d(c,root,[rng](int lo,int hi){return std::uniform_int_distribution<int>(lo,hi)(*rng);});DoraRender render(d.game,c,root/"frames");int fired=0;std::set<int> modes;bool entryPressed=false;
 for(int n=0;n<90000;++n){auto& g=d.game;const auto& state=g.session();need(g.hasGame()&&state.hp()>0,"Warehouse capability ordinary inputs died");InputFrame in;const int rounds=state.magazine(21);modes.insert(state.variable(91));
  if(g.inputPromptView().cursor){in=key();}
  else if(g.dialogue().active()||g.foreground()!=ForegroundKind::None){if(n%8==0){in=key();}}
  else if(!state.flag(577)){if(!entryPressed){in=key();entryPressed=true;}}
  else if(state.flag(578)){break;}
  else if(state.hp()<60&&state.count(5)>0&&state.flag(1)&&!state.flag(45)){d.heal();continue;}
  else if(state.flag(45)){if(n%3==0){in=key(InputAction::Down);}else if(n%3==1){in=key(InputAction::Up);}}
  else if(!g.raised()){in=key(InputAction::Weapon);}
  else {const auto p=g.world().player().motion.tile(),t=findCharacter(g.world(),"m204-e2").motion.tile();const int dx=t.x-p.x,dy=t.y-p.y;const int mode=state.variable(91);
   if(mode==6||mode==9||mode==10){in.movement=next(g.world(),{p.x<14?10:18,p.y<15?18:12});}
   else if((dx&&dy)||std::abs(dx)+std::abs(dy)>6){for(auto side:{Direction::Down,Direction::Up,Direction::Left,Direction::Right}){const auto direction=next(g.world(),adjacentTile(t,side));if(direction){in.movement=direction;break;}}}
   else {const auto face=std::abs(dx)>std::abs(dy)?(dx>0?Direction::Right:Direction::Left):(dy>0?Direction::Down:Direction::Up);if(g.world().player().motion.facing()!=face){in.buttons[int(InputAction::Weapon)].held=true;in.movement=face;}else if(n%12==0){in=key(InputAction::Attack);}}
  }
  d.tick(in);fired+=std::max(0,rounds-d.game.session().magazine(21));render.pending=d.pending;render.tick();if(n%500==0){render.frame("battle-"+std::to_string(n));std::cout<<"Warehouse step="<<d.ticks<<" HP="<<g.session().hp()<<" damage="<<g.session().variable(89)<<" mode="<<g.session().variable(91)<<" ammo="<<g.session().magazine(21)<<'\n';}
 }
 need(d.game.session().flag(578)&&fired>0&&d.game.session().variable(89)>=1000&&!d.game.session().flag(586),"Ordinary inputs did not reach complete Warehouse victory");render.frame("victory");
 std::ofstream(root/"capability-battle.json")<<Json{{"kind","controlled-warehouse-entry-public-input"},{"natural06",false},{"steps",d.ticks},{"fired",fired},{"modes",modes},{"hp",d.game.session().hp()},{"damage",d.game.session().variable(89)},{"fixture",{{"hp",100},{"mainWeapon",21},{"magazine21",8},{"reserve42",99},{"herb5",20}}}}.dump(2)<<'\n';
}

}
int main(int argc,char**argv){try{std::cout.setf(std::ios::unitbuf);need(argc==5,"warehouse package actual-g6-save isolated-output rules|battle|load");const auto c=loadCombatContent(argv[1]);const auto saved=decodeSave(c,read(argv[2]));need(c.compatibility=="post-f5-g6-normal-4","Wrong warehouse runtime identity");std::filesystem::create_directories(argv[3]);if(std::string(argv[4])=="death"){regularDeath(c,saved.session);}else if(std::string(argv[4])=="landing"){landing(c,saved.session);}else if(std::string(argv[4])=="critical"){criticalAndGeometry(c,saved.session);}else if(std::string(argv[4])=="media"){sourceMedia(c,saved.session,argv[3]);}else if(std::string(argv[4])=="entry"){entryAndVictory(c,saved.session);}else if(std::string(argv[4])=="load"){publicOldLoad(c,read(argv[2]),argv[3]);}else if(std::string(argv[4])=="battle"){publicBattle(c,saved.session,argv[3]);}else{ordinaryCompatibility(c,saved,argv[3]);sourceRules(c,saved.session);entryAndVictory(c,saved.session);}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
