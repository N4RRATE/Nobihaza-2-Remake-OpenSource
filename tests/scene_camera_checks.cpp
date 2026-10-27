#include "scene_camera.h"
#include <stdexcept>
#include <iostream>
void require(bool b,const char* m){if(!b){throw std::runtime_error(m);}}
int main(){
 for(int side=0;side<2;++side){
  SceneCamera camera;const GridPosition player{21.+side,27};camera.reset(player,25,40);
  require(camera.center().x==240&&camera.center().y==440,"actual entry viewport is clipped before pan");
  require(camera.pan(Direction::Left,3+side,6)==6+side*2,"lateral pan scheduled duration");
  camera.tick(player);require(camera.center().x==232,"first pan tick immediately moves away from boundary");
  for(int n=1;n<6+side*2;++n){camera.tick(player);}
  require(camera.center().x==192-side*16,"entry-specific lateral pan is visible");
  const int wait=camera.pan(Direction::Up,27,6);require(wait==54,"edge does not shorten scheduled Wait");
  for(int n=0;n<wait;++n){camera.tick(player);}
  require(camera.moving()&&camera.center().y==120,"blocked pan target stays pending; interpreter wait still expires");
  camera.pan(Direction::Down,27,2);for(int n=0;n<618;++n){camera.tick(player);}
  require(camera.center().y==429,"reverse pan starts at actual top with no invisible overscroll debt");
  const int restore=camera.restore(6);require(restore==6+side*2,"reset uses actual accumulated pan");
  for(int n=0;n<restore;++n){camera.tick(player);}
  require(!camera.moving()&&camera.center().x==240&&camera.center().y==440,"reset reaches original actual viewport");
 }
 SceneCamera small;small.reset({4,5},20,15);require(small.center().x==160&&small.center().y==120,"small room stays centered");
 std::cout<<"camera checks passed\n";
}
