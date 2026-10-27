#include "full_content.h"
#include "content_hash.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

CombatContent loadExFontContent(const std::filesystem::path& path,const nlohmann::json& data){
    using Json=nlohmann::json;
    const auto need=[](bool ok,const char* message){if(!ok){throw std::runtime_error(message);}};
    need(data.size()==4&&data.at("schema")=="nobihaza.g9text.v1","Unknown ExFont revision");
    const auto& source=data.at("source");const auto& p=data.at("projection");
    // Later audited worlds extend this input registry; glyph rules and save fields do not change.
    const std::map<std::string,std::string> predecessors{
        {"22080d66890c11667ffe600bdfda598403cd872947507f3c6bc35c85ef45b17d","post-f5-g8-region-3"},
        {"80c86a59eb2188c7c9de96d694682c9efdaef47019f8c6f512f79fa342eade29","post-f5-g8-region-3"}};
    need(source.size()==2&&source.at("file")=="g8.json"&&predecessors.contains(source.at("sha256").get<std::string>())&&
         data.at("content_compatibility")==predecessors.at(source.at("sha256").get<std::string>()),"Unreviewed ExFont predecessor");
    need(p.size()==3&&p.contains("images")&&p.contains("messages")&&p.contains("resources")&&
         contentSha256(p.dump())=="54a705ae50e7585c623b3c697701ab4cecfc91b2d0920281cdd7080e7f27f4db","Unreviewed ExFont projection");
    const auto verified=[&](const std::string& name,const std::string& sha,std::uintmax_t limit){
        need(!name.empty()&&std::filesystem::path(name).filename()==name,"Invalid ExFont resource path");
        const auto file=path.parent_path()/name;
        need(!std::filesystem::is_symlink(file)&&std::filesystem::is_regular_file(file)&&std::filesystem::file_size(file)<=limit,"Missing or oversized ExFont resource");
        std::ifstream input(file,std::ios::binary);need(bool(input),"Cannot read ExFont resource");
        const auto bytes=readContentBytes(input);need(contentSha256(bytes)==sha,"Changed ExFont resource");return bytes;
    };
    for(const auto& [name,sha]:p.at("resources").items()){verified(name,sha,128*1024);}
    std::istringstream input(verified(source.at("file"),source.at("sha256"),128*1024*1024));
    auto c=loadCombatContentDefinition(path.parent_path()/source.at("file").get<std::string>(),parseCombatContentDefinition(input));
    need(c.compatibility==data.at("content_compatibility").get<std::string>(),"ExFont revision changed save compatibility");
    for(const auto& [id,row]:p.at("images").items()){
        need(!c.media.at("images").contains(id),"ExFont revision shadows an existing image");
        c.media["images"][id]=row;
    }
    for(const auto& [id,row]:p.at("messages").items()){
        need(c.messages.contains(id),"ExFont revision adds an unknown message");
        const auto& previous=c.messages.at(id);auto replacement=readCombatMessage(row);
        need(previous.inheritDisplay==replacement.inheritDisplay&&previous.inheritFace==replacement.inheritFace&&
             previous.message.display==replacement.message.display&&previous.message.faceImage==replacement.message.faceImage&&
             previous.message.faceIndex==replacement.message.faceIndex&&previous.message.faceRight==replacement.message.faceRight&&
             previous.message.faceFlipped==replacement.message.faceFlipped,"ExFont revision changes message layout");
        need(previous.message.lines.size()==replacement.message.lines.size(),"ExFont revision changes message pages");
        for(std::size_t line=0;line<previous.message.lines.size();++line){
            need(previous.message.lines[line].raw==replacement.message.lines[line].raw,"ExFont revision changes original text");
        }
        c.messages.at(id)=std::move(replacement);
    }
    validateSceneContent(c,*c.maps.begin()->second.sprites);
    return c;
}
