#include "combat_scene.h"
#include "branch_content.h"
#include <iostream>
#include <stdexcept>
void require(bool x,const char* why){if(!x){throw std::runtime_error(why);}}
CombatEntry entry(const CombatContent& c,int map,TilePosition pos,Direction d){auto e=c.entries.at("defeated-claimed");e.map=map;e.playerTile=pos;e.direction=d;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.flags[84]=true;e.session.flags[274]=true;e.session.flags[213]=true;return e;}
InputFrame confirm(){InputFrame z;z.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;return z;}
void finish(CombatScene& scene){for(int n=0;n<10000;++n){scene.update(n%12==0?confirm():InputFrame{});if(n>120&&scene.foreground()==ForegroundKind::None){return;}}throw std::runtime_error("Return flow stuck");}
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);require(c.catalog.at("flows").contains("spade-unlock"),"Spade lock must be playable");
 for(bool wrongKind:{false,true}){auto bad=c;const std::string id=bad.branch.at("episodes").at("spade_lock").at("discard_choice");if(wrongKind){{auto cueVariant=bad.cue(id);cueVariant["kind"]="wait";bad.setMediaDefinition("cues",id,cueVariant);}}else{bad.cues.erase(id);}bool rejected=false;try{loadBranchReturns(bad);}catch(const std::exception&){rejected=true;}require(rejected,"Missing or wrong discard choice fails before gameplay");}
 {
  auto e=entry(c,25,{3,5},Direction::Up);e.session.flags[74]=false;e.session.skills.erase(3);e.session.skills.erase(4);e.session.magazines[14]=2;GameSession state(c.items,e.session,c.facts);CombatScene room(c,state,e);room.update({});require(!state.knowsSkill(3),"Unknown mixing skill is not granted on room entry");room.update(confirm());finish(room);require(state.count(14)==1&&state.magazine(14)==2&&state.flag(74),"Optional shotgun remains obtainable after cooperation");state.discard(14,1);room.update(confirm());finish(room);require(state.count(14)==0,"Optional gun cannot be received twice");
 }
 {
  auto e=entry(c,25,{9,16},Direction::Down);e.session.skills.insert(4);GameSession state(c.items,e.session,c.facts);CombatScene room(c,state,e);room.update({});room.update(confirm());finish(room);require(room.mapId()==12&&!state.knowsSkill(3)&&state.knowsSkill(4),"Real room exit restores disabled mixing identity");
 }
 {auto bad=c;bad.enemies.erase("m63-e12");bool rejected=false;try{loadBranchReturns(bad);}catch(const std::exception&){rejected=true;}require(rejected,"Natural key requires installed rescue capability");}
 for(int result:{272,273,337,338}){
  auto e=entry(c,25,{7,12},Direction::Up);e.session.flags[result]=true;e.session.skills.insert(4);e.session.magazines[12]=4;e.session.magazines[27]=2;GameSession state(c.items,e.session,c.facts);CombatScene room(c,state,e);room.update({});require(state.knowsSkill(3)&&!state.knowsSkill(4),"Meeting return enables known mixing");room.update(confirm());finish(room);
  const int reward=result==273?12:result==338?27:0;if(reward){require(state.count(reward)==1&&state.magazine(reward)==(reward==12?4:2),"Timely return grants weapon without filling magazine");state.discard(reward,1);room.update(confirm());finish(room);require(state.count(reward)==0,"Repeated reward page cannot regrant discarded weapon");}else{require(state.count(12)==0&&state.count(27)==0,"Intervention does not grant timely weapons");}
 }
 for(bool discard:{false,true}){
  auto e=entry(c,20,{18,12},Direction::Up);e.session.inventory[70]=1;e.session.flags[144]=true;GameSession state(c.items,e.session,c.facts);CombatScene room(c,state,e);room.update(confirm());
  bool selected=false;for(int n=0;n<2000&&!state.flag(147);++n){InputFrame f;if(room.inputPromptView().phase!=DialoguePhase::Hidden&&!selected){if(!discard){f.buttons[static_cast<int>(InputAction::Down)].pressed=true;}selected=true;}else if(n%12==0){f=confirm();}room.update(f);}
  require(state.flag(147)&&state.count(70)==(discard?0:1),"Both discard choices open spade lock");finish(room);room.update(confirm());finish(room);require(room.mapId()==20,"Training map stays a clear boundary");
 }
 std::cout<<"Cooperation return checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
