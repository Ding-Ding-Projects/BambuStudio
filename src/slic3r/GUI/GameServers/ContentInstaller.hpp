#pragma once
#include <string>

namespace Slic3r { namespace GUI { namespace GameServers {

struct ContentInstallResult {
    bool ok = false;
    std::string message;
};

ContentInstallResult install_java_content(const std::string &server_folder, const std::string &flavour,
                                          const std::string &archive);
}}}
