#include "title_navigation.h"
#include "combat_presentation.h"
#include "scene_renderer.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#ifdef __APPLE__
#include <mach/mach.h>
#endif

namespace {
using Json=nlohmann::json;
using Clock=std::chrono::steady_clock;
void need(bool ok,const std::string& message){if(!ok){throw std::runtime_error(message);}}
InputFrame key(InputAction action=InputAction::Confirm){InputFrame in;in.buttons[std::size_t(action)]={true,true,false};return in;}
double ms(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
std::uint64_t resident(){
#ifdef __APPLE__
    mach_task_basic_info_data_t info{};mach_msg_type_number_t count=MACH_TASK_BASIC_INFO_COUNT;
    if(task_info(mach_task_self(),MACH_TASK_BASIC_INFO,reinterpret_cast<task_info_t>(&info),&count)==KERN_SUCCESS){return info.resident_size;}
#endif
    return 0;
}
}
int main(int argc,char** argv){try{
    need(argc==4||argc==5||argc==6,"package isolated-save-directory output-json [difficulty] [startup] (slot-01.json must already be an isolated copy)");
    const std::string difficulty=argc>=5?argv[4]:"normal";
    need(difficulty=="easy"||difficulty=="normal"||difficulty=="hard","Unknown profile difficulty");
    const bool startupOnly=argc==6;need(!startupOnly||std::string(argv[5])=="startup","Unknown performance mode");
    Json report={{"scope","real title/public Load; development-entry map variations; bounded animation65 stress fixture, not natural gameplay or native FPS"},{"difficulty",difficulty},{"samples",Json::array()}};
    const auto write=[&]{std::ofstream out(argv[3]);need(bool(out),"Cannot write performance report");out<<report.dump(2)<<'\n';need(bool(out),"Incomplete performance report");};
    const auto record=[&](const std::string& label,Clock::time_point start){report["samples"].push_back({{"phase",label},{"ms",ms(start)},{"resident_bytes",resident()}});write();std::cout<<label<<' '<<report["samples"].back().at("ms")<<" ms\n"<<std::flush;};
    const auto started=Clock::now();auto content=loadCombatContent(argv[1]);record("content_load",started);
    // The same released content serves two map sizes and successive restored sessions.
    for(int map:{193,194,200}){auto e=content.entries.at("post-pickup");e.session.difficulty=difficulty;e.map=map;e.playerTile=map==193?TilePosition{90,7}:map==194?TilePosition{14,12}:TilePosition{14,12};e.positions.clear();e.pages.clear();e.directions.clear();e.through.clear();content.entries["profile-"+std::to_string(map)]=e;}
    auto shared=std::make_shared<SceneSpriteTextures>();std::unique_ptr<SceneRenderer> renderer,preparedRenderer;
    GameApplication game(std::move(content),"title",[](int,int high){return high;},[&](const SceneDefinition& d){auto candidate=std::make_unique<SceneRenderer>(d,shared);preparedRenderer=std::move(candidate);});
    game.configureSaveDirectory(argv[2]);finishTitleOpening(game);sf::Font font;need(font.openFromFile(game.content().maps.begin()->second.fontPath),"Profile font missing");
    const auto mediaStart=Clock::now();auto presentation=std::make_unique<CombatPresentation>(game.content(),font);record("initial_presentation",mediaStart);
    std::unique_ptr<CombatPresentation> preparedPresentation;
    game.configureRestorePresentation([&](const SceneDefinition& d,const SavedPresentation& state){auto nextRenderer=std::make_unique<SceneRenderer>(d,shared);auto next=std::make_unique<CombatPresentation>(game.content(),font);next->prepareRestoredState(state);preparedRenderer=std::move(nextRenderer);preparedPresentation=std::move(next);});
    DialogueRenderer dialogue(font);dialogue.setMedia(presentation->images(),presentation->palette(),&presentation->systemImage());sf::RenderTexture surface({960,720});
    const auto adopt=[&]{if(preparedRenderer){renderer=std::move(preparedRenderer);}if(preparedPresentation){presentation=std::move(preparedPresentation);presentation->activateRestoredState();dialogue.setMedia(presentation->images(),presentation->palette(),&presentation->systemImage());}};
    const auto present=[&](bool advance=true){adopt();presentation->update(game.takePresentation(),advance);game.setPresentationState(presentation->saveState());};
    const auto draw=[&]{const auto center=game.cameraCenter();surface.setView(sf::View({float(center.x)+presentation->shakeOffset(),float(center.y)},{320,240}));surface.clear();if(game.hasGame()){presentation->drawBackground(surface,game);renderer->draw(surface,game.world(),presentation->characterFlashes(),game.playerVisible(),presentation->saturation());}presentation->draw(surface,game);dialogue.draw(surface,game.dialogue().view());dialogue.draw(surface,game.inputPromptView());surface.display();};
    present(false);draw();record("offscreen_title_first_frame_total",started);
    {
        const auto start=Clock::now();game.update(key(InputAction::Up));game.update(key());present(false);need(game.mode()==ApplicationMode::Playing&&game.openingActive(),"Public New Game did not prepare opening");draw();record("first_public_new_game_to_opening_frame",start);
        game.returnToTitle();finishTitleOpening(game);renderer.reset();preparedRenderer.reset();preparedPresentation.reset();present(false);
        shared=std::make_shared<SceneSpriteTextures>(); // Keep the first Load cold to sprite uploads after the independent New Game sample.
    }
    for(int attempt=0;attempt<2;++attempt){
        const auto start=Clock::now();game.returnToTitle();finishTitleOpening(game);renderer.reset();preparedRenderer.reset();preparedPresentation.reset();present(false);game.update(key());present(false);need(game.mode()==ApplicationMode::Loading,"Profile public Load missing");game.update(key());present(false);need(game.mode()==ApplicationMode::Playing,"Profile public Load rejected");need(game.session().difficulty()==difficulty,"Profile Load changed difficulty");draw();record(attempt?"subsequent_public_load_to_frame":"first_public_load_to_frame",start);report["loaded_map"]=game.mapId();
    }
    // Failure at the same restore boundary must retain the active map and presentation.
    const auto goodMap=game.mapId();auto broken=SaveFiles(game.content(),argv[2]).read(1);broken.scene.sprite="absent-profile-graphic";std::string error;
    need(!game.restore(broken,error)&&game.mapId()==goodMap&&!error.empty(),"Rejected profile restore replaced current game");draw();report["failed_restore_preserved_current_game"]=true;
    for(int map:{193,194,200}){const auto start=Clock::now();game.restartDevelopmentEntry("profile-"+std::to_string(map));present(false);need(game.session().difficulty()==difficulty,"Profile map fixture changed difficulty");draw();record("development_map_"+std::to_string(map)+"_to_frame",start);report["map_sizes"][std::to_string(map)]={game.definition().width,game.definition().height};}
    if(!startupOnly){for(int concurrency:{0,1,4}){
        std::vector<double> samples;int submitted=0;
        for(int i=0;i<360;++i){const auto start=Clock::now();game.update({});present();if(concurrency&&i%30==0){std::vector<PresentationEvent> events;for(int n=0;n<concurrency;++n){events.push_back({PresentationEvent::Kind::Animation,"65",{},{14.+n*.5,12},0});++submitted;}presentation->update(events,false);}draw();samples.push_back(ms(start));}
        std::sort(samples.begin(),samples.end());report["frames"][std::to_string(concurrency)]={{"samples",samples.size()},{"animation65_submissions",submitted},{"median_ms",samples[samples.size()/2]},{"p95_ms",samples[samples.size()*95/100]},{"p99_ms",samples[samples.size()*99/100]},{"max_ms",samples.back()},{"resident_bytes",resident()}};
    }
    }
    report["result"]="PASS";write();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
