#pragma once
#include "ServerSystem.hpp"
namespace Slic3r { namespace GUI { namespace GameServers {
ZipOutcome backup_local_bedrock(const std::string &folder,const std::string &target);
bool local_bedrock_archive(const std::string &path,const std::string &folder);
}}}
