#include "title_navigation.h"
#include "g2_natural_driver.h"
#include "g4_dora_render.h"
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
namespace {
std::string bytes(const std::filesystem::path&p){std::ifstream f(p);need(bool(f),"Missing file "+p.string());return {std::istreambuf_iterator<char>(f),{}};}
int quiet(int,int hi){return hi;}
struct Runtime{
 GameApplication game;DoraRender view;std::map<std::string,int> cues;std::optional<MusicPlaybackIdentity> token;
 Runtime(const CombatContent&c,const std::filesystem::path&p,const std::string&entry="title"):game(c,entry,quiet),view(game,c,p/"frames"){}
 void tick(InputFrame in={}){if(auto t=view.presentation->musicTicks()){game.reportMusicTicks(t->first,t->second);}game.update(in);for(const auto&e:game.takePresentation()){++cues[e.id];if(e.musicPlayback){token=e.musicPlayback;}view.pending.push_back(e);}view.tick();}
 void framePrompt(const std::string& name){view.frame(name);DialogueRenderer text(view.font);text.setMedia(view.presentation->images(),view.presentation->palette());text.draw(view.surface,game.inputPromptView());view.surface.display();need(view.surface.getTexture().copyToImage().saveToFile(view.root/(name+".png")),"Clear prompt pixels missing");}
 void frameFiles(const std::string& name){view.surface.setView(sf::View({160,120},{320,240}));view.surface.clear(sf::Color::Black);view.presentation->draw(view.surface,game);view.surface.display();need(view.surface.getTexture().copyToImage().saveToFile(view.root/(name+".png")),"Clear file menu pixels missing");}
 void slots(){for(int n=0;n<200&&game.mode()!=ApplicationMode::Saving;++n){tick();}need(game.mode()==ApplicationMode::Saving&&game.fileMenuView(),"Clear slots not opened");}
 void gate(){for(int n=0;n<200&&game.hasGame()&&game.mapId()==215;++n){tick();}need(!game.hasGame()||game.mapId()==216,"Clear gate stalled");}
 void prompt(){for(int n=0;n<200&&!game.inputPromptView().cursor;++n){tick();}need(game.mapId()==216&&game.inputPromptView().cursor,"M216 prompt absent");}
};
SaveSnapshot replay(const CombatContent&c,const std::filesystem::path&prior,const std::filesystem::path&out,bool currentTitle=false){
 Runtime r(c,out);r.game.configureSaveDirectory(out/"slots");finishTitleOpening(r.game);SaveFiles f(c,out/"slots");auto old=decodeSave(c,bytes(prior/"slots/slot-01.json"));need(f.write(1,old).outcome==WriteOutcome::Committed,"Seed actual Save68 failed");r.game.returnToTitle();finishTitleOpening(r.game);r.game.takePresentation();
 const auto evidence=Json::parse(bytes(prior/"natural-result.json"));const int ack=evidence.at("audio_first_pass_tick");std::ifstream trace(prior/"public-inputs.jsonl");std::string line;int ticks=0,credits=0,victories=0;std::optional<MusicPlaybackIdentity> creditToken;
 while(std::getline(trace,line)){
  const auto row=Json::parse(line);InputFrame in;const int movement=row.at("movement"),held=row.at("held"),pressed=row.at("pressed");if(movement>=0){in.movement=Direction(movement);}for(std::size_t i=0;i<in.buttons.size();++i){in.buttons[i].held=held&(1<<i);in.buttons[i].pressed=pressed&(1<<i);}
  if(currentTitle&&ticks==0){need(movement==-1&&held==1&&pressed==1&&r.game.mode()==ApplicationMode::Title&&r.game.menuSelection()==1,"Recorded legacy title prelude changed");in={};}
  r.game.update(in);if(currentTitle&&ticks==2){need(r.game.mode()==ApplicationMode::Playing&&r.game.mapId()==old.scene.map&&r.game.session().saveCount()==old.session.saveCount,"Current public title Load did not reproduce recorded source boundary");}for(const auto&e:r.game.takePresentation()){++r.cues[e.id];if(e.musicPlayback){r.token=e.musicPlayback;}if(r.game.mapId()>=215){r.view.pending.push_back(e);}}if(r.game.mapId()>=215){r.view.tick();}++ticks;if(ticks%6000==0){std::cout<<"Recorded input replay step="<<ticks<<" map="<<r.game.mapId()<<'\n';}if(r.game.takeFinalBossVictory()){++victories;}
  if(r.game.hasGame()&&r.game.mapId()==211){++credits;if(r.token&&r.token->resource==c.cue("m211-e1-p1-c5").at("sound").at("resource").get<std::string>()){creditToken=r.token;}if(credits==ack){need(creditToken&&r.game.reportMusicFirstPass(*creditToken),"Recorded valid first-pass feedback rejected");}}
 }
 need(victories==1&&r.game.hasGame()&&r.game.mapId()==215&&r.game.session().exportState().party.empty(),"Recorded natural route did not enter Clear from actual214");r.slots();const auto before=r.game.session().exportState();need(before.party==std::vector<int>{8}&&before.actor8Name=="　通关数据","Clear before-file identity absent");
 // Slot2 is isolated; recorded Save68 remains slot1. Real muted PCM keeps running.
 r.tick(key(InputAction::Down));r.tick(key());need(r.game.mode()==ApplicationMode::Playing,"Real Clear commit failed");auto saved=f.read(2);need(saved.formatVersion==6&&saved.continuation.kind=="clear_after_file"&&saved.session.saveCount==before.saveCount+1&&saved.continuation.statisticBefore==before.variables.at(399),"Clear native/statistic counts wrong");
 if(currentTitle){need(r.cues["m215-e1-p1-c2"]==1,"Release Clear source C2 repeated");std::ofstream(out/"natural-link.json")<<Json{{"kind","recorded public inputs; obsolete title Up becomes idle because current saved title defaults to Load"},{"prior_steps",ticks},{"prior_victories",victories},{"source214_to215",true},{"native_before",before.saveCount},{"native_after",saved.session.saveCount},{"v399",saved.continuation.statisticBefore},{"saved_schema",saved.formatVersion},{"actual_PCM_save",true},{"post_clear_consumer","independent release load-only checks current C7/216 boundary"}}.dump(2)<<'\n';std::cout<<"Recorded actual214 -> Clear Committed; independent current C7/216 check follows\n";return saved;}
 r.gate();need(r.game.mapId()==216&&r.game.session().exportState().party.empty()&&r.game.session().actor8Name()=="dammy","Committed Clear did not return through C7/C8");r.prompt();r.framePrompt("clear216-choice");
 need(r.cues["m215-e1-p1-c2"]==1,"Source C2 repeated");r.tick(key(InputAction::Cancel));r.tick();need(r.game.postClearRequest()&&r.game.postClearRequest()->selection==1&&r.game.session().saveCount()==saved.session.saveCount,"M216 Cancel is not second request or incremented counts");
 std::ofstream(out/"natural-link.json")<<Json{{"kind","replay of03 recorded public inputs and validated03 credit audio feedback; new214->Clear uses actual PCM/save"},{"prior_steps",ticks},{"prior_victories",victories},{"source214_to215",true},{"native_before",before.saveCount},{"native_after",saved.session.saveCount},{"v399",saved.continuation.statisticBefore},{"early_gate_ticks",r.game.session().variable(44)},{"saved_schema",6},{"C2_once",true},{"Cancel_request",1}}.dump(2)<<'\n';
 std::cout<<"Recorded actual source214->215/realPCM/Clear saved/C7-C8/216 Cancel shell PASS\n";return saved;
}
CombatContent controlled(const CombatContent&c,const SaveSnapshot&s){auto copy=c;CombatEntry e;e.map=215;e.playerTile={9,7};e.direction=s.scene.direction;e.sprite=s.scene.sprite;e.session=s.session;e.session.saveCount=s.continuation.countBefore;e.session.actor8Name="dammy";e.session.party.clear();copy.entries["controlled-clear"]=e;return copy;}
void outcomes(const CombatContent&c,const SaveSnapshot&actual,const std::filesystem::path&root){
 const auto local=controlled(c,actual);
 for(const auto name:{"save","cancel","failure","recovery-current","recovery-old","late-cancel","late-save"}){
  Runtime r(local,root/name,"controlled-clear");bool fail=true;const std::string scenario=name;
  r.game.configureSaveDirectory(root/name/"slots",[&](std::string_view stage){if(fail&&((scenario=="failure"&&stage=="begin")||((scenario=="recovery-current"&&stage=="after-replace")||(scenario=="recovery-old"&&stage=="replace")))){throw std::runtime_error("Clear isolated "+scenario);}});
  if(scenario=="recovery-old"){auto old=actual;old.build="old-committed";SaveFiles seed(local,root/name/"slots");need(seed.write(1,old).outcome==WriteOutcome::Committed,"Old recovery seed failed");}
  r.slots();const auto baseline=r.game.session().exportState();need(r.token.has_value(),"Clear BGM instance absent");auto stale=*r.token;++stale.scene;need(!r.game.reportMusicTicks(stale,900),"Old scene ticks accepted");stale=*r.token;++stale.instance;need(!r.game.reportMusicTicks(stale,900),"Old instance ticks accepted");
  if(scenario.starts_with("late")){
   for(int n=0;n<200;++n){std::this_thread::sleep_for(std::chrono::milliseconds(40));r.tick();auto t=r.view.presentation->musicTicks();if(t&&t->second>500){break;}}
   need(r.view.presentation->musicTicks()&&r.view.presentation->musicTicks()->second>500,"Actual PCM late ticks never crossed500");
  }
  if(scenario=="cancel"||scenario=="late-cancel"){r.tick(key(InputAction::Cancel));}
  else{r.tick(key());if(scenario=="recovery-old"){r.tick(key(InputAction::Up));r.tick(key());}
   if(scenario=="failure"){need(r.game.mode()==ApplicationMode::Saving&&r.game.session().saveCount()==baseline.saveCount,"Failed write committed count");r.tick(key(InputAction::Cancel));r.tick(key(InputAction::Cancel));}
   if(scenario.starts_with("recovery")){
    need(r.game.mode()==ApplicationMode::Saving&&r.game.session().saveCount()==baseline.saveCount,"RecoveryRequired counted Saved");fail=false;r.tick(key());r.tick(key());if(r.game.fileMenuView()&&!r.game.fileMenuView()->confirmation){r.tick(key());}
    auto view=r.game.fileMenuView();need(view&&view->confirmation,"Current recovery choice confirmation missing");r.tick(key(InputAction::Up));r.tick(key());if(scenario=="recovery-old"){need(r.game.mode()==ApplicationMode::Saving&&r.game.session().saveCount()==baseline.saveCount,"Old backup recovery masqueraded as current Saved");r.tick(key(InputAction::Cancel));r.tick(key(InputAction::Cancel));}
   }
  }
  r.gate();const bool late=scenario.starts_with("late");need(late?!r.game.hasGame():r.game.mapId()==216,"C9 gate depends on Saved bool");
  if(r.game.hasGame()){const bool committed=scenario=="save"||scenario=="recovery-current";need(r.game.session().saveCount()==baseline.saveCount+(committed?1:0)&&r.game.session().variable(399)==actual.continuation.statisticBefore,"Outcome counts wrong");need(r.game.session().exportState().party.empty()&&r.game.session().actor8Name()=="dammy","Return identity wrong");if(scenario=="save"){r.prompt();r.framePrompt("actual-clear216-prompt");}}
  std::cout<<"Clear "<<name<<" actualPCM outcome PASS\n";
 }
}
void strict(const CombatContent&c,const SaveSnapshot&s){
 const auto good=encodeSave(c,s);need(decodeSave(c,good)==s,"Clear strict roundtrip differs");
 for(const auto name:{"kind","version","actor8","party","node","final","574","575","rank","unknown","music"}){
  auto j=Json::parse(good);std::string n=name;
  if(n=="kind"){j["after_save"]["kind"]="ordinary";}else if(n=="version"){j["version"]=4;j["schema"]="nobihaza.save.v4";}else if(n=="actor8"){j["clear_actor"]["name"]="dammy";}else if(n=="party"){j["session"]["party"]={1,8};}else if(n=="node"){j["after_save"]["point"]="m215-e1-p1-c2";}else if(n=="unknown"){j["clear_actor"]["future"]=1;}else if(n=="music"){j["presentation"]["music"]["pitch"]=100;}else{const auto key=c.facts.switches.contains(582)?c.facts.switches.at(582):"";if(n=="final"||n=="574"||n=="575"){const auto target=n=="final"?key:c.facts.switches.at(std::stoi(n));for(auto& pair:j["session"]["world_flags"]){if(pair[0]==target){pair[1]=false;}}}else{for(auto& pair:j["session"]["world_values"]){if(pair[0]=="score.rank"){pair[1]=0;}}}}
  bool rejected=false;try{decodeSave(c,j.dump());}catch(const std::exception&){rejected=true;}need(rejected,"Bad Clear accepted "+n);
 }
 auto old=decodeSave(c,bytes("private/implementation-g6/G6-07/natural-terminal/slot-01.json"));need(old.continuation.kind=="ordinary"&&old.formatVersion==4,"Known actual ordinary v4 refused");
 auto malformed=Json::parse(encodeSave(c,old));malformed["session"]["party"]={8};bool badOld=false;try{decodeSave(c,malformed.dump());}catch(const std::exception&){badOld=true;}need(badOld,"Clear relaxed ordinary party validation");
 GameApplication live(c,"final-capability",quiet);const auto liveState=live.session().exportState();const auto gen=live.generation();live.configureRestorePresentation([](const SceneDefinition&,const SavedPresentation&){throw std::runtime_error("isolated Clear resource reject");});std::string error;need(!live.restore(s,error)&&live.generation()==gen&&live.session().exportState()==liveState,"Clear candidate preparation failure lost old session");
 auto restored=prepareRestore(c,s,{},quiet);const auto count=restored.session->saveCount(),statistics=restored.session->variable(399);const auto request=restored.scene->takeSaveRequest();need(!request&&count==s.session.saveCount&&statistics==s.continuation.statisticBefore,"Clear Load replayed save/count");restored.scene->deactivate();auto before=restored.session->exportState();for(int n=0;n<50;++n){restored.scene->update({});}need(before==restored.session->exportState(),"Retired C7 result mutated session");
 std::cout<<"Clear schema/cursor/party/actor/unknown/music/final/rank strict rejection/retirement PASS\n";
}
void endAudio(const CombatContent&c,const std::filesystem::path&root){
 const auto& d=c.media.at("audio").at("bgm-clear-j-nazo");PredecodedMusic track(c.directory/d.at("file").get<std::string>(),{},false,d.at("clear_midi"));track.setVolume(0);track.play();const auto first=track.midiTicks();need(first&&*first<=500,"Fresh PCM instance did not begin before gate");
 int current=0;for(int n=0;n<320;++n){std::this_thread::sleep_for(std::chrono::milliseconds(40));current=track.midiTicks().value_or(0);if(current>1100){break;}}
 std::this_thread::sleep_for(std::chrono::milliseconds(350));const int end=track.midiTicks().value_or(0);std::this_thread::sleep_for(std::chrono::milliseconds(160));need(track.getStatus()==sf::SoundSource::Status::Playing&&track.midiTicks()==end&&end>1100,"End-loop did not stay alive with frozen MIDI mtime");track.stop();
 PredecodedMusic replay(c.directory/d.at("file").get<std::string>(),{},false,d.at("clear_midi"));replay.setVolume(0);replay.play();need(replay.midiTicks()&&*replay.midiTicks()<=500,"New/Load music instance inherited end position");
 std::ofstream(root/"actual-midi.json")<<Json{{"kind","real SFML PCM consumption at volume0; fixed message MIDI mtime profile"},{"fresh_ticks",*first},{"end_ticks",end},{"end_stays_playing",true},{"end_ticks_frozen",true},{"new_instance_restarts",true},{"source_start_mtime_us",2349204},{"tempo_mtime_us",2349929},{"quantum_frames",64},{"quantum_mtime_us",725},{"end_mtime_us",7743047}}.dump(2)<<'\n';std::cout<<"Actual muted PCM/first-silence trim/tempo mtime/end alive and frozen/new instance PASS\n";
}
void retiredSave(const CombatContent&c,const SaveSnapshot&snapshot,const std::filesystem::path&root){
 auto local=controlled(c,snapshot);const auto& entry=local.entries.at("controlled-clear");GameSession session(local.items,entry.session,local.facts);CombatScene scene(local,session,entry,quiet);std::optional<SaveRequest> request;
 for(int n=0;n<200&&!request;++n){scene.update({});scene.takePresentation();request=scene.takeSaveRequest();}need(request.has_value(),"Controlled actual Clear request absent");auto stale=*request;++stale.request;need(!scene.clearSavePending(stale),"Old Clear request ID accepted");
 SaveFiles files(local,root/"retired-save");SavedPresentation state;state.music=readSound(local.catalog.at("clear").at("music"));SaveController controller(local,session,scene,files,*request,state);const auto before=session.exportState();scene.deactivate();controller.update(key());controller.update(key());need(controller.finished()&&session.exportState()==before&&!std::filesystem::exists(root/"retired-save/slot-01.json")&&!scene.clearSavePending(*request),"Retired Clear wrote or applied old result");std::cout<<"Retired Clear Scene/request/controller repeated input preserves count and disk PASS\n";
}
void independent(const CombatContent&c,const std::filesystem::path&slot,const std::filesystem::path&root,bool currentBoundary=false){
 Runtime r(c,root);r.game.configureSaveDirectory(slot.parent_path());finishTitleOpening(r.game);r.game.returnToTitle();finishTitleOpening(r.game);r.tick(key());r.tick(key(InputAction::Down));need(r.game.fileMenuView()&&r.game.fileMenuView()->lines.at(1).find("通关数据 LV1 HP 1")!=std::string::npos,"Clear file menu displayed Actor1 vitals instead of Actor8");r.frameFiles("independent-clear-slot");r.tick(key());need(r.game.mapId()==215&&r.game.mode()==ApplicationMode::Playing,"Independent public Clear Load failed");auto saved=decodeSave(c,bytes(slot));
 r.gate();r.prompt();need(r.game.session().saveCount()==saved.session.saveCount&&r.game.session().variable(399)==saved.continuation.statisticBefore&&r.game.session().exportState().party.empty()&&r.game.session().actor8Name()=="dammy","Independent Load counts/identity wrong");need(r.cues["m215-e1-p1-c2"]==0&&r.cues["restore-clear-instance"]==1,"Independent Load replayed source C2");r.framePrompt("independent-load216");if(currentBoundary){std::cout<<"Independent release public Clear Load/actual PCM/C7/216 controls/counts PASS\n";return;}r.tick(key());r.tick();need(r.game.postClearRequest()&&r.game.postClearRequest()->selection==0,"Start shell did not retain request");std::cout<<"Independent process public Clear Load/music fresh/C7 only/216 start request PASS\n";
}
}
int main(int argc,char**argv){try{std::cout.setf(std::ios::unitbuf);need(argc==5,"Expected package output replay|load prior-dir|slot");if(std::string(argv[3])=="audio"){CombatContent component;component.directory=std::filesystem::path(argv[1]).parent_path();component.media["audio"]=Json::parse(bytes(argv[1])).at("projection").at("audio");endAudio(component,argv[2]);return 0;}std::cout<<"Loading Clear catalogue\n";auto c=loadCombatContent(argv[1]);std::cout<<"Loaded Clear catalogue\n";if(std::string(argv[3])=="catalogue"){return 0;}const std::filesystem::path root=argv[2];std::filesystem::create_directories(root);if(std::string(argv[3])=="load-only"){independent(c,argv[4],root,true);}else if(std::string(argv[3])=="load"){retiredSave(c,decodeSave(c,bytes(argv[4])),root);independent(c,argv[4],root);}else if(std::string(argv[3])=="replay-only"){(void)replay(c,argv[4],root,true);}else{auto s=std::string(argv[3])=="continue"?decodeSave(c,bytes(argv[4])):replay(c,argv[4],root);strict(c,s);outcomes(c,s,root/"outcomes");}return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
