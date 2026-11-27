#pragma once
#include "ServerModel.hpp"
#include <filesystem>
namespace Slic3r { namespace GUI { namespace GameServers {
bool copy_runnable_server(const std::filesystem::path &source, const std::filesystem::path &destination, JarInfo &info, std::string &error);
bool publish_server_setup(const std::filesystem::path &stage, const std::filesystem::path &target, const std::string &jar, std::string &error);
}}} // namespace Slic3r::GUI::GameServers
