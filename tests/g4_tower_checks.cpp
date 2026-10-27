#include "g2_natural_driver.h"
#include "full_content.h"
#include "scene_presentation.h"
#include "content_hash.h"
#include "combat_presentation.h"
#include "scene_renderer.h"
#include "dialogue_renderer.h"
namespace{
template<class F>void rejects(F fn){bool caught=false;try{fn();}catch(const std::exception&){caught=true;}need(caught,"Invalid tower candidate accepted");}
CombatEntry entry(const CombatContent& c,const SessionEntry& s,int map,TilePosition tile){auto e=c.entries.at("post-pickup");e.session=s;e.map=map;e.playerTile=tile;e.direction=Direction::Up;e.positions.clear();e.directions.clear();e.pages.clear();e.through.clear();e.session.flags[1]=e.session.flags[18]=e.session.flags[13]=true;return e;}
void rules(const CombatContent& c,const SaveSnapshot& saved,const std::string& raw,const std::filesystem::path& root){
 need(decodeSave(c,encodeSave(c,saved))==saved,"New G4 ordinary identity failed roundtrip");SaveFiles files(c,root/"native-g4");need(files.write(1,saved).outcome==WriteOutcome::Committed&&files.read(1)==saved,"New G4 ordinary identity failed native persistence");const auto restored=prepareRestore(c,files.read(1));need(restored.scene->mapId()==151&&restored.session->hp()==46&&restored.session->saveCount()==60,"New G4 ordinary identity changed restored progress");
 auto old=decodeSave(*c.legacySource,raw);auto reduced=saved.session;need(!reduced.worldFlags.at(c.facts.switches.at(507)),"Upgrade invented S507");reduced.worldFlags.erase(c.facts.switches.at(507));need(reduced==old.session&&saved.scene==old.scene&&saved.presentation==old.presentation,"Tower adapter changed old progress");
 need(GameSession(c.items,saved.session,c.facts).variable(283)==218&&continueCheckpoint(c,218).at("map")==218&&continueCheckpoint(c,219).at("map")==219,"Retained old checkpoint lost");
 auto bad=saved;bad.session.worldFlags.erase(c.facts.switches.at(507));rejects([&]{validateSave(c,bad);});rejects([&]{continueCheckpoint(c,777);});
 GameSession s(c.items,saved.session,c.facts);s.captureContinueCheckpoint(c.magazineSeedBindings,219,{9,7});const auto captured=s.exportState();s.loseHp(3);s.captureContinueCheckpoint(c.magazineSeedBindings,218,{9,7});need(s.variable(283)==219&&s.variable(288)==captured.worldValues.at(c.facts.variables.at(288)),"Repeated capture overwrote219");auto candidate=s.prepareContinueRestore(c.magazineSeedBindings,219,{9,7});need(candidate.hp()==46&&candidate.variable(630)==2&&s.hp()==43,"Continue preparation partially published");s.commitContinueRestore(candidate);need(s.hp()==46&&s.variable(630)==2&&s.count(111)==1&&s.saveCount()==60,"Continue219 changed source statistics/card");
 auto e=entry(c,saved.session,190,{14,30});GameSession owned(c.items,e.session,c.facts);CombatScene scene(c,owned,e,safe);ScenePresentation p(scene);p.validateSequence(c.catalog.at("flows").at("m190-e7-p1").at("steps"),"m190-e7");
 auto illegal=c.catalog.at("flows").at("m190-e7-p1").at("steps");illegal.push_back({{"kind","wait"},{"ticks",1}});rejects([&]{p.validateSequence(illegal,"m190-e7");});rejects([&]{p.validateSequence(Json::array({Json{{"kind","clear_combat_workspace"},{"first",239},{"last",269}}}),"m190-e7");});
 const auto generation=scene.generation();scene.deactivate();need(!scene.restoreMessageDisplay(generation,{}),"Old tower generation accepted message state");const auto stable=owned.exportState();for(int n=0;n<50;++n){scene.update({});}need(owned.exportState()==stable,"Retired tower changed checkpoint");
 need(!c.maps.contains(192)&&c.catalog.at("pending_maps").at("192")==2,"01 published fake battlefield");
 std::cout<<"01 rules old-adaptation/218-219/single-backup/prepare-commit/terminal-call/rejections/retirement PASS\n";
}
void failureAndVariation(const CombatContent& original,const SessionEntry& base){
 // Explicit alternate-map fixture tests the shared transfer transaction; no production boss is added.
 auto c=original;c.catalog.erase("pending_maps");
 for(auto& op:c.catalog["flows"]["m190-e8-p1"]["steps"]){if(op.at("kind")=="required_relocate"){op["destination"]={{"map",159},{"tile",{12,4}}};}}
 for(int amount:{0,1,3}){
  auto e=entry(c,base,190,{14,30});e.session.inventory[180]=amount;e.session.inventory[195]=2;
  GameSession state(c.items,e.session,c.facts);CombatScene scene(c,state,e,safe);bool fail=true;int rejected=0;
  scene.configurePresentation([&](const SceneDefinition& d){if(d.width==c.maps.at(159).width&&fail){++rejected;throw std::runtime_error("Injected final transfer resource failure");}});
  InputFrame move;move.movement=Direction::Up;scene.update(move);
  for(int n=0;n<30000&&scene.transferError().empty();++n){scene.update(n%8==0?key():InputFrame{});scene.takePresentation();}
  need(scene.mapId()==190&&!scene.transferError().empty()&&state.flag(586)&&state.variable(283)==219&&!state.flag(1)&&!state.flag(18)&&!state.flag(507),"Transfer rejection published a partial world/tail");
  need(state.count(180)==std::max(0,amount-1)&&state.count(195)==2+(amount>0),"CE166 lost repeated file quantities");
  const auto before=state.exportState();for(int n=0;n<20;++n){scene.update({});scene.takePresentation();}need(state.exportState()==before&&rejected>1,"Rejected retry replayed conversion/checkpoint");
  fail=false;for(int n=0;n<30&&scene.foreground()!=ForegroundKind::None;++n){scene.update({});scene.takePresentation();}
  need(scene.mapId()==159&&state.flag(507)&&state.flag(1)&&state.flag(18)&&scene.playerVisible()&&state.exportState().worldValues==before.worldValues,"Successful required transfer failed the tail or resampled checkpoint");
  if(amount==0){
   state.loseHp(1000);for(int n=0;n<3000&&!scene.inputPromptView().cursor;++n){scene.update(n%8==0?key():InputFrame{});scene.takePresentation();}
   need(scene.inputPromptView().cursor.has_value(),"219 Continue prompt absent");const auto dead=state.exportState();scene.update(key());for(int n=0;n<3000&&scene.transferError().empty();++n){scene.update(n%8==0?key():InputFrame{});scene.takePresentation();}
   need(!scene.transferError().empty()&&state.exportState()==dead,"Unavailable192 Continue partially committed219 backup/counter");
  }
  std::cout<<"01 alternate-map final-transfer reject/retry/tail file-count="<<amount<<" PASS\n";
 }
}
struct RenderBridge{
 G2NaturalDriver& r;const CombatContent& c;sf::Font font;sf::RenderTexture surface{{320,240}};std::unique_ptr<CombatPresentation> present,staged;std::vector<PresentationEvent> events;std::filesystem::path root;std::set<std::string> frames;
 RenderBridge(G2NaturalDriver& run,const CombatContent& content,const std::filesystem::path& directory):r(run),c(content),root(directory){need(font.openFromFile(c.maps.begin()->second.fontPath),"Tower font missing");present=std::make_unique<CombatPresentation>(c,font);r.game.configureRestorePresentation([&](const SceneDefinition&,const SavedPresentation& p){auto next=std::make_unique<CombatPresentation>(c,font);next->prepareRestoredState(p);staged=std::move(next);});}
 void tick(){if(staged){present=std::move(staged);present->activateRestoredState();}present->update(events);events.clear();r.game.setPresentationState(present->saveState());}
 void frame(const std::string& name){if(!frames.insert(name).second){return;}const auto center=r.game.cameraCenter();surface.setView(sf::View({float(center.x),float(center.y)},{320,240}));surface.clear(sf::Color::Black);present->drawBackground(surface,r.game);SceneRenderer renderer(r.game.definition());renderer.draw(surface,r.game.world(),present->characterFlashes(),r.game.playerVisible(),present->saturation());present->draw(surface,r.game);DialogueRenderer dialogue(font);dialogue.setMedia(present->images(),present->palette());dialogue.draw(surface,r.game.dialogue().view());surface.display();auto pixels=surface.getTexture().copyToImage();if(name=="pending-black"){for(unsigned y=0;y<240;++y){for(unsigned x=0;x<320;++x){need(pixels.getPixel({x,y})==sf::Color::Black,"Source pending transfer did not render black");}}}need(pixels.saveToFile(root/(name+".png")),"Tower offscreen export failed");}
};
Json natural(const CombatContent& c,const std::string& raw,const std::filesystem::path& out,int x){
 const auto directory=out/std::to_string(x);std::filesystem::create_directories(directory);std::ofstream(directory/"slot-01.json")<<raw;G2NaturalDriver r(c,directory);r.game.configureSaveDirectory(directory);std::unique_ptr<RenderBridge> render;if(x==14){render=std::make_unique<RenderBridge>(r,c,directory);}
 int notices=0;bool body=false,locked=true,parallel=false,graphic1=false,graphic2=false;std::vector<std::string> cues;double previousCamera{},previousActor{};
 const auto graphic=[&](const std::string& id){for(const auto& step:c.routes.at(id).steps){if(step.kind=="graphic"){return step.sprite;}}throw std::runtime_error("Missing reviewed graphic");};
 const auto dora1=graphic("m190-e8-p1-c82"),dora2=graphic("m190-e8-p1-c84");
 r.observe=[&](const PresentationEvent& event){if(render){render->events.push_back(event);}if(event.kind==PresentationEvent::Kind::Cue){cues.push_back(event.id);}if(event.kind==PresentationEvent::Kind::Notice&&event.id.find("后续战斗")!=std::string::npos){++notices;}};
 r.afterTick=[&]{if(render){render->tick();}if(!r.game.hasGame()||r.game.mapId()!=190){return;}if(r.game.foreground()==ForegroundKind::Content&&r.game.world().player().motion.tile()==TilePosition{14,22}){body=true;}
  const auto actor=findCharacter(r.game.world(),"m190-e8");const double camera=r.game.cameraCenter().y,position=actor.motion.position().y;
  if(body){locked&=r.game.foreground()==ForegroundKind::Content&&!r.game.session().flag(1)&&!r.game.session().flag(18);graphic1|=actor.spriteId==dora1;graphic2|=actor.spriteId==dora2;if(r.game.dialogue().active()&&previousCamera!=camera&&previousActor!=position){parallel=true;}}
  if(render&&body){if(r.game.dialogue().active()){render->frame("prelude-message");}if(actor.spriteId==dora1){render->frame("dora-graphic1");}if(actor.spriteId==dora2){render->frame("dora-graphic2");}}
  previousCamera=camera;previousActor=position;
 };
 r.load();need(r.game.session().hp()==46&&r.game.session().count(111)==1&&r.game.session().saveCount()==60,"Real G3 Save60 Load changed progress");r.reception();const auto inheritedMusic=render?render->present->saveState().music:SoundDefinition{};r.actObject("m159-e6");if(render){need(render->present->saveState().music==inheritedMusic&&render->present->musicPlaying(),"M190 type1 failed actual inherited playback");render->frame("tower-entered");}need(r.game.mapId()==190&&r.game.world().player().motion.tile()==TilePosition{14,37},"Natural tower door absent");r.actObject("m190-e6");need(r.game.mapId()==159&&r.game.world().player().motion.tile()==TilePosition{12,4},"Natural tower return absent");r.actObject("m159-e6");
 r.walk({x,30});InputFrame in;in.movement=Direction::Up;r.tick(in);for(int n=0;n<30000&&!notices;++n){r.tick(n%8==0?key():InputFrame{});}std::cout<<"observed notices="<<notices<<" body="<<body<<" locked="<<locked<<" graphics="<<(graphic1&&graphic2)<<" parallel="<<parallel<<" cues="<<Json(cues).dump()<<'\n';need(notices==1&&body&&locked&&graphic1&&graphic2,"Full source prelude did not reach locked transfer");
 std::cout<<"pending map="<<r.game.mapId()<<" 586="<<r.game.session().flag(586)<<" destination="<<r.game.session().variable(283)<<" 507="<<r.game.session().flag(507)<<" player-visible="<<r.game.playerVisible()<<'\n';
 need(r.game.mapId()==190&&r.game.session().flag(586)&&r.game.session().variable(283)==219&&!r.game.session().flag(507),"Pending transfer falsely opened battlefield");
 need(findCharacter(r.game.world(),"m190-e8").motion.tile().y>12,"Called Self did not move E8");need(parallel,"Source no-wait camera/route/message did not overlap");
 if(render){render->frame("pending-black");need(render->present->musicPlaying()&&render->present->saveState().music.resource==c.cue("m190-e8-p1-c143").at("sound").at("resource").get<std::string>(),"Prelude tail did not play actual starwolf");}
 auto before=r.game.session().exportState();for(int n=0;n<50;++n){r.tick(key(InputAction::Cancel));}need(r.game.session().exportState()==before&&notices==1&&r.game.mode()==ApplicationMode::Playing,"Pending transfer repeated capture or opened ordinary input");
 for(int id=240;id<=269;++id){need(r.game.session().variable(id)==0,"Entry left combat workspace");}need(r.game.session().hp()==46&&r.game.session().saveCount()==60&&r.game.session().count(111)==1,"Prelude healed/saved/discarded card");
 std::ofstream(directory/"observed.json")<<Json{{"x",x},{"steps",r.ticks},{"map",r.game.mapId()},{"checkpoint",r.game.session().variable(283)},{"cues",cues},{"parallel",parallel},{"body_locked",locked}}.dump(2)<<'\n';
 Json checkpoint;for(int id=272;id<=300;++id){checkpoint[std::to_string(id)]=r.game.session().variable(id);}for(int id=401;id<=606;++id){checkpoint[std::to_string(id)]=r.game.session().variable(id);}
 std::cout<<"01 natural x="<<x<<" steps="<<r.ticks<<" checkpoint=219 full-body/Self/parallel locked192 PASS\n";return checkpoint;
}
}
int main(int argc,char** argv){try{std::cout.setf(std::ios::unitbuf);need(argc==5,"01 package real-save isolated-root rules|natural");auto c=loadCombatContent(argv[1]);const auto raw=read(argv[2]);need(contentSha256(raw)=="f87bb3d63d67803dc33916916dface8096d7ad09ef86cf3270570c93627edb62","Changed actual G3 final ordinary disk");const auto saved=decodeSave(c,raw);const std::filesystem::path root=argv[3];std::filesystem::create_directories(root);if(std::string(argv[4])=="rules"){rules(c,saved,raw,root);failureAndVariation(c,saved.session);}else{Json reference;for(int x:{13,14,15}){const auto checkpoint=natural(c,raw,root,x);if(reference.is_null()){reference=checkpoint;}else{need(reference==checkpoint,"Three source entries produced different checkpoints");}}std::cout<<"01 three natural entries identical backup PASS\n";}return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
