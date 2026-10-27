#pragma once
#include <algorithm>
#include <cstdint>
#include <span>
#include <stdexcept>
// Positions are PCM frames; interleaved sample offsets are private to the reader.
class PcmLoop{
public:
    PcmLoop(std::span<const std::int16_t> samples,unsigned channels,std::size_t initial,std::size_t begin,std::size_t end)
        :samples_(samples),channels_(channels),initial_(initial),begin_(begin),end_(end),cursor_(initial){
        if(!channels||samples.size()%channels||begin>=end||end>samples.size()/channels||initial>=end){throw std::runtime_error("Invalid PCM loop range");}
    }
    void read(std::span<std::int16_t> output){
        if(output.size()%channels_){throw std::runtime_error("PCM output is not frame aligned");}
        std::size_t offset=0;
        while(offset<output.size()){
            const auto count=std::min((end_-cursor_)*channels_,output.size()-offset);
            std::copy_n(samples_.data()+cursor_*channels_,count,output.data()+offset);
            offset+=count;cursor_+=count/channels_;if(cursor_==end_){cursor_=begin_;}
        }
    }
    void seek(std::size_t frame){cursor_=frame<end_?frame:begin_+(frame-begin_)%(end_-begin_);}
    std::size_t initial() const{return initial_;}
    std::size_t framesUntilLoop() const{return end_-cursor_;}
private:
    std::span<const std::int16_t> samples_;unsigned channels_;std::size_t initial_,begin_,end_,cursor_;
};
