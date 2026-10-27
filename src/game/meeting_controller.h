#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <nlohmann/json.hpp>
class CombatScene;
// Finite story coordinator. The borrowed scene remains the sole owner of actors.
class MeetingController{
public:
    explicit MeetingController(CombatScene&);
    void advance();
    void retry(){failed_=false;}
    bool waitingForReturn() const;
private:
    enum class Stage{BeforeFacing,Facing,AfterFacing,Transfer25,AfterTransfer,Escape,Transfer12,AwaitReturn};
    bool run(const nlohmann::json&,const std::string&);
    bool transfer(const nlohmann::json&);
    void enter(Stage stage){stage_=stage;cursor_=0;}
    CombatScene& scene_;
    Stage stage_{Stage::BeforeFacing};
    std::size_t cursor_{};
    std::uint64_t generation_{};
    nlohmann::json facingSteps_;
    bool failed_{};
};
