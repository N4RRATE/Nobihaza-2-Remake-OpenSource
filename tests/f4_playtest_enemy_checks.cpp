#include "combat_scene.h"
#include "enemy_controller.h"
#include "save_snapshot.h"
#include "combat_presentation.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool ok,const std::string& message){if(!ok){throw std::runtime_error(message);}}
void tick(CombatScene& scene,int n){while(n--){scene.update({});}}
CombatEntry entry(const CombatContent& c,int map){auto e=c.entries.at("defeated-claimed");e.map=map;e.playerTile={1,1};e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.flags[1]=true;e.session.flags[18]=true;e.session.baseAttack=300;return e;}
EnemyView enemy(const CombatScene& scene,const std::string& id){const auto states=scene.enemyStates();const auto found=std::find_if(states.begin(),states.end(),[&](const auto& state){return state.id==id;});require(found!=states.end(),id+" visible zombie has no combat controller");return *found;}
AttackResolution hit(CombatScene& scene,const std::string& id,AttackKind kind=AttackKind::Handgun){auto p=findCharacter(scene.world(),id).motion.position();std::set<std::string> resolved;return scene.resolveAttack({(p.x+.5)*16,(p.y+.5)*16},true,0,false,resolved,kind);}
void durableRoundtrip(const CombatContent& c,GameSession& s,const CombatEntry& target,const std::string& id,int flag){
 auto safe=entry(c,22);safe.playerTile={10,5};safe.direction=Direction::Up;
 CombatScene crow(c,s,safe);const auto saved=captureSave(c,crow,{crow.generation(),"m22-e2","fixture"},{});
 const auto decoded=decodeSave(c,encodeSave(c,saved));auto restored=prepareRestore(c,decoded);
 require(restored.session->flag(flag),"Death fact survives strict save/restore at a legal crow point");
 CombatScene revisit(c,*restored.session,target);require(!findCharacter(revisit.world(),id).active,"Dead enemy stays absent after load and revisit");
}
}
int main(int argc,char** argv){
 if(argc<2){return 2;}
 try{
  const auto c=loadCombatContent(argv[1]);const std::string mode=argc>2?argv[2]:"both";
  if(mode=="all-lickers"||mode=="all-lickers-present"){
   sf::Font font;
   std::unique_ptr<CombatPresentation> presentation;
   if(mode=="all-lickers-present"){
    require(font.openFromFile(c.maps.begin()->second.fontPath),"Cannot load presentation font");
    presentation=std::make_unique<CombatPresentation>(c,font);
   }
   int checked=0,failed=0;
   for(const auto& [id,definition]:c.expansion.enemies){
    if(definition.behavior!=EnemyBehaviorKind::Licker){continue;}
    ++checked;
    const auto& rules=definition.rules.is_null()?c.expansion.behaviorDefinitions.at("licker"):definition.rules;
    const auto& profile=rules.at("runtime_profile");
    for(const auto kind:{AttackKind::Magnum,AttackKind::Shotgun,AttackKind::GlNormal}){for(bool suppressed:{false,true}){try{
     auto e=entry(c,definition.map);e.session.hp=e.session.maxHp=9999;e.session.flags[13]=true;
     for(const auto& [other,d]:c.expansion.enemies){e.session.flags[d.deadFlag]=other!=id;}
     if(definition.activationFlag){e.session.flags[*definition.activationFlag]=true;}
     if(profile.contains("ending")){e.session.flags[profile.at("ending").at("conditional_cue").at("unless_fact")]=suppressed;}
     e.session.variables[91]=0;
     GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,[](int lo,int){return lo;});scene.update({});
     auto initial=scene.takePresentation();if(presentation){presentation->update(initial);}
     require(hit(scene,id,kind).processed==1,id+" must receive actual lethal attack");
     require(!s.flag(definition.deadFlag),id+" cannot commit before death presentation");
     std::vector<PresentationEvent> events;
     const auto consume=[&]{auto next=scene.takePresentation();events.insert(events.end(),next.begin(),next.end());if(presentation){presentation->update(next);}};
     consume();
     if(profile.contains("ending")){
      const std::string flash=profile.at("ending").at("conditional_cue").at("cue");
      const bool shown=std::any_of(events.begin(),events.end(),[&](const auto& event){return event.kind==PresentationEvent::Kind::Cue&&event.id==flash&&event.actor==id;});
      require(shown!=suppressed,id+" death flash must respect its original per-instance suppression fact");
      if(presentation){require(presentation->characterFlashes().contains(id)!=suppressed,id+" flash must reach actual presentation");}
     }
     for(int n=0;n<20;++n){scene.update({});consume();}
     require(!s.flag(definition.deadFlag),id+" original Wait5 cannot commit early");
     for(int n=0;n<50;++n){scene.update({});consume();}
     require(s.flag(definition.deadFlag)&&enemy(scene,id).mode==EnemyMode::Corpse,id+" must commit death and stop AI");
     const int page=profile.at("pages").at("ending");
     const std::string soundKey=profile.contains("ending")&&!profile.contains("cue_commands")?id+"-p"+std::to_string(page)+"-c5":profile.at("cues").at("p"+std::to_string(page)+"-c6").get<std::string>();
     const auto sound=readSound(c.cue(soundKey).at("sound"));
     require(std::any_of(events.begin(),events.end(),[&](const auto& event){return event.kind==PresentationEvent::Kind::Sound&&event.sound==sound;}),id+" original death sound must be presented");
     CombatScene revisit(c,s,e);require(!findCharacter(revisit.world(),id).active,id+" remains dead on revisit");
     const auto state=s.exportState();scene.deactivate();tick(scene,50);require(s.exportState()==state,id+" retired scene must stop effects");
     if(kind==AttackKind::Magnum&&!suppressed){
      GameSession interrupted(c.items,e.session,c.facts);CombatScene retired(c,interrupted,e,[](int lo,int){return lo;});retired.update({});
      require(hit(retired,id,kind).processed==1,id+" interrupted death must receive hit");retired.deactivate();
      const auto before=interrupted.exportState();tick(retired,100);require(interrupted.exportState()==before&&!interrupted.flag(definition.deadFlag),id+" canceled ending must not commit from retired scene");
      CombatScene resumed(c,interrupted,e);require(findCharacter(resumed.world(),id).active,id+" unfinished death remains live after revisit");
     }
     std::cout<<id<<" death passed weapon="<<static_cast<int>(kind)<<" suppressed="<<suppressed<<'\n';
    }catch(const std::exception& error){++failed;std::cerr<<id<<": "<<error.what()<<'\n';}}}
   }
   require(checked>0&&failed==0,std::to_string(failed)+" / "+std::to_string(checked)+" licker deaths failed");
   if(c.expansion.enemies.contains("m78-e4")){
    auto broken=c;broken.expansion.enemies.at("m78-e4").rules["runtime_profile"]["cues"]["p26-c6"]="missing-death-media";
    auto e=entry(broken,78);e.session.flags[13]=true;e.session.flags[251]=false;
    GameSession s(broken.items,e.session,broken.facts);CombatScene scene(broken,s,e,[](int lo,int){return lo;});scene.update({});
    require(hit(scene,"m78-e4",AttackKind::Magnum).processed==1,"Missing-resource fixture must receive lethal hit");
    bool rejected=false;try{tick(scene,100);}catch(const nlohmann::json::out_of_range&){rejected=true;}
    require(rejected&&!s.flag(251),"Missing death media must fail explicitly before death commit");
   }
   std::cout<<"All "<<checked<<" admitted lickers: death variants, cancellation and missing-media rejection passed\n";
   return 0;
  }
  if(mode!="licker"){
   auto e=entry(c,59);GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,[](int lo,int){return lo;});
   for(const auto* id:{"m59-e3","m59-e5"}){(void)enemy(scene,id);require(hit(scene,id).processed==1,std::string(id)+" cannot receive an actual attack");}
   tick(scene,100);require(s.flag(167)&&s.flag(168),"Both hot-water zombie deaths commit independently");
   CombatScene returned(c,s,e);require(!findCharacter(returned.world(),"m59-e3").active&&!findCharacter(returned.world(),"m59-e5").active,"Both dead zombies remain absent on revisit");
   durableRoundtrip(c,s,e,"m59-e3",167);durableRoundtrip(c,s,e,"m59-e5",168);
   for(int event:{3,5}){
    auto attack=entry(c,59);attack.session.baseAttack=11;attack.session.baseDefense=1;
    attack.session.flags[event==3?168:167]=true;attack.session.flags[event==3?167:168]=false;
    const std::string id="m59-e"+std::to_string(event);const auto tile=c.objects.at(id).tile;
    attack.playerTile={tile.x-1,tile.y};attack.directions[id]=Direction::Left;
    GameSession player(c.items,attack.session,c.facts);CombatScene bite(c,player,attack,[](int lo,int){return lo;});const int hp=player.hp();
    tick(bite,300);require(player.hp()<hp,id+" must actually attack the adjacent player");
    bite.deactivate();const auto state=player.exportState();tick(bite,50);require(player.exportState()==state,"Retired scene must stop enemy effects");
   }
   auto fall=entry(c,59);fall.session.flags[167]=true;fall.session.baseAttack=64;fall.session.variables[35]=0;fall.session.flags[47]=false;
   {GameSession stats(c.items,fall.session,c.facts);fall.session.baseAttack+=64-stats.attack();}
   GameSession fallen(c.items,fall.session,c.facts);CombatScene down(c,fallen,fall,[](int lo,int){return lo;});
   // Source Actor20 defense3; low-selector adjustment1 produces60 damage, the original second zombie's threshold.
   const auto fallenHit=hit(down,"m59-e5");require(fallenHit.damage==60,"M59 E5 expected60 actual "+std::to_string(fallenHit.damage)+" attack "+std::to_string(fallen.attack())+" defense "+c.enemies.at("m59-e5").rules.at("defense").dump());tick(down,2);
   require(enemy(down,"m59-e5").mode==EnemyMode::Fallen,"Second zombie enters its original temporary fallen state");tick(down,310);
   require(!fallen.flag(168)&&enemy(down,"m59-e5").mode!=EnemyMode::Fallen,"Original five-second fall recovers without committing death");
  }
  if(mode!="zombies"){
   for(const auto kind:{AttackKind::Magnum,AttackKind::Shotgun}){for(bool rage:{false,true}){
    auto e=entry(c,26);e.session.flags[166]=true;e.session.flags[187]=false;e.session.flags[37]=rage;e.session.variables[91]=0;
    GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,[](int lo,int){return lo;});scene.update({});scene.takePresentation();
    require(hit(scene,"m26-e4",kind).processed==1,"M26 licker must receive actual lethal attack");require(!s.flag(187),"Death fact waits for original presentation");
    tick(scene,20);require(!s.flag(187),"M26 original Wait5 cannot commit early");tick(scene,50);require(s.flag(187),"M26 licker death must finish and persist");
    const auto events=scene.takePresentation();const auto sound=readSound(c.cue("m26-e4-p25-c5").at("sound"));
    require(std::any_of(events.begin(),events.end(),[&](const auto& e){return e.kind==PresentationEvent::Kind::Sound&&e.sound==sound;}),"Source death sound must reach presentation");
    const bool flash=std::any_of(events.begin(),events.end(),[](const auto& e){return e.kind==PresentationEvent::Kind::Cue&&e.id=="m26-e4-p25-c2"&&e.actor=="m26-e4";});
    require(flash!=rage,"Death flash retains source S37 condition and actor binding");
    CombatScene returned(c,s,e);require(!findCharacter(returned.world(),"m26-e4").active,"Dead M26 licker must stay absent on revisit");
    durableRoundtrip(c,s,e,"m26-e4",187);
   }}
  }
  std::cout<<"F4 playtest enemies passed: "<<mode<<'\n';
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
