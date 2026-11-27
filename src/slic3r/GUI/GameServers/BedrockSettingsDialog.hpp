#pragma once
#include <string>
#include "WorldRecoveryDialog.hpp"
class wxWindow;
namespace Slic3r { namespace GUI { namespace GameServers {
void show_target_settings(wxWindow *parent,WorldRecoveryTarget target);
void show_bedrock_settings(wxWindow *parent,const std::string &folder);
}}}
