#include "save_platform.h"
#import <Foundation/Foundation.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdexcept>
#include <cerrno>
#include <cstring>
namespace{
    [[noreturn]] void fail(const char* operation){throw std::runtime_error(std::string(operation)+": "+std::strerror(errno));}
    struct Descriptor{int fd;~Descriptor(){if(fd>=0){::close(fd);}}};
}
std::filesystem::path defaultSaveDirectory(){
    @autoreleasepool{
        NSURL* root=[[[NSFileManager defaultManager] URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask] firstObject];
        if(!root){throw std::runtime_error("Application Support unavailable");}
        return std::filesystem::path([[root path] fileSystemRepresentation])/"Nobihaza2Remake"/"saves";
    }
}
namespace save_platform{
std::string read(const std::filesystem::path& path){
    Descriptor f{::open(path.c_str(),O_RDONLY|O_NOFOLLOW)};if(f.fd<0){fail("open save");}struct stat st{};
    if(::fstat(f.fd,&st)!=0){fail("stat save");}if(!S_ISREG(st.st_mode)||st.st_size<0||st.st_size>8*1024*1024){throw std::runtime_error("Not a regular save within 8 MiB");}
    std::string out(static_cast<std::size_t>(st.st_size),'\0');std::size_t done=0;
    while(done<out.size()){const auto n=::read(f.fd,out.data()+done,out.size()-done);if(n<0&&errno==EINTR){continue;}if(n<=0){fail("read save");}done+=n;}
    char extra;if(::read(f.fd,&extra,1)!=0){throw std::runtime_error("Save changed while reading");}
    const int fd=f.fd;f.fd=-1;if(::close(fd)!=0){fail("close save");}return out;
}
void writeExclusive(const std::filesystem::path& path,const std::string& bytes,const Fault& fault){
    Descriptor f{::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600)};if(f.fd<0){fail("create save candidate");}
    if(fault){fault("write");}std::size_t done=0;while(done<bytes.size()){const auto n=::write(f.fd,bytes.data()+done,bytes.size()-done);if(n<0&&errno==EINTR){continue;}if(n<=0){fail("write save");}done+=n;}
    if(fault){fault("flush");}if(::fsync(f.fd)!=0){fail("sync save");}
    if(::fcntl(f.fd,F_FULLFSYNC)!=0){fail("full sync save");}
    if(fault){fault("close");}const int fd=f.fd;f.fd=-1;if(::close(fd)!=0){fail("close candidate");}
}
void replace(const std::filesystem::path& temporary,const std::filesystem::path& destination){if(::rename(temporary.c_str(),destination.c_str())!=0){fail("replace save");}}
void archiveCopy(const std::filesystem::path& from,const std::filesystem::path& to){
    if(!std::filesystem::is_regular_file(std::filesystem::symlink_status(from))){throw std::runtime_error("Archive source is not regular");}
    std::filesystem::copy_file(from,to);Descriptor f{::open(to.c_str(),O_RDONLY|O_NOFOLLOW)};if(f.fd<0||::fsync(f.fd)!=0||::fcntl(f.fd,F_FULLFSYNC)!=0){fail("sync recovery archive");}
}
void syncDirectory(const std::filesystem::path& path){Descriptor f{::open(path.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW)};if(f.fd<0||::fsync(f.fd)!=0){fail("sync save directory");}}
}
