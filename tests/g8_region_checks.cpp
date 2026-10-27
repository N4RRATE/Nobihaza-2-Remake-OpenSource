#include "title_navigation.h"
#include "g8_region_driver.h"
#include "full_content.h"
#include "content_hash.h"
#include "hangar_behavior.h"
#include "enemy_controller.h"
#include "zombie_behavior.h"
#include <cctype>

namespace {
CombatEntry fixture(const CombatContent& c,int map,const std::string& difficulty){
    auto e=c.entries.at("post-pickup");e.map=map;e.playerTile={8,9};e.direction=Direction::Up;
    e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.difficulty=difficulty;
    e.session.hp=e.session.maxHp=999;e.session.baseDefense=999;e.session.conditions={};
    e.session.flags[1]=e.session.flags[18]=true;e.session.flags[13]=e.session.flags[15]=false;
    e.session.variables[280]=difficulty=="easy"?1:difficulty=="hard"?3:2;
    return e;
}
void receivers(const CombatContent& c){
    const auto& patches=c.catalog.at("difficulty").at("enemy_rules");int checked=0;
    for(const auto& difficulty:{"easy","normal","hard"}){const int level=difficulty==std::string("easy")?0:difficulty==std::string("hard")?2:1;
        for(const auto& [id,row]:patches.items()){
            const bool hunter=c.enemies.contains(id)&&c.enemies.at(id).rules.value("behavior",std::string{})=="hunter";
            const bool licker=c.expansion.enemies.contains(id)&&c.expansion.enemies.at(id).map>=157;
            if(!hunter&&!licker){continue;}
            const int map=hunter?c.enemies.at(id).map:c.expansion.enemies.at(id).map;
            auto e=fixture(c,map,difficulty);e.positions[id]={8,6};e.directions[id]=Direction::Down;
            GameSession s(c.items,e.session,c.facts);
            for(int flag:{361,365,366,367,368}){s.setFlag(flag,true);}
            for(const auto& [other,d]:c.enemies){if(d.map==map){s.setFlag(d.deadFlag,other!=id);if(d.rules.contains("activation_fact")){s.setFlag(d.rules.at("activation_fact"),true);}}}
            for(const auto& [other,d]:c.expansion.enemies){if(d.map==map){s.setFlag(d.deadFlag,other!=id);if(d.activationFlag){s.setFlag(*d.activationFlag,true);}}}
            s.setVariable(89,0);s.setVariable(103,0);s.setVariable(91,0);s.setVariable(105,0);s.setFlag(47,false);
            CombatScene scene(c,s,e,safe);scene.update({});const auto p=findCharacter(scene.world(),id).motion.position();std::set<std::string> hit;
            const auto result=scene.resolveAttack({{(p.x+.5)*16,(p.y+1)*16-8},{hunter?9:0,hunter?9:0},hunter?AttackKind::Handgun:AttackKind::HandgunHp,2,1,false,99,AttackPhase::Impact},hit);
            const int defense=hunter?std::array{6,12,18}[level]:std::array{5,18,24}[level];
            need(result.processed==1&&s.variable(49)==defense,"Regional receiver selected wrong defense: "+id+" "+difficulty+" processed="+std::to_string(result.processed)+" V49="+std::to_string(s.variable(49)));
            ++checked;
        }
    }
    need(checked==57,"Regional enemy inventory is incomplete");std::cout<<"REGION_RECEIVERS 19*3 actual contacts passed\n";
}
void lickerActions(const CombatContent& c){
    for(const auto& difficulty:{"easy","normal","hard"}){const int level=difficulty==std::string("easy")?0:difficulty==std::string("hard")?2:1;
        for(int distance:{1,4}){
            auto e=fixture(c,160,difficulty);e.playerTile=distance==1?TilePosition{10,8}:TilePosition{14,9};e.positions["m160-e4"]={10,9};e.directions["m160-e4"]=distance==1?Direction::Up:Direction::Right;e.session.flags[5]=distance==4;
            e.session.baseDefense=0;
            GameSession s(c.items,e.session,c.facts);s.setFlag(384,true);s.setFlag(381,false);CombatScene scene(c,s,e,[](int lo,int hi){return lo==-5&&hi==2?0:lo==0&&hi==10?5:lo;});std::set<int> attacks;
            const auto received=[&](int attack){return std::max(1,attack-s.defense()/2);};
            const int claw=received(std::array{9,15,22}[level]),tongue=received(std::array{25,30,37}[level]),pounce=received(std::array{45,60,81}[level]);
            for(int n=0;n<2000;++n){scene.update({});scene.takePresentation();attacks.insert(s.variable(23));if(distance==1?attacks.contains(claw)&&attacks.contains(tongue):attacks.contains(pounce)){break;}}
            need(distance==1?attacks.contains(claw)&&attacks.contains(tongue):attacks.contains(pounce),"Mixed Actor licker action failed "+std::string(difficulty)+" distance="+std::to_string(distance)+" attacks="+Json(attacks).dump());
        }
    }std::cout<<"REGION_LICKER_ACTIONS real claw/tongue/pounce E/N/H passed\n";
}
void hunterActions(const CombatContent& c){
    for(const std::string difficulty:{"easy","normal","hard"}){
        auto e=fixture(c,143,difficulty);e.playerTile={10,10};e.positions["m143-e2"]={10,9};e.directions["m143-e2"]=Direction::Down;e.session.baseDefense=0;
        GameSession s(c.items,e.session,c.facts);for(int flag:{361,365,366,367,368}){s.setFlag(flag,true);}
        CombatScene scene(c,s,e,[](int lo,int hi){return lo==-5&&hi==2?0:lo;});const int before=s.hp();
        for(int n=0;n<1000&&s.hp()==before;++n){scene.update({});scene.takePresentation();}
        const int raw=difficulty=="easy"?12:difficulty=="hard"?30:20;
        need(before-s.hp()==std::max(1,raw-s.defense()/2),"Actual Hunter Actor29 strike selected the wrong difficulty: "+difficulty);
    }
    std::cout<<"REGION_HUNTER_ACTIONS actual Actor29 slash E/N/H passed\n";
}
void gases(const CombatContent& c){
    int storyOwners=0;for(const auto& [id,definition]:c.enemies){if(definition.story){++storyOwners;need(!definition.rules.contains("behavior")||definition.rules.value("behavior",std::string{})=="hangar","Unexamined explicit story behavior: "+id+" "+definition.rules.dump());}}
    need(storyOwners==2,"Story factory consumers changed");
    for(const std::string difficulty:{"easy","normal","hard"}){
        auto e=c.entries.at("post-pickup");e.session.difficulty=difficulty;e.session.hp=e.session.maxHp=1000;e.session.baseAttack=20;e.session.flags[18]=e.session.flags[52]=true;e.session.flags[101]=false;e.session.variables[35]=0;
        GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,[](int lo,int){return lo;});auto typed=makeEnemyBehavior(scene,c.enemies.at("m4-e4"));need(dynamic_cast<ZombieBehavior*>(typed.get())!=nullptr,"Default story soldier lost its source behavior");typed.reset();
        auto nullSource=c.enemies.at("m4-e4");nullSource.rules=nullptr;auto nullController=makeEnemyBehavior(scene,nullSource);nullController->update({});need(dynamic_cast<ZombieBehavior*>(nullController.get())!=nullptr,"Null story definition did not receive its original rules");nullController.reset();
        const auto p=findCharacter(scene.world(),"m4-e4").motion.position();std::set<std::string> resolved;const auto hit=scene.resolveAttack({{(p.x+.5)*16,(p.y+.5)*16},{0,0},AttackKind::Handgun,2,0,false,1,AttackPhase::Impact},resolved);
        need(hit.processed==1&&s.variable(49)==(difficulty=="easy"?3:difficulty=="hard"?10:8),"Default first soldier lost its selected difficulty rules");
    }
    for(const auto& difficulty:{"easy","normal","hard"}){for(int mask=0;mask<8;++mask){
        auto e=fixture(c,120,difficulty);e.playerTile={33,55};e.positions["m120-e13"]={27,7};
        GameSession s(c.items,e.session,c.facts);for(int flag:{346,347,348,349}){s.setFlag(flag,true);}s.setFlag(350,false);s.setFlag(21,true);s.setVariable(91,5);
        CombatScene scene(c,s,e,safe);auto typed=makeEnemyBehavior(scene,c.enemies.at("m120-e13"));need(dynamic_cast<HangarBehavior*>(typed.get())!=nullptr,"Story completion marker replaced explicit Hangar behavior");typed.reset();std::set<std::string> shows;
        for(int n=0;n<100;++n){scene.update({});scene.takePresentation();}
        need(s.flag(23)&&s.flag(25)&&s.flag(27),"Initial empty roar did not create three actual gas instances");
        for(int n=0;n<3;++n){if(!(mask&(1<<n))){s.setFlag(257+n,true);}}scene.update({});scene.takePresentation();
        for(int n=0;n<3;++n){need(s.flag(23+2*n)==bool(mask&(1<<n)),"Independent actual gas cancellation changed another slot");}
        s.setFlag(21,true);s.setVariable(91,5);
        for(int n=0;n<120;++n){scene.update({});for(const auto& v:scene.takePresentation()){if(v.kind==PresentationEvent::Kind::Cue){shows.insert(v.id);}}}
        for(int n=0;n<3;++n){const int eid=std::array{5,7,8}[n];const auto cue="m120-e"+std::to_string(eid)+"-p1-c"+(n==0?"4":"5");const bool expected=difficulty==std::string("hard")?!(mask&(1<<n)):mask==0;need(shows.contains(cue)==expected,"Active gas slot predicate differs "+std::string(difficulty)+" mask="+std::to_string(mask)+" shows="+Json(shows).dump()+" V89="+std::to_string(s.variable(89))+" V91="+std::to_string(s.variable(91)));}
    }}
    for(const std::string mode:{"emit","near","selector","page","death","retire"}){
        auto e=fixture(c,120,"hard");e.playerTile=mode=="near"?TilePosition{29,7}:TilePosition{33,40};e.positions["m120-e13"]={27,7};
        GameSession s(c.items,e.session,c.facts);for(int flag:{346,347,348,349}){s.setFlag(flag,true);}s.setFlag(350,false);s.setFlag(21,false);s.setVariable(91,0);
        CombatScene scene(c,s,e,safe);int shows=0;
        const auto tick=[&]{scene.update({});for(const auto& v:scene.takePresentation()){shows+=v.kind==PresentationEvent::Kind::Cue&&v.id=="m120-e5-p1-c4";}};
        for(int n=0;n<420;++n){tick();}need(shows==0,"Passive gas fired before Wait70");
        if(mode=="selector"){s.setVariable(91,4);}else if(mode=="page"){s.setFlag(18,false);}else if(mode=="death"){s.loseHp(s.hp());}else if(mode=="retire"){scene.deactivate();}
        tick();need(shows==(mode=="emit"||mode=="near"?1:0),"Passive post-wait/invalid-page cancellation failed "+mode);
        if(mode=="near"){need(s.conditions().neuralParalysis&&!s.conditions().poisoned&&s.hp()<999,"Passive near gas did not apply original neural-paralysis8 damage");}
        if(mode=="page"){s.setFlag(18,true);for(int n=0;n<420;++n){tick();}need(shows==0,"Invalid page reused old wait");tick();need(shows==1,"Reentered passive page did not start a fresh wait");}
    }std::cout<<"REGION_GASES 24 live slot combinations; Wait70 selector/page/death/retire passed\n";
}
void preset(const CombatContent& c){
    auto e=fixture(c,162,"easy");GameSession s(c.items,e.session,c.facts);s.setFlag(382,true);s.setFlag(383,false);s.setVariable(89,0);s.setVariable(35,7);
    CombatScene scene(c,s,e,safe);for(int n=0;n<10;++n){scene.update({});}
    const auto a=findCharacter(scene.world(),"m162-e2");need(!a.active&&a.motion.tile()==TilePosition{0,34}&&s.variable(89)==0&&s.variable(35)==7&&!s.flag(383),"EASY removed slot fabricated damage/kill or removed its neighbor");
    need(c.activeVariant(c.objects.at("m162-e2"),s)->page==23&&c.activeVariant(c.objects.at("m162-e3"),s)->page==11,"EASY preset selected the damage-gated terminal page");
    std::cout<<"REGION_PRESET first absent at0,34; V89/kill unchanged; second remains\n";
}
template<class F>void reject(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}need(failed,"Invalid regional save accepted");}
void loaderRejections(const CombatContent& c,GameApplication& app,const SaveSnapshot& valid,const std::filesystem::path& root){
    const auto original=Json::parse(read((c.directory/"g8.json").string().c_str()));
    for(const std::string kind:{"identity","extra-field","projection","source-declaration","missing-source","symlink-source","damaged-source"}){
        const auto directory=root/"loader-rejections"/kind;std::filesystem::create_directories(directory);auto bad=original;
        const auto source=directory/"g8-first-part-source.json";if(std::filesystem::exists(source)||std::filesystem::is_symlink(source)){std::filesystem::remove(source);}
        std::string expected="Missing region predecessor";
        if(kind=="identity"){bad["content_compatibility"]="unreviewed-region";expected="Unknown region difficulty identity";}
        if(kind=="extra-field"){bad["extra"]=true;expected="Unknown region difficulty identity";}
        if(kind=="projection"){bad["projection"]["level_sample"]["actor"]=20;expected="Unreviewed region difficulty projection";}
        if(kind=="source-declaration"){bad["source"]["sha256"]="changed";expected="Unknown region difficulty predecessor";}
        if(kind=="symlink-source"){std::filesystem::create_symlink(std::filesystem::absolute(c.directory/"g8-first-part-source.json"),source);}
        if(kind=="damaged-source"){std::ofstream(source)<<"damaged predecessor";expected="Changed region predecessor";}
        const auto file=directory/"g8.json";{std::ofstream output(file);output<<bad.dump();need(output.good(),"Could not write bad region candidate");}
        const auto state=app.session().exportState();const auto generation=app.generation();const auto tile=app.world().player().motion.tile();
        app.configureRestorePresentation([&](const SceneDefinition&,const SavedPresentation&){auto failed=loadCombatContent(file);(void)failed;});std::string error;
        const bool restored=app.restore(valid,error);app.configureRestorePresentation({});
        need(!restored&&error.find(expected)!=std::string::npos,"Bad region candidate missed its actual loader boundary: "+kind+" "+error);
        need(app.generation()==generation&&app.session().exportState()==state&&app.world().player().motion.tile()==tile,"Bad region candidate replaced the live game: "+kind);
        std::cout<<"REGION_RESOURCE_REJECT "<<kind<<" live game retained: "<<error<<'\n';
    }
}
void independentLoads(const CombatContent& c,int argc,char** argv){
    GameApplication app(c,"post-pickup",safe);
    for(int n=3;n<argc;++n){
        const std::filesystem::path path=argv[n];const auto original=read(argv[n]);const auto saved=decodeSave(c,original);const auto expected=prepareRestore(c,saved);const int slot=std::stoi(path.filename().string().substr(5,2));
        if(saved.session.difficulty!="normal"&&saved.scene.map>=120){
            auto falselyClaimed=Json::parse(encodeSave(c,saved));falselyClaimed["content"]=c.legacySource->compatibility;
            reject([&]{decodeSave(c,falselyClaimed.dump());});
        }
        app.returnToTitle();finishTitleOpening(app);app.configureSaveDirectory(path.parent_path());finishTitleOpening(app);for(int k=0;k<3&&app.menuSelection()!=1;++k){app.update(key(InputAction::Down));}app.update(key());need(app.mode()==ApplicationMode::Loading,"Independent region title Load absent");
        for(int k=1;k<slot;++k){app.update(key(InputAction::Down));}const auto menu=app.fileMenuView();need(menu&&menu->summaries.at(slot-1)&&menu->summaries.at(slot-1)->label==saved.summary.label,"Actual file menu changed the saved difficulty label");
        app.update(key());need(app.hasGame()&&!app.openingActive()&&app.mapId()==saved.scene.map&&app.world().player().motion.tile()==expected.scene->world().player().motion.tile()&&app.session().exportState()==expected.session->exportState(),"Independent region Load changed state: "+path.string());
        need(read(argv[n])==original,"Independent Load rewrote the actual source disk");std::cout<<"REGION_LOAD "<<path<<" difficulty="<<app.session().difficulty()<<" map="<<app.mapId()<<" count="<<app.session().saveCount()<<" PASS\n";
    }
    loaderRejections(c,app,decodeSave(c,read(argv[argc-1])),std::filesystem::path(argv[argc-1]).parent_path());
}
void saveContract(const CombatContent& c,const std::filesystem::path& root){
    for(const std::string difficulty:{"easy","normal","hard"}){
        const auto original=read((root/difficulty/"map-107/slot-01.json").string().c_str());const auto source=decodeSave(c,original);
        need(source.contentCompatibility==c.compatibility&&source.session.difficulty==difficulty&&source.scene.map==107,"Valid G8-02 source did not adapt unchanged");
        const auto old=decodeSave(*c.legacySource,original);need(old.session==source.session&&old.scene==source.scene,"G8-03 adaptation changed actual state");
        if(difficulty!="normal"){auto spoof=Json::parse(original);spoof["after_save"]["kind"]="chapter_handoff_after_file";reject([&]{decodeSave(c,spoof.dump());});}
        auto e=fixture(c,126,difficulty);e.playerTile={1,0};e.session=source.session;
        GameSession s(c.items,e.session,c.facts);for(int flag:{346,347,348,349,350,357}){s.setFlag(flag,true);}s.setFlag(360,false);s.setFlag(1,false);s.setFlag(18,false);s.setVariable(13,1);
        CombatScene scene(c,s,e,safe);for(int n=0;n<3000&&!scene.inputPromptView().cursor;++n){scene.update({});scene.takePresentation();}
        need(scene.inputPromptView().cursor.has_value(),"Controlled Chapter question absent");scene.update(key());
        for(int n=0;n<10;++n){scene.update({});}const auto request=scene.takeSaveRequest();need(request.has_value(),"Controlled Chapter request absent");
        const auto snapshot=captureSave(c,scene,*request,{});const auto roundtrip=decodeSave(c,encodeSave(c,snapshot));
        need(roundtrip.session==snapshot.session&&roundtrip.scene==snapshot.scene&&roundtrip.continuation==snapshot.continuation,"Chapter encoding changed controlled state");
        auto upper=difficulty;std::transform(upper.begin(),upper.end(),upper.begin(),[](unsigned char x){return std::toupper(x);});
        need(snapshot.session.actorName==c.catalog.at("chapter").at("name_by_difficulty").at(upper).get<std::string>()&&snapshot.summary.label==snapshot.session.actorName,"Chapter chose a different difficulty name");
        auto bad=snapshot;bad.session.actorName="　大雄/NORMAL/？？？";if(difficulty!="normal"){reject([&]{validateSave(c,bad);});}
        for(const auto& kind:{"direct_unscored","clear_after_file"}){bad=snapshot;bad.continuation.kind=kind;reject([&]{validateSave(c,bad);});}
        GameApplication app(c,"post-pickup",safe);std::string error;need(app.restore(snapshot,error),"Valid controlled Chapter restore failed: "+error);
        const auto before=app.session().exportState();const auto generation=app.generation();app.configureRestorePresentation([](const SceneDefinition&,const SavedPresentation&){throw std::runtime_error("controlled missing region media");});
        need(!app.restore(snapshot,error)&&app.generation()==generation&&app.session().exportState()==before,"Failed Chapter candidate replaced live game");
        scene.deactivate();reject([&]{captureSave(c,scene,*request,{});});
    }
    std::cout<<"REGION_SAVE old02-source-first adaptation/rejection;3 Chapter actor names/roundtrip/stale/failed-candidate passed\n";
}
void hall(const CombatContent& c,const char* source,const std::filesystem::path& root){
    std::filesystem::create_directories(root);std::ofstream(root/"slot-01.json")<<read(source);
    G2NaturalDriver r(c,root);r.game.configureSaveDirectory(root);finishTitleOpening(r.game);r.load();need(r.game.mapId()==127&&r.game.session().flag(360),"Actual Chapter did not wake at127");
    r.actObject("m127-e3");r.actObject("m128-e9");r.actObject("m128-e7");r.actObject("m129-e4");need(r.game.mapId()==132,"Actual mansion entrance missing");r.saveAt(2);
    std::cout<<"REGION_HALL actual Chapter-to-M132 save difficulty="<<r.game.session().difficulty()<<" ticks="<<r.ticks<<'\n';
}
void natural(const CombatContent& c,const char* source,const std::filesystem::path& root,int startAt=0){
    std::filesystem::create_directories(root);std::ofstream(root/"slot-01.json")<<read(source);
    RegionDriver r(c,root,"public-inputs-stage"+std::to_string(startAt)+".jsonl");r.game.configureSaveDirectory(root);finishTitleOpening(r.game);r.load();
    const auto start=r.game.session().exportState();const int allowance=2;
    if(startAt<=3){
    if(startAt==0){need(r.game.mapId()==127&&r.game.session().flag(360),"Natural region source is not an actual completed Chapter");
    for(const auto* id:{"m127-e7","m127-e7","m127-e8","m127-e8","m127-e3","m128-e9","m128-e7","m129-e4"}){r.actObject(id);}
    auto hallSkills=start.skills;if(hallSkills.erase(4)){hallSkills.insert(3);}need(r.game.session().exportState().skills==hallSkills,"Actual hall entry lost its original mixing-skill handoff");
    r.saveAt(2);r.equip(21);
    r.readyMagazine(21);
    r.upstairs();regionStone(r);
    r.actAt({25,17},Direction::Up);r.choices(0);need(r.game.session().flag(364),"Original wax key unlock failed");
    r.actAt({25,17},Direction::Up);r.actAt({17,6},Direction::Up);r.actAt({10,7},Direction::Up);r.walk({9,10});r.settle();
    need(r.game.mapId()==142&&r.game.session().flag(366),"Original shadow chase failed");
    r.actAt({9,10},Direction::Down);r.walk({9,6},[&]{return r.game.session().flag(367);});r.settle();r.equip(21);if(r.game.session().hp()<80){r.healTo(100);}battle(r);
    r.actAt({4,9},Direction::Down);if(r.game.session().hp()<80){r.healTo(100);}battle(r);r.actAt({3,14},Direction::Down);if(r.game.session().hp()<80){r.healTo(100);}battle(r);r.actAt({27,13},Direction::Right);
    r.unlock({22,10},Direction::Right,1);r.actAt({22,10},Direction::Right);if(r.game.session().hp()<80){r.healTo(100);}battle(r);
    r.actAt({22,5},Direction::Up);if(r.game.session().hp()<80){r.healTo(100);}battle(r);r.actAt({20,8},Direction::Down);if(r.game.session().hp()<80){r.healTo(100);}battle(r);
    r.actAt({23,26},Direction::Down);if(r.game.session().hp()<80){r.healTo(100);}battle(r);r.unlock({3,14},Direction::Left);r.actAt({3,14},Direction::Left);r.saveAt(3);
    }else{need(startAt>=1&&startAt<=3&&r.game.mapId()==132&&r.game.session().flag(419),"Natural continuation is not the actual cleared-corridor hall disk");}
    if(startAt<=1){r.hallBath();r.drain(0);r.drain(0);need(r.game.session().count(102)==1&&r.game.session().flag(495),"Original drained key was not picked up");r.bathHall();r.hallKitchen();r.actAt({5,7},Direction::Up);r.actAt({5,7},Direction::Up);
    need(r.game.session().count(13)==1,"Natural kitchen I13 pickup absent");r.actAt({12,8},Direction::Left);r.equip(13);r.press(InputAction::Weapon);for(int n=0;n<100;++n){r.tick();}r.press(InputAction::Attack);for(int n=0;n<100;++n){r.tick();}
    const int old11=r.game.session().magazine(11);for(int n=0;n<12;++n){InputFrame in;in.buttons[int(InputAction::Cancel)].held=true;r.tick(in);}for(int n=0;n<100;++n){r.tick();}
    need(r.game.session().magazine(13)==13&&r.game.session().magazine(11)==old11,"Natural I13 reload refilled another magazine or failed");r.press(InputAction::Attack);for(int n=0;n<100;++n){r.tick();}r.lower();need(r.game.session().magazine(13)==12,"Natural I13 consumed round lost");r.kitchenHall();r.saveAt(11);
    r.hallStudy();r.books(true);r.equip(21);if(r.game.session().hp()<80){r.healTo(100);}battle(r);r.actObject("m135-e9");r.studyHall();
    r.actAt({4,11},Direction::Left);r.walk({11,5});r.settle();r.clock(2,43);r.actObject("m129-e17");
    r.actAt({23,8},Direction::Right);r.saveAt(4);
    }else if(startAt==2){need(r.game.session().flag(435)&&r.game.session().count(97)==1,"Natural continuation is not the actual clock reward hall disk");}
    if(startAt<=2){r.equip(11);r.hallMirror();r.fire();r.walk({6,4});r.walk({5,5});r.walk({5,4});r.settle();r.item(98);r.settle();
    r.walk({10,4});r.move(Direction::Up);r.actObject("m146-e3");r.actObject("m146-e2");r.mirrorHall();r.saveAt(5);
    }else{need(r.game.session().flag(436)&&r.game.session().count(99)==1&&!r.game.session().flag(426),"Natural continuation is not the actual device hall disk");}
    r.hallPiano();r.item(100);r.choices(0);r.equip(21);if(r.game.session().hp()<80){r.healTo(100);}battle(r);for(int n=0;n<1500&&!r.game.session().flag(438);++n){r.tick(n%8==0?key():InputFrame{});}
    need(r.game.session().flag(378)&&r.game.session().flag(379)&&r.game.session().flag(438),"Real piano pair/monitor did not complete");
    r.actObject("m137-e15");need(r.game.mapId()==152,"Original monitor exit did not reach152");r.actObject("m152-e4");need(r.game.mapId()==153,"Original collection stairs missing");r.actObject("m153-e4");
    need(r.game.mapId()==151&&r.game.session().variable(389)==3&&r.game.session().knowsSkill(3)&&!r.game.session().knowsSkill(4),"Original collection/treatment allowance or mixing skill absent");r.saveMonitor(6);
    r.actObject("m151-e22");r.walk({15,11});r.settle();r.press();r.press();need(r.game.session().count(22)==8&&r.game.session().magazine(22)==4,"Original repeated rocket grant altered the shared magazine");
    r.actObject("m151-e8");r.actObject("m155-e9");r.actObject("m155-e10");const int green=r.game.session().count(2);r.actObject("m155-e9");r.actObject("m155-e10");need(r.game.session().count(2)==green,"Repeated original herbs granted again");r.actObject("m155-e2");need(r.mixMedicine(),"Original learned mixing skill could not consume the actual green/red pickups");
    const int sprayStat=r.game.session().variable(382);r.investigateChoice("m151-e10",0);need(r.game.session().variable(382)==sprayStat,"Allowance treatment changed consumed-spray statistics");need(r.game.session().variable(389)==allowance,"Original post-443 treatment did not consume one allowance");
    r.saveMonitor(7);
    r.actObject("m151-e8");r.actObject("m155-e6");r.actObject("m157-e3");r.actObject("m158-e20");r.actObject("m158-e22");need(r.game.session().count(190)==1,"Original mail clue absent");r.actObject("m158-e2");r.actObject("m157-e2");r.actObject("m155-e2");r.saveMonitor(12);
    }else{need(startAt==4&&r.game.mapId()==151&&r.game.session().flag(443)&&r.game.session().count(190)==1&&r.game.session().variable(389)==allowance,"Natural continuation is not the actual monitor mail/quota disk");}
    // I13 remains its real shared magazine. Load the DE in the existing safe room, before the original double encounter.
    r.readyMagazine(21);r.enterLift();r.fight();r.actObject("m160-e11");r.equip(22);r.enterHunters();r.barrel(22);if(!r.alive().empty()){r.actObject("m162-e6");if(r.game.session().hp()<80&&r.game.session().count(5)>0){r.item(5);}r.enterHunters();r.fight();}
    need(r.game.session().flag(382)&&r.game.session().flag(383),"Real area2 hunters not complete");r.back();r.saveMonitor(8);
    r.enterLift();r.enterHunters();r.actObject("m162-e13");
    const auto t=c.objects.at("m163-e8").tile;r.walk(adjacentTile(t,Direction::Down));r.move(Direction::Up);r.tick(key());
    for(int n=0;n<3000&&r.game.foreground()!=ForegroundKind::Drainage;++n){r.tick(n%8==0?key():InputFrame{});}need(r.game.foreground()==ForegroundKind::Drainage,"Power panel absent");r.tick();r.tick();
    for(auto a:{InputAction::Confirm,InputAction::Down,InputAction::Confirm,InputAction::Right,InputAction::Right,InputAction::Confirm,InputAction::Down,InputAction::Right,InputAction::Confirm,InputAction::Left,InputAction::Confirm}){r.press(a);for(int n=0;n<20;++n){r.tick();}}r.choices(0);
    r.actObject("m163-e3");r.actObject("m162-e6");r.actObject("m160-e5");const auto terminal=c.objects.at("m161-e3").tile;r.walk(adjacentTile(terminal,Direction::Down));r.move(Direction::Up);r.tick(key());r.choices(0);r.equip(21);r.healTo(100);
    r.actObject("m161-e4");r.actObject("m160-e7");r.walk({5,6},[&]{return r.game.session().flag(503);});for(int n=0;n<300&&!r.game.session().flag(502);++n){r.tick();}r.ambushFight();r.actObject("m157-e2");r.actObject("m155-e2");r.saveMonitor(9);
    r.reception();r.startReception();r.open();r.credentials();r.end();r.actObject("m159-e5");need(r.game.mapId()==164,"Real registration did not unlock M164");r.actObject("m164-e3");r.receptionBack();r.saveMonitor(10);
    const auto& s=r.game.session();need(s.variable(389)==allowance&&s.count(101)==1&&s.difficulty()==start.difficulty,"Natural final state lost quota/card/difficulty");
    std::ofstream(root/"natural-result.json")<<Json{{"difficulty",s.difficulty()},{"ticks",r.ticks},{"maps",r.maps},{"hp",s.hp()},{"save_count",s.saveCount()},{"magazine22",s.magazine(22)},{"quota389",s.variable(389)}}.dump(2)<<'\n';
    std::cout<<"REGION_NATURAL "<<s.difficulty()<<" stage="<<startAt<<" actual-disk->164->151 ticks="<<r.ticks<<" count="<<s.saveCount()<<" hp="<<s.hp()<<'\n';
}
}
int main(int argc,char** argv){try{
    std::cout.setf(std::ios::unitbuf);need(argc>=3,"g8_region_checks package --rules | --hall chapter root");auto c=loadCombatContent(argv[1]);const std::string mode=argv[2];
    if(mode=="--rules"||mode=="--rules-save"){receivers(c);lickerActions(c);if(mode=="--rules-save"){need(argc==4,"--rules-save G8-02/independent-loads");saveContract(c,argv[3]);}gases(c);preset(c);}
    else if(mode=="--hall"){need(argc==5,"--hall chapter root");hall(c,argv[3],argv[4]);}
    else if(mode=="--natural"){need(argc==5||argc==6,"--natural actual-disk root [stage]");natural(c,argv[3],argv[4],argc==6?std::stoi(argv[5]):0);}
    else if(mode=="--save"){need(argc==4,"--save G8-02/independent-loads");saveContract(c,argv[3]);}
    else if(mode=="--loads"){need(argc>=4,"--loads actual save paths");hunterActions(c);independentLoads(c,argc,argv);}
    else if(mode=="--gases"){gases(c);preset(c);}
    else{throw std::runtime_error("Unknown regional check mode");}
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
