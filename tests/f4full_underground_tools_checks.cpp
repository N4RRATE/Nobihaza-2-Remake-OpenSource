#include "full_content.h"
#include "save_snapshot.h"
#include "scene_presentation.h"
#include "enemy_controller.h"
#include "enemy_host.h"
#include "picture_tone.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace{
void require(bool ok,const char* reason){if(!ok){throw std::runtime_error(reason);}}
InputFrame confirm(){InputFrame input;input.buttons[static_cast<int>(InputAction::Confirm)]={true,true,false};return input;}
void tick(CombatScene& scene,int ticks,const InputFrame& input={}){while(ticks--){scene.update(input);}}
CombatEntry entry(const CombatContent& c,int map,TilePosition tile,Direction direction=Direction::Up){
    auto e=c.entries.at("defeated-claimed");e.map=map;e.playerTile=tile;e.direction=direction;e.positions.clear();e.pages.clear();e.directions.clear();e.through.clear();
    e.session.flags[1]=e.session.flags[18]=true;for(int fact:{10,13,15,199,200,201,202,227,228,229,231,585}){e.session.flags[fact]=false;}return e;
}
CombatContent isolated(const CombatContent& c){
    auto result=c;result.enemies.clear();
    for(auto& flow:result.catalog.at("flows")){if(flow.value("trigger",std::string{})=="autorun"){flow["trigger"]="call-only";}}
    return result;
}
void finish(CombatScene& scene){for(int i=0;i<12000&&scene.foreground()!=ForegroundKind::None;++i){scene.update(confirm());}require(scene.foreground()==ForegroundKind::None,"Source tool or pickup flow completes its bounded presentation");}
void decline(CombatScene& scene){for(int i=0;i<12000&&scene.foreground()!=ForegroundKind::None;++i){auto input=confirm();if(scene.inputPromptView().phase!=DialoguePhase::Hidden){input={};input.buttons[static_cast<int>(InputAction::Cancel)]={true,true,false};}scene.update(input);}require(scene.foreground()==ForegroundKind::None,"Source declined fuse flow completes");}
bool sample(const std::vector<PresentationEvent>& events,const std::string& cue,GridPosition expected){
    return std::any_of(events.begin(),events.end(),[&](const auto& e){return e.id==cue&&e.sampledScreenPosition&&std::abs(e.sampledScreenPosition->x-expected.x)<.001&&std::abs(e.sampledScreenPosition->y-expected.y)<.001;});
}
}
int main(int argc,char** argv){try{
    if(argc!=2){return 2;}const auto content=loadCombatContent(argv[1]);const auto& underground=content.full->at("runtime").at("underground");
    require(pictureTonePixel({255,0,0,255},{50,160,100,100})==std::array<std::uint8_t,4>{128,153,0,255},"Original hard-light formula adds green to a red source pixel at 160 percent");
    require(pictureTonePixel({255,0,0,71},{50,150,100,100})==std::array<std::uint8_t,4>{128,129,0,71},"Later green150 differs from green160 and preserves source alpha");
    require(pictureTonePixel({255,0,0,255},{100,100,100,0})==std::array<std::uint8_t,4>{76,76,76,255},"Original fixed-point luminance produces the source saturation-zero pixel");
    require(pictureTonePixel({81,149,234,0},{100,100,100,100})==std::array<std::uint8_t,4>{81,149,234,0},"Neutral source tone preserves all channels including transparent pixels");
    require(content.catalog.at("underground_lighting").size()==11,"All eleven source light pages compile into finite sampling profiles");
    const auto& blood=underground.at("ordinary");
    for(const auto& [id,effect]:blood.at("effects").items()){
        const auto& tones=content.media.at("effects").at(id).at("stage_tone_percent");const bool initial=id.ends_with("-101");
        require(tones.size()==4&&tones.at(0)==nlohmann::json::array({50,initial?160:150,100,100}),"CE15/16 green blood preserves its distinct first source tone");
        for(int stage=1;stage<4;++stage){require(tones.at(stage)==nlohmann::json::array({50,150,100,100}),"Later green blood stages retain the original source tone");}
    }
    require(!content.media.at("effects").at("enemy-slot-15-blood-1").contains("stage_tone_percent"),"New colored effect capability preserves the old white-default blood presentation");
    for(int variation:{0,1,2,3,4,5}){
        auto candidate=content;auto bad=underground;
        for(const auto& [id,cue]:blood.at("presentation_cues").items()){candidate.cues.erase(id);}
        for(const auto& [id,effect]:blood.at("effects").items()){candidate.media["effects"].erase(id);}
        const auto id=bad.at("ordinary").at("effects").begin().key();
        if(variation==0){bad["ordinary"]["effects"][id]["stage_tone_percent"][0][1]=999;}
        if(variation==1){bad["ordinary"]["effects"][id]["image"]="missing-source-image";}
        if(variation==2){candidate.media["effects"][id]=blood.at("effects").at(id);}
        if(variation>=3){for(const auto& [enemy,row]:underground.at("encounters").items()){candidate.enemies.erase(enemy);}}
        if(variation==3){bad["encounters"]["m66-e2"]["runtime_profile"]["body_cache"]["source_object"]="unknown-manager";}
        if(variation==4){bad["flows"]["m75-e9-p1"]["steps"][0]["condition"]["kind"]="item";}
        if(variation==5){bad["encounters"]["m66-e2"]["runtime_profile"]["body_cache"]["register_variable"]=999;}
        const std::array<const char*,6> errors{"blood tone value","blood image","Conflicting underground blood effect","canine body manager","environment condition","canine body variable"};
        bool rejected=false;try{installUndergroundContent(candidate,bad);}catch(const std::exception& error){rejected=std::string(error.what()).find(errors.at(variation))!=std::string::npos;}
        require(rejected,"Malformed tone, missing media, duplicate identity and unsupported cache or environment definitions are rejected by the finite installer");
    }
    int pursuers=0,ordinary=0;for(const auto& [id,row]:underground.at("encounters").items()){
        require(content.enemies.contains(id),"Every typed underground encounter is actually registered");
        if(row.at("type")=="pursuer"){++pursuers;require(content.enemies.at(id).rules.at("stop_fact")==585,"Formal underground stop page retains S585 instead of the training S162");}else{++ordinary;}
    }require(pursuers==7&&ordinary==9,"Seven pursuers and nine ordinary source enemies have distinct identities");
    for(int map:{66,67,68,73,74,75,76,77}){require(!content.pendingMapTickets.contains(map),"Only completed ticket96 maps release their gate after all typed consumers are installed");}
    require(content.pendingMapTickets.contains(70)?content.pendingMapTickets.at(70)==98:content.catalog.at("flows").contains("guard-first-intro"),"Drainage gate matches the installed first-guard consumer");
    auto c=isolated(content);
    for(bool pursuing:{false,true}){
        auto e=entry(c,75,{6,6});e.session.flags[227]=pursuing;GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});
        const auto& branch=underground.at("flows").at("m75-e9-p1").at("steps").at(0);
        const auto cue=branch.at(pursuing?"else":"then").at(0).at("cue").get<std::string>();const auto sound=readSound(c.cue(cue).at("sound"));
        const auto events=room.takePresentation();require(std::any_of(events.begin(),events.end(),[&](const auto& event){return event.kind==PresentationEvent::Kind::Music&&event.sound==sound;}),"Original parallel music selects the current pursuit fact in both source branches");
        room.update({});const auto after=room.takePresentation();require(std::none_of(after.begin(),after.end(),[](const auto& event){return event.kind==PresentationEvent::Kind::Music;}),"Erased source music event does not restart each update");
    }
    for(int sourcePage:{1,3}){
        auto e=entry(c,77,{6,6});e.session.flags[73]=true;e.session.flags[226]=sourcePage==3;GameSession s(c.items,e.session,c.facts);s.forgetSkill(3);s.forgetSkill(4);CombatScene room(c,s,e);room.update({});
        require(s.skills().contains(3)&&!s.skills().contains(4),"Both source environment pages unconditionally learn mixing3 even when mixing4 was unknown");
        s.forgetSkill(3);s.learnSkill(4);room.update({});require(!s.skills().contains(3)&&s.skills().contains(4),"Erased source skill event does not reapply its transition");
    }
    {
        auto e=entry(c,68,{6,6});e.session.flags[233]=true;e.session.flags[234]=false;GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});
        require(!s.flag(234),"Original environment completion fact waits for its forced route barrier");
        tick(room,1200);require(s.flag(234),"Parallel source route completes before publishing the original completion fact");
    }
    for(const auto* id:{"m66-e2","m67-e4"}){
        auto dogContent=c;const auto definition=content.enemies.at(id);dogContent.enemies.emplace(id,definition);
        auto e=entry(dogContent,definition.map,{12,10});e.positions[id]={6,6};e.session.mainWeapon=0;e.session.baseAttack=35;e.session.flags[47]=false;e.session.variables[35]=0;
        GameSession s(dogContent.items,e.session,dogContent.facts);CombatScene room(dogContent,s,e,[](int low,int high){return low<=5&&high>=5?5:low;});auto dog=makeEnemyBehavior(room,definition);dog->update({});
        const auto p=findCharacter(room.world(),id).motion.position();const auto hit=dog->receiveAttack({{(p.x+.5)*16,(p.y+.5)*16},{0,0},AttackKind::Handgun,2});
        require(hit.processed&&hit.damage==30&&s.variable(49)==5,"Original Actor26 defense and receiver2 alias work across both underground dog maps");
        room.takePresentation();dog->receiveAttack({{(p.x+.5)*16,(p.y+.5)*16},{0,0},AttackKind::Handgun,4});dog->update({});
        const auto& profile=definition.rules.at("runtime_profile");const auto prefix=profile.at("page_roles").at("knocked_back").at("presentation").get<std::string>();int command=17;
        if(profile.at("cue_commands").contains("knocked_back")){command=profile.at("cue_commands").at("knocked_back").at("17");}const auto sound=readSound(dogContent.cue(prefix+"-c"+std::to_string(command)).at("sound"));
        const auto events=room.takePresentation();require(std::any_of(events.begin(),events.end(),[&](const auto& event){return event.kind==PresentationEvent::Kind::Sound&&event.sound==sound;}),"Typed dog knockback consumes the original sound command without duplicating media");
    }
    for(const auto* id:{"m66-e2","m66-e4","m67-e2","m67-e4"}){
        const auto& definition=content.enemies.at(id);const auto& cache=definition.rules.at("runtime_profile").at("body_cache");
        auto e=entry(c,definition.map,{12,10});e.positions[id]={6,6};e.session.flags[definition.deadFlag]=false;e.session.flags[definition.rules.at("activation_fact").get<int>()]=true;
        GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);auto dog=makeEnemyBehavior(room,definition);dog->update({});EnemyHost host(room,id);
        for(int state:{0,1,2}){
            const int stateVariable=definition.rules.at("source_slots").at("state");s.setVariable(stateVariable,state);dog->updateParallel();
            auto expected=room.sampleActorScreen(id);expected.x+=cache.at("center_offset_pixels").at(0).get<int>();expected.y+=cache.at("center_offset_pixels").at(1).get<int>();
            require(s.variable(cache.at("register_variable"))==cache.at("register_event")&&s.variable(cache.at("screen_variables").at(0))==static_cast<int>(expected.x)&&s.variable(cache.at("screen_variables").at(1))==static_cast<int>(expected.y),"All three original manager pages publish their declared event and screen sample on both dog maps");
            const auto tile=host.actor().motion.tile();require(s.variable(cache.at("tile_variables").at(0))==tile.x&&s.variable(cache.at("tile_variables").at(1))==tile.y,"Original manager stores tile coordinates beside its screen workspace");
        }
        require(s.variable(cache.at("half_variables").at(0))==8&&s.variable(cache.at("half_variables").at(1))==5,"Original roaming page publishes its smaller cached half size");dog->activateState(1);dog->updateParallel();
        require(s.variable(cache.at("half_variables").at(0))==8&&s.variable(cache.at("half_variables").at(1))==8,"Original pursuit page updates the cached half size");
        auto actor=host.actor();actor.motion=CharacterMotion(actor.motion.tile(),actor.motion.facing(),actor.motion.speed());require(actor.motion.beginJump({1,0}),"Source cache jump fixture starts");for(int i=0;i<4;++i){actor.motion.advance();}host.setActor(actor);dog->updateParallel();
        auto expected=room.sampleActorScreen(id);expected.y-=8;require(actor.motion.jumpHeight()>0&&s.variable(cache.at("screen_variables").at(1))==static_cast<int>(expected.y),"Source screen cache subtracts jump height exactly once");
        const auto cachedY=s.variable(cache.at("screen_variables").at(1));s.setFlag(18,false);actor.motion.advance();host.setActor(actor);dog->updateParallel();require(s.variable(cache.at("screen_variables").at(1))==cachedY,"Inactive manager pages retain the previous source sample");s.setFlag(18,true);
        bool unknown=false;try{dog->receiveAttack({{-10000,-10000},{0,0},AttackKind::Grenade,9,0,false,0,AttackPhase::ResolvedContact});}catch(const std::exception& error){unknown=std::string(error.what())=="Unsupported canine receiver";}require(unknown,"Resolved source contact does not invent an unsupported canine receiver9");
        require(!dog->receiveAttack({{-10000,-10000},{0,0},AttackKind::Grenade,6}).contact,"Ordinary attack still rejects distant live geometry");
        const auto contact=dog->receiveAttack({{-10000,-10000},{0,0},AttackKind::Grenade,6,0,false,0,AttackPhase::ResolvedContact});require(contact.processed&&dog->accumulatedDamage()==2000,"Resolved source contact retains receiver6 damage while accepting already resolved geometry");
        for(int i=0;i<1000&&!s.flag(definition.deadFlag);++i){dog->update({});}require(s.flag(definition.deadFlag),"Original bounded death presentation reaches its source dead fact");dog->updateParallel();
        require(s.variable(cache.at("screen_variables").at(0))==0&&s.variable(cache.at("screen_variables").at(1))==0&&s.variable(cache.at("register_variable"))==cache.at("register_event")&&s.variable(cache.at("half_variables").at(1))==8,"Death clears only screen coordinates and retains original event registration and half size");
        require(!dog->receiveAttack({{}, {0,0},AttackKind::Grenade,9,0,false,0,AttackPhase::ResolvedContact}).processed,"Dead source dog rejects contacts before receiver dispatch");
        if(definition.map==67){
            for(int fact:{188,189,190,191,192}){s.setFlag(fact,true);}s.setFlag(227,true);s.setFlag(226,false);s.setFlag(160,false);s.setFlag(21,false);auto pursuer=makeEnemyBehavior(room,content.enemies.at("m67-e13"));pursuer->updateParallel();
            require(s.variable(90)==13&&!dog->receiveAttack({{}, {0,0},AttackKind::Grenade,9,0,false,0,AttackPhase::ResolvedContact}).processed,"Post-fuse source pursuer replaces slot1 registration while both original dogs remain dead");
        }
    }
    // Scene fixtures isolate tool/save behavior from the separately verified enemy and entry story consumers.
    {
        auto e=entry(c,68,{27,6});GameSession s(c.items,e.session,c.facts);s.grant(76,1);CombatScene room(c,s,e);room.update({});
        require(s.count(76)==0&&s.count(77)==1,"Original hook contact converts one inactive tool to its usable form");
        auto request=room.requestToolUse();require(bool(request),"The source use tile grants the existing menu request identity");room.cancelToolUse();
        require(!room.useTool(*request)&&!s.flag(229),"Cancelled tool request cannot commit the source request switch");
        request=room.requestToolUse();InputFrame move;move.movement=Direction::Down;room.update(move);tick(room,60);
        require(s.count(76)==1&&s.count(77)==0&&!room.requestToolUse()&&!room.useTool(*request),"Leaving the original contact area restores the inactive hook and rejects its stale request");
        auto returnEntry=entry(c,68,{27,6});CombatScene returned(c,s,returnEntry);returned.update({});auto use=returned.requestToolUse();require(use&&returned.useTool(*use),"A new scene grants and consumes its own current tool request");finish(returned);
        require(s.flag(231)&&!s.flag(229)&&s.count(76)==0&&s.count(77)==0&&s.flag(1)&&s.flag(18),"Original hook installation consumes the tool, commits completion and restores control");
        require(!returned.useTool(*use),"Completed request cannot install the hook twice");
    }
    for(bool defeated:{false,true}){
        auto power=c;power.pendingMapTickets.erase(69);const auto origin=power.objects.at("m69-e12").tile;auto e=entry(power,69,origin);e.session.flags[226]=defeated;e.session.flags[235]=e.session.flags[236]=e.session.flags[237]=false;
        GameSession s(power.items,e.session,power.facts);s.grant(78,1);CombatScene room(power,s,e);room.update({});require(s.count(79)==1&&s.count(78)==0,"Fuse source conversion area works across a different origin and target event");
        const auto request=room.requestToolUse();require(request&&request->target=="m69-e5"&&room.useTool(*request),"Actual source high page selects fuse flow2 or5 from the current completion fact");decline(room);
        require(s.count(79)==0&&s.count(78)==0&&s.flag(237)&&!s.flag(236),"Original decline still consumes the fuse and commits the repeat-interaction page");
        require(!room.requestToolUse()&&!room.useTool(*request),"Declined fuse cannot reuse its old menu request");
        const auto target=power.objects.at("m69-e5").tile;auto retryEntry=entry(power,69,{target.x,target.y+1});CombatScene retry(power,s,retryEntry);retry.update(confirm());
        require(retry.foreground()==ForegroundKind::Content,"Original source action page permits a manual retry after the consumed fuse");finish(retry);
        require(defeated?s.flag(236):s.flag(227)&&s.eventTimer().running,"Manual retry uses the appropriate original pre- or post-defeat power branch without another fuse");
    }
    {
        auto e=entry(c,66,c.objects.at("m66-e22").tile);e.session.flags[236]=true;e.session.flags[275]=e.session.flags[276]=false;GameSession s(c.items,e.session,c.facts);s.grant(83,1);CombatScene room(c,s,e);room.update({});
        auto request=room.requestToolUse();require(s.count(84)==1&&request&&request->target=="m66-e20","Red card conversion event requests its separate original target source page");room.cancelToolUse();require(!room.useTool(*request)&&!s.flag(275),"Cancelled cross-event request cannot commit its original target switch");
        s.setFlag(236,false);room.update({});require(!room.requestToolUse(),"Restoring the inactive-item source page for validation retains the original power prerequisite");s.setFlag(236,true);room.update({});request=room.requestToolUse();require(request&&room.useTool(*request),"Restored source prerequisite grants a new valid card request");finish(room);
        require(s.flag(276)&&s.count(83)==0&&s.count(84)==0&&!room.requestToolUse()&&!room.useTool(*request),"Original red card opening consumes one card and cannot replay its completed request");
    }
    {
        const auto object=c.objects.at("m76-e5").tile;auto e=entry(c,76,{object.x,object.y+1});e.session.hp=30;e.session.pursuerHealth=117;
        for(int fact:{188,189,190,191,192}){e.session.flags[fact]=false;}
        GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update(confirm());finish(room);
        require(s.hp()==30&&s.pursuerHealth()==500&&s.count(78)==1&&s.flag(202)&&s.flag(227),"Fuse source pickup restores Actor30 health while preserving wounded player health");
        for(int fact:{188,189,190,191,192}){require(s.flag(fact),"All five original clearing facts commit at the fuse pickup step");}
        room.update(confirm());finish(room);require(s.count(78)==1,"Higher source page prevents duplicate fuse pickup");
        const auto before=s.exportState();bool refused=false;
        try{ScenePresentation(room).validateSequence(nlohmann::json::array({{{"kind","flag"},{"id",202},{"value",false}},{{"kind","restore_actor"},{"actor",999},{"health","full"},{"states","clear"}}}),"m76-e5");}catch(const std::exception&){refused=true;}
        require(refused&&s.exportState()==before,"Unknown actor is rejected before any valid prefix can mutate gameplay");
    }
    {
        const auto object=c.objects.at("m77-e11").tile;auto e=entry(c,77,object);e.session.magazines[21]=3;GameSession s(c.items,e.session,c.facts);s.grant(21,1);
        const int a=s.count(5),b=s.count(21),d=s.count(43);CombatScene room(c,s,e);
        for(int i=1;i<=2;++i){room.update(confirm());finish(room);require(s.count(5)==a+i&&s.count(21)==b+i&&s.count(43)==d+i&&s.magazine(21)==3,"Original repeated underground supply grants all three items without refilling an owned magazine");}
    }
    for(int page:{2,4}){
        const auto object=c.objects.at("m77-e6").tile;auto e=entry(c,77,{object.x,object.y+1});e.session.flags[227]=page==4;e.session.flags[585]=page==4;
        GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update(confirm());const auto request=room.takeSaveRequest();require(bool(request),"Both reviewed source pages permit a normal public save request");
        const auto snap=captureSave(c,room,*request,{});auto restored=prepareRestore(c,decodeSave(c,encodeSave(c,snap)));
        require(restored.scene->mapId()==77&&restored.session->flag(585)==(page==4)&&snap.summary.label.find("/NORMAL/")!=std::string::npos,"New save point restores both page contexts using unchanged save schema and source slot label");
        auto invalid=snap;invalid.session.worldFlags[c.facts.switches.at(585)]=false;invalid.session.worldFlags[c.facts.switches.at(227)]=true;bool rejected=false;try{validateSave(c,invalid);}catch(const std::exception&){rejected=true;}
        require(rejected,"Higher blocked page is rejected during save validation");
    }
    for(int variation:{0,1,2}){
        const auto object=c.objects.at("m77-e6").tile;auto e=entry(c,77,{object.x,object.y+1},variation==0?Direction::Down:Direction::Up);
        if(variation==1){e.session.party={2};}if(variation==2){e.session.flags[227]=true;}
        GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update(confirm());require(!room.takeSaveRequest(),"Wrong direction, missing source party member and pursuit-blocked page cannot request a save");
    }
    for(const auto [map,page]:{std::pair{66,1},std::pair{66,2},std::pair{75,1}}){
        auto e=entry(c,map,{6,6},Direction::Down);e.session.flags[199]=map==66&&page==1;e.session.flags[200]=map!=66||page==2;
        GameSession s(c.items,e.session,c.facts);CombatScene room(c,s,e);room.update({});const auto events=room.takePresentation();
        const auto light=std::find_if(c.catalog.at("underground_lighting").begin(),c.catalog.at("underground_lighting").end(),[&](const auto& row){return c.objects.at(row.at("object").template get<std::string>()).map==map&&row.at("page")==page;});
        require(light!=c.catalog.at("underground_lighting").end(),"Representative source light profile exists");const auto actor=light->at("actor").get<std::string>();const auto facing=findCharacter(room.world(),actor).motion.facing();const auto& data=light->at("directions").at(directionName(facing));
        auto expected=room.sampleActorScreen(actor);expected.x+=data.at("offset_pixels").at(0).get<int>();expected.y+=data.at("offset_pixels").at(1).get<int>();
        require(sample(events,data.at("show_cues").at(0),expected)&&sample(events,light->at("move_cues").at(0),expected),"NPC and player source lights sample camera-relative screen foot and original direction offset");
        ChoicePrompt question;question.options={MessageLine{"OK",{{MessageTokenKind::Text,U"OK",0}}}};const auto prompt=room.requestInput(question);room.update({});const auto paused=room.takePresentation();const auto firstMove=light->at("move_cues").at(0).template get<std::string>(),secondMove=light->at("move_cues").at(1).template get<std::string>();require(std::none_of(paused.begin(),paused.end(),[&](const auto& event){return event.id==firstMove||event.id==secondMove;}),"CN engine picture commands pause while the message window is active");room.cancelInput(prompt);room.update({});
        require(sample(room.takePresentation(),light->at("move_cues").at(0),expected),"Blocked light command resumes with its sampled source position");
    }
    std::cout<<"Underground typed installation, tools, fuse recovery, supply, save contexts and source lighting passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
