#include "save_files.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
void require(bool value,const char* message){if(!value){throw std::runtime_error(message);}}
std::string read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);if(!f){throw std::runtime_error("Missing input save");}return {(std::istreambuf_iterator<char>(f)),{}};}
SaveSnapshot lawfulSave(const CombatContent& c){
    auto e=c.entries.at("defeated-claimed");e.map=16;e.playerTile={10,21};e.direction=Direction::Up;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();
    GameSession session(c.items,e.session,c.facts);CombatScene scene(c,session,e);return captureSave(c,scene,{scene.generation(),"m16-e5","BSAA 1F入口"},{});
}
int main(int argc,char** argv){
    if(argc!=3&&argc!=4){return 2;}
    try{
        const auto c=loadCombatContent(argv[1]);const auto root=std::filesystem::absolute(argv[2]);
        require(!std::filesystem::exists(root),"Save checks require a fresh isolated directory");std::filesystem::create_directories(root);
        const auto& a=*c.legacySource;const auto& f3=*a.legacySource;
        auto older=lawfulSave(f3);auto newer=lawfulSave(a);newer.session.magazines[21]=3;
        std::ofstream(root/"slot-01.json")<<encodeSave(a,newer);std::ofstream(root/"slot-01.backup.json")<<encodeSave(f3,older);
        SaveFiles files(c,root);const auto adapted=files.read(1);require(adapted.session.magazines.at(21)==3,"Mixed source main keeps ammunition");
        require(files.recoveryChoices(1).size()==2,"Both reviewed sources remain recoverable");
        require(files.write(1,adapted).outcome==WriteOutcome::Committed,"F4-A main plus F3 backup can save B/C");
        std::ofstream(root/"slot-02.backup.json")<<encodeSave(f3,older);
        auto choices=files.recoveryChoices(2);require(choices.size()==1,"Missing main exposes known old backup");
        require(files.recover(2,choices.front()).outcome==WriteOutcome::Committed,"Explicit old backup recovery succeeds");
        require(files.write(2,files.read(2)).outcome==WriteOutcome::Committed,"Recovered old backup can save again");
        auto bad=nlohmann::json::parse(encodeSave(f3,older));bad["scene"]["objects"].erase(0);
        bool rejected=false;try{decodeSave(c,bad.dump());}catch(const std::exception&){rejected=true;}require(rejected,"Source objects validated before adaptation");
        require(adapted.session.worldValues.at("combat.last-defense")==0,"Old remake missing work defense uses explicit zero compatibility initialization");
        auto retained=adapted;retained.session.worldValues["combat.last-defense"]=9;
        auto roundtrip=prepareRestore(c,decodeSave(c,encodeSave(c,retained)));require(roundtrip.session->variable(49)==9,"Retained defense survives save and load");
        for(int variation:{0,1,2,3,4,5}){auto invalid=adapted;
            if(variation==0){invalid.session.inventory[67]=1;}else if(variation==1){invalid.session.worldFlags["power.stair-shutter-open"]=true;}else if(variation==2){invalid.session.worldValues.erase("combat.last-defense");}else if(variation==3){invalid.session.inventory[69]=1;}else if(variation==4){invalid.session.worldFlags[c.facts.switches.at(141)]=true;}else{invalid.session.worldFlags[c.facts.switches.at(96)]=invalid.session.worldFlags[c.facts.switches.at(97)]=true;}
            bool refused=false;try{prepareRestore(c,invalid);}catch(const std::exception&){refused=true;}require(refused,"Malformed power/work state cannot restore");
        }
        auto lighter=adapted;lighter.session.inventory[68]=1;lighter.session.worldFlags[c.facts.switches.at(145)]=true;lighter.session.worldFlags[c.facts.switches.at(141)]=true;auto lit=prepareRestore(c,decodeSave(c,encodeSave(c,lighter)));require(lit.session->count(68)==1&&lit.session->flag(141),"Lighter and flame restore together");
        auto current=prepareRestore(c,adapted);rejected=false;
        try{prepareRestore(c,adapted,[](const SceneDefinition&){throw std::runtime_error("injected missing texture");});}catch(const std::exception&){rejected=true;}
        require(rejected&&current.scene->mapId()==16&&current.session->magazine(21)==3,"Failed preparation preserves valid candidate");
        const auto once=current.session->variable(399);auto again=prepareRestore(c,adapted);require(again.session->variable(399)==once,"Repeated Load does not compound after-save statistics");
        if(argc==4){
            for(const auto& p:std::filesystem::directory_iterator(argv[3])){
                if(p.path().extension()!=".json"||!p.path().filename().string().starts_with("slot-")){continue;}
                const auto raw=read(p.path());const auto saved=decodeSave(c,raw);auto restored=prepareRestore(c,saved);
                require(restored.session->hp()==saved.session.hp&&restored.session->exportState().inventory==saved.session.inventory,"Real source copy restores without reward replay");
                require(read(p.path())==raw,"Load never writes the source copy");
            }
        }
        std::cout<<"B/C strict source, mixed backup and candidate checks passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
