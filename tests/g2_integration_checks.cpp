#include "g2_natural_driver.h"
#include "content_hash.h"

namespace {
void summary(G2NaturalDriver& run, const std::filesystem::path& root, const std::string& mode) {
    const auto& s = run.game.session();
    Json out{{"mode", mode}, {"ticks", run.ticks}, {"maps", run.maps}, {"hp", s.hp()},
             {"I13", s.count(13)}, {"I22", s.count(22)}, {"magazine22", s.magazine(22)},
             {"magazine13",s.magazine(13)},{"equipment",Json::array({s.equipped(1),s.equipped(2),s.equipped(3),s.equipped(4),s.equipped(5)})},
             {"magazine21", s.magazine(21)}, {"I101", s.count(101)}, {"save_count", s.saveCount()},
             {"V35", s.variable(35)}, {"conditions", Json::array({s.exportState().conditions.bleedingTicks,
              s.exportState().conditions.poisonWaitTicks})}};
    std::ofstream(root / (mode + "-result.json")) << out.dump(2) << '\n';
    std::cout << "G2_INTEGRATION " << out.dump() << '\n';
}
void save(G2NaturalDriver& run, const std::filesystem::path& root, int slot) {
    const int before = run.game.session().saveCount();
    run.saveMonitor(slot, true);
    need(!std::filesystem::exists(root / ("slot-0" + std::to_string(slot) + ".json")), "Cancelled save wrote slot");
    run.saveMonitor(slot);
    SaveFiles files(run.game.content(), root);
    const auto saved = files.read(slot);
    need(saved.scene.map == 151 && saved.continuation.kind == "ordinary" &&
         saved.session.saveCount == before + 1, "Ordinary save did not increment once");
}
void powerRoom(G2NaturalDriver& r) {
    r.enterLift(); r.enterHunters(); r.actObject("m162-e13");
    need(r.game.mapId() == 163, "Power room absent");
}
void powerBack(G2NaturalDriver& r) {r.actObject("m163-e3"); r.back();}
void panel(G2NaturalDriver& r) {
    const auto t = r.game.content().objects.at("m163-e8").tile;
    r.walk(adjacentTile(t, Direction::Down)); r.move(Direction::Up); r.tick(key());
    for (int n=0; n<3000 && r.game.foreground()!=ForegroundKind::Drainage && !r.game.inputPromptView().cursor; ++n)
        r.tick(n%8==0 ? key() : InputFrame{});
    r.tick(); r.tick();
}
void solution(G2NaturalDriver& r) {
    for (auto a : {InputAction::Confirm,InputAction::Down,InputAction::Confirm,InputAction::Right,
                  InputAction::Right,InputAction::Confirm,InputAction::Down,InputAction::Right,
                  InputAction::Confirm,InputAction::Left,InputAction::Confirm}) {
        r.press(a); for (int n=0;n<20;++n) r.tick();
    }
}
void registration(G2NaturalDriver& r, bool powered, bool already = false) {
    r.reception(); r.startReception(); r.choose(0); r.prompt();
    r.open(); r.type(0); r.prompt();
    r.open(); r.type(200); r.type(209); r.type(209); r.prompt();
    need(r.game.session().count(101)==int(already), "Wrong credentials awarded a card");
    r.open(); r.credentials();
    need(r.game.session().count(101)==int(powered), "Correct credentials used wrong server fact");
    r.open(); r.credentials();
    need(r.game.session().count(101)==int(powered), "Duplicate registration increased card count");
    r.end();
    if (powered) {
        r.actObject("m159-e5");
        need(r.game.mapId()==164 && r.game.world().player().motion.tile()==TilePosition{2,9}, "Wrong G3 west landing");
        r.walk({10,9}); r.settle(); r.actObject("m164-e3");
        need(r.game.mapId()==159 && r.game.session().count(101)==1, "West roundtrip consumed card");
    }
    r.receptionBack();
}
void natural(G2NaturalDriver& r, const std::filesystem::path& root,bool resume=false) {
    need(r.game.mapId()==151 && (resume||!r.game.session().flag(493)) && !r.game.session().flag(444), "Not a real G1 start");
    if(!resume){r.actObject("m151-e8"); r.actObject("m155-e6"); r.actObject("m157-e3");
    r.actObject("m158-e20"); r.actObject("m158-e22");
    need(r.game.session().count(190)==1 && !r.game.session().flag(445), "No-power mail reward absent");
    r.actObject("m158-e2"); r.actObject("m157-e2"); r.actObject("m155-e2"); save(r,root,2);}
    registration(r,false);
    if(r.game.session().variable(635)==1){r.healTo(100);}
    r.equip(21);if(r.game.session().variable(635)==1&&r.game.session().magazine(21)==0&&!r.game.session().hasReserve()){need(r.game.session().count(15)>0,"Cycle has no actual lift shotgun");r.equip(15);need(r.game.session().magazine(15)>0||r.game.session().hasReserve(),"Cycle actual shotgun ammunition absent");} r.enterLift(); r.fight();
    need(r.game.session().flag(381) && r.game.session().flag(384), "Real lift enemies skipped");
    r.actObject("m160-e11"); const int combatWeapon=r.game.session().count(22)>0?22:21; r.equip(combatWeapon); r.enterHunters(); r.barrel(combatWeapon);
    if (!r.alive().empty()) {
        if(combatWeapon==21) {
            if (r.game.session().hp()<80 && r.game.session().count(5)>0) r.item(5);
            r.fight();
        } else {
            r.actObject("m162-e6");need(r.game.mapId()==160,"Hunter retreat was interrupted");
            if (r.game.session().hp()<80 && r.game.session().count(5)>0) r.item(5);
            r.enterHunters(); r.fight();
        }
    }
    need(r.game.session().flag(382) && r.game.session().flag(383) && r.game.session().flag(446), "Hunter/barrel not completed");
    r.back(); save(r,root,3);
    powerRoom(r); panel(r); r.press(InputAction::Cancel); r.choices(0);
    need(!r.game.session().flag(444) && !r.game.session().flag(445), "Cancel committed panel facts");
    panel(r); solution(r); r.choices(1);
    need(r.game.session().flag(444) && !r.game.session().flag(445), "No failed to preserve unlocked-only state");
    powerBack(r); save(r,root,4);
    powerRoom(r); panel(r); r.choices(0);
    need(r.game.session().flag(445) && !r.game.session().flag(503), "Power-first ordering diverged");
    r.actObject("m163-e3"); r.actObject("m162-e6"); r.actObject("m160-e5");
    const auto terminal=r.game.content().objects.at("m161-e3").tile;
    for (int answer : {1,0}) {r.walk(adjacentTile(terminal,Direction::Down));r.move(Direction::Up);r.tick(key());r.choices(answer);}
    need(r.game.session().flag(447) && !r.game.session().flag(503) && !r.game.session().flag(502), "First server restart crossed contact early");
    r.actObject("m161-e4"); r.actObject("m160-e7");
    r.walk({5,6},[&]{return r.game.session().flag(503);});
    for(int n=0;n<300&&!r.game.session().flag(502);++n) r.tick();
    need(r.game.session().flag(502), "First ambush drop absent");
    r.ambushFight(); r.actObject("m157-e2");r.actObject("m155-e2"); save(r,root,5);
    registration(r,true); save(r,root,6);
    need(r.game.session().count(101)==1 && !r.game.session().flag(449) && r.game.session().count(20)==0, "G3 completion fabricated");
    summary(r,root,"natural");
}
void verifyRestored(G2NaturalDriver& r,const SaveSnapshot& saved) {
        auto prepared=prepareRestore(r.game.content(),saved);
        const auto expected=prepared.session->exportState(),actual=r.game.session().exportState();
        need(actual.worldFlags==expected.worldFlags && actual.worldValues==expected.worldValues &&
             actual.inventory==expected.inventory && actual.magazines==expected.magazines &&
             actual.itemUses==expected.itemUses && actual.hp==expected.hp && actual.tp==expected.tp &&
             actual.conditions==expected.conditions && actual.mainWeapon==expected.mainWeapon &&
             actual.auxiliaryWeapon==expected.auxiliaryWeapon && actual.saveCount==expected.saveCount &&
             actual.skills==expected.skills && actual.party==expected.party,
             "Independent Load changed committed facts/resources/conditions/equipment/statistics");
        need(r.game.session().variable(399)==saved.continuation.statisticBefore+1,
             "Ordinary Load did not complete V399 exactly once");
}
void load(G2NaturalDriver& r, const std::filesystem::path& root) {
    SaveFiles files(r.game.content(),root);
    for(int slot=2;slot<=6;++slot) {
        r.load(slot); const auto saved=files.read(slot);
        verifyRestored(r,saved);
        need(r.game.mapId()==151, "Independent Load lost legal save scene");
    }
    registration(r,true,true);
    need(r.game.session().flag(381)&&r.game.session().flag(384)&&r.game.session().flag(382)&&
         r.game.session().flag(383)&&r.game.session().flag(402)&&r.game.session().flag(446), "Load lost enemy/barrel facts");
    summary(r,root,"load");
}
void boundary(G2NaturalDriver& r, const std::filesystem::path& root) {
    r.load(6); r.reception(); r.actObject("m159-e5");
    r.walk({18,9}); InputFrame right;right.movement=Direction::Right;r.tick(right);
    for(int n=0;n<120;++n) r.tick(right);
    need(r.game.world().player().motion.tile()==TilePosition{19,9} &&
         r.game.foreground()==ForegroundKind::Content && !r.game.session().flag(449) &&
         r.game.session().count(20)==0, "G3 handoff fabricated east encounter");
    summary(r,root,"boundary");
}
void grab(G2NaturalDriver& r, const std::filesystem::path& root, bool success, bool ambush) {
    r.load(1);
    const auto start=r.game.session().exportState();
    if(ambush) {
        powerRoom(r);panel(r);r.choices(0);r.actObject("m163-e3");r.actObject("m162-e6");
        r.actObject("m160-e7");r.walk({5,6},[&]{return r.game.session().flag(503);});
    } else r.enterLift();
    bool caught=false,escaped=false;int hp=0,pressed=0;std::set<std::string> cues;
    r.observe=[&](const PresentationEvent& e){if(e.kind==PresentationEvent::Kind::Cue)cues.insert(e.id);};
    for(int n=0;n<16000;++n) {
        if(r.game.mode()==ApplicationMode::Title) break;
        const auto& s=r.game.session();hp=s.hp();need(hp>0,"Grab trial died by HP before positive-HP failure");
        caught|=s.flag(10)&&!s.flag(1);
        if(caught&&!s.flag(10)&&s.flag(1)) {escaped=true;break;}
        InputFrame in;
        if(caught&&success&&n%2==0) {in=key(InputAction::Up);in.buttons[int(InputAction::Left)].pressed=true;++pressed;}
        else if(!caught) {
            const auto id=ambush?"m157-e4":s.variable(91)>=7?"m160-e4":"m160-e6";
            const int state=ambush?s.variable(91):s.variable(id==std::string("m160-e4")?91:105);
            if(state>=7&&state<=9) {
                const auto tile=findCharacter(r.game.world(),id).motion.tile();
                in.movement=next(r.game.world(),state==7?tile:adjacentTile(tile,Direction::Down));
            }
        }
        if(n%500==0) std::cout<<"GRAB n="<<n<<" hp="<<hp<<" caught="<<caught<<" state="<<s.variable(91)<<"/"<<s.variable(105)<<" p="<<r.game.world().player().motion.tile().x<<","<<r.game.world().player().motion.tile().y<<'\n';
        r.tick(in);
    }
    r.observe={};
    need(caught&&escaped==success&&hp>0,"Natural grab result absent");
    const std::string mode=ambush?"grab-ambush-success":success?"grab-success":"grab-failure";
    if(success) {
        need(r.game.session().flag(13),"Successful natural grab lost recovery protection");
        if(ambush){r.actObject("m157-e2");r.actObject("m155-e2");}
        else{r.actObject("m160-e7");r.actObject("m157-e2");r.actObject("m155-e2");}
        save(r,root,2);summary(r,root,mode);
    } else {
        need(r.game.mode()==ApplicationMode::Title,"Positive-HP grab failed to Title");
        need(read((root/"slot-01.json").c_str())==read((root/"source.json").c_str())&&!std::filesystem::exists(root/"slot-02.json"),"Failure wrote a save");
        r.load(1);
        need(r.game.session().saveCount()==start.saveCount&&r.game.session().variable(630)==(start.variables.contains(630)?start.variables.at(630):0),"Failure/TitleLoad invented statistics");
        summary(r,root,mode);
    }
    std::ofstream(root/(mode+"-grab.json"))<<Json{{"caught",caught},{"escaped",escaped},{"positive_hp_before_failure_or_escape",hp},{"direction_press_samples",pressed},{"source_cues",cues}}.dump(2)<<'\n';
}
}
int main(int argc,char** argv) {
    try {
        std::cout.setf(std::ios::unitbuf);
        need(argc==5,"G2package realG1source isolated-root natural|load|boundary");
        auto c=loadCombatContent(argv[1]);
        const bool cycle=std::string(argv[4])=="cycle"||std::string(argv[4])=="cycle-resume";need(c.compatibility==(cycle?"post-f5-g7-normal-7":"post-f5-g2-normal-6"),"Requested complete G2/cycle package missing");
        if(cycle){const auto source=decodeSave(c,read(argv[2]));need(source.session.worldValues.at(c.facts.variables.at(635))==1,"Cycle G2 requires the actual completed first-cycle source");}
        const std::filesystem::path root=argv[3];std::filesystem::create_directories(root);
        const std::string mode=cycle?"natural":argv[4];
        const bool grabMode=mode=="grab-success"||mode=="grab-failure"||mode=="grab-ambush-success";
        if(mode=="natural"||mode=="i13-response"||grabMode) {std::ofstream(root/"slot-01.json")<<read(argv[2]);if(grabMode)std::ofstream(root/"source.json")<<read(argv[2]);}
        std::ofstream draws(root/("random-"+mode+".jsonl"));
        CombatScene::Random random=grabMode?CombatScene::Random{[&](int low,int high){draws<<Json{{"low",low},{"high",high},{"value",low}}.dump()<<'\n';return low;}}:CombatScene::Random{safe};
        G2NaturalDriver r(c,root,"public-inputs-"+mode+".jsonl",random);r.game.configureSaveDirectory(root);
        if(mode=="natural") {r.load(1);natural(r,root,std::string(argv[4])=="cycle-resume");}
        else if(mode=="load") load(r,root);
        else if(mode=="boundary") boundary(r,root);
        else if(grabMode) grab(r,root,mode!="grab-failure",mode=="grab-ambush-success");
        else if(mode=="grab-load") {r.load(2);verifyRestored(r,SaveFiles(c,root).read(2));need(!r.game.session().flag(10)&&r.game.session().flag(1),"Grab Load retained owner lock");summary(r,root,mode);}
        else if(mode=="i13-response") {
            r.load(1);r.equip(13);r.enterLift();
            const int rounds=r.game.session().magazine(13),hits=r.game.session().variable(35);
            r.fight(1);
            need(r.game.session().magazine(13)==rounds-1&&r.game.session().variable(35)==hits+1,
                 "Natural I13 target did not receive its one CE11 hit");
            summary(r,root,mode);
        }
        else throw std::runtime_error("Unknown mode");
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
