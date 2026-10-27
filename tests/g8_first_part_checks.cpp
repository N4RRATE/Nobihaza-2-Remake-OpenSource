#include "title_navigation.h"
#include "game_application.h"
#include "enemy_controller.h"
#include "enemy_host.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <fstream>
#include "full_content.h"
#include "g0_natural_driver.h"
#include <stdexcept>
namespace {
void need(bool ok,const char* why){if(!ok){throw std::runtime_error(why);}}
void acidGeometry(const CombatContent& c,bool matrix=false){
 const std::vector<std::string> ids=matrix?std::vector<std::string>{"m73-e4","m74-e4","m74-e7","m106-e7","m111-e9","m111-e12","m112-e7"}:std::vector<std::string>{"m106-e7"};
 const std::vector<std::array<int,2>> offsets=matrix?std::vector<std::array<int,2>>{{16,16},{32,-32},{18,0},{19,0},{-18,0},{-19,0},{0,18},{0,19}}:std::vector<std::array<int,2>>{{16,16}};
 for(const auto& difficulty:matrix?std::vector<std::string>{"easy","normal","hard"}:std::vector<std::string>{"normal"}){for(const auto& id:ids){for(const auto offset:offsets){
  const auto& d=c.enemies.at(id);const auto& profile=d.rules.at("runtime_profile");auto e=c.entries.at("post-pickup");e.map=d.map;e.playerTile={12,6};e.positions.clear();e.directions.clear();e.pages.clear();e.through.clear();e.session.difficulty=difficulty;e.session.hp=e.session.maxHp=1000;e.session.baseDefense=0;e.session.armor=e.session.accessory=e.session.otherEquipment=0;e.session.conditions={};
  GameSession state(c.items,e.session,c.facts);for(int flag:{13,15,18,21,23,585}){state.setFlag(flag,false);}state.setFlag(d.deadFlag,false);int poisonRolls=0;
  CombatScene scene(c,state,e,[&](int lo,int hi){if(lo==0&&hi==10){++poisonRolls;}return hi;});auto spider=makeEnemyBehavior(scene,d);state.setFlag(profile.at("acid").at("clear_attack_fact"),true);spider->updateParallel();spider->updateParallel();
  EnemyHost acid(scene,profile.at("acid_source_object"));auto actor=acid.actor();const int dx=offset[0],dy=offset[1];actor.motion=CharacterMotion({12+dx/16,6+dy/16},Direction::Right,std::max(1,16*std::max(std::abs(dx%16),std::abs(dy%16))));
  if(dx%16){actor.motion.beginStep(dx>0?Direction::Right:Direction::Left);actor.motion.advance();}else if(dy%16){actor.motion.beginStep(dy>0?Direction::Down:Direction::Up);actor.motion.advance();}
  acid.setActor(actor);const int before=state.hp();spider->updateParallel();const auto events=scene.takePresentation();need(std::any_of(events.begin(),events.end(),[&](const auto& event){return event.id==profile.at("acid").at("impact_cues").at(1).get<std::string>();}),"Controlled acid must reach real landing presentation");
  const bool rectangle=id=="m106-e7"||id=="m111-e9"||id=="m112-e7";const bool expected=rectangle?std::abs(dx)<=18&&std::abs(dy)<=18:std::abs(dx+dy)<=18;
  if((state.hp()<before)!=expected||state.conditions().poisoned!=expected||poisonRolls!=int(expected)){std::cerr<<"acid "<<id<<" "<<difficulty<<" dx="<<dx<<" dy="<<dy<<" hp="<<state.hp()<<" rolls="<<poisonRolls<<"\n";throw std::runtime_error("Original acid landing geometry/poison boundary differs");}
  for(int n=0;n<100;++n){spider->updateParallel();}need(!state.flag(profile.at("acid").at("clear_attack_fact")),"Acid miss/hit must clear its own source slot");
 }}}
 std::cout<<"Acid real landing/poison/cleanup shapes PASS\n";
}
void guardFeeding(const CombatContent& c){
 for(const auto& difficulty:{"easy","normal","hard"}){for(bool agility:{false,true}){
 auto e=c.entries.at("defeated-claimed");e.map=72;e.session.difficulty=difficulty;e.playerTile={14,12};e.positions.clear();e.directions.clear();e.pages.clear();e.through.clear();e.positions["m72-e6"]={10,9};e.positions["m72-e7"]={10,7};e.directions["m72-e6"]=Direction::Up;e.session.flags[18]=true;e.session.flags[223]=false;e.session.variables[261]=5;e.session.variables[247]=30;e.session.variables[91]=100;e.session.variables[89]=0;e.session.hp=e.session.maxHp=1000;e.session.conditions={};
 for(const auto& row:c.full->at("runtime").at("parking").at("first_guard").at("runtime_profile").at("slots")){e.positions[row.at("object").get<std::string>()]={24,20};}e.positions["m72-e7"]={10,7};e.positions["m72-e9"]={10,5};
 GameSession state(c.items,e.session,c.facts);state.setVariable(105,2);state.setVariable(103,0);state.setVariable(261,5);state.setVariable(247,30);state.setVariable(91,100);state.setVariable(89,0);state.setFlag(223,agility);state.setFlag(18,true);state.setFlag(224,false);state.setFlag(225,false);CombatScene scene(c,state,e,[](int lo,int){return lo;});
 scene.update({});state.setFlag(223,agility);for(int n=1;n<35;++n){scene.update({});}
 const auto views=scene.enemyStates();const auto target=std::find_if(views.begin(),views.end(),[](const auto& v){return v.id=="m72-e7";});need(target!=views.end(),"Missing real feeding guard slot");
 need(state.variable(247)<30,"Feeding gate test must fire real NPC projectile");need((target->damage>0)==agility,"Feeding NPC P7 requires S223 for damage");const auto beyond=std::find_if(views.begin(),views.end(),[](const auto& v){return v.id=="m72-e9";});need(beyond!=views.end(),"Missing legal target beyond feeding guard");if(!agility){need(beyond->damage>0,"Rejected feeding receiver must let real NPC projectile reach legal target beyond");}
 }}std::cout<<"NPC feeding projectile S223 rejection/acceptance in three difficulties PASS\n";
}
void blueprintVariants(const CombatContent& c){
 const std::array<std::string,8> ids{"m19-e2","m21-e3","m21-e5","m21-e7","m26-e4","m78-e4","m99-e4","m99-e6"};
 for(int level=0;level<3;++level){const auto difficulty=std::array<std::string,3>{"easy","normal","hard"}[level];for(const auto& id:ids){
  const auto& d=c.expansion.enemies.at(id);const bool licker=d.behavior==EnemyBehaviorKind::Licker;const int defense=licker?std::array{5,9,15}[level]:id=="m21-e7"?std::array{3,8,10}[level]:std::array{1,3,8}[level];const int attack=licker?std::array{9,15,22}[level]:std::array{7,10,20}[level];
  auto e=c.entries.at("post-pickup");e.map=d.map;e.playerTile={10,9};e.direction=Direction::Up;e.positions.clear();e.directions.clear();e.pages.clear();e.through.clear();e.positions[id]={10,8};e.directions[id]=Direction::Down;e.session.difficulty=difficulty;e.session.hp=e.session.maxHp=1000;e.session.baseDefense=0;e.session.armor=e.session.accessory=e.session.otherEquipment=0;e.session.conditions={};e.session.flags[1]=e.session.flags[18]=true;e.session.flags[13]=e.session.flags[15]=false;e.session.flags[d.deadFlag]=false;e.session.variables[35]=0;if(d.activationFlag){e.session.flags[*d.activationFlag]=true;}
  for(const auto& [other,row]:c.expansion.enemies){if(row.map==d.map&&other!=id){e.session.flags[row.deadFlag]=true;}}
  std::cerr<<"BP fixture "<<id<<" "<<difficulty<<" construct\n";GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e,[](int lo,int){return lo;});std::cerr<<"BP controller\n";auto controller=makeEnemyBehavior(scene,d);if(licker){controller->activateFromEncounter();}const auto p=findCharacter(scene.world(),id).motion.position();const auto hit=controller->receiveAttack({{(p.x+.5)*16,(p.y+.5)*16},{512,512},AttackKind::Handgun,2});
  std::cerr<<"BP receiver complete\n";need(hit.processed,"Blueprint actual receiver missing");if(state.variable(49)!=defense){std::cerr<<"Blueprint "<<id<<" "<<difficulty<<" defense="<<state.variable(49)<<" expected="<<defense<<"\n";throw std::runtime_error("Blueprint must consume selected original Actor defense");}
  GameSession incoming(c.items,e.session,c.facts);CombatScene battle(c,incoming,e,[](int lo,int){return lo;});bool saw=false;
  for(int n=0;n<2000&&!saw;++n){battle.update({});battle.takePresentation();saw=incoming.hp()<1000;}
  need(saw,"Blueprint real movement/attack did not reach original bite/claw");need(1000-incoming.hp()==std::max(1,attack-5),"Blueprint original bite/claw attack must follow selected Actor");
 }}std::cout<<"Eight actual blueprint receiver/attack consumers in three difficulties PASS\n";
 if(c.catalog.at("difficulty").contains("enemy_rules")){int definitions=0,blueprints=0;const auto& maps=c.catalog.at("difficulty").at("support").at("world").at("easy");const auto supported=[&](int map){return std::find(maps.begin(),maps.end(),map)!=maps.end();};
  for(const auto& [id,d]:c.enemies){if(supported(d.map)){need(c.catalog.at("difficulty").at("enemy_rules").contains(id),"Unbound instantiated definition");++definitions;}}
  for(const auto& [id,d]:c.expansion.enemies){if(supported(d.map)){need(c.catalog.at("difficulty").at("enemy_rules").contains(id),"Unbound instantiated blueprint");++blueprints;}}
  std::cout<<"Actual factory region coverage definitions="<<definitions<<" blueprints="<<blueprints<<"; dedicated Guard enemy slots/dog and Crocodile are source-bound separately\n";
 }
}
void companionDog(const CombatContent& c){
 for(const auto& difficulty:{"easy","normal","hard"}){
  auto e=c.entries.at("post-pickup");e.map=117;e.playerTile={10,9};e.session.difficulty=difficulty;e.positions.clear();e.directions.clear();e.pages.clear();e.through.clear();e.session.flags[1]=e.session.flags[18]=e.session.flags[13]=e.session.flags[222]=true;e.session.flags[341]=false;e.session.hp=e.session.maxHp=5000;
  for(int v=240;v<=269;++v){e.session.variables[v]=0;}e.session.variables[261]=11;e.session.variables[247]=13;
  const auto& slots=c.catalog.at("guard_second").at("runtime_profile").at("slots");for(const auto& slot:slots){e.positions[slot.at("object").get<std::string>()]={35,13};e.session.flags[slot.at("dead_fact").get<int>()]=false;e.session.variables[slot.at("source_slots").at("damage").get<int>()]=0;e.session.variables[slot.at("source_slots").at(slot.value("type",std::string{})=="dog"?"state":"mode").get<int>()]=0;}
  e.positions["m117-e6"]={10,9};e.positions["m117-e22"]={10,8};e.directions["m117-e6"]=Direction::Up;GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e,[](int lo,int){return lo;});bool damaged=false;
  for(int n=0;n<50;++n){scene.update({});for(const auto& enemy:scene.enemyStates()){damaged|=enemy.id=="m117-e22"&&enemy.damage>0;}}
  need(damaged&&state.variable(269)==0,"Real NPC bat must retain its dog damage without player quota");
 }std::cout<<"NPC dog receiver retains real damage in three difficulties PASS\n";
}
void easyPresets(const CombatContent& c){
 for(const auto& id:{"m39-e10","m39-e14","m44-e2","m81-e4"}){
  const auto& definition=c.enemies.at(id);auto e=c.entries.at("post-pickup");e.map=definition.map;e.playerTile={1,1};e.positions.clear();e.directions.clear();e.pages.clear();e.through.clear();e.session.difficulty="easy";e.session.conditions={};
  for(int flag:c.catalog.at("difficulty").at("easy_preseed")){e.session.flags[flag]=true;}e.session.flags[1]=e.session.flags[18]=false;
  GameSession state(c.items,e.session,c.facts);const auto before=state.exportState();CombatScene scene(c,state,e);for(int n=0;n<20;++n){scene.update({});}
  const auto a=findCharacter(scene.world(),id);need(!a.active&&a.through&&a.motion.tile()==TilePosition{0,0},"EASY preset must rebuild original absent actor");
  const auto views=scene.enemyStates();const auto view=std::find_if(views.begin(),views.end(),[&](const auto& v){return v.id==id;});need(view!=views.end()&&view->mode==EnemyMode::Corpse&&view->damage==0,"EASY preset may not run a fake death");
  const auto& object=c.objects.at(id);need(c.activeVariant(object,state)->page==(std::string(id)=="m81-e4"?21:20),"EASY preset selects original terminal page");
  need(state.exportState().inventory==before.inventory&&state.variable(35)==before.variables.at(35),"EASY preset may not add loot or combat hit count");
  std::set<std::string> hit;need(scene.resolveAttack({{8,8},{0,0},AttackKind::Handgun,2},hit).processed==0,"EASY absent actors must not retain combat contact");
 }
 auto e=c.entries.at("post-pickup");e.map=48;e.playerTile={1,1};e.session.difficulty="easy";e.positions.clear();e.pages.clear();e.directions.clear();e.through.clear();e.session.flags[130]=true;e.session.flags[134]=false;e.session.variables[145]=0;
 GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e);const auto* page=c.activeVariant(c.objects.at("m48-e2"),state);need(page->page!=21&&page->page!=22&&findCharacter(scene.world(),"m48-e2").active,"M48 S130 without damage must not delete an eleventh enemy");
 std::cout<<"EASY four original preset actors and M48 cross-map guard PASS\n";
}
void rescueVariants(const CombatContent& c){
 for(const auto& difficulty:{"easy","normal","hard"}){
  auto e=c.entries.at("post-pickup");e.map=42;e.playerTile={1,1};e.positions.clear();e.pages.clear();e.directions.clear();e.through.clear();e.session.difficulty=difficulty;e.session.flags[1]=e.session.flags[18]=false;e.session.flags[181]=true;e.session.flags[193]=e.session.flags[498]=e.session.flags[401]=false;
  for(int fact:{176,177,182,183,184,185,186}){e.session.flags[fact]=true;}
  GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e);scene.update({});need(state.flag(193)==(std::string(difficulty)=="easy")&&!state.flag(401),"Original seven/eight clearance sets must not fake S401");
  if(std::string(difficulty)!="easy"){bool arrival=false;for(int n=0;n<425&&!state.flag(498);++n){scene.update({});for(const auto& enemy:scene.enemyStates()){arrival|=enemy.id=="m42-e21"&&enemy.sourceState==200;}}need(state.flag(498)&&arrival,"NORMAL/HARD original delay must activate mode200 reinforcement");state.setFlag(401,true);scene.update({});need(state.flag(193),"NORMAL/HARD clearance requires actual eighth death fact");}
  else{for(int n=0;n<425;++n){scene.update({});}need(!state.flag(498)&&!state.flag(401),"EASY clearance never fabricates reinforcement or its death");}
 }
 std::cout<<"Three-difficulty rescue original clearance and reinforcement PASS\n";
}
void corridorDoors(const CombatContent& c){
 bool sawMovement=false;int routes=0;
 const std::array<std::string,4> inputs{"m39-e16","m45-e6","m46-e2","m97-e2"};const std::array<TilePosition,4> landings{{{27,24},{27,5},{19,5},{7,14}}};
 const std::array<TilePosition,4> origins{{{23,4},{10,10},{9,11},{3,8}}};
 const std::array<int,4> destinations{45,46,39,97};const std::array<int,4> exits{2,4,5,6};
 auto fixture=[&](int map,TilePosition tile,const std::string& difficulty,bool keyHistory){auto e=c.entries.at("post-pickup");e.map=map;e.playerTile=tile;e.direction=Direction::Up;e.positions.clear();e.directions.clear();e.pages.clear();e.through.clear();e.session.difficulty=difficulty;e.session.hp=e.session.maxHp=5000;e.session.baseDefense=1000;e.session.conditions={};return e;};
 const auto flags=[&](GameSession& s,bool history){for(int f:{1,18,174,175,303}){s.setFlag(f,true);}s.setFlag(13,false);s.setFlag(15,false);s.setFlag(305,history);for(const auto& [id,d]:c.enemies){if(d.map==138){s.setFlag(d.deadFlag,false);}}};
 for(const auto& difficulty:{"easy","normal","hard"}){for(bool history:{false,true}){for(int entrance=0;entrance<4;++entrance){
  const auto& object=c.objects.at(inputs[entrance]);const int expected=(entrance==3||history)&&std::string(difficulty)!="easy"?138:41;auto e=fixture(object.map,origins[entrance],difficulty,history);e.direction=object.tile.x!=e.playerTile.x?(object.tile.x>e.playerTile.x?Direction::Right:Direction::Left):(object.tile.y>e.playerTile.y?Direction::Down:Direction::Up);GameSession s(c.items,e.session,c.facts);flags(s,history);CombatScene origin(c,s,e,[](int lo,int){return lo;});
  for(int n=0;n<600&&origin.mapId()==object.map;++n){InputFrame input;if(n%12==0){input=g0natural::key();}origin.update(input);}
  if(origin.mapId()!=expected||origin.world().player().motion.tile()!=landings[entrance]){std::cerr<<"entry "<<inputs[entrance]<<" "<<difficulty<<" history="<<history<<" map="<<origin.mapId()<<"\n";throw std::runtime_error("Original corridor entry map/landing differs");}
  for(int exit=0;exit<4;++exit){
   auto start=fixture(expected,landings[entrance],difficulty,history);GameSession state(c.items,start.session,c.facts);flags(state,history);CombatScene scene(c,state,start,[](int lo,int){return lo;});const auto door="m"+std::to_string(expected)+"-e"+std::to_string(exits[exit]);const auto target=c.objects.at(door).tile;std::map<std::string,TilePosition> before;for(const auto& [id,d]:c.enemies){if(d.map==expected){before[id]=findCharacter(scene.world(),id).motion.tile();}}
   for(int n=0;n<10000&&scene.mapId()==expected;++n){InputFrame in;
    if(scene.foreground()!=ForegroundKind::None||scene.dialogue().active()){if(n%12==0){in=g0natural::key();}}
    else if(!scene.world().player().motion.moving()){
     const auto player=scene.world().player().motion.tile();bool found=false;
     for(auto side:{Direction::Down,Direction::Left,Direction::Right,Direction::Up}){const auto goal=adjacentTile(target,side);if(player==goal){const auto facing=static_cast<Direction>((int(side)+2)%4);if(scene.world().player().motion.facing()!=facing){in.movement=facing;}else{in=g0natural::key();}found=true;break;}if(const auto step=g0natural::next(scene.world(),goal)){in.movement=step;found=true;break;}}
     if(!found&&n%60==0){in=g0natural::key(InputAction::Left);}
    }
    if(state.flag(10)){in.buttons[static_cast<int>(InputAction::Left)].pressed=true;}scene.update(in);
    if(scene.mapId()==expected){for(const auto& [id,tile]:before){sawMovement|=findCharacter(scene.world(),id).motion.tile()!=tile;}}
   }
   if(scene.mapId()!=destinations[exit]){std::cerr<<"path map="<<expected<<" entry="<<entrance<<" exit="<<door<<" actual="<<scene.mapId()<<" difficulty="<<difficulty<<" history="<<history<<" at="<<scene.world().player().motion.tile().x<<","<<scene.world().player().motion.tile().y<<"\n";throw std::runtime_error("Live collision corridor did not reach original return door");}
   for(const auto& [id,tile]:before){need(!state.flag(c.enemies.at(id).deadFlag),"Corridor path must not fake an enemy death");}++routes;
  }
 }}}
 need(sawMovement,"M138 reachability must include real moving enemies");std::cout<<"Four corridor entries and "<<routes<<" live occupied return paths PASS\n";
}
int reloadAction(const CombatContent& c,int weapon,const std::string& difficulty,bool bag,bool cancel=false){
 auto e=c.entries.at("post-pickup");e.map=22;e.playerTile={10,5};e.direction=Direction::Up;e.positions.clear();e.directions.clear();e.pages.clear();e.through.clear();e.session.difficulty=difficulty;if(weapon==27){e.session.auxiliaryWeapon=weapon;}else{e.session.mainWeapon=weapon;}e.session.magazines[weapon]=0;e.session.inventory[weapon]=1;e.session.inventory[51]=1;if(c.items.at(weapon).boxItem>0){e.session.inventory[c.items.at(weapon).boxItem]=1;}e.session.inventory[c.items.at(weapon).looseItem]=c.items.at(weapon).boxItem>0?0:2;
 GameSession s(c.items,e.session,c.facts);s.setFlag(1,true);s.setFlag(18,true);s.setFlag(13,false);s.setFlag(15,false);if(bag){need(s.equip(51,c.items.at(51).equipmentType),"Reload test I51 equip rejected");}CombatScene scene(c,s,e);scene.update(g0natural::key(weapon==27?InputAction::AuxiliaryWeapon:InputAction::Weapon));for(int n=0;n<100;++n){scene.update({});}need(scene.raised(),"Source weapon did not raise before reload");scene.update(g0natural::key(InputAction::Attack));const int full=c.items.at(weapon).capacity;need(s.magazine(weapon)==full,"Reload must commit ammo before first wait");
 for(int n=1;n<500;++n){if(cancel&&n==1){s.setFlag(17,false);}scene.update(g0natural::key(InputAction::Attack));if(s.magazine(weapon)<full){return n;}}
 throw std::runtime_error("Source reload did not release weapon input");
}
void reloadActions(const CombatContent& c,bool sourceOnly=false){
 if(sourceOnly){need(reloadAction(c,11,"normal",false,true)<reloadAction(c,11,"normal",false),"Closing S17 after first wait must cancel the source reload without refund");return;}
 for(int weapon:{11,12,13,14,15,16,17,18,19,20,21,56}){
  const int easy=reloadAction(c,weapon,"easy",false),easyBag=reloadAction(c,weapon,"easy",true),normal=reloadAction(c,weapon,"normal",false),normalBag=reloadAction(c,weapon,"normal",true),hard=reloadAction(c,weapon,"hard",false),hardBag=reloadAction(c,weapon,"hard",true);
  need(easy==easyBag&&normal==hard&&normalBag==hardBag,"Difficulty/accessory reload precedence differs");need(weapon==19?easy<normal:easy==normalBag,"EASY must use original complete per-weapon path");
  need(reloadAction(c,weapon,"easy",false,true)<easy,"Source S17 cancellation after first wait must release weapon early");std::cout<<"reload "<<weapon<<" E="<<easy<<" N/H="<<normal<<" I51="<<normalBag<<"\n";
 }
 const int lc9=reloadAction(c,27,"normal",false);
 for(const auto& difficulty:{"easy","normal","hard"}){for(bool bag:{false,true}){need(reloadAction(c,27,difficulty,bag)==lc9,"LC9 must retain its fixed CE119 path across difficulty/accessory");}}
 std::cout<<"LC9 fixed reload "<<lc9<<" ticks PASS\n";
 std::cout<<"Per-action difficulty reload and S17 cancellation PASS\n";
}
void reloadResources(const CombatContent& c){
 for(const auto& difficulty:{"easy","normal","hard"}){for(const auto row:{std::array{0,0,1,7,23,0},std::array{6,0,1,7,29,0},std::array{0,1,1,7,26,0},std::array{6,1,1,7,0,1}}){
  auto e=c.entries.at("post-pickup").session;e.difficulty=difficulty;e.inventory[15]=1;e.mainWeapon=15;e.inventory[33]=row[1];e.inventory[34]=row[2];e.magazines[15]=row[0];GameSession state(c.items,e,c.facts);const int other=state.magazine(14);need(state.reload()==row[3]-row[0]&&state.magazine(15)==row[3]&&state.count(33)==row[4]&&state.count(34)==row[5]&&state.magazine(14)==other,"I15 original pure-box/mixed resource branch differs");const auto full=state.exportState();need(state.reload()==0&&state.exportState()==full,"Full I15 reload must reject without transfer");
 }}std::cout<<"I15 three-difficulty original resource branches PASS\n";
}
void loaded(const CombatContent& c,const std::filesystem::path& path,const std::string& difficulty){
 std::ifstream file(path);const std::string bytes{std::istreambuf_iterator<char>(file),{}};auto expected=prepareRestore(c,decodeSave(c,bytes));GameApplication game(c,"title");game.configureSaveDirectory(path.parent_path());finishTitleOpening(game);InputFrame key;key.buttons[static_cast<int>(InputAction::Confirm)]={true,true,false};game.update(key);need(game.mode()==ApplicationMode::Loading,"Independent Load menu missing");game.update(key);need(game.hasGame()&&game.session().difficulty()==difficulty&&!game.openingActive()&&game.mapId()==expected.scene->mapId()&&game.session().exportState()==expected.session->exportState(),"Independent ordinary Load changed source difficulty/state");std::cout<<"Independent ordinary Load "<<difficulty<<" map="<<game.mapId()<<" count="<<game.session().saveCount()<<" PASS\n";
}
void savePageGates(const CombatContent& c){
 for(const auto& difficulty:{"easy","normal","hard"}){for(bool unlocked:{false,true}){
  auto e=c.entries.at("post-pickup");e.map=77;const auto tile=c.objects.at("m77-e6").tile;e.playerTile={tile.x,tile.y+1};e.direction=Direction::Up;e.positions.clear();e.pages.clear();e.directions.clear();e.through.clear();e.session.difficulty=difficulty;e.session.flags[1]=e.session.flags[18]=e.session.flags[227]=true;e.session.flags[585]=unlocked;e.session.flags[13]=e.session.flags[15]=false;
  GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e);need(c.activeVariant(c.objects.at("m77-e6"),state)->page==(unlocked?4:3),"M77 original highest-page gate differs");std::optional<SaveRequest> request;
  for(int n=0;n<300&&!request;++n){scene.update(n%12==0?g0natural::key():InputFrame{});request=scene.takeSaveRequest();}need(bool(request)==unlocked,"M77 original blank P3/high P4 must gate actual save request");
  if(!unlocked){bool rejected=false;try{captureSave(c,scene,{scene.generation(),"m77-e6",""},{});}catch(const std::exception&){rejected=true;}need(rejected,"Unavailable source save page must reject synthetic capture");}
 }}std::cout<<"M77 original P3/P4 save gate in three difficulties PASS\n";
}
void checkpointLoads(CombatContent c,const std::filesystem::path& root){
 GameApplication game(std::move(c),"title");const auto& content=game.content();
 for(const auto& difficulty:{"easy","normal","hard"}){for(int map:{16,22,77,79,107}){
  const auto final=root/(std::string("natural-")+difficulty+"-final");const auto directory=(std::filesystem::exists(final)?final:root/(std::string("natural-")+difficulty))/"checkpoints";std::filesystem::path chosen;int highest=-1;
  for(const auto& file:std::filesystem::directory_iterator(directory)){const auto name=file.path().filename().string(),prefix="map-"+std::to_string(map)+"-count-";if(name.starts_with(prefix)&&name.ends_with(".json")){const int count=std::stoi(name.substr(prefix.size()));if(count>highest){highest=count;chosen=file.path();}}}need(highest>=0,"Missing real natural checkpoint for save-point Load matrix");
  const auto isolated=root/"independent-loads"/difficulty/("map-"+std::to_string(map));std::filesystem::create_directories(isolated);std::filesystem::copy_file(chosen,isolated/"slot-01.json",std::filesystem::copy_options::overwrite_existing);
  std::ifstream input(chosen);const std::string bytes{std::istreambuf_iterator<char>(input),{}};const auto snapshot=decodeSave(content,bytes);auto expected=prepareRestore(content,snapshot);const std::map<int,std::string> objects{{16,"m16-e5"},{22,"m22-e2"},{77,"m77-e6"},{79,"m79-e6"},{107,"m107-e3"}};
  const auto point=std::find_if(content.catalog.at("save_points").begin(),content.catalog.at("save_points").end(),[&](const auto& p){return p.at("object")==objects.at(map);});need(point!=content.catalog.at("save_points").end(),"Missing reviewed save point");std::string name=difficulty;std::transform(name.begin(),name.end(),name.begin(),[](unsigned char ch){return std::toupper(ch);});need(snapshot.summary.label==point->at("slot_label_by_difficulty").at(name).get<std::string>(),"Real saved slot must retain exact original difficulty label");
  game.returnToTitle();finishTitleOpening(game);game.configureSaveDirectory(isolated);finishTitleOpening(game);for(int n=0;n<3&&game.menuSelection()!=1;++n){game.update(g0natural::key(InputAction::Down));}game.update(g0natural::key());need(game.mode()==ApplicationMode::Loading,"Independent title Load missing");const auto menu=game.fileMenuView();need(menu&&menu->summaries.at(0)&&menu->summaries.at(0)->label==snapshot.summary.label,"File menu must show original saved difficulty label");game.update(g0natural::key());need(game.hasGame()&&game.session().difficulty()==difficulty&&!game.openingActive()&&game.mapId()==map&&game.world().player().motion.tile()==expected.scene->world().player().motion.tile()&&game.session().exportState()==expected.session->exportState(),"Public Load changed ordinary source state, tile or difficulty");
  std::cout<<"Independent ordinary Load "<<difficulty<<" map="<<map<<" count="<<highest<<" PASS\n";
 }}
}
void firstEnemy(const CombatContent& c){
 for(const auto& difficulty:{"easy","normal","hard"}){
  auto e=c.entries.at("post-pickup");e.session.difficulty=difficulty;e.session.hp=e.session.maxHp=1000;e.session.baseAttack=20;e.session.conditions={};e.session.flags[18]=e.session.flags[52]=true;e.session.flags[101]=false;e.session.variables[35]=0;
  GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,[](int lo,int){return lo;});const auto p=findCharacter(scene.world(),"m4-e4").motion.position();std::set<std::string> resolved;
  const auto hit=scene.resolveAttack({{(p.x+.5)*16,(p.y+.5)*16},{0,0},AttackKind::Handgun,2,0,false,1,AttackPhase::Impact},resolved);
  need(hit.processed==1,"First enemy public hit missing");need(s.variable(49)==(std::string(difficulty)=="easy"?3:std::string(difficulty)=="hard"?10:8),"M4 Actor21 defense must follow selected difficulty");
 }
}
}
int main(int argc,char** argv){try{std::cout.setf(std::ios::unitbuf);need(argc>=2,"Expected package");auto c=loadCombatContent(argv[1]);
if(argc==3&&std::string(argv[2])=="--dump"){
 nlohmann::json out;for(const auto& [id,e]:c.enemies){if(e.map<=112||e.map==138){out["enemies"][id]={{"map",e.map},{"story",e.story},{"dead",e.deadFlag},{"rules",e.rules}};}}
 out["rules"]=c.rules;out["catalog"]=c.catalog;out["full"]=c.resolvedFullDefinition();out["branch"]=c.branch;out["actions"]=c.actionProfiles;out["interactions"]=c.interactions;out["transitions"]=c.transitions;std::cout<<out.dump(2)<<'\n';return 0;}
if(argc==4&&std::string(argv[2])=="--acceptance"){blueprintVariants(c);easyPresets(c);rescueVariants(c);guardFeeding(c);companionDog(c);reloadActions(c);savePageGates(c);checkpointLoads(std::move(c),argv[3]);return 0;}
if(argc==4&&std::string(argv[2])=="--checkpoint-loads"){savePageGates(c);checkpointLoads(std::move(c),argv[3]);return 0;}
if(argc==5&&std::string(argv[2])=="--load"){loaded(c,argv[3],argv[4]);return 0;}
if(argc==3&&std::string(argv[2])=="--blueprints"){blueprintVariants(c);return 0;}
if(argc==3&&std::string(argv[2])=="--extended"){blueprintVariants(c);easyPresets(c);rescueVariants(c);guardFeeding(c);companionDog(c);reloadActions(c);savePageGates(c);return 0;}
if(argc==3&&std::string(argv[2])=="--complete"){firstEnemy(c);reloadResources(c);acidGeometry(c,true);guardFeeding(c);reloadActions(c);corridorDoors(c);return 0;}
if(argc==3&&std::string(argv[2])=="--reload-source"){reloadActions(c,true);return 0;}
if(argc==3&&std::string(argv[2])=="--reload"){reloadActions(c);return 0;}
if(argc==3&&std::string(argv[2])=="--corridors"){corridorDoors(c);return 0;}
if(argc==3&&std::string(argv[2])=="--all"){firstEnemy(c);reloadResources(c);acidGeometry(c,true);guardFeeding(c);return 0;}
if(argc==3&&std::string(argv[2])=="--acid"){acidGeometry(c);return 0;}if(argc==3&&std::string(argv[2])=="--guard"){guardFeeding(c);return 0;}
firstEnemy(c);std::cout<<"G8-02 first enemy difficulty behavior PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
