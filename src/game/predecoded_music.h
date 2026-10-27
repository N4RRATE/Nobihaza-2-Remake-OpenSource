#pragma once
#include "pcm_loop.h"
#include "end_midi_music.h"
#include <SFML/Audio.hpp>
#include <array>
#include <memory>
#include <optional>
#include <atomic>
struct MusicLoop{std::size_t initial{},begin{},end{};};
class PredecodedMusic{
    class Stream final:public sf::SoundStream{
    public:
        Stream(const sf::SoundBuffer& buffer,MusicLoop loop,bool observe)
            :reader_({buffer.getSamples(),static_cast<std::size_t>(buffer.getSampleCount())},buffer.getChannelCount(),loop.initial,loop.begin,loop.end),channels_(buffer.getChannelCount()),loop_(loop),observe_(observe){
            if(channels_>8){throw std::runtime_error("Unsupported PCM channel count");}
            initialize(channels_,buffer.getSampleRate(),buffer.getChannelMap());
            setPlayingOffset(sf::microseconds(static_cast<std::int64_t>(loop.initial)*1000000/buffer.getSampleRate()));
            reader_.seek(loop.initial);
            setLooping(observe);
        }
        ~Stream() override{stop();}
        bool firstPassComplete() const{return firstPass_.load(std::memory_order_acquire);}
    private:
        bool onGetData(Chunk& chunk) override{
            // Fixed bounded copy only. No file access, decoder, allocation or synthesis in callback.
            const std::size_t frames=observe_?std::min<std::size_t>(512,reader_.framesUntilLoop()):512;
            const bool boundary=observe_&&frames==reader_.framesUntilLoop();
            const std::size_t count=frames*channels_;reader_.read({chunk_.data(),count});chunk={chunk_.data(),count};return !boundary;
        }
        std::optional<std::uint64_t> onLoop() override{
            // SFML 3.1 calls this after the last chunk has been consumed, not when it is prepared.
            reader_.seek(loop_.begin);firstPass_.store(true,std::memory_order_release);return loop_.begin*channels_;
        }
        void onSeek(sf::Time offset) override{reader_.seek(static_cast<std::size_t>(std::max<std::int64_t>(0,offset.asMicroseconds()))*getSampleRate()/1000000);}
        PcmLoop reader_;unsigned channels_;MusicLoop loop_;bool observe_;std::atomic<bool> firstPass_{false};std::array<std::int16_t,4096> chunk_{};
    };
public:
    PredecodedMusic(const std::filesystem::path& path,std::optional<MusicLoop> loop,bool observeFirstPass=false,const nlohmann::json& endMidi=nullptr){
        if(!buffer_.loadFromFile(path)){throw std::runtime_error("Cannot decode BGM: "+path.string());}
        if(!endMidi.is_null()){endMidi_=std::make_unique<EndMidiMusic>(buffer_,endMidi);}
        else if(loop||observeFirstPass){stream_=std::make_unique<Stream>(buffer_,loop.value_or(MusicLoop{0,0,static_cast<std::size_t>(buffer_.getSampleCount()/buffer_.getChannelCount())}),observeFirstPass);}
        else{sound_=std::make_unique<sf::Sound>(buffer_);sound_->setLooping(true);}
    }
    void play(){if(endMidi_){endMidi_->play();}else if(stream_){stream_->play();}else{sound_->play();}}
    sf::SoundSource::Status getStatus() const{return endMidi_?endMidi_->getStatus():stream_?stream_->getStatus():sound_->getStatus();}
    void stop(){if(endMidi_){endMidi_->stop();}else if(stream_){stream_->stop();}else{sound_->stop();}}
    void setVolume(float value){source().setVolume(value);}
    void setPitch(float value){source().setPitch(value);}
    void setPan(float value){source().setPan(value);}
    std::optional<int> midiTicks() const{return endMidi_?std::optional<int>{endMidi_->ticks()}:std::nullopt;}
    bool firstPassComplete() const{return stream_&&stream_->firstPassComplete();}
private:
    sf::SoundSource& source(){return endMidi_?static_cast<sf::SoundSource&>(*endMidi_):stream_?static_cast<sf::SoundSource&>(*stream_):static_cast<sf::SoundSource&>(*sound_);}
    sf::SoundBuffer buffer_;std::unique_ptr<EndMidiMusic> endMidi_;std::unique_ptr<sf::Sound> sound_;std::unique_ptr<Stream> stream_;
};
