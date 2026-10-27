#include "g1_piano_driver.h"
#include "enemy_host.h"

// Stage combinations use copied real saves; defect fixtures are explicitly separate.
struct StageDriver : PianoDriver {
    using PianoDriver::PianoDriver;
    void skill(int id) { press(InputAction::Cancel); select(1); select(id); }
};

void bleedingFixture(const CombatContent& content, const SaveSnapshot& saved) {
    auto entry = content.entries.at("post-pickup");
    entry.map = 141; entry.playerTile = {8, 7};
    entry.pages.clear(); entry.positions.clear(); entry.directions.clear(); entry.through.clear();
    entry.session = saved.session;
    entry.session.conditions.bleedingTicks = 600;
    entry.session.conditions.bleedingActive = true;
    entry.session.flags[1] = entry.session.flags[18] = true;
    GameSession session(content.items, entry.session, content.facts);
    CombatScene scene(content, session, entry, [](int, int) { return 0; });
    EnemyHost enemy(scene, "m141-e2");
    session.setVariable(41, 9);
    need(enemy.receiveSourceDamage(1), "Engineering CE12 hit rejected");
    need(session.conditions().bleedingTicks == 600 && session.conditions().bleedingActive, "Repeated bleeding restarted CE34");
    need(session.useMedicine(6) == ItemUseResult::Applied, "Real I6 cure unavailable");
    need(session.conditions().bleedingTicks == 600 && !session.conditions().bleedingActive, "Medicine destroyed CE34 wait");
    session.setVariable(41, 9);
    need(enemy.receiveSourceDamage(1) && session.conditions().bleedingTicks == 600 && session.conditions().bleedingActive,
         "Engineering post-cure hit restarted pending CE34 wait");
    std::cout << "ENGINEERING_FIXED_BLEED CE12 repeated600; I6 cures State3/keeps600; CE12 new State3/keeps600; no natural-state claim\n";
}

int main(int argc, char** argv) { try {
    std::cout.setf(std::ios::unitbuf);
    need(argc == 5, "package real-source new-isolated-root quick|quick-load|bleeding|burn|burn-load|g2-ready|g2-load");
    const auto content = loadCombatContent(argv[1]);
    const auto root = std::filesystem::path(argv[3]);
    std::filesystem::create_directories(root);
    const std::string mode = argv[4];
    if (!mode.ends_with("load")) { std::ofstream(root / "slot-01.json") << read(argv[2]); }
    StageDriver run(content, root, "public-inputs-" + mode + ".jsonl");
    run.game.configureSaveDirectory(root);
    run.load(mode.ends_with("load") ? 2 : 1);
    SaveFiles files(content, root);
    if (mode == "bleeding") { bleedingFixture(content, files.read(1)); return 0; }
    if (mode == "g2-load") {
        need(run.game.mapId()==151 && run.game.session().variable(389)==3 &&
             run.game.session().flag(443) && run.game.session().flag(557) && run.game.session().flag(558) &&
             run.game.session().equipped(1)==22 && run.game.session().magazine(22)==4 && run.game.session().count(22)==3,
             "G2 natural ammunition/quota/facts lost on independent Load");
        run.actObject("m151-e8"); need(run.game.mapId()==155, "G2 actual exit lost");
        std::cout << "NATURAL_G2_LOAD true151->155 inventory22=3 magazine22=4 quota389=3 passed\n";
        return 0;
    }
    if (mode == "g2-ready") {
        need(run.game.mapId()==151 && run.game.session().variable(389)==3 && run.game.session().count(22)==0,
             "G2 first visit source changed");
        const int oldHpMagazine=run.game.session().magazine(13);
        run.actObject("m151-e22"); run.walk({15,11}); run.settle(); run.press();
        need(run.game.session().count(22)==4, "G2 once-only natural I22 grant failed");
        run.equip(22); run.equip(13); run.equip(22);
        need(run.game.session().magazine(22)==4 && run.game.session().magazine(13)==oldHpMagazine,
             "G2 equipment switch refilled another weapon");
        run.actObject("m151-e8"); run.actObject("m155-e9"); run.actObject("m155-e10"); run.actObject("m155-e2");
        run.walk({8,7}); run.move(Direction::Up); run.tick(key());
        need(run.game.mode()==ApplicationMode::Saving,"G2 legal M151 save absent");
        for(int n=0;n<200;++n){run.tick();} run.tick(key()); run.tick(key(InputAction::Down)); run.tick(key());
        need(run.game.mode()==ApplicationMode::Playing,"G2 ordinary save failed");
        validateSave(content,files.read(2));
        std::cout << "NATURAL_G2_READY ticks=" << run.ticks << " maps=" << Json(run.maps)
                  << " actual151Save/onceI22/13-22-switch/155herbs/return151 ammo4 quota3 no injected state\n";
        return 0;
    }
    if (mode == "quick-load") {
        const auto disk = files.read(2);
        need(disk.session.quickshotTicks > 0 && run.game.session().flag(48), "Positive quickshot missing");
        need(run.game.session().quickshotTicks() > disk.session.quickshotTicks,
             "Load did not follow existing positive-value restart contract");
        need(run.game.session().saveCount() == disk.session.saveCount &&
             run.game.session().variable(399) == disk.continuation.statisticBefore + 1,
             "Independent Load double-counted ordinary save");
        std::cout << "NATURAL_QUICK_LOAD disk=" << disk.session.quickshotTicks
                  << " live=" << run.game.session().quickshotTicks() << " old contract retained\n";
        return 0;
    }
    if (mode == "burn-load") {
        const auto disk = files.read(2);
        need(!disk.session.variables.contains(301) && run.game.session().variable(301) == 0,
             "Recorded burn projection omission changed");
        std::cout << "NATURAL_DEFECT_BURN_LOAD saved301absent live301=0 count35="
                  << run.game.session().variable(35) << '\n'; return 0;
    }
    if (mode == "quick") {
        need(run.game.mapId() == 132 && run.game.session().knowsSkill(9), "Natural skill source absent");
        const int tp = run.game.session().tp();
        run.skill(9); const int first = run.game.session().quickshotTicks();
        for (int n = 0; n < 240; ++n) { run.tick(); }
        const int beforeRepeat = run.game.session().quickshotTicks(); run.skill(9);
        need(run.game.session().quickshotTicks() <= beforeRepeat && run.game.session().tp() == tp - 10,
             "Same-room repeated skill refreshed effect or skipped TP");
        const int beforeDoor = run.game.session().quickshotTicks();
        run.actObject("m132-e4"); need(run.game.mapId() == 136 && !run.game.session().flag(48), "CE55 door did not clear48");
        run.skill(9); const int afterDoor = run.game.session().quickshotTicks();
        need(afterDoor <= beforeDoor && afterDoor > 0 && run.game.session().tp() == tp - 15, "Cross-door recast refreshed the paused CE60 wait");
        std::cout << "NATURAL_FIXED_QUICK first=" << first << " sameRoom=" << beforeRepeat
                  << " beforeDoor=" << beforeDoor << " recast=" << afterDoor << " TP=" << run.game.session().tp() << '\n';
        run.actObject("m136-e5"); run.skill(9);
        run.saveAt(2); const auto disk = files.read(2);
        need(disk.session.quickshotTicks > 0 && run.game.session().quickshotTicks() <= disk.session.quickshotTicks,
             "Successful Saved refreshed quickshot");
        need(run.game.session().saveCount() == disk.session.saveCount &&
             run.game.session().variable(399) == disk.continuation.statisticBefore + 1,
             "Ordinary Saved count/statistic mismatch");
        std::cout << "NATURAL_QUICK_SAVE disk=" << disk.session.quickshotTicks
                  << " live=" << run.game.session().quickshotTicks() << " count=" << disk.session.saveCount << '\n';
        return 0;
    }
    need(mode == "burn", "Unknown stage mode");
    need(run.game.mapId() == 132 && !run.game.session().flag(271), "M128 Hunter already dead in source");
    run.equip(17); need(run.game.session().magazine(17) > 0, "Naturally held flame round unavailable");
    run.actObject("m132-e2"); run.actObject("m129-e2"); need(run.game.mapId() == 128, "Natural burn room absent");
    // Remain by the original exit; let the actual Hunter approach the equipped flame weapon.
    run.press(InputAction::Weapon);
    for (int n = 0; n < 3000 && run.game.session().variable(301) == 0; ++n) {
        need(run.game.session().hp() > 0, "Natural burn approach died");
        const auto p = run.game.world().player().motion.tile(), t = findCharacter(run.game.world(), "m128-e2").motion.tile();
        InputFrame input;
        if (!run.game.raised()) { input = key(InputAction::Weapon); }
        else if (std::abs(t.x-p.x)+std::abs(t.y-p.y) <= 4) {
            const Direction face = std::abs(t.x-p.x)>std::abs(t.y-p.y) ? (t.x>p.x?Direction::Right:Direction::Left) : (t.y>p.y?Direction::Down:Direction::Up);
            if (run.game.world().player().motion.facing()!=face) { input.movement=face; input.buttons[int(InputAction::Weapon)].held=true; }
            else if (n%6==0) { input=key(InputAction::Attack); }
        }
        run.tick(input);
    }
    need(run.game.session().variable(301)>0, "Natural flame failed to create nonzero301");
    const int ignited = run.game.session().variable(301);
    std::cout << "NATURAL_BURN_CONTACT counter=" << ignited << " HP=" << run.game.session().hp() << '\n';
    for (int n=0; n<1200 && run.game.raised(); ++n) {
        InputFrame input; input.buttons[int(InputAction::Weapon)].held=n%25<24; run.tick(input);
    }
    need(!run.game.raised(), "Natural launcher lowering did not finish");
    for(int n=0; n<300 && run.game.session().conditions().recoveryTicks>0; ++n){run.tick();}
    run.settle();
    std::cout << "NATURAL_BURN_LOWER counter=" << run.game.session().variable(301)
              << " HP=" << run.game.session().hp() << " recovery=" << run.game.session().conditions().recoveryTicks << '\n';
    run.actObject("m128-e7");
    for(int n=0; n<120 && run.game.mapId()==128; ++n){run.tick(n%8==0?key():InputFrame{});}
    need(run.game.mapId()==129, "Burn natural exit absent");
    const int afterExit = run.game.session().variable(301);
    run.actObject("m129-e4"); need(run.game.mapId()==132 && afterExit>0, "Burn did not carry to safe hall");
    run.saveAt(2); need(run.game.session().variable(301)==afterExit, "Hall/save altered paused burn counter");
    need(!files.read(2).session.variables.contains(301), "Recorded burn omission changed");
    std::cout << "NATURAL_DEFECT_BURN ignited=" << ignited << " afterExit=" << afterExit
              << " hallLive=" << run.game.session().variable(301) << " disk301absent maps=" << Json(run.maps)
              << " no injected position/inventory/counters\n";
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; } }
