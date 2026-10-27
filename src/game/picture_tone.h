#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
// Source picture percentages use neutral100, converted to the engine's neutral128.
// Fixed Player0de2a9ab: sprite_picture.cpp168-171, bitmap.cpp841-879.
inline std::array<std::uint8_t,4> pictureTonePixel(std::array<std::uint8_t,4> pixel,const std::array<int,4>& percent){
    std::array<int,4> tone{};
    for(int i=0;i<4;++i){if(percent[i]<0||percent[i]>200){throw std::invalid_argument("Unsupported picture tone percentage");}tone[i]=std::min(255,percent[i]*128/100);}
    std::array<int,3> color{pixel[0],pixel[1],pixel[2]};
    if(tone[3]!=128){
        const int luminance=(19595*color[0]+38470*color[1]+7471*color[2])>>16;
        const int saturation=tone[3]>128?1024+16*(tone[3]-128):tone[3]*8;
        for(auto& channel:color){channel=std::clamp((luminance*1024+(channel-luminance)*saturation)>>10,0,255);}
    }
    for(int i=0;i<3;++i){
        const int result=tone[i]<=128?2*tone[i]*color[i]/255:255-2*(255-tone[i])*(255-color[i])/255;
        pixel[i]=static_cast<std::uint8_t>(std::clamp(result,0,255));
    }
    return pixel;
}
