#pragma once
#include <string>
#include <nlohmann/json.hpp>
class CombatScene;
// Bounded presentation execution shared by opening, exploration and meeting flows.
// The scene owns actor motion and pending requests. This borrowed service owns no copies.
class ScenePresentation{
public:
    explicit ScenePresentation(CombatScene& scene):scene_(scene){}
    void message(const std::string&);
    void sound(const std::string&);
    void route(const std::string&,const std::string& self={});
    void cue(const std::string&,const std::string& self={});
    // False means a forced-route barrier has not completed; caller retains its cursor.
    bool executeCue(const std::string&,const std::string& self={});
    void advanceRoutes();
    void validateSequence(const nlohmann::json& steps,const std::string& self,bool allowKeyCall=true) const;
private:
    CombatScene& scene_;
};
