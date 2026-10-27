#include "combat_presentation.h"
#include "full_content.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cstring>
int main(int argc,char** argv){try{
 if(argc!=2){throw std::runtime_error("usage escort_picture_checks package");}auto content=loadCombatContent(argv[1]);const auto& escort=content.catalog.at("escort");
 auto images=nlohmann::json::object();const auto system=content.media.at("ui").at("menu").at("system_image").get<std::string>();images[system]=content.media.at("images").at(system);
 std::vector<PresentationEvent> events;
 for(const auto& [number,page]:escort.at("details").at("pages").items()){
  for(const auto& row:page.at("role").at("media")){
   const auto id=row.at("cue").get<std::string>();const auto& cue=content.cue(id);const auto kind=cue.at("kind");
   if(kind!="show_picture"&&kind!="move_picture"){continue;}if(kind=="show_picture"){const auto image=cue.at("image").get<std::string>();images[image]=content.media.at("images").at(image);}
   PresentationEvent event{PresentationEvent::Kind::Cue,id,{},{},0};event.resolvedPicture=ResolvedPicture{{42,63},cue.at("transparency_percent"),0};events.push_back(std::move(event));
  }
 }
 content.media["images"]=std::move(images);content.media["effects"]=nlohmann::json::object();content.media["audio"]=nlohmann::json::object();sf::Font font;CombatPresentation consumer(content,font);
 consumer.update(events);consumer.update({{PresentationEvent::Kind::SceneChanged,{},{},{},0}});consumer.update(events);
 // These source pages set fixed_to_map, but each resolved event is already sampled in screen pixels.
 // Moving the render view must not subtract the camera a second time.
 auto sample=*std::find_if(events.begin(),events.end(),[&](const PresentationEvent& event){const auto& cue=content.cue(event.id);return cue.at("kind")=="show_picture"&&cue.at("fixed_to_map").get<bool>();});
 sample.resolvedPicture=ResolvedPicture{{160,120},0,0};CombatPresentation anchored(content,font);anchored.update({sample});GameApplication game(content,"post-pickup");sf::RenderTexture target({320,240});
 const auto draw=[&](float offset){target.setView(sf::View(sf::FloatRect{{offset,0},{320,240}}));target.clear(sf::Color::Transparent);anchored.draw(target,game);target.display();return target.getTexture().copyToImage();};auto first=draw(0),second=draw(56);
 if(std::memcmp(first.getPixelsPtr(),second.getPixelsPtr(),320*240*4)!=0){throw std::runtime_error("Resolved screen picture was shifted twice by camera");}
 auto unresolved=events.front();unresolved.resolvedPicture.reset();bool rejected=false;try{consumer.update({unresolved});}catch(const std::runtime_error&){rejected=true;}if(!rejected){throw std::runtime_error("Unresolved variable anchor was accepted");}
 std::cout<<"105 actual picture consumer accepts all three resolved costume pages/recreated Scene and rejects missing anchor\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
