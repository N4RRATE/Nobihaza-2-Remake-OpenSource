#include "g3_hunters_driver.h"
#include "g2_natural_driver.h"
#include "content_hash.h"
namespace {
template<class F>void rejects(F fn){bool caught=false;try{fn();}catch(const std::exception&){caught=true;}need(caught,"G3-06 invalid candidate accepted");}
CombatEntry entry(const CombatContent& c,const SessionEntry& base,int map=185,TilePosition tile={28,5}){
 auto e=c.entries.at("post-pickup");e.map=map;e.playerTile=tile;e.direction=Direction::Down;e.session=base;e.positions.clear();e.pages.clear();e.directions.clear();e.through.clear();e.session.flags[1]=e.session.flags[18]=true;e.session.flags[13]=true;return e;
}
EnemyView enemy(const CombatScene& scene,const std::string& id){for(const auto& e:scene.enemyStates()){if(e.id==id){return e;}}throw std::runtime_error("Missing B1 enemy");}
void rules(const CombatContent& c,const SaveSnapshot& saved,const std::string& raw){
 need(c.maps.at(185).width==34&&c.maps.at(184).width==20&&c.enemies.contains("m187-e3"),"B1 actual geometry/zombie absent");
 auto prior=decodeSave(*c.legacySource,raw), adapted=decodeSave(c,raw);auto state=adapted.session;
 for(int fact:{403,404,405,406,509,566}){need(!state.worldFlags.at(c.facts.switches.at(fact)),"06 adaptation invented progress");state.worldFlags.erase(c.facts.switches.at(fact));}
 need(state==prior.session&&adapted.scene==prior.scene&&adapted.continuation==prior.continuation,"06 adapter changed original stats/ammo/scene");
 for(bool unlocked:{false,true}){
  auto e=entry(c,saved.session);e.session.worldFlags[c.facts.switches.at(462)]=unlocked;GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,safe);scene.update({});
  for(const auto& id:{"m185-e2","m185-e10","m185-e14"}){
   const auto* p=c.activeVariant(c.objects.at(id),s);need(p&&p->page==(unlocked?10:8),"462 hide/activity priority wrong");const auto a=findCharacter(scene.world(),id);need((a.layer==1)==unlocked&&a.spriteId.has_value()==unlocked,"462 render/collision wrong");
   auto receiver=makeEnemyBehavior(scene,c.enemies.at(id));auto contact=receiver->receiveAttack({{}, {},AttackKind::De,5,0,false,1,AttackPhase::ResolvedContact});need(contact.processed==int(unlocked),"Hidden Hunter receiver admitted");
  }
  for(const auto& id:{"m185-e6","m185-e7","m185-e8","m185-e9","m185-e11","m185-e17"}){need(findCharacter(scene.world(),id).layer==(unlocked?0:1),"Isolation gate collision wrong");}
 }
 {
  auto e=entry(c,saved.session);e.session.flags[18]=false;GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,safe);scene.update({});for(const auto& id:{"m185-e2","m185-e10","m185-e14"}){need(c.activeVariant(c.objects.at(id),s)->page==9,"462 visible P9 before activity lost");}
  s.setFlag(18,true);scene.update({});for(int slot=1;slot<=3;++slot){const std::string id=slot==1?"m185-e2":slot==2?"m185-e10":"m185-e14";const auto& blood=c.enemies.at(id).rules.at("blood");s.setFlag(20+2*slot,true);s.setVariable(81+14*(slot-1)+2,60*slot);s.setVariable(81+14*(slot-1)+3,80);EnemyHost host(scene,id);host.advanceBlood(blood,slot);need(s.bloodPictureProgress(14+slot).type==slot,"Three CE15-17 blood identities mixed");}
  rejects([&]{s.bloodPictureProgress(18);});
 }
 for(int slot=1;slot<=3;++slot){
  const std::string id=slot==1?"m185-e2":slot==2?"m185-e10":"m185-e14";auto e=entry(c,saved.session);GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,safe);scene.update({});auto receiver=makeEnemyBehavior(scene,c.enemies.at(id));s.setVariable(35,2);s.setFlag(47,true);auto contact=receiver->receiveAttack({{}, {},AttackKind::Grenade,7,0,false,1,AttackPhase::ResolvedContact});need(contact.processed&&contact.damage==0&&s.variable(35)==2&&s.flag(47),"B1 receiver7 substituted CE11");
  for(int n=0;n<350&&!s.flag(c.enemies.at(id).deadFlag);++n){scene.update({});scene.takePresentation();}need(s.flag(c.enemies.at(id).deadFlag)&&findCharacter(scene.world(),id).motion.tile()==TilePosition{0,0},"B1 stable death/parking failed");
 }
 {
  auto e=entry(c,saved.session);GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,safe);scene.update({});const auto& object=c.objects.at("m185-e14");s.setFlag(403,true);s.setVariable(117,1600);s.setVariable(103,0);need(c.activeVariant(object,s)->page==24,"Third corpse substituted own117");s.setVariable(103,1600);need(c.activeVariant(object,s)->page==25,"Third corpse lost second103");s.setFlag(403,false);s.setVariable(119,0);s.setVariable(105,4);need(c.activeVariant(c.objects.at("m185-e12"),s)->page==6,"Third manager substituted own119");
 }
 {
  auto e=entry(c,saved.session);GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,safe);scene.update({});auto receiver=makeEnemyBehavior(scene,c.enemies.at("m185-e14"));need(receiver->activateState(10),"Third fatal action absent");bool show=false,erase=false;
  for(int n=0;n<140;++n){scene.update({});for(const auto& ev:scene.takePresentation()){if(ev.kind!=PresentationEvent::Kind::Cue||!ev.id.starts_with("m185-e14-p17-")){continue;}const auto& cue=c.cue(ev.id);show|=cue.at("kind")=="show_picture"&&cue.at("picture_id")==3&&cue.at("effect_power_original")==60;erase|=cue.at("kind")=="erase_picture"&&cue.at("picture_id")==2;}}
  need(show&&erase,"Third picture3/effect60/source erase2 lost");scene.deactivate();s.resetRoomFacts();const auto before=s.exportState();for(int n=0;n<100;++n){scene.update({});}need(s.exportState()==before,"Retired hunter wrote new Scene state");
 }
 const auto& third=c.enemies.at("m185-e14").rules;for(int canonical:{14,15,16}){const int native=third.at("source_pages").at(std::to_string(canonical));const auto& vs=c.objects.at("m185-e14").variants;const auto it=std::find_if(vs.begin(),vs.end(),[&](const auto& v){return v.page==native;});need(it!=vs.end(),"Missing native action page");const auto& route=it->autonomousRoute;need(std::any_of(route.steps.begin(),route.steps.end(),[](const auto& step){return step.kind=="graphic";}),"Hunter intermediate charset flattened");if(canonical==14){need(std::any_of(route.steps.begin(),route.steps.end(),[](const auto& step){return step.kind=="through_on";})&&std::none_of(route.steps.begin(),route.steps.end(),[](const auto& step){return step.kind=="begin_jump";}),"Through36 became Jump24");}}
 for(int example=0;example<10;++example){auto bad=saved;if(example<6){bad.session.worldFlags.erase(c.facts.switches.at(std::array<int,6>{403,404,405,406,509,566}[example]));}else if(example==6){bad.session.inventory[111]=1;}else if(example==7){bad.session.worldFlags[c.facts.switches.at(509)]=true;}else{bad.session.worldFlags[c.facts.switches.at(462)]=false;bad.session.worldFlags[c.facts.switches.at(example==8?566:406)]=true;}rejects([&]{validateSave(c,bad);});}
 auto native=saved;for(int fact:{403,404,405,406,509,566}){native.session.worldFlags[c.facts.switches.at(fact)]=true;}native.session.inventory[111]=1;auto restored=prepareRestore(c,decodeSave(c,encodeSave(c,native)));need(restored.session->flag(509)&&restored.session->count(111)==1&&restored.session->flag(403),"06 engineering snapshot did not restore facts/card");auto e=entry(c,restored.session->exportState());CombatScene rebuilt(c,*restored.session,e,safe);rebuilt.update({});for(const auto& id:{"m185-e2","m185-e10","m185-e14"}){need(!findCharacter(rebuilt.world(),id).active,"Persisted Hunter revived on reentry");}
 std::cout<<"06 focused fixed05 adaptation/462/three receivers/stable deaths/cross-slot/picture60-through/retirement/10bad/native snapshot PASS\n";
}
using g3hunters::natural;

}
int main(int argc,char** argv){try{std::cout.setf(std::ios::unitbuf);need(argc==5,"06 package real58 isolated-root rules|natural|check");auto c=loadCombatContent(argv[1]);auto raw=read(argv[2]);auto saved=decodeSave(c,raw);need(c.compatibility=="post-f5-g3-normal-6","Wrong06 identity");std::string mode=argv[4];if(mode=="check"){std::cout<<"06 load/strict05 native adaptation PASS\n";return 0;}if(mode=="rules"){rules(c,saved,raw);return 0;}need(mode=="natural","Unknown06 mode");auto root=std::filesystem::path(argv[3]);std::filesystem::create_directories(root);std::ofstream(root/"slot-01.json")<<raw;G2NaturalDriver r(c,root,"public-inputs-natural.jsonl");r.game.configureSaveDirectory(root);r.load();natural(r);std::ofstream(root/"boundary-session.json")<<Json{{"kind","live06 at07 boundary; not a save"},{"map",r.game.mapId()},{"steps",r.ticks},{"hp",r.game.session().hp()},{"inventory111",r.game.session().count(111)},{"maps",r.maps},{"flags",r.game.session().exportState().worldFlags}}.dump(2)<<'\n';return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
