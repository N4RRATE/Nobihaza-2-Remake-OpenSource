#include "save_platform.h"
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <stdexcept>
namespace{
    [[noreturn]] void fail(const char* operation){throw std::runtime_error(std::string(operation)+" Windows error "+std::to_string(GetLastError()));}
    struct Handle{HANDLE value{INVALID_HANDLE_VALUE};~Handle(){if(value!=INVALID_HANDLE_VALUE){CloseHandle(value);}}};
    void regular(HANDLE h){BY_HANDLE_FILE_INFORMATION info{};if(!GetFileInformationByHandle(h,&info)){fail("inspect save");}if(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)){throw std::runtime_error("Save reparse point or directory refused");}}
}
std::filesystem::path defaultSaveDirectory(){PWSTR root=nullptr;const HRESULT hr=SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_DEFAULT,nullptr,&root);if(FAILED(hr)){throw std::runtime_error("LocalAppData unavailable");}std::filesystem::path path(root);CoTaskMemFree(root);return path/L"Nobihaza2Remake"/L"saves";}
namespace save_platform{
std::string read(const std::filesystem::path& path){
    Handle f{CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr)};if(f.value==INVALID_HANDLE_VALUE){fail("open save");}regular(f.value);LARGE_INTEGER size{};if(!GetFileSizeEx(f.value,&size)){fail("size save");}if(size.QuadPart<0||size.QuadPart>8*1024*1024){throw std::runtime_error("Save exceeds 8 MiB");}std::string bytes(static_cast<std::size_t>(size.QuadPart),'\0');DWORD read=0;if(!ReadFile(f.value,bytes.data(),static_cast<DWORD>(bytes.size()),&read,nullptr)||read!=bytes.size()){fail("read save");}HANDLE h=f.value;f.value=INVALID_HANDLE_VALUE;if(!CloseHandle(h)){fail("close save");}return bytes;
}
void writeExclusive(const std::filesystem::path& path,const std::string& bytes,const Fault& fault){
    Handle f{CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr)};if(f.value==INVALID_HANDLE_VALUE){fail("create candidate");}
    if(fault){fault("write");}DWORD written=0;if(!WriteFile(f.value,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)||written!=bytes.size()){fail("write candidate");}if(fault){fault("flush");}if(!FlushFileBuffers(f.value)){fail("flush candidate");}if(fault){fault("close");}HANDLE h=f.value;f.value=INVALID_HANDLE_VALUE;if(!CloseHandle(h)){fail("close candidate");}
}
void replace(const std::filesystem::path& temporary,const std::filesystem::path& destination){
    const DWORD attrs=GetFileAttributesW(destination.c_str());
    if(attrs==INVALID_FILE_ATTRIBUTES){if(GetLastError()!=ERROR_FILE_NOT_FOUND){fail("inspect destination");}if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH)){fail("publish new save");}}
    else if(!ReplaceFileW(destination.c_str(),temporary.c_str(),nullptr,0,nullptr,nullptr)){
        // ERROR_UNABLE_TO_REMOVE_REPLACED / _MOVE_REPLACEMENT(_2) may leave different names.
        // Do not retry: the transaction owner retains marker/backup and returns RecoveryRequired.
        fail("replace save requires inspection");
    }
}
void archiveCopy(const std::filesystem::path& from,const std::filesystem::path& to){
    if(!std::filesystem::is_regular_file(std::filesystem::symlink_status(from))){throw std::runtime_error("Archive source is not regular");}
    if(!CopyFileW(from.c_str(),to.c_str(),TRUE)){fail("copy recovery archive");}Handle f{CreateFileW(to.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr)};if(f.value==INVALID_HANDLE_VALUE||!FlushFileBuffers(f.value)){fail("flush recovery archive");}
}
void syncDirectory(const std::filesystem::path&){/* File handles are flushed; namespace recovery is checked by the transaction owner. */}
}
