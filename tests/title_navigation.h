#pragma once
#include "game_application.h"

// UI fixtures must wait for the native title fade/open before sending decisions.
inline void finishTitleOpening(GameApplication& game){
    while(game.mode()==ApplicationMode::Title&&game.titleOpeningTicks()<8){game.update({});}
}
