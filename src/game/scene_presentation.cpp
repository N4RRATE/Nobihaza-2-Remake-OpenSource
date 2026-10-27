#include "scene_presentation.h"
#include "full_content.h"
#include "combat_scene.h"
#include "guard_controller.h"
#include "escort_controller.h"
#include <algorithm>
void ScenePresentation::message(const std::string& id){
    if(scene_.inputRequest_){throw std::runtime_error("Input prompt already owns message window");}
    const auto& source=scene_.content_.messages.at(id);auto m=resolveCombatMessage(source,scene_.session_);
    if(source.inheritDisplay){m.display=scene_.messageState_.display;}
    if(source.inheritFace){m.faceImage=scene_.messageState_.faceImage;m.faceIndex=scene_.messageState_.faceIndex;m.faceRight=scene_.messageState_.faceRight;m.faceFlipped=scene_.messageState_.faceFlipped;}
    if(!m.display.fixedPosition){
        const double y=scene_.world_->player().motion.position().y*scene_.definition_.tileSize;
        if(m.display.position==2&&y>=160){m.display.position=0;}
        else if(m.display.position==0&&y<80){m.display.position=2;}
        m.display.fixedPosition=true;
    }
    scene_.messageRequest_=scene_.dialogue_.request(m);
}
void ScenePresentation::sound(const std::string& id){const auto& c=scene_.content_.cue(id);if(c.at("sound").at("resource").is_null()){scene_.presentation_.push_back({PresentationEvent::Kind::StopSounds,{},{},{},0});}else{scene_.presentation_.push_back({PresentationEvent::Kind::Sound,"",readSound(c.at("sound")),{},{}});}}
void ScenePresentation::route(const std::string& id,const std::string& self){
    const auto& route=scene_.content_.routes.at(id);std::string actor;
    if(route.target==10001){actor="player";}else if(route.target==10005){actor=self;}else{
        for(const auto& a:scene_.world_->objects()){if(a.eventId==route.target){actor=a.id;break;}}
    }
    if(actor.empty()){throw std::runtime_error("Cannot bind route target: "+id);}
    findCharacter(*scene_.world_,actor);
    std::erase_if(scene_.routes_,[&](const auto& r){return r.actor()==actor;});scene_.routes_.emplace_back(route,actor,id);
}
void ScenePresentation::cue(const std::string& id,const std::string& self){
    const auto& c=scene_.content_.cue(id);const auto k=c.at("kind").get<std::string>();
    if(k=="place_object"){
        auto actor=findCharacter(*scene_.world_,c.at("object"));const auto& xy=c.at("tile");actor.motion=CharacterMotion({xy.at(0),xy.at(1)},actor.motion.facing(),actor.motion.speed());replaceCharacter(*scene_.world_,actor);
    }else if(k=="animation"){
        const int target=c.at("target");std::string actor=target==10001?"player":target==10005?self:std::string{};
        if(actor.empty()){for(const auto& candidate:scene_.world_->objects()){if(candidate.eventId==target){actor=candidate.id;break;}}}
        if(actor.empty()){throw std::runtime_error("Unbound animation target: "+id);}
        scene_.presentation_.push_back({PresentationEvent::Kind::Cue,id,{},findCharacter(*scene_.world_,actor).motion.position(),0,actor});
    }else if(k=="forced_route"){route(id,self);}
    else if(k=="wait"){scene_.wait_=std::max(1,c.at("tenths").get<int>()*6);}
    else if(k=="sound"){sound(id);}
    else if(k=="actor_graphic"){auto p=scene_.world_->player();p.spriteId=c.at("sprite").get<std::string>();scene_.world_->setPlayerState(p);}
    else if(k=="cancel_routes"){scene_.routes_.clear();}
    else if(k=="bgm"){
        scene_.musicPlayback_.reset();scene_.musicFirstPass_=false;scene_.creditsMusicWait_=-1;
        if(c.at("sound").is_null()||c.at("sound").at("resource").is_null()){scene_.presentation_.push_back({PresentationEvent::Kind::StopMusic,{},{},{},{}});}
        else{
            const bool monitored=(scene_.content_.catalog.contains("epilogue")&&id==scene_.content_.catalog.at("epilogue").at("credits_music").at("music").get<std::string>())||(scene_.content_.catalog.contains("clear")&&id==scene_.content_.catalog.at("clear").at("music_cue").get<std::string>());
            scene_.presentation_.push_back({PresentationEvent::Kind::Music,monitored?id:"map-entry",readSound(c.at("sound")),{},{}});
            if(monitored){scene_.musicPlayback_=MusicPlaybackIdentity{scene_.generation_,++scene_.nextMusicPlayback_,scene_.presentation_.back().sound.resource};scene_.presentation_.back().musicPlayback=scene_.musicPlayback_;}
        }
    }else if(k=="fade_bgm"){scene_.presentation_.push_back({PresentationEvent::Kind::FadeMusic,id,{},{},c.at("milliseconds")});
    }else if(k=="face"){
        if(c.at("value").is_null()){scene_.messageState_.faceImage.clear();}
        else{const auto& f=c.at("value");scene_.messageState_.faceImage=f.at("image");scene_.messageState_.faceIndex=f.at("index");scene_.messageState_.faceRight=f.at("right");scene_.messageState_.faceFlipped=f.at("flipped");}
    }else if(k=="message_options"){
        const auto& o=c.at("value");scene_.messageState_.display={o.at("transparent"),o.at("position"),o.at("fixed"),o.at("continue_events")};
    }else if(k=="camera"){
        const std::string operation=c.at("operation");int ticks=0;
        if(operation=="lock"){scene_.camera_.lock(true);}else if(operation=="unlock"){scene_.camera_.lock(false);}
        else if(operation=="pan"){ticks=scene_.camera_.pan(static_cast<Direction>(c.at("direction_original").get<int>()),c.at("distance_tiles"),c.at("speed_effective"));}
        else if(operation=="reset"){ticks=scene_.camera_.restore(c.at("speed_effective"));}
        if(c.at("wait").get<bool>()){scene_.wait_=std::max(1,ticks);}
    }else if(k=="panorama"&&c.contains("scroll")){
        const auto& p=c.at("scroll");PanoramaState next;
        next.definition={c.at("image"),p.at("loop_x"),p.at("loop_y"),p.at("auto_x"),p.at("auto_y"),p.at("speed_x"),p.at("speed_y")};
        // Pinned ChangeBG retains scrolling axes; non-looping axes reset on a non-looping map.
        if(scene_.panorama_){next.phase=scene_.panorama_->phase;}
        const auto& size=scene_.content_.media.at("images").at(next.definition.image).at("size_px");
        for(int axis=0;axis<2;++axis){const bool loop=axis?next.definition.loopY:next.definition.loopX,mapLoop=axis?false:scene_.definition_.loopX;const auto span=size.at(axis).get<std::int64_t>()*32;next.phase[axis]=(!loop&&!mapLoop)?0:(next.phase[axis]%span+span)%span;}
        scene_.panorama_=std::move(next);
    }else if(k=="panorama"){
        scene_.panorama_.reset();scene_.presentation_.push_back({PresentationEvent::Kind::Cue,id,{},{},0});
    }else if(k=="player_visibility"){scene_.playerVisible_=c.at("original_fields").at(0).get<int>()==1;
    }else if(k=="transition_setting"){
        (c.at("system_transition_slot").get<int>()==0?scene_.eraseTransition_:scene_.showTransition_)=c.at("transition_id");
    }else{scene_.presentation_.push_back({PresentationEvent::Kind::Cue,id,{},self.empty()?scene_.world_->player().motion.position():findCharacter(*scene_.world_,self).motion.position(),0,self});
        if((k=="show_picture"||k=="move_picture")&&c.at("position").at("mode")=="scene_anchor"){
            auto& event=scene_.presentation_.back();event.sampledScreenPosition=scene_.pictureAnchors_.at(c.at("position").at("anchor").get<std::string>());
            const auto center=scene_.camera_.center();event.pictureMapOrigin=GridPosition{center.x-160,center.y-120};
        }
        if((k=="show_picture"||k=="move_picture")&&c.at("position").at("mode")=="literal"){
            const auto center=scene_.camera_.center();scene_.presentation_.back().pictureMapOrigin=GridPosition{center.x-160,center.y-120};
        }
        if((k=="show_picture"||k=="move_picture")&&c.at("position").at("mode")=="epilogue_credits"){
            scene_.presentation_.back().resolvedPicture=ResolvedPicture{scene_.creditsPosition_,c.at("transparency_percent"),k=="move_picture"?c.at("duration_tenths").get<int>():0};
        }
        if((k=="show_picture"||k=="move_picture")&&c.at("position").at("mode")=="variables"&&scene_.content_.catalog.contains("score")&&id.starts_with("m213-e1-p1-")){
            if(c.at("position").at("xy")!=nlohmann::json::array({83,84})){throw std::runtime_error("Unknown score picture workspace");}
            const int picture=c.at("picture_id");const auto views=endingScorePictures(scene_.session_.endingScoreInput(),scene_.session_.variable(381));
            const auto view=std::find_if(views.begin(),views.end(),[&](const auto& v){return v.id==picture;});
            if(view==views.end()){throw std::runtime_error("Unknown score picture view");}
            scene_.presentation_.back().resolvedPicture=ResolvedPicture{{double(view->x),double(view->y)},c.at("transparency_percent"),k=="move_picture"?c.at("duration_tenths").get<int>():0};
        }
        if(k=="move_picture"&&c.at("wait").get<bool>()){scene_.wait_=std::max(1,c.at("duration_tenths").get<int>()*6);}
        if((k=="tint"||k=="flash_screen"||k=="shake")&&c.at("wait").get<bool>()){scene_.wait_=std::max(1,c.at("duration_tenths").get<int>()*6);}
        if(k=="erase_screen"||k=="show_screen"){
            const int value=c.value("original_transition_id",0);const int transition=value<0?(k=="erase_screen"?scene_.eraseTransition_:scene_.showTransition_):value;
            scene_.wait_=transition==(k=="erase_screen"?20:19)?1:20;scene_.presentation_.back().duration=scene_.wait_;scene_.presentation_.back().transition=transition;
        }
    }
}

bool ScenePresentation::executeCue(const std::string& id,const std::string& self){
    const auto& value=scene_.content_.cue(id);
    if(value.at("kind")=="wait_routes"){return !scene_.routesPending();}
    cue(id,self);
    if(value.at("kind")=="animation"&&value.value("wait",false)){
        const auto& animation=scene_.content_.media.at("animations").at(value.at("animation").get<std::string>());
        scene_.wait_=static_cast<int>(animation.at("frames").size())*animation.at("playback").at("logic_updates_per_frame").get<int>();
    }
    return true;
}
void ScenePresentation::advanceRoutes(){
    auto& s=scene_;bool pagesChanged=false;
    const auto switchChanged=[&](int id,bool value){pagesChanged|=s.session_.flag(id)!=value;s.session_.setFlag(id,value);};
    // Same-direction finite routes vacate leading tiles before followers test occupancy.
    const auto direction=s.routes_.empty()?std::optional<Direction>{}:s.routes_.front().translationDirection();
    if(direction&&std::all_of(s.routes_.begin(),s.routes_.end(),[&](const auto& route){return route.translationDirection()==direction;})){
        std::stable_sort(s.routes_.begin(),s.routes_.end(),[&](const auto& a,const auto& b){
            const auto x=findCharacter(*s.world_,a.actor()).motion.tile(),y=findCharacter(*s.world_,b.actor()).motion.tile();
            if(*direction==Direction::Left){return x.x<y.x;}if(*direction==Direction::Right){return x.x>y.x;}
            return *direction==Direction::Up?x.y<y.y:x.y>y.y;
        });
    }
    for(auto it=s.routes_.begin();it!=s.routes_.end();){if(it->tick(*s.world_,s.session_,s.presentation_,s.random_,switchChanged)){it=s.routes_.erase(it);}else{++it;}}
    for(auto it=s.ambientRoutes_.begin();it!=s.ambientRoutes_.end();){
        if(!findCharacter(*s.world_,it->actor()).active){it=s.ambientRoutes_.erase(it);continue;}
        const bool forced=std::any_of(s.routes_.begin(),s.routes_.end(),[&](const auto& r){return r.actor()==it->actor();});
        if(!forced&&(!s.messageRequest_||s.dialogue_.view().display.continueEvents)&&(!s.inputRequest_||s.inputPrompt_.view().display.continueEvents)&&it->tick(*s.world_,s.session_,s.presentation_,s.random_,switchChanged)){it=s.ambientRoutes_.erase(it);}else{++it;}
    }
    if(pagesChanged){s.refreshObjectPages();}
}

void ScenePresentation::validateSequence(const nlohmann::json& steps,const std::string& self,bool allowKeyCall) const{
    const auto& c=scene_.content_;int map=scene_.map_;
    const auto actorPresent=[&](const std::string& actor){const auto found=c.objects.find(actor);return actor=="player"||(found!=c.objects.end()&&found->second.map==map);};
    const auto checkTarget=[&](int target){
        if(target==10001){return;}
        if(target==10005){if(actorPresent(self)){return;}}
        else{for(const auto& [id,o]:c.objects){if(o.map==map&&!o.variants.empty()&&o.variants.front().definition.sourceEventId==target){return;}}}
        throw std::runtime_error("Missing presentation actor "+std::to_string(target)+" in map "+std::to_string(map));
    };
    for(std::size_t n=0;n<steps.size();++n){const auto& op=steps.at(n);const std::string kind=op.at("kind");
        try{
            if(kind=="ending_rank"||kind=="ending_rewards"||kind=="ending_unlock"){
                const auto& score=c.catalog.at("score");
                if(kind=="ending_unlock"){const int id=op.at("id");if(id<601||id>605||op.at("value")!=true){throw std::runtime_error("Unknown ending reward commit");}}
                if(kind=="ending_rewards"){for(const auto& [key,rows]:score.at("awards").items()){validateSequence(rows,self,false);}}
            }
            else if(kind=="sample_equipped_weapon"){if(!c.catalog.contains("g3")){throw std::runtime_error("Unbound equipment display sample");}}
            else if(kind=="registration_next"||kind=="registration_keyboard"){
                const auto& profile=c.catalog.at("registration");if(self!=profile.at("object").get<std::string>()||!actorPresent(self)){throw std::runtime_error("Registration operation outside its reception");}
                if(kind=="sample_equipped_weapon"){if(!c.catalog.contains("g3")){throw std::runtime_error("Unbound equipment display sample");}}
            else if(kind=="registration_next"){const auto& flows=profile.at("flows");if(std::find(flows.begin(),flows.end(),op.at("flow"))==flows.end()){throw std::runtime_error("Unknown registration continuation");}}
            }
            else if(kind=="if_player_moved"||kind=="unless_flag"||kind=="if_flag"){validateSequence(op.at("steps"),self,allowKeyCall);}
            else if(kind=="branch"){
                const auto& condition=op.at("condition");const auto type=condition.at("kind").get<std::string>();const int id=condition.value("id",0);
                if(type!="domain_compare"&&type!="player_facing"&&type!="actor_facing"&&id<=0){throw std::runtime_error("Invalid finite branch identity");}
                if(type=="fact"){(void)condition.at("value").get<bool>();}
                else if(type=="item"){(void)c.items.at(id);(void)condition.at("present").get<bool>();}
                else if(type=="known_skill"){
                    if(id!=scene_.session_.actorId()){throw std::runtime_error("Unsupported skill owner");}
                    (void)c.skills.at(condition.at("value").get<int>());
                }
                else if(type=="player_facing"){(void)directionFromName(condition.at("value").get<std::string>());}
                else if(type=="difficulty_at_least"){if(id!=19||condition.at("value").get<int>()<1||condition.at("value").get<int>()>3){throw std::runtime_error("Unsupported difficulty predicate");}}
                else if(type=="actor_facing"){
                    if(!actorPresent(condition.at("object"))||condition.at("directions").empty()){throw std::runtime_error("Missing finite facing actor");}for(const auto& d:condition.at("directions")){(void)directionFromName(d.get<std::string>());}
                }
                else if(type=="domain_compare"){
                    (void)guardSourceVariable(condition.at("field"));
                    static const std::set<std::string> operators{"at_least","at_most","equal","not_equal","greater","less"};if(!operators.contains(condition.at("operator"))){throw std::runtime_error("Unsupported domain comparison");}
                    (void)condition.at("operand").at("constant").get<int>();
                }
                else if(type!="actor"){throw std::runtime_error("Unsupported finite branch condition: "+type);}
                validateSequence(op.at("then"),self,allowKeyCall);validateSequence(op.at("else"),self,allowKeyCall);
            }
            else if(kind=="discard_key_choice"){
                if(!allowKeyCall||op.at("key_slot")!=44||op.at("flow")!="ce100"){throw std::runtime_error("Unsupported finite key call");}
                const auto& called=c.catalog.at("flows").at("ce100");
                if(!called.value("runtime_enabled",true)){throw std::runtime_error("Key flow is not implemented");}
                validateSequence(called.at("steps"),self,false);
            }
            else if(kind=="continue_flow"){
                const auto& called=c.catalog.at("flows").at(op.at("flow").get<std::string>());
                if(!allowKeyCall||n+1!=steps.size()||(called.at("trigger")!="invoked"&&!called.value("callable",false))||!called.value("runtime_enabled",true)||called.at("steps").empty()||!actorPresent(called.at("object"))){throw std::runtime_error("Unsupported terminal finite continuation");}
                validateSequence(called.at("steps"),called.at("object"),false);
            }
            else if(kind=="capture_continue_once"){
                const auto& destination=op.at("destination");const auto& checkpoint=continueCheckpoint(c,destination.at("map"));
                if(destination.at("tile")!=checkpoint.at("tile")){throw std::runtime_error("Unbound Continue capture destination");}
            }
            else if(kind=="clear_combat_workspace"){
                if(op.at("first")!=240||op.at("last")!=269){throw std::runtime_error("Unsupported combat workspace range");}
            }
            else if(kind=="invoke_flow"){
                if(!allowKeyCall){throw std::runtime_error("Nested finite event calls are unsupported");}
                const auto& called=c.catalog.at("flows").at(op.at("flow").get<std::string>());const auto& body=called.at("steps");
                if(!called.value("runtime_enabled",true)||!body.is_array()||body.size()!=1||body.at(0).at("kind")!="erase_event_once"){throw std::runtime_error("Unsupported finite event call");}
                validateSequence(body,called.at("object"),false);
            }
            else if(kind=="invoke_controller"){
                if(op.at("controller")=="escort-fall-choreography"){
                    const auto& flow=c.catalog.at("flows").at("escort-fall");if(op.at("object")!=flow.at("object")){throw std::runtime_error("Unbound fall actor");}validateSequence(flow.at("steps"),flow.at("object"),false);continue;
                }
                if(op.at("controller")!="crocodile-intro"||op.at("source_page")!="m108-e5-p1"){throw std::runtime_error("Unsupported finite controller call");}
                validateSequence(c.catalog.at("flows").at("crocodile-intro").at("steps"),c.catalog.at("flows").at("crocodile-intro").at("object"),false);
            }
            else if(kind=="decision_sample"){
                if(op.size()!=3||op.at("profile")!="ce39-after-room-reset"){throw std::runtime_error("Unreviewed finite Decision sample");}
            }
            else if(kind=="file_reading_lock"){
                const bool locked=op.at("locked");if(op.at("common_event")!=(locked?166:167)){throw std::runtime_error("Unsupported file action lock");}
            }
            else if(kind=="file_reading"){
                const auto& reading=c.catalog.at("readings").at(op.at("reading").get<std::string>());
                if(op.at("return_to_menu")!=false||op.at("common_event")!=157||reading.at("continuation").at("after_reading")!="calling-flow"){throw std::runtime_error("Unsupported calling-flow reading");}
                for(const auto* part:{"open","body","close"}){validateSequence(reading.at(part),self,false);}
            }
            else if(kind=="number_input"){
                const auto field=op.at("field");if(!((op.at("digits")==2&&(field=="clock_hours"||field=="clock_minutes"))||(op.at("digits")==4&&field=="access_password"&&c.catalog.contains("access_keys")))){throw std::runtime_error("Unsupported finite numeric input");}(void)guardSourceVariable(field);
            }
            else if(kind=="system_graphic"){(void)c.media.at("images").at(op.at(op.contains("image")?"image":"system_image").get<std::string>());}
            else if(kind=="presentation_group"){
                const auto& cues=op.at("cues");if(!cues.is_array()||cues.empty()){throw std::runtime_error("Empty finite presentation group");}
                for(const auto& id:cues){validateSequence(nlohmann::json::array({{{"kind","cue"},{"cue",id}}}),self,allowKeyCall);}
            }
            else if(kind=="cancel_player_actions"||(kind=="room_reset"&&c.full->contains("runtime")&&c.full->at("runtime").contains("player_action_cleanup"))){
                const auto name=kind=="room_reset"?std::string("ce5"):op.value("cleanup",std::string{});
                if(!name.empty()){
                    // Earlier F4 packages admit CE5 through their existing weapon cancellation contract.
                    if(c.full->empty()){
                        if(name!="ce5"){throw std::runtime_error("Unsupported legacy player cleanup");}
                        continue;
                    }
                    const auto& cleanup=c.full->at("runtime").at("player_action_cleanup").at(name);
                    if(cleanup.at("actor")!=1){throw std::runtime_error("Unsupported player cleanup actor");}
                    for(const auto* branch:{"normal","injured"}){const auto& cues=cleanup.at(branch);
                        validateSequence(nlohmann::json::array({{{"kind","cue"},{"cue",cues.at("graphic_cue")}},{{"kind","cue"},{"cue",cues.at("route_cue")}}}),self,allowKeyCall);
                    }
                }
            }
            else if(kind=="actor_display"){
                if(op.at("actor")!=scene_.session_.actorId()||!c.media.at("images").contains(op.at("face").get<std::string>())||op.at("face_index").get<int>()<0||op.at("face_index").get<int>()>15){throw std::runtime_error("Invalid actor display");}
            }
            else if(kind=="clear_save"||kind=="clear_actor_name"||kind=="clear_music_gate"||kind=="post_clear_request"){
                if(!c.catalog.contains("clear")||((kind=="post_clear_request")?map!=216:map!=c.catalog.at("clear").at("map"))){throw std::runtime_error("Clear operation outside source scene");}
                if(kind=="clear_actor_name"&&op.at("name")!="dammy"&&op.at("name")!="　通关数据"){throw std::runtime_error("Unknown Clear actor name");}
                if(kind=="post_clear_request"&&op.at("selection")!=0&&op.at("selection")!=1){throw std::runtime_error("Unknown Clear choice");}
            }
            else if(kind=="clear_title"){
                if(!(c.catalog.contains("clear")&&map==c.catalog.at("clear").at("map"))&&!(c.catalog.contains("bonus")&&map==c.catalog.at("bonus").at("map"))){throw std::runtime_error("Title request outside delivered ending scene");}
            }
            else if(kind=="chapter_save"){
                const auto& chapter=c.catalog.at("chapter");if(op.at("node")!=chapter.at("node")||map!=chapter.at("map")||self!=chapter.at("object").get<std::string>()){throw std::runtime_error("Invalid chapter save node");}
            }
            else if(kind=="grant"||kind=="remove"){
                const int item=op.at("item"),count=op.at("count");(void)c.items.at(item);if(count<1||count>99){throw std::runtime_error("Invalid finite inventory change");}
            }
            else if(kind=="picture_anchor"){
                const auto actor=op.at("actor")=="self"?self:op.at("actor").get<std::string>();
                if(!actorPresent(actor)||op.at("anchor").get<std::string>().empty()||op.at("offset").size()!=2){throw std::runtime_error("Missing finite picture anchor actor");}
                for(const auto& axis:op.at("sample")){if(axis!="x"&&axis!="y"){throw std::runtime_error("Unsupported picture anchor axis");}}
                for(const auto& n:op.at("offset")){(void)n.get<int>();}if(op.contains("y")){(void)op.at("y").get<int>();}
            }
            else if(kind=="selected_key"){(void)c.items.at(op.at("item").get<int>());}
            else if(kind=="actor_resource"){if((op.at("resource")!="hp"&&op.at("resource")!="tp")||op.at("points")!=999){throw std::runtime_error("Unsupported actor resource change");}}
            else if(kind=="restore_actor"){
                const bool pursuer=c.full->contains("state_contract")&&op.at("actor")==c.full->at("state_contract").at("pursuer_health").at("original_actor");
                if((op.at("actor")!=scene_.session_.actorId()&&!pursuer)||op.at("health")!="full"||op.at("states")!="clear"){throw std::runtime_error("Unsupported actor restoration");}
            }
            else if(kind=="party_change"){
                const int actor=op.at("actor");if(actor!=1&&actor!=8){throw std::runtime_error("Unsupported finite party actor");}(void)op.at("join").get<bool>();
            }
            else if(kind=="victory_head"){
                if(!c.catalog.contains("final_boss")||map!=c.catalog.at("final_boss").at("map")||op.at("state")!=5){throw std::runtime_error("Unsupported victory head presentation");}
            }
            else if(kind=="credits_begin"||kind=="credits_coordinate"){
                if(!c.catalog.contains("epilogue")||map!=c.objects.at(c.catalog.at("epilogue").at("credits_music").at("object").get<std::string>()).map){throw std::runtime_error("Credits workspace outside its scene");}
                if(kind=="credits_coordinate"){
                    if((op.at("axis")!="x"&&op.at("axis")!="y")||(op.at("operation")!="set"&&op.at("operation")!="add")){throw std::runtime_error("Invalid credits coordinate");}(void)op.at("value").get<int>();
                }
            }
            else if(kind=="flags"){
                (void)op.at("value").get<bool>();const auto& ids=op.at("ids");
                if(!ids.is_array()||ids.empty()){throw std::runtime_error("Empty finite flag group");}
                for(const auto& id:ids){if(id.get<int>()<=0){throw std::runtime_error("Invalid finite flag identity");}}
            }
            else if(kind=="combine_skill"){
                const int skill=op.at("skill");
                if(op.at("actor")!=scene_.session_.actorId()||(skill!=3&&skill!=4)){throw std::runtime_error("Unsupported mixing skill transition");}
                (void)c.skills.at(skill);(void)op.at("learn").get<bool>();
            }
            else if(kind=="scene_sample"){
                (void)guardSourceVariable(op.at("field"));if(!actorPresent(op.at("entity"))||(op.at("component")!="tile_x"&&op.at("component")!="tile_y")){throw std::runtime_error("Unsupported finite actor sampling");}
            }
            else if(kind=="enemy_entry_state"){
                const auto& enemy=c.expansion.enemies.at(op.at("enemy"));
                if(op.at("state")!=9||!enemy.rules.contains("entry_state_variable")){throw std::runtime_error("Unsupported enemy entry state");}
            }
            else if(kind=="domain_value"){
                const bool door=op.at("field")=="corridor_door_status"&&(op.at("value")==1||op.at("value")==2);
                const bool light=(op.at("field")=="light_facing"||op.at("field")=="light_saved_facing")&&op.at("value")==0;
                const bool pose=op.at("field")=="player_pose"&&op.at("value").get<int>()>=0&&op.at("value").get<int>()<=4;
                if(!door&&!light&&!pose){(void)guardSourceVariable(op.at("field"));}
                if(op.at("operation")!="set"&&op.at("operation")!="add"&&!(op.at("field")=="treatment_remaining"&&op.at("operation")=="subtract")){throw std::runtime_error("Unsupported scene domain assignment");}
            }
            else if(kind=="guard_reward_tier"){if(op.at("tiers").size()!=9){throw std::runtime_error("Incomplete guard reward tiers");}for(const auto& tier:op.at("tiers")){(void)tier.at("kills_at_least").get<int>();(void)tier.at("rescues_at_least").get<int>();validateSequence(tier.at("steps"),self,allowKeyCall);}}
            else if(kind=="tile_substitution"){
                const int from=op.at("from_index"),to=op.at("to_index");
                if(from<0||from>=144||to<0||to>=144){throw std::runtime_error("Invalid tile substitution index");}
                if(op.at("layer")=="upper"){
                    const auto& rows=c.maps.at(map).upperTileReplacements;if(std::none_of(rows.begin(),rows.end(),[&](const auto& r){return r.fromIndex==from&&r.toIndex==to;})){throw std::runtime_error("Missing finite tile substitution");}
                }else if(op.at("layer")=="lower"){
                    const auto& lower=c.maps.at(map).lowerTiles;if(!lower||std::find(lower->allowed.begin(),lower->allowed.end(),std::array<int,2>{from,to})==lower->allowed.end()){throw std::runtime_error("Missing finite lower substitution");}
                }else{throw std::runtime_error("Unsupported tile substitution layer");}
            }
            else if(kind=="domain_arithmetic"&&op.at("field").get<std::string>().starts_with("escort_")){
                static const std::set<std::string> fields{"escort_skin_enabled","escort_skin_frame","escort_last_facing","escort_walk_phase"};if(!fields.contains(op.at("field"))){throw std::runtime_error("Unknown escort field");}(void)op.at("operand").at("constant").get<int>();
            }
            else if(kind=="domain_arithmetic"){(void)guardSourceVariable(op.at("field"));const auto operation=op.at("operation");if(operation!="multiply"&&operation!="add"&&operation!="set"){throw std::runtime_error("Unsupported guard arithmetic");}const auto& operand=op.at("operand");if(operand.contains("field")){(void)guardSourceVariable(operand.at("field"));}else{(void)operand.at("constant").get<int>();}}
            else if(kind=="reset_guard_work"){if(op.at("range")!=nlohmann::json::array({240,269})||op.at("kills")!=0||op.at("rescues")!=0){throw std::runtime_error("Unsupported guard reset");}}
            else if(kind=="companion_level_refresh"||kind=="companion_party"||kind=="restore_companion"){const bool first=c.full->contains("runtime")&&c.full->at("runtime").contains("parking")&&op.at("actor")==c.full->at("runtime").at("parking").at("first_guard").at("actor");const bool second=c.catalog.contains("guard_second")&&op.at("actor")==c.catalog.at("guard_second").at("runtime_profile").at("actor");if(!first&&!second){throw std::runtime_error("Unsupported companion identity");}}
            else if(kind=="remove_selected_key"){
                if(op.at("key_slot")!=44||op.at("count")!=1){throw std::runtime_error("Unsupported selected key consumption");}
            }
            else if(kind=="erase_event_once"&&(op.value("object",self)=="player"||!actorPresent(op.value("object",self)))){throw std::runtime_error("Missing erased actor");}
            else if(kind=="place_object"){
                if(!actorPresent(op.at("object"))){throw std::runtime_error("Missing placed actor");}
                if(op.contains("tile_fields")){
                    const auto& heat=c.catalog.at("heat_room");
                    if(map!=heat.at("map")||op.at("tile_fields")!=heat.at("ground_fields")||(op.at("object")!=heat.at("ground_object")&&op.at("object")!=heat.at("conversion_object"))){throw std::runtime_error("Unsupported persistent ground placement");}
                }else{(void)op.at("tile").get<std::array<int,2>>();}
            }
            else if(kind=="place_at_player"){if(!actorPresent(op.at("object"))){throw std::runtime_error("Missing placed actor");}}
            else if(kind=="choice"){
                if(op.contains("options")){
                    const auto& options=op.at("options");const int cancel=op.at("cancel_option");
                    if(!options.is_array()||options.empty()||options.size()>4||cancel<0||cancel>static_cast<int>(options.size())){throw std::runtime_error("Unsupported finite choice definition");}
                    for(const auto& option:options){if(option.at("label").get<std::string>().empty()){throw std::runtime_error("Empty finite choice label");}validateSequence(option.at("steps"),self,allowKeyCall);}
                }else{(void)c.cue(op.at("cue").get<std::string>());validateSequence(op.at("accept"),self,allowKeyCall);validateSequence(op.at("decline"),self,allowKeyCall);}
            }
            else if(kind=="track_picture_until_flag"){
                if(!actorPresent(op.at("actor"))||!c.cues.contains(op.at("cue").get<std::string>())||c.cue(op.at("cue").get<std::string>()).at("kind")!="move_picture"||op.at("flag").get<int>()<=0){throw std::runtime_error("Invalid tracked story picture");}
                (void)op.at("offset").get<std::array<int,2>>();(void)op.at("value").get<bool>();
            }
            else if(kind=="required_relocate"){
                const auto& destination=op.at("destination");map=destination.at("map");const auto& tile=destination.at("tile");
                if(c.catalog.contains("pending_maps")&&c.catalog.at("pending_maps").contains(std::to_string(map))){
                    if(c.catalog.contains("score")&&destination==c.catalog.at("score").at("clear_destination")){continue;}
                    const auto& retry=c.catalog.at("continue").at("retry");if(destination!=retry){throw std::runtime_error("Unbound required destination");}
                }else{
                    const auto& definition=c.maps.at(map);const int x=tile.at(0),y=tile.at(1);if(x<0||y<0||x>=definition.width||y>=definition.height){throw std::runtime_error("Required transfer outside map");}
                }
            }
            else if(kind=="relocate"){
                const auto& destination=op.at("destination");const auto& tile=destination.at("tile");const auto facing=op.value("facing",std::string("preserve"));map=destination.at("map");
                (void)scene_.prepareWorld(map,{tile.at(0),tile.at(1)},facing=="preserve"?scene_.world_->player().motion.facing():directionFromName(facing),scene_.session_);
            }
            else if(kind=="scene_transfer"){map=op.at("map");(void)c.maps.at(map);}
            else if(kind=="message"){(void)c.messages.at(op.at(op.contains("ref")?"ref":"message").get<std::string>());}
            else if(kind=="cue"){
                const std::string id=op.at(op.contains("ref")?"ref":"cue");const auto& cue=c.cue(id);const std::string type=cue.at("kind");
                if(type=="forced_route"){
                    const auto& route=c.routes.at(id);checkTarget(route.target);
                    for(const auto& step:route.steps){if(step.kind=="graphic"&&!c.maps.at(map).sprites->contains(step.sprite)){throw std::runtime_error("Missing route sprite: "+step.sprite);}}
                }else if(type=="panorama"&&cue.contains("scroll")){
                    (void)c.media.at("images").at(cue.at("image").get<std::string>());const auto& p=cue.at("scroll");
                    if(p.size()!=7||p.at("speed_unit")!="original-rpgmaker-speed-setting"||(p.at("auto_x").get<bool>()&&!p.at("loop_x").get<bool>())||(p.at("auto_y").get<bool>()&&!p.at("loop_y").get<bool>())||p.at("speed_x").get<int>()< -8||p.at("speed_x").get<int>()>8||p.at("speed_y").get<int>()< -8||p.at("speed_y").get<int>()>8){throw std::runtime_error("Invalid dynamic panorama parameters");}
                }
                else if(type=="animation"){checkTarget(cue.at("target"));(void)c.media.at("animations").at(cue.at("animation").get<std::string>());}
                else if(type=="place_object"&&!actorPresent(cue.at("object"))){throw std::runtime_error("Missing placed actor");}
            }else{
                // Resource-free operations are still a closed finite vocabulary.
                // A new command requires a named executor and validation contract.
                static const std::set<std::string> valueSteps{
                    "flag","toggle_flag","learn_skill","forget_skill","wait","wait_routes","finish","finish_event",
                    "pose","height","place_player","reset_player_actions","restore_dead_enemies","convert_tool",
                    "enemy_state","set_enemy_damage","timer","timer_set","timer_start","timer_stop","discard_key",
                    "normalize_loose_ammo","password_reward","chapter_handoff_after_file","room_reset","erase_event_once",
                    "enable_combine_if_known","match_positions","all_dogs_defeated",
                    "world_fact","initial_value","grant_item","party_member","equip_armor","stop_environment",
                    "normal_enemy_initialization","actor_name","decision_sound","menu_permission","save_permission",
                    "difficulty_baseline","normal_confirm_commit","difficulty_confirm_commit","difficulty_enemy_initialization","erase_origin_event"};
                if(!valueSteps.contains(kind)){throw std::runtime_error("Unsupported finite presentation step: "+kind);}
            }
        }catch(const std::exception& e){throw std::runtime_error("Presentation step "+std::to_string(n+1)+" "+op.value("source",nlohmann::json{}).dump()+": "+e.what());}
    }
}
