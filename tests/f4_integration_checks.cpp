#include "title_navigation.h"
#include "game_application.h"
#include <iostream>
#include <stdexcept>
void need(bool b,const char* s){if(!b)throw std::runtime_error(s);}
InputFrame key(InputAction a=InputAction::Confirm){InputFrame f;f.buttons[static_cast<int>(a)].pressed=true;return f;}
// Independent legal-save combinations, not a replacement for the natural route.
CombatEntry sample(const CombatContent& c,int map,int index){
 auto e=c.entries.at("defeated-claimed");e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();
 e.map=map;const auto& crow=c.objects.at(map==16?"m16-e5":"m22-e2");e.playerTile=adjacentTile(crow.tile,Direction::Down);e.direction=Direction::Up;
 auto& s=e.session;s.flags[58]=true;s.saveCount=7;s.variables[399]=5;s.variables[388]=2345;s.inventory[21]=1;s.magazines[21]=3;
 if(index>=1){s.flags[66]=true;s.inventory[61]=1;}
 if(index>=2){s.flags[67]=true;if(index%2)s.inventory[61]=0;}
 if(index>=4){s.flags[68]=true;s.inventory[62]=1;}
 if(index>=6){s.flags[71]=s.flags[72]=s.flags[61]=s.flags[57]=true;}
 if(index>=8){for(int id:{73,75,76,77,78,79,80,81,82,84})s.flags[id]=true;s.skills.insert(3);s.skills.erase(4);s.inventory[51]=1;}
 if(index==9||index==10){s.flags[74]=true;s.inventory[14]=1;s.magazines[14]=2;}
 if(index==11){s.quickshotTicks=400;s.flags[47]=true;s.tp=15;}
 if(index==9){s.mainWeapon=14;s.inventory[14]=0;s.accessory=51;s.inventory[51]=0;}
 if(index==10){s.mainWeapon=21;s.inventory[21]=0;}
 return e;
}
void openSave(GameApplication& g,int slot){
 g.update(key());need(g.mode()==ApplicationMode::Saving,"Reviewed crow enters save flow");
 for(int n=0;n<200;++n)g.update({});g.update(key());need(g.fileMenuView().has_value(),"Crow confirmation opens slots");
 for(int n=1;n<slot;++n)g.update(key(InputAction::Down));g.update(key());
 need(g.mode()==ApplicationMode::Playing,"Save commits and returns");
}
int main(int argc,char**argv){try{
 if(argc!=4)return 2;auto c=loadCombatContent(argv[1]);const std::filesystem::path root=argv[2];const std::string mode=argv[3];
 for(int map:{16,22})for(int i=0;i<12;++i){
  const auto dir=root/std::to_string(map);auto e=sample(c,map,i);c.entries["64-save-combination-fixture"]=e;
  if(mode=="write"){
   GameApplication game(c,"64-save-combination-fixture");game.configureSaveDirectory(dir);finishTitleOpening(game);openSave(game,i+1);
   need(game.session().saveCount()==8&&game.session().variable(399)==6,"Current save resumes once at 8/6");
   auto disk=SaveFiles(c,dir).read(i+1);need(disk.session.saveCount==8&&disk.session.variables.at(399)==5,"Disk is exactly 8/5");
   need(disk.session.magazines.at(21)==3&&disk.session.magazines.at(14)==(i==9||i==10?2:5),"Independent weapon magazines retained");
   if(i>=8){auto invalid=disk;invalid.session.worldFlags[c.facts.switches.at(72)]=false;invalid.session.worldFlags[c.facts.switches.at(71)]=false;
    bool rejected=false;try{encodeSave(c,invalid);}catch(const std::exception&){rejected=true;}need(rejected,"Meeting history cannot precede ventilation access");}
  }else if(mode=="load"){
   GameApplication game(c,"title");game.configureSaveDirectory(dir);finishTitleOpening(game);game.update(key());
   need(game.mode()==ApplicationMode::Loading,"Fresh process uses title Load");for(int n=0;n<i;++n)game.update(key(InputAction::Down));game.update(key());
   need(game.hasGame()&&game.mapId()==map&&game.mode()==ApplicationMode::Playing,"Load commits correct save room");
   const auto& state=game.session();need(state.saveCount()==8&&state.variable(399)==6&&state.variable(388)==2345,"Counts and generated password survive");
   need(state.count(61)==(i>=1&&(i<2||i%2==0)?1:0)&&state.count(62)==(i>=4?1:0)&&state.count(63)==0,"Key and tool forms survive");
   need(state.flag(66)==(i>=1)&&state.flag(67)==(i>=2)&&state.flag(68)==(i>=4)&&state.flag(72)==(i>=6),"Independent tool facts survive");
   need(state.flag(73)==(i>=8)&&state.flag(84)==(i>=8)&&state.knowsSkill(3)==(i>=8)&&!state.knowsSkill(4),"Meeting and safe-room skill remain consistent");
   need(state.flag(74)==(i==9||i==10)&&state.count(14)==(i==10?1:0),"Optional shotgun remains optional after Load");
   need(state.equipped(1)==(i==9?14:i==10?21:e.session.mainWeapon)&&state.equipped(4)==(i==9?51:0),"Weapon and pouch equipment survive independently of inventory");
   need(state.magazine(21)==3&&state.magazine(14)==(i==9||i==10?2:5),"Load does not reseed magazines");
   if(i==11)need(state.quickshotTicks()==900&&state.tp()==15&&state.flag(47),"Quickshot rebuilds wait without charging TP or consuming precision");
   const auto disk=SaveFiles(c,dir).read(i+1);std::string error;need(game.restore(disk,error)&&game.session().variable(399)==6,"Repeated Load never increments again");
   const auto before=game.session().exportState();const auto generation=game.generation();
   game.configureRestorePresentation([](const SceneDefinition&,const SavedPresentation&){throw std::runtime_error("64 missing texture fixture");});
   need(!game.restore(disk,error)&&game.generation()==generation&&game.session().exportState()==before,"Candidate failure preserves complete old session");
   game.configureRestorePresentation({});need(game.newGame(error),"New NORMAL session starts after prior progress");
   need(!game.session().flag(66)&&!game.session().flag(68)&&!game.session().flag(72)&&!game.session().flag(73)&&!game.session().flag(84)&&!game.session().knowsSkill(3)&&game.session().count(62)==0&&game.session().count(14)==0&&game.session().count(21)==0,"New Game clears prior F4 progress and skills");
   need(game.session().magazine(14)==0&&game.session().magazine(21)==0,"Fresh Map2 has not executed original magazine seed commands yet");
  }else return 2;
 }
 std::cout<<"F4 integration "<<mode<<": 24 isolated save combinations passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
