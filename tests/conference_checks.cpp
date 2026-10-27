#include "combat_scene.h"
#include <iostream>
void need(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
InputFrame confirm(){InputFrame i;i.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;return i;}
CombatEntry entry(const CombatContent& c){
 auto e=c.entries.at("defeated-claimed");e.map=24;e.playerTile={11,5};e.direction=Direction::Down;
 for(int flag:{73,75,76,77,78,79,80,81,82})e.session.flags[flag]=true;
 e.positions["m24-e2"]={11,6};e.positions["m24-e3"]={11,7};e.positions["m24-e10"]={10,5};e.positions["m24-e5"]={14,5};return e;
}
int main(int argc,char**argv){try{
 if(argc!=2)return 2;auto c=loadCombatContent(argv[1]);auto e=entry(c);GameSession session(c.items,e.session,c.facts);session.learnSkill(3);
 CombatScene scene(c,session,e);scene.update(confirm());bool saw25=false;
 for(int n=0;n<40000&&scene.mapId()!=12;++n){scene.update(n%11==0?confirm():InputFrame{});scene.takePresentation();saw25|=scene.mapId()==25;}
 need(saw25&&scene.mapId()==12,"Meeting must pass through Map25 and return to Map12");
 need(scene.world().player().motion.tile()==TilePosition{19,17},"Original Map12 return location");
 need(scene.foreground()!=ForegroundKind::None&&!session.flag(1)&&!session.flag(18)&&!session.flag(84),"Transfer must not release control or commit return reward early");
 need(session.knowsSkill(4)&&!session.knowsSkill(3),"Escape converts only the known mixing skill");
 // A late binding error is detected before the first control mutation.
 auto broken=c;broken.routes.at("m25-e9-p2-c84").target=9999;GameSession unchanged(broken.items,e.session,broken.facts);CombatScene invalid(broken,unchanged,e);
 const auto original=unchanged.exportState();bool rejected=false;try{invalid.update(confirm());}catch(const std::exception&){rejected=true;}
 need(rejected&&unchanged.exportState()==original&&invalid.mapId()==24,"Preflight rejects missing future actor before mutation");
 // Presentation preparation is the real application callback. Failure cannot
 // replace the world, consume the skill or advance the stage.
 GameSession retryState(c.items,e.session,c.facts);retryState.learnSkill(3);int attempts=0;bool fail=true;
 CombatScene retry(c,retryState,e,{},[&](const SceneDefinition&){if(++attempts>1&&fail)throw std::runtime_error("injected resource failure");});
 const auto initialGeneration=retry.generation();retry.update(confirm());
 for(int n=0;n<20000&&retry.transferError().empty();++n){retry.update(n%11==0?confirm():InputFrame{});retry.takePresentation();}
 need(!retry.transferError().empty()&&retry.mapId()==24&&retry.generation()==initialGeneration,"Candidate failure retains old world identity");
 const int failedAttempts=attempts;for(int n=0;n<100;++n)retry.update({});need(attempts==failedAttempts,"Failure never silently spins retries");
 fail=false;retry.update(confirm());for(int n=0;n<30000&&retry.mapId()!=12;++n){retry.update(n%11==0?confirm():InputFrame{});retry.takePresentation();}
 need(retry.mapId()==12&&retry.generation()!=initialGeneration,"Explicit retry completes fresh scene identities");
 retry.deactivate();const auto stopped=retryState.exportState();for(int n=0;n<100;++n)retry.update(confirm());need(stopped==retryState.exportState(),"Deactivated story cannot commit old results");
 // Changed approach direction and no teaching skill preserve the source branch.
 auto side=e;side.playerTile={12,6};side.direction=Direction::Left;GameSession untrained(c.items,side.session,c.facts);CombatScene alternate(c,untrained,side);alternate.update(confirm());
 for(int n=0;n<40000&&alternate.mapId()!=12;++n){alternate.update(n%11==0?confirm():InputFrame{});alternate.takePresentation();}
 need(alternate.mapId()==12&&!untrained.knowsSkill(3)&&!untrained.knowsSkill(4),"No-skill branch never grants mixing; another facing completes");
 std::cout<<"Conference checks passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
