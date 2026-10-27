#include "ending_score.h"
#include <stdexcept>

namespace{
void validate(const EndingScoreInput& in){
    if(in.sprayCount<0||in.retryCount<0||in.saveCount<0||in.hours<0||in.minutes<0||in.minutes>59||in.seconds<0||in.seconds>59||in.guardFirstScore<0||in.difficultyLevel<1||in.difficultyLevel>3){
        throw std::invalid_argument("Invalid ending statistics");
    }
}
}
EndingScoreResult endingScore(const EndingScoreInput& in){
    validate(in);
    EndingScoreResult result;
    result.score=100-10LL*in.sprayCount-2LL*in.retryCount-3LL*in.saveCount;
    if(in.hours>=2){result.score-=11LL*in.hours+2LL*(in.minutes/15);}
    result.rank=result.score>=80?(in.specialItemUsed?10:28):result.score>=70?10:result.score>=60?11:result.score>=50?12:result.score>=30?13:14;
    const auto add=[&](int flag){if(!in.unlocked.at(flag-601)){result.unlocks.push_back(flag);}};
    if(in.difficultyLevel>=2&&(result.rank==28||result.rank==10)){
        if(result.rank==28&&in.saveCount==0&&in.sprayCount==0&&in.retryCount==0){add(604);}
        if(in.hours==0||(in.hours==1&&in.minutes==0&&in.seconds==0)){add(601);}
        if(in.sprayCount==0){add(602);}
        if(in.guardFirstScore>=40){add(603);}
    }
    add(605);
    return result;
}
std::array<EndingScorePicture,9> endingScorePictures(const EndingScoreInput& in,int rank){
    validate(in);
    if(rank!=28&&(rank<10||rank>14)){throw std::invalid_argument("Invalid ending rank");}
    // M213 divides tens without modulo: 100 retains the original A glyph.
    return {{{3,232,4497-240LL*(in.saveCount/10)},{4,249,4497-240LL*(in.saveCount%10)},
             {5,184,4449-240LL*(in.hours/10)},{6,201,4449-240LL*(in.hours%10)},
             {7,232,4449-240LL*(in.minutes/10)},{8,249,4449-240LL*(in.minutes%10)},
             {9,280,4449-240LL*(in.seconds/10)},{10,297,4449-240LL*(in.seconds%10)},
             {11,240,4402-240LL*rank}}};
}
