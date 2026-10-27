#pragma once
#include "g2_natural_driver.h"
#include "g4_truth_driver.h"
#include "content_hash.h"
#include <random>

namespace g8continue {
inline std::string bytes(const std::filesystem::path& path){std::ifstream input(path);g0natural::need(bool(input),"Missing natural Continue source");return {std::istreambuf_iterator<char>(input),{}};}
inline void natural(const CombatContent& c,int checkpoint,const std::filesystem::path& source,const std::filesystem::path& out,std::uint32_t seed=19202026,const std::function<void(G2NaturalDriver&)>& configure={},const std::function<void(G2NaturalDriver&)>& afterContinue={}){
    const auto raw=bytes(source);const auto disk=decodeSave(c,raw);
    g0natural::need(checkpoint>=218&&checkpoint<=220&&disk.scene.map==151,"Natural Continue requires a registered checkpoint and actual pre-Boss M151 disk");
    const GameSession initial(c.items,disk.session,c.facts);g0natural::need(!initial.flag(checkpoint==218?449:504)&&!initial.flag(586),"Natural Continue source already won or has an active checkpoint");
    std::cout<<"CONTINUE_NATURAL_BEGIN "<<disk.session.difficulty<<" map="<<checkpoint<<" seed="<<seed<<" source="<<source<<'\n';
    std::filesystem::create_directories(out);std::ofstream(out/"slot-01.json")<<raw;
    std::mt19937 random(seed);G2NaturalDriver r(c,out,"public-inputs.jsonl",[&](int lo,int hi){return std::uniform_int_distribution<int>(lo,hi)(random);});
    r.game.configureSaveDirectory(out);if(configure){configure(r);}
    if(checkpoint==218){
        r.load();r.reception();r.actObject("m159-e5");
        for(int n=0;n<6000&&r.game.mapId()==164;++n){InputFrame input;
            if(r.game.foreground()!=ForegroundKind::None||r.game.dialogue().active()){if(n%8==0){input=g0natural::key();}}
            else if(!r.game.world().player().motion.moving()){input.movement=g0natural::next(r.game.world(),{19,9});}r.tick(input);
        }
    }else if(checkpoint==219){g4truth::enter(r,true);}
    else{
        g4truth::winDora(r,true);
        for(int n=0;n<20000&&!r.game.inputPromptView().cursor;++n){r.tick(n%8==0?g0natural::key():InputFrame{});}
        g0natural::need(r.game.inputPromptView().cursor.has_value(),"Natural truth choice absent before 220");r.press();
        for(int n=0;n<30000&&(r.game.mapId()!=191||r.game.foreground()!=ForegroundKind::None);++n){r.tick(n%8==0?g0natural::key():InputFrame{});}
        g0natural::need(r.game.session().flag(504)&&r.game.session().flag(506)&&r.game.session().variable(627)==5,"Natural complete truth before 220 absent");
    }
    r.settle();auto& game=r.game;
    const int battle=continueCheckpoint(c,checkpoint).at("retry").at("map");
    g0natural::need(game.mapId()==battle&&game.session().flag(586)&&game.session().variable(283)==checkpoint,"Natural original checkpoint capture absent");
    const auto captured=game.session().exportState();const GameSession backup(c.items,captured,c.facts);
    const auto die=[&]{
        for(int n=0;n<12000&&!game.inputPromptView().cursor;++n){InputFrame input;
            if(game.dialogue().active()){if(n%8==0){input=g0natural::key();}}
            else if(game.session().hp()>0&&(game.session().flag(45)||game.session().flag(10))){if(n%2==0){input=g0natural::key(n%4?InputAction::Right:InputAction::Up);}}
            else if(checkpoint==218&&game.session().hp()>0&&!game.world().player().motion.moving()&&game.world().player().motion.tile()!=TilePosition{16,12}){input.movement=g0natural::next(game.world(),{16,12});}
            else if(game.session().hp()==0&&n%8==0){input=g0natural::key();}
            r.tick(input);g0natural::need(game.mode()==ApplicationMode::Playing,"Natural death returned to title before Continue choice");
            if(n%1000==0){std::cout<<"CONTINUE_NATURAL_DEATH "<<game.session().difficulty()<<" map="<<checkpoint<<" step="<<n<<" HP="<<game.session().hp()<<'\n';}
        }
        g0natural::need(game.session().hp()==0&&game.inputPromptView().cursor.has_value(),"Original enemy did not cause the natural Continue death");
    };
    die();r.tick(g0natural::key());
    for(int n=0;n<6000&&(game.session().hp()==0||game.foreground()!=ForegroundKind::None);++n){r.tick(n%8==0?g0natural::key():InputFrame{});}
    g0natural::need(game.mapId()==battle&&game.foreground()==ForegroundKind::None&&game.session().hp()==backup.variable(288)&&game.session().tp()==backup.variable(289),"Natural Yes did not restore saved HP/SP at the real battle entry");
    g0natural::need(game.session().difficulty()==disk.session.difficulty&&game.session().variable(630)==backup.variable(630)+1&&game.session().saveCount()==backup.saveCount()&&game.session().variable(399)==backup.variable(399),"Natural Yes changed identity/save counters or repeated V630");
    g0natural::need(game.session().exportState().magazines==captured.magazines&&game.session().variable(382)==backup.variable(382),"Natural Yes changed magazine/non-backup spray facts");
    for(int slot=1;slot<=4;++slot){g0natural::need(game.session().equipped(slot)==backup.variable(283+slot),"Natural Yes changed equipment backup");}
    g0natural::need(game.session().equipped(5)==0,"Natural Yes restored the fifth equipment slot");
    const int retries=game.session().variable(630);const int yesStep=r.ticks;std::cout<<"CONTINUE_NATURAL_YES "<<disk.session.difficulty<<" map="<<checkpoint<<" V630="<<retries<<" PASS\n";
    if(afterContinue){afterContinue(r);}
    die();g0natural::need(game.session().variable(630)==retries,"Natural repeat death incremented V630 before confirmation");r.tick(g0natural::key(InputAction::Down));r.tick({});r.tick(g0natural::key());
    for(int n=0;n<6000&&game.mode()!=ApplicationMode::Title;++n){r.tick(n%8==0?g0natural::key():InputFrame{});}
    g0natural::need(game.mode()==ApplicationMode::Title&&bytes(source)==raw&&bytes(out/"slot-01.json")==raw,"Natural No did not reach title or changed the disk");
    std::ofstream(out/"natural-result.json")<<Json{{"difficulty",disk.session.difficulty},{"checkpoint",checkpoint},{"source",source.string()},{"source_sha256",contentSha256(raw)},{"source_save_count",disk.session.saveCount},{"loaded_save_count",backup.saveCount()},{"saved_V399",backup.variable(399)},{"V630_before",backup.variable(630)},{"V630_after_Yes",retries},{"Yes_step",yesStep},{"total_steps",r.ticks},{"maps",r.maps},{"random_seed",seed},{"random_policy","ordinary uniform mt19937; no event/range overrides"},{"death_source","original enemy damage with public movement and recovery input"},{"runtime_state_injection",false},{"Yes",true},{"No_title",true},{"disk_unchanged",true}}.dump(2)<<'\n';
    std::cout<<"CONTINUE_NATURAL_NO "<<disk.session.difficulty<<" map="<<checkpoint<<" steps="<<r.ticks<<" PASS\n";
}
}
