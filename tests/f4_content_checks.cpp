#include "combat_content.h"
#include "save_snapshot.h"
#include "game_application.h"
#include <fstream>
#include "content_hash.h"
#include <iostream>
#include <stdexcept>
void require(bool v,const char* message){if(!v){throw std::runtime_error(message);}}
int main(int argc,char** argv){
    if(argc!=2){return 2;}
    try{
        require(contentSha256("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","Empty hash known answer");
        require(contentSha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","Hash known answer");
        auto c=loadCombatContent(argv[1]);
        require(c.maps.at(19).width==35&&c.maps.at(20).height==30,"F4 retains actual different map dimensions");
        auto entry=c.entries.at("natural").session;
        GameSession session(c.items,entry);
        require(session.magazine(14)==5&&session.magazine(21)==8,"Unowned weapons retain reviewed magazine seeds");
        session.grant(21,1);require(session.canEquip(21,1),"DE has its reviewed executable runtime after53");
        if(c.items.at(14).stateOnly){require(!session.canEquip(14,1),"State-only weapon cannot be equipped");}
        bool rejected=false;try{session.grant(14,1);}catch(const std::exception&){rejected=true;}
        if(c.items.at(14).stateOnly){require(rejected&&session.count(14)==0,"State-only grant fails without changing inventory");}
        else{require(!rejected&&session.count(14)==1&&session.canEquip(14,1),"Prepared B shotgun is executable after59");}
        std::ifstream original(c.directory/"f3-source-contract.json");
        require(bool(original),"Package carries its independent source contract");
        // A real lawful F3 save remains validated against the original definitions.
        auto source=loadCombatContent(c.directory/"f3-source-contract.json");
        auto start=source.entries.at("defeated-claimed");start.map=16;start.playerTile={10,21};start.direction=Direction::Up;
        start.pages.clear();start.positions.clear();start.directions.clear();start.through.clear();source.entries["save-fixture"]=start;
        GameSession saving(source.items,start.session);CombatScene room(source,saving,"save-fixture");
        auto old=captureSave(source,room,{room.generation(),"m16-e5","BSAA 1F入口"},{});
        auto migrated=decodeSave(c,encodeSave(source,old));
        require(migrated.contentCompatibility=="f4a-normal-1"&&migrated.session.magazines.at(14)==5&&migrated.session.magazines.at(21)==8,"Known source save gains only reviewed seeds");
        require(migrated.session.inventory==old.session.inventory&&migrated.scene==old.scene,"Migration preserves progress and saved objects");
        auto restored=prepareRestore(c,migrated);
        require(restored.scene->mapId()==16&&restored.session->variable(399)==old.continuation.statisticBefore+1,"Restoration applies after-save once");
        auto bad=nlohmann::json::parse(encodeSave(source,old));bad["session"]["magazines"].push_back({21,8});
        bool invalid=false;try{decodeSave(c,bad.dump());}catch(const std::exception&){invalid=true;}
        require(invalid,"F4 items cannot evade fixed F3 source validation");
        bad=nlohmann::json::parse(encodeSave(source,old));bad["scene"]["objects"].erase(0);invalid=false;
        try{decodeSave(c,bad.dump());}catch(const std::exception&){invalid=true;}require(invalid,"Missing old scene objects cannot be filled by migration");
        GameSession factSession(c.items,entry,c.facts);
        factSession.setFlag(57,true);
        auto facts=factSession.exportState();
        require(!facts.flags.contains(57)&&facts.worldFlags.at("m19.licker-activated"),"Source adapter writes one durable named fact");
        auto missing=migrated;missing.session.magazines.erase(14);bool missingRejected=false;
        try{encodeSave(c,missing);}catch(const std::exception&){missingRejected=true;}require(missingRejected,"F4 save cannot omit unowned magazine seeds");
        auto current=decodeSave(c,encodeSave(c,migrated));
        require(current==migrated,"F4 state-only seed survives its own snapshot roundtrip");
        bool failed=false;try{prepareRestore(c,current,[](const SceneDefinition&){throw std::runtime_error("missing image");});}catch(const std::exception&){failed=true;}
        require(failed&&restored.scene->mapId()==16,"A failed independent candidate preserves the earlier restored scene");
        std::cout<<"F4 content checks passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
