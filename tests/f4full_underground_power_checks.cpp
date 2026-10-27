#include "combat_scene.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace{
void require(bool ok,const char* message){if(!ok){throw std::runtime_error(message);}}
}
int main(int argc,char** argv){try{
    if(argc!=2){return 2;}auto content=loadCombatContent(argv[1]);
    const auto sourceFull=content.full;
    const auto& source=sourceFull->at("runtime").at("underground").at("environmental_explosion");
    require(source.at("source_registration").at("half_pixels")==nlohmann::json::array({8,10}),"M67 E16P8 registers its own 8 by 10 contact body");
    const std::string barrel=source.at("object");const int map=content.objects.at(barrel).map;
    // Isolate the barrel's public contact/presentation from separately checked pursuit and gas.
    content.enemies.clear();auto fullDefinition=*content.full;fullDefinition["runtime"]["underground"]["gas_monitors"].erase(std::to_string(map));content.full=std::make_shared<const nlohmann::json>(std::move(fullDefinition));
    for(auto& flow:content.catalog.at("flows")){if(flow.value("trigger",std::string{})=="autorun"){flow["trigger"]="call-only";}}
    for(const auto position:{TilePosition{12,6},TilePosition{10,8}}){
        auto entry=content.entries.at("defeated-claimed");entry.map=map;entry.playerTile={position.x,position.y+1};entry.direction=Direction::Up;
        entry.positions.clear();entry.pages.clear();entry.directions.clear();entry.through.clear();entry.positions[barrel]=position;
        entry.session.hp=entry.session.maxHp=100;entry.session.baseDefense=4;entry.session.armor=entry.session.accessory=entry.session.otherEquipment=0;entry.session.conditions={};
        GameSession session(content.items,entry.session,content.facts);session.setFlag(1,true);session.setFlag(18,true);
        for(int flag:{13,15,45,477,573,227}){session.setFlag(flag,false);}session.setVariable(87,8);session.setVariable(88,15);
        CombatScene scene(content,session,entry,[](int low,int){return low;});
        const GridPosition foot{(position.x+.5)*16,(position.y+1.)*16};std::set<std::string> resolved;
        require(scene.resolveAttack({{foot.x+8.01,foot.y+10},{0,0},AttackKind::Knife,2},resolved).processed==0&&!session.flag(477),"Outside the source body does not ignite the barrel");
        require(scene.resolveAttack({{foot.x+8,foot.y+10},{0,0},AttackKind::Knife,2},resolved).processed==1&&session.flag(477),"Boundary contact ignites the declared environmental target");
        require(scene.resolveAttack({foot,{0,0},AttackKind::Knife,2},resolved).processed==0,"One attack cannot ignite the same barrel twice");
        scene.update({});scene.update({});require(session.hp()==100,"Two source Wait0 commands precede blast damage");
        scene.update({});require(session.hp()==27,"Source CE12 receives raw80 with defense4 and random minus5 exactly once");
        for(int tick=0;tick<30;++tick){scene.update({});}
        require(session.flag(573)&&session.hp()==27,"The final picture clears and the used flag prevents repeated damage");
        int frames=0;bool erased=false;
        for(const auto& event:scene.takePresentation()){
            frames+=std::find(source.at("frame_cues").begin(),source.at("frame_cues").end(),event.id)!=source.at("frame_cues").end();
            erased|=event.id==source.at("clear_picture").get<std::string>();
        }
        require(frames==6&&erased,"All six original explosion frames and erase cue are delivered");
        CombatScene returned(content,session,entry);resolved.clear();
        require(returned.resolveAttack({foot,{0,0},AttackKind::Knife,2},resolved).processed==0,"A reconstructed scene preserves the consumed barrel");
    }
    bool refused=false;try{validateAttackSample({{}, {0,0},AttackKind::Grenade,9,0,false,0,AttackPhase::ResolvedContact});}catch(const std::invalid_argument&){refused=true;}
    require(refused,"Weapon input cannot inject an internal source callback that bypasses collision");
    std::cout<<"Underground source barrel contact, delayed damage, complete presentation and revisit passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
