#include "scene_camera.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
GridPosition SceneCamera::clamp(GridPosition value) const{
    return {loopX_?value.x:width_<=20?width_*8.:std::clamp(value.x,160.,width_*16.-160),
            height_<=15?height_*8.:std::clamp(value.y,120.,height_*16.-120)};
}
void SceneCamera::reset(GridPosition player,int width,int height,bool loopX){
    if(width<1||height<1){throw std::runtime_error("Invalid camera map size");}
    loopX_=loopX;width_=width;height_=height;player_=player;center_=clamp({player.x*16+16,player.y*16+8});offset_=target_={};locked_=false;
}
void SceneCamera::lock(bool value){locked_=value;}
int SceneCamera::schedule(int speed){
    if(speed<1||speed>6){throw std::runtime_error("Invalid camera speed");}step_=(2<<speed)/16.;
    return static_cast<int>(std::ceil(std::max(std::abs(target_.x-offset_.x),std::abs(target_.y-offset_.y))/step_));
}
int SceneCamera::pan(Direction direction,int tiles,int speed){
    if(tiles<0){throw std::runtime_error("Invalid camera pan distance");}
    const auto delta=adjacentTile({0,0},direction);target_.x+=delta.x*tiles*16;target_.y+=delta.y*tiles*16;return schedule(speed);
}
int SceneCamera::restore(int speed){target_={};return schedule(speed);}
bool SceneCamera::moving() const{return offset_.x!=target_.x||offset_.y!=target_.y;}
void SceneCamera::tick(GridPosition player){
    if(!locked_){
        const auto follow=[](double current,double wanted,double movement){
            if(movement>0&&wanted>current){return current+std::min(movement,wanted-current);}
            if(movement<0&&wanted<current){return current+std::max(movement,wanted-current);}return current;
        };
        center_=clamp({follow(center_.x,player.x*16+16+offset_.x,(player.x-player_.x)*16),
                       follow(center_.y,player.y*16+8+offset_.y,(player.y-player_.y)*16)});
    }
    player_=player;const auto previous=center_;
    center_=clamp({center_.x+std::clamp(target_.x-offset_.x,-step_,step_),center_.y+std::clamp(target_.y-offset_.y,-step_,step_)});
    offset_.x+=center_.x-previous.x;offset_.y+=center_.y-previous.y;
}
