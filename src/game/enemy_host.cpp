#include "enemy_host.h"
#include "combat_scene.h"
#include "weapon_controller.h"
#include "enemy_controller.h"
#include <algorithm>
#include <stdexcept>
EnemyHost::EnemyHost(CombatScene& scene,std::string actor):scene_(scene),id_(std::move(actor)){}
const CombatContent& EnemyHost::content() const{return scene_.content_;}
GameSession& EnemyHost::session() const{return scene_.session_;}
int EnemyHost::map() const{return scene_.map_;}
CharacterState EnemyHost::actor() const{return findCharacter(*scene_.world_,id_);}
TilePosition EnemyHost::targetTile(const std::string& actor) const{return findCharacter(*scene_.world_,actor).motion.tile();}
CharacterState EnemyHost::player() const{return scene_.world_->player();}
GridPosition EnemyHost::actorScreenFoot() const{const auto character=actor();const auto position=character.motion.position(),camera=scene_.camera_.center();return {(position.x+.5)*scene_.definition_.tileSize-camera.x+160,(position.y+1)*scene_.definition_.tileSize-camera.y+120-character.motion.jumpHeight()};}
std::optional<Direction> EnemyHost::sourceDirection() const{
    switch(session().variable(201)){
    case 2:return Direction::Down;case 4:return Direction::Left;
    case 6:return Direction::Right;case 8:return Direction::Up;default:return std::nullopt;
    }
}
void EnemyHost::sampleSourceDirection(){
    switch(actor().motion.facing()){
    case Direction::Down:session().setVariable(201,2);break;case Direction::Left:session().setVariable(201,4);break;
    case Direction::Right:session().setVariable(201,6);break;case Direction::Up:session().setVariable(201,8);break;
    }
}
bool EnemyHost::pictureCommandsAllowed() const{return scene_.dialogue_.view().phase==DialoguePhase::Hidden&&scene_.inputPrompt_.view().phase==DialoguePhase::Hidden;}
GridPosition EnemyHost::screenPicturePosition(GridPosition sampledScreen) const{
    const auto camera=scene_.camera_.center();
    return {(sampledScreen.x+camera.x-160)/scene_.definition_.tileSize-.5,(sampledScreen.y+camera.y-120)/scene_.definition_.tileSize-1};
}
GridPosition EnemyHost::pictureWorkspacePosition(int commonEvent) const{
    (void)session().burnWaitRemaining(commonEvent);const int variable=93+14*(commonEvent-24);
    return screenPicturePosition({static_cast<double>(session().variable(variable)),static_cast<double>(session().variable(variable+1))});
}
void EnemyHost::setActor(const CharacterState& actor){if(actor.id!=id_){throw std::logic_error("Enemy changed another actor");}replaceCharacter(*scene_.world_,actor);}
void EnemyHost::setPlayer(const CharacterState& player){if(!holdsPlayer()){throw std::logic_error("Enemy does not own player control");}scene_.world_->setPlayerState(player);}
AttackKind EnemyHost::damageKind(AttackKind launched) const{
    if(launched!=AttackKind::Sg&&launched!=AttackKind::Lc9&&launched!=AttackKind::Grenade){return launched;}
    return equippedDamageKind(scene_.weapon_->auxiliaryRaised());
}
AttackKind EnemyHost::equippedDamageKind(bool auxiliary) const{
    const int item=session().equipped(auxiliary?2:1);
    if(content().catalog.contains("weapon_bindings")&&content().catalog.at("weapon_bindings").contains(std::to_string(item))){
        const auto& binding=content().catalog.at("weapon_bindings").at(std::to_string(item));
        const std::string kind=binding.at("kind");
        if(kind=="shotgun"){return binding.value("damage_kind",std::string{})=="semi-auto-shotgun"?AttackKind::SemiAutoShotgun:AttackKind::Shotgun;}
        if(kind=="handgun-hp"){return AttackKind::HandgunHp;}
        if(kind=="rocket"){return AttackKind::Rocket;}
        if(kind=="knife"){return AttackKind::Knife;}
        if(kind=="saa"){return AttackKind::Saa;}
        if(kind=="de"){return AttackKind::De;}
        if(kind=="ar"){return AttackKind::Ar;}
        if(kind=="gl"){
            static const std::map<std::string,AttackKind> kinds{{"normal",AttackKind::GlNormal},{"flame",AttackKind::GlFlame},{"acid",AttackKind::GlAcid}};
            return kinds.at(binding.at("ammunition").get<std::string>());
        }
    }
    if(auxiliary){return item==24||item==25?AttackKind::Knife:item==27?AttackKind::Lc9:AttackKind::Grenade;}
    return item==12?AttackKind::Sg:item==14?AttackKind::Shotgun:item==20?AttackKind::Magnum:item==21?AttackKind::De:AttackKind::Handgun;
}
const nlohmann::json* EnemyHost::burnRules() const{
    const auto& full=*content().full;
    if(!full.contains("runtime")){return nullptr;}
    const auto& runtime=full.at("runtime");
    if(runtime.contains("combat_effects")){return &runtime.at("combat_effects");}
    return runtime.contains("weapon_actions")?&runtime.at("weapon_actions"):nullptr;
}
void EnemyHost::startBurn(){
    const auto* effects=burnRules();if(!effects){throw std::logic_error("Missing burn source rules");}
    const auto& rules=*effects;
    const auto& burn=rules.at("burns").at(std::to_string(content().enemyBurnSources().at(id_).get<int>()));
    if(session().flag(burn.at("activation_fact"))){return;}
    session().setFlag(burn.at("activation_fact"),true);
}
bool EnemyHost::burning() const{
    const auto* effects=burnRules();if(!effects){return false;}const auto& rules=*effects;
    return session().flag(rules.at("burns").at(std::to_string(content().enemyBurnSources().at(id_).get<int>())).at("activation_fact"));
}
int EnemyHost::advanceBurn(std::optional<GridPosition> sampledBody){
    const auto* effects=burnRules();if(!effects){return 0;}const auto& rules=*effects;
    if(!content().enemyBurnSources().contains(id_)){return 0;}
    const auto& burn=rules.at("burns").at(std::to_string(content().enemyBurnSources().at(id_).get<int>()));
    auto& s=session();if(!s.flag(burn.at("activation_fact"))){return 0;}
    const int ce=burn.at("common_event"),counter=burn.at("counter_source_variable");
    auto stage=s.burnPictureStage(ce);int damage=0;
    const auto setStage=[&](BurnPictureStage next){stage=next;s.setBurnPictureStage(ce,next);};
    const auto emit=[&](const std::string& cue,GridPosition position){
        present({PresentationEvent::Kind::Cue,cue,{},screenPicturePosition(position),0,id_,position});
    };
    const auto sample=[&]{if(sampledBody){return *sampledBody;}auto p=actorScreenFoot();const auto& offset=burn.at("body_offset_from_worldfoot_pixels");p.x+=offset.at(0).get<int>();p.y+=offset.at(1).get<int>();return p;};
    const auto finish=[&]{s.setVariable(counter,0);setStage(BurnPictureStage::Erase);};
    const auto sampleFrame=[&](int frame){
        const auto body=sample();if(burn.at("stop_if_both_body_screen_nonpositive").get<bool>()&&body.x<=0&&body.y<=0){finish();return;}
        const auto& offset=burn.at("frames").at(frame).at("offset_from_body_pixels");
        const auto& variables=burn.at("initial_show_source_variables");
        s.setVariable(variables.at(0),static_cast<int>(body.x)+offset.at(0).get<int>());
        s.setVariable(variables.at(1),static_cast<int>(body.y)+offset.at(1).get<int>());
    };
    if(stage==BurnPictureStage::InitialShow){
        if(!pictureCommandsAllowed()){return 0;}const auto& variables=burn.at("initial_show_source_variables");
        emit(burn.at("show_cue"),{double(s.variable(variables.at(0))),double(s.variable(variables.at(1)))});setStage(BurnPictureStage::MoveFirst);sampleFrame(0);
    }else if(stage==BurnPictureStage::Waiting){
        if(s.advanceBurnWait(ce,true)){
            damage=burn.at("damage");const auto& increment=burn.at("counter_increment");
            const int next=s.variable(counter)+random(increment.at(0),increment.at(1));
            if(next>=burn.at("lifetime_threshold").get<int>()){finish();}
            else{s.setVariable(counter,next);setStage(BurnPictureStage::MoveFirst);sampleFrame(0);}
        }else{
            const int remaining=s.burnWaitRemaining(ce);
            if(remaining==4){setStage(BurnPictureStage::MoveSecond);sampleFrame(1);}
            else if(remaining==2){setStage(BurnPictureStage::MoveThird);sampleFrame(2);}
            else{return 0;}
        }
    }
    if(!pictureCommandsAllowed()){return damage;}
    if(stage==BurnPictureStage::Erase){emit(burn.at("erase_cue"),{0,0});s.setFlag(burn.at("activation_fact"),false);s.finishBurnWait(ce);return damage;}
    const int frame=stage==BurnPictureStage::MoveFirst?0:stage==BurnPictureStage::MoveSecond?1:2;const auto body=sample();const auto& definition=burn.at("frames").at(frame);
    const auto& variables=burn.at("initial_show_source_variables");const GridPosition position{double(s.variable(variables.at(0))),double(s.variable(variables.at(1)))};
    emit(definition.at("move_cue"),position);
    if(body.y<=burn.at("hide_if_body_screen_y_at_most").get<int>()){emit(definition.at("hide_cues").at(0),position);}
    else if(body.y>=burn.at("hide_if_body_screen_y_at_least").get<int>()){emit(definition.at("hide_cues").at(1),position);}
    setStage(BurnPictureStage::Waiting);if(s.burnWaitRemaining(ce)==0){s.advanceBurnWait(ce,true);}return damage;
}
int EnemyHost::random(int low,int high) const{return scene_.random_(low,high);}
int EnemyHost::terrain(TilePosition p) const{
    if(p.x<0||p.y<0||p.x>=scene_.definition_.width||p.y>=scene_.definition_.height){return 0;}
    return scene_.world_->mappedLowerTerrain(p).value_or(scene_.content_.terrainIds.at(map()).at(p.y*scene_.definition_.width+p.x));
}
void EnemyHost::claimAutonomy(){std::erase_if(scene_.ambientRoutes_,[&](const auto& route){return route.actor()==id_;});}
bool EnemyHost::advance(RoutePlayer& route,const std::function<void(int,bool)>& setSwitch){if(route.actor()!=id_){throw std::logic_error("Enemy route identity mismatch");}return route.tick(*scene_.world_,scene_.session_,scene_.presentation_,scene_.random_,setSwitch);}
void EnemyHost::startRoute(const std::string& cue,bool player){
    const auto& route=content().routes.at(cue);
    if(player){
        if(route.target!=10001||(!holdsPlayer()&&!recoveryPermit_)){throw std::logic_error("Enemy cannot route the player");}
    }else if(route.target!=10005&&route.target!=actor().eventId){throw std::logic_error("Enemy cannot route another actor");}
    scene_.startRoute(cue,id_);if(player&&!holdsPlayer()){recoveryPermit_=false;}
}
void EnemyHost::route(RouteDefinition definition){cancelRoutes();scene_.routes_.emplace_back(std::move(definition),id_);}
void EnemyHost::cancelRoutes(bool player){if(player&&!holdsPlayer()){throw std::logic_error("Enemy cannot cancel player routes");}std::erase_if(scene_.routes_,[&](const auto& r){return r.actor()==id_||(player&&r.actor()=="player");});}
bool EnemyHost::routePending(const std::string& source) const{return std::any_of(scene_.routes_.begin(),scene_.routes_.end(),[&](const auto& r){return r.actor()==id_&&(source.empty()||r.source()==source);});}
void EnemyHost::sound(const std::string& cue){scene_.playSound(cue);}
void EnemyHost::cue(const std::string& cue,bool actor){
    const auto& value=content().cue(cue);const std::string kind=value.at("kind");
    if(kind=="forced_route"){startRoute(cue,content().routes.at(cue).target==10001);return;}
    if(kind=="cancel_routes"){scene_.cue(cue);return;}
    if(kind=="actor_graphic"||kind=="message_options"||kind=="camera"){
        if(!holdsPlayer()&&!recoveryPermit_){throw std::logic_error("Enemy does not own player presentation");}
    }else if(kind!="sound"&&kind!="bgm"&&kind!="fade_bgm"&&kind!="animation"&&kind!="flash_character"){
        throw std::logic_error("Unsupported enemy presentation operation: "+kind);
    }
    scene_.cue(cue,actor?id_:std::string{});
}
void EnemyHost::present(PresentationEvent event){scene_.presentation_.push_back(std::move(event));}
int EnemyHost::bloodType(int writeSlot,int readSlot,int value){
    if(!supportsEnemyBloodSlot(content(),writeSlot)||!supportsEnemyBloodSlot(content(),readSlot)){throw std::logic_error("Invalid enemy blood slot for "+id_+": write "+std::to_string(writeSlot)+", read "+std::to_string(readSlot));}
    scene_.enemyBloodTypes_[writeSlot]=value;return scene_.enemyBloodTypes_[readSlot];
}
void EnemyHost::damagePlayer(int amount){scene_.applyPlayerDamage(amount);}
bool EnemyHost::receiveSourceDamage(int attack){
    auto& s=session();if(s.flag(13)){return false;}
    s.setVariable(40,s.variable(40)/2);
    const int amount=std::max(1,attack-s.variable(40)+random(-5,2));s.setVariable(23,amount);
    damagePlayer(amount);s.setVariable(9,s.hp());if(s.hp()==0){s.setFlag(15,true);}return true;
}
AttackContact EnemyHost::receivePartDamage(const AttackSample& sample,int defense){
    auto& s=session();s.setVariable(49,defense);s.setFlag(12,false);
    const int count=s.variable(35)+1;s.setVariable(35,count);
    const int attack=auxiliaryDamage(sample.kind)?s.auxiliaryAttack():s.attack()+(sample.aimed?content().items.at(content().rules.at("handgun_damage").at("aim_attack_item")).attack:0);
    const auto rolled=rollWeaponDamage(sample.kind,attack,defense,sample.advances,[&](int low,int high){return random(low,high);});int amount=rolled.amount;
    if(sample.kind==AttackKind::Knife){if(count%3==0&&random(0,100)-count<=15){amount=s.auxiliaryAttack()+random(0,10);}sound("f3-ce11-c13");}
    else if(!auxiliaryDamage(sample.kind)){
        const bool critical=s.flag(47)||(count%3==0&&random(0,100)-count<=mainCriticalThreshold(sample.kind));s.setFlag(47,false);
        if(critical){s.setVariable(35,0);s.setFlag(12,true);}
    }
    if(s.flag(12)){amount*=2;}s.setVariable(34,amount);return {true,true,amount,rolled.endRequested};
}
bool EnemyHost::requestPlayerReaction(int receiverPage){
    if(!scene_.active_||session().hp()<=0||session().flag(15)||session().flag(13)||session().flag(45)||
       scene_.grabOwner_.has_value()||scene_.foreground_!=ForegroundKind::None){return false;}
    return applyPlayerReaction(receiverPage);
}
bool EnemyHost::requestSourceReaction(int receiverPage){
    if(!scene_.active_||session().flag(13)||session().flag(45)){return false;}
    return applyPlayerReaction(receiverPage);
}
bool EnemyHost::requestTargetReaction(const std::string& target){
    const auto& contact=content().full->at("runtime").at("training_target_contacts").at(target);
    const auto& object=content().objects.at(target);
    const auto* page=content().activeVariant(object,session());
    if(!scene_.active_||object.map!=map()||!page||page->page!=contact.at("page")||page->definition.trigger!=4||
       contact.at("response_object")!=id_||targetTile(target)!=player().motion.tile()||
       std::any_of(contact.at("guards_off").begin(),contact.at("guards_off").end(),[&](const auto& fact){return session().flag(fact);})){return false;}
    scene_.playSound(contact.at("sound"));scene_.cue(contact.at("flash"),target);
    return applyPlayerReaction(contact.at("response_page"));
}
bool EnemyHost::requestSpiderReaction(Direction direction){
    const auto& enemy=content().enemies.at(id_);
    if(enemy.rules.value("behavior",std::string{})!="spider"){throw std::logic_error("Spider reaction requires its source owner");}
    if(!scene_.active_||session().flag(13)||session().flag(45)){return false;}
    const auto& hit=enemy.rules.at("runtime_profile").at("swoop").at("player_hit");
    session().setVariable(31,hit.at("pose"));recoveryPermit_=true;
    cue(hit.at("pose_cues").at(0),false);startRoute(hit.at("pose_cues").at(1),true);
    recoveryPermit_=true;startRoute(hit.at("player_routes").at(std::string(directionName(direction))),true);
    return true;
}
bool EnemyHost::applyPlayerReaction(int receiverPage){
    const auto& profile=content().enemies.at(id_).rules.at("runtime_profile").at("side_contacts");
    const auto& page=profile.at("pages").at(std::to_string(receiverPage));
    const auto direction=sourceDirection();
    const std::string route=direction?page.at("routes").at(std::string(directionName(*direction))).get<std::string>():std::string{};
    const std::string graphic=profile.at("restore_graphic_cue"),restore=profile.at("restore_route_cue");
    if(!content().full->contains("runtime")||!content().full->at("runtime").contains("player_recovery")||
       profile.at("player_actor")!=1||(!route.empty()&&content().routes.at(route).target!=10001)||content().routes.at(restore).target!=10001||
       content().cue(graphic).at("kind")!="actor_graphic"||profile.at("wait_for_route_completion").get<bool>()){
        throw std::logic_error("Unsupported player reaction definition");
    }
    const std::string sound=profile.at("hurt_sound_cue");(void)content().cue(sound);
    session().setFlag(profile.at("clear_player_control_fact"),false);
    if(!session().flag(profile.at("hurt_sound_suppression_fact"))){scene_.playSound(sound);}
    session().setVariable(31,profile.at("restore_mode"));scene_.cue(graphic);scene_.startRoute(restore,id_);if(!route.empty()){
        scene_.startRoute(route,id_);
        // Native player routes start after the calling map event, before CE36 can replace them next update.
        scene_.routes_.back().tick(*scene_.world_,session(),scene_.presentation_,scene_.random_);
    }
    session().setFlag(profile.at("approach_fact_after_route_submit"),true);return true;
}
void EnemyHost::requestAttackEnd(std::uint64_t id){scene_.weapon_->requestAttackEnd(id);}
void EnemyHost::sourceJumpHit(const std::string& playerRoute){
    scene_.cancelPlayerActions("ce5",id_);recoveryPermit_=true;startRoute(playerRoute,true);
}
void EnemyHost::sourceFatalHit(){scene_.startDeath(true);}
void EnemyHost::cancelWeapon(){scene_.weapon_->cancel(ActionCancel::Grabbed);}
bool EnemyHost::holdsPlayer() const{return scene_.grabOwner_==id_;}
void EnemyHost::requestGrab(){if(std::find(scene_.grabQueue_.begin(),scene_.grabQueue_.end(),id_)==scene_.grabQueue_.end()){scene_.grabQueue_.push_back(id_);}}
void EnemyHost::releaseGrab(){recoveryPermit_=holdsPlayer();scene_.releaseGrabOwner(id_);}
void EnemyHost::yieldBehind(){scene_.yieldBehind(id_);}
void EnemyHost::storyEnding(int variant){scene_.storyEnding_=variant;}
bool EnemyHost::completeEncounter(const std::string& flow){
    if(!scene_.active_||session().hp()<=0||scene_.foreground_!=ForegroundKind::None){return false;}
    const auto& rules=content().enemies.at(id_).rules;
    const auto& definition=content().catalog.at("flows").at(flow);
    if(rules.value("completion_flow",std::string{})!=flow||definition.at("object")!=id_){throw std::logic_error("Enemy completion identity mismatch");}
    if(!definition.value("runtime_enabled",true)){return false;}
    scene_.startContentFlow(flow);return true;
}

bool EnemyHost::playerRoutePending() const{return std::any_of(scene_.routes_.begin(),scene_.routes_.end(),[](const auto& r){return r.actor()=="player";});}
bool EnemyHost::hasTerrainAtDistance(int distance) const{return distance<std::max(scene_.definition_.width,scene_.definition_.height);}
void EnemyHost::fail(const std::string& flow){scene_.startFailure(flow,id_);}

void EnemyHost::reappearanceCue(const std::string& flow,const std::string& cue){
    const auto& special=content().enemies.at(id_).rules.at("runtime_profile").at("reappear").at("special");
    bool allowed=special.value("flow",std::string{})==flow;
    if(special.contains("sequences")){for(const auto& step:special.at("sequences")){allowed|=step.value("invoke",std::string{})==flow;}}
    const auto& definition=content().catalog.at("flows").at(flow);
    if(!allowed||!definition.value("runtime_enabled",true)||content().objects.at(definition.at("object").get<std::string>()).map!=map()||
       std::none_of(definition.at("steps").begin(),definition.at("steps").end(),[&](const auto& step){return step.value("cue",std::string{})==cue;})){
        throw std::logic_error("Unsupported pursuer reappearance cue");
    }
    scene_.cue(cue,definition.at("object"));
}
void EnemyHost::placeReappearanceObject(const std::string& object,TilePosition tile){
    const auto& special=content().enemies.at(id_).rules.at("runtime_profile").at("reappear").at("special");bool allowed=false;
    for(const auto& step:special.at("sequences")){
        allowed|=step.value("place_object",std::string{})==object||step.value("place_object_after_wait",std::string{})==object;
        if(step.contains("place_objects_before_wait")){for(const auto& placement:step.at("place_objects_before_wait")){allowed|=placement.at("object")==object;}}
    }
    if(!allowed||content().objects.at(object).map!=map()){throw std::logic_error("Unsupported pursuer reappearance placement");}
    auto actor=findCharacter(*scene_.world_,object);actor.motion=CharacterMotion(tile,actor.motion.facing(),actor.motion.speed());replaceCharacter(*scene_.world_,actor);
}

GridPosition EnemyHost::sampleActorScreen(const std::string& id) const{return scene_.sampleActorScreen(id);}
GridPosition EnemyHost::sourcePlayerBody() const{return scene_.sourcePlayerBody_;}
void EnemyHost::neuralGasHit(const std::string& flash){
    if(!scene_.active_||session().hp()<=0||session().flag(13)){return;}
    const auto& cue=content().cue(flash);if(cue.at("kind")!="flash_character"||cue.at("target")!=10001){throw std::logic_error("Gas flash must target player");}
    scene_.cue(flash,id_);session().setVariable(23,10);
    damagePlayer(biteDamage(10,session().defense(),random(-5,2)));
    session().setNeuralParalysis(true);session().setFlag(148,true);
}

void EnemyHost::advanceBlood(const nlohmann::json& blood,int selected){
    const int ce=blood.at("common_event");const int scratch=93+14*(ce-15);
    const int bodyX=blood.value("body_x_variable",83),bodyY=blood.value("body_y_variable",84);
    auto& session=this->session();if(!session.flag(blood.at("activation_fact"))){return;}
    const auto emit=[&](const std::string& cue,int commonEvent){
        const int variable=93+14*(commonEvent-24);
        const GridPosition sampled{double(session.variable(variable)),double(session.variable(variable+1))};
        present({PresentationEvent::Kind::Cue,cue,{},pictureWorkspacePosition(commonEvent),0,id_,sampled});
    };
    const auto sample=[&](int commonEvent,const nlohmann::json& offset){
        const int variable=93+14*(commonEvent-24);
        session.setVariable(variable,session.variable(bodyX)+offset.at(0).get<int>());
        session.setVariable(variable+1,session.variable(bodyY)+offset.at(1).get<int>());
    };
    auto progress=session.bloodPictureProgress(ce);
    if(!progress.type){
        const GridPosition body{static_cast<double>(session.variable(bodyX)),static_cast<double>(session.variable(bodyY))};
        if((body.x<=0&&body.y<=0)||body.y<=blood.at("stop_if_body_screen_y_at_most").get<int>()||body.y>=blood.at("stop_if_body_screen_y_at_least").get<int>()||
           !blood.at("types").contains(std::to_string(selected))){session.setFlag(blood.at("activation_fact"),false);return;}
        progress.type=selected;
    }
    const auto* type=&blood.at("types").at(std::to_string(progress.type));
    if(progress.frame==static_cast<int>(type->at("frames").size())){
        if(!pictureCommandsAllowed()){return;}
        emit(type->at("erase_cue"),ce+9);
        // CE15's later branches inherit scratch from the earlier branch's final frame.
        if(selected>progress.type&&blood.at("types").contains(std::to_string(selected))){
            progress={selected,0,true,false};type=&blood.at("types").at(std::to_string(selected));
            const auto& first=type->at("frames").at(0).at("offset_from_body_pixels");
            session.setVariable(scratch+1,session.variable(scratch+1)+first.at(1).get<int>());
        }else{session.setBloodPictureProgress({},ce);session.setFlag(blood.at("activation_fact"),false);return;}
    }
    const auto& frame=type->at("frames").at(progress.frame);
    if(!progress.sampled){sample(ce+9,frame.at("offset_from_body_pixels"));progress.sampled=true;}
    session.setBloodPictureProgress(progress,ce);
    if(!pictureCommandsAllowed()){return;}
    if(!progress.frame&&!progress.shown){emit(type->at("show_cue"),ce+9);progress.shown=true;}
    emit(frame.at("move_cue"),ce+9);++progress.frame;progress.sampled=false;
    session.setBloodPictureProgress(progress,ce);
}

void EnemyHost::playerPose(int pose){
    if(!holdsPlayer()&&!recoveryPermit_){throw std::logic_error("Player pose requires its owner");}
    const auto& row=content().catalog.at("access_keys").at("player_poses").at(std::to_string(pose));
    session().setVariable(31,pose);scene_.cue(row.at("graphic"));scene_.startRoute(row.at("route"),id_);
}
void EnemyHost::cleanupPlayer(){
    scene_.cancelPlayerActions("ce5",id_);if(!session().flag(10)){session().setVariable(29,0);}
}
void EnemyHost::startEscapeRecovery(){
    session().startRecovery(30,90);session().setFlag(1,false);session().setFlag(19,true);session().setFlag(17,true);
    session().setVariable(31,1);scene_.cue("ce9-c8");scene_.startRoute("ce9-c9",id_);
}
bool EnemyHost::dialogueActive()const{return scene_.messageRequest_.has_value()||scene_.inputRequest_.has_value();}
int EnemyHost::receiverAt(TilePosition tile)const{
    for(const auto& a:scene_.world_->objects()){if(a.active&&a.motion.tile()==tile){return a.eventId;}}
    return 0;
}
void EnemyHost::collisionReceiver(TilePosition tile){
    for(auto& enemy:scene_.enemies_){if(enemy->id()!=id_&&targetTile(enemy->id())==tile){enemy->collisionYield(actor().motion.facing());return;}}
}
void EnemyHost::eraseOwnedPictures(int first,int last){
    for(int picture=first;picture<=last;++picture){const auto key=id_+"-p22-c"+std::to_string(picture+1);present({PresentationEvent::Kind::Cue,key,{}, {},0,id_});}
}

Direction EnemyHost::sourceActorFacing(const std::string& id)const{return findCharacter(*scene_.world_,id).motion.facing();}
