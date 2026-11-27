#pragma once
#include <string>
class wxWindow;
namespace Slic3r { namespace GUI { namespace GameServers {
struct WorldRecoveryTarget {
    bool container=false,bedrock=false;
    std::string profile, server_folder, owner;
};
void show_world_recovery(wxWindow *parent,WorldRecoveryTarget target);
}}}
