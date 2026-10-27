#include "title_navigation.h"
#include "game_application.h"
#include <iostream>
#include <stdexcept>

void need(bool b,const char* why){if(!b){throw std::runtime_error(why);}}
InputFrame key(InputAction a=InputAction::Confirm){InputFrame f;f.buttons[static_cast<int>(a)].pressed=true;return f;}
// Legal-save state combinations, not a claim of 30 naturally completed playthroughs.
CombatEntry sample(const CombatContent& c,int map,int index){
 auto e=c.entries.at("defeated-claimed");e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();
 e.map=map;const auto& crow=c.objects.at(map==16?"m16-e5":"m22-e2");e.playerTile=adjacentTile(crow.tile,Direction::Down);e.direction=Direction::Up;
 auto& s=e.session;s.flags[58]=true;s.saveCount=7;s.variables[399]=5;s.variables[388]=2345;
 for(int id:{66,67,68,71,72,61,57,73,75,76,77,78,79,80,81,82,84}){s.flags[id]=true;}s.inventory[62]=1;s.skills.insert(3);s.skills.erase(4);
 if(index>=1){s.flags[87]=s.flags[88]=true;s.inventory[66]=1;}
 if(index>=2){s.flags[93]=true;s.inventory[66]=0;}
 if(index>=3){s.flags[145]=true;s.inventory[68]=1;}
 if(index>=4){for(int id:{141,142,143}){s.flags[id]=true;}}
 if(index==5||index==6||index==7||index==11||index>=12){s.flags[144]=true;s.inventory[70]=1;}
 if(index==6||index==11){s.flags[273]=true;}
 if(index==7){s.flags[272]=s.flags[147]=true;s.inventory[70]=0;}
 if(index>=8){s.flags[316]=true;s.inventory[87]=1;}
 if(index==9||index==12||index==13){s.flags[338]=true;}
 if(index==10||index==14){s.flags[337]=true;}
 if(index==9||index==10||index>=11){s.flags[318]=true;s.inventory[87]=0;s.inventory[91]=1;}
 if(index==10||index==13||index==14){s.flags[314]=true;s.inventory[91]=0;}
 if(index==6||index==7||index>=9){s.flags[274]=s.flags[213]=true;}
 if(index==11){s.flags[336]=true;s.mainWeapon=12;s.inventory[12]=0;}
 if(index>=12){s.flags[147]=true;}
 if(index==12||index==13){s.flags[339]=true;s.auxiliaryWeapon=27;s.inventory[27]=0;}
 s.magazines[12]=4;s.magazines[27]=2;s.inventory[31]=17;s.inventory[29]=1;s.accessory=51;
 if(index==14){s.auxiliaryWeapon=29;s.inventory[29]=0;}
 if(index==13){s.quickshotTicks=400;s.flags[47]=true;s.tp=15;}
 return e;
}
SavedPresentation media(const CombatContent& c,int index){
 SavedPresentation p;
 for(const auto& [id,v]:c.media.at("audio").items()){if(id.starts_with("bgm-")){p.music.resource=id;break;}}
 need(!p.music.resource.empty(),"Real BGM reference required");p.music.volume=65;p.music.pitch=100;p.music.balance=50;p.tone={80+index,90,100};p.saturation=80;return p;
}
void check(const CombatContent& c,const GameApplication& g,const CombatEntry& e,int index){
 const GameSession expected(c.items,e.session,c.facts);const auto& s=g.session();
 for(int f:{87,88,93,145,141,142,143,144,147,316,318,314,272,273,337,338,274,213,336,339}){need(s.flag(f)==expected.flag(f),"Persistent branch fact mismatch");}
 for(int item:{66,67,68,69,70,87,88,91,92,12,27,29,31}){need(s.count(item)==expected.count(item),"Tool, reward or ammunition inventory changed");}
 need(s.magazine(12)==4&&s.magazine(27)==2,"SG and LC9 must retain independent magazines");
 for(int slot=1;slot<=5;++slot){need(s.equipped(slot)==expected.equipped(slot),"Equipment changed during recovery");}
 need(s.saveCount()==8&&s.variable(399)==6&&s.variable(388)==2345,"Save continuation must be exactly 8/6");
 need(s.knowsSkill(3)&&!s.knowsSkill(4),"Safe save context restores active mixing identity");
 if(index==13){need(s.tp()==15&&s.flag(47),"Persistent skill does not double charge");}
}
int main(int argc,char** argv){try{
 if(argc!=4){return 2;}auto c=loadCombatContent(argv[1]);const std::filesystem::path root=argv[2];const std::string mode=argv[3];
 for(int map:{16,22}){for(int i=0;i<15;++i){
  auto e=sample(c,map,i);c.entries["81-state-fixture"]=e;const auto dir=root/std::to_string(map);const auto picture=media(c,i);
  if(mode=="write"){
   GameApplication g(c,"81-state-fixture");g.configureSaveDirectory(dir);finishTitleOpening(g);g.setPresentationState(picture);g.update(key());need(g.mode()==ApplicationMode::Saving,"Actual crow must open saving");const int remaining=g.session().quickshotTicks();
   for(int n=0;n<200;++n){g.update({});}g.update(key());need(g.fileMenuView().has_value(),"Original confirmation must open slots");
   for(int n=0;n<i;++n){g.update(key(InputAction::Down));}g.update(key());need(g.mode()==ApplicationMode::Playing,"Save must return control");check(c,g,e,i);
   auto disk=SaveFiles(c,dir).read(i+1);need(disk.session.saveCount==8&&disk.session.variables.at(399)==5,"Disk holds 8/5 before continuation");need(disk.presentation==picture,"Capture preserves supplied playback/tone values");
   if(i==13){need(g.session().quickshotTicks()==remaining,"Save UI pauses remaining effect time");}
  }else if(mode=="load"){
   GameApplication g(c,"title");g.configureSaveDirectory(dir);finishTitleOpening(g);std::optional<SavedPresentation> prepared;
   g.configureRestorePresentation([&](const SceneDefinition& d,const SavedPresentation& p){need(d.width>0&&d.height>0,"Prepared real scene");prepared=p;});
   g.update(key());need(g.mode()==ApplicationMode::Loading,"New process title Load");for(int n=0;n<i;++n){g.update(key(InputAction::Down));}g.update(key());
   need(g.mode()==ApplicationMode::Playing&&g.mapId()==map,"Load restores selected real save room");check(c,g,e,i);need(prepared&&*prepared==picture,"Restore sends original audio/tone to presentation owner");
   if(i==13){need(g.session().quickshotTicks()==900,"Load rebuilds original quickshot wait");}
   const auto disk=SaveFiles(c,dir).read(i+1);std::string error;need(g.restore(disk,error)&&g.session().variable(399)==6,"Repeated Load must not accumulate continuation");
   const auto before=g.session().exportState();const auto generation=g.generation();
   g.configureRestorePresentation([](const SceneDefinition&,const SavedPresentation&){throw std::runtime_error("81 missing presentation resource");});
   need(!g.restore(disk,error)&&g.generation()==generation&&g.session().exportState()==before&&g.mode()==ApplicationMode::Playing,"Failed resource preparation preserves running state");
   g.configureRestorePresentation({});auto invalid=disk;invalid.session.worldFlags[c.facts.switches.at(314)]=true;invalid.session.worldFlags[c.facts.switches.at(318)]=false;
   need(!g.restore(invalid,error)&&g.generation()==generation&&g.session().exportState()==before,"Invalid candidate cannot replace valid session");
   need(g.newGame(error),"Fresh NORMAL game can start after B/C progress");
   for(int f:{87,93,145,144,147,316,318,314,272,273,337,338,274,336,339}){need(!g.session().flag(f),"New game retained old branch fact");}
   need(g.session().count(70)==0&&g.session().count(87)==0&&g.session().count(91)==0&&g.session().magazine(12)==0&&g.session().magazine(27)==0,"Fresh Map2 cannot keep weapons or premature magazine seeds");
  }else{return 2;}
 }}std::cout<<"B/C integration "<<mode<<": 30 isolated legal-save combinations passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
