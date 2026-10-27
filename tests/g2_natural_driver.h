#pragma once
#include "g1_piano_driver.h"
#include <queue>
inline std::vector<InputAction> registrationPath(const Json& p,int from,int to){
 std::queue<int> q;q.push(from);std::map<int,std::vector<InputAction>> seen{{from,{}}};
 while(!q.empty()){int k=q.front();q.pop();if(k==to){return seen.at(k);}for(const auto& [name,a]:std::initializer_list<std::pair<const char*,InputAction>>{{"up",InputAction::Up},{"right",InputAction::Right},{"left",InputAction::Left},{"down",InputAction::Down}}){int n=p.at("neighbors").at(std::to_string(k)).at(name);if(!seen.contains(n)){seen[n]=seen.at(k);seen[n].push_back(a);q.push(n);}}}throw std::runtime_error("Unreachable key");
}
struct G2NaturalDriver:PianoDriver{
 using PianoDriver::PianoDriver;
 void lower(){for(int attempt=0;attempt<3&&game.raised();++attempt){for(int n=0;n<24;++n){InputFrame in;in.buttons[int(InputAction::Weapon)].held=true;tick(in);}for(int n=0;n<100;++n){tick();}}need(!game.raised(),"G2-03 repeated source lower gesture failed");}
 bool mixMedicine(){const auto& c=game.content();if(!c.expansion.mixingRecipe||c.expansion.mixing.is_null()){return false;}const auto& recipe=*c.expansion.mixingRecipe;const int skill=c.expansion.mixing.at("enabled_skill");if(!game.session().knowsSkill(skill)||!game.session().canMix(recipe)||game.session().count(recipe.result)>=99){return false;}const auto stock=game.session().exportState().inventory;press(InputAction::Cancel);select(1);select(skill);for(int n=0;n<2000&&game.mode()!=ApplicationMode::Equipment;++n){tick(n%8==0?key():InputFrame{});}need(game.mode()==ApplicationMode::Equipment,"Public mixing did not return to menu");closeMenu();for(auto [id,amount]:recipe.ingredients){need(game.session().count(id)==stock.at(id)-amount,"Public mixing did not consume actual ingredients");}const int before=stock.contains(recipe.result)?stock.at(recipe.result):0;need(game.session().count(recipe.result)==std::min(99,before+recipe.count),"Public mixing result absent");return true;}
 int healingItem()const{int medicine=0,value=0;for(const auto& [id,d]:game.content().items){const int score=d.medicine.hpPercent+d.medicine.hpPoints;if(d.equipmentType==6&&score>value&&game.session().count(id)&&game.session().canSelectItem(id)){medicine=id;value=score;}}return medicine;}
 bool healTo(int target){bool used=false;while(game.session().hp()<target){const int medicine=healingItem();if(!medicine){break;}const int hp=game.session().hp(),count=game.session().count(medicine);item(medicine);need(game.session().hp()>hp&&game.session().count(medicine)<count,"Public medicine did not consume and heal actual stock");used=true;}return used;}
 int fireRecoveryTicks(int weapon)const{
  const auto& binding=game.content().catalog.at("weapon_bindings").at(std::to_string(weapon));
  std::string profile=binding.at("fire");if(game.session().flag(48)&&binding.contains("quick_fire")){profile=binding.at("quick_fire");}
  int duration=0;for(const auto& id:game.content().actionProfiles.at(profile).at("cue_order")){
   if(id=="transact-gl-reload"){continue;}const auto& cue=game.content().cue(id.get<std::string>());
   if(cue.at("kind")=="wait"){duration+=std::max(1,cue.at("tenths").get<int>()*6);}
  }return duration;
 }
 // Finish the current public action, then hold/release Shift and recover menu controls.
 void lowerForMenu(int readyTick){
  while(ticks<readyTick){need(game.session().hp()>0,"Public fire recovery died before lower");tick();}
  for(int attempt=0;attempt<3&&game.raised();++attempt){
   for(int n=0;n<24&&game.raised();++n){InputFrame hold;hold.buttons[int(InputAction::Weapon)].held=true;tick(hold);}tick();
   for(int n=0;n<100&&game.raised();++n){tick();}
  }
  for(int n=0;n<200&&(game.session().flag(45)||game.session().flag(10)||!game.session().flag(1)||!game.session().flag(18)||game.session().flag(17)||game.session().flag(19));++n){
   need(game.session().hp()>0,"Public lower recovery died before menu");tick(n%2==0?key(n%4?InputAction::Right:InputAction::Up):InputFrame{});
  }
  need(!game.raised()&&!game.session().flag(45)&&!game.session().flag(10)&&game.session().flag(1)&&game.session().flag(18)&&!game.session().flag(17)&&!game.session().flag(19),"Public menu controls did not recover");
 }
 // Select an owned weapon with real ammunition through the existing public menu.
 void equipAvailable(std::initializer_list<int> candidates){
  const auto& session=game.session();
  for(int id:candidates){
   if(session.equipped(1)!=id&&session.count(id)==0){continue;}
   const auto& item=game.content().items.at(id);
   const bool reserve=item.regularReload&&(session.count(item.looseItem)>0||session.count(item.boxItem)>0);
   if(session.magazine(id)==0&&!reserve){continue;}
   equip(id);need(game.session().equipped(1)==id,"Public ammunition-aware equipment selection failed");return;
  }
  throw std::runtime_error("Actual owned weapons have no ammunition");
 }
 void saveMonitor(int slot,bool cancel=false){walk({8,7});move(Direction::Up);tick(key());need(game.mode()==ApplicationMode::Saving,"G2-03 monitor save absent");for(int n=0;n<200;++n){tick();}tick(key());need(game.fileMenuView().has_value(),"G2-03 slots absent");if(cancel){tick(key(InputAction::Cancel));need(game.mode()==ApplicationMode::Playing,"G2-03 save cancel failed");return;}for(int n=1;n<slot;++n){tick(key(InputAction::Down));}tick(key());if(auto v=game.fileMenuView();v&&v->confirmation){if(v->answer){tick(key(InputAction::Up));}tick(key());}need(game.mode()==ApplicationMode::Playing,"G2-03 save failed");}
 void enterLift(){actObject("m151-e8");actObject("m155-e6");actObject("m157-e15");need(game.mapId()==160,"G2-03 lift absent");}
 void enterHunters(){actObject("m160-e10");need(game.mapId()==162,"G2-03 Hunter room absent");}
 void back(){actObject("m162-e6");need(game.mapId()==160,"G2-03 Hunter return absent");actObject("m160-e7");actObject("m157-e2");actObject("m155-e2");need(game.mapId()==151,"G2-03 monitor return absent");}
 struct Target{std::string id;int damage,state,dead;};
 std::vector<Target> alive(){std::vector<Target> out;auto& s=game.session();for(const auto& [id,e]:game.content().enemies){if(e.map==game.mapId()&&!s.flag(e.deadFlag)){out.push_back({id,s.variable(e.rules.at("damage_variable")),s.variable(e.rules.at("state_variable")),e.deadFlag});}}for(const auto& [id,e]:game.content().expansion.enemies){if(e.map==game.mapId()&&!s.flag(e.deadFlag)){int v=e.rules.at("entry_state_variable");out.push_back({id,s.variable(v-2),s.variable(v),e.deadFlag});}}return out;}
 void fight(int maxShots=0){int fired=0;for(int n=0;n<25000;++n){auto& s=game.session();need(s.hp()>0&&game.mode()==ApplicationMode::Playing,"G2-03 natural fight died");auto enemies=alive();if(enemies.empty()){break;}if(s.variable(635)==1&&s.hp()<65&&game.foreground()==ForegroundKind::None&&s.flag(1)&&!s.flag(45)&&!s.flag(10)&&healTo(100)){continue;}InputFrame in;const int weapon=s.equipped(1),rounds=s.magazine(weapon);
  if(game.foreground()!=ForegroundKind::None||game.dialogue().active()){if(n%8==0){in=key();}}
  else if(!game.raised()){in=key(InputAction::Weapon);}
  else{auto p=game.world().player().motion.tile();auto it=std::min_element(enemies.begin(),enemies.end(),[&](const auto& a,const auto& b){auto x=findCharacter(game.world(),a.id).motion.tile(),y=findCharacter(game.world(),b.id).motion.tile();return std::abs(x.x-p.x)+std::abs(x.y-p.y)<std::abs(y.x-p.x)+std::abs(y.y-p.y);});auto t=findCharacter(game.world(),it->id).motion.tile();int dx=t.x-p.x,dy=t.y-p.y;Direction face=std::abs(dx)>std::abs(dy)?(dx>0?Direction::Right:Direction::Left):(dy>0?Direction::Down:Direction::Up);
   if(n%500==0){std::cout<<"G2-03 fight map="<<game.mapId()<<" n="<<n<<" weapon="<<weapon<<" rounds="<<rounds<<" hp="<<s.hp()<<" target="<<it->id<<" state="<<it->state<<" damage="<<it->damage<<'\n';}
   if(it->damage>=100||it->state>=100){}
   else if(it->state>=7&&it->state<=9&&game.mapId()==160){in.movement=next(game.world(),it->state==7?t:adjacentTile(t,Direction::Down));}
   else if((dx&&dy)||std::abs(dx)+std::abs(dy)>4){for(Direction side:{Direction::Down,Direction::Up,Direction::Right,Direction::Left}){auto step=next(game.world(),adjacentTile(t,side));if(step){in.movement=step;break;}}}
   else if(game.world().player().motion.facing()!=face){in.buttons[int(InputAction::Weapon)].held=true;in.movement=face;}
   else if(n%(game.mapId()==162?6:12)==0){need(rounds>0||s.hasReserve(),"G2-03 actual magazine and reserve exhausted");in=key(InputAction::Attack);}
  }
  tick(in);fired+=std::max(0,rounds-game.session().magazine(weapon));if(maxShots&&fired>=maxShots){break;}if(n==24999){throw std::runtime_error("G2-03 fight stalled");}
 }lower();std::cout<<"G2-03 real battle fired="<<fired<<" HP="<<game.session().hp()<<'\n';}
 void barrel(int weapon=22){walk({25,17});settle();move(Direction::Down);tick(key(InputAction::Weapon));for(int n=0;n<20;++n){tick();}need(game.raised()&&game.session().equipped(1)==weapon&&game.session().magazine(weapon)>0,"G2-03 natural actual rocket absent");const int rounds=game.session().magazine(weapon);tick(key(InputAction::Attack));for(int n=0;n<1500&&!game.session().flag(446);++n){tick();need(game.session().hp()>0,"G2-03 barrel approach died");}need(game.session().flag(446)&&game.session().magazine(weapon)==rounds-1,"G2-03 real rocket failed to explode barrel once");lower();std::cout<<"G2-03 barrel HP="<<game.session().hp()<<" mode="<<int(game.mode())<<" S1="<<game.session().flag(1)<<" S18="<<game.session().flag(18)<<" remaining="<<alive().size()<<'\n';need(game.session().hp()>0&&game.mode()==ApplicationMode::Playing,"G2-03 natural barrel aftermath died");}
 void monitor(int slot,bool cancel=false){saveMonitor(slot,cancel);}
 int keyId=200;
 void ambushFight(){int fired=0;for(int n=0;n<12000&&!game.session().flag(402);++n){auto& s=game.session();need(s.hp()>0&&game.mode()==ApplicationMode::Playing,"Natural05 died");const auto p=game.world().player().motion.tile(),t=findCharacter(game.world(),"m157-e4").motion.tile();const int state=s.variable(91),damage=s.variable(89),weapon=s.equipped(1),rounds=s.magazine(weapon);InputFrame in;
  if(game.foreground()!=ForegroundKind::None||game.dialogue().active()){if(n%8==0){in=key();}}
  else if(!game.raised()){in=key(InputAction::Weapon);}
  else if(damage>=100){}
  else if(state>=7&&state<=9){in.movement=next(game.world(),adjacentTile(t,Direction::Down));}
  else if((p.x!=t.x&&p.y!=t.y)||std::abs(p.x-t.x)+std::abs(p.y-t.y)>5){for(auto side:{Direction::Down,Direction::Up,Direction::Right,Direction::Left}){auto step=next(game.world(),adjacentTile(t,side));if(step){in.movement=step;break;}}}
  else {int dx=t.x-p.x,dy=t.y-p.y;auto d=std::abs(dx)>std::abs(dy)?(dx>0?Direction::Right:Direction::Left):(dy>0?Direction::Down:Direction::Up);if(game.world().player().motion.facing()!=d){in.buttons[int(InputAction::Weapon)].held=true;in.movement=d;}else if(n%12==0){need(s.magazine(weapon)>0,"Natural05 real ammunition exhausted");in=key(InputAction::Attack);}}
  if(n%400==0){std::cout<<"FIGHT05 n="<<n<<" p="<<p.x<<","<<p.y<<" e="<<t.x<<","<<t.y<<" state="<<state<<" damage="<<damage<<" HP="<<s.hp()<<" I22="<<rounds<<'\n';}tick(in);fired+=std::max(0,rounds-s.magazine(weapon));
 }
 need(game.session().flag(402)&&fired>0,"Natural05 actual licker kill absent");lower();std::cout<<"REAL05 fired="<<fired<<" HP="<<game.session().hp()<<" count35="<<game.session().variable(35)<<'\n';}
 void reception(){actObject("m151-e8");actObject("m155-e5");actObject("m156-e5");need(game.mapId()==159,"Reception entry absent");}
 void receptionBack(){actObject("m159-e4");actObject("m156-e2");actObject("m155-e2");need(game.mapId()==151,"Reception return absent");}
 void prompt(){for(int n=0;n<3000&&!game.inputPromptView().cursor;++n){tick(n%8==0?key():InputFrame{});}need(game.inputPromptView().cursor.has_value()&&game.session().flag(448)&&!game.session().flag(1)&&!game.session().flag(18),"Natural consultation locks absent");}
 void startReception(){const auto t=game.content().objects.at("m159-e3").tile;walk(adjacentTile(t,Direction::Down));move(Direction::Up);tick(key());prompt();}
 void choose(int row){prompt();for(int i=0;i<row;++i){press(InputAction::Down);}press();}
 void open(){choose(1);for(int n=0;n<3000&&game.foreground()!=ForegroundKind::Registration;++n){tick(n%8==0?key():InputFrame{});}need(game.foreground()==ForegroundKind::Registration,"Natural keyboard absent");for(int i=0;i<32;++i){tick();}keyId=200;}
 void type(int target){for(auto a:registrationPath(game.content().catalog.at("registration"),keyId,target)){press(a);}keyId=target;press();}
 void credentials(){for(int k:{102,301,302,102,208,208,200,209,304,2,1,1,9,5,3,8,209}){type(k);}prompt();}
 void end(){choose(2);settle();need(game.session().flag(1)&&game.session().flag(18)&&!game.session().flag(448),"Natural consultation leaked locks");}
};
