#include "warehouse_save_state.h"
#include "combat_presentation.h"
#include "scene_renderer.h"
#include "simulation_clock.h"
#include "picture_tone.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <SFML/Audio/InputSoundFile.hpp>
namespace{
    std::string mediaLocation(const CombatContent& content,const std::string& id,const nlohmann::json& value){
        return id+" ["+value.value("source_file",std::string{"generated"})+"; "+
            (value.contains("source")?value.at("source").value("location",std::string{}):std::string{})+
            "; "+(content.directory/value.at("file").get<std::string>()).string()+"]";
    }
    sf::Vector2f foot(GridPosition p){return {static_cast<float>((p.x+.5)*16),static_cast<float>((p.y+1)*16)};}
    sf::Color tone(const nlohmann::json& a){return {static_cast<std::uint8_t>(std::clamp(a.at(0).get<int>()*255/100,0,255)),static_cast<std::uint8_t>(std::clamp(a.at(1).get<int>()*255/100,0,255)),static_cast<std::uint8_t>(std::clamp(a.at(2).get<int>()*255/100,0,255))};}
}
CombatPresentation::CombatPresentation(const CombatContent& content,const sf::Font& font):content_(content),font_(font){
    for(const auto& [id,image]:content.media.at("images").items()){
        auto& texture=images_[id];const auto path=content.directory/image.at("file").get<std::string>();
        if(!texture.loadFromFile(path)){throw std::runtime_error("Cannot load image: "+mediaLocation(content,id,image));}
        const auto& size=image.at("size_px");if(texture.getSize()!=sf::Vector2u(size.at(0),size.at(1))){throw std::runtime_error("Wrong image dimensions: "+mediaLocation(content,id,image));}texture.setSmooth(false);
    }
    systemImage_=content_.media.at("ui").at("menu").at("system_image").get<std::string>();
    // Match 05's audited CN1.00 palette key; prepared resources follow this presentation's lifetime.
    for(const auto& [id,image]:images_){if(image.getSize()==sf::Vector2u{160,80}){windowSkins_.try_emplace(id,image.copyToImage(),sf::Color(255,156,0));}}
    if(!windowSkins_.contains(systemImage_)){throw std::runtime_error("Invalid menu System atlas: "+systemImage_);}
    // Finite source stages share one prepared texture per image/tone; drawing does no pixel work.
    for(const auto& [id,effect]:content.media.at("effects").items()){
        if(!effect.contains("stage_tone_percent")){continue;}
        const auto& tones=effect.at("stage_tone_percent");
        if(tones.size()!=effect.at("stage_offsets_px").size()){throw std::runtime_error("Effect tone stage count differs");}
        for(std::size_t stage=0;stage<tones.size();++stage){
            const auto value=tones.at(stage).get<std::array<int,4>>();const auto image=effect.contains("stage_images")?effect.at("stage_images").at(stage).get<std::string>():effect.at("image").get<std::string>();
            const auto key=std::pair{image,value};if(tonedImages_.contains(key)){continue;}
            auto pixels=images_.at(image).copyToImage();const auto size=pixels.getSize();
            for(unsigned y=0;y<size.y;++y){for(unsigned x=0;x<size.x;++x){const auto color=pixels.getPixel({x,y});const auto toned=pictureTonePixel({color.r,color.g,color.b,color.a},value);pixels.setPixel({x,y},{toned[0],toned[1],toned[2],toned[3]});}}
            auto& texture=tonedImages_[key];if(!texture.loadFromImage(pixels)){throw std::runtime_error("Cannot prepare effect tone texture");}texture.setSmooth(false);
        }
    }
    palette_=images_.at(content.media.at("ui").at("menu").at("system_image").get<std::string>()).copyToImage();
    for(const auto& [id,audio]:content.media.at("audio").items()){
        const auto path=content.directory/audio.at("file").get<std::string>();
        if(id.starts_with("bgm-")||audio.value("source_file",std::string{}).starts_with("Music/")){
            // Check the candidate without retaining decoded music or starting playback.
            sf::InputSoundFile input;
            if(!input.openFromFile(path)||!input.getChannelCount()||!input.getSampleCount()){
                throw std::runtime_error("Cannot open BGM: "+mediaLocation(content,id,audio));
            }
            const auto frames=input.getSampleCount()/input.getChannelCount();
            if(audio.contains("loop_samples")){
                const auto& loop=audio.at("loop_samples");const auto start=loop.at("start").get<std::int64_t>(),end=loop.at("end").get<std::int64_t>(),initial=audio.at("initial_sample_compatibility").get<std::int64_t>();
                if(start<0||end<=start||initial<0||initial>=end||static_cast<std::uint64_t>(end)>frames){throw std::runtime_error("Invalid BGM PCM loop: "+mediaLocation(content,id,audio));}
            }
            if(audio.contains("clear_midi")){
                const auto& timing=audio.at("clear_midi");
                if(input.getSampleRate()!=44100||input.getChannelCount()!=2||timing.at("end_frame")!=frames||timing.at("initial_frame")<0||timing.at("initial_frame")>=frames||timing.at("profile")!="fixed-player-message-midi-end-v1"||timing.at("pitch")!=50){throw std::runtime_error("Invalid end MIDI PCM profile: "+mediaLocation(content,id,audio));}
            }
            continue;
        }
        if(!buffers_[id].loadFromFile(path)){throw std::runtime_error("Cannot decode sound: "+mediaLocation(content,id,audio));}
    }
}
std::unique_ptr<PredecodedMusic> CombatPresentation::prepareMusic(const SoundDefinition& sound,bool observeFirstPass) const{
    const auto& definition=content_.media.at("audio").at(sound.resource);std::optional<MusicLoop> loop;
    if(definition.contains("loop_samples")){if(sound.pitch!=100){throw std::runtime_error("MIDI tempo must be rendered offline");}loop=MusicLoop{definition.at("initial_sample_compatibility"),definition.at("loop_samples").at("start"),definition.at("loop_samples").at("end")};}
    try{
        auto music=std::make_unique<PredecodedMusic>(content_.directory/definition.at("file").get<std::string>(),loop,observeFirstPass,definition.value("clear_midi",nlohmann::json{}));music->setVolume(static_cast<float>(sound.volume));music->setPitch(definition.contains("clear_midi")?1.f:sound.pitch/100.f);music->setPan((sound.balance-50)/50.f);return music;
    }catch(const std::exception& e){throw std::runtime_error("BGM "+mediaLocation(content_,sound.resource,definition)+": "+e.what());}
}
void CombatPresentation::prepareRestoredState(const SavedPresentation& state){
    if(!state.warehouseVisual.is_null()&&!windowSkins_.contains(state.warehouseVisual.at("system").get<std::string>())){throw std::runtime_error("Restored System atlas was not prepared");}
    musicPlayback_.reset();
    auto candidate=state.music.resource.empty()?nullptr:prepareMusic(state.music);music_=std::move(candidate);musicDescription_=state.music;musicVolume_=static_cast<float>(state.music.volume);musicFade_=toneRemaining_=0;
    saturation_=startSaturation_=targetSaturation_=state.saturation;
    for(int i=0;i<3;++i){screenTone_[i]=startTone_[i]=targetTone_[i]=static_cast<float>(state.tone[i]);}
    restoreWarehouseVisual(state.warehouseVisual);
}
void CombatPresentation::activateRestoredState(){if(music_&&!(rememberFadedMusic_&&musicFadeTotal_>0&&musicFade_==0)){music_->setVolume(musicFadeTotal_>0&&musicFade_>0?musicVolume_*musicFade_/musicFadeTotal_:musicVolume_);music_->play();}}
SavedPresentation CombatPresentation::saveState() const{
    SavedPresentation state;state.saturation=static_cast<int>(std::round(saturation_));state.music=musicDescription_;for(int i=0;i<3;++i){state.tone[i]=static_cast<int>(std::round(screenTone_[i]));}if(content_.catalog.contains("warehouse_direct")){state.warehouseVisual=warehouseVisual();}return state;
}
const std::map<std::string,sf::Texture>& CombatPresentation::images() const{return images_;}
const sf::Image& CombatPresentation::palette() const{return palette_;}
void CombatPresentation::sound(const SoundDefinition& s){
    if(s.resource.empty()){return;}auto playing=std::make_unique<sf::Sound>(buffers_.at(s.resource));
    playing->setVolume(static_cast<float>(s.volume));playing->setPitch(s.pitch/100.f);playing->setPan((s.balance-50)/50.f);playing->play();sounds_.push_back(std::move(playing));
}
void CombatPresentation::cue(const PresentationEvent& e){
    const auto& c=content_.cue(e.id);const auto kind=c.at("kind").get<std::string>();
    if(kind=="weather"){weatherType_=c.at("weather_type");weatherStrength_=c.at("strength");if(weatherStrength_<0||weatherStrength_>2){throw std::runtime_error("Unsupported rain strength");}if(weatherType_!=0&&weatherType_!=1){throw std::runtime_error("Unsupported weather type");}return;}
    if(kind=="shake"){shakeTicks_=c.at("duration_tenths").get<int>()*6;shakeStrength_=c.at("strength");shakeSpeed_=c.at("speed");shakeAge_=0;return;}
    if(kind=="erase_screen"||kind=="show_screen"){
        scrollDirection_=0;blackTarget_=kind=="erase_screen"?255:0;
        if(e.transition==9||e.transition==10){
            scrollDirection_=e.transition==9?-1:1;scrollShowing_=kind=="show_screen";
            scrollFrame_=0;scrollDuration_=std::max(1,e.duration);scrollCaptured_=false;black_=blackTarget_;
        }else if(e.duration==1){black_=blackTarget_;}return;
    }
    if(kind=="panorama"){if(!c.contains("scroll")){panorama_=c.at("image");}return;}
    if(kind=="tint"){
        const auto& value=c.at("tone_percent");startTone_=screenTone_;startSaturation_=saturation_;targetSaturation_=value.at(3);
        for(int i=0;i<3;++i){targetTone_[i]=value.at(i);}
        toneDuration_=toneRemaining_=c.at("duration_tenths").get<int>()*6;
        if(!toneDuration_){screenTone_=targetTone_;saturation_=targetSaturation_;}return;
    }
    if(kind=="flash_screen"){
        const auto& rgb=c.at("rgb_power");const int maximum=c.at("channel_max");
        screenFlash_={static_cast<std::uint8_t>(rgb.at(0).get<int>()*255/maximum),static_cast<std::uint8_t>(rgb.at(1).get<int>()*255/maximum),static_cast<std::uint8_t>(rgb.at(2).get<int>()*255/maximum),static_cast<std::uint8_t>(rgb.at(3).get<int>()*255/maximum)};
        flashDuration_=flashRemaining_=std::max(1,c.at("duration_tenths").get<int>()*6);return;
    }
    if(kind=="erase_picture"){pictures_.erase(c.at("picture_id").get<int>());return;}
    if(kind=="animation"){effects_.push_back({c.at("animation"),e.position,0,true});return;}
    if(kind=="show_picture"||kind=="move_picture"){
        auto& p=pictures_[c.at("picture_id").get<int>()];if(kind=="show_picture"&&(!e.resolvedPicture||p.image.empty()||p.image!=c.at("image").get<std::string>())){p=Picture{};p.image=c.at("image");}
        if(c.at("position").at("mode")!="literal"&&!e.resolvedPicture&&e.actor.empty()){throw std::runtime_error("Variable picture requires reviewed resolved anchor");}
        if(kind=="show_picture"){p.mapFixed=c.at("fixed_to_map");p.eraseOnMapChange=c.value("erase_on_map_change",true);}
        const bool variable=c.at("position").at("mode")!="literal";
        // shortcut: legacy variable cues keep world anchors until their source screen sampling is audited.
        const bool sceneAnchor=c.at("position").at("mode")=="scene_anchor";
        p.worldAnchored=variable&&!sceneAnchor&&(!e.sampledScreenPosition||p.mapFixed);
        if(sceneAnchor&&(!e.sampledScreenPosition||!e.pictureMapOrigin)){throw std::runtime_error("Scene picture requires prepared anchor");}
        const auto xy=(sceneAnchor||e.resolvedPicture)?nlohmann::json::array({0,0}):c.at("position").at("xy");
        p.targetPosition=!variable?sf::Vector2f{xy.at(0).get<float>(),xy.at(1).get<float>()}:
            p.worldAnchored?foot(e.position):sf::Vector2f{static_cast<float>(e.sampledScreenPosition->x),static_cast<float>(e.sampledScreenPosition->y)};
        // Literal coordinates are screen pixels even for map-fixed pictures.
        if((!variable||sceneAnchor)&&p.mapFixed&&e.pictureMapOrigin){p.targetPosition+=sf::Vector2f{float(e.pictureMapOrigin->x),float(e.pictureMapOrigin->y)};}
        p.rotationSpeed=c.at("effect_mode")==1?c.at("effect_power_effective").get<float>()*360.f/256:0;p.targetZoom=c.at("zoom_percent");p.targetOpacity=255*(100-c.at("transparency_percent").get<float>())/100;p.targetColor=tone(c.at("tone_percent"));p.targetWave=c.at("effect_mode")==2?c.at("effect_power_effective").get<int>():0;
        if(e.resolvedPicture){const auto& r=*e.resolvedPicture;p.mapFixed=false;p.worldAnchored=false;p.targetPosition={static_cast<float>(r.position.x),static_cast<float>(r.position.y)};p.targetOpacity=255.f*(100-r.transparency)/100;}
        p.total=p.remaining=(e.resolvedPicture?e.resolvedPicture->durationTenths:c.value("duration_tenths",0))*6;p.startPosition=p.position;p.startZoom=p.zoom;p.startOpacity=p.opacity;p.startColor=p.color;p.startWave=p.wave;
        if(p.total==0){p.position=p.targetPosition;p.zoom=p.targetZoom;p.opacity=p.targetOpacity;p.color=p.targetColor;p.wave=p.targetWave;}
        return;
    }
    if(kind=="flash_character"){
        const std::string actor=c.at("target")==10001?"player":e.actor;
        if(actor.empty()){throw std::runtime_error("Unbound flash target");}
        const auto& rgb=c.at("rgb_power");const int max=c.at("channel_max");
        const int duration=std::max(1,c.at("duration_tenths").get<int>()*6);
        flashes_[actor]={{static_cast<std::uint8_t>(rgb.at(0).get<int>()*255/max),static_cast<std::uint8_t>(rgb.at(1).get<int>()*255/max),static_cast<std::uint8_t>(rgb.at(2).get<int>()*255/max),static_cast<std::uint8_t>(rgb.at(3).get<int>()*255/max)},duration,duration};return;
    }
    throw std::runtime_error("Unsupported presentation cue: "+kind);
}
void CombatPresentation::update(const std::vector<PresentationEvent>& events,bool advanceWorld){
    ++uiFrame_;
    if(advanceWorld){
    if(scrollDirection_&&++scrollFrame_>=scrollDuration_){scrollDirection_=0;scrollCaptured_=false;}
    advanceRain();if(shakeTicks_>0){--shakeTicks_;++shakeAge_;}
    if(noticeTicks_>0){--noticeTicks_;}
    if(toneRemaining_>0){--toneRemaining_;const float f=1.f-static_cast<float>(toneRemaining_)/toneDuration_;saturation_=startSaturation_+(targetSaturation_-startSaturation_)*f;for(int i=0;i<3;++i){screenTone_[i]=startTone_[i]+(targetTone_[i]-startTone_[i])*f;}}
    if(flashRemaining_>0){--flashRemaining_;}
    for(auto& [id,p]:pictures_){p.rotation+=p.rotationSpeed;}
    std::erase_if(sounds_,[](const auto& s){return s->getStatus()==sf::SoundSource::Status::Stopped;});
    for(auto& [id,f]:flashes_){--f.remaining;}
    std::erase_if(flashes_,[](const auto& pair){return pair.second.remaining<=0;});
    for(auto& effect:effects_){++effect.age;}
    std::erase_if(effects_,[&](const Effect& e){
        if(e.animation){const auto& a=content_.media.at("animations").at(e.id);return e.age>=static_cast<int>(a.at("frames").size())*a.at("playback").at("logic_updates_per_frame").get<int>();}
        int duration=0;for(int n:content_.media.at("effects").at(e.id).at("wait_zero_count_after_stage")){duration+=n;}return e.age>=duration;
    });
    }
    for(const auto& e:events){
        switch(e.kind){
        case PresentationEvent::Kind::SystemGraphic:if(!windowSkins_.contains(e.id)){throw std::runtime_error("Missing prepared System atlas: "+e.id);}systemImage_=e.id;palette_=images_.at(e.id).copyToImage();break;
        case PresentationEvent::Kind::StopSounds:sounds_.clear();break;
        case PresentationEvent::Kind::ResetGame:
            musicPlayback_.reset();
            weatherType_=weatherStrength_=shakeTicks_=0;rain_.clear();rainRandom_.seed(17431);rainCameraOrigin_.reset();saturation_=startSaturation_=targetSaturation_=100;
            musicDescription_={};
            music_.reset();sounds_.clear();musicFade_=musicFadeTotal_=0;musicVolume_=100;
            pictures_.clear();effects_.clear();flashes_.clear();notice_.clear();noticeTicks_=0;
            scrollDirection_=0;scrollCaptured_=false;black_=blackTarget_=0;screenTone_=targetTone_={90,90,90};toneRemaining_=0;flashRemaining_=0;panorama_.clear();break;
        case PresentationEvent::Kind::SceneChanged:
            if(musicPlayback_){musicFade_=0;}musicPlayback_.reset();
            rainCameraOrigin_.reset();
            std::erase_if(pictures_,[](const auto& value){return value.second.eraseOnMapChange;});
            effects_.clear();flashes_.clear();panorama_.clear();flashRemaining_=0;break;
        case PresentationEvent::Kind::Sound:sound(e.sound);break;
        case PresentationEvent::Kind::Music:{
            if((e.id=="map-entry"||e.id=="restore-clear-instance")&&music_&&musicFade_==0&&musicDescription_.resource==e.sound.resource){
                musicPlayback_=e.musicPlayback;musicDescription_=e.sound;musicFade_=0;musicVolume_=static_cast<float>(e.sound.volume);
                music_->setVolume(musicVolume_);music_->setPitch(content_.media.at("audio").at(e.sound.resource).contains("clear_midi")?1.f:e.sound.pitch/100.f);music_->setPan((e.sound.balance-50)/50.f);if(music_->getStatus()==sf::SoundSource::Status::Stopped){music_->play();}break;
            }
            auto candidate=prepareMusic(e.sound,e.musicPlayback.has_value());music_=std::move(candidate);musicDescription_=e.sound;musicPlayback_=e.musicPlayback;musicFade_=0;musicVolume_=static_cast<float>(e.sound.volume);music_->play();break;
        }
        case PresentationEvent::Kind::StopMusic:musicPlayback_.reset();musicDescription_={};if(music_){music_->stop();}musicFade_=0;break;
        case PresentationEvent::Kind::FadeMusic:
            rememberFadedMusic_=content_.catalog.contains("music_memory")&&std::find(content_.catalog.at("music_memory").at("fade_cues").begin(),content_.catalog.at("music_memory").at("fade_cues").end(),e.id)!=content_.catalog.at("music_memory").at("fade_cues").end();
            if(music_){musicFadeTotal_=musicFade_=std::max(1,e.duration*60/1000);}break;
        case PresentationEvent::Kind::Animation:effects_.push_back({e.id,e.position,0,true});break;
        case PresentationEvent::Kind::Effect:{
            const auto& definition=content_.media.at("effects").at(e.id);
            if(definition.contains("picture_channel")){
                const int channel=definition.at("picture_channel");
                std::erase_if(effects_,[&](const Effect& old){return !old.animation&&content_.media.at("effects").at(old.id).value("picture_channel",-1)==channel;});
            }
            effects_.push_back({e.id,e.position,0,false,e.actor});break;
        }
        case PresentationEvent::Kind::Cue:cue(e);break;
        case PresentationEvent::Kind::Notice:notice_=e.id;noticeTicks_=e.duration;break;
        }
    }
    if(advanceWorld){
    for(const auto& e:effects_){if(e.animation){
        const auto& animation=content_.media.at("animations").at(e.id);const int rate=animation.at("playback").at("logic_updates_per_frame");
        if(e.age%rate==0){for(const auto& timing:animation.at("timings")){if(timing.at("frame").get<int>()==e.age/rate+1&&!timing.at("sound").is_null()&&!timing.at("sound").at("resource").is_null()){sound(readSound(timing.at("sound")));}}}
    }}
    if(musicFade_>0){music_->setVolume(musicVolume_*--musicFade_/musicFadeTotal_);if(musicFade_==0){music_->stop();if(!rememberFadedMusic_){musicDescription_={};}}}
    black_+=std::clamp(blackTarget_-black_,-12.75f,12.75f);
    for(auto& [id,p]:pictures_){p.phase=(p.phase+8)%256;if(p.remaining>0){--p.remaining;const float t=1.f-static_cast<float>(p.remaining)/p.total;p.position=p.startPosition+(p.targetPosition-p.startPosition)*t;p.zoom=p.startZoom+(p.targetZoom-p.startZoom)*t;p.opacity=p.startOpacity+(p.targetOpacity-p.startOpacity)*t;p.wave=p.startWave+(p.targetWave-p.startWave)*t;
        p.color={static_cast<std::uint8_t>(p.startColor.r+(p.targetColor.r-p.startColor.r)*t),static_cast<std::uint8_t>(p.startColor.g+(p.targetColor.g-p.startColor.g)*t),static_cast<std::uint8_t>(p.startColor.b+(p.targetColor.b-p.startColor.b)*t)};
    }}
    }
}
void CombatPresentation::drawText(sf::RenderTarget& target,const std::string& text,float x,float y,sf::Color color) const{
    const auto world=target.getView();const auto viewport=target.getViewport(world);const float scale=viewport.size.y/world.getSize().y;
    const auto size=target.getSize();target.setView(sf::View(sf::FloatRect({0.f,0.f},{float(size.x),float(size.y)})));
    const auto decoded=decodeUiText(text);sf::Text label(font_,sf::String(decoded),static_cast<unsigned>(std::max(1.f,std::round(12*scale))));
    label.setPosition({viewport.position.x+x*scale,viewport.position.y+y*scale});label.setFillColor(color);target.draw(label);target.setView(world);
}
void CombatPresentation::drawInterface(sf::RenderTarget& target,const GameApplication& scene){
    const auto old=target.getView();auto ui=old;ui.setCenter({160,120});ui.setSize({320,240});target.setView(ui);
    if(auto files=scene.fileMenuView()){
        target.clear(sf::Color(0,0,16));drawUiWindow(target,{{0,0},{320,32}});drawUiLine(target,files->title,{{8,8},{304,16}});
        const int count=int(files->lines.size()),first=std::clamp(files->selection-2,0,std::max(0,count-3));
        for(int n=first;n<std::min(first+3,count);++n){const float y=40+(n-first)*64;drawUiWindow(target,{{0,y},{320,64}});
            if(n==files->selection){drawUiCursor(target,{{4,y+8},{64,16}});}drawUiLine(target,"FILE "+std::to_string(n+1),{{8,y+8},{64,16}});
            auto text=files->lines[n];const bool summary=n<int(files->summaries.size())&&files->summaries[n].has_value();
            if(summary){const auto& meta=*files->summaries[n];const auto stats=text.find(" LV");
                drawUiLine(target,meta.label,{{8,y+24},{296,16}},0,true);
                if(stats!=std::string::npos){drawUiLine(target,text.substr(stats+1),{{8,y+40},{224,16}});}
                // Long original identities occupy the face area; keep their complete label readable.
                if(decodeUiText(meta.label).size()<=15){sf::Sprite face(images_.at(meta.face));face.setTextureRect({{meta.faceIndex%4*48,meta.faceIndex/4*48},{48,48}});face.setPosition({264,y+8});target.draw(face);}
            }else{drawUiLine(target,text,{{8,y+24},{304,16}},3,true);}
        }
        const auto arrow=[&](bool down,float y){windowSkins_.at(systemImage_).drawScrollArrow(target,{152,y},down);};
        if(first>0){arrow(false,32);}if(first+3<count){arrow(true,232);}
        const bool feedback=files->confirmation||(!files->prompt.empty()&&!files->prompt.starts_with("W/S ")&&!files->prompt.starts_with("WASD "));
        if(feedback){
            auto chars=decodeUiText(files->prompt);std::vector<std::string> lines;
            while(!chars.empty()){sf::String part(chars.substr(0,25));auto bytes=part.toUtf8();lines.emplace_back(bytes.begin(),bytes.end());chars.erase(0,std::min<std::size_t>(25,chars.size()));}
            const int visible=files->confirmation?4:7,height=files->confirmation?112:144,top=(240-height)/2;
            drawUiWindow(target,{{0,float(top)},{320,float(height)}});
            const int start=lines.size()>std::size_t(visible)?(uiFrame_/120)%(lines.size()-visible+1):0;
            for(int i=start;i<std::min(int(lines.size()),start+visible);++i){drawUiLine(target,lines[i],{{8,float(top+8+(i-start)*16)},{304,16}});}
            if(files->confirmation){drawUiCursor(target,{{104+float(files->answer)*64,float(top+height-24)},{56,16}});drawUiLine(target,"是",{{112,float(top+height-24)},{48,16}});drawUiLine(target,"否",{{176,float(top+height-24)},{48,16}});}
        }
        target.setView(old);return;
    }
    if(scene.mode()==ApplicationMode::Title){
        sf::Sprite background(images_.at(content_.media.at("ui").at("title").at("background").get<std::string>()));
        const auto brightness=std::uint8_t(scene.titleFadeTicks()*255/35);background.setColor({brightness,brightness,brightness});target.draw(background);
        const auto& labels=content_.media.at("ui").at("title").at("labels");const float height=16+labels.size()*16,top=212-height;
        float width=16;for(const auto& id:labels){float textWidth=0;for(char32_t c:decodeUiText(content_.media.at("terms").at(id.get<std::string>()).at("display"))){textWidth+=messageCharacterWidth(c)*6;}width=std::max(width,textWidth+16);}
        const float left=(320-width)/2;
        windowSkins_.at(systemImage_).drawWindow(target,{{left,top},{width,height}},1.f,true,height*scene.titleOpeningTicks()/8);
        if(scene.titleOpeningTicks()==8){
            drawUiCursor(target,{{left+4,top+8+scene.menuSelection()*16},{width-8,16}});
            int row=0;for(const auto& id:labels){const bool disabled=(row==0&&!content_.catalog.contains("opening"))||(row==1&&!scene.titleLoadEnabled());drawUiLine(target,content_.media.at("terms").at(id.get<std::string>()).at("display"),{{left+8,top+8+row*16},{width-16,16}},disabled?3:0,false,true);++row;}
        }
        if(noticeTicks_>0){drawUiWindow(target,{{0,0},{320,64}});drawUiLine(target,notice_,{{8,8},{304,16}},4,true);}
        target.setView(old);return;
    }
    if(scene.mode()==ApplicationMode::Boundary){
        drawUiWindow(target,{{0,70},{320,100}});
        if(scene.exitRequest()->completed){drawText(target,"F4 本版内容已完成",36,84);drawText(target,"已到达机库前的终点",36,112);drawText(target,"Z 留在此处   X 返回标题",36,140);target.setView(old);return;}
        drawText(target,"已到达本阶段开发边界",36,84);drawText(target,"后续目的地：Map "+std::to_string(scene.exitRequest()->map),36,112);drawText(target,"按 X 返回走廊",36,140);target.setView(old);return;
    }
    if(scene.mode()==ApplicationMode::Equipment){
        if(scene.formalMenu()){drawFormalMenu(target,scene);target.setView(old);return;}
        drawUiWindow(target,{{0,0},{320,240}});
        drawText(target,"装备    "+content_.media.at("ui").at("menu").at("actor_name").get<std::string>()+"    HP "+std::to_string(scene.session().hp()),10,8);
        drawText(target,scene.menuSlot()==1?"主武器  （A/D 切换）":"辅助武器  （A/D 切换）",10,36);
        const int equipped=scene.menuSlot()==1?scene.session().equipment().mainWeapon:scene.session().equipment().auxiliaryWeapon;
        drawText(target,"当前："+(equipped?content_.items.at(equipped).name:std::string("－")),10,58);
        const auto choices=scene.equipmentChoices();int row=0;
        for(int item:choices){if(row==scene.menuSelection()){drawUiCursor(target,{{6,86.f+row*20},{308,18}});}drawText(target,item?content_.items.at(item).name:std::string("卸下"),10,86+row*20);++row;}
        const int item=choices.at(scene.menuSelection());
        if(item){const auto description=decodeUiText(content_.items.at(item).description);std::u32string first=description.substr(0,24);sf::String shown(first);const auto utf8=shown.toUtf8();drawText(target,std::string(utf8.begin(),utf8.end()),10,160);}
        const int gun=scene.session().equipment().mainWeapon;
        if(gun&&content_.items.at(gun).capacity>0){const auto& ammo=content_.items.at(gun);
            drawText(target,"弹匣 "+std::to_string(scene.session().magazine(gun))+" / "+std::to_string(ammo.capacity)+"    备用 "+std::to_string(scene.session().count(ammo.looseItem))+" + "+std::to_string(scene.session().count(ammo.boxItem))+"盒",10,184);
        }
        drawText(target,"W/S 选择  Z 确认  X 返回",10,210);
    }
    target.setView(old);
}
void CombatPresentation::draw(sf::RenderTarget& target,const GameApplication& scene){
    if(scene.fileMenuView()||scene.mode()==ApplicationMode::Title||scene.mode()==ApplicationMode::Equipment||scene.mode()==ApplicationMode::Boundary){drawInterface(target,scene);return;}
    const auto worldView=target.getView();sf::RectangleShape tint(worldView.getSize());tint.setPosition(worldView.getCenter()-worldView.getSize()/2.f);
    const auto channel=[](float value){return static_cast<std::uint8_t>(std::clamp(value==100?255:2*static_cast<int>(value*128/100),0,255));};
    tint.setFillColor({channel(screenTone_[0]),channel(screenTone_[1]),channel(screenTone_[2])});target.draw(tint,sf::BlendMultiply);
    for(auto& e:effects_){
        if(!e.animation){
            const auto& effect=content_.media.at("effects").at(e.id);int index=0,until=0;
            for(int n:effect.at("wait_zero_count_after_stage")){until+=n;if(e.age<until){break;}++index;}
            if(index>=static_cast<int>(effect.at("stage_offsets_px").size())){continue;}
            auto anchor=foot(e.anchor);
            if(effect.at("anchor")=="actor-screen-position-each-stage"){
                anchor=foot(findCharacter(scene.world(),e.actor).motion.position());
                if(!e.clipChecked){
                    const auto screen=anchor-(worldView.getCenter()-worldView.getSize()/2.f)-sf::Vector2f{0,8};
                    const auto& clip=effect.at("start_center_clip_y_px");
                    e.clipped=(screen.x<=0&&screen.y<=0)||screen.y<=clip.at(0).get<float>()||screen.y>=clip.at(1).get<float>();e.clipChecked=true;
                }
                if(e.clipped){continue;}
            }
            const auto& off=effect.at("stage_offsets_px").at(index);
            const std::string resource=effect.contains("stage_images")?effect.at("stage_images").at(index).get<std::string>():effect.at("image").get<std::string>();
            const auto& texture=effect.contains("stage_tone_percent")?tonedImages_.at({resource,effect.at("stage_tone_percent").at(index).get<std::array<int,4>>() }):images_.at(resource);
            sf::Sprite image(texture);auto size=image.getTexture().getSize();image.setOrigin({size.x/2.f,size.y/2.f});
            image.setColor({255,255,255,static_cast<std::uint8_t>(255*(100-effect.value("transparency_percent",0))/100)});
            image.setPosition(anchor+sf::Vector2f(off.at(0).get<float>(),off.at(1).get<float>()));target.draw(image);
        }else{
            const auto& animation=content_.media.at("animations").at(e.id);const int index=e.age/animation.at("playback").at("logic_updates_per_frame").get<int>();if(index>=static_cast<int>(animation.at("frames").size())){continue;}
            for(const auto& cell:animation.at("frames").at(index).at("cells")){
                if(!cell.at("visible").get<bool>()){continue;}sf::Sprite image(images_.at(animation.at("image").get<std::string>()));const auto& r=cell.at("rect_px");const auto size=images_.at(animation.at("image").get<std::string>()).getSize();
                const int x=r.at(0),y=r.at(1),width=std::min(r.at(2).get<int>(),int(size.x)-x),height=std::min(r.at(3).get<int>(),int(size.y)-y);
                // Keep nominal cell origin while clipping the original Animation32 bottom edge.
                if(width<=0||height<=0){continue;}image.setTextureRect({{x,y},{width,height}});image.setOrigin({r.at(2).get<float>()/2,r.at(3).get<float>()/2});const auto& offset=cell.at("offset_px");image.setPosition(foot(e.anchor)+sf::Vector2f(offset.at(0).get<float>(),offset.at(1).get<float>()-12));const float scale=cell.at("zoom_percent").get<float>()/100;image.setScale({scale,scale});auto color=tone(cell.at("tone_percent"));color.a=static_cast<std::uint8_t>(255*(100-cell.at("transparency_percent").get<int>())/100);image.setColor(color);target.draw(image);
            }
        }
    }
    const auto old=target.getView();auto ui=old;ui.setCenter({160,120});ui.setSize({320,240});target.setView(ui);
    drawRain(target,old.getCenter());
    for(const auto& [id,p]:pictures_){if(p.image.empty()){continue;}sf::Sprite image(images_.at(p.image));const auto size=image.getTexture().getSize();image.setOrigin({size.x/2.f,size.y/2.f});image.setPosition(p.mapFixed||p.worldAnchored?p.position-(old.getCenter()-old.getSize()/2.f):p.position);image.setRotation(sf::degrees(p.rotation));image.setScale({p.zoom/100,p.zoom/100});auto color=p.color;color.a=static_cast<std::uint8_t>(std::clamp(p.opacity,0.f,255.f));image.setColor(color);
        if(p.wave==0){target.draw(image);}else{
            // Pinned compatibility model: 32 source-pixel wavelength; 8/256 turn per logic tick.
            const float scale=p.zoom/100;const int height=static_cast<int>(size.y*scale);
            for(int y=0;y<height;++y){
                const int sourceY=std::min(static_cast<int>(size.y)-1,static_cast<int>(y/scale));
                sf::Sprite strip(images_.at(p.image));strip.setTextureRect({{0,sourceY},{static_cast<int>(size.x),1}});strip.setScale({scale,1});strip.setColor(color);
                const float offset=4*scale*p.wave*std::sin(p.phase*6.2831853f/256+y*6.2831853f/(32*scale));
                strip.setPosition({image.getPosition().x-size.x*scale/2+offset,image.getPosition().y-height/2.f+y});target.draw(strip);
            }
        }
    }
    if(flashRemaining_>0){sf::RectangleShape flash({320,240});auto color=screenFlash_;color.a=static_cast<std::uint8_t>(color.a*flashRemaining_/flashDuration_);flash.setFillColor(color);target.draw(flash);}
    if(noticeTicks_>0){drawText(target,notice_,8,222);}
    if(scrollDirection_){drawScrollTransition(target);}
    else if(black_>0){sf::RectangleShape cover({320,240});cover.setFillColor(sf::Color(0,0,0,static_cast<std::uint8_t>(black_)));target.draw(cover);}target.setView(old);
}

std::map<std::string,sf::Color> CombatPresentation::characterFlashes() const{
    std::map<std::string,sf::Color> result;
    for(const auto& [id,f]:flashes_){auto c=f.color;c.a=static_cast<std::uint8_t>(c.a*f.remaining/f.total);result.emplace(id,c);}
    return result;
}

void CombatPresentation::drawBackground(sf::RenderTarget& target){
    if(panorama_.empty()){return;}sf::Sprite image(images_.at(panorama_));image.setPosition(target.getView().getCenter()-target.getView().getSize()/2.f);target.draw(image);
}

// Pinned Player 0de2a9ab game_map.cpp: parallax uses 1/32 pixel units,
// speed +/-n advances -/+2^n units per game update. Camera scroll is half
// speed on looping axes; non-looping images track the available oversize.
void CombatPresentation::drawBackground(sf::RenderTarget& target,const GameApplication& scene){
    if(!scene.hasGame()){return;}
    const auto& dynamic=scene.panorama();
    if(!dynamic&&!panorama_.empty()){drawBackground(target);return;}
    if(!dynamic&&!scene.definition().defaultPanorama){return;}
    const auto& map=scene.definition();const auto& panorama=dynamic?dynamic->definition:*map.defaultPanorama;
    const auto& texture=images_.at(panorama.image);const auto size=texture.getSize();
    const auto viewSize=target.getView().getSize();const auto camera=scene.cameraCenter();
    const sf::Vector2f origin{static_cast<float>(camera.x)-viewSize.x/2.f,static_cast<float>(camera.y)-viewSize.y/2.f};
    const auto axis=[&](int imageSize,int mapSize,float view,float top,bool loop,bool automatic,int speed,int axisIndex){
        if(!loop){
            const float distance=std::max(0.f,mapSize-view);
            const float oversize=std::max(0.f,imageSize-view);
            return distance>0.f?-static_cast<float>(static_cast<int>(std::min(distance,oversize)*top/distance)):0.f;
        }
        const std::int64_t span=static_cast<std::int64_t>(imageSize)*32;
        std::int64_t position=static_cast<std::int64_t>(top*16.f);
        if(dynamic){position+=dynamic->phase[axisIndex];}
        else if(automatic&&speed!=0){
            const int amount=1<<std::abs(speed);
            const auto updates=static_cast<std::int64_t>(scene.world().frame()%span);
            position+=(speed<0?amount:-amount)*updates;
        }
        // Integer division matches GetParallaxX/Y truncation, including slow settings.
        position=(position%span+span)%span;
        int offset=static_cast<int>(-position/32);
        if(offset>0){offset-=imageSize;}
        return static_cast<float>(offset);
    };
    const float x=axis(size.x,map.width*map.tileSize,viewSize.x,origin.x,panorama.loopX,panorama.autoX,panorama.speedX,0);
    const float y=axis(size.y,map.height*map.tileSize,viewSize.y,origin.y,panorama.loopY,panorama.autoY,panorama.speedY,1);
    sf::Sprite image(texture);
    for(float row=y;row<viewSize.y;row+=size.y){
        for(float column=x;column<viewSize.x;column+=size.x){
            image.setPosition(origin+sf::Vector2f{column,row});target.draw(image);
            if(!panorama.loopX){break;}
        }
        if(!panorama.loopY){break;}
    }
}

float CombatPresentation::shakeOffset() const{return shakeTicks_>0?std::sin(shakeAge_*shakeSpeed_*0.1f)*shakeStrength_*2.f:0.f;}

// CN M87 E19 P1 C2 uses rain strength 2. Geometry/lifetime follow pinned
// Player weather.cpp / game_screen.cpp; random samples are remake presentation.
void CombatPresentation::advanceRain(){
    if(weatherType_!=1){return;}
    const auto random=[&](int maximum){return static_cast<int>(rainRandom_()%maximum);};
    if(rain_.empty()){
        for(int i=0;i<100;++i){rain_.push_back({random(320),random(240),random(40)});}
    }
    for(auto& particle:rain_){
        if(particle.life>0){--particle.life;--particle.x;particle.y+=4;}
        else if(random(10)==0){particle={random(320),random(240),12};}
    }
}
void CombatPresentation::drawRain(sf::RenderTarget& target,sf::Vector2f camera){
    if(weatherType_!=1){return;}
    if(!rainCameraOrigin_){rainCameraOrigin_=camera;}
    const auto offset=camera-*rainCameraOrigin_;
    constexpr int counts[]={20,60,100};
    sf::VertexArray pixels(sf::PrimitiveType::Triangles);
    const auto wrap=[](int value,int size){return (value%size+size)%size;};
    for(int i=0;i<std::min(counts[weatherStrength_],static_cast<int>(rain_.size()));++i){
        const auto& p=rain_[i];if(p.life==0||p.life>12){continue;}
        sf::Color color;
        color.r=static_cast<std::uint8_t>(std::clamp(screenTone_[0]*255/100,0.f,255.f));
        color.g=static_cast<std::uint8_t>(std::clamp(screenTone_[1]*255/100,0.f,255.f));
        color.b=static_cast<std::uint8_t>(std::clamp(screenTone_[2]*255/100,0.f,255.f));
        color.a=static_cast<std::uint8_t>((5+weatherStrength_)*p.life);
        for(int row=0;row<24;++row){
            const float x=static_cast<float>(wrap(p.x+5-row/4-static_cast<int>(std::floor(offset.x)),320));
            const float y=static_cast<float>(wrap(p.y+row-static_cast<int>(std::floor(offset.y)),240));
            for(const sf::Vector2f corner:std::array<sf::Vector2f,6>{{{x,y},{x+1,y},{x,y+1},{x+1,y},{x+1,y+1},{x,y+1}}}){pixels.append({corner,color});}
        }
    }
    target.draw(pixels); // One batch; no per-drop texture or audio allocations.
}

// Pinned Player 0de2a9ab transition.cpp ScrollUp/Down: translate the captured
// image into/out of black. The existing finite transition duration remains 20 ticks.
void CombatPresentation::drawScrollTransition(sf::RenderTarget& target){
    if(!scrollCaptured_){
        scrollViewport_=target.getViewport(target.getView());
        if(auto* surface=dynamic_cast<sf::RenderTexture*>(&target)){surface->display();scrollScreen_=surface->getTexture();}
        else if(auto* window=dynamic_cast<sf::RenderWindow*>(&target)){
            if(!scrollScreen_.resize(window->getSize())){throw std::runtime_error("Cannot capture scroll transition");}scrollScreen_.update(*window);
        }else{throw std::runtime_error("Unsupported scroll render target");}
        scrollCaptured_=true;
    }
    sf::RectangleShape cover({320,240});cover.setFillColor(sf::Color::Black);target.draw(cover);
    sf::Sprite screen(scrollScreen_);screen.setTextureRect(scrollViewport_);
    screen.setScale({320.f/scrollViewport_.size.x,240.f/scrollViewport_.size.y});
    const float progress=static_cast<float>(scrollFrame_)/std::max(1,scrollDuration_-1);
    screen.setPosition({0,240*scrollDirection_*(scrollShowing_?progress-1:progress)});target.draw(screen);
}

#include "warehouse_visual.inc"
