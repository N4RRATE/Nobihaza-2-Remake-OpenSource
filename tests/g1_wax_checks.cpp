#include "g1_natural_driver.h"
#include "hunter_behavior.h"
#include "enemy_controller.h"
#include <iostream>
#include "g1_hunter_rules.h"
void battle(Driver& run){
 int shots=0;const int before=run.game.session().variable(35);
 for(int n=0;n<12000&&!run.game.session().flag(270);++n){
  auto& game=run.game;const auto& s=game.session();InputFrame in;
  need(s.hp()>0,"Natural Hunter battle died");
  if(n%200==0){std::cout<<"battle "<<n<<" hp="<<s.hp()<<" damage="<<s.variable(89)<<" state="<<s.variable(91)<<" ammo="<<s.magazine(s.equipped(1))<<" raised="<<game.raised()<<'\n';}
  if(game.foreground()!=ForegroundKind::None||game.dialogue().active()){if(n%8==0){in=key();}}
  else if(!game.raised()){in=key(InputAction::Weapon);}
  else{const auto p=game.world().player().motion.tile(),e=findCharacter(game.world(),"m141-e2").motion.tile();const int dx=e.x-p.x,dy=e.y-p.y;const Direction facing=std::abs(dx)>std::abs(dy)?(dx>0?Direction::Right:Direction::Left):(dy>0?Direction::Down:Direction::Up);
   if(game.world().player().motion.facing()!=facing){in=key(InputAction::Weapon);in.movement=facing;}
   else if((s.variable(91)<=2||s.variable(91)==11)&&n%6==0){in=key(InputAction::Attack);++shots;}
  }
  run.tick(in);
 }
 need(run.game.session().flag(270)&&shots>0,"Natural Hunter battle did not win");need(run.game.session().variable(35)>=0,"CE11 count invalid");std::cout<<"REAL_HUNTER shots="<<shots<<" count35="<<before<<"->"<<run.game.session().variable(35)<<'\n';
 for(int attempt=0;attempt<3&&run.game.raised();++attempt){for(int n=0;n<24;++n){InputFrame held;held.buttons[int(InputAction::Weapon)].held=true;run.tick(held);}for(int n=0;n<100;++n){run.tick();}}
 need(!run.game.raised(),"Public source posture completion did not lower weapon");
 run.settle();
}
int main(int argc,char** argv){try{
 std::cout.setf(std::ios::unitbuf);
 need(argc==5,"03 package 02 save isolated-root natural|load|rules|shadow");auto c=loadCombatContent(argv[1]);auto root=std::filesystem::path(argv[3]);std::filesystem::create_directories(root);std::string mode=argv[4];
 if(mode=="natural"||mode=="shadow"){std::ofstream(root/"slot-01.json")<<read(argv[2]);}
 Driver run(c,root,"public-inputs-"+mode+".jsonl");run.game.configureSaveDirectory(root);run.load(mode=="load"||mode=="rules"?2:1);
 std::set<int> shadowMaps;
 if(mode=="shadow"){
  run.observe=[&](const PresentationEvent& e){
   if(e.kind!=PresentationEvent::Kind::SceneChanged||!run.game.session().flag(365)||run.game.session().flag(366)){return;}
   const int map=run.game.mapId();if(map!=134&&map!=139){return;}
   const auto expected=c.cue("m128-e5-p1-c1").at("sprite").get<std::string>();
   const auto actual=run.game.world().player().spriteId.value_or("");
   need(actual==expected,"Chase shadow on M"+std::to_string(map)+" was rendered as "+actual+", expected "+expected);
   need(run.game.world().player().motion.speed()==64,"Chase lost M128 route acceleration on M"+std::to_string(map));
   shadowMaps.insert(map);std::cout<<"SHADOW_FRAME map="<<map<<" sprite="<<actual<<" speed="<<run.game.world().player().motion.speed()<<'\n';
   const auto& player=run.game.world().player();const auto position=player.motion.position();
   std::ofstream(root/("shadow-frame-"+std::to_string(map)+".json"))<<Json{{"map",map},{"sprite",actual},{"speed",player.motion.speed()},{"tile",{player.motion.tile().x,player.motion.tile().y}},{"position",{position.x,position.y}},{"direction",directionName(player.graphicFacing())},{"column",player.animation.column()}}.dump(2)<<'\n';
  };
 }
 if(mode=="rules"){SaveFiles files(c,root);hunterchecks::rules(c,files.read(2),run);return 0;}
 if(mode=="load"){
  SaveFiles files(c,root);GameSession stored(c.items,files.read(2).session,c.facts);need(run.game.session().variable(35)==stored.variable(35)&&stored.variable(35)==11,"Independent Load changed real CE11 count");
  for(int f:{361,365,366,367,368,270}){need(run.game.session().flag(f),"Independent Load lost Hunter fact");}
  need(run.game.session().count(94)==1,"Independent Load lost I94");run.upstairs();run.actAt({5,5},Direction::Up);run.actAt({25,17},Direction::Up);need(run.game.mapId()==141,"Independent Load wax return failed");need(run.game.session().variable(89)==0&&findCharacter(run.game.world(),"m141-e2").motion.tile()==TilePosition{0,0},"Dead Hunter revived or transient damage persisted");std::cout<<"INDEPENDENT_G1_03_LOAD I94/death/facts/35/corpse passed\n";return 0;
 }
 need(run.game.mapId()==132&&!run.game.session().flag(365),"Old02 source did not adapt safely");run.upstairs();run.actAt({5,5},Direction::Up);run.actAt({25,17},Direction::Up);need(run.game.mapId()==141&&run.game.session().flag(361),"Natural wax intro did not finish");
 run.actAt({17,6},Direction::Up);need(run.game.mapId()==142,"Natural storeroom entry failed");run.actAt({10,7},Direction::Up);need(run.game.session().flag(365)&&run.game.session().count(94)==1&&!run.game.session().flag(366),"I94 pickup/chase timing failed");
 run.walk({9,10});run.settle();need(run.game.mapId()==142&&run.game.session().flag(366)&&!run.game.session().flag(367),"Natural chase did not return to142 with correct commits");need(std::find(run.maps.begin(),run.maps.end(),134)!=run.maps.end(),"Chase skipped original134");
 if(mode=="shadow"){
  need(shadowMaps==std::set<int>{134,139},"Chase shadow never crossed both source maps");
  const auto restored=c.cue("m139-e8-p1-c5").at("sprite").get<std::string>();
  need(run.game.world().player().spriteId==restored,"M139 C5 did not explicitly restore Nobita before M142");
  need(run.game.world().player().motion.speed()==32,"M139 final speed_down did not restore normal player speed");
  run.observe={};run.load();need(run.game.mapId()==132&&run.game.world().player().spriteId==restored&&run.game.world().player().motion.speed()==32&&!run.game.session().flag(365),"Fresh Load retained the destroyed chase actor graphic/speed");
  // The real source save wears I53 run shoes; remove them through the actual menu.
  run.tick(key(InputAction::Cancel));need(run.game.mode()==ApplicationMode::Equipment,"Slow walk setup menu absent");
  const auto select=[&](int id){auto view=run.game.menuView();for(std::size_t n=0;n<view.rows.size()&&run.game.menuView().rows.at(run.game.menuView().selection).id!=id;++n){run.tick(key(view.columns==2?InputAction::Right:InputAction::Down));}need(run.game.menuView().rows.at(run.game.menuView().selection).id==id,"Slow walk setup row absent");run.tick(key());};
  select(2);select(4);select(0);run.tick(key(InputAction::Cancel));run.tick(key(InputAction::Cancel));
  need(run.game.mode()==ApplicationMode::Playing&&run.game.session().equipped(4)==0,"Run shoes remained equipped in slowdown probe");
  run.walk({10,8});for(int n=0;n<100&&!run.game.raised();++n){run.tick(key(InputAction::Weapon));}for(int n=0;n<100;++n){run.tick();}
  need(run.game.raised(),"Weapon slowdown probe never raised its weapon");
  InputFrame locked;locked.buttons[static_cast<std::size_t>(InputAction::Cancel)].held=true;run.tick(locked);
  need(run.game.session().flag(3)&&run.game.world().player().motion.speed()<32,"Weapon slowdown probe never entered slow walk");
  locked.movement=Direction::Up;for(int n=0;n<300&&run.game.mapId()==132;++n){run.tick(locked);}run.settle();
  need(run.game.mapId()==133&&!run.game.raised()&&!run.game.session().flag(3)&&run.game.world().player().motion.speed()==32&&run.game.world().player().spriteId==restored,"Story transfer retained temporary weapon graphic/slow walk");
  std::cout<<"NATURAL_G1_SHADOW hunterB2/explicitRestore/freshLoad/weaponSlowWalkCleanup passed ticks="<<run.ticks<<'\n';return 0;
 }
 run.actAt({9,10},Direction::Down);need(run.game.mapId()==141,"Natural return to wax failed");run.walk({9,6},[&]{return run.game.session().flag(367);});run.settle();need(run.game.session().flag(367)&&run.game.session().flag(368)&&!run.game.session().flag(270),"Hunter intro did not complete");
 battle(run);run.actAt({4,9},Direction::Down);need(run.game.mapId()==139,"Wax exit failed");run.actAt({3,14},Direction::Down);run.actAt({27,13},Direction::Right);run.walk({12,8});run.settle();need(run.game.mapId()==132,"Hunter victory did not reach hall");run.save();SaveFiles files(c,root);auto disk=files.read(2);need(disk.session.worldFlags.at(c.facts.switches.at(270))&&disk.session.inventory.at(94)==1,"Save lost victory/key");
 std::cout<<"NATURAL_G1_03 ticks="<<run.ticks<<" maps="<<Json(run.maps)<<" I94/deferredChase/hunter/save passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
