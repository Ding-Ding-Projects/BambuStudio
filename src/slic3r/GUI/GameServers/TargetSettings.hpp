#pragma once
#include "WorldRecoveryDialog.hpp"
#include <nlohmann/json.hpp>
namespace Slic3r { namespace GUI { namespace GameServers {
struct TargetSettingsResult {std::string error; nlohmann::json result;};
TargetSettingsResult target_settings(const WorldRecoveryTarget &target,const std::string &exe,nlohmann::json request);
}}}
