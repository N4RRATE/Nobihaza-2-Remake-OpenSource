#include "game_application.h"
#include "enemy_controller.h"
#include <iostream>
#include <stdexcept>
void require(bool b,const char* m){if(!b){throw std::runtime_error(m);}}
InputFrame confirm(){InputFrame f;f.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;return f;}
void settle(CombatScene& scene){for(int i=0;i<5000;++i){scene.update(i%12==0?confirm():InputFrame{});if(scene.foreground()==ForegroundKind::None&&!scene.world().player().motion.moving()){return;}}throw std::runtime_error("Exploration flow stuck");}
CombatEntry fixture(const CombatContent& c,int map,TilePosition pos,Direction facing){auto e=c.entries.at("defeated-claimed");e.map=map;e.playerTile=pos;e.direction=facing;e.pages.clear();e.positions.clear();e.through.clear();e.directions.clear();return e;}
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}const auto c=loadCombatContent(argv[1]);
 for(int map:{26,27,28,29,30,31,32,33,34,37,38,39,40,41,46,50}){require(!c.pendingMapTickets.contains(map),"Initial branch maps have actual runtime consumers");}
 const auto& object=c.objects.at("m28-e10");auto e=fixture(c,28,object.tile,Direction::Up);
 GameSession session(c.items,e.session,c.facts);CombatScene room(c,session,e);room.update(confirm());settle(room);
 require(session.count(182)==1,"Handbook acquired via action, not fixture grant");room.update(confirm());settle(room);require(session.count(182)==1,"Repeated investigation cannot duplicate handbook");
 auto restored=fixture(c,30,{2,2},Direction::Down);restored.session.flags[113]=true;
 GameSession dead(c.items,restored.session,c.facts);CombatScene revisit(c,dead,restored);for(int i=0;i<3;++i){revisit.update({});}
 const auto body=std::find_if(revisit.world().objects().begin(),revisit.world().objects().end(),[](const auto& a){return a.id=="m30-e3";});
 require(body!=revisit.world().objects().end()&&body->motion.tile()==TilePosition{39,29},"Map30 reviewed dead restore differs from generic zero tile");
 require(c.catalog.at("readings").contains("item-182"),"Acquired file has repeatable menu reading");
 std::cout<<"B/C exploration checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
