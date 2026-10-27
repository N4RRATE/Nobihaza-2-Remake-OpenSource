#include "game_application.h"
#include <fstream>
#include <chrono>
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* reason){if(!ok){throw std::runtime_error(reason);}}
SaveSnapshot lawfulSave(const CombatContent& content){
    auto entry=content.entries.at("defeated-claimed");entry.map=16;entry.playerTile={10,21};entry.direction=Direction::Up;
    entry.pages.clear();entry.positions.clear();entry.directions.clear();entry.through.clear();
    GameSession session(content.items,entry.session,content.facts);CombatScene scene(content,session,entry);
    return captureSave(content,scene,{scene.generation(),"m16-e5","BSAA 1F入口"},{});
}
int main(int argc,char** argv){
    if(argc!=2){return 2;}
    try{
        const auto path=std::filesystem::absolute(argv[1]);auto full=loadCombatContent(path);
        if(full.compatibility=="post-f5-g0-normal-1"){
            require(full.legacySource->compatibility=="f4-normal-1"&&full.maps.at(120).width==55&&full.maps.at(127).height==15,"G0 preserves full source and original dimensions");
            for(const auto name:{"f3-source-contract.json","f4a-source-contract.json","f4bc-source-contract.json","g0-f4-source.json"}){
                const auto source=loadCombatContent(path.parent_path()/name);auto old=lawfulSave(source);old.session.magazines[11]=4;old.session.variables[382]=5;
                const auto bytes=encodeSave(source,old);const auto upgraded=decodeSave(full,bytes);
                require(upgraded.session.inventory==old.session.inventory&&upgraded.session.magazines.at(11)==4&&upgraded.session.variables.at(382)==5,"D1 retains legal old inventory/magazines/statistics");
                for(const auto& [id,key]:full.catalog.at("g0").at("facts").items()){require(!upgraded.session.worldFlags.at(key.get<std::string>()),"Old adapter must not fabricate hangar progress");}
                require(decodeSave(full,encodeSave(full,upgraded))==upgraded&&prepareRestore(full,upgraded).scene->mapId()==16,"Old chain prepares G0 only after source validation");
                require(encodeSave(source,old)==bytes,"Source snapshot is not rewritten by import");
                auto bad=nlohmann::json::parse(bytes);bad["session"]["world_flags"].push_back({"hangar.prebattle-return",true});bool rejected=false;try{decodeSave(full,bad.dump());}catch(const std::exception&){rejected=true;}require(rejected,"Legacy source cannot smuggle target facts");
            }
            std::cout<<"G0 fixed projection and F3/F4A/BC/F4 explicit strict source-chain D1 upgrade passed\n";return 0;
        }
        require(full.compatibility=="f4-normal-1"&&full.legacySource->compatibility=="f4bc-normal-1","real cumulative full reader retains strict B/C source");
        require(full.maps.size()==full.legacySource->maps.size()+(full.maps.contains(51)?1:0),"only the audited training map may extend inherited starter metadata");
        require(full.requiredMagazines.size()==10&&full.items.at(16).capacity==1&&full.items.at(19).capacity==30,"all ten persistent slots are declared from first batch");
        require(full.items.at(16).stateOnly&&full.items.at(16).boxItem==0,"loose-only GL definition cannot be used before actions arrive");
        require(full.items.at(3).medicine.hpPoints==10&&full.items.at(3).medicine.cureStates==std::vector<int>{2},"real original blue herb keeps both healing and poison cure");
        for(const auto name:{"f3-source-contract.json","f4a-source-contract.json","f4bc-source-contract.json"}){
            const auto source=loadCombatContent(path.parent_path()/name);auto old=lawfulSave(source);old.session.magazines[11]=4;
            const auto bytes=encodeSave(source,old);auto restored=decodeSave(full,bytes);
            require(restored.session.magazines.at(11)==4&&restored.session.magazines.at(16)==1&&restored.session.magazines.at(17)==0&&restored.session.magazines.at(19)==30,"bounded upgrade seeds absent slots only");
            require(restored.session.inventory==old.session.inventory&&restored.session.worldValues.at("guard.player-kills")==0,"upgrade copies inventory and initializes genuinely absent score");
            require(decodeSave(full,encodeSave(full,restored))==restored,"actual full content v2 roundtrip");
            require(prepareRestore(full,restored).scene->mapId()==16,"actual candidate scene restoration succeeds");
            require(encodeSave(source,old)==bytes,"old source snapshot bytes untouched");
            auto bad=nlohmann::json::parse(bytes);bad["session"]["magazines"].push_back({16,1});
            bool rejected=false;try{decodeSave(full,bad.dump());}catch(const std::exception&){rejected=true;}
            require(rejected,"strict source rejects target-only slot before adaptation");
        }
        auto current=lawfulSave(full);current.session.eventTimer={EventTimerSource::Training,1234,true,false,false};
        require(decodeSave(full,encodeSave(full,current))==current,"natural hidden training remainder may be saved");
        auto reject=[&](SaveSnapshot bad){bool rejected=false;try{encodeSave(full,bad);}catch(const std::exception&){rejected=true;}require(rejected,"starter refuses unavailable late progress");};
        current.session.pursuerHealth=317;
        require(decodeSave(full,encodeSave(full,current))==current,"training Actor30 direct damage survives legal return/save without HP reset");
        auto late=current;late.session.conditions.poisoned=true;reject(late);
        late=current;late.session.inventory[78]=1;reject(late);
        late=current;late.session.eventTimer={EventTimerSource::Power,1234,true,false,false};reject(late);
        for(int item:{52,53}){late=current;late.session.accessory=item;late.session.inventory[item]=0;reject(late);}
        auto trainingEquipment=current;trainingEquipment.session.accessory=54;
        require(decodeSave(full,encodeSave(full,trainingEquipment))==trainingEquipment,"training I54 equipment is available independently of late accessory definitions");
        GameApplication application(full,"defeated-claimed");const auto generation=application.generation();const auto live=application.session().exportState();
        auto invalid=current;invalid.session.pursuerHealth=0;std::string error;
        require(!application.restore(invalid,error)&&!error.empty()&&application.generation()==generation&&application.session().exportState()==live,"failed full candidate leaves old game and scene alive");
        const auto fixture=std::filesystem::temp_directory_path()/("nobihaza-full-reader-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}} cleanup{fixture};
        std::filesystem::copy(path.parent_path(),fixture,std::filesystem::copy_options::recursive);
        std::ifstream input(path);const auto original=nlohmann::json::parse(input);
        for(int variation=0;variation<8;++variation){auto bad=original;
            if(variation==0){bad["unreviewed"]=true;}
            else if(variation==1){bad["f4full"]["state_contract"]["magazine_seeds"]["16"]=0;}
            else if(variation==2){bad["source_contracts"]["f4bc-normal-1"]["sha256"]="bad";}
            else if(variation==3){bad["items"]["3"]["medicine"]["hp_points"]=99;}
            else if(variation==4){bad["f4full"]["runtime"]["flows"]["invented"]={{"steps",nlohmann::json::array()}};}
            else if(variation==5){bad["entries"]["natural"]["magazines"]["11"]=14;}
            else if(variation==6){bad["capabilities"]["weapons"]["16"]="executable";}
            auto bytes=bad.dump();if(variation==7){bytes.insert(1,"\"stage\":\"training\",");}
            std::ofstream(fixture/"malformed.json")<<bytes;
            bool rejected=false;try{loadCombatContent(fixture/"malformed.json");}catch(const std::exception&){rejected=true;}
            if(!rejected){throw std::runtime_error("Full package malformed variation accepted: "+std::to_string(variation));}
        }
        std::filesystem::remove(fixture/"malformed.json");
        require(loadCombatContent(fixture/"f4full.json").compatibility=="f4-normal-1","copied package is self-contained");
        const auto cwd=std::filesystem::current_path();std::filesystem::current_path(std::filesystem::temp_directory_path());
        auto independent=loadCombatContent(path);std::filesystem::current_path(cwd);
        require(independent.maps.size()==full.maps.size(),"package paths do not depend on working directory");
        std::cout<<"F4 full real package and source checks passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
