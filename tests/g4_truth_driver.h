#pragma once
#include "g2_natural_driver.h"
#include <random>
// Shared original public-input battles; rendering remains a caller-owned observer.
namespace g4truth {
inline void enter(G2NaturalDriver& r,bool cycle=false){r.load();if(cycle){
 // Cash follows the locked Dora/truth flow; load its real empty I20 in the safe room.
 r.equip(20);const auto& ammo=r.game.content().items.at(20);const int rounds=r.game.session().magazine(20),reserve=r.game.session().count(ammo.looseItem)+r.game.session().count(ammo.boxItem)*ammo.boxRounds;
 if(!rounds){need(reserve>0&&!r.game.session().flag(507),"Cycle safe room I20 actual reserve absent");r.tick(key(InputAction::Weapon));for(int n=0;n<60;++n){r.tick();}need(r.game.raised(),"Cycle safe room public raise failed");r.tick(key(InputAction::Attack));for(int n=0;n<600;++n){r.tick();}r.lowerForMenu(r.ticks);const int loaded=r.game.session().magazine(20),remaining=r.game.session().count(ammo.looseItem)+r.game.session().count(ammo.boxItem)*ammo.boxRounds;need(loaded>0&&reserve-remaining==loaded-rounds,"Cycle safe room public reload did not transfer actual reserve");std::cout<<"Cycle safe room actual I20 reload "<<rounds<<"->"<<loaded<<" reserve="<<reserve<<"->"<<remaining<<" step="<<r.ticks<<'\n';}
 }r.reception();r.actObject("m159-e6");r.walk({14,30});InputFrame in;in.movement=Direction::Up;r.tick(in);for(int n=0;n<30000&&(r.game.mapId()!=192||r.game.foreground()!=ForegroundKind::None);++n){r.tick(n%8==0?key():InputFrame{});}need(r.game.mapId()==192&&r.game.session().flag(507)&&r.game.session().flag(1)&&r.game.session().flag(18),"Actual intro did not unlock Dora");}
// Reuse G4-02's verified public route: actual loss/219 followed by persuasion,
// with the original disk's inventory and HP. No victory or entry facts are injected.
inline void winDora(G2NaturalDriver& r,bool cycle=false){
 enter(r,cycle);

 if(!cycle){const auto original=r.game.session().exportState();const int beforeContinue=r.game.session().variable(630);
 for(int n=0;n<10000&&r.game.session().hp()>0;++n){r.tick();}need(r.game.session().hp()==0,"Natural death did not occur");
 for(int n=0;n<5000&&!r.game.inputPromptView().cursor;++n){r.tick(n%8==0?key():InputFrame{});}need(r.game.inputPromptView().cursor.has_value(),"Natural CE38 Continue absent");r.tick(key());
 bool via219=false;for(int n=0;n<10000;++n){r.tick(n%8==0?key():InputFrame{});via219|=r.game.mapId()==219;if(r.game.mapId()==192&&r.game.foreground()==ForegroundKind::None){break;}}
 need(via219&&r.game.mapId()==192&&r.game.session().hp()==(cycle?original.hp:46)&&r.game.session().variable(630)==beforeContinue+1&&r.game.session().saveCount()==(cycle?original.saveCount:60)&&r.game.session().magazine(20)==(cycle?original.magazines.at(20):2)&&r.game.session().magazine(21)==(cycle?original.magazines.at(21):5)&&r.game.session().count(111)==1&&!r.game.session().flag(504)&&r.game.session().flag(507),"Natural219 restore lost actual backup");
 std::cout<<"02 natural death/Continue219 restored actual source PASS step="<<r.ticks<<'\n';}
 std::set<int> modes;int caught=0;int lastProgress=0;int peakProgress=0;std::vector<int> milestones;TilePosition fleeTarget{8,25};int corner=0;int observedHp=-1,observedMedicine=-1,lastMode=-1;TilePosition jumpEscape{};
 for(int n=0;n<80000&&r.game.mapId()==192;++n){
  auto& g=r.game;const auto& s=g.session();const int mode=s.variable(91);modes.insert(mode);
  if(cycle&&(s.hp()!=observedHp||r.healingItem()!=observedMedicine)){std::cout<<"Dora actual hp="<<s.hp()<<" medicine="<<r.healingItem()<<" count="<<s.count(r.healingItem())<<" mode="<<mode<<" progress="<<s.variable(89)<<" player="<<g.world().player().motion.tile().x<<","<<g.world().player().motion.tile().y<<" boss="<<findCharacter(g.world(),"m192-e2").motion.tile().x<<","<<findCharacter(g.world(),"m192-e2").motion.tile().y<<" step="<<r.ticks<<'\n';observedHp=s.hp();observedMedicine=r.healingItem();}need(s.hp()>0,"02 natural battle died");if(s.hp()<(cycle?60:70)&&(cycle?r.healingItem()!=0:s.count(5)>0)&&g.foreground()==ForegroundKind::None&&(cycle||!s.flag(45)&&s.flag(1))){if(cycle){r.lowerForMenu(r.ticks);need(r.healTo(100),"Cycle Dora actual healing absent");}else{r.item(5);}continue;}InputFrame in;
  if(g.dialogue().active()){if(n%8==0){in=key();}}
  else if(s.flag(10)||s.flag(45)){if(n%3==0){in=key(InputAction::Down);}else if(n%3==1){in=key(InputAction::Up);}}
  else if(g.foreground()==ForegroundKind::None){
   auto player=g.world().player();const auto boss=findCharacter(g.world(),"m192-e2").motion.tile();
   // P13 missiles outlive the main page; keep more than their source 32px blast radius.
   const auto missileClearance=[&](TilePosition goal){double clearance=100;for(const auto [event,variable]:{std::pair{5,119},{6,133},{9,147},{11,161}}){if(s.variable(variable)<=0){continue;}const auto p=findCharacter(g.world(),"m192-e"+std::to_string(event)).motion.position();const double dy=goal.y-p.y;if(dy>=-3&&dy<=12){clearance=std::min(clearance,std::abs(goal.x-p.x));}}return clearance;};
   const auto missileRisk=[&](TilePosition goal){return std::max(0.0,3-missileClearance(goal));};
   if(cycle&&mode!=4&&missileRisk(player.motion.tile())>0){const auto tile=player.motion.tile();TilePosition goal{7,tile.y};double best=-1e9;for(auto candidate:{TilePosition{7,tile.y},TilePosition{21,tile.y}}){const double score=missileClearance(candidate)-.05*std::abs(candidate.x-tile.x);if(score>best&&next(g.world(),candidate)){goal=candidate;best=score;}}in.movement=next(g.world(),goal);}
   else if(mode==11||(cycle&&mode==12)){const auto face=player.motion.facing();if(adjacentTile(player.motion.tile(),face)==boss){if(mode==11){in=key();++caught;}}else if(std::abs(player.motion.tile().x-boss.x)+std::abs(player.motion.tile().y-boss.y)==1){in.movement=faceToward(player.motion.tile(),boss);}else{std::array<Direction,4> sides{Direction::Down,Direction::Up,Direction::Right,Direction::Left};if(cycle){const auto p=player.motion.tile();std::stable_sort(sides.begin(),sides.end(),[&](auto a,auto b){auto x=adjacentTile(boss,a),y=adjacentTile(boss,b);return std::abs(x.x-p.x)+std::abs(x.y-p.y)<std::abs(y.x-p.x)+std::abs(y.y-p.y);});}for(Direction side:sides){const auto tile=adjacentTile(boss,side);const auto step=next(g.world(),tile);if(step){in.movement=step;break;}}}}
   else if(cycle&&s.difficulty()=="hard"&&(mode==1||mode==5)){const auto tile=player.motion.tile();const auto facing=findCharacter(g.world(),"m192-e2").motion.facing();const bool vertical=facing==Direction::Up||facing==Direction::Down;TilePosition goal=vertical?TilePosition{tile.x==boss.x?(boss.x<=14?21:7):(tile.x<boss.x?7:21),tile.y}:TilePosition{tile.x,tile.y==boss.y?(boss.y<=20?26:15):(tile.y<boss.y?15:26)};if(tile==goal){goal=vertical?TilePosition{tile.x,tile.y<boss.y?15:26}:TilePosition{tile.x<boss.x?7:21,tile.y};}in.movement=next(g.world(),goal);}
   // P10 is the death page: P11 mode1 air cannon, P13 mode3 missiles, P14 mode4 jump.
   else if(cycle&&mode==4){const auto tile=player.motion.tile();if(lastMode!=mode||tile==jumpEscape){const std::array<TilePosition,4> targets{{{tile.x+3,tile.y},{tile.x-3,tile.y},{tile.x,tile.y+3},{tile.x,tile.y-3}}};double distance=-1e9;for(auto goal:targets){const double away=std::abs(goal.x-boss.x)+std::abs(goal.y-boss.y)-100*missileRisk(goal);if(goal.x>=7&&goal.x<=21&&goal.y>=15&&goal.y<=26&&away>distance&&next(g.world(),goal)){jumpEscape=goal;distance=away;}}need(distance>-1e9,"Cycle Dora source P14 jump dodge path absent");}in.movement=next(g.world(),jumpEscape);}
   else if(!cycle&&(mode==4||mode==12)){const auto tile=player.motion.tile();const auto step=next(g.world(),{std::clamp(tile.x+(tile.x<14?-3:3),7,21),std::clamp(tile.y+(tile.y<21?-3:3),15,26)});if(step){in.movement=step;}}
   else if(mode!=11){const auto tile=player.motion.tile();const std::array<TilePosition,4> corners{{{8,25},{20,25},{20,16},{8,16}}};if(std::abs(tile.x-fleeTarget.x)+std::abs(tile.y-fleeTarget.y)<2){corner=(corner+1)%4;fleeTarget=corners[corner];}const auto step=next(g.world(),fleeTarget);if(step){in.movement=step;}}
  }
  lastMode=mode;r.tick(in);if(g.session().variable(89)!=lastProgress){std::cout<<"02 persuasion="<<g.session().variable(89)<<" step="<<r.ticks<<'\n';lastProgress=g.session().variable(89);peakProgress=std::max(peakProgress,lastProgress);milestones.push_back(lastProgress);}
 }
 need(r.game.mapId()==190&&r.game.session().flag(504)&&!r.game.session().flag(586),"Natural Dora victory absent");if(cycle){std::cout<<"Cycle Dora real persuasion victory HP="<<r.game.session().hp()<<" steps="<<r.ticks<<'\n';}
}
// Complete the real Save60 chain using only the existing equipment menu and attacks.
template<class Renderer>
inline void winCash(G2NaturalDriver& r,Renderer& render,const std::filesystem::path& root,bool cycle=false){
 const auto before=r.game.session().exportState();need((cycle||before.magazines.at(21)==5)&&r.game.session().count(21)>0,"Real DE equipment/ammunition absent");if(cycle){r.equipAvailable({22,20,15,19,21});}else{r.equip(21);}
 need((cycle||r.game.session().equipped(1)==21)&&r.game.session().magazine(21)==(cycle?before.magazines.at(21):5),"Public equipment menu changed independent DE magazine");
 std::set<int> modes;int fired=0,readyTick=0;Json hits=Json::array();int damage=0;
 for(int n=0;n<60000&&r.game.mapId()==191;++n){auto& g=r.game;const auto& s=g.session();need(s.hp()>0,"Natural Cash victory route died");const int weapon=s.equipped(1),rounds=s.magazine(weapon),mode=s.variable(91);modes.insert(mode);InputFrame in;
  if(s.hp()<60&&(cycle?r.healingItem()!=0:s.count(5)>0)&&g.foreground()==ForegroundKind::None&&s.flag(1)&&!s.flag(45)){if(cycle){r.lowerForMenu(readyTick);need(r.healTo(100),"Cycle Cash actual healing absent");}else{r.item(5);}continue;}
  if(g.dialogue().active()||g.foreground()!=ForegroundKind::None){if(n%8==0){in=key();}}
  else if(s.flag(45)){if(n%3==0){in=key(InputAction::Down);}else if(n%3==1){in=key(InputAction::Up);}}
  else if(!g.raised()){in=key(InputAction::Weapon);}
  else{auto p=g.world().player().motion.tile(),t=findCharacter(g.world(),"m191-e2").motion.tile();int dx=t.x-p.x,dy=t.y-p.y;
   if(mode>=7&&mode<=10){in.movement=next(g.world(),{p.x<14?8:20,p.y<20?14:24});}
   else if((dx&&dy)||std::abs(dx)+std::abs(dy)>6){for(auto side:{Direction::Down,Direction::Up,Direction::Left,Direction::Right}){const auto step=next(g.world(),adjacentTile(t,side));if(step){in.movement=step;break;}}}
   else{auto face=std::abs(dx)>std::abs(dy)?(dx>0?Direction::Right:Direction::Left):(dy>0?Direction::Down:Direction::Up);if(g.world().player().motion.facing()!=face){in.buttons[int(InputAction::Weapon)].held=true;in.movement=face;}else if(n%12==0){if(cycle&&rounds==0&&!s.hasReserve()){r.lowerForMenu(readyTick);r.equipAvailable({20,15,19,21});continue;}need(rounds>0||(cycle&&s.hasReserve()),"Natural DE ammunition exhausted before victory");in=key(InputAction::Attack);}}
  }
  r.tick(in);const int consumed=std::max(0,rounds-r.game.session().magazine(weapon));fired+=consumed;if(cycle&&consumed){readyTick=r.ticks+r.fireRecoveryTicks(weapon);}if(g.mapId()==191&&g.session().variable(89)!=damage){damage=g.session().variable(89);hits.push_back({{"step",r.ticks},{"damage",damage},{"ammo",g.session().magazine(21)},{"hp",g.session().hp()}});render.frame("cash-damage-"+std::to_string(damage));}
  if(n%500==0){std::cout<<"04 natural Cash step="<<r.ticks<<" HP="<<g.session().hp()<<" damage="<<damage<<" ammo="<<g.session().magazine(21)<<'\n';}
 }
 auto& g=r.game;need(g.mapId()==190&&g.session().flag(508)&&!g.session().flag(586)&&fired>0&&!g.session().flag(1)&&!g.session().flag(18),"Natural Cash winner boundary absent");render.frame("natural-Cash-victory-M190");
 const auto generation=g.generation();const auto won=g.session().exportState();for(int n=0;n<120&&!cycle;++n){r.tick(n%8==0?key():InputFrame{});need(g.mapId()==190&&g.generation()==generation&&g.session().flag(508)&&g.session().variable(630)==won.variables.at(630)&&g.session().saveCount()==60&&!g.session().flag(1)&&!g.session().flag(18)&&!g.takeSaveRequest(),"Pending G5 aftermath replayed battle or exposed control/save");}
 std::ofstream(root/"cash-victory.json")<<Json{{"kind",g.session().variable(635)>0?"real-Clear-public-cycle-truth-and-Cash-victory":"real-public-source-truth-and-Cash-victory"},{"natural_entry",true},{"runtime_injection",false},{"steps",r.ticks},{"before_equipment",before.mainWeapon},{"before_inventory",before.inventory},{"before_magazines",before.magazines},{"main_weapon",g.session().equipped(1)},{"remaining_magazine21",g.session().magazine(21)},{"fired",fired},{"hits",hits},{"modes",modes},{"hp",g.session().hp()},{"map",g.mapId()},{"508",g.session().flag(508)},{"586",g.session().flag(586)},{"save_count",g.session().saveCount()},{"retry_count",g.session().variable(630)},{"truth_stage",g.session().variable(627)},{"post_win_repeat_steps",cycle?0:120},{"frames",render.frames}}.dump(2)<<'\n';
 std::cout<<"04 natural Cash victory PASS fired="<<fired<<" HP="<<g.session().hp()<<" steps="<<r.ticks<<'\n';
}

}
