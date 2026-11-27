#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include "RemoteServerSetup.hpp"

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

inline std::string shell_literal(const std::string &value)
{
    std::string quoted="'";
    for(char c:value) { if(c=='\'') quoted+="'\\''";else quoted+=c; }
    return quoted+"'";
}
inline bool ownership_id(const std::string &s)
{
    return s.size()==32 && s.find_first_not_of("0123456789abcdef")==std::string::npos;
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

inline std::vector<std::string> ssh_invocation(const std::string &ssh,const std::string &profile,const std::string &action)
{
    if(ssh.empty() || !ssh_profile_name(profile) || action.empty()) return {};
    return {ssh,"-T","-o","BatchMode=yes","-o","StrictHostKeyChecking=yes","-o","ClearAllForwardings=yes",
            "-o","ConnectTimeout=8","--",profile,action};
}

inline std::vector<std::string> ssh_server_setup(const std::string &ssh,const std::string &profile,const std::string &folder,
                                                 const std::string &package,bool bedrock,int port,const std::string &owner)
{
    const auto dest=shell_path(folder),src=shell_path(package);
    if(dest.empty() || src.empty() || port<1 || port>65535 || !ownership_id(owner)) return {};
    return ssh_invocation(ssh,profile,"umask 077; exec python3 -c "+shell_literal(remote_server_setup)+" "+src+" "+dest+
                          (bedrock?" bedrock ":" java ")+std::to_string(port)+" "+owner);
}

inline std::vector<std::string> ssh_server_launch(const std::string &ssh,const std::string &profile,const std::string &folder,bool bedrock,
                                                   const std::string &owner={})
{
    const std::string path=shell_path(folder);
    if(path.empty() || (!owner.empty() && !ownership_id(owner))) return {};
    std::string launch=bedrock?"test -x ./bedrock_server && LD_LIBRARY_PATH=. exec ./bedrock_server":"test -f ./server.jar && exec java -jar ./server.jar nogui";
    std::string check="test ! -e .bambustudio-native";
    if(!owner.empty()) {
        check="test -f .bambustudio-native && test \"$(cat .bambustudio-native)\" = "+owner;
        if(!bedrock) launch="if grep -qx 'eula=false' eula.txt; then printf 'eula=true\\n' > eula.txt || exit 1; fi; "+launch;
        launch="exec flock -n .bambustudio-run.lock sh -c "+shell_literal(launch);
    }
    return ssh_invocation(ssh,profile,"cd "+path+" && test -f ./server.properties && "+check+" && "+launch);
}
}}}
