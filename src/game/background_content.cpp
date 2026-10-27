#include "full_content.h"
#include "content_hash.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

CombatContent loadBackgroundContent(const std::filesystem::path& path,const nlohmann::json& data){
    using Json=nlohmann::json;
    const auto need=[](bool ok,const char* message){if(!ok){throw std::runtime_error(message);}};
    const bool mansion=data.at("content_compatibility")=="post-f5-g8-late-4";
    need(data.size()==4&&data.at("schema")=="nobihaza.backgrounds.v1"&&(mansion||data.at("content_compatibility")=="post-f5-g7-normal-7"),"Unknown background revision");
    const auto& source=data.at("source");const auto& p=data.at("projection");
    const Json predecessor=mansion
        ?Json{{"file","g8-04-source.json"},{"sha256","79564718bb09133bda39383f6d432ed5ed611963df3a6477da6c718e4271b7ca"}}
        :Json{{"file","g7-08-source.json"},{"sha256","329dbd0afe56ef6ae8c3d0bf04fdeceb191bf35a587ed71d3503f392300f8b9f"}};
    need(source==predecessor,"Unknown background predecessor");
    // This revision may only add the reviewed opaque images and default backgrounds.
    need(p.size()==3&&p.contains("images")&&p.contains("default_panoramas")&&p.contains("resources")&&contentSha256(p.dump())==(mansion?"1b62e4233668a19b42c4bd6f66ecbb752e3d80f8d536fec17980a03cefac1d8d":"b3a1f69b2d7d1886fd98178cce5f4bc56bfb794e4bb950fe6eb450801056aeb3"),"Unreviewed background projection");
    const auto verified=[&](const std::string& name,const std::string& sha){
        need(std::filesystem::path(name).filename()==name,"Invalid background resource path");
        const auto file=path.parent_path()/name;
        need(!std::filesystem::is_symlink(file)&&std::filesystem::is_regular_file(file)&&std::filesystem::file_size(file)<=128*1024*1024,"Missing background resource");
        std::ifstream in(file,std::ios::binary);need(bool(in),"Cannot read background resource");
        const auto bytes=readContentBytes(in);need(contentSha256(bytes)==sha,"Changed background resource");return bytes;
    };
    for(const auto& [name,sha]:p.at("resources").items()){verified(name,sha);}
    std::istringstream input(verified(source.at("file"),source.at("sha256")));
    auto c=loadCombatContentDefinition(path.parent_path()/source.at("file").get<std::string>(),parseCombatContentDefinition(input));
    for(const auto& [id,row]:p.at("images").items()){
        need(!c.media.at("images").contains(id),"Background revision shadows image");c.media["images"][id]=row;
    }
    for(const auto& [id,row]:p.at("default_panoramas").items()){
        auto& map=c.maps.at(std::stoi(id));need(!map.defaultPanorama,"Background revision shadows existing default");
        map.defaultPanorama=PanoramaDefinition{row.at("image"),row.at("loop_x"),row.at("loop_y"),row.at("auto_x"),row.at("auto_y"),row.at("speed_x"),row.at("speed_y")};
    }
    // Same content identity and legacy chain: gameplay and legal save reconstruction are unchanged.
    return c;
}
