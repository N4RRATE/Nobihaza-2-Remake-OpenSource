#pragma once
#include "g2_natural_driver.h"
#include "full_content.h"
// Shared original public-input routes; legacy NORMAL checks and G8 use the same helpers.
namespace g3route {
inline void firstKey(G2NaturalDriver& r,bool unpassed=true){
 auto& g=r.game;r.reception();r.actObject("m159-e5");r.actObject("m164-e2");need(g.mapId()==166,"G3 north entry absent");
 for(const auto& id:{"m166-e21","m170-e2","m169-e5"}){r.actObject(id);}need(g.mapId()==175,"G3 ladder room absent");
 need(findCharacter(g.world(),"m175-e9").motion.tile()==TilePosition{15,7}&&!g.session().flag(468),"G3 premature vent state");
 r.walk({15,8});for(int n=0;n<3;++n){r.move(Direction::Up);}r.walk({16,4});r.move(Direction::Left);r.move(Direction::Left);
 need(findCharacter(g.world(),"m175-e9").motion.tile()==TilePosition{13,4}&&!g.session().flag(468),"G3 five source pushes failed");
 if(unpassed){r.actObject("m175-e2");r.actObject("m169-e3");r.actObject("m170-e5");r.actObject("m166-e19");r.actObject("m164-e3");r.receptionBack();r.monitor(2);
 need(!g.session().flag(468),"G3 unpassed ordinary Save committed ladder");
 r.reception();r.actObject("m159-e5");r.actObject("m164-e2");r.actObject("m166-e21");r.actObject("m170-e2");r.actObject("m169-e5");
 need(findCharacter(g.world(),"m175-e9").motion.tile()==TilePosition{15,7},"G3 unpassed reentry retained transient push position");
 r.walk({15,8});for(int n=0;n<3;++n){r.move(Direction::Up);}r.walk({16,4});r.move(Direction::Left);r.move(Direction::Left);
 }
 r.actAt({13,5},Direction::Up);need(g.session().flag(69)&&g.world().player().motion.tile()==TilePosition{13,3},"G3 actual climb absent");
 r.tick(key());r.settle();need(g.mapId()==186&&g.session().flag(468)&&!g.session().flag(69),"G3 vent tail/helper priority failed");
 for(const auto& id:{"m186-e4","m169-e21","m166-e8","m168-e3"}){r.actObject(id);}need(g.mapId()==171&&g.session().flag(461),"G3 X north route absent");
 r.actObject("m171-e22");r.actObject("m171-e21");need(g.session().flag(463)&&g.session().count(105)==1,"G3 actual X pickup absent");r.actObject("m171-e21");need(g.session().count(105)==1,"G3 duplicate X");
}
inline void naturalFight(G2NaturalDriver& r){
 int fired=0,wait=0;for(int n=0;n<12000;++n){auto& g=r.game;auto& session=g.session();need(session.hp()>0&&g.mode()==ApplicationMode::Playing,"G3 natural fight died");std::vector<std::string> alive;for(const auto& [id,e]:g.content().enemies){if(e.map==g.mapId()&&!session.flag(e.deadFlag)){alive.push_back(id);}}if(alive.empty()){r.lower();std::cout<<"G3 actual fight map="<<g.mapId()<<" fired="<<fired<<" HP="<<session.hp()<<"\n";return;}InputFrame in;
 if(g.foreground()!=ForegroundKind::None||g.dialogue().active()){if(n%8==0){in=key();}}
 else if(wait>0){--wait;}
 else if(!g.raised()){in=key(InputAction::Weapon);}
 else{auto p=g.world().player().motion.tile();const auto& id=alive.front();auto t=findCharacter(g.world(),id).motion.tile();int dx=t.x-p.x,dy=t.y-p.y;auto face=std::abs(dx)>std::abs(dy)?(dx>0?Direction::Right:Direction::Left):(dy>0?Direction::Down:Direction::Up);
 if((dx&&dy)||std::abs(dx)+std::abs(dy)>4){for(auto side:{Direction::Down,Direction::Up,Direction::Right,Direction::Left}){if(auto step=next(g.world(),adjacentTile(t,side))){in.movement=step;break;}}}
 else if(g.world().player().motion.facing()!=face){in.buttons[int(InputAction::Weapon)].held=true;in.movement=face;}
 else if(n%12==0){need(session.magazine(session.equipped(1))>0||session.hasReserve(),"G3 natural actual rounds exhausted");in=key(InputAction::Attack);wait=400;}
 }const int before=session.magazine(session.equipped(1));r.tick(in);fired+=std::max(0,before-session.magazine(session.equipped(1)));}throw std::runtime_error("G3 natural fight stalled");
}
inline void closeManual(G2NaturalDriver& r){while(r.game.mode()==ApplicationMode::Equipment&&r.game.menuView().page!=MenuPage::Root){r.press(InputAction::Cancel);}if(r.game.mode()==ApplicationMode::Equipment){r.press(InputAction::Cancel);}for(int n=0;n<1000&&r.game.mode()==ApplicationMode::Reading;++n){r.tick();}need(r.game.mode()==ApplicationMode::Playing,"G3 manual close lifecycle stalled");}
inline void returned(G2NaturalDriver& r,bool cycle=false){
 for(const auto& id:{"m171-e19","m168-e6","m166-e15","m186-e5"}){r.actObject(id);}naturalFight(r);r.actObject("m173-e2");for(const auto& id:{"m170-e5","m166-e7","m169-e6"}){r.actObject(id);}need(r.game.mapId()==176,"G3 director return absent");
 r.walk({9,9});r.settle();need(r.game.session().count(106)==1&&r.game.session().count(105)==0,"G3 terminal X context conversion absent");r.move(Direction::Up);r.tick(key());r.settle();need(!r.game.session().flag(465),"G3 terminal paper opened authentication early");r.walk({9,10});r.settle();need(r.game.session().count(105)==1&&r.game.session().count(106)==0,"G3 terminal X context did not revert");
 r.actObject("m176-e9");need(r.game.session().flag(512)&&r.game.session().count(191)==1,"G3 manual grant absent");r.actAt({10,9},Direction::Up);need(r.game.session().count(191)==1,"G3 manual repeated grant");
 r.item(191);need(r.game.mode()==ApplicationMode::Reading,"G3 I191 reading absent");for(int n=0;n<10000&&r.game.mode()==ApplicationMode::Reading;++n){r.tick(n%8==0?key():InputFrame{});}need(r.game.mode()==ApplicationMode::Equipment,"G3 reading menu return absent");closeManual(r);
 if(!cycle){r.item(191);need(r.game.mode()==ApplicationMode::Reading,"G3 repeated reading absent");for(int n=0;n<10000&&r.game.mode()==ApplicationMode::Reading;++n){r.tick(n%8==0?key():InputFrame{});}closeManual(r);}
 r.actObject("m176-e2");r.actObject("m169-e24");r.actObject("m166-e19");need(r.game.mapId()==164,"G3 west return absent");r.actObject("m164-e3");r.receptionBack();r.monitor(3);
}
}
