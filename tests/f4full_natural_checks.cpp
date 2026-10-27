#include "title_navigation.h"
#include "g0_natural_driver.h"
#include "save_platform.h"
#include "dialogue_renderer.h"
#include <SFML/Graphics/RenderTexture.hpp>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <queue>
#include <stdexcept>
#include <sstream>

namespace {
void need(bool value,const std::string& reason){if(!value){throw std::runtime_error(reason);}}
InputFrame key(InputAction action=InputAction::Confirm){InputFrame f;f.buttons[static_cast<int>(action)].pressed=true;return f;}
using Cell=std::pair<int,int>;

// Observe the real M61 Dialogue view, loading only its atlas/face/palette, never the whole media set.
void captureExFont(const GameApplication& game,const std::filesystem::path& output){
    const auto& view=game.dialogue().view();const auto& content=game.content();
    need(game.mapId()==61&&game.session().actorId()==1&&game.session().flag(178),"ExFont observation must follow the actual Sena zombie fight");
    need(view.exfontImage=="g9-message-exfont"&&view.lines.size()==2&&view.lines[1]==U"「——— 」"&&
         view.exfontIndices.size()==2&&view.exfontIndices[1]==std::vector<int>({-1,-1,-1,-1,32,-1}),"Actual M61 message lost its explicit original heart");
    need(view.faceImage=="face-actor1"&&!view.faceRight&&!view.faceFlipped&&view.display.position==2&&!view.display.transparent,"Actual M61 face/window binding changed");
    std::map<std::string,sf::Texture> images;
    for(const auto& id:{view.exfontImage,view.faceImage}){
        need(images[id].loadFromFile(content.directory/content.media.at("images").at(id).at("file").get<std::string>()),"Cannot load actual M61 image "+id);
    }
    const auto system=content.media.at("ui").at("menu").at("system_image").get<std::string>();
    sf::Image palette;need(palette.loadFromFile(content.directory/content.media.at("images").at(system).at("file").get<std::string>()),"Cannot load actual system palette");
    sf::Font font;need(font.openFromFile(game.definition().fontPath),"Cannot load actual scene font");
    DialogueRenderer renderer(font);renderer.setMedia(images,palette);std::filesystem::create_directories(output);
    const std::string rows[]={"............","..##...##...",".#..#.#..#..","#....#....#.","#.........#.","#.........#.",".#.......#..","..#.....#...","...#...#....","....#.#.....",".....#......","............"};
    for(const auto size:{sf::Vector2u{960,720},sf::Vector2u{1000,600}}){
        sf::RenderTexture target(size);sf::View camera(sf::FloatRect({0,0},{320,240}));
        const float scale=std::min(size.x/320.f,size.y/240.f);
        camera.setViewport(sf::FloatRect({(size.x-320*scale)/(2*size.x),(size.y-240*scale)/(2*size.y)},{320*scale/size.x,240*scale/size.y}));
        camera.setCenter({620,520});target.setView(camera);target.clear(sf::Color(18,39,61));renderer.draw(target,view);
        need(target.getView().getCenter()==camera.getCenter(),"Actual message changed camera");target.display();const auto pixels=target.getTexture().copyToImage();
        const auto viewport=target.getViewport(camera);const auto fill=palette.getPixel({8,55});
        for(unsigned y=0;y<12;++y){for(unsigned x=0;x<12;++x){if(rows[y][x]=='#'){
            const unsigned px=viewport.position.x+static_cast<unsigned>((112+x)*scale+scale/2);
            const unsigned py=viewport.position.y+static_cast<unsigned>((186+y)*scale+scale/2);
            need(pixels.getPixel({px,py})==fill,"Actual M61 original mask/color/12px placement differs");
        }}}
        need(pixels.saveToFile(output/(std::to_string(size.x)+"x"+std::to_string(size.y)+".png")),"Cannot save actual M61 observation");
    }
    std::cout<<"EXFONT_M61_OBSERVED map=61 actor=1 S178=1 source=M61-E2P2-C36-37 index=32 atlas=156x48 face=actor1 logical=112,186 captures=960x720,1000x600\n";
}
void rejectExFontCandidates(GameApplication& game,const std::filesystem::path& package,const std::filesystem::path& saves){
    const auto source=nlohmann::json::parse(g0natural::read(package.string().c_str()));
    need(source.at("schema")=="nobihaza.g9text.v1","ExFont check needs the actual G9 candidate");
    SaveFiles files(game.content(),saves);const auto checkpoint=files.read(1);
    for(const std::string kind:{"unreviewed-source","source-bytes","index52","missing-atlas","damaged-atlas"}){
        auto bad=source;const auto directory=saves/"rejected"/kind;std::filesystem::create_directories(directory);
        std::string expected="Unreviewed ExFont projection";
        if(kind=="unreviewed-source"){bad["source"]["sha256"]=std::string(64,'0');expected="Unreviewed ExFont predecessor";}
        if(kind=="source-bytes"){
            expected="Changed ExFont resource";
            std::filesystem::copy_file(package.parent_path()/"g9-message-exfont.png",directory/"g9-message-exfont.png",std::filesystem::copy_options::overwrite_existing);
            std::ofstream altered(directory/"g8.json",std::ios::binary);altered<<g0natural::read((package.parent_path()/"g8.json").string().c_str())<<' ';need(altered.good(),"Cannot write changed predecessor fixture");
        }
        if(kind=="index52"){bad["projection"]["messages"]["m61-e2-p2-c36"]["lines"][1]["tokens"][2]["value"]=52;}
        if(kind=="missing-atlas"){expected="Missing or oversized ExFont resource";}
        if(kind=="damaged-atlas"){
            expected="Changed ExFont resource";auto bytes=g0natural::read((package.parent_path()/"g9-message-exfont.png").string().c_str());bytes+='x';
            std::ofstream atlas(directory/"g9-message-exfont.png",std::ios::binary);atlas<<bytes;need(atlas.good(),"Cannot write corrupt candidate fixture");
        }
        const auto entry=directory/"g9.json";{std::ofstream output(entry);output<<bad.dump();need(output.good(),"Cannot write rejected candidate fixture");}
        const auto generation=game.generation();const auto state=game.session().exportState();const auto tile=game.world().player().motion.tile();
        game.configureRestorePresentation([&](const SceneDefinition&,const SavedPresentation&){auto invalid=loadCombatContent(entry);(void)invalid;});
        std::string error;const bool restored=game.restore(checkpoint,error);game.configureRestorePresentation({});
        need(!restored&&error.find(expected)!=std::string::npos,"Bad ExFont candidate did not fail at its actual loader boundary: "+kind+": "+error);
        need(game.mode()==ApplicationMode::Playing&&game.generation()==generation&&game.session().exportState()==state&&game.world().player().motion.tile()==tile,"Rejected ExFont candidate retired the current game");
        std::cout<<"EXFONT_CANDIDATE_REJECTED kind="<<kind<<" retained_generation="<<generation<<" error="<<error<<'\n';
    }
}

// Test navigation uses the real occupied World and sends only public player inputs.
class NaturalRoute {
public:
    int difficultySelection{1};bool controlledBossRandom{},reserveMainForHangar{};int controlledBossCalls{};
    bool observeExFont{},exfontObserved{};
    SavedPresentation recoveredPresentation;bool trackPresentation{};int fadeTicks{};
    std::filesystem::path saves;GameApplication game;int ticks{};std::ofstream inputs;int inputCount{};
    bool collectG0Extras{};int extraG0End{},extraG0Corpse{};std::string g0EndSound;
    explicit NaturalRoute(CombatContent content):game(std::move(content),"title",[this](int lo,int hi){if(controlledBossRandom){++controlledBossCalls;return g0natural::safe(lo,hi);}return lo;}){}
    void record(const InputFrame& input){
        if(!inputs.is_open()){return;}int held=0,pressed=0,released=0;
        for(std::size_t n=0;n<input.buttons.size();++n){held|=int(input.buttons[n].held)<<n;pressed|=int(input.buttons[n].pressed)<<n;released|=int(input.buttons[n].released)<<n;}
        inputs<<nlohmann::json{{"step",inputCount++},{"movement",input.movement?int(*input.movement):-1},{"held",held},{"pressed",pressed},{"released",released}}.dump()<<'\n';
        need(inputs.good(),"Cannot record public natural inputs");
    }
    void tick(InputFrame input={}){
        if(observeExFont&&!exfontObserved&&!game.dialogue().view().exfontImage.empty()){input.buttons[static_cast<int>(InputAction::Confirm)]={};}
        if(game.hasGame()&&(game.session().flag(10)||game.session().flag(45))){input.buttons[static_cast<int>(InputAction::Left)].pressed=true;}
        record(input);const auto priorStage=game.openingStage();game.update(input);++ticks;
        if(observeExFont&&!exfontObserved&&!game.dialogue().view().exfontImage.empty()&&game.dialogue().view().phase==DialoguePhase::AwaitingInput){captureExFont(game,saves/"rendered");exfontObserved=true;}
        for(const auto& e:game.takePresentation()){
            if(collectG0Extras){extraG0End+=e.kind==PresentationEvent::Kind::Sound&&e.sound.resource==g0EndSound;extraG0Corpse+=e.kind==PresentationEvent::Kind::Cue&&e.id=="m120-e13-p18-c40";}
            if(e.kind==PresentationEvent::Kind::Notice){std::cerr<<"Notice: "<<e.id<<std::endl;}
            if(trackPresentation){
                if(e.kind==PresentationEvent::Kind::Music){recoveredPresentation.music=e.sound;fadeTicks=0;}
                if(e.kind==PresentationEvent::Kind::StopMusic){recoveredPresentation.music={};fadeTicks=0;}
                if(e.kind==PresentationEvent::Kind::FadeMusic){fadeTicks=(e.duration+16)/17;}
                if(e.kind==PresentationEvent::Kind::Cue){const auto& cue=game.content().cue(e.id);if(cue.at("kind")=="tint"){const auto& t=cue.at("tone_percent");for(int n=0;n<3;++n){recoveredPresentation.tone[n]=t.at(n);}recoveredPresentation.saturation=t.at(3);}}
            }
        }
        if(trackPresentation){if(fadeTicks>0&&!--fadeTicks){recoveredPresentation.music={};}game.setPresentationState(recoveredPresentation);}
        need(game.mode()!=ApplicationMode::Title,"Route returned to Title at tick "+std::to_string(ticks)+" opening="+priorStage);
        need(!game.hasGame()||game.session().hp()>0,"Route died on map "+std::to_string(game.mapId())+" pursuerHP="+std::to_string(game.session().pursuerHealth())+" rounds="+std::to_string(game.session().magazine(game.session().equipped(1))));
    }
    void press(InputAction action=InputAction::Confirm){tick(key(action));tick();}
    void settle(int choice=0){
        std::uint64_t prompt{};int firstChoiceLine{};
        for(int n=0;n<60000;++n){
            if(game.mode()==ApplicationMode::Boundary){return;}
            need(game.mode()==ApplicationMode::Playing,"Unexpected UI mode "+std::to_string(static_cast<int>(game.mode())));
            if(game.foreground()==ForegroundKind::None&&!game.dialogue().active()&&!game.world().player().motion.moving()){return;}
            auto input=n%12==0?key():InputFrame{};
            const auto view=game.inputPromptView();
            if(const auto cursor=view.cursor){
                if(prompt!=view.request){prompt=view.request;firstChoiceLine=cursor->line;}
                input=key(cursor->line==firstChoiceLine+choice?InputAction::Confirm:InputAction::Down);
            }
            tick(input);
        }
        throw std::runtime_error("Foreground stuck map "+std::to_string(game.mapId()));
    }
    std::optional<Direction> next(TilePosition goal){
        const auto& world=game.world();const auto start=world.player().motion.tile();std::queue<TilePosition> pending;pending.push(start);
        std::map<Cell,std::optional<Direction>> first;first[{start.x,start.y}]={};
        while(!pending.empty()){
            const auto at=pending.front();pending.pop();if(at==goal){return first.at({at.x,at.y});}
            for(auto d:{Direction::Up,Direction::Right,Direction::Down,Direction::Left}){
                auto actor=world.player();actor.motion=CharacterMotion(at,d,actor.motion.speed());
                const auto tile=adjacentTile(at,d);Cell cell{tile.x,tile.y};
                if(first.contains(cell)||!world.canMove(actor,d)){continue;}
                first[cell]=first.at({at.x,at.y}).value_or(d);pending.push(tile);
            }
        }
        return {};
    }
    int healingItem() const{
        int selected=0,score=0;
        for(const auto& [id,definition]:game.content().items){
            const auto& medicine=definition.medicine;
            if(definition.equipmentType!=6||!game.session().count(id)||!game.session().canSelectItem(id)||medicine.hpPercent+medicine.hpPoints<=0){continue;}
            const bool cure=game.session().conditions().poisoned&&std::find(medicine.cureStates.begin(),medicine.cureStates.end(),2)!=medicine.cureStates.end();
            const int value=medicine.hpPercent+medicine.hpPoints+(cure?1000:0);if(value>score){score=value;selected=id;}
        }return selected;
    }
    void walk(TilePosition goal){
        const int map=game.mapId();int blockedTicks=0;
        for(int n=0;n<24000;++n){
            settle();if(game.mapId()!=map||game.mode()==ApplicationMode::Boundary){return;}
            if(game.world().player().motion.tile()==goal){return;}
            if(game.session().hp()<60){if(const int medicine=healingItem()){item(medicine);continue;}}
            InputFrame f;f.movement=next(goal);if(!f.movement){if(++blockedTicks>=120){break;}}else{blockedTicks=0;}tick(f);
        }
        throw std::runtime_error("Blocked map "+std::to_string(map)+" at "+std::to_string(game.world().player().motion.tile().x)+","+std::to_string(game.world().player().motion.tile().y)+" toward "+std::to_string(goal.x)+","+std::to_string(goal.y));
    }
    void lower(){
        for(int attempt=0;attempt<3&&game.raised();++attempt){
            for(int n=0;n<24;++n){InputFrame f;f.buttons[static_cast<int>(InputAction::Weapon)].held=true;tick(f);}
            for(int n=0;n<60;++n){tick();}settle();
        }
        need(!game.raised(),"Public Shift gesture lowers weapon");
    }
    void action(const std::string& id,int choice=0){
        settle();lower();const int map=game.mapId();const auto& object=game.content().objects.at(id);need(object.map==map,"Wrong map before "+id);
        const auto tile=findCharacter(game.world(),id).motion.tile();const auto* page=game.content().activeVariant(object,game.session());need(page,"No active page "+id);
        if(page->definition.layer!=1){walk(tile);if(game.mapId()==map){press();}}
        else{
            bool reached=false;
            for(auto side:{Direction::Down,Direction::Left,Direction::Right,Direction::Up}){
                const auto target=adjacentTile(tile,side);if(target!=game.world().player().motion.tile()&&!next(target)){continue;}
                walk(target);if(game.mapId()!=map){reached=true;break;}
                InputFrame f;f.movement=static_cast<Direction>((static_cast<int>(side)+2)%4);tick(f);press();reached=true;break;
            }
            need(reached,"No approach "+id+" tile="+std::to_string(tile.x)+","+std::to_string(tile.y)+" from="+std::to_string(game.world().player().motion.tile().x)+","+std::to_string(game.world().player().motion.tile().y)+" page="+std::to_string(page->page));
        }
        settle(choice);
        const auto transition=game.content().transitions.find(id+"-p"+std::to_string(page->page));
        if(transition!=game.content().transitions.end()&&transition->value("runtime_enabled",false)&&game.mapId()==map){
            for(int attempt=0;attempt<5&&game.mapId()==map;++attempt){for(int n=0;n<60;++n){tick();}press();settle(choice);}
        }
        std::cout<<id<<" -> "<<game.mapId()<<" HP="<<game.session().hp()<<" ticks="<<ticks<<std::endl;
    }
    void travel(int target){
        for(int hop=0;hop<60&&game.mapId()!=target;++hop){
            const int origin=game.mapId();std::queue<int> pending;pending.push(origin);std::map<int,std::string> first;first[origin]="";
            while(!pending.empty()&&!first.contains(target)){const int map=pending.front();pending.pop();
                for(const auto& [id,t]:game.content().transitions.items()){const auto& object=game.content().objects.at(t.at("object").get<std::string>());if(object.map!=map||!t.value("runtime_enabled",true)){continue;}
                    const auto* page=game.content().activeVariant(object,game.session());if(!page||page->page!=t.at("page")){continue;}const int destination=t.at("destination").at("map");if(first.contains(destination)){continue;}first[destination]=map==origin?object.id:first.at(map);pending.push(destination);}}
            need(first.contains(target),"No active map route from "+std::to_string(origin)+" to "+std::to_string(target));action(first.at(target));need(game.mapId()!=origin,"Transfer refused "+first.at(target));
        }
        need(game.mapId()==target,"Travel hop limit");
    }
    void selectMenuRow(int id){
        auto v=game.menuView();const auto found=std::find_if(v.rows.begin(),v.rows.end(),[&](const auto& row){return row.id==id;});
        need(found!=v.rows.end(),"Missing menu row "+std::to_string(id)+" mode="+std::to_string(static_cast<int>(game.mode()))+" foreground="+std::to_string(static_cast<int>(game.foreground()))+" controls="+std::to_string(game.session().flag(1))+std::to_string(game.session().flag(18))+" grab="+std::to_string(game.session().flag(10)));const int target=static_cast<int>(found-v.rows.begin());
        for(int n=0;n<static_cast<int>(v.rows.size())&&game.menuView().selection!=target;++n){press(v.columns==2?InputAction::Right:InputAction::Down);}
        need(game.menuView().selection==target,"Cannot select menu row");press();
    }
    void openMenu(){lower();for(int n=0;n<600&&game.mode()==ApplicationMode::Playing;++n){tick(n%12==0?key(InputAction::Cancel):InputFrame{});}need(game.mode()==ApplicationMode::Equipment,"Public X menu absent weapon="+std::to_string(game.session().equipped(1))+" raised="+std::to_string(game.raised())+" S1/18/17/45/148="+std::to_string(game.session().flag(1))+std::to_string(game.session().flag(18))+std::to_string(game.session().flag(17))+std::to_string(game.session().flag(45))+std::to_string(game.session().flag(148)));}
    void equip(int item,int slot=1){if(game.session().equipped(slot)==item){return;}openMenu();selectMenuRow(2);selectMenuRow(slot);selectMenuRow(item);press(InputAction::Cancel);press(InputAction::Cancel);need(game.mode()==ApplicationMode::Playing&&game.session().equipped(slot)==item,"Actual equipment menu commits");}
    void item(int item){openMenu();selectMenuRow(0);selectMenuRow(item);if(game.mode()==ApplicationMode::Equipment&&game.menuView().page==MenuPage::Target){press();}for(int n=0;n<3&&game.mode()==ApplicationMode::Equipment;++n){press(InputAction::Cancel);}settle();}
    void saveAtCrow(){
        const std::map<int,std::string> crows{{22,"m22-e2"},{16,"m16-e5"},{77,"m77-e6"},{79,"m79-e6"},{107,"m107-e3"},{122,"m122-e16"}};need(crows.contains(game.mapId()),"Reviewed natural crow checkpoint");const auto crow=crows.at(game.mapId());const auto tile=findCharacter(game.world(),crow).motion.tile();walk({tile.x,tile.y+1});InputFrame face;face.movement=Direction::Up;tick(face);press();
        need(game.mode()==ApplicationMode::Saving,"Natural crow opens save UI");const int before=game.session().saveCount();
        for(int n=0;n<1200&&game.mode()==ApplicationMode::Saving;++n){
            const auto menu=game.fileMenuView();
            tick(menu&&menu->confirmation?key(menu->answer?InputAction::Down:InputAction::Confirm):(n%12==0?key():InputFrame{}));
        }
        need(game.mode()==ApplicationMode::Playing&&game.session().saveCount()==before+1,"Natural checkpoint committed by public save UI");
        if(!saves.empty()){std::filesystem::create_directories(saves/"checkpoints");std::filesystem::copy_file(saves/"slot-01.json",saves/"checkpoints"/("map-"+std::to_string(game.mapId())+"-count-"+std::to_string(game.session().saveCount())+".json"),std::filesystem::copy_options::overwrite_existing);}
        std::cout<<"Natural checkpoint saved map="<<game.mapId()<<" count="<<game.session().saveCount()<<std::endl;
    }
    void opening(){
        game.update(key());need(game.hasGame()&&game.mapId()==2&&game.session().saveCount()==0,"Actual fresh NORMAL NewGame");
        need(!game.session().exportState().magazines.contains(11)&&game.session().magazine(17)==0&&game.session().magazine(18)==0,"Opening-owned magazine waits for source command; unbound launcher slots remain zero");
        std::uint64_t firstPrompt{};
        for(int n=0;n<80000&&game.openingActive();++n){
            InputFrame input;const auto stage=game.openingStage();
            if(const auto cursor=game.inputPromptView().cursor){const auto request=game.inputPromptView().request;if(!firstPrompt){firstPrompt=request;}const int desired=request==firstPrompt?difficultySelection:1;input=key(cursor->line==desired?InputAction::Confirm:InputAction::Down);}
            else if(stage=="door-approach-common"){
                const auto door=game.content().objects.at("m4-e2").tile;const TilePosition target{door.x,door.y+1};
                if(!game.world().player().motion.moving()){
                    if(game.world().player().motion.tile()==target){if(game.world().player().motion.facing()!=Direction::Up){input.movement=Direction::Up;}else{input=key();}}
                    else{input.movement=next(target);}
                }
            }else if(n%13==0){input=key();}
            tick(input);
        }
        need(!game.openingActive()&&game.mapId()==4&&game.session().flag(51),"Legal skip and NORMAL reach first room");
        need(game.session().magazine(11)==15&&game.session().magazine(12)==13&&game.session().magazine(16)==1&&game.session().magazine(19)==30&&game.session().magazine(21)==8,"Actual opening commits all owned magazine seeds once");settle();
    }
    void pushLadder(Direction direction,TilePosition target){
        for(int n=0;n<6000&&findCharacter(game.world(),"m12-e9").motion.tile()!=target;++n){InputFrame f;f.movement=direction;tick(f);}
        settle();need(findCharacter(game.world(),"m12-e9").motion.tile()==target,"Actual ladder push reaches vent approach");
    }
    void pushObject(const std::string& id,Direction direction,TilePosition target){
        lower();for(int n=0;n<6000&&findCharacter(game.world(),id).motion.tile()!=target;++n){InputFrame f;f.movement=direction;f.buttons[static_cast<int>(direction==Direction::Left?InputAction::Left:direction==Direction::Right?InputAction::Right:direction==Direction::Up?InputAction::Up:InputAction::Down)].held=true;tick(f);}settle();need(findCharacter(game.world(),id).motion.tile()==target,"Real puzzle push reached source destination: "+id);
    }
    void newBlack(){
        if(!game.session().count(68)){travel(32);action("m32-e22");need(game.session().count(66)==1,"Natural cable pickup");travel(33);walk({9,7});tick();item(67);need(game.session().flag(93),"Real cable menu restores power");
        travel(28);walk({10,4});pushObject("m28-e3",Direction::Left,{8,4});need(game.session().flag(97),"Wardrobe moved by actual contact");action("m28-e4");travel(44);equip(21);
        for(const auto* id:{"m44-e2","m44-e4","m44-e6"}){fight(id,game.content().enemies.at(id).deadFlag);}action("m44-e12");need(game.session().count(68)==1&&game.session().flag(145),"Actual three enemy clearance grants lighter");
        travel(16);saveAtCrow();}
        action("m16-e15");action("m26-e8");travel(34);travel(37);travel(16);travel(63);walk({7,6});pushObject("m63-e2",Direction::Left,{3,6});walk({3,5});pushObject("m63-e2",Direction::Down,{3,8});
        walk({2,8});InputFrame face;face.movement=Direction::Right;tick(face);press();settle();need(game.session().flag(69),"Real stool climb");face.movement=Direction::Right;tick(face);press();settle();need(game.session().flag(197),"Upper switch question commits");face.movement=Direction::Left;tick(face);press();settle();action("m63-e10");
        walk({10,6});tick();item(69);need(game.session().flag(141),"Natural candle ignition");
        walk({8,9});pushObject("m49-e6",Direction::Down,{8,12});walk({12,9});pushObject("m49-e5",Direction::Down,{12,12});
        walk({7,6});pushObject("m49-e2",Direction::Down,{7,10});walk({6,10});pushObject("m49-e2",Direction::Right,{12,10});
        walk({13,6});pushObject("m49-e4",Direction::Down,{13,9});walk({14,9});pushObject("m49-e4",Direction::Left,{8,9});walk({8,8});pushObject("m49-e4",Direction::Down,{8,10});
        walk({11,12});pushObject("m49-e5",Direction::Right,{13,12});walk({13,13});pushObject("m49-e5",Direction::Up,{13,7});
        walk({9,12});pushObject("m49-e6",Direction::Left,{7,12});walk({7,13});pushObject("m49-e6",Direction::Up,{7,7});
        for(int n=0;n<1200&&!game.session().flag(143);++n){tick();}settle();need(game.session().flag(143),"Actual statue positions reveal pedestal");walk({10,5});InputFrame north;north.movement=Direction::Up;tick(north);press();settle();need(game.session().count(70)==1,"Natural spade key");
        travel(63);for(int n=0;n<12000&&!game.session().flag(274);++n){tick(n%12==0?key():InputFrame{});}settle();need(game.session().flag(274),"Natural rescue or BSAA intervention finishes");travel(16);travel(34);travel(27);action("m27-e2");action("m26-e5");saveAtCrow();
    }
    void titleLoad(int slot=1){for(int n=0;n<3&&game.menuSelection()!=1;++n){record(key(InputAction::Down));game.update(key(InputAction::Down));}record(key());game.update(key());need(game.mode()==ApplicationMode::Loading,"Actual title Load");for(int n=1;n<slot;++n){record(key(InputAction::Down));game.update(key(InputAction::Down));}record(key());game.update(key());need(game.mode()==ApplicationMode::Playing&&(game.mapId()==16||game.mapId()==77||game.mapId()==79||game.mapId()==107||game.mapId()==121||game.mapId()==122),"Natural checkpoint starts at its actual crow");settle();}
    void solveDrainage(){
        lower();walk(adjacentTile(game.content().objects.at("m107-e5").tile,Direction::Right));InputFrame face;face.movement=Direction::Left;tick(face);press();
        for(int n=0;n<800&&game.foreground()!=ForegroundKind::Drainage;++n){tick(n%12==0?key():InputFrame{});}
        need(game.foreground()==ForegroundKind::Drainage,"Original panel opens through public investigation");
        for(auto button:{InputAction::Confirm,InputAction::Left,InputAction::Confirm,InputAction::Right,InputAction::Down,InputAction::Confirm,InputAction::Right,InputAction::Confirm,InputAction::Down,InputAction::Confirm,InputAction::Right,InputAction::Confirm}){tick(key(button));for(int n=0;n<20;++n){tick();}}
        settle();need(game.session().flag(321),"Actual nine-panel input commits drainage after its presentation");
    }
    void newArea(){
        travel(18);action("m18-e9");action("m18-e9");travel(22);action("m22-e4");action("m22-e7");equip(21);
        action("m22-e3");action("m21-e11");action("m21-e11");travel(11);action("m11-e5");action("m11-e5");
        need(game.session().count(62)==1,"Wardrobe key unlocks real wrench");travel(12);
        walk({32,8});pushLadder(Direction::Left,{28,8});walk({28,9});pushLadder(Direction::Up,{28,7});
        walk({28,8});action("m12-e9");tick();need(game.session().count(63)==1&&game.session().flag(69),"Natural pushed ladder reaches active wrench zone");
        item(63);need(game.session().flag(72),"Actual item menu removes ventilation bars");action("m12-e14");action("m23-e3");
        for(const auto* actor:{"m24-e6","m24-e10","m24-e5","m24-e4","m24-e3","m24-e2","m24-e7","m24-e8"}){action(actor);}
        action("m24-e2");for(int n=0;n<80000&&!game.session().flag(84);++n){tick(n%12==0?key():InputFrame{});}
        need(game.session().flag(84)&&game.mapId()==12,"All public NPC conversations finish conference and branch opening");travel(16);saveAtCrow();
    }
    void guardFirst(){
        for(int n=0;n<80000&&game.mapId()!=72;++n){tick(n%12==0?key():InputFrame{});}settle();need(game.mapId()==72,"Actual first guard intro enters M72");const auto& slots=game.content().full->at("runtime").at("parking").at("first_guard").at("runtime_profile").at("slots");
        for(int n=0;n<18000&&!game.session().flag(239);++n){if(n%600==0){std::cout<<"Guard tick="<<n<<" HP="<<game.session().hp()<<" ammo="<<game.session().magazine(game.session().equipped(1))<<" kills="<<game.session().variable(269)<<" rescue="<<game.session().variable(267)<<" grabbed="<<game.session().flag(10)<<std::endl;}
            if(game.foreground()!=ForegroundKind::None){tick(n%12==0?key():InputFrame{});continue;}
            if(game.mapId()!=72||!game.session().flag(1)||!game.session().flag(18)){tick();continue;}
            if(game.session().hp()<65&&!game.session().flag(10)){if(const int medicine=healingItem()){item(medicine);continue;}}
            const auto player=game.world().player().motion.tile(),companion=findCharacter(game.world(),"m72-e6").motion.tile();std::string target;int distance=10000;
            for(const auto& slot:slots){const auto id=slot.at("object").get<std::string>();const auto& actor=findCharacter(game.world(),id);if(!actor.active||game.session().flag(slot.at("dead_fact"))){continue;}const auto tile=actor.motion.tile();if(tile==TilePosition{0,0}){continue;}const int d=std::abs(tile.x-companion.x)+std::abs(tile.y-companion.y);if(d<distance){target=id;distance=d;}}
            InputFrame input;
            if(!game.raised()){input=key(InputAction::Weapon);}
            else if(!target.empty()){const auto enemy=findCharacter(game.world(),target).motion.tile();const auto facing=std::abs(enemy.x-player.x)>=std::abs(enemy.y-player.y)?(enemy.x>player.x?Direction::Right:Direction::Left):(enemy.y>player.y?Direction::Down:Direction::Up);
                if(enemy.x!=player.x&&enemy.y!=player.y){input.movement=std::abs(enemy.x-player.x)>=std::abs(enemy.y-player.y)?(enemy.y>player.y?Direction::Down:Direction::Up):(enemy.x>player.x?Direction::Right:Direction::Left);}else if(game.world().player().motion.facing()!=facing){input.buttons[static_cast<int>(InputAction::Weapon)].held=true;input.movement=facing;}else{input.buttons[static_cast<int>(InputAction::Attack)].held=true;input.buttons[static_cast<int>(InputAction::Attack)].pressed=n%6==0;}}
            tick(input);
        }
        settle();need(game.session().flag(239)&&game.mapId()==70,"Public first guard shooting reaches its real ending");std::cout<<"Natural first guard score="<<game.session().variable(395)<<" HP="<<game.session().hp()<<std::endl;
    }
    void firstBattle(){
        action("m4-e10");equip(11);for(int n=0;n<10;++n){press(InputAction::Weapon);}for(int n=0;n<30;++n){tick();}
        for(int n=0;n<20000&&!game.session().flag(101);++n){
            if(game.foreground()!=ForegroundKind::None){tick(n%12==0?key():InputFrame{});continue;}
            if(game.session().hp()<60){if(const int medicine=healingItem()){item(medicine);continue;}}
            InputFrame f;const auto player=game.world().player().motion.tile();const auto enemy=findCharacter(game.world(),"m4-e4").motion.tile();
            const auto facing=std::abs(enemy.x-player.x)>=std::abs(enemy.y-player.y)?(enemy.x>player.x?Direction::Right:Direction::Left):(enemy.y>player.y?Direction::Down:Direction::Up);
            if(!game.raised()){f=key(InputAction::Weapon);}
            else if(game.world().player().motion.facing()!=facing){f.movement=facing;f.buttons[static_cast<int>(InputAction::Weapon)].held=true;}
            else{f.buttons[static_cast<int>(InputAction::Attack)].held=true;f.buttons[static_cast<int>(InputAction::Attack)].pressed=n%30==0;}
            tick(f);
        }
        std::cerr<<"First battle HP="<<game.session().hp()<<" rounds="<<game.session().magazine(11)<<" raised="<<game.raised()<<" player="<<game.world().player().motion.tile().x<<","<<game.world().player().motion.tile().y<<" facing="<<static_cast<int>(game.world().player().motion.facing())<<" enemy="<<findCharacter(game.world(),"m4-e4").motion.tile().x<<","<<findCharacter(game.world(),"m4-e4").motion.tile().y<<" controls="<<game.session().flag(1)<<game.session().flag(18)<<std::endl;
        need(game.session().flag(101),"Public fire input defeats first soldier");action("m4-e12");need(game.session().flag(54),"Actual corpse supply obtained");need(game.session().count(24)==1&&game.session().count(31)==(game.session().difficulty()=="hard"?0:30),"Original M4 knife/all-difficulty initial ammunition branch differs");
    }
    void fight(const std::string& actor,int endFact){
        const auto clearShot=[&](TilePosition from,TilePosition target){
            if(!game.world().containsTile(from)||(from.x!=target.x&&from.y!=target.y)){return false;}
            const auto direction=from.x!=target.x?(target.x>from.x?Direction::Right:Direction::Left):(target.y>from.y?Direction::Down:Direction::Up);auto probe=game.world().player();
            while(std::abs(from.x-target.x)+std::abs(from.y-target.y)>1){probe.motion=CharacterMotion(from,direction,probe.motion.speed());if(!game.world().canMove(probe,direction)){return false;}from=adjacentTile(from,direction);}return true;
        };
        const int weapon=game.session().equipped(1);const bool keepRange=(game.content().enemies.contains(actor)&&game.content().enemies.at(actor).rules.value("behavior",std::string{})=="pursuer")||(game.content().expansion.enemies.contains(actor)&&game.content().expansion.enemies.at(actor).behavior==EnemyBehaviorKind::Licker);
        for(int n=0;n<18000&&!game.session().flag(endFact);++n){
            if(game.foreground()!=ForegroundKind::None){tick(n%12==0?key():InputFrame{});continue;}
            if(game.session().hp()<60){if(const int medicine=healingItem()){item(medicine);continue;}}
            if(game.content().expansion.enemies.contains(actor)&&game.content().expansion.enemies.at(actor).behavior==EnemyBehaviorKind::Licker){const auto views=game.enemyStates();const auto target=std::find_if(views.begin(),views.end(),[&](const auto& v){return v.id==actor;});if(target!=views.end()&&target->sourceState>=7&&target->sourceState<=9){tick(n%2==0?key(InputAction::Down):InputFrame{});continue;}}
            const auto p=game.world().player().motion.tile(),e=findCharacter(game.world(),actor).motion.tile();if(n%500==0){std::cout<<"Combat "<<actor<<" player="<<p.x<<","<<p.y<<" enemy="<<e.x<<","<<e.y<<" face="<<static_cast<int>(game.world().player().motion.facing())<<" raised="<<game.raised()<<" HP="<<game.session().hp()<<" ammo="<<game.session().magazine(weapon)<<std::endl;}
            const auto facing=std::abs(e.x-p.x)>=std::abs(e.y-p.y)?(e.x>p.x?Direction::Right:Direction::Left):(e.y>p.y?Direction::Down:Direction::Up);
            InputFrame input;
            const int separation=std::abs(e.x-p.x)+std::abs(e.y-p.y);
            if(keepRange&&(separation<=3||(!game.session().magazine(weapon)&&separation<=7))){
                int best=separation;for(auto dir:{Direction::Down,Direction::Left,Direction::Right,Direction::Up}){const auto tile=adjacentTile(p,dir);const int away=std::abs(e.x-tile.x)+std::abs(e.y-tile.y);if(away>best&&game.world().canMove(game.world().player(),dir)){input.movement=dir;best=away;}}
                if(input.movement){tick(input);continue;}
            }
            if(!game.raised()){
                const int distance=std::abs(e.x-p.x)+std::abs(e.y-p.y);int best=distance;
                if(distance<=4){for(auto dir:{Direction::Down,Direction::Left,Direction::Right,Direction::Up}){const auto next=adjacentTile(p,dir);const int away=std::abs(e.x-next.x)+std::abs(e.y-next.y);if(away>best&&game.world().canMove(game.world().player(),dir)){input.movement=dir;best=away;}}}
                if(!input.movement){input=key(InputAction::Weapon);}
            }
            else if(keepRange&&!game.session().magazine(weapon)){input.buttons[static_cast<int>(InputAction::Attack)].held=true;input.buttons[static_cast<int>(InputAction::Attack)].pressed=n%30==0;}
            else if(keepRange&&(separation>6||!clearShot(p,e))){
                for(int radius:{4,5,3}){for(auto side:{Direction::Down,Direction::Left,Direction::Right,Direction::Up}){const auto delta=adjacentTile({0,0},side);const TilePosition goal{e.x+delta.x*radius,e.y+delta.y*radius};if(clearShot(goal,e)){if(const auto step=next(goal)){input.movement=step;break;}}}if(input.movement){break;}}
                if(!input.movement){for(auto side:{Direction::Down,Direction::Left,Direction::Right,Direction::Up}){if(auto step=next(adjacentTile(e,side))){input.movement=step;break;}}}
            }
            else if(!keepRange&&(std::abs(e.x-p.x)+std::abs(e.y-p.y)>6||(e.x!=p.x&&e.y!=p.y))){for(auto side:{Direction::Down,Direction::Left,Direction::Right,Direction::Up}){if(auto step=next(adjacentTile(e,side))){input.movement=step;break;}}}else if(game.world().player().motion.facing()!=facing){input.buttons[static_cast<int>(InputAction::Weapon)].held=true;input.movement=facing;}
            else{input.buttons[static_cast<int>(InputAction::Attack)].held=true;input.buttons[static_cast<int>(InputAction::Attack)].pressed=n%30==0;}
            tick(input);
        }
        need(game.session().flag(endFact),"Public fire did not reach fact "+std::to_string(endFact)+" at "+actor+" weapon="+std::to_string(game.session().equipped(1))+" controls="+std::to_string(game.session().flag(1))+std::to_string(game.session().flag(18))+" recovery="+std::to_string(game.session().flag(45))+" reserves="+std::to_string(game.session().count(42)));
        settle();std::cout<<"Fight "<<actor<<" HP="<<game.session().hp()<<" rounds="<<game.session().magazine(weapon)<<std::endl;
    }
    void crocodile(){
        need(game.mapId()==109,"Natural boss intro reaches battle");
        for(int n=0;n<24000&&game.mapId()==109;++n){
            if(game.foreground()!=ForegroundKind::None){tick(n%12==0?key():InputFrame{});continue;}
            if(game.session().hp()<60){if(const int medicine=healingItem()){item(medicine);continue;}}
            if(n%300==0){std::cout<<"Crocodile damage="<<game.session().variable(89)<<" HP="<<game.session().hp()<<" rounds="<<game.session().magazine(21)<<" body="<<game.session().variable(84)<<" mode="<<game.session().variable(91)<<std::endl;}
            InputFrame input;
            if(!game.raised()){input=key(InputAction::Weapon);}
            else if(game.world().player().motion.facing()!=Direction::Up){input.movement=Direction::Up;}
            else{input.buttons[static_cast<int>(InputAction::Attack)].held=true;input.buttons[static_cast<int>(InputAction::Attack)].pressed=n%30==0;}
            tick(input);
        }
        need(game.mapId()==110,"Public DE fire reaches actual crocodile victory");
        for(int n=0;n<600&&(!game.session().flag(1)||!game.session().flag(18)||game.foreground()!=ForegroundKind::None);++n){tick(n%12==0?key():InputFrame{});}settle();
    }
    void guardSecond(){
        need(game.mapId()==117,"Natural vent intro reaches second guard");const auto& slots=game.content().full->at("runtime").at("evacuation").at("runtime_profile").at("second_guard").at("slots");
        bool grenades=reserveMainForHangar&&game.session().equipped(2)==29;
        for(int n=0;n<18000&&!game.session().flag(341);++n){
            if(n%600==0){std::cout<<"Final guard tick="<<n<<" HP="<<game.session().hp()<<" ammo="<<game.session().magazine(game.session().equipped(1))<<" kills="<<game.session().variable(269)<<" timer="<<game.session().eventTimer().remainingTicks<<std::endl;}
            if(game.foreground()!=ForegroundKind::None){tick(n%12==0?key():InputFrame{});continue;}
            if(game.mapId()!=117||!game.session().flag(1)||!game.session().flag(18)){tick();continue;}
            const auto playerTile=game.world().player().motion.tile();TilePosition nearestTile{};int nearestDistance=10000;
            for(const auto& slot:slots){const auto& actor=findCharacter(game.world(),slot.at("object"));if(!actor.active||game.session().flag(slot.at("dead_fact"))||actor.motion.tile()==TilePosition{0,0}){continue;}const auto tile=actor.motion.tile();const int d=std::abs(tile.x-playerTile.x)+std::abs(tile.y-playerTile.y);if(d<nearestDistance){nearestDistance=d;nearestTile=tile;}}
            if(nearestDistance<=1||(game.session().hp()<65&&nearestDistance<=3)){
                std::optional<Direction> retreat;int best=nearestDistance;for(auto direction:{Direction::Left,Direction::Right,Direction::Up,Direction::Down}){const auto tile=adjacentTile(playerTile,direction);const int distance=std::abs(tile.x-nearestTile.x)+std::abs(tile.y-nearestTile.y);if(distance>best&&game.world().canMove(game.world().player(),direction)){best=distance;retreat=direction;}}
                if(retreat){InputFrame move;move.movement=retreat;tick(move);continue;}
            }
            if(game.session().hp()<65&&!game.session().flag(10)&&nearestDistance>3){if(const int medicine=healingItem()){item(medicine);continue;}}
            if(game.session().equipped(1)==21&&!game.session().magazine(21)&&!game.session().count(42)&&!game.session().count(43)){equip(16);continue;}
            if(!grenades&&game.session().equipped(1)==16&&!game.session().magazine(16)&&!game.session().count(35)){if(game.session().equipped(2)==29){lower();grenades=true;}else{equip(19);}continue;}
            if(grenades&&game.session().equipped(2)!=29){grenades=false;equip(reserveMainForHangar?21:19);continue;}
            const auto player=game.world().player().motion.tile(),companion=findCharacter(game.world(),"m117-e6").motion.tile();std::string target;int distance=10000;

            for(const auto& [number,slot]:slots.items()){if(number=="8"){continue;}const auto id=slot.at("object").get<std::string>();const auto& actor=findCharacter(game.world(),id);if(!actor.active||game.session().flag(slot.at("dead_fact"))||actor.motion.tile()==TilePosition{0,0}||(reserveMainForHangar&&game.session().variable(slot.at("source_slots").at("damage"))>=100)){continue;}const auto tile=actor.motion.tile();const int d=std::abs(tile.x-companion.x)+std::abs(tile.y-companion.y);if(d<distance){target=id;distance=d;}}
            InputFrame input;
            if(!game.raised()){input=key(grenades?InputAction::AuxiliaryWeapon:InputAction::Weapon);}
            else if(!target.empty()){const auto enemy=findCharacter(game.world(),target).motion.tile();const auto facing=std::abs(enemy.x-player.x)>=std::abs(enemy.y-player.y)?(enemy.x>player.x?Direction::Right:Direction::Left):(enemy.y>player.y?Direction::Down:Direction::Up);
                if(reserveMainForHangar&&grenades&&((enemy.x!=player.x&&enemy.y!=player.y)||std::abs(enemy.x-player.x)+std::abs(enemy.y-player.y)>4)){
                    for(auto side:{Direction::Down,Direction::Left,Direction::Right,Direction::Up}){auto target=enemy;for(int step=0;step<3;++step){target=adjacentTile(target,side);}input.movement=next(target);if(input.movement){break;}}
                }else if(enemy.x!=player.x&&enemy.y!=player.y){input.movement=std::abs(enemy.x-player.x)>=std::abs(enemy.y-player.y)?(enemy.y>player.y?Direction::Down:Direction::Up):(enemy.x>player.x?Direction::Right:Direction::Left);}else if(game.world().player().motion.facing()!=facing){input.movement=facing;input.buttons[static_cast<int>(InputAction::Weapon)].held=true;}else{input.buttons[static_cast<int>(InputAction::Attack)].held=true;input.buttons[static_cast<int>(InputAction::Attack)].pressed=n%6==0;}}
            tick(input);
        }
        need(game.session().flag(341),"Public final guard input reaches a real timer/quota ending");
        for(int n=0;n<60000&&(game.mapId()!=119||game.foreground()!=ForegroundKind::None);++n){tick(n%12==0?key():InputFrame{});}need(game.mapId()==119,"Actual second guard score/story reaches119");
        std::cout<<"Natural final guard score="<<game.session().variable(394)<<" HP="<<game.session().hp()<<std::endl;
    }
};
}
int main(int argc,char** argv){try{
    need(argc==4||argc==5,"usage: f4full_natural_checks package isolated-save-dir new|new-next|new-a|new-black|bc|bc-tank|bc-ug|bc-power|bc-parking|g0-visit|g0-stage|g9-exfont|g9-exfont-load|load-only");
    const auto begin=std::chrono::steady_clock::now();auto content=loadCombatContent(argv[1]);
    std::cout<<"loader_seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<" maps="<<content.maps.size()<<" enemies="<<content.enemies.size()<<std::endl;
    if(std::string(argv[3])=="load-only"){return 0;}
    NaturalRoute route(std::move(content));if(argc==5){const std::string difficulty=argv[4];need(difficulty=="easy"||difficulty=="normal"||difficulty=="hard","Unknown route difficulty");route.difficultySelection=difficulty=="easy"?0:difficulty=="hard"?2:1;}route.saves=argv[2];route.game.configureSaveDirectory(argv[2]);finishTitleOpening(route.game);
    const auto finishG0=[&]{
                const auto ordinary=std::filesystem::path(argv[2])/"slot-01.json";
                std::filesystem::copy_file(ordinary,std::filesystem::path(argv[2])/"natural-ordinary.json",std::filesystem::copy_options::overwrite_existing);
                if(route.game.session().difficulty()=="hard"){
                    if(!route.game.session().flag(355)){
                        const int rounds=route.game.session().count(36),bullets=route.game.session().count(33);route.action("m122-e17");
                        need(route.game.session().flag(355)&&route.game.session().count(36)==rounds+6&&route.game.session().count(33)==bullets+7,"Original M122 E17 supply was not collected through public input");
                        route.action("m122-e17");need(route.game.session().count(36)==rounds+6&&route.game.session().count(33)==bullets+7,"Original M122 supply repeated");
                    }
                    if(!route.game.session().flag(501)){
                        const int sprays=route.game.session().count(1);route.action("m122-e19");need(route.game.session().flag(501)&&route.game.session().count(1)==sprays+1,"Original M122 E19 spray was not collected through public input");
                        route.action("m122-e19");need(route.game.session().count(1)==sprays+1,"Original M122 spray repeated");
                    }
                    std::cout<<"G0 actual original supplies S355/S501="<<route.game.session().flag(355)<<route.game.session().flag(501)<<" flameReserve="<<route.game.session().count(36)<<" sprays="<<route.game.session().count(1)<<std::endl;
                }
                if(route.game.session().difficulty()=="hard"&&route.game.content().expansion.mixingRecipe&&route.game.session().knowsSkill(3)&&route.game.session().canMix(*route.game.content().expansion.mixingRecipe)){
                    const auto recipe=*route.game.content().expansion.mixingRecipe;const auto stock=route.game.session().exportState().inventory;
                    route.openMenu();route.selectMenuRow(1);route.selectMenuRow(3);for(int n=0;n<2000&&route.game.mode()!=ApplicationMode::Equipment;++n){route.tick(n%8==0?key():InputFrame{});}
                    need(route.game.mode()==ApplicationMode::Equipment,"Original M122 mixing did not return to menu");for(int n=0;n<3&&route.game.mode()==ApplicationMode::Equipment;++n){route.press(InputAction::Cancel);}
                    for(auto [item,amount]:recipe.ingredients){need(route.game.session().count(item)==stock.at(item)-amount,"Original M122 mixing did not consume real ingredients");}
                    need(route.game.session().count(recipe.result)==(stock.contains(recipe.result)?stock.at(recipe.result):0)+recipe.count,"Original M122 mixing result missing");
                }
                const int count=route.game.session().saveCount(),statistic=route.game.session().variable(399),spray=route.game.session().variable(382),score=route.game.session().variable(394);
                if(route.game.session().difficulty()=="hard"&&route.game.session().count(54)){route.equip(54,4);}
                if(!route.game.session().magazine(21)&&!route.game.session().count(42)&&!route.game.session().count(43)){route.equip(route.game.session().difficulty()=="hard"?17:19);}else{route.equip(21);}
                if(route.game.session().magazine(route.game.session().equipped(1))==0){
                    route.press(InputAction::Weapon);for(int n=0;n<200;++n){route.tick();}
                    route.press(InputAction::Attack);for(int n=0;n<200;++n){route.tick();}
                    need(route.game.session().magazine(route.game.session().equipped(1))>0,"Natural safe-window actual weapon reload failed");route.lower();
                }
                route.controlledBossRandom=true;
                int usedG0Sprays=0;
                const auto waitForMenu=[&]{for(int n=0;n<120;++n){route.tick();}};
                const auto menuStillSafe=[&]{return route.game.mapId()==120&&!route.game.session().flag(350)&&route.game.foreground()==ForegroundKind::None;};
                const auto selectAvailable=[&]{
                    if(route.game.session().flag(10)||route.game.session().flag(45)||!route.game.session().flag(1)||!route.game.session().flag(18)||route.game.foreground()!=ForegroundKind::None||route.game.dialogue().active()){return;}
                    if(route.game.session().hp()<65&&route.game.session().flag(1)&&route.game.session().flag(18)&&!route.game.session().flag(45)&&route.game.foreground()==ForegroundKind::None&&!route.game.dialogue().active()){
                        if(const int medicine=route.healingItem()){waitForMenu();if(!menuStillSafe()){return;}route.item(medicine);usedG0Sprays+=medicine==1;std::cout<<"G0 public medicine="<<medicine<<" HP="<<route.game.session().hp()<<std::endl;}
                    }
                    const auto& session=route.game.session();if(session.magazine(session.equipped(1))>0||session.hasReserve()){return;}
                    for(int id:{21,11,19,17,16}){
                        const auto& item=route.game.content().items.at(id);
                        if((session.count(id)||session.equipped(1)==id)&&(session.magazine(id)>0||(item.regularReload&&(session.count(item.looseItem)>0||session.count(item.boxItem)>0)))){waitForMenu();if(!menuStillSafe()){return;}route.equip(id);std::cout<<"G0 public equipment switch weapon="<<id<<" magazine="<<session.magazine(id)<<std::endl;return;}
                    }
                    throw std::runtime_error("G0 actual owned weapons and reserves exhausted");
                };
                std::cout<<"G0 actual weapon="<<route.game.session().equipped(1)<<" rounds="<<route.game.session().magazine(route.game.session().equipped(1))<<" map="<<route.game.mapId()<<" stock5="<<route.game.session().count(5)<<" green="<<route.game.session().count(2)<<" foreground="<<int(route.game.foreground())<<" moving="<<route.game.world().player().motion.moving()<<std::endl;
                route.collectG0Extras=true;route.extraG0End=route.extraG0Corpse=0;route.g0EndSound=readSound(route.game.content().cue("m120-e13-p18-c4").at("sound")).resource;
                g0natural::driveVictory(route.game,route.game.session(),route.game.content(),[&](const InputFrame& input){route.record(input);if(route.inputCount%10000==0){std::cout<<"G0 path map="<<route.game.mapId()<<" foreground="<<int(route.game.foreground())<<" moving="<<route.game.world().player().motion.moving()<<" player="<<route.game.world().player().motion.tile().x<<","<<route.game.world().player().motion.tile().y<<" HP="<<route.game.session().hp()<<std::endl;}},selectAvailable,[&](const std::string& id){return id=="m120-e13-p18-c4"?route.extraG0End:route.extraG0Corpse;});route.collectG0Extras=false;
                need(route.controlledBossCalls>0,"Public Load discarded its configured source random provider");
                const auto magazines=route.game.session().exportState().magazines;
                std::vector<int> maps{122};
                for(int n=0;n<50000&&!route.game.inputPromptView().cursor;++n){route.tick(n%8==0?key():InputFrame{});if(maps.back()!=route.game.mapId()){maps.push_back(route.game.mapId());}}
                need(maps==std::vector<int>({122,123,124,125,124,125,126})&&route.game.foreground()==ForegroundKind::Content,"Natural stage aviation/question incomplete");
                route.press();need(route.game.mode()==ApplicationMode::Saving,"Native Chapter yes must enter Application FileMenu");
                for(int n=0;n<1000&&route.game.mode()==ApplicationMode::Saving;++n){const auto v=route.game.fileMenuView();route.tick(v&&v->confirmation?key(v->answer?InputAction::Down:InputAction::Confirm):(n%8==0?key():InputFrame{}));}
                need(route.game.mode()==ApplicationMode::Playing&&route.game.session().saveCount()==count+1&&route.game.session().variable(399)==statistic,"Chapter commit must increment native count only once");
                SaveFiles files(route.game.content(),argv[2]);const auto chapter=files.read(1);
                need(chapter.continuation.kind=="chapter_handoff_after_file"&&chapter.session.magazines==magazines,"Natural Chapter disk lost continuation/ammunition");
                need(route.game.session().variable(382)==spray+usedG0Sprays&&route.game.session().variable(394)==score&&score==60&&route.game.session().count(15)==1,"Natural score60/reward/statistics differ");
                for(int n=0;n<30000&&(!route.game.session().flag(360)||route.game.foreground()!=ForegroundKind::None);++n){route.tick(n%8==0?key():InputFrame{});}
                need(route.game.mapId()==127&&route.game.world().player().motion.tile()==TilePosition{11,7}&&route.game.session().flag(360)&&route.game.session().flag(1)&&route.game.session().flag(18),"Direct full wake must release original final tile/controls");
                need(!route.game.session().knowsSkill(3)&&route.game.session().knowsSkill(4)&&route.game.session().hp()==route.game.session().maxHp(),"Direct wake FullHeal/skills incomplete");
                route.equip(15);const int priorI15=route.game.session().magazine(15);route.press(InputAction::Weapon);for(int n=0;n<200;++n){route.tick();}need(route.game.raised(),"Stage public I15 raise failed");
                route.press(InputAction::Attack);for(int n=0;n<200;++n){route.tick();}const int firstI15=priorI15>0?priorI15-1:route.game.content().items.at(15).capacity;need(route.game.session().magazine(15)==firstI15,"Stage I15 first public request changed source ammunition");
                route.press(InputAction::Attack);for(int n=0;n<200;++n){route.tick();}need(route.game.session().magazine(15)==firstI15-1,"Stage I15 subsequent fire failed");route.lower();
                const auto gifts=route.game.session().exportState().inventory;const int deAmmo=route.game.session().magazine(21);
                route.action("m127-e7");route.action("m127-e7");route.action("m127-e8");route.action("m127-e8");
                for(auto [id,amount]:std::map<int,int>{{39,2},{36,6},{37,2},{43,4},{21,4}}){const int old=gifts.contains(id)?gifts.at(id):0;need(route.game.session().count(id)==std::min(99,old+amount),"Stage repeated gifts differ");}need(route.game.session().magazine(21)==deAmmo,"Repeated DE gift refilled shared magazine");
                route.openMenu();const auto paused=route.game.session().exportState();const auto position=route.game.world().player().motion.tile();for(int n=0;n<600;++n){route.tick();}need(route.game.session().exportState()==paused&&route.game.world().player().motion.tile()==position,"Equipment menu changed game state");route.press(InputAction::Cancel);
                route.action("m127-e3");if(route.game.mode()==ApplicationMode::Boundary){need(route.game.exitRequest()->map==128,"Natural PF21 boundary absent");route.press(InputAction::Cancel);need(route.game.mode()==ApplicationMode::Playing,"PF21 boundary Cancel did not restore control");}else{need(route.game.mode()==ApplicationMode::Playing&&route.game.mapId()==128,"Delivered G1 consumer did not accept actual wake exit");}
                need(g0natural::read(ordinary.string().c_str())==encodeSave(route.game.content(),chapter),"Natural actions rewrote Chapter disk");
                std::cout<<"NATURAL_G0_STAGE_COMPLETE score="<<score<<" count="<<route.game.session().saveCount()<<" V399="<<statistic<<" V382="<<spray<<" awake=S360 I15="<<route.game.session().magazine(15)<<" gifts=repeat consumer_map="<<route.game.mapId()<<" inputs="<<route.inputCount<<" ticks="<<route.ticks<<" RNG=guard-easy-low-or-hard-02-safe,boss-source-controlled no-state-injection\n";
    };
    std::istringstream modes(argv[3]);std::string mode;while(std::getline(modes,mode,',')){
    if(route.game.hasGame()){route.game.returnToTitle();finishTitleOpening(route.game);}route.inputs.close();
    route.inputs.open(std::filesystem::path(argv[2])/(mode=="g0-stage"||mode=="recover-f5"?"public-inputs.jsonl":"public-inputs-"+mode+".jsonl"));need(route.inputs.is_open(),"Cannot open natural input trace");
    if(mode=="new"){route.opening();route.firstBattle();route.action("m4-e2");route.action("m6-e13");route.travel(14);for(const auto* id:{"m14-e4","m14-e5","m14-e6"}){route.action(id);}route.travel(16);route.saveAtCrow();}
    else if(mode=="new-next"){route.titleLoad();need(route.game.mode()==ApplicationMode::Playing&&route.game.mapId()==16,"Natural new-game checkpoint map");route.settle();route.equip(11);route.action("m16-e4");for(const auto* id:{"m17-e3","m17-e5"}){route.fight(id,route.game.content().enemies.at(id).deadFlag);}route.action("m17-e7");route.action("m17-e8");route.action("m17-e2");route.travel(13);route.action("m13-e7");route.action("m13-e7");route.action("m18-e11");route.travel(16);route.saveAtCrow();}
    else if(mode=="new-a"){route.titleLoad();route.newArea();}
    else if(mode=="g9-exfont"||mode=="g9-exfont-load"){
        route.titleLoad();need(route.game.mapId()==16&&route.game.session().flag(163)&&route.game.session().count(80)==1&&!route.game.session().flag(180),"Actual ordinary source must be after training and before Sena");
        rejectExFontCandidates(route.game,argv[1],route.saves);
        if(mode=="g9-exfont-load"){
            const auto& message=route.game.content().messages.at("m61-e2-p2-c36").message;
            need(message.exfontImage=="g9-message-exfont"&&message.lines[1].raw=="\\<「———$g」"&&message.lines[1].tokens[2].kind==MessageTokenKind::ExFont&&message.lines[1].tokens[2].value==32,"Rebased candidate lost its unchanged M61 bitmap binding");
            route.tick();need(route.game.mode()==ApplicationMode::Playing&&route.game.mapId()==16,"Rejected rebased candidates left the old game unusable");
            std::cout<<"EXFONT_REBASE_COMPLETE map=16 old_game_operational=1 source_save_count="<<route.game.session().saveCount()<<" index=32 compatibility="<<route.game.content().compatibility<<std::endl;continue;
        }
        route.observeExFont=true;route.equip(21);
        route.travel(14);route.action("m14-e21");route.action("m14-e21");route.action("m59-e7");route.action("m60-e3");route.action("m40-e11");
        need(route.game.session().count(73)==1,"Medical key picked naturally");route.action("m40-e10");route.action("m60-e2");route.action("m59-e8");
        route.travel(29);route.action("m29-e10");route.action("m29-e10");need(route.game.mapId()==42,"Diamond key opens courtyard");
        route.action("m42-e9");route.walk({16,6});route.fight("m61-e3",178);route.action("m61-e2");
        need(route.game.session().flag(180)&&route.exfontObserved,"Actual Sena discovery did not display the original ExFont page");
        std::cout<<"NATURAL_EXFONT_COMPLETE S180=1 ticks="<<route.ticks<<" inputs="<<route.inputCount<<" no-state-injection no-formal-writes\n";
    }
    else if(mode=="g8-smoke-save"){route.titleLoad();route.travel(22);route.saveAtCrow();route.travel(16);route.saveAtCrow();}
    else if(mode=="g0-resume"){route.titleLoad();need(route.game.mapId()==122&&route.game.session().flag(347),"Actual rescued M122 crow prerequisite");finishG0();}
    else if(mode=="recover-f5"){
        SaveFiles files(route.game.content(),argv[2]);const auto source=files.read(5);need(source.scene.map==107&&source.continuation.kind=="ordinary","Recovery source must be validated M107 ordinary slot5");
        route.trackPresentation=true;route.recoveredPresentation=source.presentation;route.titleLoad(5);
        const int count=route.game.session().saveCount(),stats=route.game.session().variable(399);
        if(!route.game.session().flag(321)){route.solveDrainage();}
        route.equip(21);
        if(!route.game.session().flag(333)){route.action("m107-e2");route.walk({28,11});route.walk({29,11});route.action("m112-e14");route.equip(53,4);route.walk({1,11});route.walk({0,11});route.action("m111-e4");}
        if(!route.game.session().flag(323)){route.action("m107-e2");route.action("m111-e5");route.action("m108-e2");route.walk({28,10});route.walk({27,10});for(int n=0;n<12000&&(route.game.mapId()!=109||route.game.foreground()!=ForegroundKind::None);++n){route.tick(n%12==0?key():InputFrame{});}route.crocodile();route.walk({9,0});route.action("m113-e11");route.action("m111-e4");}
        route.equip(21);route.equip(29,2);route.action("m107-e2");route.action("m111-e28");route.action("m111-e5");
        for(const auto* herb:{"m113-e3","m113-e4","m113-e5"}){route.action(herb);}
        route.walk({37,5});route.walk({37,4});route.action("m114-e24");route.action("m115-e3");
        for(int n=0;n<30000&&(route.game.mapId()!=117||route.game.foreground()!=ForegroundKind::None||!route.game.session().flag(18));++n){route.tick(n%12==0?key():InputFrame{});}if(route.game.session().variable(635)>0&&route.game.session().count(56)){route.equip(56);}route.guardSecond();route.action("m119-e4");
        route.walk({11,29});route.walk({11,28});route.settle();
        need(route.game.session().saveCount()==count&&route.game.session().variable(399)==stats,"Recovery replay duplicated saves");
        const auto saved=route.game.captureF5Recovery();save_platform::writeExclusive(std::filesystem::path(argv[2])/"candidate.json",encodeSave(route.game.content(),saved));
        std::cout<<"F5_RECOVERY_CANDIDATE map=121 count="<<count<<" V399="<<stats<<" HP="<<route.game.session().hp()<<" inputs="<<route.inputCount<<" no-state-injection no-formal-writes\n";return 0;
    }
    else if(mode=="recovery-load"||mode=="recovery-load-only"){
        route.titleLoad(5);need(route.game.mapId()==121&&route.game.world().player().motion.tile()==TilePosition{11,28},"Independent recovery process did not land M121");
        const auto initial=route.game.session().exportState();const int count=route.game.session().saveCount(),stats=route.game.session().variable(399);
        route.openMenu();const auto paused=route.game.session().exportState();for(int n=0;n<120;++n){route.tick();}need(route.game.session().exportState()==paused,"Recovery menu did not pause state");route.press(InputAction::Cancel);
        route.walk({11,29});route.walk({11,28});route.game.returnToTitle();finishTitleOpening(route.game);route.titleLoad(5);
        need(route.game.session().exportState()==initial,"Repeated public Load changed inventory or counters");
        if(mode=="recovery-load-only"){std::cout<<"RECOVERY_PUBLIC_SLOT5_LOAD M121 menu/movement/two Loads no file writes passed\n";return 0;}
        route.action("m121-e2");need(route.game.mapId()==120&&!route.game.session().flag(346),"Recovery cannot naturally enter hangar first visit");
        route.action("m120-e10");for(int n=0;n<60000&&!route.game.session().flag(347);++n){route.tick(n%12==0?key():InputFrame{});}route.settle();
        need(route.game.mapId()==122&&route.game.session().flag(346)&&route.game.session().flag(347),"Recovery first-visit cannot reach ordinary crow");
        route.walk({11,10});route.walk({11,9});route.press();route.settle();route.action("m122-e19");route.saveAtCrow();
        need(route.game.session().saveCount()==count+1&&route.game.session().variable(399)==stats+1,"Next ordinary save changed recovery counter semantics");
        std::cout<<"RECOVERY_INDEPENDENT_LOAD M121 control/movement/menu/twice-Load and natural M120/M122 ordinary save passed\n";return 0;
    }
    else if(mode=="g0-load"){
        route.titleLoad();need(route.game.mapId()==122&&route.game.session().flag(346)&&route.game.session().flag(347),"Independent process Load reaches stable hangar window");
        auto count=route.game.session().saveCount();auto stats=route.game.session().variable(399);int fire=route.game.session().count(36),sprays=route.game.session().count(1);
        for(int n=0;n<120;++n){route.tick();}need(route.game.foreground()==ForegroundKind::None,"Loaded high pages do not replay first visit");
        route.action("m122-e17");route.action("m122-e19");need(route.game.session().count(36)==fire&&route.game.session().count(1)==sprays,"Load cannot repeat supplies");
        need(route.game.session().saveCount()==count&&route.game.session().variable(399)==stats,"Load itself never repeats successful-save statistics");
        std::cout<<"G0 independent-process safe window, inventory and high-page restore passed"<<std::endl;
    }
    else if(mode=="new-layout"){
        route.titleLoad();for(int y=0;y<40;++y){std::cout<<y<<" ";for(int x=0;x<25;++x){std::cout<<(route.next({x,y})||route.game.world().player().motion.tile()==TilePosition{x,y}?'o':'.');}std::cout<<std::endl;}
        for(const auto& [id,obj]:route.game.content().objects){if(obj.map==16){const auto* page=route.game.content().activeVariant(obj,route.game.session());const auto& actor=findCharacter(route.game.world(),id);std::cout<<id<<" page="<<(page?page->page:0)<<" layer="<<actor.layer<<" at="<<actor.motion.tile().x<<","<<actor.motion.tile().y<<std::endl;}}
    }
    else if(mode=="new-black"){route.titleLoad();route.newBlack();}
    else if(mode=="bc-tank"){
        route.titleLoad();need(route.game.session().flag(195),"Natural medical-return prerequisite");
        route.travel(20);route.action("m20-e3");
        if(!route.game.session().flag(198)){route.action("m51-e9");need(route.game.session().flag(198),"Natural tank start");}
        if(!route.game.session().flag(201)){route.action("m51-e9");for(int n=0;n<80000&&(!route.game.session().flag(201)||route.game.mapId()!=51||route.game.foreground()!=ForegroundKind::None);++n){route.tick(n%12==0?key():InputFrame{});}need(route.game.session().flag(201)&&route.game.mapId()==51,"Actual unlit first visit returns to tank");}
        route.travel(41);route.action("m41-e2");route.action("m41-e2");route.action("m45-e9");
        need(route.game.session().count(75)==1&&route.game.session().flag(172),"Diamond second door and real flashlight pickup");route.travel(16);route.saveAtCrow();
    }
    else if(mode=="bc-ug"){
        route.titleLoad();need(route.game.session().count(75)==1,"Natural flashlight prerequisite");route.equip(21);
        route.travel(20);route.action("m20-e3");route.action("m51-e9");need(route.game.mapId()==66,"Tank enters lit underground");
        route.action("m66-e12");route.action("m67-e10");route.action("m69-e3");need(route.game.session().count(76)==1,"Natural iron hook pickup");
        route.action("m69-e2");route.action("m67-e8");route.action("m66-e13");route.walk({27,6});route.tick();route.item(77);
        need(route.game.session().flag(231),"Natural menu installs iron hook");route.action("m68-e12");
        route.travel(77);route.action("m77-e11");route.saveAtCrow();
    }
    else if(mode=="bc-power"){
        route.titleLoad();need(route.game.mapId()==77&&route.game.session().flag(231),"Natural installed hook checkpoint");route.equip(21);
        route.travel(76);route.action("m76-e5");need(route.game.session().count(78)==1&&route.game.session().flag(202),"Natural fuse pickup starts pursuit");
        for(const auto* id:{"m76-e7","m75-e2","m74-e9","m73-e10","m68-e10","m66-e12","m67-e10"}){route.action(id);}
        route.walk(route.game.content().objects.at("m69-e12").tile);route.tick();std::cout<<"Fuse approach HP="<<route.game.session().hp()<<" pursuer="<<route.game.session().pursuerHealth()<<" at="<<route.game.world().player().motion.tile().x<<","<<route.game.world().player().motion.tile().y<<std::endl;route.item(79);std::cout<<"Fuse installed HP="<<route.game.session().hp()<<std::endl;need(route.game.session().flag(237),"Natural menu installs fuse");route.walk({7,10});route.fight("m69-e9",226);
        need(route.game.session().flag(236)&&route.game.session().count(83)==1,"Real pursuer defeat restores power and grants card");
        route.action("m69-e2");route.action("m67-e8");route.walk(route.game.content().objects.at("m66-e22").tile);route.tick();route.item(84);need(route.game.session().flag(276),"Natural card menu unlocks source armory");route.action("m66-e19");
        for(const auto* id:{"m85-e2","m85-e11","m85-e3","m85-e4","m85-e6","m85-e7","m85-e8","m85-e10","m85-e14","m85-e15","m85-e16"}){route.action(id);}route.equip(19);route.travel(77);route.saveAtCrow();
    }
    else if(mode=="bc-blue"){
        route.titleLoad();need(route.game.session().flag(310),"Natural ruby/report checkpoint");route.equip(19);
        if(!route.game.session().count(87)&&!route.game.session().count(91)){
            route.action("m16-e15");route.action("m26-e8");route.travel(34);route.action("m34-e2");route.action("m50-e3");need(route.game.session().count(87)==1,"Natural power-gated teeth pickup");route.action("m50-e2");
        }
        if(route.game.session().conditions().poisoned){route.travel(91);route.action("m91-e13");route.item(3);need(!route.game.session().conditions().poisoned,"Actual parking blue herb cures ongoing poison");}
        route.travel(96);route.walk({10,8});route.tick();route.item(88);need(route.game.session().flag(318)&&route.game.session().count(91)==1,"Actual teeth tool grants blue sapphire");
        route.action("m96-e9");route.action("m95-e2");route.action("m84-e6");route.travel(30);route.travel(16);route.saveAtCrow();
    }
    else if(mode=="bc-final"||mode=="g0-visit"||mode=="g0-stage"||mode=="g8-end"){
        route.titleLoad();need(route.game.session().flag(323),"Natural boss victory-return checkpoint");if(mode=="g0-stage"&&route.game.session().difficulty()=="hard"){route.controlledBossRandom=true;route.reserveMainForHangar=true;}route.equip(21);route.equip(29,2);route.action("m107-e2");route.action("m111-e28");route.action("m111-e5");need(route.game.mapId()==113,"Original post-victory alternate exit");for(const auto* herb:{"m113-e3","m113-e4","m113-e5"}){route.action(herb);}route.walk({37,5});route.walk({37,4});need(route.game.mapId()==114,"Natural tall vent entry");route.action("m114-e24");route.action("m115-e3");for(int n=0;n<30000&&(route.game.mapId()!=117||route.game.foreground()!=ForegroundKind::None||!route.game.session().flag(18));++n){route.tick(n%12==0?key():InputFrame{});}if(route.game.session().variable(635)>0&&route.game.session().count(56)){route.equip(56);}route.guardSecond();route.action("m119-e4");need(route.game.mapId()==121,"Natural escort reaches hangar front");if(mode=="g8-end"){need(route.game.mapId()==121&&route.game.session().flag(341),"Three-difficulty first part naturally reaches F5 handoff");std::cout<<"NATURAL_G8_F5 difficulty="<<route.game.session().difficulty()<<" HP="<<route.game.session().hp()<<" count="<<route.game.session().saveCount()<<" ticks="<<route.ticks<<std::endl;return 0;}route.action("m121-e2");if(mode=="g0-visit"||mode=="g0-stage"){
            need(route.game.mapId()==120&&!route.game.session().flag(346)&&!route.game.session().flag(347),"Natural F5 endpoint enters first hangar without injected first-visit switches");
            route.action("m120-e10");
            for(int n=0;n<60000&&!route.game.session().flag(347);++n){route.tick(n%12==0?key():InputFrame{});}
            route.settle();need(route.game.mapId()==122&&route.game.session().flag(346)&&route.game.session().flag(347)&&route.game.playerVisible(),"Complete first-visit127C / return11C / second39C reach natural safe window");
            need(route.game.session().knowsSkill(3)&&!route.game.session().knowsSkill(4),"Natural parallel Skill4 to3 normalization");
            int bullets=route.game.session().count(33),fire=route.game.session().count(36),sprays=route.game.session().count(1);
            route.walk({11,10});route.walk({11,9});route.press();route.settle();route.action("m122-e19");
            std::cout<<"Supplies 33="<<route.game.session().count(33)<<" 36="<<route.game.session().count(36)<<" 1="<<route.game.session().count(1)<<" flags="<<route.game.session().flag(355)<<route.game.session().flag(501)<<std::endl;
            need(route.game.session().count(33)==std::min(99,bullets+7)&&route.game.session().count(36)==std::min(99,fire+6)&&route.game.session().count(1)==std::min(99,sprays+1)&&route.game.session().flag(355)&&route.game.session().flag(501),"Natural one-time source-order supplies");
            route.action("m122-e17");route.action("m122-e19");need(route.game.session().count(36)==std::min(99,fire+6)&&route.game.session().count(1)==std::min(99,sprays+1),"Natural second pickup grants nothing");
            route.equip(16);route.item(36);need(route.game.session().equipped(1)==17&&route.game.session().count(36)>0,"Natural supplied incendiary ammunition uses109 menu selection");
            if(route.game.session().difficulty()=="hard"){
                if(route.game.session().hp()<route.game.session().maxHp()&&route.game.session().count(2)){route.item(2);}
            }else{
                int sprayBefore=route.game.session().variable(382);route.item(1);need(route.game.session().variable(382)==sprayBefore+1&&route.game.session().count(1)==std::min(99,sprays+1)-1,"Natural successful spray increments109 statistic once");
            }
            route.saveAtCrow();std::cout<<"NATURAL_G0_FIRST_SAVE map=122 S346=true S347=true count="<<route.game.session().saveCount()<<std::endl;
            if(mode=="g0-stage"){
                finishG0();
            }
            return 0;
        }
        need(route.game.mode()==ApplicationMode::Boundary&&route.game.exitRequest()&&route.game.exitRequest()->completed,"Both full natural routes submit reviewed F4 completion");std::cout<<"NATURAL_F4_ENDPOINT map=121 complete=true HP="<<route.game.session().hp()<<" save_count="<<route.game.session().saveCount()<<std::endl;
    }
    else if(mode=="g0-finish"){route.titleLoad();need(route.game.mapId()==122&&route.game.session().flag(346)&&route.game.session().flag(347)&&!route.game.session().flag(350),"G0 continuation needs the actual pre-battle ordinary disk");finishG0();return 0;}
    else if(mode=="bc-boss"){
        route.titleLoad();need(route.game.session().flag(321)&&route.game.session().flag(333),"Natural drained/shoe checkpoint");route.equip(21);route.action("m107-e2");route.action("m111-e5");route.action("m108-e2");route.walk({28,10});route.walk({27,10});for(int n=0;n<12000&&(route.game.mapId()!=109||route.game.foreground()!=ForegroundKind::None);++n){route.tick(n%12==0?key():InputFrame{});}route.crocodile();route.walk({9,0});need(route.game.mapId()==113,"Actual post-victory north exit");route.action("m113-e11");route.action("m111-e4");route.saveAtCrow();
    }
    else if(mode=="bc-drainage"){
        route.titleLoad();need(route.game.mapId()==107&&route.game.session().flag(320),"Natural truth crow checkpoint");route.equip(16);if(!route.game.session().flag(321)){route.solveDrainage();}
        route.action("m107-e2");route.walk({28,11});route.walk({29,11});need(route.game.mapId()==112,"Actual drained lower side room entry");route.action("m112-e14");need(route.game.session().count(53)==1&&route.game.session().flag(333),"Natural shoe pickup before boss");route.equip(53,4);route.walk({1,11});route.walk({0,11});need(route.game.mapId()==111,"Shoe room source return");route.action("m111-e4");route.saveAtCrow();
    }
    else if(mode=="bc-truth"){
        route.titleLoad();need(route.game.session().count(89)&&route.game.session().count(91),"Natural two gems checkpoint");route.action("m16-e15");route.action("m26-e8");route.travel(34);route.travel(37);
        route.walk({10,5});route.tick();route.item(90);need(route.game.session().flag(313),"Natural ruby eye install");route.item(92);need(route.game.session().flag(315)&&route.game.mapId()==102,"Actual second eye opens transport");route.action("m102-e7");route.action("m101-e17");for(int n=0;n<15000&&(!route.game.session().flag(320)||route.game.foreground()!=ForegroundKind::None);++n){route.tick(n%12==0?key():InputFrame{});}need(route.game.mapId()==104&&route.game.session().flag(320),"Complete original truth file and trap wake");route.travel(107);route.saveAtCrow();
    }
    else if(mode=="bc-central"){
        route.titleLoad();need(route.game.session().flag(305)&&route.game.session().count(71)==1,"Natural clover key checkpoint");route.equip(16);
        route.action("m16-e17");route.action("m39-e16");
        while(route.game.mapId()==138){std::string nearest;int distance=10000;const auto p=route.game.world().player().motion.tile();for(const auto& [id,row]:route.game.content().full->at("runtime").at("truth").at("encounters").items()){const int fact=row.at("dead_fact");if(route.game.session().flag(fact)){continue;}const auto tile=findCharacter(route.game.world(),id).motion.tile();const int d=std::abs(tile.x-p.x)+std::abs(tile.y-p.y);if(d<distance){distance=d;nearest=id;}}if(nearest.empty()){break;}route.fight(nearest,route.game.content().enemies.at(nearest).deadFlag);}
        const std::string corridor="m"+std::to_string(route.game.mapId());route.action(corridor+"-e6");route.action(corridor+"-e6");need(route.game.mapId()==97&&route.game.session().flag(303),"Actual difficulty-selected central clover door");route.action("m97-e20");need(route.game.session().count(89)==1&&route.game.session().count(185)==1,"Natural central ruby and report");
        route.action("m97-e2");route.action("m"+std::to_string(route.game.mapId())+"-e5");route.travel(16);route.saveAtCrow();
    }
    else if(mode=="bc-barracks"){
        route.titleLoad();need(route.game.mapId()==79&&route.game.session().flag(240),"Natural parking crow checkpoint");route.controlledBossRandom=true;route.equip(19);
        route.action("m79-e2");route.action("m78-e3");route.action("m91-e2");need(route.game.session().flag(278),"Natural south shutter opens");route.action("m91-e11");route.action("m84-e4");route.action("m83-e19");
        route.walk({17,9});route.tick();route.item(82);need(route.game.session().flag(280),"Actual valve menu extinguishes fire");route.action("m89-e32");for(const auto* id:{"m83-e2","m83-e4"}){route.fight(id,route.game.content().enemies.at(id).deadFlag);}if(route.game.session().difficulty()=="hard"){route.equip(21);}route.action("m83-e22");for(const auto* id:{"m99-e4","m99-e6"}){route.fight(id,route.game.content().expansion.enemies.at(id).deadFlag);}route.action("m99-e3");need(route.game.session().flag(305)&&route.game.session().count(71)==1,"Natural clover key pickup");
        route.action("m99-e2");route.action("m83-e17");route.action("m84-e6");route.travel(16);route.saveAtCrow();
    }
    else if(mode=="bc-parking"){
        route.titleLoad();need(route.game.session().flag(236)&&route.game.session().flag(277),"Natural powered armory checkpoint");route.equip(21);if(route.game.session().hp()<100&&route.game.session().count(5)){route.item(5);}
        route.travel(66);route.action("m66-e12");route.action("m67-e9");route.guardFirst();route.action("m70-e28");need(route.game.session().flag(240),"Actual truck movement opens physical route");
        route.action("m70-e32");route.fight("m78-e4",route.game.content().expansion.enemies.at("m78-e4").deadFlag);route.action("m78-e8");route.action("m79-e3");route.action("m79-e4");need(route.game.session().count(81)==1&&route.game.session().count(16)==1,"Natural valve and GL pickups");route.saveAtCrow();
    }
    else if(mode=="bc"){route.titleLoad();need(route.game.mode()==ApplicationMode::Playing&&route.game.mapId()==16,"Real BC source reaches source room");route.settle();route.equip(21);if(!route.game.session().flag(163)){route.travel(20);route.action("m20-e3");if(route.game.mapId()==20){route.action("m20-e3");}need(route.game.mapId()==51,"Real old-area exit reaches training");route.action("m51-e2");for(const auto* id:{"m52-e9","m52-e11","m52-e13","m52-e15"}){route.action(id);}route.action("m52-e5");need(route.game.session().count(72)==1,"Natural training key obtained");if(!route.game.session().flag(162)){route.walk({13,5});route.fight("m52-e4",162);for(int n=0;n<1200&&!route.game.session().count(54);++n){route.tick(n%12==0?key():InputFrame{});}route.settle();need(route.game.session().count(54)==1,"Natural training defeat awards one optical sight");}route.action("m52-e17");route.action("m52-e17");need(route.game.mapId()==53,"Training back-door escape");route.action("m53-e11");route.action("m54-e13");route.action("m56-e16");route.action("m58-e7");route.action("m58-e8");route.action("m58-e6");route.action("m54-e8");need(route.game.mapId()==51&&route.game.session().flag(163),"Training back exit opens naturally");route.travel(16);route.saveAtCrow();}else{if(!route.game.session().count(80)&&!route.game.session().flag(171)){route.travel(20);route.action("m20-e3");route.action("m51-e2");route.action("m52-e17");route.action("m53-e11");route.action("m54-e11");route.action("m55-e5");route.action("m55-e7");need(route.game.session().count(80)==1,"Training side-room key obtained");route.action("m55-e6");route.action("m54-e13");route.action("m56-e16");route.action("m58-e6");route.action("m54-e8");route.travel(16);route.saveAtCrow();}route.travel(14);route.action("m14-e21");route.action("m14-e21");route.action("m59-e7");route.action("m60-e3");route.action("m40-e11");need(route.game.session().count(73)==1,"Medical key picked naturally");route.action("m40-e10");route.action("m60-e2");route.action("m59-e8");route.travel(29);route.action("m29-e10");route.action("m29-e10");need(route.game.mapId()==42,"Diamond key opens courtyard");route.action("m42-e9");route.walk({16,6});route.fight("m61-e3",178);route.action("m61-e2");need(route.game.session().flag(180),"Actual Sena discovery");route.action("m61-e14");need(route.game.session().flag(194)&&route.game.session().hp()==route.game.session().maxHp(),"Natural treatment restores actor before departure");route.travel(42);need(route.game.session().flag(181),"Natural return activates courtyard wave");route.walk({19,6});route.fight("m42-e34",196);for(const auto* enemy:{"m42-e4","m42-e7","m42-e14","m42-e2","m42-e10","m42-e17","m42-e19","m42-e21"}){const int dead=route.game.content().enemies.at(enemy).deadFlag;if(std::string(enemy)=="m42-e21"&&route.game.session().difficulty()=="easy"&&!route.game.session().flag(498)){continue;}if(!route.game.session().flag(dead)){route.fight(enemy,dead);}}for(int n=0;n<120&&!route.game.session().flag(193);++n){route.tick();}need(route.game.session().flag(193),"Original difficulty-specific death set clears courtyard");route.travel(61);for(int n=0;n<80000&&(route.game.mapId()!=25||route.game.foreground()!=ForegroundKind::None);++n){route.tick(n%12==0?key():InputFrame{});}need(route.game.session().flag(194)&&route.game.session().flag(195)&&route.game.session().count(74)==1&&route.game.mapId()==25,"Actual treatment and Map182 return grant tank key");route.travel(16);route.saveAtCrow();}}
    else{throw std::runtime_error("Unknown natural route mode");}
    if(mode!="bc-final"){std::cout<<"Natural preparation reached map="<<route.game.mapId()<<" HP="<<route.game.session().hp()<<" ticks="<<route.ticks<<"; full F4 endpoint not yet checked\n";}
}
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
