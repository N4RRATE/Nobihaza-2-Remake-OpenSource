#include "combat_scene.h"
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* why){if(!ok){throw std::runtime_error(why);}}
InputFrame confirm(){InputFrame i;i.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;return i;}
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);auto e=c.entries.at("defeated-claimed");e.map=33;e.playerTile={9,7};e.direction=Direction::Left;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.inventory[66]=1;e.session.flags[87]=true;
 GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});
 require(s.count(66)==0&&s.count(67)==1,"Power cable enters usable area through world update");auto request=room.requestToolUse();require(request.has_value(),"Power menu request must exist");
 auto wrong=*request;wrong.scene++;require(!room.useTool(wrong),"Foreign request rejected");require(room.useTool(*request),"Current request accepted");require(!room.useTool(*request),"Request consumed once");
 for(int tick=0;tick<2400;++tick){room.update(tick%12==0?confirm():InputFrame{});if(room.foreground()==ForegroundKind::None&&s.flag(93)){break;}}
 require(s.flag(93)&&s.count(67)==0&&s.count(66)==0,"Installation opens shutter and consumes cable at source point");require(!room.requestToolUse(),"Completed installation cannot restart");
 GameSession other(c.items,e.session,c.facts);CombatScene old(c,other,e);old.update({});auto stale=old.requestToolUse();old.deactivate();require(stale&&!old.useTool(*stale),"Destroyed scene invalidates request");
 auto shifted=c;shifted.catalog["context_tools"][0]["use_tile"]={8,7};e.playerTile={8,7};GameSession changed(shifted.items,e.session,shifted.facts);CombatScene moved(shifted,changed,e);moved.update({});require(moved.requestToolUse().has_value(),"Tool area follows content coordinate variation");
 for(bool accept:{false,true}){
  auto pick=e;pick.map=32;pick.playerTile={3,3};pick.direction=Direction::Up;pick.session.inventory.erase(66);pick.session.flags[87]=false;
  GameSession inventory(c.items,pick.session,c.facts);CombatScene cable(c,inventory,pick);cable.update(confirm());bool selected=false;
  for(int tick=0;tick<6000;++tick){InputFrame in;const auto& view=cable.inputPromptView();
   if(view.cursor&&!selected){require(view.lines.size()==4,"Cable question and choices share one window");in.buttons[static_cast<int>(accept||view.cursor->line==3?InputAction::Confirm:InputAction::Down)].pressed=true;if(accept||view.cursor->line==3){selected=true;}}
   else if(tick%12==0){in=confirm();}cable.update(in);if(selected&&cable.foreground()==ForegroundKind::None){break;}
  }
  require(selected&&inventory.flag(87)==accept&&inventory.count(66)==(accept?1:0),"Cable choice branches preserve source grant timing");
 }
 std::cout<<"Power tool request, commit and lifetime checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
