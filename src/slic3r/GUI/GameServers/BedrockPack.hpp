#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <nlohmann/json.hpp>

namespace Slic3r { namespace GUI { namespace GameServers {
struct BedrockPack {
    std::string id, group;
    nlohmann::json version;
};

inline BedrockPack bedrock_pack(const std::string &text)
{
    const auto doc = nlohmann::json::parse(text, nullptr, false);
    if (!doc.is_object() || !doc.contains("header") || !doc["header"].is_object() ||
        !doc.contains("modules") || !doc["modules"].is_array()) return {};
    const auto &h = doc["header"];
    if (!h.contains("uuid") || !h["uuid"].is_string() || !h.contains("version") ||
        !h["version"].is_array() || h["version"].size() != 3) return {};
    BedrockPack p;
    p.id = h["uuid"].get<std::string>();
    if (p.id.size() != 36) return {};
    for (std::size_t i=0; i<p.id.size(); ++i) {
        const bool sep = i==8 || i==13 || i==18 || i==23;
        if (sep ? p.id[i]!='-' : !std::isxdigit(static_cast<unsigned char>(p.id[i]))) return {};
        p.id[i] = char(std::tolower(static_cast<unsigned char>(p.id[i])));
    }
    if (!std::all_of(h["version"].begin(),h["version"].end(),[](const auto &n){return n.is_number_unsigned() && n.template get<std::uint64_t>() <= 65535;})) return {};
    p.version=h["version"];
    for (const auto &mod : doc["modules"]) {
        if (!mod.is_object() || !mod.contains("type") || !mod["type"].is_string()) return {};
        const std::string t=mod["type"].get<std::string>();
        const std::string group=t=="resources" ? "resource" : t=="data" || t=="script" ? "behavior" : "";
        if (group.empty() || (!p.group.empty() && p.group!=group)) return {};
        p.group=group;
    }
    return p.group.empty() ? BedrockPack{} : p;
}
}}}
