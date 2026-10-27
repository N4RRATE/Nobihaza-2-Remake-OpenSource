#include "chimera_behavior.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
using Json=nlohmann::json;
ChimeraBehavior::ChimeraBehavior(CombatScene& s,const EnemyDefinition& d):definition_(d),host_(s,d.id){host_.claimAutonomy();}
int ChimeraBehavior::value(const char* k)const{return host_.session().variable(rules().at(k));}
void ChimeraBehavior::set(const char* k,int n){host_.session().setVariable(rules().at(k),n);}
int ChimeraBehavior::sourceState()const{return value("state_variable");}
int ChimeraBehavior::accumulatedDamage()const{return value("damage_variable");}
EnemyMode ChimeraBehavior::mode()const{
 if(retired_||host_.session().flag(definition_.deadFlag)){return EnemyMode::Corpse;}
 if(!host_.session().flag(rules().at("activation_fact"))){return EnemyMode::Dormant;}
 if(ending_){return EnemyMode::Ending;}if(pendingGrab_){return EnemyMode::PendingGrab;}if(holdsPlayer()){return EnemyMode::Grab;}
 if(sourceState()==100){return EnemyMode::Retreat;}return page_>=14?EnemyMode::Windup:EnemyMode::Pursuit;
}
void ChimeraBehavior::pose(int n){set("pose_variable",n);}
void ChimeraBehavior::play(int p,int n){const auto& cues=rules().at("cues").at(std::to_string(p));if(!cues.contains(std::to_string(n))){throw std::logic_error("Missing Chimera cue");}host_.cue(cues.at(std::to_string(n)));}
void ChimeraBehavior::playerPose(int n){host_.playerPose(n);}
void ChimeraBehavior::applyPage(int n){
 page_=n;autonomy_.reset();const auto& variants=host_.content().objects.at(id()).variants;
 const auto it=std::find_if(variants.begin(),variants.end(),[&](const auto& v){return v.page==n;});if(it==variants.end()){throw std::logic_error("Missing Chimera page");}
 auto a=host_.actor();const auto& p=it->definition;a.spriteId=p.spriteId;a.layer=p.layer;a.through=a.routeThrough.value_or(p.through);a.setSpeedLevel(p.moveSpeed);a.setAnimation(CharacterAnimation(animationForSpeed(p.moveSpeed),p.initialColumn,p.animationType),a.motion.facing());host_.setActor(a);
 if(!it->autonomousRoute.steps.empty()){autonomy_.emplace(it->autonomousRoute,id());}
}
void ChimeraBehavior::change(int n){host_.cancelRoutes();phase_=wait_=0;set("state_variable",n);host_.session().setFlag(rules().at("action_fact"),n>0);applyPage(n==0?11:n==100?21:13+n);}
void ChimeraBehavior::finish(){pose(0);host_.eraseOwnedPictures(2,4);host_.session().setVariable(rules().at("direction_variable").get<int>()+4,0);host_.session().setVariable(rules().at("direction_variable").get<int>()+1,0);host_.session().setFlag(rules().at("action_fact"),false);phase_=wait_=0;applyPage(11);}
void ChimeraBehavior::sampleBody(){
 auto& s=host_.session();const auto& object=host_.content().objects.at(rules().at("manager").get<std::string>());const auto* selected=host_.content().activeVariant(object,s);if(!selected||selected->definition.trigger!=4||!rules().at("manager_frames").contains(std::to_string(selected->page))){return;}
 const auto& v=rules().at("body_variables");const auto a=host_.actor();const auto tile=a.motion.tile();const auto foot=host_.actorScreenFoot();
 s.setVariable(v[0],tile.x);s.setVariable(v[1],tile.y);s.setVariable(v[2],int(foot.x));s.setVariable(v[3],int(foot.y)-21);s.setVariable(v[6],8);s.setVariable(v[7],15);
 const int dir=a.motion.facing()==Direction::Up?8:a.motion.facing()==Direction::Right?6:a.motion.facing()==Direction::Down?2:4;
 s.setVariable(rules().at("direction_variable"),dir);
}
void ChimeraBehavior::manager(){
 auto& s=host_.session();const auto& object=host_.content().objects.at(rules().at("manager").get<std::string>());const auto* selected=host_.content().activeVariant(object,s);if(!selected||selected->definition.trigger!=4||!host_.pictureCommandsAllowed()){return;}
 const auto& profiles=rules().at("manager_frames");const auto page=std::to_string(selected->page);if(!profiles.contains(page)){return;}
 const auto& profile=profiles.at(page);const std::string dir=std::to_string(value("direction_variable"));if(!profile.at("show").contains(dir)){return;}
 const int offset=rules().at("direction_variable").get<int>();const auto foot=host_.actorScreenFoot();GridPosition sample{foot.x,foot.y-30};
 const bool entered=managerPage_!=selected->page;managerPage_=selected->page;
 const auto emit=[&](const std::string& cue){host_.present({PresentationEvent::Kind::Cue,cue,{},host_.screenPicturePosition(sample),0,id(),sample});};
 if(entered||s.variable(offset+1)!=value("direction_variable")){s.setVariable(offset+1,value("direction_variable"));for(const auto& cue:profile.at("show").at(dir)){emit(cue);}if(entered&&!profile.at("idle").get<bool>()){pose(0);}}
 if(profile.at("walking").get<bool>()){
  const auto tile=host_.actor().motion.tile();const int combined=tile.x*1000+tile.y;s.setVariable(offset+6,combined);if(s.variable(offset+7)!=combined){s.setVariable(offset+7,combined);s.setVariable(offset+4,s.variable(offset+4)+32);}
  const int cycle=profile.at("idle").get<bool>()?offset+5:rules().at("decision_random_variable").get<int>();s.setVariable(cycle,s.variable(cycle)+1);
  if(s.variable(offset+4)>0){if(s.variable(cycle)>=28||s.variable(cycle)<=12){sample.y+=1;}if(s.variable(cycle)>=33){s.setVariable(cycle,0);}s.setVariable(offset+4,s.variable(offset+4)-1);}else{s.setVariable(offset+4,0);}
 }
 s.setVariable(offset+2,int(sample.x));s.setVariable(offset+3,int(sample.y));
 const int frame=profile.at("idle").get<bool>()?0:std::clamp(value("pose_variable"),0,int(profile.at("moves").size())-1);for(const auto& cue:profile.at("moves").at(std::to_string(frame))){emit(cue);}
}
void ChimeraBehavior::updateParallel(){
 if(retired_){return;}sampleBody();manager();
 host_.advanceBlood(rules().at("blood"),value("reaction_variable"));
 const auto& v=rules().at("body_variables");const int n=host_.advanceBurn(GridPosition{double(host_.session().variable(v[2])),double(host_.session().variable(v[3]))});if(n){set("damage_variable",accumulatedDamage()+n);}
}
void ChimeraBehavior::decide(){
 auto& s=host_.session();const auto t=host_.actor().motion.tile(),p=host_.player().motion.tile();const auto& b=rules().at("body_variables");const int cx=s.variable(b[0]),cy=s.variable(b[1]);const int d=value("direction_variable");
 const auto choose=[&](bool condition,int n,int hi,int boundary,bool atLeast){if(!condition||(n==7&&rules().at("slot")==2&&s.flag(452))){return false;}const int roll=host_.random(0,hi);set(n==7?"decision_random_variable":"state_variable",roll);if(atLeast?roll>=boundary:roll<=boundary){change(n);return true;}return false;};
 if(d==8){if(choose(t.y-1<=p.y&&t.y>=p.y&&t.x==p.x,3,4,1,true)||choose(t.y-3>=p.y&&t.y-8<=p.y&&t.x==p.x,7,400,1,false)||choose(t.y-2<=p.y&&t.y>=p.y&&t.x-2<=p.x&&cx>=p.x,2,4,1,false)||choose(t.y-2<=p.y&&t.y>=p.y&&t.x+2>=p.x&&cx<=p.x,1,4,1,false)){return;}}
 else if(d==6){if(choose(t.x+1>=p.x&&cx<=p.x&&t.y==p.y,3,4,1,true)||choose(t.x+3<=p.x&&t.x+11>=p.x&&t.y==p.y,7,400,1,false)||choose(t.x+2>=p.x&&cx<=p.x&&t.y-2<=p.y&&cy>=p.y,1,4,1,false)||choose(t.x+2>=p.x&&cx<=p.x&&t.x+2>=p.y&&cy<=p.y,2,4,1,false)){return;}}
 else if(d==2){if(choose(t.y+1>=p.y&&t.y<=p.y&&t.x==p.x,3,4,1,true)||choose(t.y+3<=p.y&&t.y+9>=p.y&&t.x==p.x,7,400,1,false)||choose(t.y+2>=p.y&&t.y<=p.y&&t.x-2<=p.x&&cx>=p.x,1,4,1,false)||choose(t.y+2>=p.y&&t.y<=p.y&&t.x+2>=p.x&&cx<=p.x,2,4,1,false)){return;}}
 else if(d==4){if(choose(t.x-1<=p.x&&cx>=p.x&&t.y==p.y,3,4,1,true)||choose(t.x-3>=p.x&&t.x-8<=p.x&&t.y==p.y,7,400,1,false)||choose(t.x-2<=p.x&&cx>=p.x&&t.y-2<=p.y&&cy>=p.y,1,4,1,false)||choose(t.x-2<=p.x&&cx>=p.x&&t.x+2>=p.y&&cy<=p.y,2,4,1,false)){return;}}
 if(autonomy_&&!host_.routePending()){host_.advance(*autonomy_);}
}
bool ChimeraBehavior::grabRange()const{
 const auto t=host_.actor().motion.tile(),p=host_.player().motion.tile();const auto& b=rules().at("body_variables");const int cx=host_.session().variable(b[0]),cy=host_.session().variable(b[1]);
 switch(host_.actor().motion.facing()){case Direction::Up:return t.x==p.x&&t.y-2<=p.y&&cy>=p.y;case Direction::Right:return t.x+2>=p.x&&cx<=p.x&&t.y==p.y;case Direction::Down:return t.x==p.x&&t.y+1>=p.y&&cy<=p.y;case Direction::Left:return t.x-2<=p.x&&cx>=p.x&&t.y==p.y;}return false;
}
void ChimeraBehavior::claw(bool right){
 const auto a=host_.sampleActorScreen(id()),p=host_.sourcePlayerBody();const auto d=host_.actor().motion.facing();
 const GridPosition offset=d==Direction::Up?GridPosition{right?-10.:10.,-40}:d==Direction::Right?GridPosition{15,right?7.:-10.}:d==Direction::Down?GridPosition{right?10.:-10.,5}:GridPosition{-15,right?7.:-10.};
 if(std::abs(a.x+offset.x-p.x)>32||std::abs(a.y+offset.y-p.y)>32||host_.session().flag(13)){return;}
 play(right?15:14,80);auto& s=host_.session();s.setVariable(24,2);s.setFlag(8,false);s.setFlag(8,true);s.setFlag(17,true);host_.cleanupPlayer();host_.receiveSourceDamage(rules().at("attack"));s.setFlag(17,false);
}
bool ChimeraBehavior::timeline(const std::string& name){
 const auto& rows=rules().at("timelines").at(name);
 while(phase_<int(rows.size())){const auto& row=rows.at(phase_++);const auto kind=row.at("kind");
  if(kind=="pose"){pose(row.at("value"));}else if(kind=="player_pose"){playerPose(row.at("value"));}else if(kind=="cue"){host_.cue(row.at("cue"));}else if(kind=="direction_animation"){host_.cue(rules().at("fatal_direction_animations").at(std::to_string(int(host_.player().motion.facing()))));}else if(kind=="wait"){wait_=row.at("ticks");return false;}else{throw std::logic_error("Unknown Chimera timeline");}
 }return true;
}
void ChimeraBehavior::action(const InputFrame& input){
 auto& s=host_.session();if(page_==17&&holdsPlayer()&&!host_.dialogueActive()){for(auto a:{InputAction::Down,InputAction::Left,InputAction::Right,InputAction::Up}){if(input.buttons[static_cast<std::size_t>(a)].pressed){s.setVariable(29,s.variable(29)+1);break;}}}
 if(wait_>0&&--wait_>0){return;}
 if(page_==14||page_==15){if(phase_==0){pose(0);phase_=1;wait_=6;return;}if(phase_==1){pose(1);phase_=2;wait_=6;return;}if(phase_==2){play(page_,5);pose(2);claw(page_==15);phase_=3;wait_=1;return;}if(phase_==3){pose(3);phase_=4;wait_=30;return;}finish();return;}
 if(page_==16){if(phase_==0){pose(0);phase_=1;wait_=7;return;}if(phase_==1){pose(1);phase_=2;wait_=6;return;}if(phase_==2){play(16,6);pose(2);phase_=3;wait_=6;if(grabRange()&&!s.flag(13)){set("state_variable",4);pendingGrab_=true;host_.requestGrab();}return;}if(phase_==3){pose(3);phase_=4;wait_=30;return;}finish();return;}
 if(page_==17){
  if(!holdsPlayer()){return;}if(!s.flag(rules().at("action_fact"))||accumulatedDamage()>=500){release(true);finish();return;}
  if(phase_==0){playerPose(8);phase_=1;wait_=6;return;}if(phase_==1){s.setVariable(30,0);phase_=2;}
  static constexpr int poses[]{9,8,10,8};if(phase_<6){playerPose(poses[phase_-2]);++phase_;wait_=6;return;}
  const int need=s.variable(9)<=20?100:s.variable(9)<=50?20:10;
  if(s.variable(29)>=need){release(false);change(100);return;}
  s.setVariable(30,s.variable(30)+1);if(s.variable(30)>=3){phase_=wait_=0;set("state_variable",5);applyPage(18);return;}phase_=2;return;
 }
 if(page_==18){if(timeline("fatal_first")){phase_=0;set("state_variable",6);applyPage(19);}return;}
 if(page_==19){if(timeline("fatal_final")){host_.fail("chimera-fatal");}return;}
 if(page_==20){
  if(phase_==0){const auto& route=rules().at("acid_turn_routes");const int d=int(host_.actor().motion.facing());const auto t=host_.actor().motion.tile(),p=host_.player().motion.tile();const bool negative=d==0||d==2?t.x>p.x:t.y>p.y;const bool positive=d==0||d==2?t.x<p.x:t.y<p.y;if(negative||positive){host_.startRoute(route.at(std::to_string(d)).at(negative?0:1));wait_=7;}phase_=1;return;}
  if(phase_==1){if(s.flag(rules().at("acid_gate"))){finish();return;}pose(0);phase_=2;wait_=6;return;}if(phase_==2){pose(1);phase_=3;wait_=6;return;}if(phase_==3){pose(2);phase_=4;wait_=1;return;}if(phase_==4){set("reaction_variable",122);s.setFlag(rules().at("blood_fact"),false);s.setFlag(rules().at("blood_fact"),true);s.setFlag(rules().at("acid_fact"),true);phase_=5;wait_=6;return;}if(phase_==5){pose(1);phase_=6;wait_=6;return;}finish();return;
 }
 if(page_==21){if(phase_==0){pose(0);const auto p=host_.player().motion.tile();auto a=host_.actor();const auto d=a.motion.facing();const auto v=adjacentTile({0,0},d);a.motion=CharacterMotion({p.x-v.x,p.y-v.y},d,a.motion.speed());host_.setActor(a);host_.startRoute(rules().at("retreat_routes").at(std::to_string(int(d))));phase_=1;retreatTicks_=0;}if(rules().at("retreat_receiver").get<bool>()){const auto tile=adjacentTile(host_.actor().motion.tile(),static_cast<Direction>((int(host_.actor().motion.facing())+2)%4));const int receiver=host_.receiverAt(tile);if(receiver>0){host_.collisionReceiver(tile);}}if(++retreatTicks_>=100){finish();}else{wait_=1;}return;}
 if(page_==22){if(phase_==0){play(22,1);host_.eraseOwnedPictures(1,4);phase_=1;wait_=12;return;}const auto& v=rules().at("body_variables");s.setVariable(v[2],0);s.setVariable(v[3],0);host_.eraseOwnedPictures(1,4);s.setFlag(definition_.deadFlag,true);ending_=false;if(holdsPlayer()){release(true);}auto a=host_.actor();a.active=false;a.through=true;host_.setActor(a);return;}
}
void ChimeraBehavior::update(const InputFrame& input){
 if(retired_||host_.session().flag(definition_.deadFlag)||host_.session().hp()==0){return;}auto& s=host_.session();
 const auto* selected=host_.content().activeVariant(host_.content().objects.at(id()),s);if(!selected){return;}
 if(selected->page==22){if(page_!=22){if(holdsPlayer()){release(true);}phase_=wait_=0;ending_=true;applyPage(22);}action(input);return;}
 // A higher source page or cleared action fact retires that page's wait.
 // Keep a receiver's already committed forced route; it has its own lifetime.
 if(page_>=14&&selected->page!=page_){if(holdsPlayer()){release(true);}pendingGrab_=false;phase_=wait_=0;applyPage(selected->page);}
 if(!s.flag(rules().at("activation_fact"))||selected->page==13){return;}
 if(page_==0){applyPage(selected->page>=14&&selected->page<=21?selected->page:11);}if(page_>=14){action(input);return;}
 if(!s.flag(18)){return;}
 if(selected->page==12&&host_.burning()){s.setFlag(rules().at("action_fact"),false);set("damage_variable",accumulatedDamage()+host_.random(3,4));play(12,4);wait_=1;return;}
 if(pendingGrab_){return;}decide();
}
bool ChimeraBehavior::activateState(int n){if(n<0||(n>7&&n!=100)||host_.session().flag(definition_.deadFlag)){return false;}change(n);return true;}
bool ChimeraBehavior::beginGrab(){
 pendingGrab_=false;if(host_.session().flag(13)||!grabRange()){finish();return false;}auto& s=host_.session();s.setFlag(10,true);host_.cancelWeapon();host_.cancelRoutes(true);host_.cleanupPlayer();
 auto p=host_.player();const auto d=host_.actor().motion.facing();p.face(static_cast<Direction>((int(d)+2)%4));host_.setPlayer(p);auto a=host_.actor();const auto tile=p.motion.tile();a.motion=CharacterMotion(d==Direction::Up?TilePosition{tile.x,tile.y+2}:d==Direction::Right?TilePosition{tile.x-2,tile.y}:d==Direction::Down?TilePosition{tile.x,tile.y-1}:TilePosition{tile.x+2,tile.y},d,a.motion.speed());host_.setActor(a);s.setFlag(1,false);phase_=wait_=0;set("state_variable",4);applyPage(17);return true;
}
void ChimeraBehavior::release(bool interrupted){
 auto& s=host_.session();if(holdsPlayer()){host_.cancelRoutes(true);s.setFlag(10,false);host_.cleanupPlayer();host_.releaseGrab();if(interrupted){s.setFlag(13,true);s.setFlag(1,true);s.setFlag(rules().at("action_fact"),false);}else{s.setFlag(11,false);s.setFlag(11,true);host_.startEscapeRecovery();}}else{host_.releaseGrab();}pendingGrab_=false;
}
void ChimeraBehavior::retire(){if(retired_){return;}if(holdsPlayer()){release(true);}host_.cancelRoutes();autonomy_.reset();retired_=true;}
void ChimeraBehavior::collisionYield(Direction){if(accumulatedDamage()<250){host_.yieldBehind();}}
AttackContact ChimeraBehavior::receiveAttack(const AttackSample& sample){
 auto& s=host_.session();if(retired_||s.flag(definition_.deadFlag)||!s.flag(rules().at("activation_fact"))){return {};}
 const auto& v=rules().at("body_variables");const auto cached=host_.screenPicturePosition({double(s.variable(v[2])),double(s.variable(v[3]))});const GridPosition target{(cached.x+.5)*16,(cached.y+1)*16};if(std::abs(sample.center.x-target.x)>sample.halfPixels[0]+s.variable(v[6])||std::abs(sample.center.y-target.y)>sample.halfPixels[1]+s.variable(v[7])){return {};}
 if(sample.phase==AttackPhase::Flight){return {true,false,0,false};}if(sample.phase==AttackPhase::Burning){host_.startBurn();return {true,true,0,false};}
 if(accumulatedDamage()>=250){return {true,true,0,false};}const int receiver=sample.receiverPage;
 if(receiver==1){const auto& row=rules().at("receiver_back_routes");host_.startRoute(row.at(std::to_string(int(host_.actor().motion.facing()))));s.setFlag(rules().at("action_fact"),false);return {true,true,0,false};}
 if(receiver==7||receiver==8){return {true,host_.requestSourceReaction(receiver),0,false};}
 if(receiver<2||receiver>6){return {};}
 const int hi=receiver==2?100:receiver==3?80:70;set("reaction_variable",value("reaction_variable")+host_.random(0,hi));if(value("reaction_variable")<=30){set("state_variable",7);s.setFlag(rules().at("action_fact"),true);}
 set("reaction_variable",receiver<=3?1:receiver==4?2:3);s.setFlag(rules().at("blood_fact"),false);s.setFlag(rules().at("blood_fact"),true);
 auto resolved=sample;resolved.kind=host_.damageKind(sample.kind);const auto contact=host_.receivePartDamage(resolved,rules().at("defense"));set("damage_variable",accumulatedDamage()+contact.damage);if(receiver==3||receiver==4||receiver==5){s.setFlag(16,true);}if(resolved.kind==AttackKind::GlFlame){host_.startBurn();}if(sourceState()==7&&!holdsPlayer()){phase_=wait_=0;applyPage(20);}return contact;
}
