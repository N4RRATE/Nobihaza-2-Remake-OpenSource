#include "world_geometry.h"
#include "character_motion.h"
#include "input_buffer.h"
#include "simulation_clock.h"
#include "scene_definition.h"
#include "exploration_scene.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace{
    int checks{};
    void require(bool condition, const char* message){
        ++checks;
        if(!condition){ throw std::runtime_error(message); }
    }
}
int main(int argc, char* argv[]){
    try{
        // 3x2 independent fixture: only the right edge of (0,0) and left edge of (1,0) open.
        TerrainMap terrain(3,2,{4,2,0,15,15,15});
        require(terrain.canTraverse({0,0},Direction::Right), "two matching direction bits permit crossing");
        require(!terrain.canTraverse({0,0},Direction::Down), "closed outgoing edge must block");
        require(!terrain.canTraverse({1,1},Direction::Up), "closed incoming edge must block");
        require(!terrain.canTraverse({0,0},Direction::Left), "map edge must block");
        require(!terrain.canTraverse({3,0},Direction::Left), "invalid origin must block");
        CharacterMotion player({1,1},Direction::Down,32);
        require(player.beginStep(Direction::Right), "idle actor starts step");
        require(player.tile() == TilePosition{2,1}, "logical tile commits at start");
        require(player.position().x == 1.0, "display starts at origin, not destination");
        player.advance();
        require(player.position().x == 1.125 && player.remainingSubunits() == 224, "first of eight updates");
        require(!player.beginStep(Direction::Up), "cannot start another step mid-grid");
        player.face(Direction::Up);
        require(player.facing() == Direction::Right, "ordinary turning waits for step end");
        for(int i = 0; i < 7; ++i){ player.advance(); }
        require(!player.moving() && player.position().x == 2.0, "eighth update completes exactly");
        CharacterMotion otherActor({4,3},Direction::Up,64);
        otherActor.beginStep(Direction::Up);
        for(int i = 0; i < 4; ++i){ otherActor.advance(); }
        require(otherActor.tile() == TilePosition{4,2} && !otherActor.moving(), "same primitive supports another actor speed");
        InputBuffer input;
        input.press(InputAction::Up);
        require(input.consume().movement == Direction::Up, "held up produces intent");
        input.press(InputAction::Right);
        require(input.consume().movement == Direction::Right, "newer perpendicular key wins");
        input.press(InputAction::Left);
        require(!input.consume().movement, "opposite pair cancels even with a third key");
        input.setFocused(false);
        input.setFocused(true);
        const auto afterFocus = input.consume();
        require(!afterFocus.movement && !afterFocus.buttons[0].held && !afterFocus.buttons[1].held &&
                !afterFocus.buttons[3].held && !afterFocus.buttons[3].pressed,
                "focus loss clears held state and pending edges");
        InputBuffer tap;tap.press(InputAction::Down);tap.release(InputAction::Down);
        require(tap.consume().movement==Direction::Down,"short tap survives until one logic step");
        require(!tap.consume().movement,"short tap is not sticky");
        InputBuffer simultaneous;
        simultaneous.press(InputAction::Up); simultaneous.press(InputAction::Right);
        require(simultaneous.consume().movement == Direction::Right, "same-update tie order");
        simultaneous.release(InputAction::Right);
        require(simultaneous.consume().movement == Direction::Up, "release restores remaining direction");
        input.press(InputAction::Confirm); input.release(InputAction::Confirm);
        auto edge = input.consume();
        require(edge.buttons[4].pressed && edge.buttons[4].released && !edge.buttons[4].held,
                "tap edges survive before next logical update");
        require(!input.consume().buttons[4].pressed, "edge consumed exactly once");
        input.setFocused(false); input.press(InputAction::Down); input.setFocused(true);
        require(!input.consume().movement, "background press is not replayed");
        for(int hz : {30,60,144}){
            SimulationClock clock;
            int steps = 0;
            for(int frame = 0; frame < hz; ++frame){ steps += clock.advance(1.0/hz).steps; }
            require(steps == 60, "same one-second simulation at different render rates");
        }
        SimulationClock stall;
        const auto catchup = stall.advance(1.0);
        require(catchup.steps == 8 && std::abs(catchup.droppedSeconds - .75) < 1e-9,
                "long stall clamps time and reports discarded duration");
        require(stall.advance(0).steps == 7 && stall.advance(0).steps == 0, "bounded backlog drains without extra elapsed time");
        SimulationClock tiny;
        require(tiny.advance(.001).steps == 0, "render-only frame does not advance logic");
        require(argc == 2, "expected real scene path");
        const auto definition = loadSceneDefinition(argv[1]);
        require(definition.passageMasks.size() == 300, "load real directional terrain");
        require(definition.player.moveSpeed == 4 && definition.player.subunitsPerUpdate == 32 &&
                definition.player.subunitsPerTile == 256, "validated source movement units");
        require(definition.controls.ordinaryMovement && !definition.controls.combat, "separate control channels");
        require(definition.objects[3].active && !definition.objects[3].through &&
                !definition.objects[11].active, "active page and through are independent of graphics");
        ExplorationScene world(definition);
        InputFrame left; left.movement = Direction::Left;
        world.update(left);
        require(world.player().motion.tile() == TilePosition{8,5} && !world.player().motion.moving(),
                "live soldier blocks player despite passable floor");
        auto soldier = world.objects()[3]; soldier.active = false;
        world.setObjectState(soldier);
        require(world.canPlayerMove(Direction::Left), "deactivated object releases occupancy immediately");
        world.update(left);
        require(world.player().motion.tile() == TilePosition{7,5} &&
                world.player().motion.position().x == 7.875, "scene commits tile and advances continuous position");
        InputFrame down; down.movement = Direction::Down;
        world.update(down);
        require(world.player().motion.facing() == Direction::Left, "turning deferred while moving");
        for(int i=0;i<6;++i){ world.update({}); }
        require(world.player().motion.position().x == 7.0 && !world.player().motion.moving(),
                "release finishes current step");
        world.update(down);
        require(world.player().motion.tile() == TilePosition{7,6}, "gun below layer does not block");
        ExplorationScene gated(definition);
        auto permissions = gated.controls(); permissions.ordinaryMovement = false;
        gated.setControls(permissions); gated.update(down);
        require(gated.player().motion.tile() == TilePosition{8,5}, "movement permission gates ordinary starts");
        require(gated.controls().ordinaryAction && !gated.controls().combat, "gating movement preserves other permissions");
        auto wallEntry = definition;
        wallEntry.player.tileX = 6; wallEntry.player.tileY = 5;
        wallEntry.player.direction = "down";
        ExplorationScene realWall(wallEntry);
        realWall.update(left);
        require(realWall.player().motion.tile() == TilePosition{6,5} &&
                !realWall.player().motion.moving() && realWall.player().motion.facing() == Direction::Left,
                "real map wall at (5,5) blocks movement but allows turning");
        ExplorationScene occupancy(definition);
        auto blocker = occupancy.objects()[3];
        blocker.spriteId.reset(); occupancy.setObjectState(blocker);
        require(!occupancy.canPlayerMove(Direction::Left), "empty same-layer graphics still block");
        blocker.through = true; occupancy.setObjectState(blocker);
        require(occupancy.canPlayerMove(Direction::Left), "through change releases occupancy immediately");
        blocker.through = false;
        blocker.motion = CharacterMotion({9,5},Direction::Down,32);
        occupancy.setObjectState(blocker);
        require(occupancy.canPlayerMove(Direction::Left) && !occupancy.canPlayerMove(Direction::Right),
                "object relocation changes both occupied tiles");
        blocker.layer = 0; blocker.overlapForbidden = true; occupancy.setObjectState(blocker);
        require(occupancy.canPlayerMove(Direction::Right), "event overlap flag does not become player blocking");
        auto alternate = definition;
        alternate.width = 25; alternate.height = 15;
        alternate.passageMasks.assign(375,15); alternate.objects.clear();
        alternate.player.tileX = 24; alternate.player.tileY = 14;
        ExplorationScene edgeWorld(alternate);
        InputFrame right; right.movement = Direction::Right;
        edgeWorld.update(right);
        require(edgeWorld.player().motion.tile() == TilePosition{24,14} &&
                edgeWorld.player().motion.facing() == Direction::Right, "different map dimensions and blocked turning");
        require(!edgeWorld.canPlayerMove(Direction::Down), "bottom edge is derived from data");
        alternate.width = 50; alternate.height = 3; alternate.passageMasks.assign(150,15);
        alternate.player.tileX = 1; alternate.player.tileY = 1;
        for(int hz : {30,60,144}){
            ExplorationScene paced(alternate);
            InputBuffer keys; keys.press(InputAction::Right);
            SimulationClock timer;
            for(int frame = 0; frame < hz; ++frame){
                const auto ticks = timer.advance(1.0/hz);
                for(int i=0;i<ticks.steps;++i){ paced.update(keys.consume()); }
            }
            require(paced.player().motion.tile() == TilePosition{9,1} &&
                    paced.player().motion.position().x == 8.5,
                    "same scene result at 30/60/144 display updates per second");
        }
        ExplorationScene background(alternate);
        InputBuffer keys; keys.press(InputAction::Right); background.update(keys.consume());
        keys.setFocused(false);
        for(int i=0;i<20;++i){ background.update(keys.consume()); }
        require(background.player().motion.tile() == TilePosition{2,1} && !background.player().motion.moving(),
                "world finishes current movement while input is unfocused");
        keys.setFocused(true); background.update(keys.consume());
        require(background.player().motion.tile() == TilePosition{2,1}, "refocus does not replay held key");
        keys.press(InputAction::Right); background.update(keys.consume());
        require(background.player().motion.tile() == TilePosition{3,1}, "fresh press resumes movement");
        ExplorationScene fresh(alternate);
        require(fresh.player().motion.tile() == TilePosition{1,1} && !fresh.player().motion.moving(),
                "new scene has no stale transient movement");
        SimulationClock backlog;
        (void)backlog.advance(.25);
        require(backlog.advance(.2).droppedSeconds > .06, "accumulated stall is bounded as well");
        InputBuffer repeat;
        repeat.press(InputAction::Down); (void)repeat.consume(); repeat.press(InputAction::Down);
        require(!repeat.consume().buttons[2].pressed, "duplicate held press does not create edge");
        std::cout << checks << " movement checks passed\n";
    }catch(const std::exception& error){
        std::cerr << "Movement check failed: " << error.what() << '\n';
        return 1;
    }
}
