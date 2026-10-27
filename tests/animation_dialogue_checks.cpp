#include "local_text_fixtures.h"
#include "character_animation.h"
#include "dialogue.h"
#include "exploration_scene.h"
#include <iostream>
#include <functional>
#include <stdexcept>
namespace{
    int checks{};
    void require(bool ok,const char* message){ ++checks; if(!ok){throw std::runtime_error(message);} }
    void rejects(const std::function<void()>& action,const char* message){
        bool failed = false;
        try{action();}catch(const std::exception&){failed=true;}
        require(failed,message);
    }
}
int main(int argc,char** argv){
    try{
        require(argc == 2,"expected scene path");
        CharacterAnimation spin(animationForSpeed(6),0,5);
        for(int i=0;i<16;++i){spin.update(false,true);require(spin.rotation()==((i+1)/4)%4,"Speed6 spin rotates every four logic updates");require(spin.column()==0,"Spin keeps the original graphic column even while animation-paused");}
        CharacterAnimation walk;
        for(int i=0;i<5;++i){ walk.update(true); }
        require(walk.column() == 1,"middle frame lasts until moving threshold");
        walk.update(true);
        require(walk.column() == 2,"sixth moving update shows right step");
        for(int i=0;i<7;++i){ walk.update(false); }
        require(walk.column() == 2,"stop preserves side frame before convergence");
        walk.update(false);
        require(walk.column() == 1,"stopping converges to middle at continuous threshold");
        for(int i=0;i<60;++i){ walk.update(false); }
        require(walk.column() == 1,"idle remains in middle");
        walk.update(true);
        require(walk.column() == 0,"idle animation count is retained for next step");
        CharacterAnimation changing;for(int i=0;i<4;++i){changing.update(true);}
        changing.setSpeedLevel(5);changing.update(true);require(changing.column()==2,"speed change preserves animation counter");
        CharacterAnimation fixed({},0,4);
        for(int i=0;i<30;++i){ fixed.update(true); }
        require(fixed.column() == 0,"fixed graphic never walks even during movement");
        CharacterAnimation continuous({},1,1);
        for(int i=0;i<8;++i){ continuous.update(false); }
        require(continuous.column() == 2,"continuous idle animation advances");
        CharacterAnimation quick(animationForSpeed(5),1,2);
        for(int i=0;i<5;++i){ quick.update(true); }
        require(quick.column() == 2,"another actor speed uses its own animation timing");
        quick.update(true,true);
        require(quick.column() == 1,"animation pause resets normal graphic to middle");
        const auto scene = loadSceneDefinition(argv[1]);
        ExplorationScene real(scene);
        InputFrame down; down.movement = Direction::Down;
        for(int i=0;i<6;++i){ real.update(down); }
        require(real.player().animation.column() == 2,"scene advances walking animation with actual movement");
        for(int i=0;i<40;++i){ real.update({}); }
        require(real.player().animation.column() == 1 && !real.player().motion.moving(),"scene stop converges");
        require(real.objects()[3].animation.column() == 1 && real.objects()[9].animation.column() == 0,
                "stationary soldier and fixed gun keep correct frames");
        MessageDefinition text;
        text.lines = {{"",{{MessageTokenKind::Text,decodeMessageText(localSourceText("animation_dialogue_checks.0")),0}}}};
        Dialogue dialogue;
        const auto request = dialogue.request(text);
        for(int i=0;i<6;++i){ dialogue.update(false,false); }
        require(dialogue.view().phase == DialoguePhase::Opening,"message keeps seven-update opening transition");
        dialogue.update(false,false);
        require(dialogue.view().lines[0] == decodeMessageText(localSourceText("animation_dialogue_checks.1")),"opening finish starts first text update");
        MessageDefinition soldierText;
        soldierText.display = {true,2,true,false};
        soldierText.lines = {{"",{{MessageTokenKind::InstantLine,{},0}}},
                             {"",{{MessageTokenKind::InstantLine,{},0}}},
                             {"",{{MessageTokenKind::Text,decodeMessageText(localSourceText("animation_dialogue_checks.2")),0},
                                  {MessageTokenKind::Speed,{},2},
                                  {MessageTokenKind::Text,decodeMessageText(localSourceText("animation_dialogue_checks.3")),0},
                                  {MessageTokenKind::ShortWait,{},0}}},
                             {"",{{MessageTokenKind::InstantLine,{},0}}}};
        Dialogue timed;
        timed.request(soldierText);
        for(int i=0;i<16;++i){timed.update(true,true);}
        require(timed.view().lines[2] == decodeMessageText(localSourceText("animation_dialogue_checks.4")),"opening plus text tick10 reveals first Chinese character");
        timed.update(true,true); timed.update(true,true);
        require(timed.view().lines[2] == decodeMessageText(localSourceText("animation_dialogue_checks.5")),"confirm does not fast-forward speed2 text");
        timed.update(false,false);
        require(timed.view().lines[2] == decodeMessageText(localSourceText("animation_dialogue_checks.6")),"speed2 fullwidth character takes three updates");
        for(int i=19;i<55;++i){timed.update(true,true);}
        require(timed.view().phase == DialoguePhase::Typing,"short wait and final blank line delay cannot be skipped");
        timed.update(false,false);
        require(timed.view().phase == DialoguePhase::AwaitingInput,"text tick50 waits for fresh input");
        const auto timedId = timed.view().request;
        timed.update(true,false);
        require(timed.view().phase == DialoguePhase::Closing && !timed.takeCompletion(),"confirmation starts close without early completion");
        for(int i=0;i<6;++i){timed.update(true,false);}
        require(timed.active() && !timed.takeCompletion(),"close-finished update remains guarded");
        timed.update(false,false);
        const auto finished = timed.takeCompletion();
        require(!timed.active() && finished && finished->request == timedId &&
                finished->result == MessageResult::Completed && !timed.takeCompletion(),"completion belongs to request and is consumed once");
        const auto repeated = timed.request(soldierText);
        require(repeated != timedId && !timed.cancel(timedId),"old request cannot cancel a repeated message");
        require(timed.cancel(repeated),"caller can cancel its live request");
        for(int i=0;i<7;++i){timed.update(false,false);}
        require(timed.takeCompletion()->result == MessageResult::Cancelled,"cancellation is distinct from completion");
        Dialogue other;
        require(other.request(text) != request,"new scene message identity cannot reuse old request");
        require(scene.messages.contains("soldier-unconscious"),"load real complete message definition");
        require(scene.objects[3].interaction.kind == InteractionKind::Message &&
                scene.objects[9].interaction.kind == InteractionKind::Deferred,"ordinary investigation and deferred gun stay distinct");
        ExplorationScene investigation(scene);
        require(investigation.investigationTarget() == "m4-e4","valid adjacent-facing soldier is selected from data");
        InputFrame confirm; confirm.buttons[static_cast<std::size_t>(InputAction::Confirm)].pressed = true;
        investigation.update(confirm);
        require(investigation.dialogue().active() && !investigation.controls().ordinaryMovement,
                "investigation opens message and gates ordinary movement");
        const auto firstInvestigation = investigation.dialogue().view().request;
        for(int i=0;i<56;++i){investigation.update(down);}
        require(investigation.player().motion.tile() == TilePosition{8,5} &&
                investigation.dialogue().view().phase == DialoguePhase::AwaitingInput,"movement does not leak through active message");
        auto closeAndMove = confirm; closeAndMove.movement = Direction::Down;
        investigation.update(closeAndMove);
        require(investigation.messageOptions() == scene.messages.at("soldier-unconscious").restoreDisplay,
                "message options restore at close start");
        for(int i=0;i<7;++i){investigation.update(closeAndMove);}
        require(!investigation.dialogue().active() && investigation.player().motion.tile() == TilePosition{8,5},
                "closing and cooldown consume input without repeat investigation");
        investigation.update(confirm);
        require(investigation.dialogue().view().request != firstInvestigation,"new investigation uses new request");
        auto denied = investigation.controls(); denied.ordinaryMovement = false; denied.ordinaryAction = false;
        investigation.setControls(denied);
        for(int i=0;i<56;++i){investigation.update({});}
        investigation.update(confirm);
        for(int i=0;i<7;++i){investigation.update({});}
        investigation.update(down);
        require(!investigation.controls().ordinaryMovement && !investigation.controls().ordinaryAction &&
                !investigation.controls().combat && investigation.player().motion.tile() == TilePosition{8,5},
                "message end does not undo independent control restrictions");
        auto wrongFacing = scene; wrongFacing.player.direction = "right";
        ExplorationScene facingAway(wrongFacing); facingAway.update(confirm);
        require(!facingAway.dialogue().active(),"wrong facing does not investigate soldier");
        auto far = scene; far.player.tileX = 9;
        ExplorationScene distant(far); distant.update(confirm);
        require(!distant.dialogue().active(),"distant soldier does not trigger");
        ExplorationScene dynamic(scene);
        auto hiddenSoldier = dynamic.objects()[3]; hiddenSoldier.active = false; dynamic.setObjectState(hiddenSoldier);
        dynamic.update(confirm);
        require(!dynamic.dialogue().active(),"hidden target is not investigated");
        auto itemScene = scene; itemScene.objects[3].layer = 0;
        itemScene.objects[3].tileX = 8; itemScene.objects[3].tileY = 5;
        itemScene.objects[3].interaction.geometry = InteractionGeometry::SameTile;
        ExplorationScene sameTile(itemScene); sameTile.update(confirm);
        require(sameTile.dialogue().active(),"message investigation supports same-tile geometry without soldier coordinates");
        ExplorationScene focused(scene); InputBuffer heldConfirm;
        heldConfirm.press(InputAction::Confirm); focused.update(heldConfirm.consume());
        heldConfirm.setFocused(false);
        for(int i=0;i<60;++i){focused.update(heldConfirm.consume());}
        heldConfirm.setFocused(true); focused.update(heldConfirm.consume());
        require(focused.dialogue().view().phase == DialoguePhase::AwaitingInput,"focus loss does not auto-close or replay confirm");
        heldConfirm.press(InputAction::Cancel); focused.update(heldConfirm.consume());
        require(focused.dialogue().view().phase == DialoguePhase::Closing,"fresh cancel closes final page");
        ExplorationScene freeAfter(scene); freeAfter.update(confirm);
        for(int i=0;i<56;++i){freeAfter.update({});}
        freeAfter.update(closeAndMove);
        for(int i=0;i<7;++i){freeAfter.update(down);}
        freeAfter.update(down);
        require(freeAfter.player().motion.tile() == TilePosition{8,6},"held direction resumes at T+8 after final confirm");
        Dialogue exactEdge; exactEdge.request(soldierText);
        for(int i=0;i<55;++i){exactEdge.update(false,false);}
        exactEdge.update(true,false);
        require(exactEdge.view().phase == DialoguePhase::Closing,"confirmation at text update50 is accepted immediately");
        Dialogue invalidMessage;
        rejects([&]{invalidMessage.request({});},"empty message request is rejected");
        require(!invalidMessage.active(),"failed request does not leave a half-active dialogue");
        ExplorationScene threeSteps(scene);
        for(int i=0;i<24;++i){threeSteps.update(down);}
        require(threeSteps.player().animation.column() == 1 && !threeSteps.player().motion.moving(),
                "arrival update still advances the walking phase");
        auto invalidSpeed = soldierText; invalidSpeed.lines[2].tokens[1].value = 21;
        rejects([&]{invalidMessage.request(invalidSpeed);},"unsupported speed rejected through message interface");
        require(!invalidMessage.active(),"late validation failure leaves request state untouched");
        ExplorationScene simultaneous(scene);
        simultaneous.update(closeAndMove);
        require(!simultaneous.dialogue().active() && simultaneous.player().motion.tile() == TilePosition{8,6},
                "ordinary movement starts before investigating an old facing target");
        auto unsupported = soldierText; unsupported.display.continueEvents = true;
        invalidMessage.request(unsupported);require(invalidMessage.view().display.continueEvents,"dialogue preserves event continuation for owning scene scheduler");invalidMessage.invalidate();
        unsupported = soldierText; unsupported.display.fixedPosition = false;
        rejects([&]{invalidMessage.request(unsupported);},"unimplemented automatic message placement is rejected");
        std::cout << checks << " animation/dialogue checks passed\n";
    }catch(const std::exception& e){std::cerr << e.what() << '\n';return 1;}
}
