#include "combat_scene.h"
#include "enemy_controller.h"
#include <iostream>
#include <stdexcept>
void require(bool value,const char* message){if(!value){throw std::runtime_error(message);}}
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);auto e=c.entries.at("defeated-claimed");e.map=21;e.playerTile={26,9};e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();
 GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e);scene.update({});
 require(scene.enemyStates().size()==3,"Real Map21 creates three independent variant controllers");
 for(const auto& enemy:scene.enemyStates()){require(enemy.mode==EnemyMode::Pursuit,"Map21 live enemies activate from original pages");}

 // Isolated deterministic rule fixtures use original geometry and public attack entry.
 const auto hit=[](CombatScene& scene,const std::string& id,int receiver,AttackKind kind=AttackKind::Handgun){
     const auto at=findCharacter(scene.world(),id).motion.position();std::set<std::string> resolved;
     return scene.resolveAttack({{(at.x+.5)*16,(at.y+.5)*16},{0,0},kind,receiver,0,false},resolved);
 };
 const auto random=[](int low,int high){return low==0&&high==10?2:high;};
 auto fallen=e;fallen.playerTile={26,9};fallen.session.mainWeapon=0;fallen.session.baseAttack=63;fallen.session.flags[47]=false;
 GameSession fs(c.items,fallen.session,c.facts);CombatScene f(c,fs,fallen,random);f.update({});
 require(hit(f,"m21-e5",2).damage==60,"Independent NORMAL sample: base attack63 minus Actor20 defense3");
 const auto firstBlood=f.takePresentation();require(std::any_of(firstBlood.begin(),firstBlood.end(),[](const auto& e){return e.kind==PresentationEvent::Kind::Effect&&e.actor=="m21-e5"&&e.id=="enemy-slot-16-blood-1";}),"Standing blood uses matching source slot and follows target");
 f.update({});require(f.enemyStates()[1].mode==EnemyMode::Fallen&&f.enemyStates()[1].sourceState==100,"60 damage in pursuit enters one-time down phase");
 require(findCharacter(f.world(),"m21-e5").layer==0,"Fallen event becomes lower layer");
 const int before=fs.variable(35);require(hit(f,"m21-e5",2).processed==1&&fs.variable(35)==before,"Fallen receiver2 acknowledges main contact without advancing CE11");
 require(hit(f,"m21-e5",3,AttackKind::Knife).processed==1,"Auxiliary receiver3 may strike downed enemy");
 require(f.enemyStates()[1].mode==EnemyMode::HitRecovery&&f.enemyStates()[1].sourceState==100,"Auxiliary receiver3 cancels down action but preserves source state100");
 require(findCharacter(f.world(),"m21-e5").layer==1,"Interrupted fallen enemy restores standing page immediately");
 std::set<std::string> resolved;const auto pos=findCharacter(f.world(),"m21-e5").motion.position();
 require(f.resolveAttack({{(pos.x+.5)*16,(pos.y+.5)*16},{0,0},AttackKind::De,5,0,false},resolved).processed==1,"Down state does not suppress DE receiver5");
 for(int n=0;n<500;++n){f.update({});}
 require(fs.flag(109)&&!fs.flag(108)&&!fs.flag(110),"Only hit enemy commits permanent death");
 CombatScene returned(c,fs,fallen,random);require(!findCharacter(returned.world(),"m21-e5").active,"Permanent death survives scene recreation");
 auto soldier=e;soldier.session.mainWeapon=0;soldier.session.baseAttack=4;soldier.session.flags[47]=false;
 GameSession ss(c.items,soldier.session,c.facts);CombatScene interrupted(c,ss,soldier,random);interrupted.update({});
 hit(interrupted,"m21-e7",2);require(interrupted.enemyStates()[2].mode==EnemyMode::HitRecovery,"Soldier normal receiver2 interrupts its action");
 hit(interrupted,"m21-e3",2);require(interrupted.enemyStates()[0].mode==EnemyMode::Pursuit,"Running zombie receiver2 does not invent an interruption");

 interrupted.takePresentation();hit(interrupted,"m21-e3",4);const auto bigBlood=interrupted.takePresentation();
 require(std::any_of(bigBlood.begin(),bigBlood.end(),[](const auto& e){return e.kind==PresentationEvent::Kind::Effect&&e.id=="enemy-slot-15-blood-3"&&e.actor=="m21-e3";}),"P4 requests source blood3 before damage/critical response");
 // Same update entry, four tiles away: beyond adjacent attack range but within60px at3 tiles.
 auto rush=e;rush.playerTile={18,5};
 GameSession rs(c.items,rush.session,c.facts);CombatScene runner(c,rs,rush,random);runner.update({});runner.update({});
 require(runner.enemyStates()[0].mode==EnemyMode::Rush,"Audited distant forward corridor activates running phase");
 auto outside=rush;outside.playerTile={18,3};GameSession os(c.items,outside.session,c.facts);CombatScene far(c,os,outside,random);far.update({});far.update({});
 require(far.enemyStates()[0].mode!=EnemyMode::Rush,"Beyond60px does not activate running phase");
 // Natural auxiliary action must dispatch receiver2 (CE39 C76), not receiver3.
 auto knifeEntry=fallen;knifeEntry.playerTile={13,8};knifeEntry.direction=Direction::Up;knifeEntry.session.auxiliaryWeapon=24;
 GameSession knifeSession(c.items,knifeEntry.session,c.facts);CombatScene knife(c,knifeSession,knifeEntry,random);knife.update({});
 hit(knife,"m21-e5",2);knife.update({});require(knife.enemyStates()[1].mode==EnemyMode::Fallen,"Knife fixture enters real down phase");
 InputFrame raise;raise.buttons[static_cast<int>(InputAction::AuxiliaryWeapon)]={true,true,false};knife.update(raise);
 for(int n=0;n<80;++n){knife.update({});}
 InputFrame attack;attack.buttons[static_cast<int>(InputAction::Attack)]={true,true,false};knife.update(attack);
 for(int n=0;n<40;++n){knife.update({});}
 require(knife.enemyStates()[1].damage>60&&knife.enemyStates()[1].mode==EnemyMode::Fallen,"Natural knife uses receiver2: damage without early stand-up");
 // Alter source/container identity without changing rule ownership.
 auto variation=c;auto node=variation.expansion.enemies.extract("m21-e7");node.key()="changed-soldier";node.mapped().id=node.key();variation.expansion.enemies.insert(std::move(node));
 auto object=variation.objects.extract("m21-e7");object.key()="changed-soldier";object.mapped().id=object.key();for(auto& v:object.mapped().variants){v.definition.id=object.key();}variation.objects.insert(std::move(object));
 GameSession vs(variation.items,e.session,variation.facts);CombatScene varied(variation,vs,e,random);varied.update({});
 require(varied.enemyStates()[2].id=="changed-soldier","Stable source order survives changed ID and map container order");
 std::cout<<"F4 zombie checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
