#pragma once
#include "game_application.h"
#include "predecoded_music.h"
#include "dialogue_renderer.h"
#include "original_window_skin.h"
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <random>
class CombatPresentation{
public:
    CombatPresentation(const CombatContent& content,const sf::Font& font);
    void update(const std::vector<PresentationEvent>& events,bool advanceWorld=true);
    void draw(sf::RenderTarget& target,const GameApplication& scene);
    float saturation() const{return saturation_;}
    float shakeOffset() const;
    SavedPresentation saveState() const;
    bool musicPlaying() const{return music_&&music_->getStatus()==sf::SoundSource::Status::Playing;}
    std::optional<std::pair<MusicPlaybackIdentity,int>> musicTicks() const{if(musicPlayback_&&music_){if(auto ticks=music_->midiTicks()){return std::pair{*musicPlayback_,*ticks};}}return {};}
    std::optional<MusicPlaybackIdentity> musicFirstPass() const{return musicPlayback_&&music_&&music_->firstPassComplete()?musicPlayback_:std::nullopt;}
    void prepareRestoredState(const SavedPresentation&);
    void activateRestoredState();
    void drawBackground(sf::RenderTarget& target);
    void drawBackground(sf::RenderTarget& target,const GameApplication& scene);
    const std::map<std::string,sf::Texture>& images() const;
    const sf::Image& palette() const;
    const std::string& systemImage() const{return systemImage_;}
    std::map<std::string,sf::Color> characterFlashes() const;
private:
    struct Effect{std::string id;GridPosition anchor;int age{};bool animation{};std::string actor{};bool clipChecked{},clipped{};};
    struct Picture{
        bool mapFixed{},worldAnchored{};bool eraseOnMapChange{true};float rotation{};float rotationSpeed{};
        std::string image;sf::Vector2f position{160,120};float zoom{100};float opacity{};
        sf::Color color{sf::Color::White};sf::Vector2f startPosition,targetPosition;
        float startZoom{},targetZoom{},startOpacity{},targetOpacity{};
        sf::Color startColor,targetColor;int remaining{},total{};float wave{},startWave{},targetWave{};int phase{};
    };
    nlohmann::json warehouseVisual() const;
    void restoreWarehouseVisual(const nlohmann::json&);
    void sound(const SoundDefinition& sound);
    void advanceRain();
    void drawRain(sf::RenderTarget& target, sf::Vector2f camera);
    std::unique_ptr<PredecodedMusic> prepareMusic(const SoundDefinition&,bool observeFirstPass=false) const;
    void cue(const PresentationEvent& event);
    void drawScrollTransition(sf::RenderTarget& target);
    void drawText(sf::RenderTarget& target,const std::string& text,float x,float y,sf::Color color=sf::Color::White) const;
    void drawUiWindow(sf::RenderTarget&,sf::FloatRect) const;
    void drawUiCursor(sf::RenderTarget&,sf::FloatRect) const;
    void drawUiLine(sf::RenderTarget&,const std::string&,sf::FloatRect,int color=0,bool scroll=false,bool gradient=false) const;
    sf::Color uiColor(int index) const;
    void drawFormalMenu(sf::RenderTarget& target,const GameApplication& game);
    void drawInterface(sf::RenderTarget& target,const GameApplication& scene);
    const CombatContent& content_;const sf::Font& font_;
    std::map<std::string,sf::Texture> images_;
    std::map<std::string,OriginalWindowSkin> windowSkins_;
    unsigned uiFrame_{};
    mutable float titleTextScale_{};
    mutable std::string titleTextSystem_;
    mutable std::map<std::pair<std::string,int>,sf::Texture> titleText_;
    std::map<std::pair<std::string,std::array<int,4>>,sf::Texture> tonedImages_;
    sf::Image palette_;
    std::map<std::string,sf::SoundBuffer> buffers_;
    std::vector<std::unique_ptr<sf::Sound>> sounds_;
    // Decode the current track before playback: SFML 3.1 streams can miss
    // macOS's short audio deadline (upstream #3734). Sound must die first.
    std::unique_ptr<PredecodedMusic> music_;
    SoundDefinition musicDescription_;
    std::optional<MusicPlaybackIdentity> musicPlayback_;
    std::string systemImage_;
    bool rememberFadedMusic_{};
    int musicFade_{};int musicFadeTotal_{};float musicVolume_{100};
    std::vector<Effect> effects_;
    std::map<int,Picture> pictures_;
    struct Flash{sf::Color color;int remaining;int total;};
    std::map<std::string,Flash> flashes_;
    std::string notice_;int noticeTicks_{};
    float black_{};float blackTarget_{};
    int scrollDirection_{},scrollFrame_{},scrollDuration_{};bool scrollShowing_{},scrollCaptured_{};
    sf::Texture scrollScreen_;sf::IntRect scrollViewport_;
    std::string panorama_;
    std::array<float,3> screenTone_{90,90,90},startTone_{90,90,90},targetTone_{90,90,90};
    float saturation_{100},startSaturation_{100},targetSaturation_{100};
    struct RainParticle{int x{},y{},life{};};
    std::vector<RainParticle> rain_;
    std::minstd_rand rainRandom_{17431}; // Independent presentation randomness.
    std::optional<sf::Vector2f> rainCameraOrigin_;
    int weatherType_{},weatherStrength_{},shakeTicks_{},shakeAge_{},shakeStrength_{},shakeSpeed_{};
    int toneRemaining_{},toneDuration_{};
    sf::Color screenFlash_{sf::Color::Transparent};int flashRemaining_{},flashDuration_{};
};
int runCombatGame(CombatContent content,const std::string& entry,int frameLimit=0,const std::filesystem::path& saveDirectory={});
