#include "LocalConfigHistory.hpp"
#include "PreferencesHistory.hpp"
#include "libslic3r/ProjectHistoryManager.hpp"
#include "libslic3r/PrintConfig.hpp"
#include "libslic3r/Utils.hpp"
#include <boost/uuid/name_generator_sha1.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <fstream>

namespace Slic3r { namespace GUI { namespace LocalConfigHistory {
namespace {
std::filesystem::path root() { return std::filesystem::path(data_dir()) / "local_config_history"; }
bool permitted(const std::string &key, const std::string &value)
{
    if (key.size() > 128 || value.size() > 16384 || print_config_def.get(key) == nullptr) return false;
    std::string lower = key; std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
    for (const char *word : {"password", "token", "secret", "credential", "session", "access_code", "gcode", "host", "url", "ip_address", "uuid", "serial", "account", "user_id"})
        if (lower.find(word) != std::string::npos) return false;
    return value.find("-----BEGIN") == std::string::npos && value.find("Bearer ") == std::string::npos;
}
}
ProjectHistoryManager *manager() { return PreferencesHistory::manager(); }
bool record(const std::string &category, const std::string &identity, const std::string &label,
            const std::map<std::string, std::string> &values)
{
    if ((category != "preset" && category != "draft") || identity.empty() || identity.size() > 1024 || label.size() > 1024 || !manager()) return false;
    try {
        const boost::uuids::uuid ns = {{0x81,0x9e,0x25,0x53,0x23,0x43,0x42,0x11,0x86,0x2a,0x4b,0x42,0x61,0x2f,0x12,0x98}};
        const std::string id = boost::uuids::to_string(boost::uuids::name_generator_sha1(ns)(category + ":" + identity));
        std::filesystem::create_directories(root());
        const auto key = root() / (id + ".3mf");
        nlohmann::json payload = {{"schema", "bambustudio.local-config-history"}, {"version", 1}, {"category", category}, {"values", nlohmann::json::object()}};
        for (const auto &value : values) if (permitted(value.first, value.second)) payload["values"][value.first] = value.second;
        static std::atomic<unsigned long long> serial{0};
        const auto staging = root() / (id + "-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + "-" + std::to_string(++serial) + ".snapshot.3mf");
        { std::ofstream out(staging, std::ios::binary); out << payload.dump(); if (!out) return false; }
        // The catalog contains only stable local identities and display names.
        const auto metadata = root() / (id + ".json");
        if (!std::filesystem::exists(metadata)) { std::ofstream out(metadata); out << nlohmann::json{{"category", category}, {"name", identity.substr(0, 256)}, {"identity", id + ".3mf"}}.dump(); if (!out) return false; }
        ProjectHistoryCommitOptions options; options.message = label;
        auto result = manager()->commit_snapshot(key, staging, options).get();
        if (result.ok()) { std::error_code ec; std::filesystem::remove(staging, ec); }
        return result.ok();
    } catch (...) { return false; }
}
std::vector<Source> sources()
{
    std::vector<Source> result; std::error_code ec;
    for (std::filesystem::directory_iterator it(root(), ec), end; !ec && it != end; it.increment(ec)) {
        if (result.size() >= 1000) break;
        if (it->path().extension() != ".json" || it->file_size(ec) > 4096 || ec) continue;
        try { std::ifstream in(it->path()); auto j = nlohmann::json::parse(in);
            const auto category = j.at("category").get<std::string>();
            const auto filename = j.at("identity").get<std::string>();
            if ((category != "preset" && category != "draft") || std::filesystem::path(filename).filename().string() != filename || std::filesystem::path(filename).extension() != ".3mf") continue;
            result.push_back({category, j.at("name").get<std::string>(), root() / filename});
        } catch (...) {}
    } return result;
}
std::map<std::string, std::string> read_snapshot(const std::filesystem::path &path)
{
    std::map<std::string, std::string> result;
    try { if (std::filesystem::file_size(path) > 4 * 1024 * 1024) return result;
        std::ifstream in(path); auto j = nlohmann::json::parse(in);
        if (j.value("schema", "") != "bambustudio.local-config-history" || j.value("version", 0) != 1 || !j.at("values").is_object()) return result;
        for (auto it = j["values"].begin(); it != j["values"].end(); ++it) if (it.value().is_string()) { auto value = it.value().get<std::string>(); if (permitted(it.key(), value)) result[it.key()] = value; }
    } catch (...) {} return result;
}
}}}
