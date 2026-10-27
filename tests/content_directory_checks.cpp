#include "combat_content.h"
#include <iostream>
#include <fstream>
#include <set>
#include <stdexcept>
void need(bool ok,const char* text){if(!ok){throw std::runtime_error(text);}}
int main(int argc,char** argv){try{
    need(argc==2,"content package required");
    for(const char* text:{R"({"runtime":{}})",R"({"runtime":{"weapon_actions":{"enemy_sources":{"weapon":2}},"combat_effects":{}}})",R"({"runtime":{"weapon_actions":{"enemy_sources":{"weapon":2}},"combat_effects":{"enemy_sources":null}}})",R"({"runtime":{"weapon_actions":{"enemy_sources":{}},"fatal_death":null}})",R"({"runtime":{"weapon_actions":{"enemy_sources":null},"combat_effects":{"enemy_sources":{"preferred":4}}}})",R"({"runtime":{"weapon_actions":{"enemy_sources":{"weapon":2},"burns":{"2":{}}},"combat_effects":{"enemy_sources":{"effect":4},"burns":{"4":{}}},"fatal_death":{"actor":1}}})"}){
        const auto original=nlohmann::json::parse(text);CombatContent separated;separated.setFullDefinition(original);
        need(separated.resolvedFullDefinition()==original,"Separated full definition lost missing/null/empty/branch values");
        need(!separated.full->at("runtime").contains("fatal_death")&&separated.fatalDeath.has_value()==original.at("runtime").contains("fatal_death"),"Fatal rule absent/null distinction changed");
        for(const auto* branch:{"weapon_actions","combat_effects"}){
            if(original.at("runtime").contains(branch)&&original.at("runtime").at(branch).contains("enemy_sources")){
                need(!separated.full->at("runtime").at(branch).contains("enemy_sources"),"Burn binding has two authorities");
                need(separated.enemyBurnSlots.at(branch)==original.at("runtime").at(branch).at("enemy_sources"),"Distinct burn branches were merged");
            }
        }
        if(original.at("runtime").contains("combat_effects")){
            if(original.at("runtime").at("combat_effects").contains("enemy_sources")){need(separated.enemyBurnSources()==original.at("runtime").at("combat_effects").at("enemy_sources"),"Combat effect branch lost priority");}
            else{bool rejected=false;try{(void)separated.enemyBurnSources();}catch(const std::exception&){rejected=true;}need(rejected,"Missing preferred binding silently fell back to the weapon branch");}
        }
        auto variant=separated;variant.enemyBurnSlots["weapon_actions"]={{"private",9}};variant.fatalDeath=nlohmann::json{{"actor",99}};
        need(separated.resolvedFullDefinition()==original,"Small binding/rule variant contaminated its source");
    }
    auto content=loadCombatContent(argv[1]);
    bool equipmentBoundary=false,fullBoundary=false,cueReplacement=false;
    std::size_t generations=0,maps=0;std::set<const SpriteDefinitions*> directories;
    for(const CombatContent* source=&content;source;source=source->legacySource.get()){
        need(!source->maps.empty(),"Empty historical map directory");const auto directory=source->maps.begin()->second.sprites;
        need(directory&&!directory->empty(),"Missing frozen sprite directory");
        need(!source->media.contains("cues"),"Cue definitions have two authorities");directories.insert(directory.get());
        for(const auto& [id,map]:source->maps){need(map.sprites==directory,"One generation copied per-map sprite directories");++maps;}
        if(source->compatibility=="post-f5-g7-normal-6"){
            const std::string added="g7-equipment-00711e123035d12b-0";
            need(source->legacySource&&source->legacySource->compatibility=="post-f5-g7-normal-4","Missing equipment predecessor");
            const auto prior=source->legacySource->maps.begin()->second.sprites;
            need(directory->contains(added)&&!prior->contains(added)&&directory!=prior,"Equipment graphics leaked into the strict prior directory");
            need(source->cues.contains("g7eq-ce78-c3")&&!source->legacySource->cues.contains("g7eq-ce78-c3"),"New equipment cue leaked into the strict predecessor");
            const auto& priorCue=*source->legacySource->cues.begin();need(source->cues.at(priorCue.first)==priorCue.second,"Unchanged inherited cue copied its leaf");
            equipmentBoundary=true;
        }
        if(source->legacySource&&source->catalog.contains("epilogue")&&!source->legacySource->catalog.contains("epilogue")){
            std::ifstream input(source->directory/"g7-03-source.json");const auto original=nlohmann::json::parse(input).at("projection").at("cues");
            for(const auto& [id,definition]:original.items()){
                need(source->cue(id)==definition,"Epilogue cue differs from independently parsed original projection");
                if(source->legacySource->cues.contains(id)&&source->legacySource->cue(id)!=definition){
                    need(source->cues.at(id)!=source->legacySource->cues.at(id),"Replaced cue mutated its historical leaf");cueReplacement=true;
                }
            }
        }
        if(source->compatibility=="post-f5-g1-normal-3"){
            need(source->legacySource&&source->legacySource->compatibility=="post-f5-g1-normal-2","Missing original wax predecessor");
            const auto before=source->legacySource->full;
            need(source->full==before&&source->enemyBurnSources().at("m141-e2")==1&&
                 !source->legacySource->enemyBurnSources().contains("m141-e2"),"Wax enemy binding contaminated historical full definitions");
            fullBoundary=true;
        }
        if(source->compatibility=="post-f5-g3-normal-7"){need(source->legacySource&&source->full==source->legacySource->full,"Story-only reunion copied unchanged full rules");}
        ++generations;
    }
    if(content.compatibility=="post-f5-g8-late-4"){
        need(equipmentBoundary&&fullBoundary&&cueReplacement,"Missing actual equipment/wax generation tests");
        std::ifstream input(content.directory/"g0-f4-source.json");const auto source=nlohmann::json::parse(input).at("f4full");
        CombatContent originalSource;originalSource.setFullDefinition(source);
        need(originalSource.resolvedFullDefinition()==source,"Real original full definition changed during separation/export");
        std::ifstream completeInput(content.directory/"g0-f4-source.json");const auto complete=nlohmann::json::parse(completeInput);
        bool originalCues=false;
        for(const auto* generation=&content;generation;generation=generation->legacySource.get()){
            if(generation->compatibility!=complete.at("content_compatibility").get<std::string>()||generation->fullStage!=complete.at("stage").get<std::string>()){continue;}
            const auto exported=generation->resolvedMedia();need(exported.at("cues").size()==generation->cues.size(),"Complete media export lost a cue");
            for(const auto& [id,cue]:complete.at("cues").items()){need(generation->cue(id)==cue&&exported.at("cues").at(id)==cue,"Real original cue changed during import/export");}
            originalCues=true;break;
        }
        need(originalCues,"Original full source cue generation absent");
    }
    const auto inheritedCue=content.cues.begin()->second;const auto cueKey=content.cues.begin()->first;const auto cueValue=*inheritedCue;
    {
        auto candidate=content;candidate.setMediaDefinition("cues",cueKey,{{"kind","deliberately-invalid-candidate"}});
        need(content.cue(cueKey)==cueValue&&candidate.cues.at(cueKey)!=inheritedCue,"Private cue replacement contaminated the active source");
        bool failed=false;try{validateSceneContent(candidate,*candidate.maps.begin()->second.sprites);}catch(const std::exception& error){failed=std::string(error.what()).find("Unknown media cue: "+cueKey)!=std::string::npos;}
        need(failed&&content.cue(cueKey)==cueValue,"Invalid cue candidate did not preserve the original definition");
    }
    {
        CombatContent imported;const nlohmann::json original={{"null-cue",nullptr},{"valid",{{"kind","wait"},{"tenths",0}}}};
        imported.replaceMediaDefinitions("cues",original);need(imported.resolvedMedia().at("cues")==original,"Cue export changed null values");
        const auto retained=imported.cues.at("valid");bool failed=false;try{imported.replaceMediaDefinitions("cues",nullptr);}catch(const std::exception&){failed=true;}
        need(failed&&imported.cues.at("valid")==retained,"Invalid cue group replaced a prepared directory");
        failed=false;try{(void)imported.cue("unknown");}catch(const std::exception&){failed=true;}need(failed,"Unknown cue silently became an empty definition");
    }
    const auto inheritedFull=content.full;
    auto fullVariant=*inheritedFull;fullVariant["independent-variant-check"]=true;
    const auto changedFull=std::make_shared<const nlohmann::json>(std::move(fullVariant));
    need(!inheritedFull->contains("independent-variant-check"),"Derived full definition changed its source");
    auto inherited=content.maps.begin()->second;
    const auto inheritedKey=inherited.sprites->begin()->first;
    const auto inheritedPath=inherited.sprites->at(inheritedKey).texturePath;
    SceneDefinition retained;
    {
        auto candidate=content;const auto& original=content.maps.begin()->second;
        retained=candidate.maps.begin()->second;retained.width+=1;
        need(retained.sprites==original.sprites,"Scene copy did not retain shared immutable sprites");
        auto modified=*retained.sprites;const auto key=modified.begin()->first;const auto old=original.sprites->at(key).texturePath;
        modified.at(key).texturePath="deliberately-missing-candidate.png";
        retained.sprites=std::make_shared<const SpriteDefinitions>(std::move(modified));
        need(original.sprites->at(key).texturePath==old,"Candidate mutation contaminated active source");
        need(candidate.maps.begin()->second.sprites==original.sprites,"Candidate scene replaced its source directory");
    }
    const auto changed=retained.sprites->begin()->second.texturePath;
    content=CombatContent{};
    need(!inheritedFull->contains("independent-variant-check")&&changedFull->at("independent-variant-check")==true,"Destroyed source invalidated independently retained full definitions");
    need(*inheritedCue==cueValue,"Destroyed source invalidated a retained cue leaf");
    need(inherited.sprites->at(inheritedKey).texturePath==inheritedPath,"Destroyed content invalidated its inherited shared directory");
    need(changed=="deliberately-missing-candidate.png"&&retained.sprites->begin()->second.texturePath==changed,"Destroyed content invalidated retained scene sprites");
    std::cout<<"Frozen sprite generations="<<generations<<" maps="<<maps<<" distinct_directories="<<directories.size()<<"; independent variant and destroyed source passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
