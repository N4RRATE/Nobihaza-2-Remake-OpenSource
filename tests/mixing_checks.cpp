#include "game_application.h"
#include <iostream>
#include <fstream>
#include "expanded_content_validation.h"
#include <stdexcept>
void require(bool v,const char* why){if(!v){throw std::runtime_error(why);}}
InputFrame press(InputAction a){InputFrame i;i.buttons[static_cast<int>(a)].pressed=true;return i;}
void tick(GameApplication& a,int n=180){while(n--){a.update({});}}
void openMix(GameApplication& a){if(a.mode()!=ApplicationMode::Equipment){a.update(press(InputAction::Cancel));}a.update(press(InputAction::Down));a.update(press(InputAction::Confirm));require(a.menuView().page==MenuPage::Skills,"skills page");a.update(press(InputAction::Confirm));}
int main(int argc,char** argv){try{if(argc!=2)return 2;auto c=loadCombatContent(argv[1]);auto e=c.entries.at("defeated-claimed");e.session.skills={3};e.session.inventory[2]=1;e.session.inventory[4]=1;e.session.inventory[5]=0;c.entries["mix-fixture"]=e;
 GameApplication a(c,"mix-fixture");tick(a,1);openMix(a);require(a.mode()==ApplicationMode::Playing,"Mix request must leave menu for map confirmation");tick(a);require(a.inputPromptView().cursor.has_value(),"CE37 question and choice must appear");a.update(press(InputAction::Confirm));tick(a,2);require(a.session().count(2)==0&&a.session().count(4)==0&&a.session().count(5)==1,"Confirmed recipe consumes once and grants result");

 auto returnMenu=[](GameApplication& g){for(int i=0;i<1600&&g.mode()!=ApplicationMode::Equipment;++i){g.update(i%20==0?press(InputAction::Confirm):InputFrame{});}require(g.mode()==ApplicationMode::Equipment,"CE37 reopens root menu");};
 returnMenu(a);require(!a.session().flag(49)&&a.session().flag(19)&&!a.session().flag(1)&&!a.session().flag(18),"CE37 retains control until reopened menu closes");
 a.update(press(InputAction::Cancel));require(a.mode()==ApplicationMode::Playing&&a.session().flag(1)&&a.session().flag(18)&&!a.session().flag(19),"Exit menu restores control once");
 auto many=e;many.session.inventory[2]=2;many.session.inventory[4]=2;c.entries["many"]=many;
 GameApplication repeat(c,"many");tick(repeat,1);openMix(repeat);tick(repeat);repeat.update(press(InputAction::Cancel));returnMenu(repeat);require(repeat.session().count(2)==2&&repeat.session().count(4)==2,"X maps to No without consuming");
 for(int n=0;n<2;++n){openMix(repeat);tick(repeat);repeat.update(press(InputAction::Confirm));repeat.update(press(InputAction::Confirm));returnMenu(repeat);require(repeat.session().count(5)==n+1,"Each repeated menu request commits exactly once");}
 openMix(repeat);tick(repeat);require(!repeat.inputPromptView().cursor,"Missing ingredients do not offer choice");returnMenu(repeat);require(repeat.session().count(5)==2,"Missing path leaves result intact");
 auto full=e;full.session.inventory[5]=99;c.entries["full"]=full;GameApplication cap(c,"full");tick(cap,1);openMix(cap);tick(cap);cap.update(press(InputAction::Confirm));tick(cap,2);require(cap.session().count(5)==99&&cap.session().count(2)==0&&cap.session().count(4)==0,"Full output still consumes source ingredients");
 auto disabled=e;disabled.session.skills={4};c.entries["disabled"]=disabled;GameApplication no(c,"disabled");tick(no,1);no.update(press(InputAction::Cancel));no.update(press(InputAction::Down));no.update(press(InputAction::Confirm));require(!no.menuView().rows.at(0).enabled,"Distinct Skill4 disabled");no.update(press(InputAction::Confirm));require(no.mode()==ApplicationMode::Equipment&&no.session().count(2)==1,"Disabled skill never submits");
 GameSession session(c.items,e.session,c.facts);CombatScene old(c,session,e);old.update({});auto request=old.requestMixing();require(request.has_value(),"Scene issues context request");auto newer=old.requestMixing();require(!old.startMixing(*request),"Old request rejected");require(old.startMixing(*newer)&&!old.startMixing(*newer),"Request consumes once");old.deactivate();require(!old.startMixing(*newer),"Deactivated scene rejects old result");
 CombatScene replacement(c,session,e);require(!replacement.startMixing(*newer),"New scene cannot consume old request");
 GameApplication destroyed(c,"many");tick(destroyed,1);openMix(destroyed);tick(destroyed);destroyed.restartDevelopmentEntry("full");destroyed.update(press(InputAction::Confirm));require(destroyed.session().count(2)==1&&destroyed.session().count(5)==99,"Restart discards old confirmation");
 auto variation=e.session;variation.inventory[2]=3;variation.inventory[4]=1;variation.inventory[5]=98;GameSession varied(c.items,variation,c.facts);MixingRecipe recipe{{{2,2},{4,1}},5,2};require(varied.mix(recipe)&&varied.count(2)==1&&varied.count(4)==0&&varied.count(5)==99,"Independent quantity sample follows consume then clamp");require(!varied.mix(recipe)&&varied.count(2)==1,"Insufficient recipe is atomic");
 // Removing any ingredient resets its current-piece usage, even if a stack remains.
 auto defs=c.items;defs.at(2).uses=3;variation.inventory[2]=3;variation.inventory[4]=1;variation.itemUses[2]=1;GameSession used(defs,variation,c.facts);require(used.mix(recipe)&&used.itemUses(2)==0,"CE37 RemoveItem resets used current piece");

 auto reading=c;reading.entries["many"].session.inventory[180]=1;
 GameApplication book(reading,"many");tick(book,1);book.update(press(InputAction::Cancel));book.update(press(InputAction::Confirm));
 for(int n=0;n<100&&book.menuView().rows.at(book.menuView().selection).id!=180;++n){book.update(press(InputAction::Right));}book.update(press(InputAction::Confirm));
 for(int n=0;n<6000&&book.mode()!=ApplicationMode::Equipment;++n){book.update(n%3==0?press(InputAction::Confirm):InputFrame{});book.takePresentation();}
 require(book.mode()==ApplicationMode::Equipment,"Reading returns to menu");openMix(book);require(book.mode()==ApplicationMode::Reading&&!book.session().flag(49),"Mix waits for book presentation cleanup");bool erased=false;
 for(int n=0;n<120&&book.mode()!=ApplicationMode::Playing;++n){book.update({});for(const auto& event:book.takePresentation()){erased=erased||event.id=="ce151-c79";}}
 require(erased&&book.mode()==ApplicationMode::Playing&&book.foreground()==ForegroundKind::Mixing,"Book curtain is removed before map confirmation");
 auto dead=variation;dead.hp=0;dead.itemUses.clear();GameSession corpse(c.items,dead,c.facts);require(!corpse.mix(recipe)&&corpse.count(2)==3,"Dead actor cannot combine");

 for(int map:{16,22}){
  auto state=e;state.map=map;const std::string crow=map==16?"m16-e5":"m22-e2";state.playerTile=adjacentTile(c.objects.at(crow).tile,Direction::Down);state.direction=Direction::Up;
  state.session.inventory[62]=1;for(int f:{66,67,68,71,72})state.session.flags[f]=true;state.session.flags[73]=true;if(!c.expansion.meetingStory.is_null())state.session.flags[84]=true;for(const auto& flag:c.expansion.meeting.at("talk_flags")){state.session.flags[flag]=true;}
  GameSession durable(c.items,state.session,c.facts);CombatScene atSave(c,durable,state);
  auto saved=captureSave(c,atSave,{atSave.generation(),crow,""},{});auto resumed=prepareRestore(c,decodeSave(c,encodeSave(c,saved)));
  require(resumed.session->knowsSkill(3)&&!resumed.session->knowsSkill(4)&&resumed.session->count(2)==1,"Legal save points retain learned mixing and materials");
  for(int broken=0;broken<4;++broken){auto bad=saved;
   if(broken==0){bad.session.skills.insert(4);}else if(broken==1){bad.session.skills.erase(3);bad.session.skills.insert(4);}else if(broken==2){bad.session.worldFlags[c.facts.switches.at(77)]=false;}else{bad.session.worldFlags[c.facts.switches.at(73)]=false;}
   bool rejected=false;try{encodeSave(c,bad);}catch(const std::exception&){rejected=true;}require(rejected,"Illegal learned/safe-point mixing state rejected");
  }
 }

 std::ifstream stream(argv[1]);auto source=nlohmann::json::parse(stream);
 for(int bad=0;bad<4;++bad){auto changed=source;
  if(bad==0){changed["f4"]["mixing"]["begin"][0]={{"kind","grant"},{"item",2},{"count",1},{"source",source["f4"]["mixing"]["source"]}};}
  else if(bad==1){changed["f4"]["mixing"]["finish"]=source["f4"]["mixing"]["missing"];}
  else if(bad==2){changed["f4"]["mixing"]["question"][0]["cue"]="ce151-c1";}
  else{changed["f4"]["mixing"]["enabled_skill"]=4;}
  bool rejected=false;try{validateExpandedContent(c,changed);}catch(const std::exception&){rejected=true;}require(rejected,"Nonexecutable mixing data must fail during content validation");
 }
 std::cout<<"Mixing checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
