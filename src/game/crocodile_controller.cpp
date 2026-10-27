#include "crocodile_controller.h"
#include "full_content.h"
#include "weapon_controller.h"
#include "enemy_host.h"
#include <algorithm>
#include <cmath>
namespace{using Json=nlohmann::json;
bool overlaps(GridPosition a,GridPosition b,std::array<int,2> ah,std::array<int,2> bh){return std::abs(a.x-b.x)<=ah[0]+bh[0]&&std::abs(a.y-b.y)<=ah[1]+bh[1];}
}
CrocodileController::CrocodileController(CombatScene& s,const Json& p):scene_(s),profile_(p){
    if(const auto* patch=difficultyControllerRules(s.content_,s.session_,"crocodile")){profile_.merge_patch(*patch);}
    auto& state=s.session_;const auto& body=p.at("body");state.setVariable(83,body.at("initial_screen_x"));state.setVariable(body.at("body_offset_variable"),body.at("initial_vertical_offset"));
    for(const auto& [number,row]:p.at("slots").items()){
        const int slot=std::stoi(number),base=81+(slot-1)*14;state.setVariable(row.at("source_event_variable"),row.at("source_event_id"));
        state.setVariable(base+6,row.at("half_pixels").at(0));state.setVariable(base+7,row.at("half_pixels").at(1));
    }
    for(const auto* side:{"left","right"}){const auto& foot=p.at("feet").at(side);state.setVariable(foot.at("mode_variable"),foot.at("initial_mode"));state.setVariable(foot.at("phase_variable"),0);}
    state.setVariable(97,p.at("feet").at("left").at("initial_x"));state.setVariable(111,p.at("feet").at("right").at("initial_x"));state.setVariable(98,state.variable(84)-10);state.setVariable(112,state.variable(84)-10);
    state.setFlag(p.at("initialization_fact"),true);state.setFlag(1,true);state.setFlag(18,true);
    emit("m109-e3",1,1);emit("m109-e3",1,2);for(int command:{12,13,14}){emit("m109-e3",1,command);}enter(0);
    const auto& g=p.at("gas");gasWait_=g.at("spawn_wait_tenths").at(s.random_(g.at("wait_random_inclusive").at(0),g.at("wait_random_inclusive").at(1))-1).get<int>()*6;
    emit(g.at("object"),8,1);emit(g.at("object"),8,2);
}
void CrocodileController::picture(const std::string& id,GridPosition position){PresentationEvent e{PresentationEvent::Kind::Cue,id,{},{},0};e.actor="player";e.resolvedPicture=ResolvedPicture{position,0,0};scene_.presentation_.push_back(std::move(e));}
void CrocodileController::emit(const std::string& object,int page,int command){
    for(const auto& cue:profile_.at("roles").at(object).at(std::to_string(page)).at("media")){if(cue.at("command")==command){scene_.cue(cue.at("cue"),object);return;}}
}
GridPosition CrocodileController::attackScreen(GridPosition world) const{const auto camera=scene_.camera_.center();return {world.x-camera.x+160,world.y-camera.y+120};}
GridPosition CrocodileController::center(int slot) const{const auto& row=profile_.at("slots").at(std::to_string(slot));return {double(scene_.session_.variable(row.at("screen_variables").at(0))),double(scene_.session_.variable(row.at("screen_variables").at(1)))};}
bool CrocodileController::inFront(int range) const{const auto player=scene_.sampleActorScreen("player");const double offset=player.y-scene_.session_.variable(profile_.at("body").at("body_offset_variable"));return offset>=0&&offset<=range;}
void CrocodileController::enter(int mode){
    mode_=mode;iterations_=bodyWait_=0;auto& s=scene_.session_;s.setVariable(91,mode);
    if(mode==0||mode==1||mode==3){for(const auto* side:{"left","right"}){const auto& foot=profile_.at("feet").at(side);s.setVariable(foot.at("phase_variable"),0);s.setVariable(foot.at("mode_variable"),foot.at("initial_mode"));}}
    if(mode==0){s.setFlag(profile_.at("mouth_fact"),false);}
    else if(mode==1||mode==2){s.setFlag(profile_.at("mouth_fact"),true);emit("m109-e5",mode==1?2:3,1);picture(profile_.at("roles").at("m109-e5").at(mode==1?"2":"3").at("media").at(1).at("cue"),center(1));if(mode==2){emit("m109-e5",3,3);}}
}
void CrocodileController::updateBody(){
    auto& s=scene_.session_;const auto& f=profile_.at("fsm");const int field=profile_.at("body").at("body_offset_variable");int y=s.variable(field);
    if(s.variable(91)!=mode_){enter(s.variable(91));}
    if(bodyWait_>0&&--bodyWait_>0){return;}
    if(mode_==0){const auto& r=f.at("idle");if(y<=r.at("vertical_at_most").get<int>()){y+=r.at("vertical_add").get<int>();}s.setVariable(field,y);
        if(inFront(r.at("player_front_range_pixels"))){enter(r.at("front_next_mode"));}
        else if(s.variable(profile_.at("burn").at("reaction_variable"))>=r.at("reaction_at_least").get<int>()){s.setVariable(profile_.at("burn").at("reaction_variable"),0);enter(r.at("reaction_next_mode"));}
        bodyWait_=r.at("wait_tenths").get<int>()*6;return;
    }
    const char* name=mode_==1?"open":mode_==2?"shake":mode_==3?"charge":"retreat";const auto& r=f.at(name);
    if(mode_==3&&y>r.at("vertical_at_most").get<int>()){}else{y+=r.at("vertical_add").get<int>();s.setVariable(field,y);++iterations_;}
    bool done=mode_==3?(y>r.at("vertical_at_most").get<int>()||inFront(r.at("player_front_range_pixels"))):iterations_>=(mode_==10?s.variable(117):r.at("iterations").get<int>());
    if(!done){bodyWait_=std::max(1,r.at("wait_tenths").get<int>()*6);return;}
    if(mode_==1||mode_==3){s.setVariable(field,y+r.at("finish_vertical_add").get<int>());emit("m109-e5",mode_==1?2:4,mode_==1?16:22);picture(profile_.at("roles").at("m109-e5").at(mode_==1?"2":"4").at("media").back().at("cue"),center(1));
        if(inFront(r.at("bite_range_pixels"))){s.setVariable(196,1);}s.setVariable(117,r.at("retreat_steps"));
    }else if(mode_==10){picture(profile_.at("roles").at("m109-e5").at("5").at("media").at(1).at("cue"),center(1));}
    enter(r.at("next_mode"));
}
void CrocodileController::updateFeet(){
    auto& s=scene_.session_;const auto& feet=profile_.at("feet");const int speed=feet.at("speed_by_body_mode").at(std::to_string(mode_));
    for(int index=0;index<2;++index){const auto& foot=feet.at(index==0?"left":"right");const int phaseField=foot.at("phase_variable"),modeField=foot.at("mode_variable");int phase=s.variable(phaseField),footMode=s.variable(modeField);const int y=s.variable(profile_.at("body").at("body_offset_variable"))-10+phase;s.setVariable(index==0?98:112,y);
        const bool rising=index==0?footMode==1:footMode==2;phase+=rising?speed:-speed;
        if(rising&&phase>foot.at("peak_greater_than").get<int>()){
            s.setVariable(modeField,index==0?2:1);const int page=mode_==0?1:mode_==1?2:4;emit(feet.at("object"),page,index==0?14:mode_==0?57:40);emit(feet.at("object"),page,index==0?15:mode_==0?58:41);
            if(index==0&&mode_==0){const auto player=scene_.sampleActorScreen("player");const auto half=foot.at("crush_half_pixels").get<std::array<int,2>>();if(std::abs(player.x-s.variable(97))<=half[0]&&std::abs(player.y-y)<=half[1]){s.setVariable(196,foot.at("crush_hit_mode"));}}
        }else if(!rising&&phase<foot.at("trough_less_than").get<int>()){s.setVariable(modeField,index==0?1:2);}
        s.setVariable(phaseField,phase);
    }
    const auto& roles=profile_.at("roles").at("m109-e3").at("2").at("media");for(int i=0;i<3;++i){picture(roles.at(i).at("cue"),center(i+1));}
}
void CrocodileController::hitPlayer(){
    auto& s=scene_.session_;const auto& hit=profile_.at("player_hit");const int mode=s.variable(196);if(!mode||hitWait_>0){return;}s.setVariable(196,0);if(s.flag(hit.at("invulnerability_fact"))){return;}
    if(mode==2){s.setFlag(18,false);s.setFlag(hit.at("clear_action_fact"),false);s.setVariable(31,7);emit(hit.at("object"),3,6);finished_=true;scene_.startDeath();return;}
    s.setVariable(23,profile_.at("attributes").at("attack"));s.setVariable(40,s.defense());scene_.applyPlayerDamage(biteDamage(s.variable(23),s.variable(40),scene_.random_(-5,2)));
    if(s.hp()<=0){s.setFlag(18,false);finished_=true;scene_.startDeath();return;}
    emit(hit.at("object"),2,12);s.setVariable(24,2);s.setFlag(hit.at("clear_action_fact"),true);scene_.cancelPlayerActions("ce5");s.setFlag(18,false);scene_.startRoute("m109-e2-p2-c20");hitWait_=1;
}
void CrocodileController::movementSpeed(){
    if(!scene_.session_.flag(18)||std::any_of(scene_.routes_.begin(),scene_.routes_.end(),[](const auto& route){return route.actor()=="player";})){return;}
    const auto& p=profile_.at("player_hit");auto actor=scene_.world_->player();const bool shoe=scene_.session_.equipped(4)==p.at("shoe_item");
    actor.setSpeedLevel(shoe?p.at("shoe_movespeed").get<int>():p.at("normal_movespeed").get<int>());scene_.world_->setPlayerState(actor);
}
AttackContact CrocodileController::hit(int slot,int page,const AttackSample* sample){
    auto& s=scene_.session_;const auto& row=profile_.at("slots").at(std::to_string(slot));const auto& r=profile_.at("receivers").at(row.at("receiver_object").get<std::string>()).at(std::to_string(page));
    if(!r.at("enabled").get<bool>()){return {true,true,0,false};}
    if(s.variable(profile_.at("damage_source_variable"))>=r.at("guard_damage_below").get<int>()){return {};}
    int amount;bool end=false;
    s.setVariable(49,profile_.at("attributes").at("defense"));
    if(!sample){const auto& variance=r.at("gas_variance_inclusive");amount=100+scene_.random_(variance.at(0),variance.at(1))-s.variable(49);if(s.flag(12)){amount*=r.at("critical_multiplier").get<int>();}}
    else{
        s.setFlag(12,false);const int count=s.variable(35)+1;s.setVariable(35,count);
        const int attack=auxiliaryDamage(sample->kind)?s.auxiliaryAttack():s.attack()+(sample->aimed?scene_.content_.items.at(scene_.content_.rules.at("handgun_damage").at("aim_attack_item")).attack:0);
        const auto rolled=rollWeaponDamage(sample->kind,attack,s.variable(49),sample->advances,scene_.random_);amount=rolled.amount;end=rolled.endRequested;
        if(sample->kind==AttackKind::Knife){if(count%3==0&&scene_.random_(0,100)-count<=15){amount=s.auxiliaryAttack()+scene_.random_(0,10);}scene_.playSound("f3-ce11-c13");}
        else if(!auxiliaryDamage(sample->kind)){const bool critical=s.flag(47)||(count%3==0&&scene_.random_(0,100)-count<=mainCriticalThreshold(sample->kind));s.setFlag(47,false);if(critical){s.setVariable(35,0);s.setFlag(12,true);amount*=r.at("critical_multiplier").get<int>();}}
        amount/=r.at("normal_divisor").get<int>();
    }
    s.setVariable(34,amount);s.setVariable(profile_.at("damage_source_variable"),s.variable(profile_.at("damage_source_variable"))+amount);
    if(!r.at("reaction_count_add").is_null()){const auto& reaction=r.at("reaction_count_add");const int add=reaction.contains("constant")?reaction.at("constant").get<int>():scene_.random_(reaction.at("random_inclusive").at(0),reaction.at("random_inclusive").at(1));s.setVariable(profile_.at("burn").at("reaction_variable"),s.variable(profile_.at("burn").at("reaction_variable"))+add);}
    if(!r.at("retreat_steps").is_null()){s.setVariable(117,r.at("retreat_steps"));enter(r.at("retreat_mode"));}
    for(const auto& cue:r.at("media").at("media")){scene_.cue(cue.at("cue"),row.at("receiver_object"));}
    // M109 E8/E9 C4-9 use CE18; normal hits sample event1, gas hits event6.
    const std::string actor=sample?"player":profile_.at("gas").at("object").get<std::string>();
    scene_.presentation_.push_back({PresentationEvent::Kind::Effect,"enemy-slot-18-blood-"+std::to_string(r.at("blood_work").get<int>()),{},findCharacter(*scene_.world_,actor).motion.position(),0,actor});
    return {true,true,amount,end};
}
AttackResolution CrocodileController::receive(const AttackSample& sample,std::set<std::string>& resolved){
    AttackResolution result;if(finished_||scene_.foreground_==ForegroundKind::Death||scene_.session_.hp()<=0){return result;}
    const auto position=attackScreen(sample.center);auto& s=scene_.session_;
    for(int slot=1;slot<=3;++slot){const std::string key="crocodile-slot-"+std::to_string(slot);if(resolved.contains(key)){continue;}const auto& row=profile_.at("slots").at(std::to_string(slot));
        if(!overlaps(position,center(slot),sample.halfPixels,row.at("half_pixels").get<std::array<int,2>>())){continue;}
        ++result.contacts;if(sample.phase==AttackPhase::Flight){continue;}
        if(sample.phase==AttackPhase::Burning){const int flag=36+(slot-1)*2;s.setFlag(flag,true);resolved.insert(key);++result.processed;continue;}
        if(sample.receiverPage<2||sample.receiverPage>6){continue;}
        if(sample.kind==AttackKind::GlFlame){const int flag=36+(slot-1)*2;if(s.flag(flag)){continue;}s.setFlag(flag,true);}
        const auto contact=hit(slot,sample.receiverPage,&sample);result.damage+=contact.damage;result.endRequested|=contact.endRequested;if(contact.processed){resolved.insert(key);++result.processed;}
    }
    const auto& gas=profile_.at("gas");const std::string id=gas.at("object");
    if(s.flag(gas.at("on_fact"))&&gasFrame_<0&&!resolved.contains(id)){
        auto center=scene_.sampleActorScreen(id);center.y+=gas.at("active_center_offset_pixels").at(1).get<int>();center.x+=gas.at("active_center_offset_pixels").at(0).get<int>();
        if(overlaps(position,center,sample.halfPixels,gas.at("contact_half_pixels").get<std::array<int,2>>())){++result.contacts;if(sample.phase!=AttackPhase::Flight){s.setVariable(147,gas.at("hit_work"));gasFrame_=0;gasTick_=0;resolved.insert(id);++result.processed;}}
    }
    return result;
}
void CrocodileController::updateGas(){
    auto& s=scene_.session_;const auto& g=profile_.at("gas");const std::string id=g.at("object");const int on=g.at("on_fact");
    if(!s.flag(18)){return;}
    if(gasFrame_>=0){
        const auto sampled=scene_.sampleActorScreen(id);GridPosition blast{sampled.x+g.at("blast_center_offset_pixels").at(0).get<int>(),sampled.y+g.at("blast_center_offset_pixels").at(1).get<int>()};
        if(gasTick_==0&&gasFrame_==0){for(int command=1;command<=6;++command){emit(id,10,command);}}
        if(gasTick_==0){const int command=gasFrame_==0?10:gasFrame_==1?16:gasFrame_==2?31:gasFrame_==3?37:gasFrame_==4?43:49;const auto& role=profile_.at("roles").at(id).at("10");for(const auto& c:role.at("media")){if(c.at("command")==command){picture(c.at("cue"),blast);}}
            if(gasFrame_==g.at("show_damage_after_frame").get<int>()-1){s.setVariable(22,g.at("blast_receiver_page"));if(overlaps(blast,center(1),g.at("blast_half_pixels").get<std::array<int,2>>(),profile_.at("slots").at("1").at("half_pixels").get<std::array<int,2>>())){hit(1,g.at("blast_receiver_page"));}}
        }
        if(++gasTick_>=g.at("ticks_per_frame").get<int>()){gasTick_=0;if(++gasFrame_>=g.at("frames").get<int>()){for(int command=52;command<=55;++command){emit(id,10,command);}gasFrame_=-1;s.setFlag(on,false);s.setVariable(147,0);gasWait_=g.at("spawn_wait_tenths").at(scene_.random_(g.at("wait_random_inclusive").at(0),g.at("wait_random_inclusive").at(1))-1).get<int>()*6;}}
        return;
    }
    if(!s.flag(on)){
        if(gasWait_>0&&--gasWait_>0){return;}const auto& xs=g.at("spawn_tile_x_inclusive");auto actor=findCharacter(*scene_.world_,id);actor.motion=CharacterMotion({scene_.random_(xs.at(0),xs.at(1)),g.at("spawn_tile_y")},actor.motion.facing(),actor.motion.speed());replaceCharacter(*scene_.world_,actor);s.setFlag(on,true);s.setVariable(147,0);
        const auto screen=scene_.sampleActorScreen(id);GridPosition image{screen.x,screen.y+g.at("picture_center_offset_pixels").at(1).get<int>()};
        const auto& role=profile_.at("roles").at(id).at("8");for(const auto& c:role.at("media")){if(c.at("command")==29||c.at("command")==30){auto pos=image;if(c.at("command")==29){pos.x=160;}picture(c.at("cue"),pos);}}return;
    }
    const auto screen=scene_.sampleActorScreen(id);const auto& role=profile_.at("roles").at(id).at("9");for(const auto& c:role.at("media")){if(c.at("kind")=="move_picture"){GridPosition pos{screen.x,screen.y-8};if(c.at("command")==12){pos.x=160;}picture(c.at("cue"),pos);}}
}
void CrocodileController::update(){
    if(finished_||!scene_.active_||scene_.foreground_==ForegroundKind::Death||scene_.session_.hp()<=0){return;}auto& s=scene_.session_;
    if(s.variable(profile_.at("damage_source_variable"))>=profile_.at("victory_at_least").get<int>()){
        finished_=true;scene_.weapon_->cancel(ActionCancel::Transition);scene_.routes_.clear();scene_.startContentFlow("crocodile-victory");return;
    }
    if(hitWait_>0&&!scene_.routesPending()){hitWait_=0;s.setFlag(18,true);s.setFlag(profile_.at("player_hit").at("clear_action_fact"),false);}
    if(!s.flag(profile_.at("activation_fact"))){return;}if(s.variable(91)!=mode_){enter(s.variable(91));}hitPlayer();if(scene_.foreground_==ForegroundKind::Death){return;}updateFeet();updateBody();updateGas();updateBurn();
}

void CrocodileController::updateBurn(){
    auto& s=scene_.session_;const auto& burns=scene_.content_.full->at("runtime").at("weapon_actions");
    for(int slot=1;slot<=3;++slot){const std::string actor=slot==1?"m109-e9":slot==2?"m109-e8":"crocodile-side-third";
        const int amount=EnemyHost(scene_,actor).advanceBurn(center(slot));const auto& rule=burns.at("burns").at(std::to_string(slot));const int field=rule.at("local_damage_source_variable");s.setVariable(field,s.variable(field)+amount);
    }
}
