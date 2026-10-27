#pragma once
#include "presentation_event.h"
#include "input_buffer.h"
// Bounded document presentation; borrows content and a read-only session for message values.
struct ReadingRestore{std::uint64_t scene;MessageDisplay display;};
class ReadingController{
public:
    ReadingController(const CombatContent& content,const GameSession& session,std::string definition,std::uint64_t scene);
    void update(const InputFrame& input,std::uint64_t scene);
    void close();
    const std::string& definition() const{return definition_;}
    bool belongsTo(std::uint64_t scene) const{return scene==scene_;}
    void repeat();
    std::optional<ReadingRestore> restore() const{return finished()&&!cancelled_?std::optional<ReadingRestore>{{scene_,display_}}:std::nullopt;}
    bool atMenu() const{return phase_==Phase::Menu;}
    bool finished() const{return phase_==Phase::Finished;}
    const Dialogue& dialogue() const{return dialogue_;}
    std::vector<PresentationEvent> takePresentation();
private:
    enum class Phase{Open,Body,Return,Menu,Close,Finished};
    void nextPhase();
    const CombatContent& content_;const GameSession& session_;std::string definition_;std::uint64_t scene_;
    bool cancelled_{};
    Phase phase_{Phase::Open};std::size_t next_{};int wait_{};
    Dialogue dialogue_;MessageDisplay display_;
    std::vector<PresentationEvent> presentation_;
};
