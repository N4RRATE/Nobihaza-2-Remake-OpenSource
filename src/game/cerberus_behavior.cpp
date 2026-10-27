#include "cerberus_behavior.h"
#include "combat_scene.h"
#include <algorithm>
#include <cmath>
#include <utility>
CerberusBehavior::CerberusBehavior(CombatScene& scene,const EnemyDefinition& d):definition_(d),host_(scene,d.id){corpse_=host_.session().flag(d.deadFlag);}
const nlohmann::json& CerberusBehavior::rules() const{return host_.content().branch.at("cerberus");}
const nlohmann::json& CerberusBehavior::role(const std::string& name) const{return definition_.rules.at("page_roles").at(name);}
const ObjectVariant& CerberusBehavior::page(const std::string& name) const{
    const auto& variants=host_.content().objects.at(id()).variants;const int n=role(name).at("page");
    const auto found=std::find_if(variants.begin(),variants.end(),[&](const auto& v){return v.page==n;});if(found==variants.end()){throw std::runtime_error("Missing canine page");}return *found;
}
std::string CerberusBehavior::cue(const std::string& name,int command) const{
    if(definition_.rules.contains("cue_commands")){
        const auto& aliases=definition_.rules.at("cue_commands");const auto key=std::to_string(command);
        if(aliases.contains(name)&&aliases.at(name).contains(key)){command=aliases.at(name).at(key);}
    }
    return role(name).at("presentation").get<std::string>()+"-c"+std::to_string(command);
}
void CerberusBehavior::apply(const std::string& name){
    const auto& d=page(name).definition;auto actor=host_.actor();actor.active=true;actor.spriteId=d.spriteId;actor.layer=d.layer;actor.through=actor.routeThrough.value_or(d.through);actor.setSpeedLevel(d.moveSpeed);
    actor.setAnimation(CharacterAnimation(animationForSpeed(d.moveSpeed),d.initialColumn,d.animationType),actor.motion.facing());host_.setActor(actor);movement_.reset();
}
EnemyMode CerberusBehavior::mode() const{
    if(corpse_){return EnemyMode::Corpse;}if(ending_){return EnemyMode::Ending;}if(!active_){return EnemyMode::Dormant;}
    if(pendingGrab_){return EnemyMode::PendingGrab;}if(state_==4){return EnemyMode::Grab;}if(state_==5){return EnemyMode::Retreat;}if(state_>=10){return EnemyMode::HitRecovery;}return state_==2?EnemyMode::Rush:EnemyMode::Pursuit;
}
bool CerberusBehavior::activateState(int state){if(state!=1||corpse_||ending_){return false;}active_=true;change(1);return true;}
void CerberusBehavior::change(int next){
    state_=next;phase_=0;wait_=0;movement_.reset();sequence_.clear();nextCue_=0;
    const std::string name=next==0?"roam":next==1?"pursuit":next==2?"collision":next==4?"grapple":next==5?"retreat":next==10?"knocked_down":"knocked_back";apply(name);
}
void CerberusBehavior::sequence(const std::string& name,int first){
    sequence_.clear();nextCue_=0;
    for(const auto& c:role(name).at("cues")){const std::string id=c.at("id");const int n=std::stoi(id.substr(id.rfind("-c")+2));if(n>=first){sequence_.push_back(id);}}
}
bool CerberusBehavior::advanceSequence(){
    if(wait_>0){--wait_;return false;}
    while(nextCue_<sequence_.size()){
        const auto id=sequence_[nextCue_++];const auto& c=host_.content().cue(id);const std::string kind=c.at("kind");
        if(kind=="wait"){wait_=std::max(1,c.at("tenths").get<int>()*6);return false;}
        host_.cue(id);
        if(kind=="animation"&&c.value("wait",false)){const auto& a=host_.content().media.at("animations").at(c.at("animation").get<std::string>());wait_=static_cast<int>(a.at("frames").size())*a.at("playback").at("logic_updates_per_frame").get<int>();return false;}
    }
    return true;
}
int CerberusBehavior::frontDistance() const{
    const auto e=host_.actor();const auto a=e.motion.tile(),b=host_.player().motion.tile();const auto f=e.motion.facing();
    if((f==Direction::Up||f==Direction::Down)?a.x!=b.x:a.y!=b.y){return -1;}
    return f==Direction::Up?a.y-b.y:f==Direction::Down?b.y-a.y:f==Direction::Right?b.x-a.x:a.x-b.x;
}
void CerberusBehavior::move(){
    if(host_.routePending()){return;}
    const auto actor=host_.actor();const auto p=host_.player().motion.tile();
    if(!actor.motion.moving()&&host_.terrain(adjacentTile(actor.motion.tile(),actor.motion.facing()))==3){
        const bool vertical=actor.motion.facing()==Direction::Up||actor.motion.facing()==Direction::Down;const int delta=vertical?p.x-actor.motion.tile().x:p.y-actor.motion.tile().y;
        if(delta){RouteDefinition side;side.frequency=8;side.skipBlocked=true;side.steps.push_back({std::string(directionName(vertical?(delta>0?Direction::Right:Direction::Left):(delta>0?Direction::Down:Direction::Up))),{},{},0});host_.route(side);return;}
    }
    if(!movement_){
        if(state_==0){RouteDefinition route;route.frequency=8;route.repeat=true;route.skipBlocked=true;route.steps.push_back({"toward_player",{},{},0});movement_.emplace(route,id());}
        else{movement_.emplace(page(state_==2?"collision":"pursuit").autonomousRoute,id());}
    }
    host_.advance(*movement_,[&](int source,bool value){if(source==definition_.rules.at("attack_switch")){attack_=value;}else{host_.session().setFlag(source,value);}});
}
void CerberusBehavior::bite(){
    if(host_.session().flag(13)||host_.session().hp()<=0){return;}
    host_.damagePlayer(biteDamage(definition_.rules.at("normal_attributes").at("attack"),host_.session().defense(),host_.random(-5,2)));
    host_.present({PresentationEvent::Kind::Effect,state_==4?"blood-small-low":"blood-small",{},host_.player().motion.position(),0});
}
bool CerberusBehavior::beginGrab(){
    pendingGrab_=false;if(!attack_||host_.session().flag(13)||host_.session().hp()<=0||host_.actor().motion.tile()!=host_.player().motion.tile()){change(1);return false;}
    change(4);host_.cancelWeapon();host_.cancelRoutes(true);host_.session().setFlag(10,true);host_.session().setFlag(1,false);host_.session().setVariable(31,3);
    if(definition_.rules.contains("grab_start_sound_cue")&&!definition_.rules.at("grab_start_sound_cue").is_null()){host_.sound(definition_.rules.at("grab_start_sound_cue"));}
    host_.cue(cue("grapple",2));host_.cue("f4bc-ce9-c18",false);host_.startRoute("f4bc-ce9-c19",true);
    const auto direction=host_.actor().motion.facing();host_.startRoute(cue("grapple",8+4*static_cast<int>(direction)),true);
    auto actor=host_.actor();actor.motion=CharacterMotion(host_.player().motion.tile(),actor.motion.facing(),actor.motion.speed());host_.setActor(actor);host_.startRoute(cue("grapple",33));
    escape_=cycles_=0;wait_=7;return true;
}
void CerberusBehavior::release(bool interrupted){
    if(!holdsPlayer()){host_.releaseGrab();pendingGrab_=false;return;}
    host_.cancelRoutes(true);host_.cue(cue("grapple",interrupted?40:87));
    host_.session().setFlag(10,false);host_.session().setFlag(1,true);
    if(interrupted){host_.cancelWeapon();host_.session().startRecovery(0,90);host_.releaseGrab();attack_=false;}
    else{
        host_.sound(cue("grapple",92));host_.session().startRecovery(30,90);host_.releaseGrab();
        host_.session().setVariable(31,1);host_.cue("ce9-c8",false);host_.startRoute("ce9-c9",true);
        change(5);const int command=2+4*static_cast<int>(host_.player().motion.facing());host_.startRoute(cue("retreat",command));sequence("retreat",17);
    }
}
int CerberusBehavior::calculate(const AttackSample& sample,bool retainDefense,bool bonus){
    if(companionDamage_){const int amount=std::exchange(companionDamage_,{}).value();damage_+=amount;host_.session().setFlag(12,false);return amount;}
    const auto kind=host_.damageKind(sample.kind);auto& s=host_.session();const int defense=definition_.rules.at("normal_attributes").at("defense");if(!retainDefense){s.setVariable(49,defense);}
    s.setFlag(12,false);const int count=s.variable(35)+1;s.setVariable(35,count);
    const int attack=auxiliaryDamage(kind)?s.auxiliaryAttack():s.attack()+(sample.aimed?host_.content().items.at(host_.content().rules.at("handgun_damage").at("aim_attack_item")).attack:0);
    const auto result=rollWeaponDamage(kind,attack,s.variable(49),sample.advances,[&](int lo,int hi){return host_.random(lo,hi);});
    damageRequestedEnd_=result.endRequested;if(result.endRequested){host_.requestAttackEnd(sample.attackId);}
    if(retainDefense){s.setVariable(49,defense);}int damage=result.amount;bool critical=false;
    if(kind==AttackKind::Knife){if(count%3==0&&host_.random(0,100)-count<=15){damage=s.auxiliaryAttack()+host_.random(0,10);}host_.sound("f3-ce11-c13");}
    else if(!auxiliaryDamage(kind)){critical=s.flag(47)||(count%3==0&&host_.random(0,100)-count<=mainCriticalThreshold(kind));s.setFlag(47,false);if(critical){s.setFlag(12,true);s.setVariable(35,0);}}
    if(bonus){damage+=host_.random(5,8);}if(critical){damage*=2;}damage_+=damage;return damage;
}
void CerberusBehavior::end(){
    if(ending_||corpse_){return;}if(holdsPlayer()||pendingGrab_){release(true);}host_.cancelRoutes();attack_=false;ending_=true;pendingHit_.reset();wait_=phase_=0;
    criticalEnding_=damage_>=150&&host_.session().flag(12);if(damage_>=150&&!criticalEnding_){damage_=100;}apply(criticalEnding_?"head_death":"normal_death");sequence(criticalEnding_?"head_death":"normal_death");
}
AttackContact CerberusBehavior::receiveAttack(const AttackSample& sample){
    if(!active_||corpse_||ending_||damage_>=100){return {};}
    const auto p=host_.actor().motion.position();const GridPosition center{(p.x+.5)*16,(p.y+.5)*16};const auto& half=rules().at("geometry").at("value").at(state_==0?"roam_half_size_px":"pursuit_half_size_px");
    if(sample.phase!=AttackPhase::ResolvedContact&&(std::abs(sample.center.x-center.x)>sample.halfPixels[0]+half[0].get<int>()||std::abs(sample.center.y-center.y)>sample.halfPixels[1]+half[1].get<int>())){return {};}
    if(sample.phase==AttackPhase::Flight){return {true,false,0,false};}
    if(sample.kind==AttackKind::GlFlame&&host_.burning()){return {true,false,0,false};}
    if(sample.phase==AttackPhase::Burning){host_.startBurn();return {true,true,0,false};}
    if(sample.kind==AttackKind::GlFlame){host_.startBurn();}
    damageRequestedEnd_=false;const int before=damage_;const int receiver=sample.receiverPage;
    const auto blood=[&](int type){const auto effect=definition_.rules.at("blood_effects").at(std::to_string(type)).get<std::string>();host_.present({PresentationEvent::Kind::Effect,effect,{},p,0,id()});};
    if(receiver==1){collisionYield(host_.actor().motion.facing());return {true,true,0,false};}
    if(receiver==5||receiver==6){host_.session().setFlag(12,true);damage_=receiver==5?150:2000;if(receiver==6){blood(3);pendingGrenadeEnding_=true;}else{end();}}
    else if(receiver>=2&&receiver<=4){
        if(receiver==2&&!attack_&&state_<10){host_.startRoute(cue("receiver_2",17));blood(11);calculate(sample,false,false);if(damage_>=100){end();}}
        else{const int next=receiver==4||attack_?11:10;if(holdsPlayer()||pendingGrab_){release(true);}host_.cancelRoutes();change(next);pendingHit_=sample;}
    }else{throw std::runtime_error("Unsupported canine receiver");}
    return {true,true,damage_-before,damageRequestedEnd_};
}
void CerberusBehavior::collisionYield(Direction facing){
    if(corpse_||ending_){return;}attack_=false;const auto id=cue("receiver_1",6+4*static_cast<int>(facing));if(!host_.routePending(id)){host_.startRoute(id);}
}
void CerberusBehavior::update(const InputFrame& input){
    if(corpse_){return;}
    if(pendingGrenadeEnding_){pendingGrenadeEnding_=false;end();return;}
    if(ending_){if(advanceSequence()){damage_=criticalEnding_?1600:damage_;clearBodyCache();host_.session().setFlag(definition_.deadFlag,true);corpse_=true;ending_=false;apply(criticalEnding_?"head_corpse":"corpse");}return;}
    if(!host_.session().flag(18)||host_.session().hp()<=0){return;}
    if(!active_){if(!host_.session().flag(definition_.rules.at("activation_fact"))){return;}active_=true;change(0);}
    if(pendingGrab_){return;}
    if(pendingHit_){
        attack_=false;host_.cue(cue(state_==10?"knocked_down":"knocked_back",3));
        const int type=state_==10?1:definition_.rules.at("blow_blood_type").get<int>();
        host_.present({PresentationEvent::Kind::Effect,definition_.rules.at("blood_effects").at(std::to_string(type)),{},host_.actor().motion.position(),0,id()});
        calculate(*pendingHit_,state_==11&&definition_.rules.at("retained_defense_on_blow").get<bool>(),state_==11);pendingHit_.reset();
        if(state_==11&&host_.content().cues.contains(cue("knocked_back",17))){host_.sound(cue("knocked_back",17));}
        if(damage_>=100){end();return;}sequence(state_==10?"knocked_down":"knocked_back",definition_.rules.contains("encounter_receivers")?19:state_==10?19:21);
    }
    if(state_==10||state_==11||state_==5){if(advanceSequence()){attack_=false;change(1);}return;}
    if(state_==4){
        for(auto action:{InputAction::Up,InputAction::Right,InputAction::Down,InputAction::Left}){if(input.buttons[static_cast<int>(action)].pressed){++escape_;break;}}
        if(wait_>0){--wait_;return;}
        if(phase_==1){release(false);return;}
        if(!attack_||damage_>=100){release(true);change(1);return;}
        if(phase_==2&&(escape_>=10||cycles_>=8)){wait_=18;phase_=1;return;}
        host_.sound(cue("grapple",60));host_.sound(cue("grapple",61));bite();++cycles_;phase_=2;wait_=24;return;
    }
    if(host_.session().flag(10)){return;}
    if(wait_>0){--wait_;return;}
    if(state_==0){const auto e=host_.actor().motion.position(),p=host_.player().motion.position();const int front=frontDistance();if((front>=0&&front<=6)||(std::abs(e.x-p.x)<=3&&std::abs(e.y-p.y)<=3)){change(1);return;}move();return;}
    if(state_==1){
        if(phase_==1){phase_=2;wait_=1;return;}if(phase_==2){change(2);attack_=true;return;}
        const int front=frontDistance();if(front>=0&&front<=2&&!host_.session().flag(13)){host_.startRoute(cue("pursuit",15+20*static_cast<int>(host_.actor().motion.facing())));wait_=6;phase_=1;return;}move();return;
    }
    if(state_==2){
        if(phase_==3){change(1);return;}
        if(!attack_){apply("pounce");host_.startRoute(cue("pounce",5));wait_=12;phase_=3;return;}
        if(phase_==1){phase_=0;if(!host_.session().flag(13)&&!host_.session().flag(10)&&host_.actor().motion.tile()==host_.player().motion.tile()){
            if(host_.random(1,3)<=2){host_.sound(cue("collision",11));bite();attack_=false;}else{pendingGrab_=true;host_.requestGrab();return;}
        }}
        else if(attack_&&host_.actor().motion.tile()==host_.player().motion.tile()){phase_=1;wait_=1;return;}
        move();
    }
}
void CerberusBehavior::updateParallel(){
    sampleBodyCache();
    const int damage=host_.advanceBurn();
    if(damage&&active_&&!corpse_&&!ending_){damage_+=damage;if(damage_>=100){host_.session().setFlag(12,false);end();}}
}
void CerberusBehavior::sampleBodyCache(){
    const auto& profile=definition_.rules.value("runtime_profile",nlohmann::json::object());if(!profile.contains("body_cache")){return;}
    const auto& cache=profile.at("body_cache");auto& session=host_.session();
    // The three reviewed manager pages have identical sample commands; their highest
    // source switch page still controls whether any sample may be published.
    const auto* selected=host_.content().activeVariant(host_.content().objects.at(cache.at("source_object").get<std::string>()),session);
    if(!selected||std::none_of(cache.at("source_pages").begin(),cache.at("source_pages").end(),[&](const auto& value){return value==selected->page;})){return;}
    const auto& actor=host_.actor();const auto tile=actor.motion.tile();auto screen=host_.actorScreenFoot();
    const auto& offset=cache.at("center_offset_pixels");screen.x+=offset.at(0).get<int>();screen.y+=offset.at(1).get<int>();
    session.setVariable(cache.at("register_variable"),cache.at("register_event"));
    session.setVariable(cache.at("screen_variables").at(0),static_cast<int>(screen.x));session.setVariable(cache.at("screen_variables").at(1),static_cast<int>(screen.y));
    session.setVariable(cache.at("tile_variables").at(0),tile.x);session.setVariable(cache.at("tile_variables").at(1),tile.y);
    const auto name=state_==0?"roam":state_==1?"pursuit":std::string{};
    if(!name.empty()){
        const auto pageNumber=std::to_string(role(name).at("page").get<int>());const auto& halves=cache.at("half_pixels_by_page");
        if(halves.contains(pageNumber)){session.setVariable(cache.at("half_variables").at(0),halves.at(pageNumber).at(0));session.setVariable(cache.at("half_variables").at(1),halves.at(pageNumber).at(1));}
    }
}
void CerberusBehavior::clearBodyCache(){
    const auto& profile=definition_.rules.value("runtime_profile",nlohmann::json::object());if(!profile.contains("body_cache")){return;}
    for(const auto& variable:profile.at("body_cache").at("death_clear_screen")){host_.session().setVariable(variable,0);}
}
void CerberusBehavior::updateDeathPresentation(){
    if(!holdsPlayer()){return;}if(feedingPhase_<0){apply("feeding");feedingPhase_=0;}if(feedingWait_>0){--feedingWait_;return;}
    if(feedingPhase_==0){host_.sound(cue("feeding",2));feedingWait_=150;++feedingPhase_;}
    else if(feedingPhase_==1){host_.sound(cue("feeding",4));host_.startRoute(cue("feeding",5));host_.sound(cue("feeding",7));host_.present({PresentationEvent::Kind::Effect,"blood-small-low",{},host_.player().motion.position(),0});feedingWait_=30;++feedingPhase_;}
    else{host_.sound(cue("feeding",7));host_.present({PresentationEvent::Kind::Effect,"blood-small-low",{},host_.player().motion.position(),0});feedingWait_=30;}
}

AttackContact CerberusBehavior::receiveCompanion(int page,int damage){
    if(!definition_.rules.contains("encounter_receivers")||!active_||corpse_||ending_||damage_>=100){return {};}
    const int before=damage_;if(page==10||page==11){host_.session().setFlag(12,true);damage_=page==10?150:2000;end();return {true,true,damage_-before,false};}
    if(page<7||page>9){return {};}if(page==7&&!attack_&&state_<10){damage_+=damage;host_.startRoute(cue("receiver_2",17));if(damage_>=100){end();}return {true,true,damage,false};}
    if(holdsPlayer()||pendingGrab_){release(true);}host_.cancelRoutes();change(page==9||attack_?11:10);companionDamage_=damage;pendingHit_=AttackSample{};return {true,true,0,false};
}
