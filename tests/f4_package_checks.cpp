#include "combat_content.h"
#include "save_snapshot.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace fs=std::filesystem;
void require(bool v,const char* m){if(!v){throw std::runtime_error(m);}}
std::string read(const fs::path& p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void write(const fs::path& p,const std::string& s){std::ofstream out(p,std::ios::binary);out<<s;require(bool(out),"Fixture write failed");}
struct Fixture{fs::path path;~Fixture(){std::error_code e;fs::remove_all(path,e);}};
int main(int argc,char** argv){
 if(argc<2){return 2;}
 try{
  Fixture fixture{fs::temp_directory_path()/("nobi-f4-package-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
  fs::copy(fs::absolute(argv[1]).parent_path(),fixture.path,fs::copy_options::recursive);
  auto content=loadCombatContent(fixture.path/"f4.json");require(content.maps.at(22).height==15,"Moved package remains self-contained");
  for(const auto& animation:content.media.at("animations")){
   require(animation.contains("playback")&&animation.at("playback").at("logic_updates_per_frame")==2,"Every exported animation has executable playback timing");
  }
  const auto original=read(fixture.path/"f4.json");
  auto rejects=[&](const nlohmann::json& bad){write(fixture.path/"f4.json",bad.dump());bool failed=false;try{loadCombatContent(fixture.path/"f4.json");}catch(const std::exception&){failed=true;}write(fixture.path/"f4.json",original);require(failed,"Malformed F4 definition must fail");};
  auto json=nlohmann::json::parse(original),bad=json;bad["capabilities"]["weapons"]["14"]=json.at("batch")=="A"?"executable":"state-only";rejects(bad);
  bad=json;bad["f3"]["weapon_bindings"]["21"]["kind"]="unknown";rejects(bad);
  bad=json;bad["animations"]["5"].erase("playback");rejects(bad);
  bad=json;bad["f4"]["enemies"][0]["behavior"]="unknown";rejects(bad);
  bad=json;bad["f4"]["flows"].begin().value()["steps"][0]["kind"]="unsupported-runtime-command";rejects(bad);
  bad=json;bad["f4"]["flows"].begin().value()["steps"][0]={{"kind","cue"},{"cue","no-such-cue"}};rejects(bad);
  bad=json;bad["f4"]["enemy_behaviors"]["licker"]["grapple"]["failure_cycles"]=-1;rejects(bad);
  if(json.at("batch")!="A"){
   bad=json;bad["f4"]["attacks"].erase("21");rejects(bad);
   bad=json;bad["f4"]["attacks"]["21"]=json["f4"]["attacks"]["14"];rejects(bad);
   bad=json;bad["f3"]["weapon_bindings"]["21"]["kind"]="shotgun";rejects(bad);
   bad=json;bad["f4"]["meeting"]["entry_page"]=999;rejects(bad);
   bad=json;bad["f4"]["tool"]["active_area"]="absent";rejects(bad);
   bad=json;bad["f4"]["mixing"]["choice"]="missing";rejects(bad);
   bad=json;bad["f4"]["staged_flows"]["vent-remove"]["steps"][0]["kind"]="raw-script";rejects(bad);
   bad=json;bad["f4"]["attacks"]["14"]["muzzle_forward_px"]["down"]=8;rejects(bad);
  }
  const auto source=fixture.path/"f3-source-contract.json";auto sourceBytes=read(source);write(source,sourceBytes+" ");
  bool failed=false;try{loadCombatContent(fixture.path/"f4.json");}catch(const std::exception&){failed=true;}require(failed,"Modified fixed source contract must fail even with valid JSON");write(source,sourceBytes);
  if(argc==3){
   const auto bytes=read(argv[2]);const auto value=decodeSave(content,bytes);auto restored=prepareRestore(content,value);
   require(restored.scene->mapId()==16,"Real F3 copy restores its original save map");require(read(argv[2])==bytes,"Load never writes the source copy");
  }
  std::cout<<"F4 package and source-copy checks passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
