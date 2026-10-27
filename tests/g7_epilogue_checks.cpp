#include "g2_natural_driver.h"
#include "g4_dora_render.h"
#include "predecoded_music.h"
#include <chrono>
#include <thread>
#include <fstream>
namespace {
std::string read(const std::filesystem::path& p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int quiet(int,int high){return high;}
CombatEntry fixture(const CombatContent& c,int map){auto e=c.entries.at("final-capability");e.map=map;e.playerTile=map==212?TilePosition{9,4}:TilePosition{9,7};e.direction=Direction::Down;e.positions.clear();e.pages.clear();e.directions.clear();e.through.clear();return e;}
void party(const CombatContent& c){
 auto e=c.entries.at("final-capability");GameSession s(c.items,e.session,c.facts);const auto before=s.exportState();s.joinParty(8);s.joinParty(8);need(s.exportState().party==std::vector<int>({1,8}),"Party duplicate or missing8");s.leaveParty(1);s.leaveParty(8);s.leaveParty(8);need(s.exportState().party.empty(),"Party remove did not commit");auto after=s.exportState();after.party=before.party;need(after==before,"Party mutation destroyed actor/inventory state");s.joinParty(1);need(s.exportState()==before,"Actor1 rejoin did not retain state");bool bad=false;try{s.joinParty(2);}catch(const std::exception&){bad=true;}need(bad,"Unsupported party actor accepted");
 e.map=213;e.playerTile={9,7};e.session=s.exportState();e.session.party.clear();GameSession empty(c.items,e.session,c.facts);CombatScene score(c,empty,e,quiet);need(!score.playerVisible()&&empty.hp()>0,"Empty score party leaked default player or became dead");for(int n=0;n<100;++n){score.update({});score.takePresentation();}need(score.foreground()==ForegroundKind::Content,"Empty party blocked noncombat score");
 std::cout<<"G7-03 party1/8 idempotency/state retention/empty score noncombat PASS\n";
}
void victoryTail(const CombatContent& c){
 auto e=fixture(c,209);e.playerTile={13,9};e.direction=Direction::Left;e.session.worldFlags[c.facts.switches.at(581)]=true;e.session.worldFlags[c.facts.switches.at(576)]=true;e.session.variables[89]=1000;e.session.variables[203]=150;e.session.variables[81]=150;
 GameSession session(c.items,e.session,c.facts);bool reject=true;CombatScene scene(c,session,e,quiet,[&](const SceneDefinition& d){if(reject&&d.backgroundPath.filename()=="map-210-background.png"){throw std::runtime_error("isolated post-victory candidate failure");}});const auto generation=scene.generation();int victories=0,pairedSound=0;std::map<std::string,int> observed;
 const auto sound=c.cue("m209-e10-p10-c52").at("sound").at("resource").get<std::string>();
 const auto observe=[&]{for(const auto& event:scene.takePresentation()){++observed[event.id];if(scene.mapId()==210&&event.kind==PresentationEvent::Kind::Sound&&event.sound.resource==sound){++pairedSound;}}if(scene.takeFinalBossVictory()){++victories;}};
 for(int n=0;n<1600&&scene.transferError().empty();++n){scene.update(key());observe();}
 need(!scene.transferError().empty()&&scene.mapId()==209&&scene.generation()==generation&&session.flag(582)&&victories==1,"Failed winning transfer lost committed victory/source generation");
 const int hp=session.hp(),damage=session.variable(89);for(int n=0;n<20;++n){InputFrame in=key(InputAction::Attack);scene.update(in);observe();}std::set<std::string> hit;need(scene.resolveAttack({150,120},false,0,false,hit).processed==0&&session.hp()==hp&&session.variable(89)==damage&&victories==1,"Post-win rejected candidate reopened combat/repeated victory");
 reject=false;for(int n=0;n<1600&&scene.mapId()!=207;++n){scene.update(key());observe();}
 need(scene.mapId()==207&&victories==1&&pairedSound==2&&observed["m209-e10-p10-c54"]==1,"Victory crossWorld tail/paired SE13/player Animation66 incomplete");for(int n:{58,59,60,61}){need(observed["m209-e10-p10-c"+std::to_string(n)]==1,"Paired7/25 picture move omitted/repeated");}
 scene.deactivate();const auto state=session.exportState();for(int n=0;n<100;++n){scene.update(key());}need(session.exportState()==state&&!scene.takeFinalBossVictory(),"Retired victory emitted delayed source state");std::cout<<"G7-03 controlled real winning consumer/M210 failure preserves victory/no combat/retry/C36-62/twoSE13/Animation66/7-25 pair/retirement PASS\n";
}
void readingRestore(const CombatContent& c){
 auto e=fixture(c,210);e.playerTile={12,9};e.session.worldFlags[c.facts.switches.at(576)]=true;e.session.worldFlags[c.facts.switches.at(582)]=true;
 const std::array<int,10> sealed{195,197,198,199,200,201,202,204,205,206};const std::array<int,10> readable{180,182,183,184,185,186,187,189,190,191};
 for(int n=0;n<10;++n){e.session.inventory[sealed[n]]=1;e.session.inventory[readable[n]]=0;}
 GameSession session(c.items,e.session,c.facts);CombatScene scene(c,session,e,quiet);const auto ammo=session.exportState().magazines;const auto before=scene.generation();
 for(int n=0;n<12000&&scene.mapId()==210;++n){scene.update(n%2==0?key():InputFrame{});scene.takePresentation();}
 need(scene.mapId()==207&&scene.generation()!=before&&scene.playerVisible()&&scene.world().player().motion.tile()==TilePosition{3,8},"Full210/Self routes/Show after transfer absent");
 for(int n=0;n<10;++n){need(session.count(sealed[n])==0&&session.count(readable[n])==1,"CE167 complete ten-item file restore failed");}
 need(session.exportState().magazines==ammo,"File restoration changed ammo");std::cout<<"G7-03 actual210 Self/CE55/ten CE167 files/207 show tail/ammo retention PASS\n";
}
void credits(const CombatContent& c,const std::filesystem::path& root){
 auto e=fixture(c,211);GameSession s(c.items,e.session,c.facts);CombatScene scene(c,s,e,quiet);std::optional<MusicPlaybackIdentity> token;std::vector<double> positions;int step=0,fade=-1,stop=-1;
 for(;step<1000&&positions.size()<2;++step){scene.update({});for(const auto& event:scene.takePresentation()){if(event.musicPlayback){token=event.musicPlayback;}if(event.resolvedPicture){positions.push_back(event.resolvedPicture->position.y);}}}
 need(token&&positions==std::vector<double>({1680,240}),"Actual credits positions not1680->240: "+Json(positions).dump()+" token="+std::to_string(bool(token))+" ticks="+std::to_string(step));auto old=*token;++old.instance;need(!scene.reportMusicFirstPass(old),"Stale music instance accepted");old=*token;++old.scene;need(!scene.reportMusicFirstPass(old),"Stale music scene accepted");old=*token;old.resource="foreign";need(!scene.reportMusicFirstPass(old),"Wrong music track accepted");need(scene.reportMusicFirstPass(*token)&&!scene.reportMusicFirstPass(*token),"First pass did not commit exactly once");const int submitted=step;
 for(;step<9800&&scene.mapId()==211;++step){scene.update({});for(const auto& event:scene.takePresentation()){
  if(event.kind==PresentationEvent::Kind::FadeMusic&&event.id=="m211-e2-p1-c3"){need(fade<0,"Parallel music faded twice");fade=step;}
  if(event.kind==PresentationEvent::Kind::StopMusic){stop=step;}
  if(event.resolvedPicture){positions.push_back(event.resolvedPicture->position.y);need(event.resolvedPicture->durationTenths==715,"Credits duration truncated");}
 }}
 need(scene.mapId()==212&&positions==std::vector<double>({1680,240,-1200})&&fade==submitted&&stop-fade==60,"Music first-pass fade/wait/OFF or independent complete scroll failed");need(step>9300,"Music completion cut original credit scroll short");need(!scene.reportMusicFirstPass(*token),"Departed scene accepted old music");scene.deactivate();auto state=s.exportState();for(int n=0;n<100;++n){scene.update(key());}need(s.exportState()==state&&!scene.reportMusicFirstPass(*token),"Retired music scene resumed");
 std::ofstream(root/"credits-rules.json")<<Json{{"kind","actual Scene finite consumer with injected condition feedback; not audio timing acceptance"},{"positions",positions},{"steps",step},{"fade_tick",fade},{"stop_tick",stop},{"wait_ticks",stop-fade},{"old_scene_instance_track_rejected",true}}.dump(2)<<'\n';
 std::cout<<"G7-03 two715 scroll/independent first-pass60 wait/old track-instance-scene/retirement PASS\n";
}
void summer(const CombatContent& c,const std::filesystem::path& root){
 auto e=fixture(c,212);e.session.party={1,8};e.session.hp=20;e.session.tp=0;e.session.conditions.poisoned=true;e.session.conditions.neuralParalysis=true;GameSession s(c.items,e.session,c.facts);bool reject=true;CombatScene scene(c,s,e,quiet,[&](const SceneDefinition& d){if(reject&&d.backgroundPath.filename()=="map-213-background.png"){throw std::runtime_error("isolated score candidate failure");}});
 const auto inventory=s.exportState().inventory,magazines=s.exportState().magazines;std::set<std::string> messages;bool actors=false;std::uint64_t generation=scene.generation();
 for(int n=0;n<18000&&scene.transferError().empty();++n){scene.update(n%2==0?key():InputFrame{});scene.takePresentation();if(scene.dialogue().active()){messages.insert(std::to_string(scene.dialogue().view().request));}if(scene.dialogue().active()&&!actors){
  for(auto [id,t]:std::initializer_list<std::pair<const char*,TilePosition>>{{"m212-e4",{8,5}},{"m212-e6",{10,5}},{"m212-e5",{10,6}},{"m212-e2",{8,7}},{"m212-e1",{10,7}},{"m212-e7",{9,5}}}){need(findCharacter(scene.world(),id).motion.tile()==t,"Summer actor endpoint incorrect "+std::string(id));}
  need(scene.world().player().motion.tile()==TilePosition{8,6},"Summer player endpoint incorrect");actors=true;
 }}
 need(actors&&messages.size()==25,"Full summer messages/actor routes absent "+std::to_string(messages.size()));need(scene.mapId()==212&&scene.generation()==generation&&!scene.transferError().empty(),"Failed score candidate replaced old world");need(s.exportState().party.empty()&&s.hp()==s.maxHp()&&s.tp()==s.maxTp()&&!s.conditions().poisoned&&!s.conditions().neuralParalysis&&s.exportState().inventory==inventory&&s.exportState().magazines==magazines,"C158 actor1 recovery/party removal/ammo preservation failed");
 reject=false;for(int n=0;n<5&&scene.mapId()!=213;++n){scene.update({});scene.takePresentation();}need(scene.mapId()==213&&scene.generation()!=generation&&!scene.playerVisible(),"Score candidate retry/empty party failed");
 for(int n=0;n<25000&&!scene.endingClearRequest();++n){scene.update(n%2==0?key():InputFrame{});scene.takePresentation();}need(scene.mapId()==214&&scene.endingClearRequest()&&s.variable(381)>0,"Real score/unlocks/Clear source request absent");need(s.exportState().inventory==inventory,"Score fabricated an item award");scene.deactivate();need(!scene.endingClearRequest(),"Retired ending clear request survived");
 std::ofstream(root/"summer-rules.json")<<Json{{"kind","controlled real Scene summer/score source; not natural final entry"},{"message_requests",messages.size()},{"party_empty",true},{"actor1_restored",true},{"inventory_magazines_retained",true},{"candidate_reject_retry",true},{"rank",s.variable(381)}}.dump(2)<<'\n';
 std::cout<<"G7-03 full161/seven actors/actor1 heal-only/party remove/failed213 retry/actual score PASS\n";
}
void audio(const CombatContent& c,const std::filesystem::path& root){
 const auto path=root/"short-pcm.wav";std::vector<std::int16_t> samples(4410,0);sf::SoundBuffer pcm;need(pcm.loadFromSamples(samples.data(),samples.size(),1,44100,{sf::SoundChannel::Mono}),"Audio fixture PCM creation failed");need(pcm.saveToFile(path),"PCM fixture save failed");
 {PredecodedMusic track(path,MusicLoop{0,0,4410},true);need(!track.firstPassComplete(),"Unplayed music has completed");track.play();std::this_thread::sleep_for(std::chrono::milliseconds(25));need(!track.firstPassComplete(),"Music reported first pass before its samples consumed");for(int n=0;n<100&&!track.firstPassComplete();++n){std::this_thread::sleep_for(std::chrono::milliseconds(10));}need(track.firstPassComplete(),"Actual loop boundary did not report first pass");track.stop();}
 {PredecodedMusic track(path,std::nullopt,true);need(!track.firstPassComplete(),"New track inherited first pass");track.play();track.stop();std::this_thread::sleep_for(std::chrono::milliseconds(150));need(!track.firstPassComplete(),"Stopped track published delayed completion");}
 sf::Font font;need(font.openFromFile(c.maps.begin()->second.fontPath),"Font unavailable");CombatPresentation p(c,font);const auto& profile=c.catalog.at("epilogue").at("credits_music");const auto sound=readSound(c.cue(profile.at("music").get<std::string>()).at("sound"));PresentationEvent event{PresentationEvent::Kind::Music,profile.at("music"),sound,{},0};event.musicPlayback=MusicPlaybackIdentity{100,1,sound.resource};p.update({event});need(p.musicPlaying()&&!p.musicFirstPass(),"Actual source music not played");p.update({{PresentationEvent::Kind::SceneChanged,{},{},{},0}});need(!p.musicFirstPass(),"Music result survived map change");p.update({event});p.update({{PresentationEvent::Kind::StopMusic,{},{},{},0}});need(!p.musicPlaying()&&!p.musicFirstPass(),"Stopped presentation retained result");
 PresentationEvent picture{PresentationEvent::Kind::Cue,"m211-e1-p1-c10",{}, {},0};picture.resolvedPicture=ResolvedPicture{{160,1680},0,0};p.update({picture});picture.id="m211-e1-p1-c12";picture.resolvedPicture=ResolvedPicture{{160,240},0,715};p.update({picture});
 std::cout<<"G7-03 actual PCM consumption/new instance/stop/map disposal/real154s track preparation/resolved credits no raw xy PASS\n";
}
void natural(const CombatContent& c,const std::filesystem::path& old,const std::filesystem::path& root){
 std::filesystem::create_directories(root/"slots");std::ofstream(root/"slots/slot-01.json")<<read(old);G2NaturalDriver d(c,root,"public-inputs.jsonl",quiet);d.game.configureSaveDirectory(root/"slots");DoraRender render(d.game,c,root/"frames");std::optional<MusicPlaybackIdentity> token;std::chrono::steady_clock::time_point musicStart{};int creditsTicks=0,musicAck=-1,fade=-1,stop=-1,victories=0;std::map<int,int> mapEntries;std::map<std::string,int> cues;std::vector<double> creditsY;int oldMap=0;bool sawSummer=false;int crossStart=-1,crossEnd=-1;int lastLog=-1;int victoryDamage=-1,victoryHp=-1;
 d.observe=[&](const PresentationEvent& event){render.pending.push_back(event);++cues[event.id];if(event.musicPlayback){token=event.musicPlayback;musicStart=std::chrono::steady_clock::now();}
  if(event.resolvedPicture&&event.id.starts_with("m211-e1-p1-")){creditsY.push_back(event.resolvedPicture->position.y);}
  if(event.id=="m211-e2-p1-c3"){fade=creditsTicks;}if(event.kind==PresentationEvent::Kind::StopMusic&&d.game.mapId()==211){stop=creditsTicks;}
  if(d.game.mapId()==210&&event.kind==PresentationEvent::Kind::Sound&&event.sound.resource==c.cue("m209-e10-p10-c46").at("sound").at("resource").get<std::string>()){crossStart=d.ticks;}if(event.id=="m209-e10-p10-c61"){crossEnd=d.ticks;}
 };
 d.afterTick=[&]{
  render.tick();if(auto pass=render.presentation->musicFirstPass()){if(d.game.reportMusicFirstPass(*pass)){musicAck=creditsTicks;std::cout<<"Real credits first audio pass at tick="<<musicAck<<'\n';}}
  if(auto victory=d.game.takeFinalBossVictory()){need(victory->scene==d.game.generation()&&victory->object=="m209-e10"&&victory->page==10,"Natural victory identity incorrect");++victories;victoryDamage=d.game.session().variable(89);victoryHp=d.game.session().hp();render.frame("natural-real-victory");}
  if(victories&&d.game.mapId()==209){need(d.game.session().variable(89)==victoryDamage&&d.game.session().hp()==victoryHp,"Winner accepted further attacks or took damage");}
  if(d.game.mapId()!=oldMap){++mapEntries[d.game.mapId()];oldMap=d.game.mapId();std::cout<<"Natural03 map="<<oldMap<<" tick="<<d.ticks<<" HP="<<d.game.session().hp()<<'\n';}
  if(oldMap==210&&crossEnd>=0){render.frame("natural-cross-map-tail");}
  if(oldMap==207&&d.game.session().flag(582)&&d.game.dialogue().view().lines.size()>1&&d.game.dialogue().view().lines.back().size()>8){need(d.game.panorama()&&d.game.panorama()->definition.speedX==-7,"Natural final cockpit -7 background absent");render.frame("natural-final-cockpit-dialogue");}
  if(oldMap==211){++creditsTicks;if(creditsTicks==4500){render.frame("natural-credit-pixels");}if(creditsTicks/1800!=lastLog){lastLog=creditsTicks/1800;std::cout<<"Natural credits ticks="<<creditsTicks<<" audioPlaying="<<render.presentation->musicPlaying()<<'\n';}}
  if(oldMap==212&&d.game.dialogue().view().lines.size()>1&&d.game.dialogue().view().lines.back().size()>8){sawSummer=true;render.frame("natural-summer-seven-actors");}
  if(oldMap==213&&cues["m213-e1-p1-c149"]>0){render.frame("natural-actual-score");}
 };
 d.load();need(d.game.mapId()==204&&d.game.session().flag(578),"Real G6 Save68 did not restore won platform");const auto inputStats=d.game.session().endingScoreInput();d.walk({14,6});d.move(Direction::Up);d.settle();need(d.game.mapId()==206,"Natural elevator did not enter206");d.walk({30,8});d.move(Direction::Right);d.tick(key());for(int n=0;n<25000&&(d.game.mapId()!=209||!d.game.session().flag(581)||d.game.foreground()!=ForegroundKind::None);++n){d.tick(n%2==0?key():InputFrame{});}need(d.game.mapId()==209&&d.game.session().flag(581),"Natural cockpit did not reach actual final battle");
 d.walk({13,9});InputFrame facing;facing.buttons[int(InputAction::Weapon)]={true,true,false};d.tick(facing);for(int n=0;n<30;++n){InputFrame turn;turn.buttons[int(InputAction::Weapon)].held=true;turn.movement=Direction::Left;d.tick(turn);}need(d.game.world().player().motion.facing()==Direction::Left,"Natural final facing failed");
 for(int n=0;n<40000&&!d.game.endingClearRequest();++n){
  need(d.game.hasGame()&&d.game.session().hp()>0&&d.game.mode()==ApplicationMode::Playing,"Natural final chain died or left playing");InputFrame in;const bool final=d.game.mapId()==209&&!d.game.session().flag(582);
  if(final){in.buttons[int(InputAction::Attack)]={true,true,false};in.buttons[int(InputAction::Weapon)].held=true;in.buttons[int(InputAction::Confirm)].pressed=n%8==0;}
  else if(n%2==0){in=key();}
  if(d.game.mapId()==211&&token){const auto deadline=musicStart+std::chrono::microseconds(static_cast<long long>(creditsTicks)*1000000/60);std::this_thread::sleep_until(deadline);}
  d.tick(in);if(n%1500==0&&final){std::cout<<"Natural final n="<<n<<" HP="<<d.game.session().hp()<<" damage="<<d.game.session().variable(89)<<" body="<<d.game.session().variable(203)<<" hits="<<d.game.session().variable(384)<<" main="<<d.game.session().equipped(1)<<'\n';}
 }
 need(victories==1&&mapEntries[210]==1&&mapEntries[211]==1&&mapEntries[212]==1&&mapEntries[213]==1&&mapEntries[214]==1&&sawSummer,"Complete natural victory/epilogue route absent");need(d.game.endingClearRequest()&&d.game.session().exportState().party.empty()&&!d.game.playerVisible(),"Natural real score/Clear pending/empty party absent");need(cues["m209-e10-p10-c54"]==1&&crossStart>=0&&crossEnd>crossStart&&cues["m211-e1-p1-c12"]==1&&cues["m211-e1-p1-c14"]==1&&creditsY==std::vector<double>({1680,240,-1200}),"Cross-map animation/full credits omitted: start="+std::to_string(crossStart)+" end="+std::to_string(crossEnd)+" animation="+std::to_string(cues["m209-e10-p10-c54"])+" credits="+Json(creditsY).dump());need(musicAck>0&&fade>=musicAck&&stop-fade==60,"Actual source audio first pass independent fade/Wait10/OFF failed");const auto stats=d.game.session().endingScoreInput();need(stats.saveCount==inputStats.saveCount&&stats.retryCount==inputStats.retryCount&&stats.sprayCount==inputStats.sprayCount,"Ending changed real prior statistics");render.frame("natural214-real-unlocks");
 std::ofstream(root/"natural-result.json")<<Json{{"kind","public-input actual G6 direct Save68->cargo->final combat->complete epilogue->real score214"},{"natural_entry",true},{"steps",d.ticks},{"victories",victories},{"map_entries",mapEntries},{"credit_positions",creditsY},{"credit_ticks",creditsTicks},{"audio_first_pass_tick",musicAck},{"audio_fade_tick",fade},{"audio_stop_tick",stop},{"cross_map_start",crossStart},{"cross_map_end",crossEnd},{"rank",d.game.session().variable(381)},{"stats",{{"saves",stats.saveCount},{"retries",stats.retryCount},{"sprays",stats.sprayCount}}},{"clear_saved",false},{"cues",cues}}.dump(2)<<'\n';
 std::cout<<"G7-03 actual natural G6 Save68/cockpit/final battle/all epilogue/source audio/real score PASS steps="<<d.ticks<<'\n';
}
}
int main(int argc,char** argv){try{std::cout.setf(std::ios::unitbuf);need(argc==3||argc==4,"Expected package/evidence optional actual G6 save");const auto c=loadCombatContent(argv[1]);const std::filesystem::path root=argv[2];std::filesystem::create_directories(root);if(argc==4&&std::string(argv[3])=="media"){victoryTail(c);readingRestore(c);audio(c,root);}else if(argc==4){natural(c,argv[3],root);}else{party(c);victoryTail(c);readingRestore(c);credits(c,root);summer(c,root);audio(c,root);}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
