#pragma once
#include <array>
#include <cstdint>
#include <vector>

// Read-only values from Session; unlocked indexes are S601 through S605.
struct EndingScoreInput{
    int sprayCount{},retryCount{},saveCount{},hours{},minutes{},seconds{},guardFirstScore{};
    int difficultyLevel{2};
    bool specialItemUsed{};
    std::array<bool,5> unlocked{};
};
struct EndingScoreResult{
    std::int64_t score{};
    int rank{};
    std::vector<int> unlocks;
};
struct EndingScorePicture{int id{},x{};std::int64_t y{};};
EndingScoreResult endingScore(const EndingScoreInput&);
std::array<EndingScorePicture,9> endingScorePictures(const EndingScoreInput&,int rank);
