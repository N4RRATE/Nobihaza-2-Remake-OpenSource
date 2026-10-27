#include "branch_content.h"
#include "combat_content.h"
#include "content_hash.h"
#include <stdexcept>
#include <set>
void loadBranchReturns(CombatContent& c){
    const auto& lock=c.branch.at("episodes").at("spade_lock");
    nlohmann::json reviewed={{"lock",lock},{"flows",nlohmann::json::object()}};
    for(const auto& [id,flow]:c.branch.at("additional_flows").items()){
        if(c.objects.at(flow.at("object").get<std::string>()).map==25){reviewed["flows"][id]=flow;}
    }
    if(contentSha256(reviewed.dump())!="c82e7fb16081f352cfbccdb35c92e62fad0ad402272643086eee7ca33e903fe2"){throw std::runtime_error("Unreviewed cooperation return rules");}
    const auto& choice=c.cue(lock.at("discard_choice").get<std::string>());
    if(choice.at("kind")!="choices"||choice.at("cancel_type")!=0||choice.at("choices").size()!=2){throw std::runtime_error("Invalid key discard choice");}
    if(!c.messages.contains(lock.at("discard_message").get<std::string>())){throw std::runtime_error("Missing key discard message");}
    const std::set<std::string> kinds{"flag","cue","message","grant","system_graphic","erase_event_once","enable_combine_if_known"};
    const auto validate=[&](const nlohmann::json& steps){for(const auto& step:steps){
        const std::string kind=step.at("kind");if(!kinds.contains(kind)){throw std::runtime_error("Unsupported return operation");}
        if(kind=="message"&&!c.messages.contains(step.at("message").get<std::string>())){throw std::runtime_error("Missing return message");}
        if(kind=="cue"){const auto id=step.at("cue").get<std::string>();const auto& cue=c.cue(id);if(cue.contains("runtime_support")){throw std::runtime_error("Unsupported return cue");}if(cue.at("kind")=="forced_route"&&!c.routes.contains(id)){throw std::runtime_error("Missing return route");}}
        if(kind=="grant"&&!c.items.contains(step.at("item").get<int>())){throw std::runtime_error("Missing return item");}
    }};
    validate(lock.at("before"));validate(lock.at("after"));for(const auto& flow:reviewed.at("flows")){validate(flow.at("steps"));}
    auto& flows=c.catalog["flows"];
    for(const auto& [id,flow]:reviewed.at("flows").items()){flows[id]=flow;}
    auto steps=lock.at("before");steps.push_back({{"kind","message"},{"message",lock.at("discard_message")}});
    steps.push_back({{"kind","discard_key"},{"item",lock.at("key_item")},{"choice",lock.at("discard_choice")}});
    for(const auto& step:lock.at("after")){steps.push_back(step);}
    flows["spade-unlock"]={{"object",lock.at("object")},{"page",lock.at("page")},{"trigger","action"},{"steps",steps}};
    if(!c.enemies.contains("m63-e12")||!flows.contains("rescue-gian-timely")||!flows.contains("rescue-gian-intervened")||!c.catalog.contains("rescue_crises")){throw std::runtime_error("Incomplete rescue capability for natural key pickup");}
    flows["elements-key"]["runtime_enabled"]=true;flows["elements-key"].erase("implementation_ticket");
}
