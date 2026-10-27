#include "full_content.h"
#include "content_hash.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
using Json=nlohmann::json;
CombatContent loadCycleContent(const std::filesystem::path& path,const Json& data){
    const auto need=[](bool ok,const char* text){if(!ok){throw std::runtime_error(text);}};
    need(data.size()==4&&data.at("schema")=="nobihaza.g7cycle.v1"&&data.at("content_compatibility")=="post-f5-g7-normal-7","Unknown cycle identity");
    const auto& source=data.at("source");
    need(source==Json{{"file","g7-06-source.json"},{"sha256","f1dae7bd564adff3fa1473ecf52d1bb2815d754e7fe93231608eb8f06455f0ff"}},"Unknown cycle predecessor");
    const auto& p=data.at("projection");need(contentSha256(p.dump())=="6fb14fdbebba284016c9c9f1474c0b082aa1eb57a2f47f56c90ed6f5e5505e6d","Unreviewed cycle projection");
    const auto file=path.parent_path()/source.at("file").get<std::string>();
    need(!std::filesystem::is_symlink(file)&&std::filesystem::is_regular_file(file)&&std::filesystem::file_size(file)<=128*1024*1024,"Missing cycle predecessor");
    std::ifstream input(file,std::ios::binary);const auto bytes=readContentBytes(input);need(contentSha256(bytes)==source.at("sha256").get<std::string>(),"Changed cycle predecessor");
    std::istringstream in(bytes);auto old=std::make_shared<const CombatContent>(loadCombatContentDefinition(file,parseCombatContentDefinition(in)));
    auto c=*old;c.legacySource=old;c.compatibility=data.at("content_compatibility");c.catalog["cycle"]=p;
    need(c.catalog.contains("opening")&&c.catalog.contains("clear")&&c.catalog.contains("special_equipment"),"Cycle dependencies missing");
    for(const auto& reward:p.at("rewards")){need(c.items.contains(reward.at("item"))&&c.facts.switches.contains(reward.at("flag")),"Cycle reward unavailable");}
    return c;
}
