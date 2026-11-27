#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace GameServers {

inline bool container_field(const std::string &value)
{
    const auto alnum = [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); };
    return !value.empty() && value.size() <= 100 && alnum(value.front()) &&
           std::all_of(value.begin(), value.end(), [&](char c) { return alnum(c) || c == '_' || c == '-' || c == '.'; });
}

inline bool container_id(const std::string &value)
{
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

inline bool container_bind_address(const std::string &address)
{
    int         octets[4]{};
    std::size_t pos = 0;
    for (int i = 0; i < 4; ++i) {
        const std::size_t start = pos;
        while (pos < address.size() && address[pos] >= '0' && address[pos] <= '9') {
            if (pos - start == 3) return false;
            octets[i] = octets[i] * 10 + address[pos++] - '0';
        }
        if (pos == start || octets[i] > 255 || (pos - start > 1 && address[start] == '0')) return false;
        if (i < 3 && (pos == address.size() || address[pos++] != '.')) return false;
    }
    return pos == address.size() &&
           (address == "127.0.0.1" || octets[0] == 10 || (octets[0] == 172 && octets[1] >= 16 && octets[1] <= 31) || (octets[0] == 192 && octets[1] == 168));
}

struct ContainerSetup
{
    std::string name, version, flavour, image_tag;
    int         port    = 0;
    bool        bedrock = false, accepted = false;
    std::string bind        = "127.0.0.1";
    bool        lan_allowed = false;
    bool        nethernet   = true;
    int         udp_first   = 19140;
};

inline std::vector<std::string> container_create(const ContainerSetup &s)
{
    if (!s.accepted || !container_bind_address(s.bind) || (s.bind != "127.0.0.1" && !s.lan_allowed) || !container_field(s.name) || !container_field(s.version) ||
        !container_field(s.image_tag) || s.port < 1 || s.port > 65535)
        return {};
    const std::vector<std::string> flavours{"VANILLA", "PAPER", "PURPUR", "SPIGOT", "FABRIC", "FORGE", "NEOFORGE", "QUILT"};
    if (s.bedrock && s.nethernet && (s.udp_first < 1 || s.udp_first > 65520)) return {};
    if (!s.bedrock && std::find(flavours.begin(), flavours.end(), s.flavour) == flavours.end()) return {};
    // Anonymous volumes are fresh for each successful create and survive stop/start.
    std::vector<std::string> args{"create",    "--interactive",
                                  "--name",    s.name,
                                  "--label",   "org.bambustudio.minecraft=1",
                                  "--label",   std::string("org.bambustudio.edition=") + (s.bedrock ? "bedrock" : "java"),
                                  "--mount",   "type=volume,destination=/data",
                                  "--publish", s.bind + ":" + std::to_string(s.port) + (s.bedrock ? (s.nethernet ? ":19132/tcp" : ":19132/udp") : ":25565/tcp"),
                                  "--env",     "EULA=TRUE",
                                  "--env",     "VERSION=" + s.version};
    if (s.bedrock && s.nethernet) {
        const auto range = std::to_string(s.udp_first) + "-" + std::to_string(s.udp_first + 15);
        args.insert(args.end(), {"--publish", s.bind + ":" + range + ":19140-19155/udp", "--env", "SERVER_UDP_PORTS=" + s.bind + ":" + range + ":19140-19155"});
    }
    if (!s.bedrock) args.insert(args.end(), {"--env", "TYPE=" + s.flavour});
    args.emplace_back(std::string(s.bedrock ? "itzg/minecraft-bedrock-server:" : "itzg/minecraft-server:") + s.image_tag);
    return args;
}

}}} // namespace Slic3r::GUI::GameServers
