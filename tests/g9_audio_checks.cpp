#include "title_navigation.h"
#include "combat_presentation.h"
#include "content_hash.h"
#include "g0_natural_driver.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void need(bool value,const char* message){if(!value){throw std::runtime_error(message);}}
void firstPass(CombatPresentation& presentation){
    for(int n=0;n<300&&!presentation.musicFirstPass();++n){std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    need(presentation.musicFirstPass().has_value(),"Silent PCM never reached its actual first loop");
}
void check(const std::filesystem::path& output,const std::filesystem::path& fontPath){
    std::filesystem::create_directories(output);
    sf::Image pixel({1,1},sf::Color::White);need(pixel.saveToFile(output/"pixel.png"),"Cannot create isolated pixel");
    const std::vector<std::int16_t> silence(4410,0);sf::SoundBuffer pcm;
    need(pcm.loadFromSamples(silence.data(),silence.size(),1,44100,{sf::SoundChannel::Mono}),"Cannot create silent PCM");
    need(pcm.saveToFile(output/"silent.wav"),"Cannot save silent PCM");
    using Json=nlohmann::json;
    CombatContent c;c.directory=output;
    c.media={{"images",{{"system",{{"file","pixel.png"},{"size_px",{1,1}}}}}},
             {"ui",{{"menu",{{"system_image","system"}}}}},
             {"effects",Json::object()},{"audio",Json::object()}};
    for(const char* resource:{"bgm-first","bgm-second"}){
        c.media["audio"][resource]={{"file","silent.wav"},{"loop_samples",{{"start",0},{"end",silence.size()}}},{"initial_sample_compatibility",0}};
    }
    c.catalog["music_memory"]={{"fade_cues",{"remembered-fade"}}};
    sf::Font font;need(font.openFromFile(fontPath),"Cannot load test font");CombatPresentation p(c,font);
    PresentationEvent play{PresentationEvent::Kind::Music,"map-entry",{"bgm-first",0,100,50},{},0};
    // The existing completion token observes continuity without exposing a playback cursor.
    play.musicPlayback=MusicPlaybackIdentity{100,1,"bgm-first"};
    p.update({play},false);firstPass(p);
    play.sound.balance=25;p.update({play},false);
    need(p.musicPlaying()&&p.musicFirstPass().has_value()&&p.saveState().music==play.sound,"Same playing track restarted or lost parameter update");

    const PresentationEvent fade{PresentationEvent::Kind::FadeMusic,"remembered-fade",{},{},1000};
    p.update({fade},false);p.update({play},false);
    need(p.musicPlaying()&&!p.musicFirstPass(),"Same track during fade reused the old playback position");
    firstPass(p);
    p.update({fade},false);for(int n=0;n<60;++n){p.update({});}
    need(!p.musicPlaying()&&p.saveState().music==play.sound,"Completed remembered fade lost its named music");
    p.update({play},false);need(p.musicPlaying(),"Same named track did not replay after completed fade");

    auto other=play;other.sound.resource="bgm-second";other.musicPlayback=MusicPlaybackIdentity{100,2,"bgm-second"};
    p.update({other},false);need(p.musicPlaying()&&!p.musicFirstPass()&&p.saveState().music==other.sound,"Different track reused old playback");
    c.media["audio"]["bgm-first"]["file"]="missing.wav";
    bool rejected=false;try{p.update({play},false);}catch(const std::exception&){rejected=true;}
    need(rejected&&p.musicPlaying()&&p.saveState().music==other.sound,"Rejected replacement destroyed the current track");
    p.update({{PresentationEvent::Kind::SceneChanged,{},{},{},0}},false);
    need(!p.musicFirstPass(),"Departed scene retained music feedback");

    c.media["audio"]["bgm-first"]["file"]="silent.wav";
    const auto saved=p.saveState();CombatPresentation restored(c,font);
    restored.prepareRestoredState(saved);need(!restored.musicPlaying()&&restored.saveState()==saved,"Preparation played music or changed stored values");
    restored.activateRestoredState();need(restored.musicPlaying()&&restored.saveState()==saved&&!restored.musicFirstPass(),"Restored music did not rebuild stored values without old feedback");
    std::cout<<"G9 audio same-track continuity/fade restart/replay/replacement rejection/restore values PASS\n";
}
std::string bytes(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);need(bool(in),"Missing isolated/source audio sample");return {std::istreambuf_iterator<char>(in),{}};}
struct AudioRun{
    GameApplication& game;sf::Font font;std::unique_ptr<CombatPresentation> active,staged;
    int sceneChanges{},musicCommands{},soundCommands{};
    explicit AudioRun(GameApplication& g):game(g){
        need(font.openFromFile(game.content().maps.begin()->second.fontPath),"Missing final font");
        active=std::make_unique<CombatPresentation>(game.content(),font);
        game.configureRestorePresentation([&](const SceneDefinition&,const SavedPresentation& state){
            auto candidate=std::make_unique<CombatPresentation>(game.content(),font);candidate->prepareRestoredState(state);
            need(!candidate->musicPlaying(),"Load candidate played before adoption");staged=std::move(candidate);
        });
    }
    void step(InputFrame input={}){
        game.update(input);if(staged){active=std::move(staged);active->activateRestoredState();}
        auto events=game.takePresentation();for(const auto& event:events){sceneChanges+=event.kind==PresentationEvent::Kind::SceneChanged;musicCommands+=event.kind==PresentationEvent::Kind::Music;soundCommands+=event.kind==PresentationEvent::Kind::Sound;}
        active->update(events);game.setPresentationState(active->saveState());
    }
    void settle(){for(int n=0;n<6000;++n){if(game.mode()!=ApplicationMode::Playing){return;}if(game.foreground()==ForegroundKind::None&&!game.dialogue().active()&&!game.world().player().motion.moving()){step();if(game.foreground()==ForegroundKind::None){return;}}else{step(n%8==0?g0natural::key():InputFrame{});}}throw std::runtime_error("Audio composition scene did not settle");}
    void walk(TilePosition goal){const int map=game.mapId();for(int n=0;n<6000;++n){need(game.mapId()==map,"Audio path changed map before target");if(game.foreground()!=ForegroundKind::None||game.dialogue().active()||game.world().player().motion.moving()){step(n%8==0?g0natural::key():InputFrame{});continue;}if(game.world().player().motion.tile()==goal){return;}InputFrame input;input.movement=g0natural::next(game.world(),goal);need(input.movement.has_value(),"No public audio composition path");step(input);}throw std::runtime_error("Audio composition path stalled");}
    void move(Direction direction){InputFrame input;input.movement=direction;step(input);settle();}
    void object(const std::string& id){
        const auto actor=findCharacter(game.world(),id);const auto tile=actor.motion.tile();
        if(actor.layer!=1){walk(tile);settle();step(g0natural::key());settle();return;}
        for(Direction side:{Direction::Down,Direction::Left,Direction::Right,Direction::Up}){const auto from=adjacentTile(tile,side);if(game.world().player().motion.tile()!=from&&!g0natural::next(game.world(),from)){continue;}walk(from);move(static_cast<Direction>((int(side)+2)%4));step(g0natural::key());settle();return;}throw std::runtime_error("No public audio door adjacency");
    }
    SaveSnapshot load(const std::filesystem::path& source,const std::filesystem::path& slots){
        std::filesystem::create_directories(slots);const auto raw=bytes(source);std::ofstream(slots/"slot-01.json",std::ios::binary)<<raw;
        const auto saved=decodeSave(game.content(),raw);game.configureSaveDirectory(slots);finishTitleOpening(game);game.returnToTitle();finishTitleOpening(game);step();step(g0natural::key());
        need(game.mode()==ApplicationMode::Loading,"Public audio Load menu missing");step(g0natural::key());
        need(game.mode()==ApplicationMode::Playing&&game.session().difficulty()==saved.session.difficulty,"Public audio Load rejected source identity");
        need(active->saveState().music==saved.presentation.music&&active->musicPlaying()==!saved.presentation.music.resource.empty(),"Load replaced stored music values or failed actual playback");
        need(!active->musicFirstPass()&&!active->musicTicks(),"Ordinary Load inherited old monitored music feedback");
        need(bytes(source)==raw,"Audio check changed its source save");settle();return saved;
    }
};
void finalCheck(const std::filesystem::path& package,const std::filesystem::path& output,const std::filesystem::path& sources,bool loadOnly){
    using Json=nlohmann::json;std::filesystem::create_directories(output);
    need(contentSha256(bytes(package))=="79564718bb09133bda39383f6d432ed5ed611963df3a6477da6c718e4271b7ca","Changed frozen final audio candidate");
    auto content=loadCombatContent(package);GameApplication game(std::move(content),"title",g0natural::safe);AudioRun run(game);const auto& c=game.content();
    const auto original=loadOnly?Json{}:Json::parse(bytes(sources));Json report={{"kind",loadOnly?"independent-process public Load of audio-composed ordinary saves":"actual E/N/H ordinary sources; controlled subsequent public audio/save paths, not new-game natural completion"},{"package_sha256",contentSha256(bytes(package))},{"global_volume",0},{"listened",false},{"difficulties",Json::array()}};
    sf::Listener::setGlobalVolume(0);
    for(const std::string difficulty:{"easy","normal","hard"}){
        const auto root=output/difficulty;const std::filesystem::path source=loadOnly?root/"saved/slot-01.json":std::filesystem::path(original.at(difficulty).get<std::string>());
        const auto before=run.load(source,root/(loadOnly?"independent-slots":"source-slots"));
        need(game.mapId()==151&&game.session().difficulty()==difficulty,"Audio source is not the named actual M151 difficulty");
        if(loadOnly){
            need(!before.presentation.music.resource.empty()&&run.active->musicPlaying()&&run.active->saveState().music==before.presentation.music,"Independent audio save lost named playback");
            report["difficulties"].push_back({{"difficulty",difficulty},{"music",before.presentation.music.resource},{"pure_values_preserved",true},{"actual_Playing",true},{"save_count",game.session().saveCount()}});continue;
        }
        // Actual source E/H drivers may have stored silence; map re-entry now runs its real music cue.
        run.object("m151-e8");need(game.mapId()==155,"Audio public exit failed");run.object("m155-e2");need(game.mapId()==151,"Audio public return failed");
        const auto music=run.active->saveState().music;need(!music.resource.empty()&&run.active->musicPlaying(),"Real M151 re-entry did not play its selected BGM");
        run.step(g0natural::key(InputAction::Cancel));need(game.mode()==ApplicationMode::Equipment,"Audio menu did not open");
        InputBuffer input;input.press(InputAction::Confirm);input.setFocused(false);
        for(int n=0;n<12;++n){run.step(input.consume());need(run.active->musicPlaying()&&run.active->saveState().music==music,"Menu/unfocused update stopped or changed BGM");}
        input.setFocused(true);run.step(input.consume());need(game.mode()==ApplicationMode::Equipment,"Focus restoration replayed stale confirmation");run.step(g0natural::key(InputAction::Cancel));need(game.mode()==ApplicationMode::Playing,"Audio menu did not close");
        PresentationEvent same{PresentationEvent::Kind::Music,"map-entry",music};run.active->update({same},false);need(run.active->musicPlaying()&&run.active->saveState().music==music,"Actual same resource did not remain playing");
        std::vector<PresentationEvent> pair;for(const auto& [id,cueDefinition]:c.cues){const auto& cue=*cueDefinition;if(cue.at("kind")=="sound"&&!cue.at("sound").is_null()&&!cue.at("sound").at("resource").is_null()){auto sound=readSound(cue.at("sound"));if(pair.empty()||pair.front().sound.resource!=sound.resource){pair.push_back({PresentationEvent::Kind::Sound,id,sound});if(pair.size()==2){break;}}}}
        need(pair.size()==2,"Missing representative two legal SE");run.active->update(pair,false);need(run.active->musicPlaying()&&run.active->saveState().music==music,"Same-tick SE disturbed current BGM");
        run.active->update({{PresentationEvent::Kind::FadeMusic,"m151-e17-p1-c185",{},{},1000}},false);for(int n=0;n<60;++n){run.active->update({});}
        need(!run.active->musicPlaying()&&run.active->saveState().music==music,"Final remembered fade did not finish or lost stored name");run.active->update({same},false);need(run.active->musicPlaying(),"Final remembered same track failed replay");game.setPresentationState(run.active->saveState());
        const auto savedDir=root/"saved";std::filesystem::create_directories(savedDir);game.configureSaveDirectory(savedDir);finishTitleOpening(game);run.walk({8,7});run.move(Direction::Up);run.step(g0natural::key());need(game.mode()==ApplicationMode::Saving,"Final audio ordinary save unavailable");
        for(int n=0;n<200;++n){run.step();}run.step(g0natural::key());need(game.fileMenuView().has_value(),"Final audio ordinary slots missing");run.step(g0natural::key());
        if(auto view=game.fileMenuView();view&&view->confirmation){if(view->answer){run.step(g0natural::key(InputAction::Up));}run.step(g0natural::key());}
        need(game.mode()==ApplicationMode::Playing,"Final audio ordinary save failed");
        const auto disk=decodeSave(c,bytes(savedDir/"slot-01.json"));need(disk.presentation.music==music&&disk.session.difficulty==difficulty&&disk.continuation.kind=="ordinary","Actual ordinary disk lost BGM pure values");
        report["difficulties"].push_back({{"difficulty",difficulty},{"source",source.string()},{"source_sha256",contentSha256(bytes(source))},{"source_music",before.presentation.music.resource},{"reentry_music",music.resource},{"two_SE",{pair[0].sound.resource,pair[1].sound.resource}},{"two_SE_evidence","one actual update; source owns one sf::Sound per event; no acoustic/mix capture"},{"saved_sha256",contentSha256(bytes(savedDir/"slot-01.json"))},{"map_changes",run.sceneChanges},{"music_commands",run.musicCommands},{"source_unchanged",true}});
        std::cout<<"FINAL_AUDIO "<<difficulty<<" public151/155/menu/focus/twoSE/fade/replay/ordinary save PASS\n";
    }
    std::ofstream(output/(loadOnly?"independent-load.json":"composition.json"))<<report.dump(2)<<'\n';std::cout<<(loadOnly?"Independent audio Load":"Final audio composition")<<" PASS; silent automatic playback only, listening/original/Windows unverified\n";
}
}
int main(int argc,char** argv){try{
    if(argc>=4&&std::string(argv[1])=="--composition"){need(argc==5,"--composition frozen-package output sources-json");finalCheck(argv[2],argv[3],argv[4],false);}
    else if(argc==4&&std::string(argv[1])=="--load"){finalCheck(argv[2],argv[3],{},true);}
    else{need(argc==3,"Expected isolated-output font-file");check(argv[1],argv[2]);}return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
