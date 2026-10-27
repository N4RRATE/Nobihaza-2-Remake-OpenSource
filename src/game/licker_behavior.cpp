#include "licker_behavior.h"
#include <algorithm>
#include <cmath>
LickerBehavior::LickerBehavior(CombatScene& scene,const EnemyBlueprint& definition):definition_(definition),host_(scene,definition.id){
    dead_=host_.session().flag(definition.deadFlag);
    // Door initialization precedes relocation; commit constructs the controller against that source state.
    if(rules().contains("entry_state_variable")&&host_.session().variable(rules().at("entry_state_variable"))==9){activateState(9);}
    sampleBody();
}
bool LickerBehavior::activateFromEncounter(){if(dead_){return false;}active_=true;change(State::Pursuit);return true;}
bool LickerBehavior::activateState(int state){
    if(state==2){return activateFromEncounter();}if(state!=9||dead_){return false;}
    active_=true;change(State::Drop);return true;
}
const nlohmann::json& LickerBehavior::rules() const{return definition_.rules.is_null()?host_.content().expansion.behaviorDefinitions.at("licker"):definition_.rules;}
const ObjectVariant& LickerBehavior::page(const std::string& role) const{
    const auto& profile=rules().at("runtime_profile");const int n=role=="post_drop_recovery"?profile.at("post_drop").at("page"):profile.at("pages").at(role);
    const auto& variants=host_.content().objects.at(id()).variants;
    for(const auto& v:variants){if(v.page==n){return v;}}
    throw std::runtime_error("Missing licker page: "+role);
}
void LickerBehavior::applyPage(const std::string& role){
    const auto& v=page(role);const auto& p=v.definition;auto a=host_.actor();
    a.active=true;a.spriteId=p.spriteId;a.layer=p.layer;a.through=a.routeThrough.value_or(p.through);a.setSpeedLevel(p.moveSpeed);
    a.setAnimation(CharacterAnimation(animationForSpeed(p.moveSpeed),p.initialColumn,p.animationType),a.motion.facing());
    host_.setActor(a);autonomous_.reset();if(!v.autonomousRoute.steps.empty()){autonomous_.emplace(v.autonomousRoute,id());}
}
EnemyMode LickerBehavior::mode() const{
    if(dead_){return EnemyMode::Corpse;}if(!active_){return EnemyMode::Dormant;}
    if(pendingGrab_){return EnemyMode::PendingGrab;}if(holdsPlayer()){return EnemyMode::Grab;}
    if(action_==Action::Ending){return EnemyMode::Ending;}if(state_==State::Down){return EnemyMode::HitRecovery;}
    if(action_!=Action::None){return EnemyMode::Windup;}return EnemyMode::Pursuit;
}
void LickerBehavior::change(State next){
    state_=next;
    if(rules().value("source_cache",false)){host_.session().setVariable(rules().at("entry_state_variable"),next==State::Landing?10:static_cast<int>(next));}
    action_=Action::None;phase_=wait_=0;host_.cancelRoutes();
    attackActive_=next!=State::Wander&&next!=State::Stalk&&next!=State::Pursuit;
    if(rules().value("source_cache",false)&&next!=State::Down){host_.session().setFlag(rules().at("runtime_profile").at("attack_switch"),attackActive_);}
    switch(next){
    case State::Wander:applyPage("wander");break;case State::Stalk:applyPage("stalk");stalkOrigin_=host_.actor().motion.tile();break;
    case State::Pursuit:applyPage("pursuit");break;case State::Claw:applyPage("claw");begin(Action::Claw);break;
    case State::Pounce:applyPage("pounce");break;case State::FindWall:applyPage("find_wall");scanDistance_=1;break;
    case State::ToWall:applyPage("to_wall");break;case State::Ceiling:applyPage("climb");begin(Action::Climb);break;
    case State::TongueGrab:applyPage("grab");pendingGrab_=true;host_.requestGrab();break;
    case State::Drop:applyPage("drop");break;case State::Down:applyPage("down");begin(Action::Down);break;
    case State::Landing:applyPage("post_drop_recovery");begin(Action::DropWait);break;
    }
}
void LickerBehavior::begin(Action action){
    action_=action;phase_=wait_=0;
    const auto& profile=rules().at("runtime_profile");
    if(action==Action::Down&&profile.contains("down_flash")&&cueCondition(profile.at("down_flash"))){host_.cue(profile.at("down_flash").at("cue"));}
    if(action==Action::Ending&&profile.contains("ending")){
        const auto& conditional=profile.at("ending").at("conditional_cue");if(cueCondition(conditional)){host_.cue(conditional.at("cue"));}
    }
}
std::string LickerBehavior::cueId(int page,int command) const{return rules().at("runtime_profile").at("cues").at("p"+std::to_string(page)+"-c"+std::to_string(command));}
bool LickerBehavior::play(int p,int command){
    return playCue(cueId(p,command));
}
bool LickerBehavior::playCue(const std::string& key){
    const auto& cue=host_.content().cue(key);const std::string kind=cue.at("kind");
    if(kind=="wait"){wait_=std::max(1,cue.at("tenths").get<int>()*6);return false;}
    host_.cue(key);return true;
}
bool LickerBehavior::sequence(const std::vector<int>& commands,int p){
    while(phase_<static_cast<int>(commands.size())){if(!play(p,commands[phase_++])){return false;}}
    return true;
}
int LickerBehavior::forwardDistance(int minimum,int maximum) const{
    auto tile=host_.actor().motion.tile();const auto direction=host_.actor().motion.facing();const auto player=host_.player().motion.tile();
    for(int d=1;d<=maximum;++d){tile=adjacentTile(tile,direction);if(d>=minimum&&tile==player){return d;}}return 0;
}
bool LickerBehavior::frontRow(bool randomDown){
    const auto a=host_.actor();const auto p=host_.player().motion.tile();const auto f=adjacentTile(a.motion.tile(),a.motion.facing());
    if(a.motion.facing()==Direction::Up||a.motion.facing()==Direction::Down){
        if(p.y!=f.y||p.x<f.x-1){return false;}
        return p.x<=f.x-1+(randomDown&&a.motion.facing()==Direction::Down?host_.random(2,3):2);
    }return p.x==f.x&&std::abs(p.y-f.y)<=1;
}
void LickerBehavior::move(){if(autonomous_&&!host_.routePending()){
    host_.advance(*autonomous_,[&](int source,bool enabled){
        if(source!=rules().at("runtime_profile").at("attack_switch").get<int>()){throw std::logic_error("Unknown enemy route switch");}
        attackActive_=enabled;if(rules().value("source_cache",false)){host_.session().setFlag(source,enabled);}
    });
}}
void LickerBehavior::update(const InputFrame& input){
    if(retired_||dead_||host_.session().hp()==0){return;}
    sampleBody();
    if(!active_){if(definition_.activationFlag&&!host_.session().flag(*definition_.activationFlag)){return;}active_=true;change(State::Wander);}
    if(action_==Action::Ending){if(!rules().contains("ending_requires_fact")||host_.session().flag(rules().at("ending_requires_fact"))){advanceAction(input);}return;}
    if(!host_.session().flag(18)){return;}
    if(host_.session().flag(10)&&!holdsPlayer()&&!pendingGrab_&&static_cast<int>(state_)<=4){
        if(state_!=State::Wander||action_!=Action::None){change(State::Wander);}
        move();return;
    }
    if(pendingGrab_){return;}
    if(state_==State::Stalk&&action_==Action::None){
        if(host_.session().flag(5)){change(State::Pursuit);}
        else if(host_.actor().motion.tile()!=stalkOrigin_){change(State::Wander);}
    }
    if(state_==State::Wander&&host_.session().flag(5)){change(State::Stalk);}
    if(state_==State::Drop&&!attackActive_&&host_.session().flag(5)){state_=State::Ceiling;if(rules().value("source_cache",false)){host_.session().setVariable(rules().at("entry_state_variable"),7);}applyPage("ceiling");}
    if(state_==State::Ceiling&&action_==Action::None&&++footstepTicks_>=12){
        footstepTicks_=0;host_.sound(rules().at("runtime_profile").at("helper_sounds").at("ceiling"));
    }
    if(action_!=Action::None){advanceAction(input);return;}
    decide();if(action_==Action::None){move();}
}
void LickerBehavior::decide(){
    const auto a=host_.actor();const auto p=host_.player().motion.tile();
    switch(state_){
    case State::Wander:
        if(damage_>=rules().at("rage_damage_threshold").get<int>()||host_.session().flag(rules().value("rage_fact",38))){change(State::Pursuit);return;}
        if(frontRow()){change(State::Claw);return;}
        // P10's vertical tongue inequalities are contradictory in the source.
        if((a.motion.facing()==Direction::Left||a.motion.facing()==Direction::Right)&&forwardDistance(1,3)){
            change(State::Pursuit);attackActive_=true;begin(Action::Tongue);return;
        }
        if(!host_.session().flag(3)&&std::abs(a.motion.tile().x-p.x)<=5&&std::abs(a.motion.tile().y-p.y)<=5){change(State::Stalk);}return;
    case State::Stalk:
        if(host_.session().flag(rules().value("rage_fact",38))){change(State::Pursuit);return;}
        if(forwardDistance(1,3)||a.motion.tile()==p){change(State::Pursuit);attackActive_=true;begin(Action::Tongue);return;}
        if((a.motion.tile().x==p.x||a.motion.tile().y==p.y)&&std::abs(a.motion.tile().x-p.x)+std::abs(a.motion.tile().y-p.y)>=4&&std::abs(a.motion.tile().x-p.x)+std::abs(a.motion.tile().y-p.y)<=5){pouncePreparationPage_=11;begin(Action::PouncePrepare);return;}
        if(std::abs(a.motion.tile().x-p.x)<=3&&std::abs(a.motion.tile().y-p.y)<=3){change(State::Pursuit);}return;
    case State::Pursuit:{
        if(frontRow(true)){change(State::Claw);return;}
        const auto front=adjacentTile(a.motion.tile(),a.motion.facing());
        if(terrain(front)==3){
            const bool vertical=a.motion.facing()==Direction::Up||a.motion.facing()==Direction::Down;
            const int delta=vertical?p.x-a.motion.tile().x:p.y-a.motion.tile().y;
            if(delta){const auto direction=vertical?(delta>0?Direction::Right:Direction::Left):(delta>0?Direction::Down:Direction::Up);
                RouteDefinition route;route.frequency=8;route.skipBlocked=true;route.steps.push_back({std::string(directionName(direction)),{},{},0});host_.route(route);}
        }return;
    }
    case State::Pounce:
        if(!attackActive_){change(State::Pursuit);return;}
        if(a.motion.tile()==p){if(!host_.session().flag(13)){play(16,14);hurt(actionAttack("pounce")*rules().at("damage_sources").at("pounce").at("attack_multiplier").get<int>(),2);}begin(Action::PounceRecover);}return;
    case State::FindWall:
        do{
            for(auto direction:{Direction::Up,Direction::Right,Direction::Down,Direction::Left}){
                auto tile=a.motion.tile();for(int n=0;n<scanDistance_;++n){tile=adjacentTile(tile,direction);}
                if(terrain(tile)==3){auto actor=a;actor.face(direction);host_.setActor(actor);change(State::ToWall);return;}
            }
            if(!host_.hasTerrainAtDistance(scanDistance_)){return;}++scanDistance_;
        }while(rules().value("wall_search_same_update",false));return;
    case State::ToWall:
        if(!attackActive_){change(State::FindWall);return;}
        if(terrain(adjacentTile(a.motion.tile(),a.motion.facing()))==3){change(State::Ceiling);}return;
    case State::Ceiling:
        if(a.motion.tile()==p&&!host_.session().flag(10)){captured_=a.motion.tile();begin(Action::CeilingPrepare);}return;
    case State::Drop:
        if(std::abs(a.motion.tile().x-p.x)<=3&&std::abs(a.motion.tile().y-p.y)<=3){
            if(holdsPlayer()){release(false);}attackActive_=true;begin(Action::Drop);
        }return;
    default:return;
    }
}
void LickerBehavior::hurt(int attack,int bloodType){
    if(host_.session().flag(13)||host_.session().hp()==0){return;}
    if(bloodType){host_.present({PresentationEvent::Kind::Effect,rules().at("runtime_profile").at("blood_effects").at(std::to_string(bloodType)),{},host_.player().motion.position(),0});}
    if(rules().value("source_receivers",false)){host_.receiveSourceDamage(attack);}else{host_.damagePlayer(biteDamage(attack,host_.session().defense(),host_.random(-5,2)));}
}
void LickerBehavior::advanceAction(const InputFrame& input){
    if(action_==Action::GrabLoop){
        if(inputCooldown_>0){--inputCooldown_;}
        else{for(auto action:{InputAction::Down,InputAction::Left,InputAction::Right,InputAction::Up}){
            if(input.buttons[static_cast<int>(action)].pressed){++escape_;inputCooldown_=1;break;}
        }}
    }
    if(wait_>0&&--wait_>0){return;}
    switch(action_){
    case Action::Ending:{
        const auto& profile=rules().at("runtime_profile");const auto commands=profile.contains("ending")?profile.at("ending").at("commands").get<std::vector<int>>():std::vector<int>{5,6,7};
        // Parking profiles map semantic commands; M26's ending uses source positions.
        if(profile.contains("ending")&&!profile.contains("cue_commands")){
            while(phase_<static_cast<int>(commands.size())){if(!playCue(id()+"-p"+std::to_string(profile.at("ending").at("page").get<int>())+"-c"+std::to_string(commands[phase_++]))){return;}}
        }else if(!sequence(commands,profile.at("pages").at("ending"))){return;}
        {host_.session().setFlag(definition_.deadFlag,true);dead_=true;body_={};applyPage("corpse");}return;
    }
    case Action::Claw:
        if(sequence({9,10},15)){
            if(forwardDistance(1,1)){begin(Action::ClawWindup);}else{change(State::Pursuit);}
        }return;
    case Action::ClawWindup:
        if(sequence({37,38,39,40,41},15)){
            const auto a=host_.actor();auto first=adjacentTile(a.motion.tile(),a.motion.facing());
            if(first==host_.player().motion.tile()&&!host_.session().flag(13)){play(15,69);hurt(actionAttack("claw"),11);}
            const auto side=a.motion.facing()==Direction::Up?Direction::Right:a.motion.facing()==Direction::Down?Direction::Left:Direction::Down;
            if(adjacentTile(first,side)==host_.player().motion.tile()&&!host_.session().flag(13)){play(15,106);hurt(actionAttack("claw"),11);}
            begin(Action::ClawRecover);
        }return;
    case Action::ClawRecover:
        if(sequence({118,119,120,121,122,123,124,125},15)){
            if(host_.random(0,10)<=3){change(State::FindWall);return;}
            if(forwardDistance(1,3)||host_.actor().motion.tile()==host_.player().motion.tile()){change(State::Pursuit);attackActive_=true;begin(Action::Tongue);}
            else{change(State::Stalk);}
        }return;
    case Action::Tongue:
        if(sequence({9,10,11,12,13,14,15,16,17,18,19},14)){
            captured_=host_.actor().motion.tile();tongueDistance_=0;
            const int animation=host_.actor().motion.facing()==Direction::Up?23:host_.actor().motion.facing()==Direction::Right?28:host_.actor().motion.facing()==Direction::Down?33:38;
            play(14,animation);begin(Action::TongueSample);
        }return;
    case Action::TongueSample:
        captured_=adjacentTile(captured_,host_.actor().motion.facing());++tongueDistance_;
        if(captured_==host_.player().motion.tile()&&!host_.session().flag(13)){
            play(14,51);hurt(actionAttack("tongue")+rules().at("damage_sources").at("tongue").at("attack_add").get<int>(),1);host_.cancelWeapon();begin(Action::TongueRecover);
        }else if(tongueDistance_==3){begin(Action::TongueRecover);}else{wait_=1;}return;
    case Action::TongueRecover:
        if(sequence({149,150,151,152,153,154,155,156,157},14)){
            const auto a=host_.actor().motion.tile();const auto p=host_.player().motion.tile();
            if(damage_>=rules().at("rage_damage_threshold").get<int>()&&(a.x==p.x||a.y==p.y)&&std::abs(a.x-p.x)+std::abs(a.y-p.y)<=5){
                pouncePreparationPage_=14;begin(Action::PouncePrepare);
            }else{change(State::Pursuit);}
        }return;
    case Action::PouncePrepare:{
        // All four source branches use the same graphic and sound sequence.
        const auto commands=pouncePreparationPage_==11?std::vector<int>{96,97,98,99,100,101,102,103}:std::vector<int>{167,168,169,170,171,172,173,174};
        if(sequence(commands,pouncePreparationPage_)){change(State::Pounce);}return;
    }
    case Action::PounceRecover:
        if(sequence({23,24,25,26,27,28},16)){change(State::Pursuit);}return;
    case Action::Climb:
        if(sequence({4,5},21)){begin(Action::ClimbRoute);}return;
    case Action::ClimbRoute:
        if(!host_.routePending()){attackActive_=false;applyPage("ceiling");begin(Action::None);}return;
    case Action::CeilingPrepare:
        if(sequence({11,12,13,14},20)){
            if(captured_==host_.player().motion.tile()&&!host_.session().flag(10)&&!host_.session().flag(13)){change(State::TongueGrab);}
            else{begin(Action::CeilingMiss);wait_=18;}
        }return;
    case Action::CeilingMiss:change(State::Drop);attackActive_=false;return;
    case Action::Grab:
        if(sequence({7,12,13,14},22)){
            if(rules().value("source_cache",false)&&(host_.routePending()||host_.playerRoutePending())){return;}
            auto actor=host_.actor();actor.motion=CharacterMotion(host_.player().motion.tile(),actor.motion.facing(),actor.motion.speed());host_.setActor(actor);
            escape_=cycles_=0;inputCooldown_=1;begin(Action::GrabLoop);wait_=rules().at("grapple").at("period_tenths").get<int>()*6;
        }return;
    case Action::GrabLoop:{
        int required=0;for(const auto& threshold:rules().at("grapple").at("escape_thresholds")){
            if(threshold.at("hp_at_most").is_null()||(rules().value("source_cache",false)?host_.session().variable(9):host_.session().hp())<=threshold.at("hp_at_most").get<int>()){required=threshold.at("input_sum");break;}
        }
        if(escape_>=required){begin(Action::GrabEscape);return;}
        ++cycles_;play(22,67);
        if(cycles_>=rules().at("grapple").at("failure_cycles").get<int>()){begin(Action::GrabFatal);}else{wait_=rules().at("grapple").at("period_tenths").get<int>()*6;}return;
    }
    case Action::GrabEscape:
        if(sequence({80,81},22)){
            host_.cancelWeapon();host_.session().setVariable(31,0);host_.cue("ce9-c3",false);host_.startRoute("ce9-c4",true);
            play(22,85);begin(Action::GrabEscapeRoute);
        }return;
    case Action::GrabEscapeRoute:
        if(host_.playerRoutePending()){return;}
        if(phase_++==0){wait_=6;return;}
        host_.session().startRecovery(0,90);host_.session().setFlag(1,true);change(State::Drop);return;
    case Action::GrabFatal:
        if(sequence({69,70,71,72},22)){begin(Action::GrabFatalRoute);}return;
    case Action::GrabFatalRoute:
        if(!host_.playerRoutePending()){host_.fail(rules().at("grapple").at("failure_flow"));}return;
    case Action::Drop:
        play(23,19);begin(Action::DropRoute);return;
    case Action::DropRoute:
        if(!host_.routePending()){
            if(rules().at("runtime_profile").contains("post_drop")){
                const auto& drop=rules().at("runtime_profile").at("post_drop");change(State::Landing);attackActive_=drop.at("attack_active");
                const auto& conditional=drop.at("conditional_cue");if(cueCondition(conditional)){host_.cue(conditional.at("cue"));}
            }
            else{begin(Action::DropWait);wait_=30;}
        }return;
    case Action::DropWait:{
        const auto& profile=rules().at("runtime_profile");
        if(profile.contains("post_drop")){
            const auto& drop=profile.at("post_drop");
            const auto& cues=drop.at("cues");
            while(phase_<static_cast<int>(cues.size())){if(!playCue(cues[phase_++])){return;}}
            attackActive_=drop.at("clear_attack_value");
            const auto& range=drop.at("next_mode");change(static_cast<State>(host_.random(range[0],range[1])));
        }else{change(static_cast<State>(host_.random(0,2)));}return;
    }
    case Action::Down:
        if(sequence({8,9,10,11,12},24)){change(State::Pursuit);}return;
    default:return;
    }
}
bool LickerBehavior::beginGrab(){
    pendingGrab_=false;
    if(captured_!=host_.player().motion.tile()||host_.session().flag(10)||host_.session().flag(13)){
        state_=State::Ceiling;if(rules().value("source_cache",false)){host_.session().setVariable(rules().at("entry_state_variable"),7);}applyPage("ceiling");begin(Action::CeilingMiss);wait_=18;return false;
    }begin(Action::Grab);host_.cancelWeapon();host_.cancelRoutes(true);
    host_.sound(rules().at("runtime_profile").at("helper_sounds").at("grab_voice"));
    host_.session().setFlag(10,true);host_.session().setFlag(1,false);host_.session().setVariable(31,2);
    for(const auto& cue:rules().at("runtime_profile").at("grab_pose")){host_.cue(cue,false);}
    return true;
}
void LickerBehavior::release(bool interrupted){
    if(holdsPlayer()){
        host_.cancelRoutes(true);host_.cancelWeapon();
        if(interrupted){host_.session().startRecovery(0,90);host_.session().setFlag(1,true);attackActive_=false;}
        host_.session().setFlag(10,false);
    }
    host_.releaseGrab();pendingGrab_=false;
}
AttackContact LickerBehavior::receiveAttack(const AttackSample& sample){
    if(retired_){return {};}
    const auto center=sample.center;const auto kind=host_.damageKind(sample.kind);const int advances=sample.advances;const bool aimed=sample.aimed;
    if(dead_||!active_||action_==Action::Ending){return {};}
    const auto position=host_.actor().motion.position();const auto cached=host_.screenPicturePosition(body_);const GridPosition target=rules().value("source_cache",false)?GridPosition{(cached.x+.5)*16,(cached.y+1)*16}:static_cast<int>(state_)>=7&&static_cast<int>(state_)<10?GridPosition{}:GridPosition{(position.x+.5)*16,(position.y+.5)*16};
    if(std::abs(center.x-target.x)>sample.halfPixels[0]+definition_.hitboxHalfPixels[0]||std::abs(center.y-target.y)>sample.halfPixels[1]+definition_.hitboxHalfPixels[1]){return {};}
    if(sample.phase==AttackPhase::Flight){if(rules().value("source_receivers",false)&&static_cast<int>(state_)>=100){return {};}return {true,false,0,false};}
    if(kind==AttackKind::GlFlame&&host_.burning()){return {true,false,0,false};}
    if(sample.phase==AttackPhase::Burning){if(rules().value("outer_burn",false)||static_cast<int>(state_)<rules().at("receive_state_less_than").get<int>()){host_.startBurn();}return {true,true,0,false};}
    if((kind==AttackKind::GlFlame||(kind==AttackKind::Rocket&&rules().value("outer_burn",false)))&&(rules().value("outer_burn",false)||static_cast<int>(state_)<rules().at("receive_state_less_than").get<int>())){host_.startBurn();}
    if(static_cast<int>(state_)>=rules().at("receive_state_less_than").get<int>()){return {true,true,0,false};}
    const int receiver=sample.receiverPage;
    if(receiver==1){play(1,host_.actor().motion.facing()==Direction::Down?7:host_.actor().motion.facing()==Direction::Left?11:host_.actor().motion.facing()==Direction::Up?15:19);attackActive_=false;return {true,true,0,false};}
    if(rules().value("source_receivers",false)){host_.session().setFlag(rules().at("runtime_profile").at("receiver_clear_facts").at(std::to_string(receiver)),false);}
    host_.session().setFlag(12,false);
    const int count=host_.session().variable(35)+1;host_.session().setVariable(35,count);
    const int attack=auxiliaryDamage(kind)?host_.session().auxiliaryAttack():host_.session().attack()+(aimed?host_.content().items.at(host_.content().rules.at("handgun_damage").at("aim_attack_item")).attack:0);
    host_.session().setVariable(49,definition_.defense);
    const auto rolled=rules().value("source_receivers",false)&&kind==AttackKind::GlFlame&&advances>=2?WeaponDamage{0,false}:rollWeaponDamage(kind,attack,definition_.defense,advances,[&](int lo,int hi){return host_.random(lo,hi);});
    int amount=rolled.amount;
    bool critical=false;
    if(kind==AttackKind::Knife){
        if(count%3==0){const int roll=host_.random(0,100)-count;const int item=host_.session().equipped(2);
            if(!rules().value("source_receivers",false)||item==24){if(roll<=15){amount=host_.session().auxiliaryAttack()+host_.random(0,10);}}
            else if(item==25){if(roll<=5){amount=host_.session().auxiliaryAttack()+host_.random(0,10);}if(roll<=10){amount=host_.session().auxiliaryAttack()+host_.random(0,10);}}
        }
        host_.sound("f3-ce11-c13");
    }else if(!auxiliaryDamage(kind)){
        critical=host_.session().flag(47)||(count%3==0&&host_.random(0,100)-count<=mainCriticalThreshold(kind));
        host_.session().setFlag(47,false);
        if(critical){amount*=2;host_.session().setVariable(35,0);host_.session().setFlag(12,true);}
    }
    host_.session().setVariable(34,amount);damage_+=amount;
    if(rules().value("source_cache",false)){host_.session().setVariable(rules().at("entry_state_variable").get<int>()-2,damage_);}
    if(receiver>=4){change(State::Down);play(receiver,6);}
    else if(receiver==3){change(critical&&rules().at("receivers").at("3").value("next_state_on_critical",0)==100?State::Down:State::Pursuit);play(3,4);}
    else{if(critical&&rules().at("receivers").at("2").value("next_state_on_critical",0)==100){change(State::Down);}play(2,5);}
    const auto& profile=rules().at("runtime_profile");
    if(profile.contains("receiver_sounds")){
        const auto& sound=profile.at("receiver_sounds").at(std::to_string(receiver)).at(critical?"critical":"ordinary");
        if(!sound.is_null()){host_.cue(sound.get<std::string>());}
    }else if(receiver==6){play(6,critical?13:18);}else if(receiver>=3||!critical){play(receiver,receiver==2?19:20);}
    const std::string blood=profile.contains("receiver_blood")?std::to_string(profile.at("receiver_blood").at(std::to_string(receiver)).get<int>()):receiver==6?"13":receiver>=4?"12":"11";
    host_.present({PresentationEvent::Kind::Effect,profile.at("enemy_blood_effects").at(blood),{},position,0,id()});
    if(damage_>=definition_.terminationDamage){
        if(holdsPlayer()||pendingGrab_){release(true);}if(receiver!=6){host_.cancelRoutes();}applyPage("ending");begin(Action::Ending);
    }
    return {true,true,amount,rolled.endRequested};
}
void LickerBehavior::updateParallel(){
    if(retired_){return;}
    const int damage=host_.advanceBurn(rules().value("source_cache",false)?std::optional<GridPosition>{body_}:std::nullopt);
    if(damage&&active_&&!dead_&&action_!=Action::Ending){damage_+=damage;if(rules().value("source_cache",false)){host_.session().setVariable(rules().at("entry_state_variable").get<int>()-2,damage_);}if(damage_>=definition_.terminationDamage){if(holdsPlayer()||pendingGrab_){release(true);}host_.cancelRoutes();applyPage("ending");begin(Action::Ending);}}
}

int LickerBehavior::actionAttack(const std::string& role) const{return rules().at("damage_sources").at(role).value("attack",definition_.attack);}
bool LickerBehavior::cueCondition(const nlohmann::json& condition) const{return condition.contains("if_fact")?host_.session().flag(condition.at("if_fact")):!host_.session().flag(condition.at("unless_fact"));}
void LickerBehavior::sampleBody(){
    if(!rules().value("source_cache",false)||dead_){return;}
    if(static_cast<int>(state_)>=7&&static_cast<int>(state_)<=9){body_={};return;}
    body_=host_.actorScreenFoot();body_.y-=8;
}
int LickerBehavior::terrain(TilePosition tile) const{
    if(rules().value("wall_search_same_update",false)){const auto& map=host_.content().maps.at(host_.map());if(tile.x<0||tile.y<0||tile.x>=map.width||tile.y>=map.height){return 3;}}
    return host_.terrain(tile);
}
void LickerBehavior::retire(){if(retired_){return;}release(true);host_.cancelRoutes();retired_=true;}
