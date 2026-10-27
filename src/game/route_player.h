#pragma once
#include "exploration_scene.h"
#include "presentation_event.h"
#include <functional>
class RoutePlayer{
public:
    RoutePlayer(RouteDefinition definition,std::string actor,std::string source={});
    bool tick(ExplorationScene& world,GameSession& session,std::vector<PresentationEvent>& events,
              const std::function<int(int,int)>& random,
              const std::function<void(int,bool)>& setSwitch={});
    const std::string& actor() const;
    const std::string& source() const;
    bool firstPassComplete() const{return firstPassComplete_;}
    bool contactedPlayer() const{return contactedPlayer_;}
    void refreshDefinition(RouteDefinition definition);
    std::optional<Direction> translationDirection() const;
private:
    friend class WarehouseSaveState;
    RouteDefinition definition_;
    std::string actor_;
    std::string source_;
    std::size_t cursor_{};
    int wait_{};
    int frequency_{};
    int speed_{};
    bool locked_{};
    bool firstPassComplete_{};
    bool contactedPlayer_{};
};
CharacterState findCharacter(const ExplorationScene& world,const std::string& id);
void replaceCharacter(ExplorationScene& world,const CharacterState& actor);
Direction faceToward(TilePosition from,TilePosition to);
