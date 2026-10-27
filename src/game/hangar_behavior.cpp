#include "hangar_behavior.h"
#include "hangar_gas_rules.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
using Json=nlohmann::json;
HangarBehavior::HangarBehavior(CombatScene& scene,const EnemyDefinition& d):scene_(scene),definition_(d),host_(scene,d.id){
    const auto& r=d.rules;for(const auto* key:{"actions","body","gases","receiver_defense","attack","completion_flow"}){(void)r.at(key);}
    if(r.at("gases").size()!=3){throw std::runtime_error("Hangar requires three source gas slots");}
    actor32Hp_=definition_.rules.at("actor32_initial_hp");damage_=host_.session().variable(89);
}
int HangarBehavior::level() const{return host_.session().difficultyLevel();}
bool HangarBehavior::valid() const{return host_.session().flag(definition_.rules.at("activation_fact"))&&!host_.session().flag(definition_.rules.at("stop_fact"))&&host_.session().hp()>0&&!host_.session().flag(15);}
void HangarBehavior::picture(const std::string& id,GridPosition screen){
    const auto foot=host_.actorScreenFoot(),pos=host_.actor().motion.position();
    const GridPosition origin{(pos.x+.5)*16-foot.x,(pos.y+1)*16-foot.y};
    PresentationEvent event{PresentationEvent::Kind::Cue,id,{},host_.screenPicturePosition(screen),0,host_.id(),screen};event.pictureMapOrigin=origin;host_.present(std::move(event));
}
void HangarBehavior::cue(const std::string& id){
    const auto& c=host_.content().cue(id);const std::string k=c.at("kind");
    if(k=="sound"){host_.sound(id);}
    else if(k=="forced_route"){host_.startRoute(id);}
    else{host_.present({PresentationEvent::Kind::Cue,id,{},host_.actor().motion.position(),0,host_.id()});}
}
void HangarBehavior::stopGas(std::size_t n){
    auto& g=gases_[n];const auto& d=definition_.rules.at("gases").at(n);
    if(g.active){picture(d.at("erase"),g.screen);}
    g={};host_.session().setFlag(d.at("active_fact"),false);host_.session().setFlag(d.at("cancel_fact"),false);
    EnemyHost gasHost(scene_,d.at("object"));gasHost.claimAutonomy();gasHost.cancelRoutes();
}
void HangarBehavior::cancelAction(){
    host_.cancelRoutes();autonomy_.reset();autonomyPage_=0;action_=0;cursor_=0;wait_=0;arcStep_=arcHeight_=0;
    auto& s=host_.session();s.setVariable(definition_.rules.at("selector_variable"),0);s.setVariable(206,0);s.setVariable(202,0);s.setVariable(205,0);host_.session().setFlag(21,false);
}
void HangarBehavior::cancel(bool clearBody){
    passiveGasWait_=0;cancelAction();for(std::size_t n=0;n<gases_.size();++n){stopGas(n);}
    if(clearBody){
        host_.session().setFlag(22,false);host_.session().setBloodPictureProgress({});host_.session().setFlag(37,false);host_.session().finishBurnWait(24);
        picture(definition_.rules.at("runtime_profile").at("burn").at("erase_cue"),{});
        for(const auto& [type,row]:definition_.rules.at("runtime_profile").at("blood").at("types").items()){picture(row.at("erase_cue"),{});}
    }
    if(clearBody&&bodyShown_){
        for(int p=1;p<=7;++p){const std::string id="g0-hangar-clear-"+std::to_string(p);picture(id,{});}
        bodyShown_=false;bodyPage_=0;
    }
    cleaned_=true;
}
void HangarBehavior::sampleBody(){
    auto& s=host_.session();const auto& sampler=host_.content().objects.at(definition_.rules.at("sampler_object").get<std::string>());
    const auto* page=host_.content().activeVariant(sampler,s);
    if(!page||page->definition.trigger!=4){return;}
    const auto tile=host_.actor().motion.tile();const auto foot=host_.actorScreenFoot();const auto& offset=definition_.rules.at("body_offset");
    s.setVariable(81,tile.x);s.setVariable(82,tile.y);s.setVariable(83,int(foot.x)+offset[0].get<int>());s.setVariable(84,int(foot.y)+offset[1].get<int>());
    s.setVariable(87,12);s.setVariable(88,16);
}
void HangarBehavior::advanceBody(){
    if(!host_.pictureCommandsAllowed()){return;}
    auto& s=host_.session();host_.sampleSourceDirection();const auto a=host_.actor();const auto& object=host_.content().objects.at(definition_.rules.at("body_object").get<std::string>());
    const auto* page=host_.content().activeVariant(object,s);if(!page||page->page>=5){return;}
    const int p=page->page;const auto direction=a.motion.facing();const auto& d=definition_.rules.at("body").at(std::to_string(p));
    const auto foot=host_.actorScreenFoot();GridPosition screen{foot.x,foot.y-23};
    if(!bodyShown_||p!=bodyPage_||direction!=bodyDirection_){
        for(int n=1;n<=7;++n){picture("g0-hangar-clear-"+std::to_string(n),{});}
        for(const auto& id:d.at("show").at(std::string(directionName(direction)))){picture(id,screen);}
        bodyShown_=true;bodyPage_=p;bodyDirection_=direction;
    }
    int visible=1;
    if(p==1){
        int pulse=s.variable(205),counter=s.variable(206)+1;
        if(a.motion.tile()!=lastBodyTile_){pulse+=32;lastBodyTile_=a.motion.tile();}
        visible=pulse>0?(counter>=56?3:counter>=36?1:counter>=16?2:1):1;
        if(pulse>0&&counter>=76){counter=0;}
        s.setVariable(206,counter);s.setVariable(205,std::max(0,pulse-1));
    }else if(p==2){visible=s.variable(206)+1;}
    else if(p==3){visible=s.variable(206)==0?1:s.variable(206)==1?3:4;}
    else{visible=s.variable(206)+1;}
    for(const auto& [slot,id]:d.at("move").items()){
        // Move cues retain source tone/zoom; selected picture gets its original visible transparency.
        const std::string key="g0-hangar-frame-"+std::to_string(p)+"-"+slot+((std::stoi(slot)==visible||(p==3&&slot=="2"&&s.variable(206)==1))?"-show":"-hide");
        auto position=screen;if(p==3&&std::stoi(slot)==2){position.y=foot.y;}if(p==3&&std::stoi(slot)==3){position.y=s.variable(84);}
        picture(key,position);
    }
}
void HangarBehavior::updateParallel(){
    if(!valid()){
        if(!cleaned_){cancel(true);}mode_=host_.session().flag(350)?EnemyMode::Corpse:EnemyMode::Dormant;return;
    }
    cleaned_=false;
    // Original gas/body parallel work runs before E13; E26's sampling runs after E13 below.
    host_.advanceBlood(definition_.rules.at("runtime_profile").at("blood"),host_.session().variable(92));
    const int burn=host_.advanceBurn(GridPosition{double(host_.session().variable(83)),double(host_.session().variable(84))});
    if(burn){damage_+=burn;host_.session().setVariable(89,damage_);}
    advanceGases();advanceBody();
}
void HangarBehavior::beginAction(int choice){
    if(choice<1||choice>5){throw std::runtime_error("Unknown hangar source action");}
    host_.cancelRoutes();autonomy_.reset();action_=choice;cursor_=0;wait_=0;arcStep_=arcHeight_=0;mode_=EnemyMode::Windup;
    const auto& obj=host_.content().objects.at(id());const auto* page=host_.content().activeVariant(obj,host_.session());
    if(page){auto a=host_.actor();a.setSpeedLevel(page->definition.moveSpeed);a.layer=page->definition.layer;host_.setActor(a);}
}
void HangarBehavior::strike(const Json& d){
    auto& s=host_.session();const auto facing=host_.actor().motion.facing();const auto foot=host_.actorScreenFoot();
    const auto& b=d.at("bounds").at(std::string(directionName(facing)));const auto player=host_.sampleActorScreen("player");
    const int dx=int(foot.x)+b.at("offset")[0].get<int>()-int(player.x),dy=int(foot.y)+b.at("offset")[1].get<int>()-int(player.y);
    s.setVariable(85,dx);s.setVariable(86,dy);
    if(dx<b.at("x")[0].get<int>()||dx>b.at("x")[1].get<int>()||dy<b.at("y")[0].get<int>()||dy>b.at("y")[1].get<int>()||s.flag(13)){return;}
    const auto& extra=d.at("extra_attack");const int attack=definition_.rules.at("attack").at(level()-1).get<int>()+host_.random(extra[0],extra[1]);
    host_.sampleSourceDirection();s.setVariable(24,2);s.setVariable(23,attack);s.setFlag(17,true);
    host_.requestSourceReaction(d.at("reaction_page"));host_.sound("m120-e13-p13-c83");
    host_.damagePlayer(biteDamage(attack,s.defense(),host_.random(-5,2)));s.setFlag(17,false);
}
void HangarBehavior::advanceAction(){
    if(wait_>0&&--wait_>0){return;}
    const auto& steps=definition_.rules.at("actions").at(std::to_string(action_));
    while(cursor_<steps.size()){
        const auto& step=steps.at(cursor_);const std::string k=step.at("kind");
        if(k=="jump_arc"){
            const auto foot=host_.actorScreenFoot();auto& s=host_.session();s.setVariable(83,int(foot.x));s.setVariable(84,int(foot.y)-23-arcHeight_);
            ++arcStep_;int delta=-4;for(const auto& segment:step.at("segments")){if(arcStep_<segment[0].get<int>()){delta=segment[1];break;}}
            arcHeight_+=delta;host_.session().setVariable(86,arcHeight_);
            if(delta<0&&arcHeight_<=step.at("stop_at").get<int>()){arcHeight_=step.at("stop_at");++cursor_;}
            wait_=1;return;
        }
        ++cursor_;
        if(k=="pose"){host_.session().setVariable(206,step.at("value"));}
        else if(k=="cue"){cue(step.at("cue"));}
        else if(k=="wait"){wait_=std::max(1,step.at("tenths").get<int>()*6);return;}
        else if(k=="strike"){strike(step);}
        else if(k=="active_gases"){releaseGases();}
        else{throw std::runtime_error("Unknown hangar action stage");}
    }
    cancelAction();mode_=EnemyMode::Pursuit;
}
void HangarBehavior::advanceAutonomy(){
    auto& s=host_.session();const auto& obj=host_.content().objects.at(id());const auto* p=host_.content().activeVariant(obj,s);if(!p){return;}
    if(autonomyPage_!=p->page){
        auto a=host_.actor();a.setSpeedLevel(p->definition.moveSpeed);a.layer=p->definition.layer;host_.setActor(a);
        autonomyPage_=p->page;autonomy_.reset();if(!p->autonomousRoute.steps.empty()){autonomy_.emplace(p->autonomousRoute,id());}
    }
    if(autonomy_&&!host_.routePending()){host_.advance(*autonomy_);}
    if(p->page!=10&&p->page!=11){return;}
    s.setFlag(21,false); // P10/11 open with S21 OFF, and only a selected attack sets it ON.
    if(p->page==11){if(!s.flag(37)){cue("m120-e13-p11-c3");}s.setVariable(87,8);s.setVariable(88,15);}
    const auto a=host_.actor().motion.tile(),b=host_.player().motion.tile();const auto facing=host_.actor().motion.facing();
    const int dx=b.x-a.x,dy=b.y-a.y;
    const auto& regions=definition_.rules.at("selector_regions").at(std::string(directionName(facing)));
    const auto inside=[&](const Json& cells){return std::any_of(cells.begin(),cells.end(),[&](const auto& cell){return cell[0]==dx&&cell[1]==dy;});};
    int selected=0;
    if(p->page==11){
        const bool inFront=(facing==Direction::Up||facing==Direction::Down)?dx==0&&dy*(facing==Direction::Down?1:-1)>=0&&std::abs(dy)<=3:dy==0&&dx*(facing==Direction::Right?1:-1)>=0&&std::abs(dx)<=3;
        if(inFront){selected=1;}
    }else if(inside(regions.at("near"))){const int roll=host_.random(0,15);if(roll<=4){selected=1;}else if(roll<=8){selected=2;}else if(roll>=10){selected=3;}}
    else if(inside(regions.at("far"))){const int roll=host_.random(0,50);if(roll<=1){selected=4;}else if(roll<=6){auto actor=host_.actor();actor.setSpeedLevel(6);host_.setActor(actor);}}
    if(p->page==10){const int counter=s.variable(193);if(host_.random(0,50000)<=counter){selected=5;s.setVariable(193,0);}s.setVariable(193,s.variable(193)+1);}
    if(selected){s.setVariable(91,selected);s.setFlag(21,true);beginAction(selected);}
    // Source obstacle avoidance redirects to the player's cached axis only on terrain3.
    const auto next=adjacentTile(a,facing);
    if(host_.terrain(next)==3){const GridPosition body{double(s.variable(83)),double(s.variable(84))};const auto player=host_.sampleActorScreen("player");auto actor=host_.actor();
        if(facing==Direction::Up||facing==Direction::Down){if(body.x!=player.x){actor.face(body.x<player.x?Direction::Right:Direction::Left);}}
        else if(body.y!=player.y){actor.face(body.y<player.y?Direction::Down:Direction::Up);}
        host_.setActor(actor);}
}
void HangarBehavior::update(const InputFrame&){
    host_.claimAutonomy();if(!valid()){return;}
    damage_=host_.session().variable(89);
    if(damage_>=definition_.rules.at("threshold").get<int>()){
        passiveGasWait_=0;
        if(mode_!=EnemyMode::Ending){cancel(false);mode_=EnemyMode::Ending;}
        if(!submitted_){submitted_=host_.completeEncounter(definition_.rules.at("completion_flow"));}
        return;
    }
    if(mode_==EnemyMode::Dormant){mode_=EnemyMode::Pursuit;sampleBody();}
    if(action_){
        if(!host_.session().flag(21)||host_.session().variable(91)!=action_){cancelAction();mode_=EnemyMode::Pursuit;}
        else{advanceAction();}
    }else if(host_.session().flag(21)&&host_.session().variable(91)>=1&&host_.session().variable(91)<=5){beginAction(host_.session().variable(91));}
    else{advanceAutonomy();}
    sampleBody();advancePassiveGases();
}
void HangarBehavior::releaseGases(){
    auto& s=host_.session();std::array<bool,3> occupied{};for(std::size_t n=0;n<3;++n){occupied[n]=s.flag(definition_.rules.at("gases").at(n).at("active_fact"));}
    const auto requested=activeHangarGases(level(),occupied);if(!requested.callsEmitter){return;}
    emitGases(requested.slots,"m120-e5-p2-c172");
}
void HangarBehavior::advancePassiveGases(){
    if(!definition_.rules.contains("passive_gas")){return;}
    const auto& rule=definition_.rules.at("passive_gas");auto& s=host_.session();
    const auto* page=host_.content().activeVariant(host_.content().objects.at(rule.at("object").get<std::string>()),s);
    if(level()!=3||!page||page->page!=rule.at("page")){passiveGasWait_=0;return;}
    const int selector=s.variable(rule.at("selector_variable")),limit=rule.at("selector_less_than");
    if(passiveGasWait_==0){if(selector<limit){passiveGasWait_=rule.at("wait_tenths").get<int>()*6;}return;}
    if(--passiveGasWait_>0){return;}
    std::array<bool,3> occupied{};for(std::size_t n=0;n<3;++n){occupied[n]=s.flag(definition_.rules.at("gases").at(n).at("active_fact"));}
    const auto requested=passiveHangarGases(0,selector,occupied);
    if(requested.callsEmitter){emitGases(requested.slots,rule.at("flash_cue"));}
}
void HangarBehavior::emitGases(const std::array<bool,3>& requested,const std::string& flash){
    auto& s=host_.session();
    const auto a=host_.actor().motion.tile(),p=host_.player().motion.tile();
    if(p.x>=a.x-2&&p.x<=a.x+2&&p.y>=a.y-2&&p.y<=a.y+2&&!s.flag(13)){host_.neuralGasHit(flash);}
        const auto foot=host_.actorScreenFoot();
    for(std::size_t n=0;n<gases_.size();++n){
        const auto& d=definition_.rules.at("gases").at(n);if(!requested[n]){continue;}
        auto& g=gases_[n];g={};g.active=true;g.screen={foot.x+d.at("offset")[0].get<int>(),foot.y+d.at("offset")[1].get<int>()};
        g.playerTile={double(p.x),double(p.y)};g.velocity={double(d.at("velocity")[0].get<int>()),double(d.at("velocity")[1].get<int>())};
        s.setFlag(d.at("cancel_fact"),false);s.setFlag(d.at("active_fact"),true);picture(d.at("show"),g.screen);
    }
}
void HangarBehavior::advanceGases(){
    auto& s=host_.session();const auto player=host_.sampleActorScreen("player");const auto tile=host_.player().motion.tile();
    for(std::size_t n=0;n<gases_.size();++n){
        auto& g=gases_[n];const auto& d=definition_.rules.at("gases").at(n);
        if(!g.active){continue;}if(s.flag(d.at("cancel_fact"))||!s.flag(d.at("active_fact"))||damage_>=800){stopGas(n);continue;}
        if(!g.picturePending){
            const int period=d.at("tracking_period");
            if(g.age%period==0){
                if(g.screen.x<player.x){g.velocity.x=std::min(2.,g.velocity.x+1);}else if(g.screen.x>player.x){g.velocity.x=std::max(-2.,g.velocity.x-1);}
                if(g.screen.y<player.y){g.velocity.y=std::min(2.,g.velocity.y+1);}else if(g.screen.y>player.y){g.velocity.y=std::max(-2.,g.velocity.y-1);}
            }
            if(g.playerTile.x!=tile.x){g.playerTile.x=tile.x;if(player.x==152){g.cameraDrift.x=host_.player().motion.facing()==Direction::Right?-2:2;g.driftWait[0]=8;}}
            if(g.playerTile.y!=tile.y){g.playerTile.y=tile.y;if(player.y==120){g.cameraDrift.y=host_.player().motion.facing()==Direction::Down?-2:2;g.driftWait[1]=8;}}
            if(g.age%2==0){g.screen.x+=g.velocity.x;g.screen.y+=g.velocity.y;}
            if(g.driftWait[0]>0){--g.driftWait[0];g.screen.x+=g.cameraDrift.x;}
            if(g.driftWait[1]>0){--g.driftWait[1];g.screen.y+=g.cameraDrift.y;}
            if(g.age%8==0){if(g.age<=48){++g.scale;}else{--g.scale;if(g.scale<0){g.age=0;g.scale=0;}}}
        g.picturePending=true;
        }
        if(!host_.pictureCommandsAllowed()){continue;}
        picture(d.at("moves").at(std::clamp(g.scale,0,11)),g.screen);g.picturePending=false;
        ++g.age;const int base=d.at("workspace");s.setVariable(base+4,int(g.screen.x));s.setVariable(base+5,int(g.screen.y));s.setVariable(base+10,g.age);s.setVariable(base+11,g.scale);
        if(std::abs(g.screen.x-player.x)<=8&&std::abs(g.screen.y-player.y)<=8&&!s.flag(13)){
            host_.neuralGasHit(d.at("object").get<std::string>()+"-p2-c172");stopGas(n);
        }
    }
}
AttackContact HangarBehavior::receiveAttack(const AttackSample& sample){
    damage_=host_.session().variable(89);
    if(!valid()||mode_==EnemyMode::Dormant||damage_>=800){return {};}
    const auto& s=host_.session();
    const auto body=host_.screenPicturePosition({double(s.variable(83)),double(s.variable(84))});
    const GridPosition target{(body.x+.5)*16,(body.y+1)*16};
    if(sample.phase!=AttackPhase::ResolvedContact&&(std::abs(sample.center.x-target.x)>sample.halfPixels[0]+s.variable(87)||
       std::abs(sample.center.y-target.y)>sample.halfPixels[1]+s.variable(88))){return {};}
    if(sample.attackId&&processedAttacks_.contains(sample.attackId)){return {true,false,0,false};}
    if(sample.kind==AttackKind::GlFlame&&host_.burning()){return {true,false,0,false};}
    if(sample.phase==AttackPhase::Flight){if(!grenadeLauncher(sample.kind)){throw std::invalid_argument("Unsupported hangar flight");}return {true,false,0,false};}
    if(sample.phase==AttackPhase::Burning){if(sample.kind!=AttackKind::GlFlame){throw std::invalid_argument("Unsupported hangar burning");}host_.startBurn();if(sample.attackId){processedAttacks_.insert(sample.attackId);}return {true,true,0,false};}
    if(sample.receiverPage==1){cancelAction();const auto facing=host_.actor().motion.facing();cue(definition_.rules.at("retreat").at(std::string(directionName(facing))));if(sample.attackId){processedAttacks_.insert(sample.attackId);}return {true,true,0,false};}
    if(sample.receiverPage<2||sample.receiverPage>6){return {true,false,0,false};}
    const auto kind=host_.damageKind(sample.kind);if(kind==AttackKind::GlFlame){host_.startBurn();}auto& session=host_.session();const int page=sample.receiverPage;
    session.setFlag(12,false);const int count=session.variable(35)+1;session.setVariable(35,count);
    const int attack=auxiliaryDamage(kind)?session.auxiliaryAttack():session.attack()+(sample.aimed?host_.content().items.at(host_.content().rules.at("handgun_damage").at("aim_attack_item")).attack:0);
    const int defense=definition_.rules.at("receiver_defense").at(std::to_string(page)).at(level()-1);session.setVariable(49,defense);
    const auto rolled=rollWeaponDamage(kind,attack,defense,sample.advances,[&](int a,int b){return host_.random(a,b);});int amount=rolled.amount;
    if(kind==AttackKind::Knife){if(count%3==0&&host_.random(0,100)-count<=15){amount=session.auxiliaryAttack()+host_.random(0,10);}}
    else if(!auxiliaryDamage(kind)){
        const bool critical=session.flag(47)||(count%3==0&&host_.random(0,100)-count<=mainCriticalThreshold(kind));session.setFlag(47,false);
        if(critical){session.setVariable(35,0);session.setFlag(12,true);amount*=2;}
    }
    const auto& sounds=definition_.rules.at("cues").at(std::to_string(page));
    for(const auto& [n,id]:sounds.items()){if(host_.content().cue(id.get<std::string>()).at("kind")=="sound"&&
       (n=="5"||n=="6"||(page==5&&n=="12")||(session.flag(12)&&(n=="11"||n=="12")))){cue(id);}}
    if(page>=3&&page<=5){cancelAction();session.setFlag(16,true);mode_=EnemyMode::HitRecovery;}
    if(page==5){cue(sounds.at("4"));}
    else if(session.flag(12)&&page<=4){cue(sounds.at(page==3?"13":"12"));}
    session.setVariable(34,amount);session.setVariable(92,page<=3?1:page==4?2:3);session.setFlag(22,true);
    // Original HP mutations are nonlethal and independent of V89; P3 critical writes actor32.
    const int actorTarget=page==3&&session.flag(12)?32:30;
    if(actorTarget==30){session.damagePursuer(amount);}else{actor32Hp_=std::max(1,actor32Hp_-amount);}
    damage_+=amount;session.setVariable(89,damage_);if(sample.attackId){processedAttacks_.insert(sample.attackId);}
    if(damage_>=800){cancel(false);mode_=EnemyMode::Ending;}
    return {true,true,amount,rolled.endRequested};
}
