#include "game_application.h"
#include <iostream>
#include <stdexcept>
void require(bool b,const char* why){if(!b){throw std::runtime_error(why);}}
int main(int argc,char** argv){
    if(argc!=2){return 2;}auto data=loadCombatContent(argv[1]);
    GameApplication scene(data,"natural",[](int lo,int){return lo;});
    InputFrame input;input.movement=Direction::Down;
    for(int i=0;i<8;++i){scene.update(input);}input.movement=Direction::Left;
    for(int i=0;i<8;++i){scene.update(input);}
    require(scene.world().player().motion.tile()==TilePosition{7,6},"walk onto gun");
    input={};input.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;scene.update(input);
    require(scene.session().count(11)==0,"award waits for message");
    for(int i=0;i<1600;++i){InputFrame next;next.buttons[static_cast<int>(InputAction::Confirm)].pressed=(i%40==39);scene.update(next);if(scene.session().flag(52)){break;}}
    require(scene.session().count(11)==1,"one gun");require(scene.session().equipment().mainWeapon==0,"not auto-equipped");
    require(scene.session().magazine(11)==15&&scene.session().count(31)==0,"source ammunition");
    require(scene.session().variable(388)==1000,"natural password uses RNG");
    require(scene.session().flag(52)&&scene.session().flag(18),"control after complete performance");
    require(scene.world().player().motion.tile()==TilePosition{10,6},"player forced route");
    // Natural P7 corpse hands off to P9 pursuit: its fixed direction must not
    // survive the awakening. This runs the real pickup chain above, not a combat snapshot.
    for(int tick=0;tick<12;++tick){scene.update({});}
    const auto soldier=findCharacter(scene.world(),"m4-e4");
    require(!soldier.animation.directionFixed(),"awakened natural soldier uses pursuit animation, not fixed corpse page");
    require(soldier.graphicFacing()==soldier.motion.facing()&&soldier.graphicFacing()!=Direction::Down,"natural soldier pursuit turns toward player");
    std::cout<<"pickup behavior checks passed\n";
}
