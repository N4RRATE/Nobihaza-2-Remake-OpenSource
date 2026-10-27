#include "title_navigation.h"
#include "game_application.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <queue>
#include <stdexcept>

namespace {
void need(bool ok,const std::string& message){if(!ok){throw std::runtime_error(message);}}
InputFrame key(InputAction action=InputAction::Confirm){InputFrame f;f.buttons.at(static_cast<int>(action)).pressed=true;return f;}
template<class Scene> void settle(Scene& game){
    for(int n=0;n<6000;++n){game.update(game.dialogue().active()&&n%2?key():InputFrame{});
        if(game.foreground()==ForegroundKind::None&&!game.dialogue().active()){return;}}
    throw std::runtime_error("Unsettled source flow on "+std::to_string(game.mapId()));
}
void walk(GameApplication& game,TilePosition destination){
    using Cell=std::pair<int,int>;const auto origin=game.world().player().motion.tile();
    const Cell start{origin.x,origin.y},end{destination.x,destination.y};
    std::queue<Cell> queue;std::map<Cell,std::pair<Cell,Direction>> previous;queue.push(start);previous[start]={start,Direction::Up};
    while(!queue.empty()&&!previous.contains(end)){
        const auto cell=queue.front();queue.pop();
        for(auto direction:{Direction::Up,Direction::Right,Direction::Down,Direction::Left}){
            auto actor=game.world().player();actor.motion=CharacterMotion({cell.first,cell.second},direction,actor.motion.speed());
            const auto next=adjacentTile(actor.motion.tile(),direction);const Cell target{next.x,next.y};
            if(!previous.contains(target)&&game.world().canMove(actor,direction)){previous[target]={cell,direction};queue.push(target);}
        }
    }
    need(previous.contains(end),"No lawful path to source object on "+std::to_string(game.mapId()));
    std::vector<Direction> route;for(auto cell=end;cell!=start;cell=previous.at(cell).first){route.push_back(previous.at(cell).second);}
    std::reverse(route.begin(),route.end());const int map=game.mapId();
    for(auto direction:route){InputFrame f;f.movement=direction;game.update(f);
        for(int n=0;n<200&&game.world().player().motion.moving();++n){game.update({});}
        need(game.mapId()==map,"Unexpected transfer while approaching save point");
        if(game.foreground()!=ForegroundKind::None){settle(game);}}
    need(game.world().player().motion.tile()==destination,"Movement reached save approach");
}
void faceUp(GameApplication& game){InputFrame f;f.movement=Direction::Up;game.update(f);need(!game.world().player().motion.moving(),"Facing source crow remains stationary");}
void openSave(GameApplication& game){
    game.update(key());need(game.mode()==ApplicationMode::Saving,"Original crow enters SaveController");
    for(int n=0;n<200;++n){game.update({});}
}
CombatEntry fixture(const CombatContent& content,int map,const std::string& point){
    auto e=content.entries.at("defeated-claimed");e.map=map;e.playerTile=adjacentTile(content.objects.at(point).tile,Direction::Down);e.direction=Direction::Up;
    e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.saveCount=7;e.session.variables[399]=5;
    e.session.flags[57]=e.session.flags[156]=e.session.flags[162]=true;e.session.pursuerHealth=317;
    e.session.inventory[16]=1;e.session.inventory[35]=6;e.session.magazines[16]=1;e.session.magazines[17]=e.session.magazines[18]=0;
    e.session.inventory[19]=0;e.session.mainWeapon=19;e.session.magazines[19]=7;
    e.session.flags[66]=true;e.session.inventory[61]=1;
    return e;
}
void checkLoaded(GameApplication& game,const SaveSnapshot& disk){
    need(game.mode()==ApplicationMode::Playing&&game.mapId()==disk.scene.map,"Title commits correct independent scene");
    auto expected=disk.session;expected.flags[1]=expected.flags[18]=true;expected.variables[399]=6;
    if(expected.quickshotTicks>0){expected.quickshotTicks=900;}
    need(game.session().exportState()==expected,"Application Load preserves complete persistent values and applies only finite continuation");
    need(game.session().saveCount()==8&&game.session().variable(399)==6,"Load resumes 8/6 exactly once");
    need(game.session().pursuerHealth()==317&&game.session().magazine(19)==7&&game.session().equipped(1)==19,"Spent AR/equipment and pursuer HP survive");
    need(game.session().magazine(16)==1&&game.session().magazine(17)==0&&game.session().magazine(18)==0&&game.session().count(35)==6,"Single GL loader and reserve survive without reseeding");
    need(game.session().flag(66)&&game.session().count(61)==(disk.scene.map==79?0:1)&&game.session().flag(67)==(disk.scene.map==79),"Acquired key and opened/discarded door fact survive together");
    std::string error;const auto once=game.session().exportState();
    for(int n=0;n<2;++n){need(game.restore(disk,error)&&game.session().exportState()==once,"Repeated Load never compounds counts or rewards");}
    const auto generation=game.generation();game.configureRestorePresentation([](const SceneDefinition&,const SavedPresentation&){throw std::runtime_error("injected missing texture");});
    need(!game.restore(disk,error)&&game.generation()==generation&&game.session().exportState()==once,"Failed candidate preserves current session/scene");
    game.configureRestorePresentation({});game.returnToTitle();finishTitleOpening(game);need(!game.hasGame(),"Title drops loaded session");
    need(game.newGame(error),"Real fresh NORMAL application entry starts");
    need(game.session().eventTimer()==EventTimer{}&&game.session().pursuerHealth()==500&&!game.session().conditions().poisoned&&game.session().conditions().poisonWaitTicks==0,"New game cannot inherit prior timer/HP/DOT");
    need(!game.session().flag(321)&&!game.session().flag(354)&&game.session().count(61)==0,"Fresh game clears prior water/reward/key facts");
}
void titleLoad(GameApplication& game,int slot){
    game.update(key());need(game.mode()==ApplicationMode::Loading,"Independent process enters title Load");
    for(int n=1;n<slot;++n){game.update(key(InputAction::Down));}game.update(key());
}
void checkRewardRevisit(const CombatContent& content,const SaveSnapshot& disk){
    auto restored=prepareRestore(content,disk);
    const std::vector<std::pair<int,std::string>> visits=disk.scene.map==79?
        std::vector<std::pair<int,std::string>>{{79,"m79-e4"}}:
        disk.scene.map==107?std::vector<std::pair<int,std::string>>{{112,"m112-e14"},{111,"m111-e28"}}:
        std::vector<std::pair<int,std::string>>{};
    for(const auto& [map,object]:visits){
        auto e=content.entries.at("defeated-claimed");e.map=map;e.playerTile=adjacentTile(content.objects.at(object).tile,Direction::Down);e.direction=Direction::Up;
        e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();
        const auto inventory=restored.session->exportState().inventory;const auto equipment=restored.session->equipment();
        // The loaded session visits another source scene; this fixture is not a natural route claim.
        need(content.activeVariant(content.objects.at(object),*restored.session)->page==(map==79?3:2),"Loaded acquisition fact selects original already-taken page: "+object);
        CombatScene scene(content,*restored.session,e);
        for(int n=0;n<2;++n){scene.update(key());settle(scene);}
        need(restored.session->exportState().inventory==inventory,"Loaded claimed reward remains claimed on repeat source interaction: "+object);
        need(restored.session->equipped(1)==equipment.mainWeapon&&restored.session->equipped(4)==equipment.accessory,"Reward revisit never duplicates or replaces equipment");
    }
}
void checkSaveBlocked(const CombatContent& content){
    auto c=content;auto e=fixture(c,77,"m77-e6");e.session.flags[227]=true;e.session.flags[585]=false;
    c.entries["106-blocked-save"]=e;GameApplication game(c,"106-blocked-save");
    need(c.activeVariant(c.objects.at("m77-e6"),game.session())->page==3,"S227 higherP3 covers saveP2 until S585");
    game.update(key());settle(game);
    need(game.mode()!=ApplicationMode::Saving&&!game.takeSaveRequest()&&game.session().saveCount()==7&&game.session().variable(399)==5,"Real P3 provides no save request or statistic");
    GameSession session(c.items,e.session,c.facts);CombatScene scene(c,session,e);bool denied=false;
    try{captureSave(c,scene,{scene.generation(),"m77-e6",""},{});}catch(const std::exception&){denied=true;}
    need(denied,"Synthetic request cannot bypass the unavailable source save page");
}
void trainingUpgrade(const CombatContent& content,const std::filesystem::path& root,bool writing){
    need(content.compatibility=="f4-normal-1","All cumulative stages keep reviewed identity");
    if(writing){
        need(content.fullStage=="training","Upgrade writer uses actual training package");auto c=content;auto e=fixture(c,22,"m22-e2");
        e.map=21;e.playerTile=adjacentTile(c.objects.at("m21-e9").tile,Direction::Down);e.direction=Direction::Up;
        e.session.inventory[16]=e.session.inventory[35]=0;e.session.mainWeapon=11;e.session.magazines[19]=30;
        e.session.eventTimer={EventTimerSource::Training,3000,true,false,false};c.entries["106-training-upgrade"]=e;
        GameApplication game(c,"106-training-upgrade");game.configureSaveDirectory(root);finishTitleOpening(game);game.update(key());settle(game);
        need(game.mapId()==22&&game.session().eventTimer().running,"Real training carry reaches22 before early-stage save");
        walk(game,adjacentTile(c.objects.at("m22-e2").tile,Direction::Down));faceUp(game);openSave(game);game.update(key());game.update(key());
        const auto disk=SaveFiles(c,root).read(1);need(game.session().saveCount()==8&&game.session().variable(399)==6&&disk.session.variables.at(399)==5,"Training writer has identical finite continuation");
        const auto json=nlohmann::json::parse(encodeSave(c,disk));need(json.at("schema")=="nobihaza.save.v4"&&json.at("version")==4,"Current saves use v4, never silently extend the old shape");
    }else{
        const auto disk=SaveFiles(content,root).read(1);GameApplication game(content,"title");game.configureSaveDirectory(root);finishTitleOpening(game);titleLoad(game,1);
        auto expected=disk.session;expected.flags[1]=expected.flags[18]=true;expected.variables[399]=6;
        need(game.hasGame()&&game.mapId()==22&&game.session().exportState()==expected,"Later content loads the complete early snapshot without reseeding facts");
        need(game.session().eventTimer()==disk.session.eventTimer&&game.session().eventTimer().running&&game.session().pursuerHealth()==317,"Stage upgrade keeps exact training residue and shared HP");
        need(game.session().magazine(19)==30&&game.session().magazine(16)==1&&game.session().magazine(17)==0&&game.session().magazine(18)==0&&game.session().equipped(1)==11,"Dormant slots remain their original early seeds on later Load");
        const auto once=game.session().exportState();std::string error;need(game.restore(disk,error)&&game.session().exportState()==once,"Stage upgrade repeated Load never reapplies seed/reward");
    }
    std::cout<<"Training upgrade "<<(writing?"write":"load")<<" with actual "<<content.fullStage<<" package passed\n";
}
std::string read(const std::filesystem::path& p){std::ifstream stream(p,std::ios::binary);need(bool(stream),"Missing save input");return {(std::istreambuf_iterator<char>(stream)),{}};}
}
int main(int argc,char** argv){try{
    need(argc==4,"usage: checks cumulative-package isolated-directory write|load");
    const auto content=loadCombatContent(argv[1]);const std::filesystem::path root=argv[2];const std::string mode=argv[3];
    if(mode=="training-write"||mode=="training-load"){trainingUpgrade(content,root,mode=="training-write");return 0;}
    const bool loading=mode=="load"||mode=="final-load";
    if(mode=="final-load"){
        need(content.fullStage=="evacuation"&&!content.maps.contains(120),"105 final restore package keeps original forbidden map boundary");
        for(int map:{116,117,118,119,121}){need(content.maps.contains(map)&&!content.pendingMapTickets.contains(map),"105 tail is actually delivered before final restoration");}
        for(const auto& point:content.catalog.at("save_points")){const int map=content.objects.at(point.at("object").get<std::string>()).map;need(map!=117&&map!=118&&map!=119&&map!=121,"Final integration never invents guard/escort/boundary saves");}
    }
    const std::array<std::pair<int,std::string>,4> points{{{22,"m22-e2"},{77,"m77-e6"},{79,"m79-e6"},{107,"m107-e3"}}};
    for(int slot=1;slot<=4;++slot){const auto& [map,point]=points.at(slot-1);
        if(mode=="write"){
            auto c=content;auto e=fixture(c,map,point);
            if(map==22){
                e.map=21;e.playerTile=adjacentTile(c.objects.at("m21-e9").tile,Direction::Down);e.direction=Direction::Up;
                e.session.eventTimer={EventTimerSource::Training,3000,true,false,false};
            }else if(map==77){e.session.flags[227]=e.session.flags[585]=true;e.session.conditions.poisoned=true;e.session.conditions.poisonWaitTicks=17;}
            else if(map==79){e.session.flags[256]=e.session.flags[282]=e.session.flags[67]=true;e.session.inventory[61]=0;}
            else{for(int flag:{319,320,321,333,354}){e.session.flags[flag]=true;}e.session.accessory=53;e.session.inventory[53]=0;}
            c.entries["106-save-fixture"]=e;GameApplication game(c,"106-save-fixture");game.configureSaveDirectory(root);finishTitleOpening(game);
            if(map==22){game.update(key());settle(game);need(game.mapId()==22&&game.session().eventTimer().running&&game.session().eventTimer().remainingTicks<3000,"Real M21 exit carries training clock to22 without resetting");walk(game,adjacentTile(c.objects.at(point).tile,Direction::Down));faceUp(game);}
            openSave(game);game.update(key(InputAction::Down));game.update(key());
            need(game.mode()==ApplicationMode::Playing&&game.session().saveCount()==7&&game.session().variable(399)==5,"No preserves 7/5");
            openSave(game);game.update(key());game.update(key(InputAction::Cancel));need(game.mode()==ApplicationMode::Playing&&game.session().saveCount()==7&&game.session().variable(399)==5,"Cancel slots preserves 7/5");
            openSave(game);const auto before=game.session().exportState();game.update(key());need(game.fileMenuView().has_value(),"Yes opens original slot UI");
            for(int n=1;n<slot;++n){game.update(key(InputAction::Down));}game.update(key());
            need(game.mode()==ApplicationMode::Playing&&game.session().saveCount()==8&&game.session().variable(399)==6,"Successful write returns8/6");
            const auto disk=SaveFiles(c,root).read(slot);need(disk.session.saveCount==8&&disk.session.variables.at(399)==5,"Disk is8/5 before CE56 continuation");
            need(disk.session.eventTimer==before.eventTimer&&disk.session.conditions==before.conditions,"Save menu freezes timer and finite DOT continuation");
            need(disk.scene.map==map&&disk.session.pursuerHealth==317,"Each real save point captures independent map and shared HP");
            if(map==22){need(disk.session.eventTimer.running&&disk.session.eventTimer.remainingTicks>60,"Training defeat residue remains saveable at22");}
            if(map==77){need(c.activeVariant(c.objects.at(point),game.session())->page==4,"Pursuer-ended P4 remains effective over blocked P3");}
            if(map==107){need(game.session().flag(321)&&game.session().flag(333)&&game.session().flag(354)&&game.session().equipped(4)==53,"Water, taken shoe and supplies remain committed");}
            std::cout<<"Written source save map "<<map<<" slot "<<slot<<'\n';
        }else if(loading){
            const auto disk=SaveFiles(content,root).read(slot);GameApplication game(content,"title");game.configureSaveDirectory(root);finishTitleOpening(game);
            game.update(key());need(game.mode()==ApplicationMode::Loading,"Independent process enters title Load");
            for(int n=1;n<slot;++n){game.update(key(InputAction::Down));}game.update(key());checkLoaded(game,disk);checkRewardRevisit(content,disk);
            if(map==107){auto restored=prepareRestore(content,disk);need(restored.session->flag(321)&&restored.session->flag(333)&&restored.session->flag(354)&&restored.session->equipped(4)==53,"Later cumulative package retains water/shoe/supply state");}
            if(map==77){need(disk.session.conditions.poisoned&&disk.session.conditions.poisonWaitTicks>0,"Entered finite poison wait survives independent process");}
            std::cout<<"Loaded independent slot "<<slot<<" and checked repeats/candidate failure/new-game isolation\n";
        }else{throw std::runtime_error("Unknown process mode");}
    }
    if(mode=="write"){
        checkSaveBlocked(content);
        auto c=content;auto e=fixture(c,77,"m77-e6");e.session.flags[227]=e.session.flags[585]=true;c.entries["106-failed-save"]=e;
        const auto failure=root/"failure";const auto older=SaveFiles(c,root).read(2);SaveFiles files(c,failure);
        need(files.write(1,older).outcome==WriteOutcome::Committed,"Failure check starts with a whole existing disk record");
        const auto original=read(failure/"slot-01.json");GameApplication game(c,"106-failed-save");
        game.configureSaveDirectory(failure,[](std::string_view stage){if(stage=="begin"){throw std::runtime_error("injected106diskfailure");}});
        openSave(game);game.update(key());game.update(key());game.update(key(InputAction::Up));game.update(key());
        need(game.mode()==ApplicationMode::Saving&&game.session().saveCount()==7&&game.session().variable(399)==5,"Failed overwrite remains uncommitted7/5");
        game.update(key(InputAction::Cancel));game.update(key(InputAction::Cancel));
        need(game.mode()==ApplicationMode::Playing&&game.session().saveCount()==7&&game.session().variable(399)==5&&game.session().flag(1)&&game.session().flag(18),"Error/cancel restores control with no successful-save increment");
        need(read(failure/"slot-01.json")==original,"Failed overwrite preserves original isolated bytes");
    }
    if(loading&&std::filesystem::exists(root/"legacy"/"slot-03.json")){
        const auto path=root/"legacy"/"slot-03.json";const auto bytes=read(path);const auto old=decodeSave(content,bytes);
        GameApplication legacy(content,"title");legacy.configureSaveDirectory(root/"legacy");finishTitleOpening(legacy);legacy.update(key());legacy.update(key(InputAction::Down));legacy.update(key(InputAction::Down));legacy.update(key());
        need(legacy.hasGame()&&legacy.mapId()==old.scene.map,"Real B/C copy loads through title");
        need(legacy.session().eventTimer()==EventTimer{}&&legacy.session().pursuerHealth()==500,"Old B/C initializes only absent new semantics");
        need(legacy.session().magazine(11)==old.session.magazines.at(11)&&legacy.session().exportState().inventory==old.session.inventory,"Old B/C retains original inventory/ammunition");
        const auto once=legacy.session().exportState();std::string error;need(legacy.restore(old,error)&&legacy.session().exportState()==once,"Repeated old Load neither migrates nor reseeds");
        need(read(path)==bytes,"Legacy copy Load remains read-only");legacy.returnToTitle();finishTitleOpening(legacy);need(legacy.newGame(error)&&legacy.session().saveCount()==0&&!legacy.session().flag(321),"Old saved progress never enters a new game");
    }
    std::cout<<"106 "<<mode<<" matrix passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
