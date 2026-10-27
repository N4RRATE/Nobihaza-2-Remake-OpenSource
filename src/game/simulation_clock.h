#pragma once
struct SimulationAdvance{
    int steps{};
    double droppedSeconds{};
};
class SimulationClock{
public:
    static constexpr double stepSeconds = 1.0 / 60.0;
    SimulationAdvance advance(double elapsedSeconds);
private:
    double accumulated_{};
};
