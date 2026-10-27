#include "game_application.h"
#include <stdexcept>
#include <iostream>
void require(bool b,const char* why){if(!b){throw std::runtime_error(why);}}
InputFrame press(InputAction a){InputFrame f;f.buttons[static_cast<int>(a)]={true,true,false};return f;}
int main(int argc,char** argv){
 if(argc!=2){return 2;}auto data=loadCombatContent(argv[1]);
 GameApplication scene(data,"post-pickup",[](int lo,int){return lo;});
 scene.update(press(InputAction::Cancel)); require(scene.mode()==ApplicationMode::Equipment,"X opens actual equipment");
 scene.update(press(InputAction::Down));scene.update(press(InputAction::Confirm));
 require(scene.session().equipment().mainWeapon==11,"choose owned gun");
 scene.update(press(InputAction::Cancel));require(scene.mode()==ApplicationMode::Playing,"leave menu");
 require(scene.session().magazine(11)==15,"menu exit does not shoot");
 InputFrame shift;shift.buttons[static_cast<int>(InputAction::Weapon)].held=true;
 for(int i=0;i<6;++i){scene.update(shift);}InputFrame release;release.buttons[static_cast<int>(InputAction::Weapon)].released=true;scene.update(release);
 for(int i=0;i<40;++i){scene.update({});}
 require(scene.raised(),"weapon intent raises");scene.update(press(InputAction::Confirm));
 require(scene.session().magazine(11)==15,"confirmation no longer fires the weapon");
 scene.update(press(InputAction::Attack));
 require(scene.session().magazine(11)==14,"actual firing consumes one");
 for(int i=0;i<120;++i){scene.update({});}
 require(scene.session().magazine(11)==14,"no held input no repeated shot");
 std::cout<<"weapon input checks passed\n";
}
