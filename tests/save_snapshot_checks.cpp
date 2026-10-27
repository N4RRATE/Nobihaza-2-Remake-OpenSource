#include "save_snapshot.h"
#include "enemy_host.h"
#include <iostream>
#include <stdexcept>
void require(bool v,const char* m){if(!v){throw std::runtime_error(m);}}
int main(int argc,char** argv){
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);auto e=c.entries.at("defeated-claimed");
 GameSession original(c.items,e.session);original.grant(60,1);original.setFlag(60,true);original.setVariable(388,4321);
 original.castSkill(c.skills.at(9));for(int n=0;n<60;++n){original.tickConditions();}
 auto state=original.exportState();GameSession copy(c.items,state);
 require(copy.count(60)==1&&copy.flag(60)&&copy.variable(388)==4321,"session export preserves named facts");
 require(copy.quickshotTicks()==840&&copy.tp()==25,"session export includes skill remaining time and cost");
 original.grant(60,1);require(copy.count(60)==1,"exported state is independent");
 auto start=e;start.map=16;start.playerTile={10,21};start.direction=Direction::Up;start.pages.clear();start.positions.clear();start.directions.clear();start.through.clear();
 c.entries["save-fixture"]=start;GameSession saving(c.items,start.session);saving.finishSave(7,5);saving.castSkill(c.skills.at(9));for(int n=0;n<60;++n){saving.tickConditions();}
 CombatScene room(c,saving,"save-fixture");auto snap=captureSave(c,room,{room.generation(),"m16-e5","BSAA 1F入口"},{});
 require(snap.summary.location=="BSAA 1F入口"&&snap.summary.savedAtUnix>0&&snap.summary.faceIndex==15&&snap.session.actorName=="大雄","slot summary does not rename actor");
 require(saving.saveCount()==7&&saving.variable(399)==5,"capture does not commit live progress");
 auto bytes=encodeSave(c,snap);auto decoded=decodeSave(c,bytes);auto candidate=prepareRestore(c,decoded);
 require(candidate.session->saveCount()==8&&candidate.session->variable(399)==6,"load resumes CE56 once from disk base");
 require(candidate.session->quickshotTicks()==900&&candidate.session->tp()==25,"load recreates quickshot wait without cost");
 require(candidate.scene->mapId()==16&&candidate.scene->generation()!=room.generation()&&candidate.scene->world().player().motion.tile()==TilePosition{10,21},"restore has new runtime identity and saved location");
 finishSaveContinuation(saving,snap);finishSaveContinuation(saving,snap);
 require(saving.saveCount()==8&&saving.variable(399)==6&&saving.quickshotTicks()==840,"current continuation is idempotent and preserves remaining time");
 auto second=prepareRestore(c,decoded);require(second.session->variable(399)==6,"repeated load does not accumulate statistics");
 int rolls=0;auto injected=prepareRestore(c,decoded,{},[&](int lo,int){++rolls;return lo;});
 require(EnemyHost(*injected.scene,"m16-e2").random(3,8)==3&&rolls==1,"restored Scene retains caller random provider");
 auto reject=[&](std::string text){bool failed=false;try{(void)decodeSave(c,text);}catch(const std::exception&){failed=true;}require(failed,"invalid input must be rejected");};
 auto json=nlohmann::json::parse(bytes);
 auto bad=json;bad["session"]["inventory"].push_back({60,-1});reject(bad.dump());
 bad=json;bad["session"]["inventory"].push_back(bad["session"]["inventory"][0]);reject(bad.dump());
 bad=json;bad["session"]["hp"]=70.5;reject(bad.dump());
 bad=json;bad["session"]["magazines"]={{11,16}};reject(bad.dump());
 bad=json;bad["scene"]["tile"]={-1,21};reject(bad.dump());
 bad=json;bad["after_save"]["statistic_before"]=6;reject(bad.dump());
 bad=json;bad["session"]["world_flags"].push_back({"m17-e7.collected",true});reject(bad.dump());
 bad=json;bad["version"]=2;reject(bad.dump());bad=json;bad["content"]="future";reject(bad.dump());
 reject("{\"version\":1,"+bytes.substr(1));reject(std::string(8*1024*1024+1,' '));reject(std::string(33,'[')+"0"+std::string(33,']'));
 bad=json;bad["scene"]["objects"].push_back(bad["scene"]["objects"][0]);reject(bad.dump());
 bad=json;bad["presentation"]["music"]["resource"]="missing";reject(bad.dump());
 bool failed=false;try{(void)prepareRestore(c,snap,[](const SceneDefinition&){throw std::runtime_error("texture failure");});}catch(const std::exception&){failed=true;}
 require(failed&&saving.saveCount()==8&&room.generation()!=candidate.scene->generation(),"failed candidate leaves live session and scene intact");
 auto varied=c;auto node=varied.objects.extract("m16-e5");node.key()="test-crow";node.mapped().id="test-crow";node.mapped().tile={3,8};for(auto& v:node.mapped().variants){v.definition.id="test-crow";v.definition.tileX=3;v.definition.tileY=8;}varied.objects.insert(std::move(node));varied.catalog["save_points"][0]["object"]="test-crow";
 auto changed=snap;changed.continuation.point="test-crow";changed.scene.playerTile={3,9};changed.scene.positions.erase("m16-e5");changed.scene.directions.erase("m16-e5");changed.scene.positions["test-crow"]={3,8};changed.scene.directions["test-crow"]=Direction::Down;
 auto variation=prepareRestore(varied,decodeSave(varied,encodeSave(varied,changed)));require(variation.scene->world().player().motion.tile()==TilePosition{3,9},"save point identity and position come from content");
 std::cout<<"Save snapshot checks passed\n";
}
