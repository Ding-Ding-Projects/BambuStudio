#pragma once
#include <string>
#include <filesystem>

namespace Slic3r { namespace GUI { namespace GameServers {

struct ContentInstallResult {
    bool ok = false;
    std::string message;
};

std::string validate_java_replacement(const std::filesystem::path &source,const std::string &flavour,const std::string &group);
ContentInstallResult install_java_content(const std::string &server_folder, const std::string &flavour,
                                          const std::string &archive);
ContentInstallResult install_bedrock_content(const std::string &folder, const std::string &archive);
ContentInstallResult activate_bedrock_content(const std::string &folder, const std::string &manifest);
}}}
