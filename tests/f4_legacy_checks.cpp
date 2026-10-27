#include "title_navigation.h"
#include "game_application.h"
#include <fstream>
#include <iostream>
void need(bool v,const char* s){if(!v)throw std::runtime_error(s);}
InputFrame key(InputAction a=InputAction::Confirm){InputFrame f;f.buttons[static_cast<int>(a)].pressed=true;return f;}
std::string bytes(const std::filesystem::path& p){std::ifstream in(p,std::ios::binary);if(!in)throw std::runtime_error("Missing isolated source save");return {std::istreambuf_iterator<char>(in),{}};}
int main(int argc,char**argv){try{
 if(argc!=4)return 2;auto c=loadCombatContent(argv[1]);auto source=loadCombatContent(c.directory/"f3-source-contract.json");const std::filesystem::path dir=argv[2];std::string mode=argv[3];
 for(int slot=1;slot<=2;++slot){
  if(mode=="write"){
   auto e=source.entries.at("defeated-claimed");e.map=16;e.playerTile={10,21};e.direction=Direction::Up;
   e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.saveCount=7;e.session.variables[399]=5;e.session.variables[388]=slot==1?1234:6789;
   e.session.flags[59]=true;e.session.flags[60]=slot==2;e.session.inventory[60]=slot==1?1:0;e.session.magazines[11]=slot==1?4:9;
   e.session.hp=slot==1?63:87;source.entries["64-legacy-fixture"]=e;
   GameApplication old(source,"64-legacy-fixture");old.configureSaveDirectory(dir);finishTitleOpening(old);old.update(key());for(int n=0;n<200;++n)old.update({});old.update(key());if(slot==2)old.update(key(InputAction::Down));old.update(key());
   need(old.mode()==ApplicationMode::Playing&&old.session().saveCount()==8,"Source F3 game writes lawful original slot");
  }else if(mode=="load"){
   const auto raw=bytes(dir/(slot==1?"slot-01.json":"slot-02.json"));const auto original=decodeSave(source,raw);GameSession expected(source.items,original.session,source.facts);
   GameApplication next(c,"title");next.configureSaveDirectory(dir);finishTitleOpening(next);next.update(key());if(slot==2)next.update(key(InputAction::Down));next.update(key());
   need(next.hasGame()&&next.mode()==ApplicationMode::Playing&&next.mapId()==original.scene.map,"Independent F4 title Load commits old F3 candidate");
   const auto restored=next.session().exportState();
   need(restored.inventory==original.session.inventory&&restored.mainWeapon==original.session.mainWeapon&&restored.armor==original.session.armor&&restored.hp==original.session.hp&&restored.tp==original.session.tp,"Old inventory equipment and vitals remain unchanged");
   for(const auto& [id,n]:original.session.magazines)need(next.session().magazine(id)==n,"Old magazines must not be reseeded");
   need(next.session().magazine(14)==5&&next.session().magazine(21)==8&&!next.session().flag(66)&&!next.session().flag(84),"Only reviewed F4 seeds and initial facts are added");
   need(next.session().variable(388)==expected.variable(388)&&next.session().flag(60)==expected.flag(60)&&next.session().saveCount()==expected.saveCount()&&next.session().variable(399)==original.continuation.statisticBefore+1,"Source password archive progress and after-save survive migration");
   need(next.world().player().motion.tile()==original.scene.playerTile&&next.world().player().motion.facing()==original.scene.direction,"Load keeps original saved placement");
   need(bytes(dir/(slot==1?"slot-01.json":"slot-02.json"))==raw,"Title Load never writes original save");
  }else return 2;
 }
 std::cout<<"F3 to F4 "<<mode<<" two independent progress slots passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
