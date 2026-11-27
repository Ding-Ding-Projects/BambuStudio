#pragma once
#include <string>
namespace Slic3r { namespace GUI { namespace GameServers {
bool save_rcon_credential(const std::string &server, const std::string &value);
bool clear_rcon_credential(const std::string &server);
bool verify_local_rcon(const std::string &server, int port, unsigned long expected_pid, std::string &reason);
}}}
