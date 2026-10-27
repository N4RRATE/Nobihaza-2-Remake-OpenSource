#include "combat_presentation.h"
#include "g8_continue_natural_driver.h"
#include "content_hash.h"
#include <fstream>
#include <iostream>

namespace {
using Json=nlohmann::json;
namespace fs=std::filesystem;
Json json(const fs::path& path){std::ifstream in(path);need(bool(in),"Missing source: "+path.string());return Json::parse(in);}
// Only immutable original UI resources are uploaded; gameplay still uses the complete package.
CombatContent uiContent(const CombatContent& c){
    auto ui=c;ui.media["audio"]=Json::object();ui.media["effects"]=Json::object();
    const std::string face=c.catalog.at("menu").at("actor_face").at("image"),title=c.media.at("ui").at("title").at("background");
    std::set<std::string> faces{face};for(const auto& [id,message]:c.messages){faces.insert(message.message.faceImage);faces.insert(message.message.exfontImage);}
    for(auto it=ui.media["images"].begin();it!=ui.media["images"].end();){
        if(!faces.contains(it.key())&&it.key()!=title&&it.value().at("size_px")!=Json::array({160,80})){it=ui.media["images"].erase(it);}else{++it;}
    }return ui;
}
struct Ui {
    CombatContent content;sf::Font font;std::unique_ptr<CombatPresentation> active,prepared;
    std::unique_ptr<DialogueRenderer> text;sf::RenderTexture surface{{320,240}};
    explicit Ui(const CombatContent& c):content(uiContent(c)){
        need(font.openFromFile(c.maps.begin()->second.fontPath),"Missing actual font");reset();
    }
    void reset(){active=std::make_unique<CombatPresentation>(content,font);bind();}
    void bind(){text=std::make_unique<DialogueRenderer>(font);text->setMedia(active->images(),active->palette(),&active->systemImage());}
    void events(const std::vector<PresentationEvent>& input){std::vector<PresentationEvent> events;for(const auto& e:input){if(e.kind==PresentationEvent::Kind::SystemGraphic||e.kind==PresentationEvent::Kind::ResetGame){events.push_back(e);}}active->update(events,false);}
    sf::Image frame(const DialogueView& view){surface.clear({18,39,61});text->draw(surface,view);surface.display();return surface.getTexture().copyToImage();}
    void check(const fs::path& out,const std::string& stage,bool blue){
        MessageDefinition definition;definition.lines={{{},{{MessageTokenKind::Text,U"Continue UI",0}}}};
        Dialogue dialogue;dialogue.request(definition);for(int n=0;n<1024&&dialogue.view().phase!=DialoguePhase::AwaitingInput;++n){dialogue.update(false,false);}
        need(dialogue.view().phase==DialoguePhase::AwaitingInput,"Probe reveal failed");
        const auto image=frame(dialogue.view());need(image.saveToFile(out/(stage+"-dialogue.png")),"Cannot export actual dialogue pixels");
        // Independent fixed-Player stretch oracle: 320x80 window samples source32x32.
        const auto source=active->images().at(content.media.at("ui").at("menu").at("system_image").get<std::string>()).copyToImage();
        if(blue){need(image.getPixel({200,220})==source.getPixel({20,24}),"Continue did not restore original blue System6 background pixels");}
        auto transparent=dialogue.view();transparent.display.transparent=true;const auto clear=frame(transparent);
        need(clear.getPixel({200,220})==sf::Color(18,39,61)&&clear.getPixel({2,162})==sf::Color(18,39,61),"Transparent original window gained a blue background/frame");
        InputPrompt number;number.request(NumberPrompt{4,389},1);const auto numeric=frame(number.view());
        need(numeric.saveToFile(out/(stage+"-number.png")),"Cannot export number pixels");
        if(blue){need(numeric.getPixel({200,220})==source.getPixel({20,24}),"Number window lost restored blue skin");}
    }
    void menu(const GameApplication& game,const fs::path& out,const std::string& stage){
        need(game.formalMenu(),"Public post-Continue menu absent");surface.clear();active->draw(surface,game);surface.display();const auto image=surface.getTexture().copyToImage();
        need(image.saveToFile(out/(stage+"-menu.png")),"Cannot export menu pixels");
        const auto source=active->images().at(content.media.at("ui").at("menu").at("system_image").get<std::string>()).copyToImage();
        need(image.getPixel({250,150})==source.getPixel({22,20}),"Actual menu did not restore original blue System6 pixels");
    }
};
void lifecycle(const CombatContent& c,Ui& ui,const fs::path& source,const fs::path& out){
    fs::create_directories(out);const auto raw=g8continue::bytes(source);std::ofstream(out/"slot-01.json")<<raw;
    G2NaturalDriver r(c,out);r.game.configureSaveDirectory(out);std::vector<PresentationEvent> events;
    const auto prepare=[&](const SceneDefinition&,const SavedPresentation& state){auto next=std::make_unique<CombatPresentation>(ui.content,ui.font);auto visual=state;visual.music={};next->prepareRestoredState(visual);ui.prepared=std::move(next);};
    r.game.configureRestorePresentation(prepare);r.observe=[&](const PresentationEvent& e){events.push_back(e);};
    r.afterTick=[&]{if(ui.prepared){ui.active=std::move(ui.prepared);ui.bind();}ui.events(events);events.clear();};
    r.load();r.openMenu();ui.menu(r.game,out,"old-title-load");const auto before=r.game.session().exportState();
    ui.surface.clear();ui.active->draw(ui.surface,r.game);ui.surface.display();const auto pixels=ui.surface.getTexture().copyToImage();
    r.game.configureRestorePresentation([](const SceneDefinition&,const SavedPresentation&){throw std::runtime_error("13 injected candidate resource failure");});
    std::string error;need(!r.game.restore(decodeSave(c,raw),error)&&r.game.session().exportState()==before,"Rejected Load candidate replaced live game");
    need(!r.game.newGame(error)&&r.game.session().exportState()==before,"Rejected NewGame candidate replaced live game");
    ui.surface.clear();ui.active->draw(ui.surface,r.game);ui.surface.display();const auto retained=ui.surface.getTexture().copyToImage();
    need(std::equal(pixels.getPixelsPtr(),pixels.getPixelsPtr()+320*240*4,retained.getPixelsPtr()),"Rejected candidates changed active UI pixels");
    r.game.configureRestorePresentation(prepare);r.game.returnToTitle();r.tick();while(r.game.titleOpeningTicks()<8){r.tick();}
    ui.surface.clear();ui.active->draw(ui.surface,r.game);ui.surface.display();need(ui.surface.getTexture().copyToImage().saveToFile(out/"returned-title.png"),"Title image export");
    need(ui.active->systemImage()==c.media.at("ui").at("title").at("system_image").get<std::string>(),"Title kept gameplay System");
    r.press(InputAction::Up);r.press();need(r.game.openingActive()&&r.game.mapId()==2,"Public NewGame did not enter original opening");
    ui.check(out,"newgame-initial",false);need(g8continue::bytes(source)==raw&&g8continue::bytes(out/"slot-01.json")==raw,"Lifecycle changed source disk");
    std::ofstream(out/"result.json")<<Json{{"old_title_Load_blue_menu",true},{"failed_Load_and_NewGame_preserve_session_and_pixels",true},{"return_title_System",ui.content.media.at("ui").at("title").at("system_image")},{"public_NewGame",true},{"source_disk_unchanged",true},{"audio_and_whole_opening_excluded",true}}.dump(2)<<'\n';
    std::cout<<"Old title Load blue menu, rejected Load/NewGame pixel preservation, title System and public NewGame PASS\n";
}

}
int main(int argc,char** argv){try{
    std::cout.setf(std::ios::unitbuf);
    need(argc==5||argc==7,"continue_skin_checks package old-natural-root new-output all|one|lifecycle [difficulty checkpoint]");
    const fs::path output=fs::absolute(argv[3]);need(!fs::exists(output),"Evidence output must be new");fs::create_directories(output);
    const std::string mode=argv[4];need(mode=="all"||mode=="one"||mode=="lifecycle","Unknown UI check mode");
    const auto content=loadCombatContent(argv[1]);Ui ui(content);Json results=Json::array();
    if(std::string(argv[4])=="lifecycle"){lifecycle(content,ui,fs::path(argv[2])/"normal/218/slot-01.json",output/"lifecycle");return 0;}
    for(const std::string difficulty:{"normal","easy","hard"}){for(int checkpoint:{218,219,220}){
        if(std::string(argv[4])=="one"&&(argc!=7||difficulty!=argv[5]||std::to_string(checkpoint)!=argv[6])){continue;}
        const fs::path sourceRoot=fs::path(argv[2])/difficulty/std::to_string(checkpoint),out=output/difficulty/std::to_string(checkpoint);fs::create_directories(out);
        const auto previous=json(sourceRoot/"natural-result.json");const auto seed=previous.value("random_seed",19202026u);ui.reset();bool choiceSeen=false,afterYes=false;
        g8continue::natural(content,checkpoint,sourceRoot/"slot-01.json",out,seed,[&](G2NaturalDriver& r){
            auto events=std::make_shared<std::vector<PresentationEvent>>();
            r.game.configureRestorePresentation([&](const SceneDefinition&,const SavedPresentation& saved){auto staged=std::make_unique<CombatPresentation>(ui.content,ui.font);auto uiState=saved;uiState.music={};staged->prepareRestoredState(uiState);ui.prepared=std::move(staged);});
            r.observe=[&,events](const PresentationEvent& e){events->push_back(e);};
            r.afterTick=[&,events]{if(ui.prepared){ui.active=std::move(ui.prepared);ui.bind();}ui.events(*events);events->clear();if(r.game.hasGame()&&r.game.session().hp()==0&&r.game.inputPromptView().cursor&&!choiceSeen){choiceSeen=true;need(ui.frame(r.game.inputPromptView()).saveToFile(out/"continue-choice.png"),"Choice capture failed");}};
        },[&](G2NaturalDriver& r){
            afterYes=true;ui.check(out,"after-yes",true);r.openMenu();ui.menu(r.game,out,"after-yes");r.closeMenu();
        });
        // The callback's driver has expired; only immutable UI/resources remain here.
        need(choiceSeen&&afterYes,"Natural Continue consumers not reached");ui.check(out,"after-no-title",false);
        const auto actual=json(out/"natural-result.json");results.push_back(actual);std::ofstream(output/"result.json")<<results.dump(2)<<'\n';
        std::cout<<"CONTINUE_SKIN "<<difficulty<<" checkpoint="<<checkpoint<<" actual blue dialogue/number/menu and transparent windows PASS\n";
    }}need(!results.empty(),"No selected consumer");return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
