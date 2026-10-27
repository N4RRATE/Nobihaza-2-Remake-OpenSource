#include "combat_scene.h"
#include "enemy_controller.h"
#include "full_content.h"
#include <iostream>
#include <stdexcept>
namespace{
void need(bool v,const char* why){if(!v){throw std::runtime_error(why);}}
InputFrame confirm(){InputFrame f;f.buttons[static_cast<int>(InputAction::Confirm)].pressed=true;return f;}
CombatEntry entry(const CombatContent& c,int map=117){auto e=c.entries.at("post-pickup");initializeFullSession(c,e.session);e.map=map;e.playerTile=map==116?TilePosition{13,7}:TilePosition{22,13};e.direction=Direction::Up;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.flags[1]=e.session.flags[18]=true;e.session.flags[341]=false;e.session.hp=e.session.maxHp=5000;e.session.baseAttack=200;e.session.mainWeapon=19;e.session.inventory[19]=1;for(int i=240;i<=269;++i){e.session.variables[i]=0;}e.session.variables[247]=13;e.session.flags[13]=true;e.session.flags[222]=true;for(const auto& [n,r]:c.catalog.at("guard_second").at("runtime_profile").at("slots").items()){e.session.flags[r.at("dead_fact").get<int>()]=false;e.session.variables[r.at("source_slots").at("damage").get<int>()]=0;e.session.variables[r.at("source_slots").at(n=="8"?"state":"mode").get<int>()]=0;}return e;}
EnemyView enemy(const CombatScene& s,const std::string& id){for(const auto& e:s.enemyStates()){if(e.id==id){return e;}}throw std::runtime_error("Missing guard slot");}
GridPosition body(const CombatScene& s,const std::string& id){const auto p=findCharacter(s.world(),id).motion.position();return {(p.x+.5)*16,(p.y+1)*16-8};}
void tick(CombatScene& s,int n){while(n--){s.update({});}}
void burnRegeneration(const CombatContent& c){
    auto e=entry(c);GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,[](int lo,int){return lo;});
    const auto& burn=c.full->at("runtime").at("weapon_actions").at("burns").at("8");
    const std::string erase=burn.at("erase_cue");bool erased=false;
    const auto advance=[&]{scene.update({});for(const auto& event:scene.takePresentation()){erased|=event.kind==PresentationEvent::Kind::Cue&&event.id==erase;}};
    std::set<std::string> hit;
    scene.resolveAttack({body(scene,"m117-e22"),{0,0},AttackKind::GlFlame,6,0,false,0,AttackPhase::Burning},hit);
    need(s.flag(44)&&hit.contains("m117-e22"),"Public GL flame starts CE31 on guard dog");advance();
    need(s.burnWaitRemaining(31)>0,"CE31 begins its source wait before direct kill");
    for(int n=0;n<6;++n){advance();}
    need(enemy(scene,"m117-e22").damage==4&&s.variable(308)==1,"Living dog advances CE31 once per frame and takes one source burn pulse");
    hit.clear();scene.resolveAttack({body(scene,"m117-e22"),{0,0},AttackKind::Magnum,5},hit);
    need(enemy(scene,"m117-e22").damage>=150,"Public direct shot kills burning dog before CE31 expires");
    for(int n=0;n<6;++n){advance();}
    need(s.variable(308)>1,"CE31 keeps advancing after direct kill while dog death presentation runs");
    bool hidden=false,revived=false;int corpseDamage=0;
    for(int n=0;n<3000&&!revived;++n){
        advance();const auto dog=findCharacter(scene.world(),"m117-e22");
        if(s.flag(256)&&!corpseDamage){corpseDamage=enemy(scene,"m117-e22").damage;}
        if(s.flag(256)&&dog.active){need(enemy(scene,"m117-e22").damage==corpseDamage,"Burn never submits damage to dead dog");}
        if(!dog.active){hidden=true;need(enemy(scene,"m117-e22").damage==0,"Hidden dog keeps original cleared damage cache");need(!s.flag(44)&&erased,"CE31 finishes and erases picture39 during death/fade before hidden wait");}
        else if(hidden){revived=true;need(dog.motion.tile()==TilePosition{39,13}&&enemy(scene,"m117-e22").damage==0,"Burned dog revives at original tile with fresh damage");}
    }
    need(hidden&&revived&&corpseDamage>=100&&!s.flag(44)&&s.burnWaitRemaining(31)==0&&s.variable(308)==0&&erased,"CE31 completes despite death and regeneration");
    for(int n=0;n<120;++n){advance();need(enemy(scene,"m117-e22").damage==0,"Expired CE31 cannot damage regenerated dog");}
    need(s.variable(269)<30,"Burned dog regeneration never grants player quota");
    std::cerr<<"Public GL burn/direct kill, corpse protection, picture39 cleanup and fresh dog regeneration passed\n";
}
}
int main(int argc,char** argv){try{
need(argc==2||(argc==3&&std::string(argv[2])=="--burn-regeneration"),"usage final_guard package [--burn-regeneration]");const auto c=loadCombatContent(argv[1]);burnRegeneration(c);if(argc==3){return 0;}const auto& p=c.catalog.at("guard_second").at("runtime_profile");need(!c.pendingMapTickets.contains(117),"Delivered second guard opens in its stage and cumulative evacuation");need(p.at("attributes").at("attack")==32&&p.at("attributes").at("defense")==9&&p.at("attributes").at("agility")==30,"Actor3 equipment effective attributes");
{auto e=entry(c,116);for(int i=240;i<=269;++i){e.session.variables[i]=99;}e.session.flags[13]=e.session.flags[222]=false;GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e);for(int n=0;n<30000&&!(scene.mapId()==117&&s.flag(18));++n){scene.update(confirm());}need(scene.mapId()==117&&scene.companionHp()==150,"Natural116 intro enters real117 and Actor3");need(s.variable(247)==13&&s.variable(267)==0&&s.variable(269)==0,"Second intro clears counters and repeats original247=13");need(s.eventTimer().source==EventTimerSource::GuardFinal&&s.eventTimer().remainingTicks/60==120,"Original120sec timer and distinct source");need(s.exportState().party==std::vector<int>{1},"Encounter party remains transient");std::cerr<<"Natural116 intro, reset and timer passed\n";}
{auto e=entry(c);e.positions["m117-e6"]={10,9};e.positions["m117-e7"]={10,8};e.directions["m117-e6"]=Direction::Up;e.session.variables[261]=11;e.session.variables[89]=0;GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,[](int lo,int){return lo;});tick(scene,18);need(s.variable(47)==4&&s.variable(48)==4,"Bat writes source4x4 shared half");need(enemy(scene,"m117-e7").damage==22&&s.variable(269)==0,"Bat uses agility30 minusdef3 minus5 without halving or player quota");need(scene.companionHp()==150,"Actor3 independent health");}
{auto e=entry(c);e.positions["m117-e6"]={10,9};e.positions["m117-e7"]={10,8};e.session.variables[261]=9;e.session.variables[264]=1;GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e);scene.update({});need(s.variable(261)==11,"Mode9 falls through original approach-targetP11");}
{auto e=entry(c);e.session.flags[222]=false;e.positions["m117-e6"]={10,9};e.positions["m117-e7"]={10,8};e.session.variables[91]=2;GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,[](int lo,int hi){return lo==-5&&hi==3?0:lo==0&&hi==10?2:lo;});scene.update({});need(scene.companionHp()==144&&s.hp()==5000,"Actor3 hurt reads9/2 defense and does not hurt player");CombatScene rebuilt(c,s,e);need(rebuilt.companionHp()==150,"Rebuild restores only encounter-local Actor3 health");}
{auto e=entry(c);e.session.variables[261]=21;GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e);scene.update({});tick(scene,60);need(s.flag(222),"Recovery firstWait10 keeps invulnerability");tick(scene,60);need(s.flag(222),"Recovery secondWait10 keeps invulnerability");tick(scene,60);need(s.flag(222),"Recovery thirdWait10 keeps invulnerability");tick(scene,60);need(!s.flag(222),"Recovery fourthWait10 clears invulnerability");}
{auto e=entry(c);e.positions["m117-e6"]={10,9};e.positions["m117-e7"]={10,5};e.session.variables[261]=5;GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,[](int lo,int){return lo;});scene.update({});need(s.variable(247)==12&&s.variable(47)==3&&s.variable(48)==3,"Actor3 shot commits one ammo and source initial3x3");scene.update({});need(s.variable(47)==1&&s.variable(48)==1&&s.variable(269)==0,"Source projectile flight uses1x1 without player quota");}
std::cerr<<"Actor3 bat/hurt/mode9/recovery passed\n";
{auto e=entry(c);GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,[](int lo,int){return lo;});std::set<std::string> hit;scene.resolveAttack({body(scene,"m117-e22"),{0,0},AttackKind::Magnum,5},hit);need(enemy(scene,"m117-e22").damage>=100&&s.variable(269)==0,"Dog source player receiver never increments269");bool hidden=false,revived=false;int hiddenTicks=0;for(int n=0;n<3000&&!revived;++n){scene.update({});const auto dog=findCharacter(scene.world(),"m117-e22");if(!dog.active){hidden=true;++hiddenTicks;}else if(hidden){revived=true;need(dog.motion.tile()==TilePosition{39,13}&&enemy(scene,"m117-e22").damage==0,"Dog returns to original39,13 with independent reset");}}need(hidden&&revived&&hiddenTicks>=900,"Dog retains150-tenths hidden delay");need(s.variable(269)<30,"Dog regeneration cannot triggerquota");}
{auto e=entry(c);e.session.variables[269]=4;for(const auto& [n,r]:p.at("slots").items()){e.session.flags[r.at("dead_fact").get<int>()]=true;}GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,[](int lo,int){return lo;});tick(scene,899);need(s.variable(268)==0,"Second support waits original150tenths");s.setVariable(261,0);scene.update({});need(s.variable(268)==1&&findCharacter(scene.world(),"m117-e32").motion.tile()==TilePosition{16,13},"Support needs no first-guard rescue threshold and uses fixed16,13");}
std::cerr<<"Dog regen/quota and fixed support passed\n";
for(bool quota:{false,true}){auto e=entry(c);e.session.variables[269]=quota?30:7;e.session.variables[267]=2;GameSession s(c.items,e.session,c.facts);s.startEventTimer(EventTimerSource::GuardFinal,120,false,false);for(int i=0;i<7200;++i){s.tickEventTimer();}CombatScene scene(c,s,e);for(int n=0;n<5000&&!(scene.mapId()==116&&s.variable(394)==(quota?80:34));++n){scene.update(confirm());}need(scene.mapId()==116&&s.flag(341)&&s.variable(394)==(quota?80:34),"Timer/quota returns116 with original394 score");need(s.variable(267)==20&&s.variable(269)==(quota?60:14),"Score source mutates counters exactlyonce");need(s.exportState().party==std::vector<int>{1}&&!s.flag(1)&&!s.flag(18),"Actor cleanup leaves locked stable-party handoff");tick(scene,120);need(scene.foreground()==ForegroundKind::Content&&!scene.companionHp()&&s.variable(394)==(quota?80:34),"105tail waits locked without rescoring or NPCinstance");const auto& steps=c.catalog.at("flows").at(quota?"guard-second-end-2":"guard-second-end-1").at("steps");int remove=-1,ce5=-1,ce55=-1;for(int i=0;i<static_cast<int>(steps.size());++i){if(steps[i].at("kind")=="companion_party"){remove=i;}if(steps[i].at("kind")=="cancel_player_actions"){ce5=i;}if(steps[i].at("kind")=="room_reset"){ce55=i;}}need(quota?remove<ce5&&ce5<ce55:ce5<ce55&&ce55<remove,"Distinct original end-page cleanup order");}
{auto e=entry(c);GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e);s.setFlag(224,true);bool title=false;for(int n=0;n<1200&&!title;++n){scene.update(confirm());title=scene.takeApplicationRequest()==ApplicationMode::Title;}need(title&&s.hp()==5000&&!s.flag(341),"Dead Actor3 returns realTitle while playerHP positive, never success/Continue");}
std::cerr<<"Second guard natural entry, Actor3 rules, dog regeneration, support, both source endings/score and realTitle failure passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
