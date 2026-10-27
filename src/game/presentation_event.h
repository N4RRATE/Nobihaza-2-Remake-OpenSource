#pragma once
#include "combat_content.h"
#include <optional>
struct ResolvedPicture{GridPosition position;int transparency{};int durationTenths{};};
struct MusicPlaybackIdentity{std::uint64_t scene{},instance{};std::string resource;bool operator==(const MusicPlaybackIdentity&) const=default;};
struct PresentationEvent{
    enum class Kind{ ResetGame, SystemGraphic, StopSounds, SceneChanged, Sound, Music, StopMusic, FadeMusic, Animation, Effect, Cue, Notice };
    Kind kind{Kind::Cue};
    std::string id;
    SoundDefinition sound;
    GridPosition position;
    int duration{};
    std::string actor{};
    std::optional<GridPosition> sampledScreenPosition{};
    std::optional<ResolvedPicture> resolvedPicture{};
    std::optional<GridPosition> pictureMapOrigin{};
    int transition{-1};
    std::optional<MusicPlaybackIdentity> musicPlayback{};
};
