#include "hunter_behavior.h"
#include "combat_scene.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
using Json=nlohmann::json;
HunterBehavior::HunterBehavior(CombatScene& scene,const EnemyDefinition& definition):definition_(definition),host_(scene,definition.id){
 for(const auto* key:{"body_object","damage_variable","state_variable","reaction_variable","counter_variable","body_variables","activation_fact","threshold","scout_distance","cues"}){(void)rules().at(key);}
}
int HunterBehavior::value(const char* key)const{return host_.session().variable(rules().at(key));}
void HunterBehavior::set(const char* key,int n){host_.session().setVariable(rules().at(key),n);}
int HunterBehavior::sourceState()const{return value("state_variable");}
int HunterBehavior::accumulatedDamage()const{return value("damage_variable");}
EnemyMode HunterBehavior::mode()const{
 if(host_.session().flag(definition_.deadFlag)){return EnemyMode::Corpse;}
 if(!host_.session().flag(rules().at("activation_fact"))||retired_){return EnemyMode::Dormant;}
 if(endingPage_){return EnemyMode::Ending;}if(sourceState()>=100){return EnemyMode::HitRecovery;}
 return page_>=12?EnemyMode::Windup:EnemyMode::Pursuit;
}
void HunterBehavior::cancel(){host_.cancelRoutes();autonomy_.reset();delayed_.reset();phase_=wait_=arcStep_=0;if(pictureShown_){picture(14,65);pictureShown_=false;}}
void HunterBehavior::retire(){cancel();retired_=true;}
void HunterBehavior::applyPage(int p){
 page_=p;autonomy_.reset();const auto& variants=host_.content().objects.at(id()).variants;
 const auto it=std::find_if(variants.begin(),variants.end(),[&](const auto& v){return v.page==((sourceState()==102&&p==rules().value("barrel_action_page",-1))?p:rules().contains("source_pages")?rules().at("source_pages").at(std::to_string(p)).get<int>():p);});if(it==variants.end()){throw std::runtime_error("Missing Hunter source page");}
 const auto* selected=&*it;
 if(p>=22&&rules().value("corpse_source_selection",false)){
  selected=host_.content().activeVariant(host_.content().objects.at(id()),host_.session());
  if(!selected){throw std::runtime_error("Missing Hunter corpse source page");}
 }
 auto a=host_.actor();const auto& d=selected->definition;a.spriteId=d.spriteId;a.layer=d.layer;a.through=a.routeThrough.value_or(d.through);a.setSpeedLevel(d.moveSpeed);
 a.setAnimation(CharacterAnimation(animationForSpeed(d.moveSpeed),d.initialColumn,d.animationType),a.motion.facing());host_.setActor(a);
 if(!selected->autonomousRoute.steps.empty()){autonomy_.emplace(selected->autonomousRoute,id());}
}
void HunterBehavior::change(int next){cancel();set("state_variable",next);host_.session().setFlag(rules().at("attack_fact"),next==2||next==3||next==5||next==6||next==10||next==11);applyPage(next==102?rules().at("barrel_action_page").get<int>():next==0?9:next==1?10:next==2?13:next==3?14:next==4?11:next==5||next==6?15:next==10?16:next==11?17:next==100?18:next==101?19:12);}
bool HunterBehavior::activateState(int next){if(next!=0&&next!=1&&next!=2&&next!=3&&next!=5&&next!=10&&next!=11&&next!=100&&next!=101){return false;}if(host_.session().flag(definition_.deadFlag)){return false;}change(next);return true;}
bool HunterBehavior::play(int p,int command){
 const auto& key=rules().at("cues").at(std::to_string(p)).at(std::to_string(command));const auto& c=host_.content().cue(key.get<std::string>());
 if(c.at("kind")=="wait"){wait_=std::max(1,c.at("tenths").get<int>()*6);return false;}
 host_.cue(key);return true;
}
bool HunterBehavior::sequence(int p,std::initializer_list<int> commands){while(phase_<int(commands.size())){const int n=*(commands.begin()+phase_++);if(!play(p,n)){return false;}}return true;}
void HunterBehavior::picture(int p,int n){const std::string key=rules().at("cues").at(std::to_string(p)).at(std::to_string(n));const auto& vars=rules().at("body_variables");const GridPosition sampled{double(host_.session().variable(vars[2])),double(host_.session().variable(vars[3]))};host_.present({PresentationEvent::Kind::Cue,key,{},host_.screenPicturePosition(sampled),0,id(),sampled});}
void HunterBehavior::sampleBody(){
 auto& s=host_.session();const auto& o=host_.content().objects.at(rules().at("body_object").get<std::string>());const auto* p=host_.content().activeVariant(o,s);if(!p||p->definition.trigger!=4){return;}
 const auto& v=rules().at("body_variables");const auto tile=host_.actor().motion.tile();s.setVariable(v[0],tile.x);s.setVariable(v[1],tile.y);if(rules().contains("body_register_variable")){s.setVariable(rules().at("body_register_variable"),definition_.sourceOrder);}
 // Highest active cache page determines whether the action keeps its screen sample.
 if(rules().contains("cache_tile_only_pages")?std::find(rules().at("cache_tile_only_pages").begin(),rules().at("cache_tile_only_pages").end(),p->page)==rules().at("cache_tile_only_pages").end():p->page!=5&&p->page!=7){const auto foot=host_.actorScreenFoot();s.setVariable(v[2],int(foot.x));s.setVariable(v[3],int(foot.y)-8);if(!rules().contains("body_half_by_action_page")){s.setVariable(v[4],8);s.setVariable(v[5],5);}}
}
int HunterBehavior::forwardDistance(int maximum)const{auto t=host_.actor().motion.tile();for(int n=1;n<=maximum;++n){t=adjacentTile(t,host_.actor().motion.facing());if(t==host_.player().motion.tile()){return n;}}return 0;}
bool HunterBehavior::arcHit()const{const auto a=host_.actor();const auto p=host_.player().motion.tile();const auto f=adjacentTile(a.motion.tile(),a.motion.facing());return a.motion.facing()==Direction::Up||a.motion.facing()==Direction::Down?p.y==f.y&&std::abs(p.x-f.x)<=1:p.x==f.x&&std::abs(p.y-f.y)<=1;}
bool HunterBehavior::fatalHit()const{
 const auto a=host_.actor().motion.tile(),p=host_.player().motion.tile();const auto d=adjacentTile({0,0},host_.actor().motion.facing());
 const int along=(p.x-a.x)*d.x+(p.y-a.y)*d.y;const int across=(p.x-a.x)*d.y-(p.y-a.y)*d.x;
 return along>=0&&along<=1&&std::abs(across)<=1;
}
void HunterBehavior::decide(){
 auto& s=host_.session();const auto a=host_.actor();const auto p=host_.player().motion.tile();const int state=sourceState();
 if(state==0){const int n=forwardDistance(rules().at("scout_distance"));const auto body=host_.actorScreenFoot(),player=host_.sampleActorScreen("player");const double dx=body.x-player.x,dy=body.y-player.y;const bool half=rules().value("half_screen_scout",false);const bool front=!half||(a.motion.facing()==Direction::Down?dy<=0:a.motion.facing()==Direction::Up?dy>=0:a.motion.facing()==Direction::Left?dx>=0:dx<=0);if(n||(front&&std::abs(dx)<=48&&std::abs(dy)<=48)){change(1);return;}}
 else if(state==1){if(rules().value("pursuit_clears_attack",false)){s.setFlag(rules().at("attack_fact"),false);}if(forwardDistance(1)){set("counter_variable",host_.random(0,5));if(value("counter_variable")<=3){cancel();applyPage(12);s.setFlag(rules().at("attack_fact"),true);}else{change(5);}return;}if(forwardDistance(5)){const int variable=rules().value("skill_counter_variable",rules().at("counter_variable").get<int>());s.setVariable(variable,host_.random(0,100));if(s.variable(variable)<=30){change(2);return;}}}
 const auto next=adjacentTile(a.motion.tile(),a.motion.facing());if(host_.terrain(next)==3){const bool vertical=a.motion.facing()==Direction::Up||a.motion.facing()==Direction::Down;const int d=vertical?p.x-a.motion.tile().x:p.y-a.motion.tile().y;if(d){RouteDefinition route;route.frequency=8;route.skipBlocked=true;route.steps.push_back({std::string(directionName(vertical?(d>0?Direction::Right:Direction::Left):(d>0?Direction::Down:Direction::Up))),{},{},0});host_.route(route);}}
 if(autonomy_&&!host_.routePending()){host_.advance(*autonomy_,[&](int id,bool enabled){s.setFlag(id,enabled);});}
}
void HunterBehavior::strike(bool jumpAttack){
 auto& s=host_.session();if(s.flag(13)||!arcHit()){return;}
 play(jumpAttack?(page_==14?14:15):12,jumpAttack?(page_==14?173:172):139);s.setVariable(24,jumpAttack?2:1);s.setFlag(8,false);s.setFlag(8,true);host_.present({PresentationEvent::Kind::Effect,jumpAttack?"blood-medium":"blood-small",{},host_.player().motion.position(),0,id()});s.setVariable(23,rules().at("attack"));
 if(jumpAttack){const int lock=page_==14?rules().at("jump_lock_fact").get<int>():1;s.setFlag(lock,page_==14);host_.sourceJumpHit(rules().at("cues").at(std::to_string(page_)).at(std::to_string((page_==14?181:180)+int(host_.player().motion.facing())*4)));}
 // CE1 samples once per map update; CE12 halves the shared working value in place.
 host_.receiveSourceDamage(s.variable(23));
 if(jumpAttack){s.setFlag(page_==14?rules().at("jump_lock_fact").get<int>():1,page_!=14);}
}
void HunterBehavior::jump(bool big){
 auto& s=host_.session();const int p=big?14:15;
 if(phase_==0){const int direction=int(host_.actor().motion.facing());picture(p,4+direction*4);pictureShown_=true;height_=10;arcStep_=0;phase_=1;}
 if(phase_==1){
  if(autonomy_&&!host_.routePending()){host_.advance(*autonomy_,[&](int id,bool enabled){s.setFlag(id,enabled);});}
  const auto foot=host_.actorScreenFoot();const auto& v=rules().at("body_variables");s.setVariable(v[2],int(foot.x));s.setVariable(v[3],int(foot.y)-height_);picture(p,25);++arcStep_;
  const int t1=big?8:6,t2=big?16:12,t3=big?18:14,t4=big?20:16,t5=big?26:20;
  height_+=arcStep_<t1?4:arcStep_<t2?2:arcStep_<t3?1:arcStep_<t4?-1:arcStep_<t5?-2:-4;
  if(height_<=10&&arcStep_>=t5){height_=10;picture(p,65);pictureShown_=false;play(p,66);if(big){set("state_variable",4);}else if(rules().value("small_jump_landing_state",0)){set("state_variable",rules().at("small_jump_landing_state"));}strike(true);phase_=2;wait_=1;}else{wait_=1;}return;
 }
 if(phase_==2){phase_=3;if(big){play(14,219);}if(!big){change(11);return;}wait_=6;return;}
 if(phase_==3){play(14,221);change(1);}
}
void HunterBehavior::action(){
 auto& s=host_.session();if(wait_>0&&--wait_>0){return;}
 if(page_==12){if(phase_<2){if(!sequence(12,{1,2})){return;}if(!forwardDistance(1)){change(1);return;}}
  if(phase_<7){if(!sequence(12,{1,2,29,30,31,32,33})){return;}strike(false);}
  if(phase_<12){if(!sequence(12,{1,2,29,30,31,32,33,159,160,161,162,163})){return;}s.setFlag(rules().at("attack_fact"),false);if(rules().value("slash_exit",false)){change(1);return;}const auto a=host_.actor().motion.tile(),p=host_.player().motion.tile();const int cue=a.x>p.x?170:a.x<p.x?175:a.y>p.y?180:a.y<p.y?185:0;if(cue){play(12,cue);wait_=12;phase_=13;return;}}
  change(1);return;
 }
 if(page_==13){auto tile=host_.actor().motion.tile();for(int n=0;n<4;++n){tile=adjacentTile(tile,host_.actor().motion.facing());}if(host_.terrain(tile)==3){change(1);return;}if(sequence(13,{29,30,31,32,33})){change(rules().at("fixed_big_jump").get<bool>()||s.hp()>40?3:10);}return;}
 if(page_==14||page_==15){jump(page_==14);return;}
 if(page_==16){if(phase_==0){picture(16,5+int(host_.actor().motion.facing())*4);pictureShown_=true;set("counter_variable",0);phase_=1;}if(phase_==1){if(arcHit()||value("counter_variable")>=15){phase_=2;play(16,100);}else{if(autonomy_&&!host_.routePending()){host_.advance(*autonomy_,[&](int id,bool enabled){s.setFlag(id,enabled);});}set("counter_variable",value("counter_variable")+1);const auto foot=host_.actorScreenFoot();s.setVariable(rules().at("body_variables")[2],int(foot.x));s.setVariable(rules().at("body_variables")[3],int(foot.y)-20);picture(16,90);wait_=1;return;}}
  if(phase_==2){const int image[]={102,123,141,162};const int d=int(host_.actor().motion.facing());picture(16,image[d]);play(16,image[d]+1);if(fatalHit()){play(16,171);play(16,172);s.setVariable(31,5);s.setFlag(15,true);}phase_=3;set("counter_variable",0);}
  if(value("counter_variable")>=15){picture(16,198);pictureShown_=false;change(11);if(s.flag(15)){host_.sourceFatalHit();}}else{set("counter_variable",value("counter_variable")+1);picture(16,191);wait_=1;}return;
 }
 if(page_==17){if(sequence(17,{1,2,3,4,5,6,7,8})){change(1);}return;}
 if(page_==18){s.setFlag(rules().at("attack_fact"),true);if(sequence(18,{4,5})){change(1);}return;}
 if(sourceState()==102){
  if(phase_==0){set("reaction_variable",3);s.setFlag(rules().at("hit_fact"),false);s.setFlag(rules().at("hit_fact"),true);if(!host_.burning()){play(19,5);}play(19,12+int(host_.player().motion.facing())*4);phase_=1;}
  if(host_.routePending()){return;}host_.sound(rules().at("barrel_sound"));set("damage_variable",1000);return;
 }
 if(page_==19){
  if(phase_==0){set("reaction_variable",3);s.setFlag(rules().at("hit_fact"),false);s.setFlag(rules().at("hit_fact"),true);if(!host_.burning()){play(19,5);}play(19,12+int(host_.player().motion.facing())*4);phase_=1;}
  if(phase_==1){if(rules().value("retreat_wait_route",false)&&host_.routePending()){return;}AttackSample current=delayed_.value_or(AttackSample{});current.receiverPage=4;damage(current,true);delayed_.reset();phase_=2;play(19,44);return;}
  if(sequence(19,{12,44,45,46,47,48,49,50})){const auto body=host_.actorScreenFoot(),player=host_.sampleActorScreen("player");if(std::abs(body.x-player.x)<=68&&std::abs(body.y-player.y)<=68){play(19,60);const int next=host_.random(1,2);change(next);if(next==1){s.setFlag(rules().at("attack_fact"),true);applyPage(12);}}else{change(1);}}return;
 }
}
void HunterBehavior::ending(){
 auto& s=host_.session();if(!endingPage_){endingPage_=accumulatedDamage()>=1000?21:20;cancel();applyPage(endingPage_);phase_=0;if(endingPage_!=21){play(20,1);}play(endingPage_,endingPage_==21?1:2);}
 if(wait_>0&&--wait_>0){return;}s.setVariable(rules().at("body_variables")[2],0);s.setVariable(rules().at("body_variables")[3],0);if(endingPage_==21){set("damage_variable",1600);}s.setFlag(definition_.deadFlag,true);applyPage(endingPage_==21?23:22);auto a=host_.actor();const auto corpse=rules().value("death_tile",std::array<int,2>{0,0});a.motion=CharacterMotion({corpse[0],corpse[1]},a.motion.facing(),a.motion.speed());host_.setActor(a);cancel();
}
void HunterBehavior::update(const InputFrame&){
 if(retired_){return;}host_.claimAutonomy();auto& s=host_.session();if(s.flag(definition_.deadFlag)){return;}if(!s.flag(rules().at("activation_fact"))||s.hp()<=0||(s.flag(15)&&page_!=16)){cancel();return;}
 if(accumulatedDamage()>=rules().at("threshold").get<int>()){ending();return;}
 if(!s.flag(18)&&(sourceState()<=1||sourceState()>=100)){return;}
 if(sourceState()==0&&rules().value("direct_pursuit",false)){change(1);}
 const int state=sourceState();const int expected=state==0?9:state==1?10:state==2?13:state==3?14:state==5||state==6?15:state==10?16:state==11?17:state==100?18:state==101?19:state==102?rules().at("barrel_action_page").get<int>():11;
 if(!page_||(page_!=12&&page_!=expected&&!(page_==14&&state==4))){applyPage(expected);phase_=wait_=0;}
 if(rules().contains("body_half_by_action_page")&&rules().at("body_half_by_action_page").contains(std::to_string(page_))){const auto& half=rules().at("body_half_by_action_page").at(std::to_string(page_));const auto& v=rules().at("body_variables");s.setVariable(v[4],half[0]);s.setVariable(v[5],half[1]);}
 if(page_>=12){action();}else{decide();}sampleBody();
}
void HunterBehavior::updateParallel(){if(retired_){return;}const auto& v=rules().at("body_variables");host_.advanceBlood(rules().at("blood"),host_.session().variable(rules().at("reaction_variable")));const int burn=host_.advanceBurn(GridPosition{double(host_.session().variable(v[2])),double(host_.session().variable(v[3]))});if(burn){set("damage_variable",accumulatedDamage()+burn);}}
AttackContact HunterBehavior::damage(const AttackSample& sample,bool retreat){
 auto& s=host_.session();s.setFlag(12,false);const int count=s.variable(35)+1;s.setVariable(35,count);
 const bool auxiliary=s.flag(4);const int item=s.equipped(auxiliary?2:1);const AttackKind kind=host_.equippedDamageKind(auxiliary);s.setVariable(49,rules().at("defense"));
 if(item==17&&s.variable(42)>=2){s.setVariable(34,0);play(retreat?19:sample.receiverPage,retreat?40:16);return {true,true,0,false};}
 const int attack=auxiliary?s.auxiliaryAttack():s.attack()+(s.flag(3)?host_.content().items.at(host_.content().rules.at("handgun_damage").at("aim_attack_item")).attack:0);
 const auto rolled=rollWeaponDamage(kind,attack,s.variable(49),s.variable(42),[&](int lo,int hi){return host_.random(lo,hi);});int amount=rolled.amount;
 if(auxiliary){if(count%3==0){const int roll=host_.random(0,100)-count;if(item==24&&roll<=15){amount=s.auxiliaryAttack()+host_.random(0,10);}if(item==25){if(roll<=5){amount=s.auxiliaryAttack()+host_.random(0,10);}if(roll<=10){amount=s.auxiliaryAttack()+host_.random(0,10);}}}}
 else{bool critical=s.flag(47);if(critical){s.setFlag(47,false);}else if(count%3==0){critical=host_.random(0,100)-count<=mainCriticalThreshold(kind);}if(critical){s.setFlag(12,true);s.setVariable(35,0);if(!retreat){amount*=2;}}}
 s.setVariable(34,amount);if(retreat&&s.flag(12)){set("damage_variable",1000);play(19,36);}else{set("damage_variable",accumulatedDamage()+amount);if(!s.flag(12)){play(retreat?19:sample.receiverPage,retreat?40:16);}}
 return {true,true,amount,rolled.endRequested};
}
AttackContact HunterBehavior::receiveAttack(const AttackSample& sample){
 if(retired_||mode()==EnemyMode::Dormant||mode()==EnemyMode::Corpse||host_.session().hp()<=0){return {};}
 auto& s=host_.session();const auto& v=rules().at("body_variables");const auto body=host_.screenPicturePosition({double(s.variable(v[2])),double(s.variable(v[3]))});const GridPosition target{(body.x+.5)*16,(body.y+1)*16};
 if(sample.phase!=AttackPhase::ResolvedContact&&(std::abs(sample.center.x-target.x)>sample.halfPixels[0]+s.variable(v[4])||std::abs(sample.center.y-target.y)>sample.halfPixels[1]+s.variable(v[5]))){return {};}
 if(sample.attackId&&processed_.contains(sample.attackId)){return {true,false,0,false};}if(sample.phase==AttackPhase::Flight){return {true,false,0,false};}
 const bool ignite=sample.kind==AttackKind::GlFlame;if(ignite&&host_.burning()){return {true,false,0,false};}
 const auto finish=[&](AttackContact contact){if(ignite&&contact.processed){host_.startBurn();}return contact;};
 if(sample.receiverPage<1||(sample.receiverPage>6&&sample.receiverPage!=rules().value("barrel_receiver",0))){return {true,false,0,false};}
 if(sample.attackId){processed_.insert(sample.attackId);}const int state=sourceState();const bool admitted=accumulatedDamage()<rules().at("threshold").get<int>()&&(state<=2||state==11);
 if(sample.receiverPage==1){if(accumulatedDamage()<rules().at("threshold").get<int>()&&state<=2){s.setFlag(rules().at("attack_fact"),false);play(1,7+int(host_.actor().motion.facing())*4);}return finish({true,true,0,false});}
 if(!admitted){return finish({true,true,0,false});}
 s.setVariable(42,sample.advances);
 if(sample.receiverPage==rules().value("barrel_receiver",0)){change(102);return finish({true,true,0,false});}
 if(sample.receiverPage>=4){change(101);delayed_=sample;return finish({true,true,0,false});}
 s.setVariable(42,sample.advances);
 set("reaction_variable",sample.receiverPage==3?2:1);s.setFlag(rules().at("hit_fact"),false);s.setFlag(rules().at("hit_fact"),true);const auto result=damage(sample,false);
 if(sample.receiverPage==3){change(100);}else if(s.flag(12)&&rules().at("critical_retreat").get<bool>()){change(101);delayed_=sample;}
 return finish(result);
}
