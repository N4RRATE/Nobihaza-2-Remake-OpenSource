#include "combat_scene.h"
#include "game_session.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace{
void require(bool condition,const char* message){if(!condition){throw std::runtime_error(message);}}
InputFrame press(InputAction action){InputFrame result;result.buttons[static_cast<int>(action)]={true,true,false};return result;}
void tick(CombatScene& scene,int count,const InputFrame& input={}){while(count--){scene.update(input);}}
CombatEntry fixture(const CombatContent& content){
    auto entry=content.entries.at("post-pickup");entry.map=16;entry.playerTile={7,7};entry.direction=Direction::Right;
    entry.positions.clear();entry.pages.clear();entry.through.clear();entry.directions.clear();
    entry.session.flags[18]=true;entry.session.flags[1]=true;entry.session.flags[52]=false;return entry;
}
void raise(CombatScene& scene,InputAction action=InputAction::Weapon){scene.update(press(action));tick(scene,150);if(!scene.raised()){throw std::runtime_error("Real source action completes weapon posture: item "+std::to_string(scene.session().equipped(action==InputAction::Weapon?1:2)));}}
}
int main(int argc,char** argv){
    if(argc!=2){return 2;}
    try{
        const auto content=loadCombatContent(argv[1]);
        if(!content.full->at("runtime").contains("weapon_actions")){
            const auto base=fixture(content);GameSession state(content.items,base.session,content.facts);
            for(int weapon:{16,17,18,19,25}){require(content.items.at(weapon).stateOnly&&!state.canEquip(weapon,weapon==25?2:1),"Old content keeps all pending weapons unavailable");}
            std::cout<<"Old package state-only weapon admission checks passed; no new action behavior claimed\n";return 0;
        }
        require(mainCriticalThreshold(AttackKind::Ar)==0&&mainCriticalThreshold(AttackKind::GlAcid)==6,"CE11 preserves AR and GL critical thresholds");
        for(const auto sample:{AttackSample{{},{0,0},static_cast<AttackKind>(999)},AttackSample{{},{0,0},AttackKind::Knife,2,0,false,0,AttackPhase::Burning}}){
            bool rejected=false;try{validateAttackSample(sample);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Unknown weapons and incompatible phases remain rejected at the public attack boundary");
        }
        const auto fixed=[](int low,int high){return low<=5&&high>=5?5:low;};
        require(rollWeaponDamage(AttackKind::GlAcid,100,40,0,fixed).amount==100&&rollWeaponDamage(AttackKind::GlFlame,100,40,0,fixed).amount==60,"CE11 acid ignores defense while initial flame impact keeps it");
        const auto base=fixture(content);
        for(bool shoes:{false,true}){
            GameSession state(content.items,base.session,content.facts);const int agility=state.auxiliaryAttack();
            require(state.equip(11),"Original post-pickup handgun is equipped before testing fixed movement");
            if(shoes){state.grant(53,1);require(state.equip(53,4),"I53 is actual accessory equipment");}
            CombatScene scene(content,state,base);raise(scene);auto fixedMovement=press(InputAction::Cancel);fixedMovement.movement=Direction::Right;scene.update(fixedMovement);
            require(scene.world().player().motion.speed()==(shoes?32:16)&&state.auxiliaryAttack()==agility,"CE77 fixed movement changes speed3 to4 with I53 without changing actual agility");
        }
        GameSession ar(content.items,base.session,content.facts);ar.grant(19,1);require(ar.equip(19),"Admitted AR is equipped through the public domain");
        const int prior=ar.magazine(19);require(prior==30,"CN initial V59=30 is independent of old magazines");
        CombatScene arScene(content,ar,base);raise(arScene);
        auto held=press(InputAction::Attack);arScene.update(held);held.buttons[static_cast<int>(InputAction::Attack)].pressed=false;tick(arScene,40,held);
        require(ar.magazine(19)<prior-1&&ar.magazine(19)>0,"CE72 continuously held attack fires more than once through the real controller");
        const int committed=ar.magazine(19),handgun=ar.magazine(11);arScene.deactivate();tick(arScene,60,held);
        require(ar.magazine(19)==committed&&ar.magazine(11)==handgun,"Destroying the scene stops continued AR fire without refund or other-weapon consumption");
        ar.equip(0);require(ar.equip(19)&&ar.magazine(19)==committed,"AR re-equipment preserves its spent magazine");
        ar.grant(38,2);ar.grant(39,1);const int total=ar.magazine(19)+ar.count(38)+30*ar.count(39);ar.reload();
        require(ar.magazine(19)==30&&ar.magazine(19)+ar.count(38)+30*ar.count(39)==total,"AR box and loose reload preserve the original 30-round accounting");
        for(int weapon:{16,17,18}){
            GameSession gl(content.items,base.session,content.facts);gl.grant(16,1);require(gl.equip(16),"GL enters as the original single loader");
            gl.grant(35,3);gl.grant(36,3);gl.grant(37,3);
            if(weapon!=16){require(gl.selectWeaponAmmunition(content.items.at(weapon).looseItem,{16,17,18})==ItemUseResult::Applied,"Public original ammunition item selects the tested GL form");}
            const int ammo=content.items.at(weapon).looseItem,reserve=gl.count(ammo);
            CombatScene scene(content,gl,base);raise(scene);scene.update(press(InputAction::Attack));
            require(gl.magazine(weapon)==0&&gl.count(ammo)==reserve,"CE69 firing commits the loaded round before later auto reload");
            tick(scene,29);require(gl.magazine(weapon)==0,"Recoil waits precede the separate CE69 reload commit");
            tick(scene,150);require(gl.magazine(weapon)==1&&gl.count(ammo)==reserve-1,"CE69 later reload consumes exactly one matching reserve round");
            scene.deactivate();tick(scene,30);require(gl.magazine(weapon)==1&&gl.count(ammo)==reserve-1,"Cancelling submitted auto reload never refunds it");
        }
        for(bool blocked:{false,true}){
            auto terrainContent=content;auto& map=terrainContent.maps.at(base.map);
            std::fill(map.passageMasks.begin(),map.passageMasks.end(),15);
            if(blocked){const auto next=adjacentTile(base.playerTile,base.direction);map.passageMasks.at(next.y*map.width+next.x)=0;}
            GameSession gl(terrainContent.items,base.session,terrainContent.facts);gl.grant(16,1);gl.equip(16);
            CombatScene scene(terrainContent,gl,base);raise(scene);scene.takePresentation();scene.update(press(InputAction::Attack));tick(scene,60);
            const auto events=scene.takePresentation();const auto& binding=content.catalog.at("weapon_bindings").at("16");
            const auto moveCue=binding.at("flight_move_cue").get<std::string>();bool moved=false,shown=false;
            for(const auto& event:events){if(event.kind==PresentationEvent::Kind::Cue&&event.id==moveCue){shown=true;moved|=event.position.x>base.playerTile.x;}}
            require(blocked?!moved:shown&&moved,"Map72 E1P5 uses tile passage while its layer2 projectile skips blocked forward attempts");
            if(blocked){
                const auto explosion=binding.at("explosion_cue_order").get<std::vector<std::string>>();
                require(std::any_of(events.begin(),events.end(),[&](const auto& event){return event.kind==PresentationEvent::Kind::Cue&&event.position.x==base.playerTile.x&&std::find(explosion.begin(),explosion.end(),event.id)!=explosion.end();}),"A terrain-blocked GL completes its nine attempts and explodes at the stopped position");
            }
        }
        {
            GameSession gl(content.items,base.session,content.facts);gl.grant(16,1);gl.grant(35,1);gl.equip(16);
            CombatScene scene(content,gl,base);raise(scene);scene.update(press(InputAction::Attack));scene.deactivate();
            require(gl.magazine(16)==1&&gl.count(35)==0,"CE69 interrupted recoil jumps to its later ammo commit before terminating presentation");
        }
        {
            GameSession knife(content.items,base.session,content.facts);knife.grant(25,1);require(knife.equip(25,2),"I25 is auxiliary equipment");
            require(knife.auxiliaryAttack()==base.session.baseAgility+14&&content.items.at(25).capacity==0,"I25 adds actual agility and has no invented ammo");
            CombatScene scene(content,knife,base);raise(scene,InputAction::AuxiliaryWeapon);scene.update(press(InputAction::Attack));tick(scene,40);
            const auto soundCue=content.actionProfiles.at("survival-knife-fire").at("sound_cue").get<std::string>();const auto sound=readSound(content.cue(soundCue).at("sound"));
            const auto presentation=scene.takePresentation();require(std::any_of(presentation.begin(),presentation.end(),[&](const auto& event){return event.kind==PresentationEvent::Kind::Sound&&event.sound==sound;}),"I25 original strike sound occurs through its distinct source action");
            for(const auto& direction:{"up","right","down","left"}){
                const auto id=content.catalog.at("weapon_bindings").at("25").at("strike_effect_prefix").get<std::string>()+direction;
                const auto& effect=content.media.at("effects").at(id);const auto& frames=effect.at("stage_images");
                require(frames.size()==2&&frames.at(0)!=frames.at(1)&&effect.at("wait_zero_count_after_stage")==nlohmann::json::array({2,2})&&effect.at("transparency_percent")==30,"CN CE7 C694-748 preserves two distinct blade images, four Wait0 and 30 percent transparency");
                for(const auto& image:frames){require(content.media.at("images").contains(image.get<std::string>()),"Each original blade frame resolves to admitted physical media");}
            }
        }
        // Real target receivers, using an isolated original post-pickup scene.
        for(const auto kind:{AttackKind::GlNormal,AttackKind::GlFlame,AttackKind::GlAcid}){
            const auto entry=content.entries.at("post-pickup");GameSession state(content.items,entry.session,content.facts);CombatScene scene(content,state,entry,fixed);
            const auto id=content.rules.at("enemy").at("id").get<std::string>();const auto p=findCharacter(scene.world(),id).motion.position();const GridPosition center{(p.x+.5)*16,(p.y+.5)*16};
            std::set<std::string> resolved;const auto before=state.exportState();
            const auto flight=scene.resolveAttack({center,{3,3},kind,1,0,false,123,AttackPhase::Flight},resolved);
            require(flight.contacts==1&&flight.processed==0&&flight.damage==0&&state.exportState()==before&&resolved.empty(),"GL flight contacts trigger explosion without receiver side effects or target de-duplication");
            if(kind==AttackKind::GlFlame){
                const auto flame=scene.resolveAttack({center,{44,44},kind,2,0,false,123,AttackPhase::Burning},resolved);
                require(flame.damage==0&&flame.processed==1&&state.flag(37),"GL spread activates its source-slot burn without rolling direct damage");
                const int health=state.hp();tick(scene,7);require(scene.enemyStates().front().damage==4&&state.hp()==health,"Original full six-Wait0 cycle adds four scene-local burn damage");
                resolved.clear();const auto beforeRepeat=state.exportState();
                const auto repeated=scene.resolveAttack({center,{44,44},kind,3,0,false,124,AttackPhase::Impact},resolved);
                require(repeated.damage==0&&repeated.processed==0&&state.exportState()==beforeRepeat,"CE4 skips a second flame hit while the source-slot burn is already active");
            }
        }
        for(const auto direction:{Direction::Up,Direction::Right,Direction::Down,Direction::Left}){
            for(int knife:{24,25}){
                auto entry=content.entries.at("post-pickup");entry.playerTile={8,8};entry.direction=direction;entry.session.flags[18]=true;entry.session.flags[52]=false;
                const auto id=content.rules.at("enemy").at("id").get<std::string>();
                const auto forward=adjacentTile(entry.playerTile,direction);entry.positions[id]=forward;
                if(direction==Direction::Up||direction==Direction::Down){++entry.positions[id].x;}else{++entry.positions[id].y;}
                GameSession state(content.items,entry.session,content.facts);state.grant(knife,1);require(state.equip(knife,2),"Original knife fixture can be equipped");
                CombatScene scene(content,state,entry,fixed);raise(scene,InputAction::AuxiliaryWeapon);scene.update(press(InputAction::Attack));tick(scene,7);
                const auto enemies=scene.enemyStates();const auto found=std::find_if(enemies.begin(),enemies.end(),[&](const auto& enemy){return enemy.id==id;});
                require(found!=enemies.end()&&(knife==25?found->damage>0:found->damage==0),"I25 hits the perpendicular adjacent column in all four directions; I24 retains its narrower source geometry");
            }
        }
        std::cout<<"Full weapon public-domain/controller checks passed (isolated inventory fixtures, no natural-route claim)\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
