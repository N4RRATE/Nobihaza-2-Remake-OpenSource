#include "simulation_clock.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
SimulationAdvance SimulationClock::advance(double elapsedSeconds){
    if(!std::isfinite(elapsedSeconds) || elapsedSeconds < 0){
        throw std::runtime_error("Invalid elapsed time");
    }
    double dropped = std::max(0.0, elapsedSeconds - .25);
    accumulated_ += std::min(elapsedSeconds, .25);
    if(accumulated_ > .25){
        dropped += accumulated_ - .25;
        accumulated_ = .25;
    }
    const int steps = std::min(8, static_cast<int>((accumulated_ + 1e-10) / stepSeconds));
    accumulated_ = std::max(0.0, accumulated_ - steps * stepSeconds);
    return {steps,dropped};
}
