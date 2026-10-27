#pragma once
#include "game_application.h"
#include <queue>
#include <map>
inline InputFrame openingKey(InputAction a){InputFrame i;i.buttons[static_cast<int>(a)]={true,true,false};return i;}
inline InputFrame openingInput(const GameApplication& game,bool skip,int tick){
 InputFrame input;const auto stage=game.openingStage();const auto& prompt=game.inputPromptView();
 if(prompt.cursor){const int desired=stage=="opening-choice"?(skip?1:0):1;return openingKey(prompt.cursor->line==desired?InputAction::Confirm:InputAction::Down);}
 if(stage!="door-approach-common"){return tick%13==0?openingKey(InputAction::Confirm):input;}
 const auto& world=game.world();auto actor=world.player();if(actor.motion.moving()){return input;}
 const auto door=game.content().objects.at("m4-e2").tile;const TilePosition goal{door.x,door.y+1},start=actor.motion.tile();
 if(start==goal){if(actor.motion.facing()!=Direction::Up){input.movement=Direction::Up;return input;}return openingKey(InputAction::Confirm);}
 std::queue<TilePosition> q;q.push(start);std::map<std::pair<int,int>,std::optional<Direction>> seen;seen[{start.x,start.y}]={};
 while(!q.empty()){auto at=q.front();q.pop();if(at==goal){input.movement=seen.at({at.x,at.y});return input;}
  for(int d=0;d<4;++d){auto dir=static_cast<Direction>(d);actor.motion=CharacterMotion(at,dir,32);if(!world.canMove(actor,dir)){continue;}auto next=adjacentTile(at,dir);auto k=std::pair{next.x,next.y};if(seen.contains(k)){continue;}seen[k]=seen.at({at.x,at.y}).value_or(dir);q.push(next);}}
 throw std::runtime_error("No opening path");
}
