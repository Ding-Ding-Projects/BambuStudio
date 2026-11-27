#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace Slic3r { namespace GUI { namespace GameServers {
struct BedrockDistribution
{
    std::string version, file, url;
    bool        windows = false, preview = false;
};
inline std::vector<BedrockDistribution> bedrock_distributions(const std::string &body)
{
    auto                             doc = nlohmann::json::parse(body, nullptr, false);
    std::vector<BedrockDistribution> out;
    if (!doc.is_object() || !doc.contains("result") || !doc["result"].is_object() || !doc["result"].contains("links") || !doc["result"]["links"].is_array()) return out;
    for (const auto &entry : doc["result"]["links"]) {
        if (!entry.is_object() || !entry.contains("downloadType") || !entry["downloadType"].is_string() || !entry.contains("downloadUrl") || !entry["downloadUrl"].is_string())
            continue;
        const auto type = entry["downloadType"].get<std::string>();
        if (type != "serverBedrockWindows" && type != "serverBedrockLinux" && type != "serverBedrockPreviewWindows" && type != "serverBedrockPreviewLinux") continue;
        BedrockDistribution item;
        item.windows      = type.find("Windows") != std::string::npos;
        item.preview      = type.find("Preview") != std::string::npos;
        item.url          = entry["downloadUrl"].get<std::string>();
        const auto prefix = std::string("https://www.minecraft.net/bedrockdedicatedserver/bin-") + (item.windows ? "win" : "linux") + (item.preview ? "-preview/" : "/") +
                            "bedrock-server-";
        if (item.url.rfind(prefix, 0) != 0 || item.url.size() <= prefix.size() + 4 || item.url.substr(item.url.size() - 4) != ".zip") continue;
        item.version = item.url.substr(prefix.size(), item.url.size() - prefix.size() - 4);
        if (item.version.size() > 40 || item.version.find_first_not_of("0123456789.") != std::string::npos || item.version.front() == '.' || item.version.back() == '.' ||
            item.version.find("..") != std::string::npos)
            continue;
        item.file = "bedrock-server-" + item.version + ".zip";
        out.push_back(std::move(item));
    }
    return out;
}
}}} // namespace Slic3r::GUI::GameServers
