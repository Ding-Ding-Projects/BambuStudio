#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace Slic3r { namespace GUI { namespace GameServers {
struct InstalledJavaPackage {
    std::string subdir,name,state,digest;
};
std::vector<InstalledJavaPackage> list_installed_java_content(const std::filesystem::path &root);
std::string change_installed_java_content(const std::filesystem::path &root,const std::string &flavour,
    const InstalledJavaPackage &selected,const std::string &action,const std::filesystem::path &replacement={});
}}}
