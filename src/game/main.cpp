#include "game.h"
#include "scene_definition.h"
#include "combat_presentation.h"
#include "app_resources.h"
#include <fstream>
#include <nlohmann/json.hpp>

#include <exception>
#include <iostream>
#include <chrono>
#include <cstdlib>

int main(int argc, char* argv[]){
    const auto started=std::chrono::steady_clock::now();
    const bool bundled=argc<2||std::string(argv[1]).starts_with("--");
    std::filesystem::path contentPath;
    try{
        contentPath=bundled?bundledContentPath():std::filesystem::path(argv[1]);
        std::ifstream input(contentPath);
        if(!input){throw std::runtime_error("Cannot open scene file or combat data");}
        const auto header=nlohmann::json::parse(input);
        if(header.value("schema","")!="nobihaza.exploration.v1"){
            std::string entry=bundled?"title":"natural";int frames=0;auto saveDirectory=defaultSaveDirectory();
            for(int i=bundled?1:2;i<argc;++i){
                const std::string option=argv[i];
                if((option=="--entry"||option=="--frames"||option=="--save-dir")&&i+1<argc){
                    const std::string value=argv[++i];if(option=="--entry"){entry=value;}else if(option=="--save-dir"){if(value.empty()){throw std::runtime_error("save directory must not be empty");}saveDirectory=value;}else{frames=std::stoi(value);if(frames<=0){throw std::runtime_error("frames must be positive");}}
                }else{throw std::runtime_error("Unknown or incomplete argument: "+option);}
            }
            auto content=loadCombatContent(contentPath);
            if(std::getenv("NOBIHAZA_STARTUP_PROFILE")){std::cerr<<"startup.content_ready_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()<<'\n';}
            return runCombatGame(std::move(content),entry,frames,saveDirectory);
        }
        if(bundled){throw std::runtime_error("Bundled runtime content must be combat data");}
        if(argc!=2){throw std::runtime_error("Exploration expects exactly one scene file");}
        const auto scene = loadSceneDefinition(contentPath);
        Game game(scene);
        return game.run();
    }catch(const std::exception& error){
        std::cerr << "Startup failed (scene: " << (contentPath.empty()?"bundled runtime":contentPath.string())
                  << "): " << error.what() << '\n';
        return 1;
    }
}
