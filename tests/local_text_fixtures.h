#pragma once
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

// Source-expression goldens remain local; missing inputs must fail rather than pass vacuously.
inline std::string localSourceText(const char* key){
    static const auto data=[](){
        const char* environment=std::getenv("NOBIHAZA_TEXT_FIXTURES");
#ifdef NOBIHAZA_LOCAL_TEXT_FIXTURES
        const auto path=std::filesystem::path(environment?environment:NOBIHAZA_LOCAL_TEXT_FIXTURES);
#else
        if(!environment){throw std::runtime_error("Set NOBIHAZA_TEXT_FIXTURES to lawful local test goldens");}
        const auto path=std::filesystem::path(environment);
#endif
        if(std::filesystem::is_symlink(path)||!std::filesystem::is_regular_file(path)||std::filesystem::file_size(path)>128*1024){throw std::runtime_error("Missing/invalid local source text fixtures: "+path.string());}
        std::ifstream input(path);auto result=nlohmann::json::parse(input);
        if(!result.is_object()){throw std::runtime_error("Local source text fixtures must be an object");}return result;
    }();
    return data.at(key).get<std::string>();
}
