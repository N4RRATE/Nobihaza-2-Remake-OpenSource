#pragma once
#include "g2_natural_driver.h"
namespace g5natural {
inline void saving(G2NaturalDriver& r,int slot,int answer=0){
 r.walk({11,6});r.move(Direction::Up);const auto count=r.game.session().saveCount(),stat=r.game.session().variable(399);r.tick(key());need(r.game.mode()==ApplicationMode::Saving,"Control ordinary save absent");
 for(int n=0;n<200;++n){r.tick();}if(answer==2){r.tick(key(InputAction::Cancel));need(r.game.mode()==ApplicationMode::Saving&&!r.game.fileMenuView(),"CE56 cancel0 unexpectedly left question");}
 if(answer==1){r.tick(key(InputAction::Down));}r.tick(key());
 if(answer==1){need(r.game.mode()==ApplicationMode::Playing&&r.game.session().saveCount()==count&&r.game.session().variable(399)==stat,"CE56 No counted");return;}
 need(r.game.fileMenuView().has_value(),"Control ordinary slots absent");if(answer==2){r.tick(key(InputAction::Cancel));need(r.game.mode()==ApplicationMode::Playing&&r.game.session().saveCount()==count&&r.game.session().variable(399)==stat,"FileMenu cancellation counted");return;}
 for(int n=1;n<slot;++n){r.tick(key(InputAction::Down));}r.tick(key());if(auto v=r.game.fileMenuView();v&&v->confirmation){if(v->answer){r.tick(key(InputAction::Up));}r.tick(key());}
 need(r.game.mode()==ApplicationMode::Playing&&r.game.session().saveCount()==count+1&&r.game.session().variable(399)==stat+1,"Ordinary save did not count exactly once");
}
inline void supplies(G2NaturalDriver& r,bool fresh){
 for(auto [id,fact,item,amount]:std::initializer_list<std::tuple<const char*,int,int,int>>{{"m195-e9",552,2,1},{"m195-e10",551,2,1},{"m195-e11",550,4,1}}){const int before=r.game.session().count(item);r.actObject(id);need(r.game.session().flag(fact)&&r.game.session().count(item)==std::min(99,before+(fresh?amount:0)),"Natural herbs wrong once/cap");}
 r.actObject("m195-e6");r.actObject("m195-e8");r.actObject("m195-e7");need(r.game.mapId()==195&&!r.game.session().flag(528)&&!r.game.session().flag(530),"Initial secret room unlocked");
 r.actObject("m195-e4");need(r.game.mapId()==196&&r.game.session().flag(1)&&r.game.session().flag(18),"First north door tail absent");
 const auto a=r.game.session().count(33),b=r.game.session().count(39),m=r.game.session().magazine(21);r.actObject("m196-e10");need(r.game.session().flag(570)&&r.game.session().count(33)==std::min(99,a+(fresh?7:0))&&r.game.session().count(39)==std::min(99,b+(fresh?2:0)),"Natural box granted wrong source quantities");const int ammo=r.game.session().count(40);r.actObject("m196-e11");need(r.game.session().flag(562)&&r.game.session().count(40)==std::min(99,ammo+(fresh?6:0))&&r.game.session().magazine(21)==m,"Natural magnum reward reset independent magazine");
 r.actObject("m196-e5");need(r.game.mapId()==195&&r.game.world().player().motion.tile()==TilePosition{14,5},"First return door destination lost");r.actObject("m195-e5");need(r.game.mapId()==196,"Second north door missing");r.actObject("m196-e6");need(r.game.mapId()==195&&r.game.world().player().motion.tile()==TilePosition{15,5},"Second return door lost");r.actObject("m195-e5");need(r.game.mapId()==196&&r.game.foreground()==ForegroundKind::None,"Second north door tail failed");
}
}
