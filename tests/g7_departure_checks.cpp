#include "combat_content.h"
#include "game_application.h"
#include "save_snapshot.h"
namespace { void need(bool,const std::string&); }
#include "g4_dora_render.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace{
void need(bool ok,const std::string& text){if(!ok){throw std::runtime_error(text);}}
int quiet(int,int high){return high;}
InputFrame confirm(){InputFrame in;in.buttons[int(InputAction::Confirm)]={true,true,false};return in;}
CombatEntry entry(const CombatContent& c,int map,TilePosition tile,bool second=false){auto e=c.entries.at("departure-capability");e.map=map;e.playerTile=tile;e.positions.clear();e.pages.clear();e.directions.clear();e.through.clear();e.session.worldFlags[c.facts.switches.at(580)]=second;e.session.worldFlags[c.facts.switches.at(582)]=false;return e;}
void runDoor(CombatContent c,int y,const std::filesystem::path& root,bool render){
 c.entries["door"]=entry(c,206,{30,y});GameApplication game(std::move(c),"door",y==9?CombatScene::Random([](int low,int){return low;}):CombatScene::Random(quiet));const auto& content=game.content();std::unique_ptr<DoraRender> pixels;if(render){pixels=std::make_unique<DoraRender>(game,content,root);pixels->pending=game.takePresentation();pixels->tick();pixels->frame("cargo-door");}else{game.takePresentation();}
 const auto originalInventory=game.session().exportState().inventory;
 int steps=0,explosions=0;bool first=false,departure=false,second=false,fourteen=false,hidden=false,inventoryAtHandoff=false;std::vector<double> tentacles;
 for(;steps<100000;++steps){
  const int before=game.mapId();const auto generation=game.generation();game.update(steps%2==0?confirm():InputFrame{});const auto events=game.takePresentation();
  for(const auto& ev:events){
   need(ev.kind!=PresentationEvent::Kind::StopSounds,"Alarm closure must not blanket-stop voices");
   if(ev.kind==PresentationEvent::Kind::Sound&&ev.sound.resource==content.cue("m208-e1-p1-c30").at("sound").at("resource").get<std::string>()){++explosions;}
   if(ev.kind==PresentationEvent::Kind::Cue&&ev.id.starts_with("m207-e2-p2-")&&(ev.id=="m207-e2-p2-c93"||ev.id=="m207-e2-p2-c194"||content.cue(ev.id).at("kind")=="move_picture")){if(ev.sampledScreenPosition){tentacles.push_back(ev.sampledScreenPosition->y);}}
  }
  if(pixels){pixels->pending=events;pixels->tick();}
  if(before==206&&game.mapId()==207){need(game.world().player().motion.tile()==TilePosition{3,8},"Three original doors must share207(3,8)");need(game.generation()!=generation,"Map commit must retire generation");first=true;}
  if(game.mapId()==207&&!game.session().flag(580)){
   if(!game.session().flag(529)){need(!game.session().flag(1)&&!game.session().flag(18),"First cockpit must close alarm/controls");}
   if(game.world().player().motion.tile()==TilePosition{17,7}){fourteen=true;if(pixels){pixels->frame("first-cockpit-fourteen-steps");}}
  }
  if(game.mapId()==208){departure=true;need(!game.playerVisible()&&!game.session().flag(1)&&!game.session().flag(18),"Ship presentation must retain hidden player/closed controls");if(game.session().flag(580)){need(game.foreground()==ForegroundKind::Content,"Late580 belongs to source foreground");}if(pixels&&std::any_of(events.begin(),events.end(),[](const auto& event){return event.id=="m208-e1-p1-c6";})){pixels->frame("ship-trajectory");}if(pixels&&game.session().flag(580)){pixels->frame("late580-white-explosion");}}
  if(before==208&&game.mapId()==207&&pixels){for(const auto& picture:pixels->presentation->saveState().warehouseVisual.at("pictures")){need(picture.at(0)!=1&&picture.at(0)!=2,"M208 default pictures must clear on map commit");}}
  if(before==207&&game.mapId()==209){need(!game.panorama(),"Departure background must not leak into209");}
  if(game.mapId()==207&&game.session().flag(580)){
   second=true;need(!game.session().flag(582),"Departure must not grant final victory");
   if(game.panorama()){need(game.panorama()->definition.speedX==-6,"Second cockpit must consume -6 dynamic background");if(pixels&&pixels->presentation->saveState().warehouseVisual.at("screen").at(0)==0){pixels->frame("second-cockpit-background");}}
   if(!game.playerVisible()){hidden=true;}if(tentacles.size()==4&&pixels){pixels->frame("tentacle-discrete-step");}
  }
  if(game.mapId()==209&&!game.session().flag(581)){need(game.session().exportState().inventory==originalInventory,"Actor gun graphics must not change player inventory before209");inventoryAtHandoff=true;}
  if(game.mapId()==209&&game.session().flag(581)&&game.foreground()==ForegroundKind::None){break;}
 }
 need(steps<100000&&first&&departure&&second&&fourteen&&hidden,"Full original cockpit chain did not reach actual final battle");
 need(explosions==3,"Three identical original explosion submissions were collapsed");need(tentacles.size()==14,"Both tentacles must retain show and six discrete moves");for(int round=0;round<2;++round){for(int n=1;n<7;++n){need(tentacles[round*7+n]==tentacles[round*7+n-1]-240,"Tentacle must step -240 six times");}}
 need(game.playerVisible()&&game.session().flag(1)&&game.session().flag(18)&&!game.session().flag(582),"Actual209 must own visibility/control reopening");need(inventoryAtHandoff,"Original inventory was not observed before209 owns file sealing");
 if(pixels){pixels->frame("actual209-control-open");}
 std::ofstream(root/("door-"+std::to_string(y)+".json"))<<nlohmann::json{{"kind","public-application-capability-chain-not-G6-natural"},{"steps",steps},{"door_y",y},{"tentacle_positions",tentacles},{"explosions",explosions},{"maps",{206,207,208,207,209}},{"final_battle_started",true}}.dump(2)<<'\n';
 std::cout<<"G7-02 public door"<<y<<" complete62/40/227->actual209 PASS steps="<<steps<<'\n';
}
void panorama(CombatContent c,const std::filesystem::path& root){
 c.entries["background"]=entry(c,207,{18,7},true);auto& flow=c.catalog["flows"]["m207-e2-p2"];
 const auto legacy=std::find_if(c.cues.begin(),c.cues.end(),[](const auto& row){return row.second->at("kind")=="panorama"&&!row.second->contains("scroll");});need(legacy!=c.cues.end(),"Existing legacy panorama cue unavailable");const auto legacyId=legacy->first;const auto legacyImage=legacy->second->at("image").get<std::string>();
 flow["steps"]=nlohmann::json::array({{{"kind","cue"},{"cue","m207-e2-p2-c3"}},{{"kind","cue"},{"cue","m207-e2-p2-c7"}},{{"kind","cue"},{"cue","m207-e2-p3-c1"}},{{"kind","cue"},{"cue","m207-e2-p2-c7"}},{{"kind","cue"},{"cue",legacyId}},{{"kind","cue"},{"cue","m207-e2-p2-c7"}},{{"kind","required_relocate"},{"destination",{{"map",208},{"tile",{9,8}}}}}});
 GameApplication game(std::move(c),"background",quiet);const auto& content=game.content();DoraRender pixels(game,content,root);pixels.pending=game.takePresentation();pixels.tick();game.update({});pixels.pending=game.takePresentation();pixels.tick();need(game.panorama()&&game.panorama()->definition.speedX==-6,"Dynamic cue must install Scene-owned parameters");
 const auto background=[&](){const auto center=game.cameraCenter();pixels.surface.setView(sf::View({float(center.x),float(center.y)},{320,240}));pixels.surface.clear(sf::Color::Black);pixels.presentation->drawBackground(pixels.surface,game);pixels.surface.display();return pixels.surface.getTexture().copyToImage();};
 const auto startPixels=background();const auto phase=game.panorama()->phase;pixels.frame("panorama-minus6-start");pixels.tick();pixels.tick();need(game.panorama()->phase==phase,"Renderer must not advance logical phase");
 game.update({});pixels.pending=game.takePresentation();pixels.tick();need(game.panorama()->phase[0]==64,"-6 must advance64 internal units/update");pixels.frame("panorama-minus6-next");const auto movedPixels=background();bool different=false;for(unsigned y=0;y<240;++y){for(unsigned x=0;x<318;++x){need(movedPixels.getPixel({x,y})==startPixels.getPixel({x+2,y}),"Dynamic -6 renderer must actually shift background left2 pixels");different|=movedPixels.getPixel({x,y})!=startPixels.getPixel({x,y});}}need(different,"Dynamic panorama pixels stayed static");
 bool changed=false;for(int n=0;n<130;++n){const auto previous=game.panorama()->phase;game.update({});pixels.pending=game.takePresentation();pixels.tick();if(game.panorama()&&game.panorama()->definition.speedX==-7){need(game.panorama()->phase[0]==(previous[0]+64)%(content.media.at("images").at(game.panorama()->definition.image).at("size_px").at(0).get<int>()*32),"Same-image speed change must retain scrolling phase");changed=true;break;}}
 need(changed,"P3 -7 variation not applied");const auto before=game.panorama()->phase;game.update({});pixels.pending=game.takePresentation();pixels.tick();need(game.panorama()->phase[0]==(before[0]+128)%(content.media.at("images").at(game.panorama()->definition.image).at("size_px").at(0).get<int>()*32),"-7 must advance128 internal units/update");pixels.frame("panorama-minus7-next");
 for(int n=0;n<130&&game.panorama();++n){game.update({});pixels.pending=game.takePresentation();pixels.tick();}need(game.mapId()==207&&!game.panorama(),"Legacy image-only cue must replace a dynamic override on the same map");const auto staticPixels=background(),expected=pixels.presentation->images().at(legacyImage).copyToImage();for(unsigned y=0;y<240;++y){for(unsigned x=0;x<320;++x){need(staticPixels.getPixel({x,y})==expected.getPixel({x,y}),"Legacy panorama must draw its own static pixels after dynamic override");}}
 for(int n=0;n<130&&game.mapId()==207;++n){game.update({});pixels.pending=game.takePresentation();pixels.tick();}need(game.mapId()==208&&!game.panorama(),"Map disposal must clear dynamic override");
 std::cout<<"G7-02 Scene -6/-7 phase retention/renderer-read-only/map-disposal PASS\n";
}
void failure(CombatContent c){
 auto e=entry(c,206,{30,8});GameSession session(c.items,e.session,c.facts);bool reject=true;CombatScene scene(c,session,e,quiet,[&](const SceneDefinition& d){if(reject&&d.backgroundPath.filename()=="map-207-background.png"){throw std::runtime_error("isolated candidate resource failure");}});
 const auto generation=scene.generation();scene.update(confirm());scene.takePresentation();for(int n=0;n<100;++n){scene.update({});scene.takePresentation();}need(scene.generation()==generation&&scene.mapId()==206&&!scene.transferError().empty(),"Failed candidate must preserve source world/generation");reject=false;for(int n=0;n<5&&scene.mapId()!=207;++n){scene.update({});scene.takePresentation();}need(scene.mapId()==207&&scene.generation()!=generation,"Prepared retry must commit target");
 auto stale=scene.requestInput(NumberPrompt{});scene.deactivate();need(!scene.takeInputResult(stale),"Retired generation must not accept old result");
 std::cout<<"G7-02 required candidate retry/source preservation/retired result PASS\n";
}
void pages(const CombatContent& c){
 auto e=entry(c,207,{3,8});GameSession s(c.items,e.session,c.facts);const auto& cockpit=c.objects.at("m207-e2");need(c.activeVariant(cockpit,s)->page==1,"First cockpit page not selected");s.setFlag(580,true);need(c.activeVariant(cockpit,s)->page==2,"Late580 must select second cockpit");s.setFlag(582,true);need(c.activeVariant(cockpit,s)->page==3,"Real victory582 must select highest cockpit page");s.setFlag(580,false);need(c.activeVariant(cockpit,s)->page==3,"Highest victory page must not depend on580");
 std::cout<<"G7-02 actual page selection580/582 highest precedence PASS\n";
}
void saves(const CombatContent& c,const std::string& path){
 std::ifstream file(path);const std::string raw{std::istreambuf_iterator<char>(file),{}};need(!raw.empty(),"Missing isolated old save");const auto previous=decodeSave(*c.legacySource,raw),next=decodeSave(c,raw);
 need(next.scene==previous.scene&&next.warehouse==previous.warehouse&&next.session.inventory==previous.session.inventory&&next.session.hp==previous.session.hp&&next.session.saveCount==previous.session.saveCount&&next.session.conditions==previous.session.conditions,"Departure altered legitimate previous save");
 const auto encoded=encodeSave(c,next);need(decodeSave(c,encoded)==next,"Departure changed existing v4/v5 roundtrip");need(nlohmann::json::parse(encoded).at("version")== (next.continuation.kind=="direct_unscored"?5:4),"Existing save version changed");
 auto restored=prepareRestore(c,next,{},quiet);need(restored.scene->mapId()==previous.scene.map,"Prior save restore moved player");auto bad=next;bad.session.worldFlags[c.facts.switches.at(580)]=true;bool rejected=false;try{validateSave(c,bad);}catch(const std::exception&){rejected=true;}need(rejected,"Pre-cockpit save cannot invent completed departure");
 for(int map:{206,207,208}){auto e=entry(c,map,{9,8});GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,quiet);bool blocked=false;try{captureSave(c,scene,{scene.generation(),map==206?"m206-e2":map==207?"m207-e2":"m208-e1","cockpit"},{});}catch(const std::exception&){blocked=true;}need(blocked,"Cockpit invented an ordinary save point");}
 std::cout<<"G7-02 previous save version"<<(next.continuation.kind=="direct_unscored"?5:4)<<" restore/roundtrip/illegal580/no-cockpit-save PASS\n";
}

}
int main(int argc,char** argv){try{std::cout.setf(std::ios::unitbuf);need(argc>=3&&argc<=6,"Expected package/evidence and optional door or saves paths");const auto c=loadCombatContent(argv[1]);std::filesystem::path root=argv[2];std::filesystem::create_directories(root);if(argc==4&&std::string(argv[3])=="pages"){pages(c);}else if(argc==4&&std::string(argv[3])=="background"){panorama(c,root);}else if(argc>=5){pages(c);for(int n=4;n<argc;++n){saves(c,argv[n]);}}else if(argc==4){runDoor(c,std::stoi(argv[3]),root,true);}else{panorama(c,root);failure(c);for(int y:{7,8,9}){runDoor(c,y,root,y==8);}}return 0;}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}
