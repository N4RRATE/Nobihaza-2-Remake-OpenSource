#include "combat_content.h"
#include "expanded_content_validation.h"
#include <fstream>
#include <iostream>
int main(int argc,char**argv){try{
 if(argc!=2)return 2;auto c=loadCombatContent(argv[1]);const auto& story=c.expansion.meetingStory;
 if(story.is_null()||!c.maps.contains(25)||!c.objects.contains("m12-e20")||!c.objects.contains("m12-e21"))throw std::runtime_error("Missing C story definitions");
 if(story.at("conference").at("facing_branches").at(3).at("direction")!="up")throw std::runtime_error("Changed source duplicate-up condition");
 if(c.routes.at("m25-e9-p2-c47").steps.at(0).kind!="right"||c.routes.at("m25-e9-p2-c47").steps.at(3).kind!="down")throw std::runtime_error("Guard route differs from original");
 std::ifstream in(argv[1]);const auto d=nlohmann::json::parse(in);
 for(int n=0;n<6;++n){auto bad=d;
  if(n==0)bad["f4"]["meeting_story"]["escape"]["transfer"]["map"]=999;
  if(n==1)bad["f4"]["meeting_story"]["escape"]["skill_conversion"]["learn"]=999;
  if(n==2)bad["f4"]["meeting_story"]["return"]["steps"][0]["cue"]="absent";
  if(n==3)bad["f4"]["meeting_story"]["conference"]["facing_branches"][3]["direction"]="diagonal";
  if(n==4)bad["f4"]["meeting_story"]["safe_room_skill"]["learn"]=3.5;
  if(n==5)bad["f4"]["meeting_story"]["safe_room_skill"]["forget"]=true;
  bool rejected=false;try{validateExpandedContent(c,bad);}catch(const std::exception&){rejected=true;}if(!rejected)throw std::runtime_error("Invalid story accepted");
 }
 std::cout<<"F4 C content checks passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
