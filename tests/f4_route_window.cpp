#include "combat_presentation.h"
#include "full_content.h"
#include <iostream>
int main(int argc,char** argv){try{
 if(argc<2){return 2;}auto content=loadCombatContent(argv[1]);auto e=content.entries.at("defeated-claimed");
 const std::string option=argc>2?argv[2]:"";
 if(option=="--playtest-enemy"){
  if(argc!=5){throw std::runtime_error("playtest-enemy fixture expects map59/26 and weapon14/20");}
  const int map=std::stoi(argv[3]),weapon=std::stoi(argv[4]);
  if((map!=59&&map!=26)||(weapon!=14&&weapon!=20)){throw std::runtime_error("Unsupported playtest-enemy fixture");}
  e.map=map;e.playerTile=map==26?TilePosition{16,17}:TilePosition{8,6};e.direction=map==26?Direction::Left:Direction::Right;
  e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();initializeFullSession(content,e.session);
  e.session.hp=e.session.maxHp=9999;e.session.baseDefense=999;e.session.baseAttack=300;e.session.inventory[weapon]=1;e.session.mainWeapon=weapon;
  e.session.flags[1]=e.session.flags[18]=e.session.flags[166]=true;e.session.flags[13]=e.session.flags[167]=e.session.flags[168]=e.session.flags[187]=false;e.session.variables[91]=0;
  // Explicit lethal-hit window fixture; source combat/presentation and isolated saves remain active.
  content.entries["playtest-enemy-window-fixture"]=e;
  return runCombatGame(content,"playtest-enemy-window-fixture",0,std::filesystem::temp_directory_path()/"nobihaza-playtest-enemy-window-saves");
 }
 if(option=="--evacuation-tail"){
  if(argc!=3||!content.catalog.contains("escort")){throw std::runtime_error("Evacuation tail window requires the delivered105 escort package");}
  e.map=116;e.playerTile={15,12};e.direction=Direction::Up;e.sprite=content.maps.at(116).player.spriteId;
  e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();initializeFullSession(content,e.session);
  e.session.flags[341]=true;e.session.variables[269]=30;e.session.variables[267]=0;
  // Explicit settled-guard fixture: all subsequent116->118->119->121 behavior runs through source flows.
  content.entries["105-evacuation-tail-window-fixture"]=e;
  return runCombatGame(content,"105-evacuation-tail-window-fixture",0,std::filesystem::temp_directory_path()/"nobihaza-105-tail-window-saves");
 }
 if(option=="--truth-eyes"||option=="--truth-trap"){
  if(argc!=3||content.fullStage!="truth"||content.pendingMapTickets.contains(101)){throw std::runtime_error("Truth window requires a delivered truth package and one fixture option");}
  e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.direction=Direction::Up;
  e.session.inventory[185]=1;e.session.flags[1]=e.session.flags[18]=true;e.session.flags[13]=false;
  for(int id:{310,316,318,274,213,338}){e.session.flags[id]=true;}
  if(option=="--truth-eyes"){e.map=37;e.playerTile={10,5};e.session.inventory[89]=e.session.inventory[91]=1;}
  else{e.map=101;e.playerTile=content.objects.at("m101-e17").tile;e.session.flags[313]=e.session.flags[314]=e.session.flags[315]=true;}
  // Explicit in-memory window fixture; it never replaces natural entry or formal saves.
  content.entries["100-truth-window-fixture"]=e;
  return runCombatGame(content,"100-truth-window-fixture",0,std::filesystem::temp_directory_path()/"nobihaza-100-truth-window-saves");
 }
 e.map=argc>2?std::stoi(argv[2]):21;e.playerTile=e.map==19?TilePosition{19,16}:TilePosition{18,11};e.direction=Direction::Up;
 if(argc>4){e.playerTile={std::stoi(argv[3]),std::stoi(argv[4])};}if(argc>5){e.direction=directionFromName(argv[5]);}
 e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();e.session.mainWeapon=21;e.session.flags[57]=false;
 content.entries["55-window-fixture"]=e;
 return runCombatGame(content,"55-window-fixture",0,std::filesystem::temp_directory_path()/"nobihaza-55-window-saves");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
