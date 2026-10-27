#include "save_snapshot.h"
#include <iostream>
#include <stdexcept>
void require(bool b,const char* m){if(!b){throw std::runtime_error(m);}}
InputFrame press(InputAction a=InputAction::Confirm){InputFrame i;i.buttons[static_cast<int>(a)].pressed=true;return i;}
int main(int argc,char** argv){
 if(argc!=2){return 2;}
 InputPrompt requests;ChoicePrompt original;original.options={MessageLine{"",{{MessageTokenKind::Text,U"是",0}}},MessageLine{"",{{MessageTokenKind::Text,U"否",0}}}};
 auto identity=requests.request(original,1);ChoicePrompt duplicate=original;duplicate.options.resize(1);bool refused=false;try{requests.request(duplicate,1);}catch(const std::runtime_error&){refused=true;}requests.update(press(InputAction::Down));requests.update(press());auto result=requests.takeCompletion(identity);
 require(refused&&result&&std::get<ChoiceSelected>(result->result).index==1,"rejected duplicate prompt preserves existing navigation and result");
 auto c=loadCombatContent(argv[1]);
 auto e=c.entries.at("defeated-claimed");e.map=17;e.playerTile={3,8};e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();
 const auto& key=c.objects.at("m17-e7");e.playerTile=adjacentTile(key.tile,Direction::Down);e.direction=Direction::Up;c.entries["key-fixture"]=e;
 GameSession session(c.items,e.session);CombatScene scene(c,session,"key-fixture",[](int lo,int){return lo;});
 scene.update(press());for(int n=0;n<3000&&scene.foreground()!=ForegroundKind::None;++n){scene.update(n%12==0?press():InputFrame{});}
 require(session.count(60)==1&&session.flag(59),"actual key investigation awards once and commits S59");
 scene.update(press());for(int n=0;n<600;++n){scene.update(n%12==0?press():InputFrame{});}require(session.count(60)==1,"key cannot be claimed twice");
 auto ammo=e;ammo.playerTile=adjacentTile(c.objects.at("m17-e8").tile,Direction::Down);ammo.direction=Direction::Up;
 GameSession supplies(c.items,ammo.session);const int before=supplies.count(31);CombatScene ammoroom(c,supplies,ammo);
 for(int repeat=0;repeat<2;++repeat){ammoroom.update(press());for(int n=0;n<3000&&ammoroom.foreground()!=ForegroundKind::None;++n){ammoroom.update(n%12==0?press():InputFrame{});}}
 require(supplies.flag(65)&&supplies.count(31)==before+15,"Map17 ammo grants fifteen once and persists claim");
 auto entryAt=[&](int map,TilePosition tile,Direction direction,const SessionEntry& state){auto next=e;next.map=map;next.playerTile=tile;next.direction=direction;next.session=state;return next;};
 auto settle=[&](CombatScene& room){if(room.foreground()==ForegroundKind::None){return;}for(int n=0;n<6000;++n){if(room.inputPromptView().phase!=DialoguePhase::Hidden){return;}room.update(n%12==0?press():InputFrame{});if(room.foreground()==ForegroundKind::None&&!room.world().player().motion.moving()){return;}}throw std::runtime_error("Interaction stuck");};
 std::vector<SessionEntry> saves{e.session,session.exportState()};
 for(bool discard:{false,true}){
  auto start=entryAt(13,{23,15},Direction::Up,session.exportState());GameSession state(c.items,start.session);CombatScene room(c,state,start,[](int lo,int){return lo;});
  room.update(press());settle(room);require(room.inputPromptView().phase!=DialoguePhase::Hidden&&!state.flag(60),"key choice precedes unlock commit");
  require(room.inputPromptView().lines.size()==4&&room.inputPromptView().lines[1].find(U"要扔掉吗")!=std::u32string::npos,"discard question remains above choices in same window");
  room.update(press(InputAction::Cancel));require(room.inputPromptView().phase!=DialoguePhase::Hidden,"key choice cannot cancel");
  if(!discard){room.update(press(InputAction::Down));}room.update(press());room.update({});settle(room);
  require(state.flag(60)&&state.count(60)==(discard?0:1)&&room.mapId()==13,"both key choices unlock without automatic transfer");saves.push_back(state.exportState());
  room.update(press());for(int n=0;n<300&&room.mapId()==13;++n){room.update({});}require(room.mapId()==18&&room.world().player().motion.tile()==TilePosition{4,13},"next investigation enters real Map18");
 }
 auto start=entryAt(14,{8,12},Direction::Up,session.exportState());start.session.variables[388]=4321;
 const auto locker=c.objects.at("m14-e10").tile;start.playerTile=adjacentTile(locker,Direction::Down);
 GameSession password(c.items,start.session);CombatScene safe(c,password,start,[](int lo,int){return lo;});
 auto enterNumber=[&](int value){safe.update(press());settle(safe);require(safe.inputPromptView().phase!=DialoguePhase::Hidden,"safe asks four digits");safe.update(press(InputAction::Cancel));require(safe.inputPromptView().phase!=DialoguePhase::Hidden,"numeric X cannot cancel");for(int i=0;i<4;++i){for(int digit=0;digit<value%10;++digit){safe.update(press(InputAction::Up));}value/=10;safe.update(press(InputAction::Left));}safe.update(press());safe.update({});settle(safe);};
 enterNumber(9999);require(!password.flag(459)&&password.count(46)==0&&password.variable(388)==4321,"wrong password leaves reward and original password unchanged");
 enterNumber(4321);require(password.flag(459)&&password.count(46)==1&&password.equipment().armor==45,"correct password grants once without equipping");saves.push_back(password.exportState());
 safe.update(press());settle(safe);require(password.count(46)==1,"reward cannot repeat");
 for(auto state:saves){
  auto saveEntry=entryAt(16,{10,21},Direction::Up,state);GameSession saving(c.items,state);CombatScene atCrow(c,saving,saveEntry);
  auto snapshot=captureSave(c,atCrow,{atCrow.generation(),"m16-e5","BSAA 1F入口"},{});auto loaded=prepareRestore(c,decodeSave(c,encodeSave(c,snapshot)));
  require(loaded.session->count(60)==saving.count(60)&&loaded.session->flag(60)==saving.flag(60)&&loaded.session->flag(59)==saving.flag(59)&&loaded.session->flag(459)==saving.flag(459)&&loaded.session->variable(388)==saving.variable(388),"five progress states survive legitimate save context and independent Load");
  auto revisit=entryAt(13,{23,15},Direction::Up,loaded.session->exportState());GameSession restored(c.items,revisit.session);CombatScene door(c,restored,revisit);door.update(press());settle(door);
  if(saving.flag(60)){require(door.mapId()==18,"restored unlocked door works with or without key");}
  else if(saving.count(60)){require(door.inputPromptView().phase!=DialoguePhase::Hidden&&!restored.flag(60),"restored unspent key still offers discard choice");}
  else{require(door.mapId()==13&&!restored.flag(60)&&door.inputPromptView().phase==DialoguePhase::Hidden,"restored no-key progress still refuses locked door");}

 }
 for(int event=11;event<=18;++event){
  const auto id="m18-e"+std::to_string(event);auto report=entryAt(18,adjacentTile(c.objects.at(id).tile,Direction::Down),Direction::Up,session.exportState());GameSession reader(c.items,report.session);CombatScene room(c,reader,report);
  for(int repeat=0;repeat<2;++repeat){room.update(press());require(room.dialogue().view().phase!=DialoguePhase::Hidden,"report opens each time");settle(room);}require(reader.count(60)==1,"reports do not grant or consume items");
 }
 auto window=entryAt(18,{17,6},Direction::Right,session.exportState());GameSession watching(c.items,window.session);CombatScene view(c,watching,window);view.update({});require(!watching.flag(56),"entering room does not start window event");view.update(press());settle(view);require(watching.flag(56),"investigation commits first-window event");
 view.update(press());for(int n=0;n<300&&!view.exitRequest();++n){view.update({});}require(view.exitRequest()&&view.exitRequest()->map==19&&view.mapId()==18,"next window investigation hands off Map19 without fabricated scene");
 auto varied=c;auto node=varied.objects.extract("m17-e7");node.key()="varied-key";node.mapped().id="varied-key";for(auto& variant:node.mapped().variants){variant.definition.id="varied-key";}varied.objects.insert(std::move(node));
 varied.catalog["flows"]["m17-e7-p2"]["object"]="varied-key";for(auto& step:varied.catalog["flows"]["m17-e7-p2"]["steps"]){if(step["kind"]=="grant"){step["count"]=2;}}
 GameSession other(varied.items,e.session);CombatScene changed(varied,other,e);changed.update(press());settle(changed);require(other.count(60)==2&&other.flag(59),"content identity and reward amount vary without runtime coordinate or id assumption");
 std::cout<<"F3 return checks passed\n";
}
