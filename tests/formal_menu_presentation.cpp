#include "title_navigation.h"
#include "combat_presentation.h"
#include "scene_renderer.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace{
void need(bool ok,const char* text){if(!ok){throw std::runtime_error(text);}}
InputFrame key(InputAction a){InputFrame input;input.buttons[int(a)].pressed=true;return input;}
bool same(const sf::Image& a,const sf::Image& b){return a.getSize()==b.getSize()&&std::equal(a.getPixelsPtr(),a.getPixelsPtr()+a.getSize().x*a.getSize().y*4,b.getPixelsPtr());}
}
int main(int argc,char** argv){try{
    if(argc<3||argc>4){return 2;}auto c=loadCombatContent(argv[1]);const std::filesystem::path output=argv[2];std::filesystem::create_directories(output);
    // Declared content variation: late inventory and hurt actor, without editing the frozen package.
    auto& seed=c.entries.at("post-pickup").session;seed.hp=70;seed.inventory[2]=2;seed.inventory[46]=1;
    for(const auto& [id,item]:c.items){seed.inventory[id]=1;}seed.inventory[2]=2;
    for(auto& [id,item]:c.items){if(item.equipmentType==1){item.description+=" 长说明滚动检查：全文只能出现在帮助窗口内，不能盖住物品和边框。";}}
    auto e=c.entries.at("defeated-claimed");e.map=16;e.playerTile={10,21};e.direction=Direction::Up;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.hp=70;e.session.inventory[2]=2;e.session.inventory[46]=1;c.entries["06-save"]=e;
    GameApplication game(c,"post-pickup");game.configureSaveDirectory(output/"slots");finishTitleOpening(game);sf::Font font;need(font.openFromFile(game.definition().fontPath),"font");
    CombatPresentation p(game.content(),font);p.update(game.takePresentation(),false);sf::RenderTexture target({960,720});target.setView(sf::View({160,120},{320,240}));
    const auto atlas=p.images().at(p.systemImage()).copyToImage();OriginalWindowSkin skin(atlas,sf::Color(255,156,0));sf::RenderTexture arrows({32,8});arrows.clear(sf::Color::Black);skin.drawScrollArrow(arrows,{0,0},true);skin.drawScrollArrow(arrows,{16,0},false);arrows.display();const auto arrowPixels=arrows.getTexture().copyToImage();
    for(unsigned y=0;y<8;++y){for(unsigned x=0;x<32;++x){auto expected=atlas.getPixel({40+x%16,(x<16?16u:8u)+y});if(expected==sf::Color(255,156,0)||expected.a==0){expected=sf::Color::Black;}need(arrowPixels.getPixel({x,y})==expected,"original up/down arrow mask and direction");}}
    auto tick=[&](InputAction action){game.update(key(action));p.update(game.takePresentation(),false);};
    auto capture=[&](const char* name){target.clear();p.draw(target,game);target.display();auto image=target.getTexture().copyToImage();need(image.saveToFile(output/(std::string(name)+".png")),"capture");return image;};
    tick(InputAction::Cancel);need(game.menuView().page==MenuPage::Root,"root");const auto root=capture("root");const auto frozen=game.session().exportState();
    for(int n=0;n<60;++n){game.update({});p.update(game.takePresentation(),false);}need(game.session().exportState()==frozen,"menu pauses world and timers");
    const auto originalSkin=p.systemImage();const auto other=c.media.at("ui").at("title").at("system_image").get<std::string>();
    if(other!=originalSkin){p.update({{PresentationEvent::Kind::SystemGraphic,other}},false);need(!same(root,capture("root-other-system")),"menu follows current System identity");p.update({{PresentationEvent::Kind::SystemGraphic,originalSkin}},false);}
    tick(InputAction::Confirm);need(game.menuView().columns==2,"items source dual columns");capture("items");
    for(int n=0;n<12;++n){tick(InputAction::Down);}capture("items-scroll");tick(InputAction::Cancel);tick(InputAction::Confirm);
    for(int n=0;n<300&&game.menuView().rows.at(game.menuView().selection).id!=2;++n){tick(InputAction::Right);}need(game.menuView().rows.at(game.menuView().selection).id==2,"medicine entry");tick(InputAction::Confirm);need(game.menuView().page==MenuPage::Target,"target entry");capture("target");
    tick(InputAction::Cancel);need(game.session().hp()==70&&game.session().count(2)==2,"target cancel does not consume");tick(InputAction::Confirm);tick(InputAction::Confirm);need(game.session().hp()==100&&game.session().count(2)==1,"target applies domain medicine");capture("target-healed");tick(InputAction::Cancel);tick(InputAction::Cancel);
    tick(InputAction::Down);tick(InputAction::Confirm);need(game.menuView().page==MenuPage::Skills,"skills entry");capture("skills");tick(InputAction::Cancel);
    tick(InputAction::Down);tick(InputAction::Down);tick(InputAction::Confirm);capture("slots");tick(InputAction::Confirm);need(game.menuView().page==MenuPage::Equipment&&game.menuView().columns==2,"equipment source dual columns");
    const auto equipment=game.session().exportState();tick(InputAction::Right);need(game.menuView().selection==1,"equipment right navigation");tick(InputAction::Down);need(game.menuView().selection==3%game.menuView().rows.size(),"equipment down navigation");if(game.menuView().rows.at(game.menuView().selection).id==0){tick(InputAction::Right);}const auto helpBefore=capture("equipment");
    for(int i=0;i<240;++i){p.update({},false);}const auto scroll=capture("equipment-long-help");bool changed=false;for(unsigned y=24;y<72;++y){for(unsigned x=24;x<936;++x){changed|=scroll.getPixel({x,y})!=helpBefore.getPixel({x,y});}}need(changed,"long source description actually scrolls inside help");need(same(scroll,capture("equipment-long-help-repeat")),"draw does not advance UI");
    tick(InputAction::Cancel);need(game.session().exportState()==equipment,"preview cancel preserves equipment");tick(InputAction::Cancel);tick(InputAction::Up);tick(InputAction::Confirm);need(game.menuView().page==MenuPage::TitleConfirm,"return title entry");capture("title-confirm");tick(InputAction::Cancel);need(game.formalMenu(),"return title cancellation");tick(InputAction::Up);tick(InputAction::Confirm);tick(InputAction::Confirm);need(game.mode()==ApplicationMode::Title,"return title confirmed");capture("title");
    // Real SaveFiles summaries and title Load use isolated copies; corrupt/empty states remain visible.
    game.restartDevelopmentEntry("06-save");p.update(game.takePresentation(),false);
    tick(InputAction::Cancel);tick(InputAction::Confirm);for(int n=0;n<100&&game.menuView().rows.at(game.menuView().selection).id!=2;++n){tick(InputAction::Right);}tick(InputAction::Confirm);tick(InputAction::Confirm);need(game.session().hp()==100,"real menu healing before saving");tick(InputAction::Cancel);tick(InputAction::Cancel);tick(InputAction::Cancel);
    tick(InputAction::Confirm);for(int n=0;n<200;++n){game.update({});}tick(InputAction::Confirm);need(game.fileMenuView().has_value(),"real save point enters file UI");capture("save-files");tick(InputAction::Cancel);need(game.mode()==ApplicationMode::Playing&&game.session().saveCount()==e.session.saveCount,"save cancellation preserves counters");
    tick(InputAction::Confirm);for(int n=0;n<200;++n){game.update({});}tick(InputAction::Confirm);tick(InputAction::Confirm);if(game.fileMenuView()&&game.fileMenuView()->confirmation){tick(InputAction::Left);tick(InputAction::Confirm);}need(game.mode()==ApplicationMode::Playing,"real save committed");
    std::ofstream(output/"slots/slot-03.json")<<"corrupt";
    game.returnToTitle();finishTitleOpening(game);p.update(game.takePresentation(),false);tick(InputAction::Confirm);need(game.fileMenuView()&&game.fileMenuView()->lines.size()==15,"15-slot real title Load entry");capture("files");
    tick(InputAction::Down);tick(InputAction::Down);tick(InputAction::Confirm);capture("files-corrupt-feedback");need(game.fileMenuView()->prompt.find("原文件")!=std::string::npos,"corrupt failure protection remains");tick(InputAction::Cancel);
    for(int i=0;i<12;++i){tick(InputAction::Down);}need(game.fileMenuView()->selection==14,"last slot navigation");capture("files-scroll15");tick(InputAction::Cancel);
    tick(InputAction::Confirm);tick(InputAction::Confirm);need(game.hasGame()&&game.mode()==ApplicationMode::Playing&&game.session().hp()==100,"title Load restores isolated save");
    // Native render uses the same draw entry at three sizes; closes only its own window.
    if(argc==4&&std::string(argv[3])=="--native"){
        tick(InputAction::Cancel);sf::RenderWindow window(sf::VideoMode({960,720}),"Nobi 06 isolated menu presentation");
        for(const auto size:{sf::Vector2u{960,720},sf::Vector2u{1000,600},sf::Vector2u{400,360}}){window.setSize(size);for(int i=0;i<4;++i){while(window.pollEvent()){}window.setView(makeSceneView(window.getSize(),{320,240}));window.clear();p.draw(window,game);window.display();}sf::Texture screen;need(screen.resize(window.getSize()),"native texture");screen.update(window);need(screen.copyToImage().saveToFile(output/("native-"+std::to_string(size.x)+".png")),"native capture");}window.close();
    }
    // Preserve the existing reading-render check while the menu display is replaced.
    game.restartDevelopmentEntry("post-pickup");p.update(game.takePresentation(),false);tick(InputAction::Cancel);tick(InputAction::Confirm);
    for(int n=0;n<400&&game.menuView().rows.at(game.menuView().selection).id!=180;++n){tick(InputAction::Right);}tick(InputAction::Confirm);DialogueRenderer dialogue(font);dialogue.setMedia(p.images(),p.palette(),&p.systemImage());std::set<MessageRequestId> captured;
    for(int n=0;n<6000&&game.mode()==ApplicationMode::Reading;++n){game.update(game.dialogue().view().phase==DialoguePhase::AwaitingInput&&captured.contains(game.dialogue().view().request)?key(InputAction::Confirm):InputFrame{});p.update(game.takePresentation());if(game.dialogue().view().phase==DialoguePhase::AwaitingInput&&captured.insert(game.dialogue().view().request).second){target.clear();p.draw(target,game);dialogue.draw(target,game.dialogue().view());target.display();need(target.getTexture().copyToImage().saveToFile(output/("manual-"+std::to_string(captured.size())+".png")),"manual capture");}}
    need(captured.size()==12,"retained manual twelve page rendering");capture("manual-return");
    std::cout<<"06 real menu/target/equipment/cancel/skin/15-slot/corrupt/Load presentation PASS; original RPG_RT exact pixels and Windows pending\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
