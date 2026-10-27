#include "title_navigation.h"
#include "combat_presentation.h"
#include "scene_renderer.h"
#include "full_content.h"
#include "g0_natural_driver.h"
#include <chrono>
#include <fstream>
#include <iostream>

namespace {
using Json=nlohmann::json;
using Clock=std::chrono::steady_clock;
namespace fs=std::filesystem;
void need(bool ok,const char* text){if(!ok){throw std::runtime_error(text);}}
InputFrame key(InputAction action=InputAction::Confirm){InputFrame in;in.buttons[std::size_t(action)]={true,true,false};return in;}
double elapsed(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
}
int main(int argc,char** argv){try{
    need(argc==5,"package isolated-output actual-source ordinary|load|story|control|title-font");
    const fs::path root=fs::absolute(argv[2]);need(!fs::exists(root),"Output must be new; source saves are read-only");fs::create_directories(root/"slots");
    fs::copy_file(argv[3],root/"slots/slot-01.json");
    Json report={{"scope","public GameApplication inputs to actual offscreen frame and proven menu/control; excludes OS scheduling and authored dialogue wall time"},{"samples",Json::array()}};
    const auto flush=[&]{std::ofstream out(root/"result.json");out<<report.dump(2)<<'\n';need(bool(out),"Report write failed");};
    const bool story=std::string(argv[4])=="story",titleFont=std::string(argv[4])=="title-font";
    auto content=loadCombatContent(argv[1]);
    if(titleFont){
        // Explicit test-only content variation: the isolated v4 source stands below M151 E11.
        auto message=content.messages.begin()->second;message.inheritDisplay=false;message.inheritFace=false;message.message={};
        message.message.exfontImage="g9-message-exfont";
        message.message.lines={{{},{{MessageTokenKind::Color,{},3},{MessageTokenKind::Text,U"WMi f j 中文",0},{MessageTokenKind::ExFont,{},32}}}};
        content.messages["10-color-file"]=message;
        content.catalog.at("readings").at("manual").at("body").insert(content.catalog.at("readings").at("manual").at("body").begin(),Json{{"kind","message"},{"message","10-color-file"}});
        content.items.at(180).name+=" WMi f j";
        // Old disk control flags are false; restored play enables S1. Keep legacy validation, skip saving only in play.
        for(auto& point:content.catalog.at("save_points")){if(point.at("object")=="m151-e11"){if(!point.contains("absent_flags")){point["absent_flags"]=Json::array();}point["absent_flags"].push_back(1);}}
        for(const auto& variant:content.objects.at("m151-e11").variants){
            content.catalog["flows"]["00-10-number-"+std::to_string(variant.page)]={{"object","m151-e11"},{"page",variant.page},{"trigger","action"},{"runtime_enabled",true},{"steps",Json::array({{{"kind","number_input"},{"digits",4},{"field","access_password"}},{{"kind","finish"}}})}};
        }
    }
    if(story){auto e=content.entries.at("defeated-claimed");e.map=19;e.playerTile={19,15};e.direction=Direction::Down;e.positions.clear();e.pages.clear();e.directions.clear();e.through.clear();initializeFullSession(content,e.session);GameSession initial(content.items,e.session,content.facts);for(int flag:{19,3,17,10,45,57}){initial.setFlag(flag,false);}initial.setFlag(1,true);initial.setFlag(18,true);e.session=initial.exportState();content.entries["07-contact"]=e;}
    auto shared=std::make_shared<SceneSpriteTextures>();std::unique_ptr<SceneRenderer> renderer,preparedRenderer;
    GameApplication game(std::move(content),story?"07-contact":"title",g0natural::safe,[&](const SceneDefinition& d){preparedRenderer=std::make_unique<SceneRenderer>(d,shared);});game.configureSaveDirectory(root/"slots");finishTitleOpening(game);
    sf::Font font;need(font.openFromFile(game.content().maps.begin()->second.fontPath),"Font missing");
    auto media=std::make_unique<CombatPresentation>(game.content(),font);std::unique_ptr<CombatPresentation> preparedMedia;
    game.configureRestorePresentation([&](const SceneDefinition& d,const SavedPresentation& state){auto nextRenderer=std::make_unique<SceneRenderer>(d,shared);auto next=std::make_unique<CombatPresentation>(game.content(),font);next->prepareRestoredState(state);preparedRenderer=std::move(nextRenderer);preparedMedia=std::move(next);});
    DialogueRenderer dialogue(font);sf::RenderTexture surface({960,720});
    const auto tick=[&](InputFrame in=InputFrame{}){game.update(in);if(preparedRenderer){renderer=std::move(preparedRenderer);}if(preparedMedia){media=std::move(preparedMedia);media->activateRestoredState();}media->update(game.takePresentation(),false);game.setPresentationState(media->saveState());};
    const auto draw=[&](const std::string& image=""){dialogue.setMedia(media->images(),media->palette(),&media->systemImage());const auto center=game.cameraCenter();surface.setView(sf::View({float(center.x),float(center.y)},{320,240}));surface.clear();if(game.hasGame()){media->drawBackground(surface,game);need(bool(renderer),"Restored renderer missing");renderer->draw(surface,game.world(),media->characterFlashes(),game.playerVisible(),media->saturation());}media->draw(surface,game);dialogue.draw(surface,game.dialogue().view());dialogue.draw(surface,game.inputPromptView());surface.display();if(!image.empty()){need(surface.getTexture().copyToImage().saveToFile(root/(image+".png")),"Frame write failed");}};
    media->update(game.takePresentation(),false);if(preparedRenderer){renderer=std::move(preparedRenderer);}draw(story?"story-entry":"title");
    if(titleFont){
        need(game.mode()==ApplicationMode::Title&&game.menuSelection()==1,"Isolated old save must enable default Load");draw("title-ready");
        tick(key(InputAction::Up));tick(key());need(game.hasGame()&&game.mapId()==2&&game.openingActive(),"Public NewGame short entry missing");draw("newgame-short");
        game.returnToTitle();finishTitleOpening(game);tick();draw("returned-title");
        tick(key());need(game.mode()==ApplicationMode::Loading,"Title Load list missing");draw("old-files");tick(key(InputAction::Cancel));need(game.mode()==ApplicationMode::Title&&game.titleOpeningTicks()==8,"Load cancel must restore ready title");
        tick(key());tick(key());need(game.mode()==ApplicationMode::Playing&&game.mapId()==151,"Public v4 Load missing");draw("old-loaded");
        const auto loaded=SaveFiles(game.content(),root/"slots").read(1);const auto restored=game.session().exportState();need(restored.hp==loaded.session.hp&&restored.inventory==loaded.session.inventory&&restored.magazines==loaded.session.magazines&&restored.skills==loaded.session.skills&&restored.saveCount==loaded.session.saveCount,"Legacy Load changed owned actor state");
        for(int n=0;n<1000&&(game.foreground()!=ForegroundKind::None||game.dialogue().active());++n){tick(n%8==0?key():InputFrame{});}
        tick(key(InputAction::Cancel));need(game.formalMenu(),"Legacy Load menu control missing");draw("menu-root");tick(key());need(game.menuView().page==MenuPage::Items,"Items missing");
        for(int n=0;n<500&&game.menuView().rows.at(game.menuView().selection).id!=180;++n){tick(key(InputAction::Right));}
        need(game.menuView().rows.at(game.menuView().selection).id==180,"Held original file missing");draw("mixed-items");tick(key());need(game.mode()==ApplicationMode::Reading,"File public selection missing");
        bool color=false;int pages=0;MessageRequestId last{};
        for(int n=0;n<9000&&game.mode()==ApplicationMode::Reading;++n){
            const auto& view=game.dialogue().view();
            const bool ready=view.phase==DialoguePhase::AwaitingInput;
            if(ready&&view.request!=last){last=view.request;++pages;if(!color){need(view.lines.front().starts_with(U"WMi f j 中文")&&!view.exfontIndices.empty()&&view.exfontIndices.front().back()==32,"Colored ExFont file fixture missing");need(view.colors.front().front()==3,"File color lost");draw("colored-exfont-file");color=true;}}
            tick(ready?key():InputFrame{});
        }
        need(color&&pages==13&&game.formalMenu(),"Fixture plus twelve actual manual pages did not return menu");draw("file-return");
        tick(key(InputAction::Cancel));for(int n=0;n<1000&&game.mode()!=ApplicationMode::Playing;++n){tick(n%8==0?key():InputFrame{});}need(game.mode()==ApplicationMode::Playing,"File close did not restore play");
        tick(key());for(int n=0;n<200&&!game.inputPromptView().cursor;++n){tick();}
        need(game.inputPromptView().cursor&&game.inputPromptView().cursor->numeric,"Explicit scene numeric fixture missing");draw("number");
        tick(key(InputAction::Up));tick(key(InputAction::Left));const auto numeric=game.inputPromptView().lines;tick(key(InputAction::Cancel));need(game.inputPromptView().lines==numeric&&game.inputPromptView().cursor,"Original numeric Cancel must not submit");draw("number-cancel");
        int expected=0;for(auto glyph:numeric.front()){expected=expected*10+int(glyph-U'0');}tick(key());for(int n=0;n<100&&game.foreground()!=ForegroundKind::None;++n){tick();}need(game.session().variable(44)==expected,"Numeric result not committed through scene flow");
        tick(key(InputAction::Cancel));need(game.formalMenu(),"Numeric completion did not restore menu");tick(key(InputAction::Cancel));need(game.mode()==ApplicationMode::Playing,"Final menu Cancel did not restore play");draw("cancel-return");
        std::ifstream source(argv[3],std::ios::binary),copy(root/"slots/slot-01.json",std::ios::binary);need(std::string(std::istreambuf_iterator<char>(source),{})==std::string(std::istreambuf_iterator<char>(copy),{}),"Isolated old source bytes changed");
        report["result"]="PASS";report["manual_pages"]=pages-1;report["numeric_submitted"]=expected;
        report["scope"]="Same complete package/Application public inputs and render path; returnToTitle facade ends only the short NewGame probe; subsequent Load/menu/file/number/cancel use public inputs. No OS/manual-input claim.";
        report["fixture"]="Read-only v4 save copy from07 atM151; in-memory item180 name suffix and one colored ExFont page prepended to actual twelve-page manual; existing front E11 save binding skips S1-enabled play while preserving v4 disk validation, and its action fixture requests original4-digit access_password input. Original package/source save untouched.";
        flush();std::cout<<report.dump(2)<<'\n';return 0;
    }
    if(story){
        int ticks=0;const auto advance=[&](InputFrame in=InputFrame{}){tick(in);++ticks;need(ticks<40000,"Bounded story route exceeded limit");need(game.session().hp()>0,"Story route actor died");};
        for(int n=0;n<40&&game.foreground()==ForegroundKind::None;++n){InputFrame in;in.movement=Direction::Down;advance(in);}need(game.foreground()!=ForegroundKind::None,"Actual M19 walk contact missing");
        for(int n=0;n<3000&&(!game.dialogue().active()||game.world().player().motion.moving());++n){advance();}need(game.dialogue().active(),"Contact dialogue missing");for(int n=0;n<40;++n){advance();}
        int changes=0,last=game.world().player().animation.column();for(int n=0;n<120;++n){advance();int now=game.world().player().animation.column();changes+=now!=last;last=now;}need(changes==0&&last==1,"Walk story still steps in place");draw("walk-dialogue");
        sf::RenderWindow native(sf::VideoMode({640,480}),"Nobi 07 isolated model-driven story");const auto nativeFrame=[&](const std::string& name){for(int frame=0;frame<4;++frame){while(native.pollEvent()){}native.clear();sf::Sprite image(surface.getTexture());image.setScale({2.f/3,2.f/3});native.draw(image);native.display();}sf::Texture pixels;need(pixels.resize(native.getSize()),"Native capture allocation");pixels.update(native);need(pixels.copyToImage().saveToFile(root/("native-"+name+".png")),"Native capture write");};nativeFrame("walk-dialogue");
        for(int n=0;n<12000&&(game.foreground()!=ForegroundKind::None||game.dialogue().active());++n){advance(n%8==0?key():InputFrame{});}need(game.foreground()==ForegroundKind::None&&game.session().flag(57)&&!game.world().player().animation.paused(),"Story did not release animation/control");
        advance(key(InputAction::Cancel));need(game.mode()==ApplicationMode::Equipment,"Post-story menu missing");draw("post-story-menu");nativeFrame("post-story-menu");advance(key(InputAction::Cancel));
        const auto settle=[&]{for(int n=0;n<1200&&(game.foreground()!=ForegroundKind::None||game.dialogue().active()||game.world().player().motion.moving());++n){advance(n%8==0?key():InputFrame{});}need(game.foreground()==ForegroundKind::None,"Door continuation stalled");};
        const auto walk=[&](TilePosition target){for(int n=0;n<5000;++n){if(!game.world().player().motion.moving()&&game.world().player().motion.tile()==target){return;}InputFrame in;if(!game.world().player().motion.moving()){in.movement=g0natural::next(game.world(),target);need(in.movement.has_value(),"No legal story return route");}advance(in);}throw std::runtime_error("Story return walk stalled");};
        for(const char* id:{"m19-e11","m18-e8","m13-e4","m15-e6"}){auto actor=findCharacter(game.world(),id);bool used=false;if(actor.layer!=1){walk(actor.motion.tile());settle();advance(key());used=true;}else{for(Direction side:{Direction::Down,Direction::Left,Direction::Right,Direction::Up}){auto from=adjacentTile(actor.motion.tile(),side);if(game.world().player().motion.tile()!=from&&!g0natural::next(game.world(),from)){continue;}walk(from);InputFrame face;face.movement=Direction((int(side)+2)%4);advance(face);settle();advance(key());used=true;break;}}need(used,"No legal source-door approach");settle();report["return_maps"].push_back(game.mapId());}
        need(game.mapId()==16,"Story route did not return to original save room");walk({10,21});{InputFrame face;face.movement=Direction::Up;advance(face);}settle();advance(key());for(int n=0;n<2000&&!game.fileMenuView();++n){advance(n%8==0?key():InputFrame{});}need(game.fileMenuView().has_value(),"Story route original Save choice missing");draw("story-save-choice");nativeFrame("save-choice");advance(key(InputAction::Down));advance(key());need(game.mode()==ApplicationMode::Playing,"Story route Save failed");settle();advance(key(InputAction::Cancel));need(game.mode()==ApplicationMode::Equipment,"Story Save did not release menu");draw("story-save-control");nativeFrame("save-control");native.close();report["initial_fixture"]="declared M19 early state; subsequent contact, doors, choices, menu and ordinary save use only public inputs";report["stationary_column_changes"]=changes;report["ticks"]=ticks;report["saved_kind"]=SaveFiles(game.content(),root/"slots").read(2).continuation.kind;report["result"]="PASS";flush();std::cout<<report.dump(2)<<'\n';return 0;
    }
    const auto original=SaveFiles(game.content(),root/"slots").read(1);report["source_kind"]=original.continuation.kind;report["source_map"]=original.scene.map;
    const std::string mode=argv[4];const bool ordinary=mode=="ordinary",control=mode=="control";need(ordinary||control||mode=="load","Unknown mode");
    for(int run=0;run<(ordinary?3:1);++run){
        game.returnToTitle();finishTitleOpening(game);tick();const auto loadStart=Clock::now();tick(key());need(game.mode()==ApplicationMode::Loading,"Public file list missing");const auto listMs=elapsed(loadStart);tick(key());need(game.mode()==ApplicationMode::Playing,"Public Load rejected");draw();Json sample={{"list_ms",listMs},{"load_list_confirm_to_frame_ms",elapsed(loadStart)}};
        need(game.mapId()==original.scene.map&&game.session().saveCount()==original.session.saveCount,"Load changed map/count");const auto restored=game.session().exportState();need(restored.hp==original.session.hp&&restored.inventory==original.session.inventory&&restored.magazines==original.session.magazines&&restored.skills==original.session.skills,"Load changed owned actor/inventory state");
        if(!ordinary&&!control){draw("boundary-load");report["boundary_candidate_installed"]=true;report["samples"].push_back(sample);break;}
        for(int n=0;n<1000&&(game.foreground()!=ForegroundKind::None||game.dialogue().active());++n){tick(n%8==0?key():InputFrame{});}
        tick(key(InputAction::Cancel));need(game.mode()==ApplicationMode::Equipment,"Load did not restore menu controls");draw("menu");sample["load_to_menu_control_ms"]=elapsed(loadStart);tick(key(InputAction::Cancel));need(game.mode()==ApplicationMode::Playing,"Menu did not close");
        if(control){report["independent_control_restored"]=true;report["samples"].push_back(sample);break;}
        // Both real ordinary source samples stand facing their original save point.
        tick(key());for(int n=0;n<2000&&!game.fileMenuView();++n){tick(n%8==0?key():InputFrame{});}need(game.fileMenuView().has_value()&&game.mode()==ApplicationMode::Saving,"Original save point file UI missing");draw("save-files");
        tick(key(InputAction::Down));need(game.fileMenuView()->selection==1,"Isolated destination slot missing");
        if(run==0){const auto before=game.session().saveCount();tick(key(InputAction::Cancel));need(game.mode()==ApplicationMode::Playing&&game.session().saveCount()==before,"Save cancellation changed count");tick(key());for(int n=0;n<2000&&!game.fileMenuView();++n){tick(n%8==0?key():InputFrame{});}need(game.fileMenuView().has_value(),"Save retry missing");tick(key(InputAction::Down));}
        const int before=game.session().saveCount();const auto saveStart=Clock::now();tick(key());if(game.fileMenuView()&&game.fileMenuView()->confirmation){if(game.fileMenuView()->answer){tick(key(InputAction::Up));}tick(key());}need(game.mode()==ApplicationMode::Playing,"Save transaction not committed");
        for(int n=0;n<1000&&(game.foreground()!=ForegroundKind::None||game.dialogue().active());++n){tick(n%8==0?key():InputFrame{});}draw();tick(key(InputAction::Cancel));need(game.mode()==ApplicationMode::Equipment,"Save did not return control");draw();sample["save_confirm_transaction_to_frame_menu_ms"]=elapsed(saveStart);need(game.session().saveCount()==before+1,"Save count did not increment exactly once");tick(key(InputAction::Cancel));
        const auto disk=SaveFiles(game.content(),root/"slots").read(2);need(disk.session.saveCount==before+1&&disk.continuation.kind=="ordinary","Committed disk/count differs");sample["saved_count"]=disk.session.saveCount;report["samples"].push_back(sample);flush();
    }
    report["result"]="PASS";flush();std::cout<<report.dump(2)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
