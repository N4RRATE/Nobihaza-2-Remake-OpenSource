#include "content_hash.h"
#include <array>
#include <bit>
#include <cstdint>
#include <vector>
#include <istream>
#include <stdexcept>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#include <algorithm>
#include <limits>
#endif
std::string readContentBytes(std::istream& input){
    std::string bytes;std::array<char,64*1024> block;
    while(input){
        input.read(block.data(),block.size());const auto count=static_cast<std::size_t>(input.gcount());
        if(count>128*1024*1024-bytes.size()){throw std::runtime_error("Oversized content byte stream");}
        bytes.append(block.data(),count);
    }
    if(input.bad()||!input.eof()){throw std::runtime_error("Cannot read content byte stream");}
    return bytes;
}
std::string contentSha256(std::string_view bytes){
#ifdef __APPLE__
    CC_SHA256_CTX context;
    if(CC_SHA256_Init(&context)!=1){throw std::runtime_error("Cannot initialize content SHA256");}
    for(std::size_t offset=0;offset<bytes.size();){
        const auto count=std::min(bytes.size()-offset,static_cast<std::size_t>(std::numeric_limits<CC_LONG>::max()));
        if(CC_SHA256_Update(&context,bytes.data()+offset,static_cast<CC_LONG>(count))!=1){throw std::runtime_error("Cannot update content SHA256");}
        offset+=count;
    }
    std::array<unsigned char,CC_SHA256_DIGEST_LENGTH> digest{};
    if(CC_SHA256_Final(digest.data(),&context)!=1){throw std::runtime_error("Cannot finish content SHA256");}
    std::string result;result.reserve(64);constexpr char hex[]="0123456789abcdef";
    for(auto byte:digest){result+=hex[byte>>4];result+=hex[byte&15];}
    return result;
#else
    constexpr std::array<std::uint32_t,64> k{
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    std::array<std::uint32_t,8> h{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    std::vector<std::uint8_t> data(bytes.begin(),bytes.end());data.push_back(0x80);
    while(data.size()%64!=56){data.push_back(0);}
    const auto bits=static_cast<std::uint64_t>(bytes.size())*8;
    for(int n=7;n>=0;--n){data.push_back(static_cast<std::uint8_t>(bits>>(n*8)));}
    for(std::size_t start=0;start<data.size();start+=64){
        std::array<std::uint32_t,64> w{};
        for(int i=0;i<16;++i){for(int b=0;b<4;++b){w[i]=(w[i]<<8)|data[start+i*4+b];}}
        for(int i=16;i<64;++i){const auto x=w[i-15],y=w[i-2];w[i]=w[i-16]+(std::rotr(x,7)^std::rotr(x,18)^(x>>3))+w[i-7]+(std::rotr(y,17)^std::rotr(y,19)^(y>>10));}
        auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],q=h[7];
        for(int i=0;i<64;++i){
            const auto t1=q+(std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25))+((e&f)^(~e&g))+k[i]+w[i];
            const auto t2=(std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22))+((a&b)^(a&c)^(b&c));
            q=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=q;
    }
    std::string result;constexpr char hex[]="0123456789abcdef";
    for(auto word:h){for(int n=7;n>=0;--n){result+=hex[(word>>(n*4))&15];}}
    return result;
#endif
}
