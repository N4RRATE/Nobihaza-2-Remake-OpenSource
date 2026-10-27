#include "combat_content.h"
#include "save_snapshot.h"
#include "game_application.h"
#include <iostream>
#include <stdexcept>
void require(bool value,const char* message){if(!value){throw std::runtime_error(message);}}
SaveSnapshot lawfulSave(const CombatContent& c){
    auto entry=c.entries.at("defeated-claimed");entry.map=16;entry.playerTile={10,21};entry.direction=Direction::Up;
    entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();
    GameSession session(c.items,entry.session,c.facts);CombatScene scene(c,session,entry);
    return captureSave(c,scene,{scene.generation(),"m16-e5","BSAA 1F入口"},{});
}
int main(int argc,char** argv){
    if(argc!=2){return 2;}
    try{
        auto c=loadCombatContent(argv[1]);require(c.compatibility=="f4bc-normal-1","B/C content accepted independently of old F4");
        require(c.maps.size()==56&&c.maps.at(96).width>0,"Cumulative content keeps all actual maps");
        const auto& animated=c.maps.at(96).animatedTiles;
        require(!animated.empty(),"Animated tiles are distinct from static layers");
        for(const auto& tile:animated){
            if(tile.id==1000){require(tile.frameAt(0)==0&&tile.frameAt(24)==1&&tile.frameAt(48)==2&&tile.frameAt(72)==1&&tile.frameAt(96)==0,"AB original four-phase cadence");}
            if(tile.id==3132){require(tile.frameAt(0)==0&&tile.frameAt(6)==1&&tile.frameAt(18)==3&&tile.frameAt(24)==0,"C original four-frame cadence");}
        }
        for(const auto name:{"f3-source-contract.json","f4a-source-contract.json"}){
            auto source=loadCombatContent(c.directory/name);auto old=lawfulSave(source);old.session.magazines[11]=4;
            if(old.session.magazines.contains(21)){old.session.magazines[21]=2;}
            auto adapted=decodeSave(c,encodeSave(source,old));
            require(adapted.session.magazines.at(11)==4&&adapted.session.magazines.at(12)==13&&adapted.session.magazines.at(27)==7,"Only absent source magazines gain seeds");
            if(old.session.magazines.contains(21)){require(adapted.session.magazines.at(21)==2,"F4-A DE rounds preserved");}
            require(adapted.session.inventory==old.session.inventory,"Adaptation preserves inventory");
            require(decodeSave(c,encodeSave(c,adapted))==adapted,"Own snapshot roundtrip preserves state");
            auto restored=prepareRestore(c,adapted);require(restored.scene->mapId()==16,"Restoration reaches reviewed save map");
            auto malformed=nlohmann::json::parse(encodeSave(source,old));malformed["session"]["magazines"].push_back({12,13});
            bool rejected=false;try{decodeSave(c,malformed.dump());}catch(const std::exception&){rejected=true;}require(rejected,"Source validator rejects target-only fields");
        }
        std::cout<<"F4-B/C content and source checks passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
