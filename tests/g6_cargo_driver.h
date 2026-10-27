#pragma once
#include "g2_natural_driver.h"
namespace g6cargo {
inline void enter(G2NaturalDriver& d){d.walk({25,8});d.move(Direction::Up);d.tick(key());d.settle();need(d.game.mapId()==204&&d.game.world().player().motion.tile()==TilePosition{14,25},"Natural source door not204(14,25)");need(d.game.session().flag(529)&&d.game.session().flag(1)&&d.game.session().flag(18)&&!d.game.session().flag(577),"Natural entrance control/alarm/source battle state");}
inline void prompt(G2NaturalDriver& d,int choice){d.walk({19,6});d.move(Direction::Up);d.tick(key());d.choices(choice);need(d.game.session().flag(577)==(choice==0),"Actual Warehouse choice state");}
inline void fight(G2NaturalDriver& d){
 int fired=0,readyTick=0;std::set<int> modes;
 for(int n=0;n<100000;++n){auto& g=d.game;const auto& s=g.session();need(g.hasGame()&&s.hp()>0&&g.mode()==ApplicationMode::Playing,"Natural Warehouse battle died");const int weapon=s.equipped(1),rounds=s.magazine(weapon);modes.insert(s.variable(91));InputFrame in;
  if(g.inputPromptView().cursor){in=key();}
  else if(g.foreground()!=ForegroundKind::None||g.dialogue().active()){if(n%8==0){in=key();}}
  else if(s.flag(578)){break;}
  else if(s.hp()<60&&!s.flag(45)&&s.flag(1)&&(s.count(1)>0||s.count(2)>0)){
   d.lowerForMenu(readyTick);d.press(InputAction::Cancel);if(g.mode()!=ApplicationMode::Equipment){continue;}
   d.select(0);d.select(s.count(1)>0?1:2);if(g.mode()==ApplicationMode::Equipment&&g.menuView().page==MenuPage::Target){d.press();}d.closeMenu();continue;
  }
  else if(rounds==0&&!s.hasReserve()&&!s.flag(45)&&s.flag(1)){d.lowerForMenu(readyTick);int replacement=0;for(int id:{15,13,17,19,11,21}){if(id!=weapon&&s.count(id)>0&&s.magazine(id)>0){replacement=id;break;}}need(replacement!=0,"Natural Warehouse all actual magazines exhausted");d.equip(replacement);continue;}
  else if(s.flag(45)){if(n%3==0){in=key(InputAction::Down);}else if(n%3==1){in=key(InputAction::Up);}}
  else if(!g.raised()){in=key(InputAction::Weapon);}
  else {const auto p=g.world().player().motion.tile(),t=findCharacter(g.world(),"m204-e2").motion.tile();const int dx=t.x-p.x,dy=t.y-p.y,mode=s.variable(91);
   if(mode==6||mode==9||mode==10){in.movement=next(g.world(),{p.x<14?10:18,p.y<15?18:12});}
   else if((dx&&dy)||std::abs(dx)+std::abs(dy)>6){for(auto side:{Direction::Down,Direction::Up,Direction::Left,Direction::Right}){auto step=next(g.world(),adjacentTile(t,side));if(step){in.movement=step;break;}}}
   else {auto face=std::abs(dx)>std::abs(dy)?(dx>0?Direction::Right:Direction::Left):(dy>0?Direction::Down:Direction::Up);if(g.world().player().motion.facing()!=face){in.buttons[int(InputAction::Weapon)].held=true;in.movement=face;}else if(n%12==0){need(rounds>0||s.hasReserve(),"Natural Warehouse actual ammo exhausted");in=key(InputAction::Attack);}}
  }
  d.tick(in);const int used=std::max(0,rounds-g.session().magazine(weapon));fired+=used;if(used){readyTick=d.ticks+d.fireRecoveryTicks(weapon);}if(n%500==0){std::cout<<"Natural06 battle step="<<d.ticks<<" HP="<<g.session().hp()<<" damage="<<g.session().variable(89)<<" mode="<<g.session().variable(91)<<" weapon="<<weapon<<" ammo="<<g.session().magazine(weapon)<<'\n';}
 }
 need(d.game.session().flag(578)&&fired>0&&d.game.session().variable(89)>=1000&&!d.game.session().flag(586),"Natural Warehouse complete victory absent");d.settle();d.lower();std::cout<<"G6-06 real Warehouse victory fired="<<fired<<" damage="<<d.game.session().variable(89)<<" HP="<<d.game.session().hp()<<'\n';
}
inline void directSave(G2NaturalDriver& d){d.walk({25,6});d.settle();d.tick(key());need(d.game.mode()==ApplicationMode::Saving,"Natural victory direct save not reached");d.tick(key(InputAction::Down));d.tick(key());need(d.game.mode()==ApplicationMode::Playing,"Natural victory direct commit failed");d.settle();}
inline void elevator(G2NaturalDriver& d,int x){d.walk({x,6});d.move(Direction::Up);d.settle();need(d.game.mapId()==206&&d.game.world().player().motion.tile()==TilePosition{2,8}&&d.game.world().player().motion.facing()==Direction::Right&&d.game.session().flag(529)&&d.game.session().flag(1)&&d.game.session().flag(18),"Nine source elevator full tail/alarm/control");}
inline void cargo(G2NaturalDriver& d,int y){d.walk({30,y});d.move(Direction::Right);d.tick(key());for(int n=0;n<15000&&d.game.mapId()!=209;++n){d.tick(n%2==0?key():InputFrame{});}need(d.game.mapId()==209&&d.game.session().flag(580)&&!d.game.session().flag(529)&&d.game.session().flag(577)&&d.game.session().flag(578),"Natural cargo/cockpit/real final battle absent");d.settle();need(d.game.session().flag(581)&&d.game.playerVisible()&&d.game.session().flag(1)&&d.game.session().flag(18),"Actual final entrance did not finish its source intro/start581");}
}
