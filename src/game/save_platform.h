#pragma once
#include <filesystem>
#include <functional>
#include <string>
namespace save_platform{
    using Fault=std::function<void(std::string_view)>;
    std::string read(const std::filesystem::path&);
    void writeExclusive(const std::filesystem::path&,const std::string&,const Fault& fault={});
    void replace(const std::filesystem::path& temporary,const std::filesystem::path& destination);
    void archiveCopy(const std::filesystem::path& from,const std::filesystem::path& to);
    void syncDirectory(const std::filesystem::path&);
}
