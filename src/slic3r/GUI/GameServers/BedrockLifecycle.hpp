#pragma once
#include "ContentLifecycle.hpp"
namespace Slic3r { namespace GUI { namespace GameServers {
std::vector<InstalledJavaPackage> list_local_bedrock_content(const std::filesystem::path &root);
std::string change_local_bedrock_content(const std::filesystem::path &root,const InstalledJavaPackage &item,const std::string &action,
                                         const std::filesystem::path &replacement={});
}}}
