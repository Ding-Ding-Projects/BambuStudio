#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace GameServers {

inline bool ssh_profile_name(const std::string &s)
{
    const auto letter_digit = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); };
    auto part = [&](const std::string &word) {
        return !word.empty() && letter_digit(word[0]) && std::all_of(word.begin(), word.end(),
            [&](char c) { return letter_digit(c) || c == '-' || c == '_' || c == '.'; });
    };
    if (s.size() > 128) return false;
    const auto at = s.find('@');
    return at == std::string::npos ? part(s) : part(s.substr(0, at)) && part(s.substr(at + 1));
}

inline std::string shell_path(const std::string &path)
{
    if (path.size() < 2 || path.size() > 2048 || path.front() != '/') return {};
    std::string quoted = "'";
    for (unsigned char c : path) {
        if (c < 32 || c == 127) return {};
        if (c == '\'') quoted += "'\\''";
        else quoted += char(c);
    }
    return quoted + "'";
}

inline std::vector<std::string> ssh_server_launch(const std::string &ssh, const std::string &profile, const std::string &folder, bool bedrock)
{
    const std::string path = shell_path(folder);
    if (ssh.empty() || !ssh_profile_name(profile) || path.empty()) return {};
    // The selected host must already have a trusted host key and a working non-interactive identity.
    const std::string exec = bedrock ? "test -x ./bedrock_server && LD_LIBRARY_PATH=. exec ./bedrock_server" :
                                      "test -f ./server.jar && exec java -jar ./server.jar nogui";
    return {ssh, "-T", "-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=yes", "-o", "ClearAllForwardings=yes",
            "-o", "ConnectTimeout=8", "--", profile, "cd " + path + " && test -f ./server.properties && " + exec};
}
}}}
