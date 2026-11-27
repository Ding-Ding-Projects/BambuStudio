#pragma once
#include "WorldRecoveryDialog.hpp"
#include <nlohmann/json.hpp>
#include <vector>
#include <utility>
namespace Slic3r { namespace GUI { namespace GameServers {
std::pair<int,std::string> run_target_cli(const std::vector<std::string> &args,const std::string &stdin_line,int timeout_seconds);
struct TargetSettingsResult {std::string error; nlohmann::json result;};
TargetSettingsResult target_content_action(const WorldRecoveryTarget &target,const std::string &exe,const nlohmann::json &request);
TargetSettingsResult target_settings(const WorldRecoveryTarget &target,const std::string &exe,nlohmann::json request);
}}}
