#pragma once
#include <array>
struct HangarGasRequests{bool callsEmitter{};std::array<bool,3> slots{};};
// Source predicates; HangarBehavior owns the scene-local Wait70 timer.
HangarGasRequests activeHangarGases(int level,const std::array<bool,3>& occupied);
HangarGasRequests passiveHangarGases(int selectorBeforeWait,int selectorAfterWait,const std::array<bool,3>& occupied);
