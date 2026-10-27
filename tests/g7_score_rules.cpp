#include "ending_score.h"
#include <cassert>
#include <climits>
#include <iostream>
#include <stdexcept>

int main(){
    EndingScoreInput in;in.guardFirstScore=40;
    auto result=endingScore(in);
    assert(result.score==100&&result.rank==28&&(result.unlocks==std::vector<int>{604,601,602,603,605}));
    in.hours=1;assert(endingScore(in).unlocks==result.unlocks);
    in.seconds=1;assert((endingScore(in).unlocks==std::vector<int>{604,602,603,605}));
    in.hours=0;in.minutes=59;in.seconds=59;assert(endingScore(in).unlocks==result.unlocks);
    in={};in.hours=1;in.minutes=59;assert(endingScore(in).score==100);
    in.hours=2;in.minutes=14;assert(endingScore(in).score==78);
    in.minutes=15;assert(endingScore(in).score==76);
    in.minutes=59;assert(endingScore(in).score==72);
    // Independently expected thresholds from CN1.00 M213E1P1 C34-70.
    for(const auto [retries,expected]:std::array<std::array<int,2>,8>{{{10,28},{11,10},{15,10},{16,11},{20,11},{21,12},{25,12},{26,13}}}){
        in={};in.retryCount=retries;assert(endingScore(in).rank==expected);
    }
    in={};in.retryCount=9;in.saveCount=1;assert(endingScore(in).score==79&&endingScore(in).rank==10);
    in.retryCount=14;assert(endingScore(in).score==69&&endingScore(in).rank==11);
    in.retryCount=35;in.saveCount=0;assert(endingScore(in).rank==13);
    in.saveCount=1;assert(endingScore(in).rank==14);
    in={};in.specialItemUsed=true;in.guardFirstScore=40;
    assert(endingScore(in).rank==10&&(endingScore(in).unlocks==std::vector<int>{601,602,603,605}));
    in={};in.guardFirstScore=39;assert((endingScore(in).unlocks==std::vector<int>{604,601,602,605}));
    in.sprayCount=1;assert((endingScore(in).unlocks==std::vector<int>{601,605}));
    in={};in.difficultyLevel=1;assert((endingScore(in).unlocks==std::vector<int>{605}));
    in.difficultyLevel=3;in.guardFirstScore=40;assert(endingScore(in).unlocks==result.unlocks);
    in.unlocked={true,false,true,false,true};assert((endingScore(in).unlocks==std::vector<int>{604,602}));
    const auto before=in;in.unlocked.fill(true);assert(endingScore(in).unlocks.empty());
    in=before;assert(endingScore(in).unlocks==std::vector<int>({604,602}));
    in={};in.saveCount=100;auto pictures=endingScorePictures(in,28);
    assert(endingScore(in).score==-200&&endingScore(in).rank==14);
    assert(pictures[0].id==3&&pictures[0].x==232&&pictures[0].y==2097&&pictures[1].y==4497&&pictures[8].y==-2318);
    in.saveCount=360;assert(endingScorePictures(in,10)[0].y==-4143);
    in.saveCount=370;assert(endingScorePictures(in,14)[0].y==-4383);
    in={};in.sprayCount=INT_MAX;in.retryCount=INT_MAX;in.saveCount=INT_MAX;in.hours=INT_MAX;
    assert(endingScore(in).score==100-26LL*INT_MAX);
    const auto rejects=[](EndingScoreInput bad){try{endingScore(bad);}catch(const std::invalid_argument&){return true;}return false;};
    in={};in.minutes=60;assert(rejects(in));in={};in.retryCount=-1;assert(rejects(in));in={};in.difficultyLevel=4;assert(rejects(in));
    bool badRank=false;try{endingScorePictures({},15);}catch(const std::invalid_argument&){badRank=true;}assert(badRank);
    std::cout<<"G7-04 ending score, ordered eligibility, original glyph mapping and invalid input checks passed\n";
}
