#include "game_application.h"
#include "enemy_controller.h"
#include <iostream>
#include <stdexcept>
void require(bool value,const char* text){if(!value){throw std::runtime_error(text);}}
InputFrame key(InputAction a=InputAction::Confirm){InputFrame f;f.buttons[static_cast<int>(a)]={true,true,false};return f;}
void settle(CombatScene& scene){if(scene.foreground()==ForegroundKind::None){return;}for(int n=0;n<4000;++n){scene.update(n%15==0?key():InputFrame{});if(scene.foreground()==ForegroundKind::None&&!scene.world().player().motion.moving()){return;}}throw std::runtime_error("F4 finite flow stuck");}
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);auto e=c.entries.at("defeated-claimed");e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.map=18;e.playerTile={17,6};e.direction=Direction::Right;e.session.flags[56]=true;
 GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update(key());settle(room);
 require(room.mapId()==19&&!room.exitRequest(),"Reviewed Map18 exit must enter real Map19");
 require(room.world().player().motion.tile()==TilePosition{3,8},"Original Map19 entrance position");
 const auto entryMedia=room.takePresentation();require(std::any_of(entryMedia.begin(),entryMedia.end(),[](const auto& e){return e.kind==PresentationEvent::Kind::StopMusic;}),"Map19 LMT stops inherited music on ordinary transfer");
 auto intro=e;intro.map=19;intro.playerTile={19,16};intro.direction=Direction::Right;intro.session.flags[57]=false;
 GameSession introState(c.items,intro.session,c.facts);CombatScene encounter(c,introState,intro);encounter.update({});
 require(encounter.foreground()!=ForegroundKind::None,"Contact trigger begins licker encounter before free movement");settle(encounter);
 require(introState.flag(57)&&introState.flag(1)&&introState.flag(18),"Encounter commits activation and returns control");
 require(encounter.enemyStates().at(0).sourceState==2,"Encounter activates pursuit, distinct from ordinary revisit");
 auto hidden=e;hidden.map=22;hidden.playerTile={5,4};hidden.direction=Direction::Up;
 GameSession items(c.items,hidden.session,c.facts);CombatScene gift(c,items,hidden);
 for(int n=0;n<2;++n){gift.update(key());settle(gift);}require(items.count(21)==2&&items.count(43)==2&&items.count(5)==hidden.session.inventory[5]+4,"Hidden DE reward is repeatable at its actual tile");
 require(items.equip(21)&&items.consumeRound(),"DE reward can be equipped and used");gift.update(key());settle(gift);require(items.magazine(21)==7,"Repeated reward preserves partially spent DE magazine");
 hidden.playerTile={4,4};hidden.session=items.exportState();hidden.session.mainWeapon=20;hidden.session.inventory[20]=0;hidden.session.inventory[40]=1;hidden.session.inventory[41]=1;hidden.session.inventory[5]=2;
 GameSession removal(c.items,hidden.session,c.facts);CombatScene loss(c,removal,hidden);loss.update(key());settle(loss);
 require(removal.equipment().mainWeapon==20&&removal.count(40)==0&&removal.count(41)==0&&removal.count(5)==1,"Hidden removal clamps inventory at zero and does not strip equipped I20");loss.update(key());settle(loss);require(removal.count(5)==0,"Hidden removal remains repeatable");
 std::cout<<"F4 route checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
