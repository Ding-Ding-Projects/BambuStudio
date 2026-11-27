#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
namespace Slic3r { namespace GUI { namespace GameServers {
struct VersionReport {
    std::string source,sha1,error;
    std::filesystem::file_time_type modified{};
    std::uintmax_t bytes=0;
    nlohmann::json commands;
    std::vector<std::string> blocks;
    bool matches(const std::string &path) const;
};
VersionReport build_version_report(const std::string &jar,const std::string &java,const std::string &cache);
}}}
