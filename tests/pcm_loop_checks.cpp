#include "pcm_loop.h"
#include <array>
#include <iostream>
void require(bool value){if(!value){throw std::runtime_error("PCM loop check failed");}}
int main(){
 const std::array<std::int16_t,12> stereo{0,10,1,11,2,12,3,13,4,14,5,15};PcmLoop reader(stereo,2,1,3,6);
 std::array<std::int16_t,20> out;reader.read(out);
 require(out==std::array<std::int16_t,20>{1,11,2,12,3,13,4,14,5,15,3,13,4,14,5,15,3,13,4,14});
 reader.read(std::span(out).first(4));require(out[0]==5&&out[1]==15&&out[2]==3&&out[3]==13);
 reader.seek(0);reader.read(std::span(out).first(2));require(out[0]==0&&out[1]==10);
 PcmLoop mono(std::span(stereo).first(4),1,0,1,3);mono.read(std::span(out).first(5));require(out[0]==0&&out[1]==10&&out[2]==1&&out[3]==10&&out[4]==1);
 bool rejected=false;try{PcmLoop invalid(stereo,2,6,3,6);}catch(const std::exception&){rejected=true;}require(rejected);
 std::cout<<"Predecoded PCM loop checks passed\n";
}
