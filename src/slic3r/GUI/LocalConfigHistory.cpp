#include "LocalConfigHistory.hpp"
#include "PreferencesHistory.hpp"
#include "SettingsDraftPanel.hpp"
#include "GUI_App.hpp"
#include "Tab.hpp"
#include "Plater.hpp"
#include "libslic3r/PresetBundle.hpp"
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
bool record_config(const std::string &category, const std::string &identity,
                   const std::string &label, const DynamicPrintConfig &config)
{
    std::map<std::string, std::string> values;
    for (const auto &key : config.keys()) {
        const auto value = config.opt_serialize(key);
        if (permitted(key, value)) values.emplace(key, value);
    }
    return record(category, identity, label, values);
}
DynamicPrintConfig merge_restored_config(const DynamicPrintConfig &current, const DynamicPrintConfig &snapshot)
{
    DynamicPrintConfig merged(current);
    for (const auto &key : current.keys())
        if (permitted(key, current.opt_serialize(key)) && !snapshot.option(key)) merged.erase(key);
    merged.apply(snapshot);
    return merged;
}
bool restore_config(const std::string &category, const std::string &identity,
                    const std::filesystem::path &snapshot, std::string &error)
{
    error.clear();
    try {
        const auto values = read_snapshot(snapshot);
        if (values.empty()) { error = "The settings snapshot is empty or invalid."; return false; }
        DynamicPrintConfig restored;
        for (const auto &value : values) {
            restored.set_deserialize_strict(value.first, value.second);
            if (!restored.option(value.first)) { error = "Unsupported settings snapshot."; return false; }
        }
        if (category == "draft") {
            if (SettingsDraftPanel::RestoreHistory(identity, restored)) return true;
            error = "Open the matching draft and project before restoring this version.";
            return false;
        }
        const auto separator = identity.find(':');
        if (category != "preset" || separator == std::string::npos) { error = "Unsupported history target."; return false; }
        const auto type = static_cast<Preset::Type>(std::stoi(identity.substr(0, separator)));
        if (type != Preset::TYPE_PRINT && type != Preset::TYPE_FILAMENT && type != Preset::TYPE_PRINTER) { error = "Unsupported preset type."; return false; }
        auto *tab = wxGetApp().get_tab(type);
        if (!tab || !wxGetApp().plater() || !wxGetApp().preset_bundle ||
            tab->get_presets()->get_selected_preset().name != identity.substr(separator + 1)) {
            error = "Select the matching preset before restoring this version."; return false;
        }
        const DynamicPrintConfig previous = *tab->get_config();
        DynamicPrintConfig merged = merge_restored_config(previous, restored);
        wxGetApp().plater()->take_snapshot("Restore preset history");
        try {
            *tab->get_config() = merged;
            tab->update_dirty(); tab->reload_config(); tab->update();
            wxGetApp().plater()->on_config_change(wxGetApp().preset_bundle->full_config());
        } catch (...) {
            *tab->get_config() = previous; tab->update_dirty(); tab->reload_config(); tab->update();
            wxGetApp().plater()->on_config_change(wxGetApp().preset_bundle->full_config());
            throw;
        }
        record_config(category, identity, "Restore preset history", *tab->get_config());
        return true;
    } catch (...) { error = "Unable to restore settings. The previous live settings were retained."; return false; }
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
        if (!j.is_object() || j.size() != 4 || j.value("schema", "") != "bambustudio.local-config-history" || j.value("version", 0) != 1 ||
            (j.value("category", "") != "preset" && j.value("category", "") != "draft") || !j.at("values").is_object() || j.at("values").size() > 4096) return {};
        for (auto it = j["values"].begin(); it != j["values"].end(); ++it) {
            if (!it.value().is_string()) return {};
            auto value = it.value().get<std::string>();
            if (!permitted(it.key(), value)) return {};
            DynamicPrintConfig validation; validation.set_deserialize_strict(it.key(), value);
            if (!validation.option(it.key())) return {};
            result[it.key()] = value;
        }
    } catch (...) { return {}; } return result;
}
}}}
