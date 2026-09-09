#ifndef slic3r_GUI_HomeAssistantSettingsSyncModel_hpp_
#define slic3r_GUI_HomeAssistantSettingsSyncModel_hpp_

// Settings sync to Home Assistant (issue #16): the wx-free half. Decides which
// AppConfig keys may leave the machine and shapes the REST payloads. The
// transport, the timer and the dialog live in HomeAssistantSettingsSync.cpp.
//
// Route: POST /api/states/<entity_id>. Home Assistant creates or updates a
// state object for any entity id it receives from an authenticated caller and
// needs no integration, helper or YAML on its side, which is why this route
// was chosen over input_* helpers (those must exist before they can be set).
// The entities are read-only from Home Assistant's point of view: they carry
// the app's settings out; nothing here reads settings back in (the scheduled
// settings feature is the inbound direction and has its own contract).

#include "nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace HomeAssistant { namespace SettingsSync {

inline constexpr const char *kEnabledKey          = "ha_settings_sync";
inline constexpr const char *kAggregateEntityId   = "sensor.bambustudio_settings";
inline constexpr const char *kEntityPrefix        = "sensor.bambustudio_";
inline constexpr std::size_t kMaxAttributeValueBytes = 255; // HA truncates state strings at 255
inline constexpr std::size_t kMaxAttributes       = 200;
inline constexpr int         kDebounceSeconds     = 5;
inline constexpr int         kIntervalMinutes     = 15;

// The nine headline settings get their own sensor each so an automation can
// trigger on one of them without unpacking attributes.
inline const std::vector<std::string> &headline_keys()
{
    static const std::vector<std::string> keys = {
        "language", "dark_color_mode", "ui_density", "ui_accent_seed", "ui_font_family",
        "ui_font_scale", "funny_level_en", "funny_level_yue", "app_display_name"};
    return keys;
}

// Deny-list. A key is refused when its lowercase form contains any of these
// fragments; that catches every token, password, access code, cookie, URL,
// path, directory, and recent-file key the config has grown and any it grows
// later, without a hand-kept list of exact names. Exact names below are the
// ones that do not contain a fragment but are still connection details.
inline const std::vector<std::string> &denied_fragments()
{
    static const std::vector<std::string> fragments = {
        "token", "password", "passwd", "secret", "access_code", "accesscode", "api_key", "apikey",
        "cookie", "session", "credential", "auth", "private", "cert", "_pin", "pin_", "pincode",
        "path", "_dir", "dir_", "directory", "folder", "_file", "file_", "files", "recent", "url",
        "host", "address", "ip_", "_ip", "serial", "sn_", "dev_id", "device_id", "user_id", "userid",
        "_uid", "uid_", "email", "phone", "license", "webview", "printer_"};
    return fragments;
}

inline const std::vector<std::string> &denied_exact()
{
    static const std::vector<std::string> keys = {
        "ha_speakers", "ha_lights", "region", "user_name", "nickname", "avatar", "install_id", "machine_id"};
    return keys;
}

// Section names whose whole content is refused (printer, cloud and account
// state is not a "setting").
inline const std::vector<std::string> &denied_sections()
{
    static const std::vector<std::string> sections = {
        "printers", "presets", "user", "cloud", "recent", "recent_projects", "print_host", "physical_printer",
        "network", "camera", "certificate", "skip_version", "app_settings_secret"};
    return sections;
}

inline std::string lowercase_ascii(std::string s)
{
    for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

inline bool is_denied_section(const std::string &section)
{
    const std::string lower = lowercase_ascii(section);
    const auto &sections = denied_sections();
    return std::find(sections.begin(), sections.end(), lower) != sections.end();
}

// True when the key must never be published.
inline bool is_denied_key(const std::string &key)
{
    const std::string lower = lowercase_ascii(key);
    for (const std::string &exact : denied_exact())
        if (lower == exact) return true;
    for (const std::string &fragment : denied_fragments())
        if (lower.find(fragment) != std::string::npos) return true;
    return false;
}

// A value that looks like a secret or a path is refused even under an innocent
// key: long high-entropy strings, Windows drive paths, UNC paths, POSIX
// absolute paths, URLs, JWTs.
inline bool looks_sensitive_value(const std::string &value)
{
    if (value.size() >= 2 && std::isalpha(static_cast<unsigned char>(value[0])) && value[1] == ':' && (value.size() == 2 || value[2] == '\\' || value[2] == '/'))
        return true; // C:\ or C:/
    if (value.rfind("\\\\", 0) == 0 || value.rfind("//", 0) == 0) return true;  // UNC
    if (value.rfind("/", 0) == 0 && value.find('/', 1) != std::string::npos) return true; // /usr/...
    if (value.find("://") != std::string::npos) return true;                     // URL
    if (value.rfind("eyJ", 0) == 0) return true;                                 // JWT header
    if (value.size() >= 32) {
        // Long strings with no spaces and a mix of letters and digits read as tokens.
        bool has_alpha = false, has_digit = false, has_space = false;
        for (unsigned char c : value) {
            if (std::isalpha(c)) has_alpha = true;
            else if (std::isdigit(c)) has_digit = true;
            else if (std::isspace(c)) has_space = true;
        }
        if (has_alpha && has_digit && !has_space) return true;
    }
    return false;
}

inline bool is_publishable(const std::string &key, const std::string &value)
{
    return !is_denied_key(key) && !looks_sensitive_value(value);
}

// Attribute names must be plain identifiers; HA accepts most strings but the
// entity ids derived from them must not.
inline std::string attribute_name(const std::string &key)
{
    std::string out;
    for (unsigned char c : key) {
        if (std::isalnum(c)) out.push_back(static_cast<char>(std::tolower(c)));
        else out.push_back('_');
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    if (out.empty()) out = "setting";
    if (std::isdigit(static_cast<unsigned char>(out[0]))) out.insert(out.begin(), '_');
    return out;
}

inline std::string truncate_value(std::string value)
{
    if (value.size() > kMaxAttributeValueBytes) {
        value.resize(kMaxAttributeValueBytes);
        // Do not cut inside a UTF-8 sequence.
        while (!value.empty() && (static_cast<unsigned char>(value.back()) & 0xC0) == 0x80) value.pop_back();
    }
    return value;
}

// Filter the root section of the config (plus any allowed section, prefixed
// "section.key") down to what may be published. Deterministic ordering so the
// payload is stable and diffable.
inline std::map<std::string, std::string> publishable_settings(
    const std::map<std::string, std::string>                        &root,
    const std::map<std::string, std::map<std::string, std::string>> &sections = {})
{
    std::map<std::string, std::string> out;
    for (const auto &[key, value] : root)
        if (is_publishable(key, value)) out[attribute_name(key)] = truncate_value(value);
    for (const auto &[section, entries] : sections) {
        if (is_denied_section(section)) continue;
        for (const auto &[key, value] : entries)
            if (is_publishable(key, value)) out[attribute_name(section + "." + key)] = truncate_value(value);
    }
    if (out.size() > kMaxAttributes) {
        auto it = out.begin();
        std::advance(it, static_cast<long>(kMaxAttributes));
        out.erase(it, out.end());
    }
    return out;
}

struct StateUpdate
{
    std::string entity_id;
    std::string body; // JSON for POST /api/states/<entity_id>
};

inline std::string state_string(const std::string &value)
{
    return value.empty() ? std::string("unknown") : truncate_value(value);
}

// Build the aggregate sensor plus one sensor per headline key. `synced_at` is
// an ISO-8601 local timestamp the dialog also shows; `app_version` is the
// shipped product version (not the display name).
inline std::vector<StateUpdate> build_state_updates(
    const std::map<std::string, std::string> &settings,
    const std::string                        &synced_at,
    const std::string                        &app_version)
{
    std::vector<StateUpdate> updates;
    nlohmann::json aggregate = nlohmann::json::object();
    aggregate["state"]       = synced_at.empty() ? "unknown" : truncate_value(synced_at);
    nlohmann::json attrs     = nlohmann::json::object();
    attrs["friendly_name"]   = "BambuStudio settings";
    attrs["icon"]            = "mdi:tune";
    attrs["app_version"]     = app_version;
    attrs["setting_count"]   = settings.size();
    for (const auto &[key, value] : settings)
        attrs[key] = value;
    aggregate["attributes"]  = attrs;
    updates.push_back({kAggregateEntityId, aggregate.dump()});

    for (const std::string &key : headline_keys()) {
        const std::string name = attribute_name(key);
        auto              it   = settings.find(name);
        nlohmann::json    one  = nlohmann::json::object();
        one["state"]           = it == settings.end() ? "unknown" : state_string(it->second);
        nlohmann::json a       = nlohmann::json::object();
        a["friendly_name"]     = "BambuStudio " + name;
        a["icon"]              = "mdi:tune-variant";
        a["setting_key"]       = key;
        a["synced_at"]         = synced_at;
        one["attributes"]      = a;
        updates.push_back({std::string(kEntityPrefix) + name, one.dump()});
    }
    return updates;
}

// Defensive: the payload must never carry a denied key or a sensitive value,
// whatever the caller passed. Used by the test suite and by the transport
// before every POST.
inline bool payload_is_clean(const std::string &body)
{
    nlohmann::json j = nlohmann::json::parse(body, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return false;
    auto attrs = j.find("attributes");
    if (attrs == j.end() || !attrs->is_object()) return false;
    static const char *own[] = {"friendly_name", "icon", "app_version", "setting_count", "setting_key", "synced_at"};
    for (auto it = attrs->begin(); it != attrs->end(); ++it) {
        bool is_own = false;
        for (const char *o : own) if (it.key() == o) { is_own = true; break; }
        if (is_own) continue;
        if (is_denied_key(it.key())) return false;
        if (it->is_string() && looks_sensitive_value(it->get<std::string>())) return false;
    }
    return true;
}

} } } } // namespace Slic3r::GUI::HomeAssistant::SettingsSync

#endif // slic3r_GUI_HomeAssistantSettingsSyncModel_hpp_
