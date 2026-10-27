#include "content_hash.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

int main(){
    const auto check=[](bool ok){if(!ok){throw std::runtime_error("content byte read check failed");}};
    for(const auto size:{0,1,65535,65536,65537,200000}){
        std::string bytes(size,'x');if(size){bytes[size/2]='\0';}
        std::istringstream input(bytes);check(readContentBytes(input)==bytes);
    }
    std::istringstream positioned("prefix remainder");positioned.seekg(7);
    check(readContentBytes(positioned)=="remainder");
    auto rejects=[&](std::istream& input){bool rejected=false;try{readContentBytes(input);}catch(const std::exception&){rejected=true;}check(rejected);};
    std::istringstream failed("partial");failed.setstate(std::ios::badbit);rejects(failed);
    std::istringstream missing;missing.setstate(std::ios::failbit);rejects(missing);
    struct FailsAfterBlock : std::stringbuf{
        FailsAfterBlock():std::stringbuf(std::string(65537,'x')){}
        int reads=0;
        std::streamsize xsgetn(char* output,std::streamsize count) override{
            if(++reads==2){throw std::runtime_error("injected read failure");}
            return std::stringbuf::xsgetn(output,count);
        }
    } buffer;
    std::istream partial(&buffer);rejects(partial);
    std::istringstream oversized(std::string(128*1024*1024+1,'x'));rejects(oversized);
    std::istringstream content("abc");check(contentSha256(readContentBytes(content))=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    check(contentSha256("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    check(contentSha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")=="248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    check(contentSha256(std::string(1000000,'a'))=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    std::cout<<"content byte boundaries, read failure, size and SHA checks passed\n";
}
