#pragma once
#include <SFML/Audio.hpp>
#include <nlohmann/json.hpp>
#include <array>
#include <algorithm>
// A finite pre-rendered MIDI instance keeps consuming silence at an end-loop.
// Ticks follow the selected fixed Player message backend's mtime, never game frames.
class EndMidiMusic final:public sf::SoundStream{
public:
    EndMidiMusic(const sf::SoundBuffer& pcm,const nlohmann::json& timing):pcm_(pcm),timing_(timing),initial_(timing.at("initial_frame")),cursor_(initial_),end_(timing.at("end_frame")){
        if(pcm.getSampleRate()!=44100||pcm.getChannelCount()!=2||end_!=pcm.getSampleCount()/2||initial_>=end_||timing.at("profile")!="fixed-player-message-midi-end-v1"||timing.at("pitch")!=50){throw std::runtime_error("Invalid end MIDI PCM profile");}
        initialize(2,44100,pcm.getChannelMap());
    }
    ~EndMidiMusic() override{stop();}
    int ticks() const{
        const auto elapsed=std::max<std::int64_t>(0,getPlayingOffset().asMicroseconds());
        const std::int64_t consumed=elapsed*44100/1000000;
        const std::int64_t quanta=consumed/64;
        const std::int64_t initialTime=timing_.at("initial_mtime_us"),endTime=timing_.at("end_mtime_us");
        const std::int64_t tempoTime=initialTime+725;
        const float rate=96.f/666666.f;
        if(quanta==0){return static_cast<int>((96.f/500000.f)*initialTime);}
        const auto current=consumed>=static_cast<std::int64_t>(end_-initial_)?endTime:std::min(endTime,initialTime+quanta*725);
        return static_cast<int>(rate*tempoTime)+static_cast<int>(rate*(current-tempoTime));
    }
private:
    bool onGetData(Chunk& chunk) override{
        constexpr std::size_t frames=512;std::fill(chunk_.begin(),chunk_.end(),0);
        const auto count=std::min(frames,end_-cursor_);std::copy_n(pcm_.getSamples()+cursor_*2,count*2,chunk_.begin());cursor_+=count;
        chunk={chunk_.data(),chunk_.size()};return true;
    }
    void onSeek(sf::Time offset) override{cursor_=std::min(end_,initial_+static_cast<std::size_t>(std::max<std::int64_t>(0,offset.asMicroseconds()))*44100/1000000);}
    const sf::SoundBuffer& pcm_;nlohmann::json timing_;std::size_t initial_,cursor_,end_;std::array<std::int16_t,1024> chunk_{};
};
