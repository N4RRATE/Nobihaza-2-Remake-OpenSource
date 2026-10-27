#include "save_snapshot.h"
#include <iostream>
#include <stdexcept>
namespace{
void require(bool ok,const char* why){if(!ok){throw std::runtime_error(why);}}
InputFrame press(InputAction action=InputAction::Confirm){InputFrame i;i.buttons[static_cast<int>(action)].pressed=true;return i;}
void settle(CombatScene& scene){
 for(int n=0;n<5000;++n){
  if(scene.inputPromptView().phase!=DialoguePhase::Hidden||scene.foreground()==ForegroundKind::None){return;}
  scene.update(n%12==0?press():InputFrame{});
 }
 throw std::runtime_error("Wardrobe interaction did not settle");
}
}
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);
 auto e=c.entries.at("defeated-claimed");e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();
 e.map=11;e.playerTile=adjacentTile(c.objects.at("m11-e5").tile,Direction::Down);e.direction=Direction::Up;
 GameSession noKey(c.items,e.session,c.facts);CombatScene locked(c,noKey,e);
 locked.update(press());require(locked.dialogue().view().phase!=DialoguePhase::Hidden,"No key gives the original refusal");settle(locked);
 require(!noKey.flag(67)&&noKey.count(62)==0,"Refusal does not unlock or reward");
 std::vector<SessionEntry> states{noKey.exportState()};
 for(bool discard:{false,true}){
  GameSession state(c.items,e.session,c.facts);state.grant(61,1);state.setFlag(66,true);
  states.push_back(state.exportState());
  CombatScene room(c,state,e);room.update(press());settle(room);
  require(room.inputPromptView().phase!=DialoguePhase::Hidden&&!state.flag(67),"Key discard choice must precede unlocking");
  room.update(press(InputAction::Cancel));require(room.inputPromptView().phase!=DialoguePhase::Hidden,"Original key choice does not permit cancel");
  if(!discard){room.update(press(InputAction::Down));}room.update(press());room.update({});settle(room);
  require(state.flag(67)&&!state.flag(68)&&state.count(61)==(discard?0:1)&&state.count(62)==0,"Both choices unlock without also awarding wrench");
  states.push_back(state.exportState());
  room.update(press());settle(room);
  require(state.flag(68)&&state.count(62)==1&&state.count(63)==0,"A second investigation grants the inactive wrench once");
  room.update(press());settle(room);require(state.count(62)==1,"Repeated investigation never duplicates wrench");
  states.push_back(state.exportState());
 }
 for(const auto& progress:states){for(const int map:{16,22}){
  auto at=e;at.map=map;const std::string crow=map==16?"m16-e5":"m22-e2";
  at.playerTile=adjacentTile(c.objects.at(crow).tile,Direction::Down);at.direction=Direction::Up;
  GameSession saving(c.items,progress,c.facts);CombatScene scene(c,saving,at);
  auto snapshot=captureSave(c,scene,{scene.generation(),crow,""},{});
  auto restored=prepareRestore(c,decodeSave(c,encodeSave(c,snapshot)));
  require(restored.session->flag(67)==saving.flag(67)&&restored.session->flag(68)==saving.flag(68)&&restored.session->count(61)==saving.count(61)&&restored.session->count(62)==saving.count(62),"Both save points preserve each wardrobe stage");
  CombatScene revisit(c,*restored.session,e);revisit.update(press());settle(revisit);
  if(saving.flag(68)){require(restored.session->count(62)==1&&revisit.foreground()==ForegroundKind::None,"Loaded empty wardrobe cannot grant again");}
  else if(saving.flag(67)){require(restored.session->flag(68)&&restored.session->count(62)==1,"Loaded unlocked wardrobe still grants the wrench");}
  else if(saving.count(61)){require(revisit.inputPromptView().phase!=DialoguePhase::Hidden,"Loaded unused key still asks before unlock");}
  else{require(!restored.session->flag(67)&&restored.session->count(62)==0,"Loaded locked wardrobe refuses");}
  if(saving.flag(68)){
   for(int corrupt=0;corrupt<3;++corrupt){auto bad=snapshot;
    if(corrupt==0){bad.session.worldFlags[c.facts.switches.at(67)]=false;}
    else if(corrupt==1){bad.session.inventory[62]=0;}
    else{bad.session.inventory[63]=1;}
    bool refused=false;try{encodeSave(c,bad);}catch(const std::exception&){refused=true;}
    require(refused,"Contradictory wrench fact, quantity or active save state must be rejected");
   }
  }
 }}
 // Finite flows use content identity and position, not fixed runtime map coordinates.
 auto varied=c;auto node=varied.objects.extract("m11-e5");node.key()="fixture-wardrobe";node.mapped().id=node.key();node.mapped().tile={8,5};
 for(auto& v:node.mapped().variants){v.definition.id=node.key();v.definition.tileX=8;v.definition.tileY=5;}
 varied.objects.insert(std::move(node));for(auto& [id,flow]:varied.catalog["flows"].items()){if(flow.at("object")=="m11-e5"){flow["object"]="fixture-wardrobe";}}
 auto shifted=e;shifted.playerTile={8,6};GameSession moving(varied.items,e.session,varied.facts);moving.grant(61,1);moving.setFlag(66,true);
 CombatScene shiftedRoom(varied,moving,shifted);shiftedRoom.update(press());settle(shiftedRoom);
 require(shiftedRoom.inputPromptView().phase!=DialoguePhase::Hidden,"Moved and renamed wardrobe still requests choice");
 shiftedRoom.deactivate();shiftedRoom.update(press());
 require(!moving.flag(67)&&moving.count(61)==1,"Scene destruction invalidates pending discard result");
 std::cout<<"Wardrobe checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
