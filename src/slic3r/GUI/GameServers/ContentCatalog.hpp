#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace Slic3r { namespace GUI { namespace GameServers {

struct CatalogItem {
    std::string name, id, filename, url, sha1;
    std::uint64_t size = 0;
};

inline bool catalog_id(const std::string &s)
{
    return !s.empty() && s.size() <= 64 && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isalnum(c) != 0; });
}

inline std::vector<CatalogItem> catalog_projects(const std::string &data)
{
    const auto doc = nlohmann::json::parse(data, nullptr, false);
    std::vector<CatalogItem> rows;
    if (!doc.is_object() || !doc.contains("hits") || !doc["hits"].is_array()) return rows;
    for (const auto &hit : doc["hits"]) {
        if (!hit.is_object() || !hit.contains("project_id") || !hit["project_id"].is_string() || !hit.contains("title") || !hit["title"].is_string()) continue;
        CatalogItem i;
        i.id = hit["project_id"].get<std::string>(); i.name = hit["title"].get<std::string>();
        if (catalog_id(i.id) && !i.name.empty() && i.name.size() <= 256) rows.push_back(i);
    }
    return rows;
}

inline std::vector<CatalogItem> catalog_files(const std::string &data, const std::string &game_version, const std::vector<std::string> &loaders)
{
    const auto versions = nlohmann::json::parse(data, nullptr, false);
    std::vector<CatalogItem> out;
    if (!versions.is_array()) return out;
    for (const auto &v : versions) {
        if (!v.is_object() || !v.contains("game_versions") || !v["game_versions"].is_array() ||
            std::find(v["game_versions"].begin(), v["game_versions"].end(), game_version) == v["game_versions"].end() ||
            !v.contains("loaders") || !v["loaders"].is_array() || !v.contains("files") || !v["files"].is_array()) continue;
        if (std::none_of(loaders.begin(), loaders.end(), [&](const auto &loader) {
            return std::find(v["loaders"].begin(), v["loaders"].end(), loader) != v["loaders"].end();
        })) continue;
        for (const auto &f : v["files"]) {
            if (!f.is_object() || !f.contains("url") || !f["url"].is_string() || !f.contains("filename") || !f["filename"].is_string() ||
                !f.contains("hashes") || !f["hashes"].is_object() || !f["hashes"].contains("sha1") || !f["hashes"]["sha1"].is_string() ||
                !f.contains("size") || !f["size"].is_number_unsigned()) continue;
            CatalogItem i;
            i.filename = f["filename"].get<std::string>(); i.url = f["url"].get<std::string>();
            i.sha1 = f["hashes"]["sha1"].get<std::string>(); i.size = f["size"].get<std::uint64_t>();
            if (i.filename.empty() || i.filename.size() > 160 || i.filename.front() == '.' ||
                !std::all_of(i.filename.begin(), i.filename.end(), [](unsigned char c){ return std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '+'; }) ||
                (i.filename.size() < 5 || (i.filename.substr(i.filename.size()-4) != ".jar" && i.filename.substr(i.filename.size()-4) != ".zip")) ||
                i.url.rfind("https://cdn.modrinth.com/data/", 0) != 0 || i.url.find_first_of("\r\n\\?#") != std::string::npos ||
                i.sha1.size() != 40 || !std::all_of(i.sha1.begin(), i.sha1.end(), [](unsigned char c) {return std::isxdigit(c) != 0;}) ||
                !i.size || i.size > 128ULL * 1024 * 1024) continue;
            i.name = i.filename;
            out.push_back(std::move(i));
        }
    }
    return out;
}

inline std::vector<std::string> catalog_loaders(const std::string &flavour)
{
    std::vector<std::string> values{"datapack"};
    if (flavour == "Paper" || flavour == "Purpur") values.insert(values.end(), {"paper", "spigot", "bukkit"});
    if (flavour == "Spigot") values.insert(values.end(), {"spigot", "bukkit"});
    if (flavour == "Fabric") values.emplace_back("fabric");
    if (flavour == "Forge") values.emplace_back("forge");
    if (flavour == "NeoForge") values.emplace_back("neoforge");
    if (flavour == "Quilt") values.emplace_back("quilt");
    return values;
}
}}}
