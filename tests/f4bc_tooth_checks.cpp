#include "combat_scene.h"
#include "branch_content.h"
#include "save_snapshot.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
void require(bool x,const char* s){if(!x){throw std::runtime_error(s);}}
CombatEntry entry(const CombatContent& c,TilePosition tile,int item){auto e=c.entries.at("defeated-claimed");e.map=96;e.playerTile=tile;e.direction=Direction::Up;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.flags[316]=true;e.session.inventory[item]=1;return e;}
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);
 const auto rejected=[&](const auto& action,const char* why){bool refused=false;try{action();}catch(const std::exception&){refused=true;}require(refused,why);};
 {
  {auto control=c;loadBranchBlue(control);}
  for(const char* id:{"m96-e34","m96-e30"}){
   auto missing=c;missing.objects.erase(id);
   rejected([&]{loadBranchBlue(missing);},"Tooth origin and target must resolve before gameplay");
  }
  for(const char* id:{"m96-e34","m96-e30"}){
   auto wrongMap=c;wrongMap.objects.at(id).map=94;
   rejected([&]{loadBranchBlue(wrongMap);},"Tooth origin and target belong to the declared map");
  }
  for(int page:{1,2}){
   auto missingPage=c;std::erase_if(missingPage.objects.at("m96-e34").variants,[&](const auto& variant){return variant.page==page;});
   rejected([&]{loadBranchBlue(missingPage);},"Tooth active and inactive source pages must exist");
  }
  {auto missingPage=c;std::erase_if(missingPage.objects.at("m96-e30").variants,[](const auto& variant){return variant.page==1;});
   rejected([&]{loadBranchBlue(missingPage);},"Tooth effective target page must exist");}
  for(const char* id:{"m80-e2","m38-e5","m40-e7","m63-e12"}){validateBranchEnemyProfile(c,id,c.branch.at("zombie_profiles").at(id));}
  for(bool missing:{false,true}){
   auto bad=c;auto& flows=bad.branch["additional_flows"];if(missing){flows.erase("m90-e3-p1");}else{flows["m90-e3-p1"]["steps"].push_back({{"kind","unknown_operation"}});}
   rejected([&]{loadBranchBlue(bad);},"Missing blue flow or unknown operation is rejected before gameplay");
  }
  {auto missingPage=c;const auto& profile=missingPage.branch.at("zombie_profiles").at("m80-e2");const int pursuit=profile.at("pursuit_page");
  std::erase_if(missingPage.objects.at("m80-e2").variants,[&](const auto& page){return page.page==pursuit;});
  rejected([&]{validateBranchEnemyProfile(missingPage,"m80-e2",profile);},"Missing actual pursuit page is rejected before spawning enemy");}
  {auto wrongRoute=c;const auto& routeProfile=wrongRoute.branch.at("zombie_profiles").at("m80-e2");const std::string cue=routeProfile.at("terrain_routes").at("up").at("positive");{auto cueVariant=wrongRoute.cue(cue);cueVariant["kind"]="wait";wrongRoute.setMediaDefinition("cues",cue,cueVariant);}
  rejected([&]{validateBranchEnemyProfile(wrongRoute,"m80-e2",routeProfile);},"Terrain route must retain its executable cue kind");}
 }
 for(bool accept:{false,true}){
  auto e=entry(c,{5,5},87);e.map=50;e.session.inventory.erase(87);e.session.flags[316]=false;GameSession state(c.items,e.session,c.facts);CombatScene room(c,state,e);InputFrame z;z.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;room.update(z);bool navigated=false;
  for(int n=0;n<2500;++n){InputFrame f;if(!accept&&!navigated&&room.inputPromptView().phase!=DialoguePhase::Hidden){f.buttons[static_cast<int>(InputAction::Down)].pressed=true;navigated=true;}else if(n%12==0){f=z;}room.update(f);if(n>120&&room.foreground()==ForegroundKind::None){break;}}
  require(state.flag(316)==accept&&state.count(87)==(accept?1:0),"Actual tooth pickup choice preserves accept/decline");
 }
 for(TilePosition tile:std::vector<TilePosition>{{4,6},{10,8},{7,7},{3,6},{11,8},{4,5},{10,9},{2,7}}){
  auto e=entry(c,tile,87);GameSession state(c.items,e.session,c.facts);CombatScene room(c,state,e);room.update({});const bool inside=tile.x>=4&&tile.x<=10&&tile.y>=6&&tile.y<=8;require(state.count(88)==(inside?1:0),"Tooth activation follows inclusive source rectangle");
 }
 for(TilePosition tile:std::vector<TilePosition>{{3,6},{11,8},{3,5},{11,9},{2,7}}){
  auto e=entry(c,tile,88);GameSession state(c.items,e.session,c.facts);CombatScene room(c,state,e);room.update({});const bool border=tile.x!=2;require(state.count(87)==(border?1:0),"Tooth deactivation uses exact closed outer strips");
 }
 {
  auto e=entry(c,{7,7},88);e.session.inventory[87]=3;GameSession state(c.items,e.session,c.facts);CombatScene room(c,state,e);room.update({});require(state.count(87)==2&&state.count(88)==1,"Active tooth page removes one duplicate per iteration");room.update({});room.update({});require(state.count(87)==0,"Repeated updates finish duplicate cleanup");
 }
 {
  auto e=entry(c,{10,21},87);e.map=16;GameSession state(c.items,e.session,c.facts);CombatScene room(c,state,e);auto save=captureSave(c,room,{room.generation(),"m16-e5","BSAA 1F入口"},{});require(prepareRestore(c,decodeSave(c,encodeSave(c,save))).session->count(87)==1,"Unused tooth survives legal save");
  auto bad=save;bad.session.inventory[88]=1;bool rejected=false;try{encodeSave(c,bad);}catch(const std::exception&){rejected=true;}require(rejected,"Active tooth cannot appear at legal save point");
  auto dead=save;dead.session.worldFlags[c.facts.switches.at(255)]=true;
  bool refused=false;try{prepareRestore(c,dead);}catch(const std::exception&){refused=true;}require(refused,"M94 enemy cannot restore dead before pickup activation");
  dead.session.worldFlags[c.facts.switches.at(291)]=true;require(prepareRestore(c,decodeSave(c,encodeSave(c,dead))).session->flag(255),"Activated M94 enemy death remains a legal save state");
 }
 auto e=entry(c,{7,7},87);GameSession state(c.items,e.session,c.facts);CombatScene room(c,state,e);room.update({});auto request=room.requestToolUse();require(request.has_value(),"Natural tooth use opens after rescue capability delivery");
 auto enabled=c;enabled.catalog["flows"]["tooth-use"]["runtime_enabled"]=true;GameSession inventory(enabled.items,e.session,enabled.facts);CombatScene use(enabled,inventory,e);use.update({});request=use.requestToolUse();require(request&&use.useTool(*request),"Reviewed test request starts finite tooth flow");InputFrame z;z.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;
 for(int n=0;n<6000&&!inventory.flag(318);++n){use.update(n%12==0?z:InputFrame{});}require(inventory.flag(318)&&inventory.count(91)==1&&inventory.count(88)==0,"Tooth is consumed and gemstone/request commit together");require(!use.useTool(*request),"Completed tool request cannot replay");
 auto staleEntry=entry(enabled,{7,7},87);GameSession staleState(enabled.items,staleEntry.session,enabled.facts);CombatScene old(enabled,staleState,staleEntry);old.update({});auto stale=old.requestToolUse();require(stale.has_value(),"Valid test request");old.deactivate();require(!old.useTool(*stale),"Destroyed scene rejects old request");
 std::cout<<"Tooth tool checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
