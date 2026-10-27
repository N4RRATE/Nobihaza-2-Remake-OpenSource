#include "combat_scene.h"
#include "enemy_controller.h"
#include "combat_presentation.h"
#include "enemy_host.h"
#include <iostream>
#include <stdexcept>
#include <regex>
void require(bool condition,const char* message){if(!condition){throw std::runtime_error(message);}}
int main(int argc,char** argv){try{
    if(argc!=2&&argc!=3){return 2;}const auto content=loadCombatContent(argv[1]);
    sf::Font font;std::unique_ptr<CombatPresentation> presentation;
    std::unique_ptr<GameApplication> renderedGame;std::unique_ptr<sf::RenderTexture> target;
    if(argc==3){require(font.openFromFile(content.maps.at(42).fontPath),"Real font loads");presentation=std::make_unique<CombatPresentation>(content,font);}
    const auto consume=[&](CombatScene& scene){auto events=scene.takePresentation();if(presentation){presentation->update(events);target->clear();presentation->draw(*target,*renderedGame);target->display();}return events;};
    {
        auto e=content.entries.at("defeated-claimed");e.map=42;e.playerTile={1,1};e.positions.clear();e.pages.clear();e.directions.clear();e.through.clear();
        for(int f:{1,18,181,498}){e.session.flags[f]=true;}for(int f:{186,401}){e.session.flags[f]=false;}
        GameSession s(content.items,e.session,content.facts);CombatScene scene(content,s,e);
        for(const auto* id:{"m42-e19","m42-e21"}){
            const auto p=findCharacter(scene.world(),id).motion.position();std::set<std::string> resolved;
            require(scene.resolveAttack({{(p.x+.5)*16,(p.y+.5)*16},{0,0},AttackKind::Shotgun,3,4,false},resolved).processed==1,"Seventh/eighth source blood slots receive real shotgun flight contact");
            const auto events=scene.takePresentation();const auto expected=content.enemies.at(id).rules.at("runtime_profile").at("blood_effects").at("2").get<std::string>();
            require(std::any_of(events.begin(),events.end(),[&](const auto& event){return event.kind==PresentationEvent::Kind::Effect&&event.id==expected&&event.actor==id;}),"Each late blood slot emits its own source effect");
        }
        EnemyHost host(scene,"m42-e19");
        require(host.bloodType(7,8,1)==2&&host.bloodType(8,7,3)==1,"Cross-slot writes retain independent read values");
        for(const auto slots:{std::pair{0,7},std::pair{7,-1},std::pair{9,7},std::pair{7,2147483647}}){bool rejected=false;try{host.bloodType(slots.first,slots.second,1);}catch(const std::logic_error&){rejected=true;}require(rejected,"Unregistered or nonpositive blood workspaces stay rejected");}
        CombatScene revisit(content,s,e);EnemyHost newHost(revisit,"m42-e19");require(newHost.bloodType(1,7,1)==0,"A rebuilt scene clears transient blood types");
    }
    {
        auto invalid=content;invalid.enemies.at("m42-e19").rules["runtime_profile"]["blood_slot"]=9;bool rejected=false;
        try{validateSceneContent(invalid,{});}catch(const std::runtime_error& error){rejected=std::string(error.what()).find("m42-e19 blood_slot: 9")!=std::string::npos;}
        require(rejected,"Unregistered profile slot is rejected at load validation with its enemy and role");
        invalid=content;invalid.enemies.at("m42-e21").rules["runtime_profile"]["blood_write_slots"]["3"]=0;rejected=false;
        try{validateSceneContent(invalid,{});}catch(const std::runtime_error& error){rejected=std::string(error.what()).find("m42-e21 blood_write_slots/3: 0")!=std::string::npos;}
        require(rejected,"Invalid source write slot is rejected before gameplay");
    }
    if(presentation){for(int targetCount:{0,1,8}){
        auto c=content;auto e=c.entries.at("defeated-claimed");e.map=42;e.playerTile={15,8};e.direction=Direction::Up;
        if(targetCount<8){std::erase_if(c.enemies,[&](const auto& row){return targetCount==0||row.first!="m42-e19";});}
        e.positions.clear();e.pages.clear();e.directions.clear();e.through.clear();e.positions["m42-e34"]={15,6};
        e.session.inventory[14]=1;e.session.mainWeapon=14;e.session.magazines[14]=5;e.session.hp=e.session.maxHp=9999;e.session.baseDefense=999;e.session.conditions={};
        for(int f:{1,18,181,498}){e.session.flags[f]=true;}for(int f:{13,15,45,196,477,193,176,177,182,183,184,185,186,401}){e.session.flags[f]=false;}
        for(const auto& [id,row]:c.full->at("runtime").at("rescue").at("encounters").items()){if(row.at("map")==42){e.positions[id]={15,4};}}
        c.entries["shotgun-explosion-fixture"]=e;GameApplication game(std::move(c),"shotgun-explosion-fixture",[](int low,int){return low;});sf::RenderTexture surface({320,240});
        const auto tick=[&](InputFrame input){game.update(input);presentation->update(game.takePresentation());surface.clear();presentation->draw(surface,game);surface.display();};
        InputFrame raise;raise.buttons[static_cast<int>(InputAction::Weapon)]={true,true,false};tick(raise);for(int n=0;n<100;++n){tick({});}
        require(game.raised(),"Real shotgun raise completes before fire");InputFrame fire;fire.buttons[static_cast<int>(InputAction::Attack)]={true,true,false};tick(fire);
        for(int n=0;n<180;++n){tick({});}
        require(game.session().magazine(14)==4&&game.session().flag(196),"Real shotgun shot spends one round and completes barrel explosion");
    }}
    require(content.enemies.contains("m42-e21")&&content.enemies.contains("m61-e3"),"Rescue loads eight courtyard enemies and one separate rescue enemy");
    std::size_t references=0;std::set<int> maps,slots;
    const std::regex cuePattern("(m[0-9]+-e[0-9]+-p[0-9]+-c[0-9]+|((f4full|f3|f4bc|f4wa)-)?ce[0-9]+-c[0-9]+)");
    const auto scan=[&](const auto& self,const nlohmann::json& value,const std::string& path)->void{
        if(value.is_object()){for(const auto& [key,child]:value.items()){if(key!="source"){self(self,child,path+"/"+key);}}}
        else if(value.is_array()){for(std::size_t n=0;n<value.size();++n){self(self,value.at(n),path+"/"+std::to_string(n));}}
        else if(value.is_string()){const auto id=value.get<std::string>();if(std::regex_match(id,cuePattern)){++references;if(!content.cues.contains(id)&&!content.messages.contains(id)){throw std::runtime_error("Missing delivered enemy reference "+path+": "+id);}}}
    };
    for(const auto& [id,enemy]:content.enemies){maps.insert(enemy.map);scan(scan,enemy.rules,id);if(enemy.rules.contains("runtime_profile")){const auto& profile=enemy.rules.at("runtime_profile");if(profile.contains("blood_slot")){slots.insert(profile.at("blood_slot").get<int>());}if(enemy.rules.value("behavior",std::string{})=="spider"){slots.insert(profile.at("source_slot").get<int>());}}}
    std::cout<<"Delivered enemies="<<content.enemies.size()<<", cue/message references="<<references<<", maps=";for(int map:maps){std::cout<<map<<',';}std::cout<<" blood slots=";for(int slot:slots){std::cout<<slot<<',';}std::cout<<'\n';
    require(content.catalog.at("environment_targets").size()==1&&content.catalog.at("environment_targets").at(0).at("map")==42,"Barrel is not an NPC or a Map61 target");
    for(const TilePosition barrel:{TilePosition{15,6},TilePosition{10,8}}){
        // Explicit encounter fixture changes placement, not the reviewed original explosion rules.
        auto entry=content.entries.at("defeated-claimed");entry.map=42;entry.playerTile={barrel.x,barrel.y+1};entry.direction=Direction::Up;
        entry.positions.clear();entry.pages.clear();entry.directions.clear();entry.through.clear();entry.session.hp=100;entry.session.maxHp=100;
        entry.session.armor=0;entry.session.accessory=0;entry.session.otherEquipment=0;entry.session.baseDefense=4;entry.session.conditions={};
        entry.positions["m42-e34"]=barrel;
        for(const auto& [id,row]:content.full->at("runtime").at("rescue").at("encounters").items()){
            if(row.at("map")==42){entry.positions[id]={barrel.x,barrel.y};}
        }
        GameSession session(content.items,entry.session,content.facts);
        for(int flag:{1,18,181,498}){session.setFlag(flag,true);}
        for(int flag:{13,15,45,196,477,193,176,177,182,183,184,185,186,401}){session.setFlag(flag,false);}
        if(presentation){auto drawContent=content;auto drawEntry=entry;drawEntry.session=session.exportState();drawContent.entries["explosion-draw-fixture"]=drawEntry;renderedGame=std::make_unique<GameApplication>(std::move(drawContent),"explosion-draw-fixture");target=std::make_unique<sf::RenderTexture>(sf::Vector2u{320,240});}
        CombatScene scene(content,session,entry,[](int low,int){return low;});
        const GridPosition foot{(barrel.x+.5)*16,(barrel.y+1.0)*16};std::set<std::string> hit;
        scene.resolveAttack({{foot.x+8.01,foot.y+10},{0,0},AttackKind::Knife,2},hit);
        require(!session.flag(477),"Barrel contact rejects outside its original 8 by 10 body box");
        hit.clear();const auto contact=scene.resolveAttack({{foot.x+8,foot.y+10},{0,0},AttackKind::Knife,2},hit);
        require(contact.processed==1&&hit.contains("m42-e34")&&session.flag(477),"Closed body boundary triggers one stable environmental target");
        require(scene.resolveAttack({{foot.x+8,foot.y+10},{0,0},AttackKind::Knife,2},hit).processed==0,"Same attack cannot retrigger an ignited barrel");
        const int health=session.hp();scene.update({});scene.update({});
        require(session.hp()==health,"Explosion damage waits for the original first two Wait0 commands");
        scene.update({});require(session.hp()==27,"Raw80 minus defense/2 plus -5 uses CE12 once");
        bool blood=false;for(const auto& event:consume(scene)){blood|=event.kind==PresentationEvent::Kind::Effect&&event.id=="blood-large";}
        require(blood,"Player explosion damage submits the existing CE14 large-blood presentation");
        for(int i=0;i<100;++i){scene.update({});consume(scene);}
        require(session.flag(196)&&session.flag(193),"Explosion finishes once and all eight death flags clear the courtyard");
        require(session.hp()==27,"Later pictures do not apply the explosion damage again");
        for(const auto& enemy:scene.enemyStates()){require(enemy.mode==EnemyMode::Corpse,"Each of eight processed enemies completes its own death");}
        auto restored=entry;restored.session=session.exportState();
        CombatScene revisit(content,session,restored);std::set<std::string> repeat;
        require(revisit.resolveAttack({foot,{0,0},AttackKind::Knife,2},repeat).processed==0,"Scene reconstruction preserves the used barrel and dead enemies");
        const int before=session.hp();for(int i=0;i<10;++i){revisit.update({});}require(session.hp()==before,"Revisit does not replay explosion damage");
    }
    auto entry=content.entries.at("natural").session;entry.hp=30;entry.tp=2;entry.conditions.bleedingTicks=300;entry.conditions.poisoned=true;
    entry.conditions.poisonWaitTicks=90;entry.conditions.neuralParalysis=true;entry.conditions.invulnerableTicks=18;
    GameSession healing(content.items,entry,content.facts);healing.restoreActor();
    require(healing.hp()==healing.maxHp()&&healing.tp()==healing.maxTp()&&!healing.conditions().poisoned&&!healing.conditions().neuralParalysis&&!healing.conditions().bleedingActive,"FullHeal restores HP/TP and removes actor states");
    require(healing.conditions().poisonWaitTicks==90&&healing.conditions().invulnerableTicks==18,"FullHeal leaves running parallel waits and protection intact");
    for(bool treatment:{false,true}){
        auto arrival=content.entries.at("defeated-claimed");arrival.map=61;arrival.playerTile=treatment?TilePosition{17,8}:TilePosition{16,5};arrival.direction=Direction::Up;
        arrival.positions.clear();arrival.pages.clear();arrival.directions.clear();arrival.through.clear();arrival.session.hp=30;arrival.session.tp=2;
        GameSession rescue(content.items,arrival.session,content.facts);
        for(int flag:{1,18,178}){rescue.setFlag(flag,true);}
        for(int flag:{13,15,45,193,194,195}){rescue.setFlag(flag,false);}rescue.setFlag(180,treatment);
        CombatScene story(content,rescue,arrival,[](int low,int){return low;});InputFrame confirm;
        confirm.buttons[static_cast<std::size_t>(InputAction::Confirm)].pressed=true;
        story.update(confirm);
        require(story.foreground()==ForegroundKind::Content,"Original rescued NPC page starts from normal investigation");
        for(int i=0;i<10000&&story.foreground()!=ForegroundKind::None;++i){story.update(confirm);}
        require(story.foreground()==ForegroundKind::None&&rescue.flag(1)&&rescue.flag(18)&&!rescue.flag(13),"Sena source dialogue and routes finish and restore control");
        require(rescue.flag(treatment?194:180),"Discovery and treatment commit their distinct original facts");
        require(!rescue.flag(195)&&rescue.count(74)==0,"Discovery does not prematurely grant the later tank key or depart");
        if(treatment){require(rescue.hp()==rescue.maxHp()&&rescue.tp()==rescue.maxTp(),"Original treatment interaction invokes FullHeal at its source step");}
    }
    std::cout<<"Rescue environmental contacts, timed explosion, eight deaths, re-entry and FullHeal passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
