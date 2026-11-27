#include "BedrockSettings.hpp"
#include <cerrno>
#include <limits>
namespace Slic3r { namespace GUI { namespace GameServers {
BedrockField bedrock_field(const std::string &key)
{
    using T                                                 = BedrockValue;
    static const std::map<std::string, BedrockField> fields = {
        {"server-name", {T::Text}},
        {"level-name", {T::Text}},
        {"level-seed", {T::Text}},
        {"gamemode", {T::Choice, {"survival", "creative", "adventure"}}},
        {"difficulty", {T::Choice, {"peaceful", "easy", "normal", "hard"}}},
        {"default-player-permission-level", {T::Choice, {"visitor", "member", "operator"}}},
        {"compression-algorithm", {T::Choice, {"zlib", "snappy"}}},
        {"chat-restriction", {T::Choice, {"None", "Dropped", "Disabled"}}},
        {"script-debugger-auto-attach", {T::Choice, {"disabled", "connect", "listen"}}},
        {"server-port", {T::Integer, {}, 1, 65535}},
        {"server-portv6", {T::Integer, {}, 1, 65535}},
        {"max-players", {T::Integer, {}, 1}},
        {"view-distance", {T::Integer, {}, 5}},
        {"tick-distance", {T::Integer, {}, 4, 12}},
        {"max-threads", {T::Integer, {}, 1}},
        {"compression-threshold", {T::Integer, {}, 0, 65535}},
        {"player-idle-timeout", {T::Integer}},
        {"server-build-radius-ratio", {T::Text}},
        {"player-position-acceptance-threshold", {T::Decimal}},
        {"player-movement-action-direction-threshold", {T::Decimal}},
        {"player-movement-distance-threshold", {T::Decimal}},
        {"script-debugger-passcode", {T::Protected}},
        {"script-debugger-auto-attach-connect-address", {T::Text}},
        {"server-authoritative-block-breaking-range-scalar", {T::Decimal}},
        {"force-inbound-debug-port", {T::Integer, {}, 1, 65535}},
        {"script-debugger-auto-attach-timeout", {T::Integer}},
        {"script-watchdog-hang-threshold", {T::Integer}},
        {"script-watchdog-spike-threshold", {T::Integer}},
        {"script-watchdog-memory-warning", {T::Integer}},
        {"script-watchdog-memory-limit", {T::Integer}},
        {"script-watchdog-slow-threshold", {T::Integer}},
        {"diagnostics-capture-max-files", {T::Integer}},
        {"diagnostics-capture-max-file-size", {T::Integer}},
        {"server-ip", {T::Text}},
    };
    static const std::set<std::string> booleans = {"force-gamemode",
                                                   "allow-cheats",
                                                   "online-mode",
                                                   "allow-list",
                                                   "enable-lan-visibility",
                                                   "texturepack-required",
                                                   "content-log-file-enabled",
                                                   "server-authoritative-movement-strict",
                                                   "server-authoritative-dismount-strict",
                                                   "server-authoritative-entity-interactions-strict",
                                                   "correct-player-movement",
                                                   "server-authoritative-block-breaking",
                                                   "disable-persona",
                                                   "disable-custom-skins",
                                                   "disable-player-interaction",
                                                   "client-side-chunk-generation-enabled",
                                                   "block-network-ids-are-hashes",
                                                   "disable-client-vibrant-visuals",
                                                   "allow-inbound-script-debugging",
                                                   "allow-outbound-script-debugging",
                                                   "script-watchdog-enable",
                                                   "script-watchdog-enable-exception-handling",
                                                   "script-watchdog-enable-shutdown",
                                                   "script-watchdog-hang-exception",
                                                   "diagnostics-capture-auto-start",
                                                   "enable-script-content-log",
                                                   "content-log-console-output-enabled"};
    auto                               found    = fields.find(key);
    if (found != fields.end()) return found->second;
    if (booleans.count(key)) return {T::Bool};
    return {};
}
bool bedrock_value_allowed(const BedrockField &f, const std::string &s)
{
    if (s.size() > 1024 || std::any_of(s.begin(), s.end(), [](unsigned char c) { return c < 32 || c == 127 || c == ';'; })) return false;
    switch (f.kind) {
    case BedrockValue::Text: return true;
    case BedrockValue::Bool: return s == "true" || s == "false";
    case BedrockValue::Choice: return std::find(f.choices.begin(), f.choices.end(), s) != f.choices.end();
    case BedrockValue::Integer: {
        int  n      = 0;
        auto parsed = std::from_chars(s.data(), s.data() + s.size(), n);
        return parsed.ec == std::errc{} && parsed.ptr == s.data() + s.size() && n >= f.low && n <= f.high;
    }
    case BedrockValue::Decimal: {
        if (s.empty() || s.find_first_not_of("-.0123456789") != std::string::npos) return false;
        char *end   = nullptr;
        errno       = 0;
        auto number = std::strtod(s.c_str(), &end);
        return errno != ERANGE && end != s.c_str() && *end == '\0' && std::isfinite(number) && number >= 0;
    }
    default: return false;
    }
}
std::vector<BedrockSetting> read_bedrock_settings(const std::string &bytes)
{
    std::vector<BedrockSetting>        rows;
    std::map<std::string, std::size_t> keys;
    std::size_t                        start = 0;
    while (start < bytes.size()) {
        auto nl = bytes.find('\n', start);
        if (nl == std::string::npos) nl = bytes.size();
        auto end = nl;
        if (end > start && bytes[end - 1] == '\r') --end;
        auto eq = bytes.find('=', start);
        if (eq != std::string::npos && eq < end) {
            auto key = bytes.substr(start, eq - start);
            if (!key.empty() && key.size() <= 96 && key.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-._") == std::string::npos) {
                if (keys.count(key))
                    rows[keys[key]].duplicate = true;
                else {
                    keys[key]  = rows.size();
                    auto kind  = bedrock_field(key).kind;
                    auto value = (kind == BedrockValue::Unknown || kind == BedrockValue::Protected) ? std::string() : bytes.substr(eq + 1, end - eq - 1);
                    rows.push_back({key, value, eq + 1, end, false});
                }
            }
        }
        start = nl + 1;
    }
    return rows;
}
bool change_bedrock_setting(std::string &bytes, const std::string &key, const std::string &value)
{
    if (!bedrock_value_allowed(bedrock_field(key), value)) return false;
    for (const auto &row : read_bedrock_settings(bytes))
        if (row.key == key && !row.duplicate) {
            bytes.replace(row.start, row.end - row.start, value);
            return true;
        }
    return false;
}
}}} // namespace Slic3r::GUI::GameServers
