#ifndef slic3r_GUI_StatusApiCore_hpp_
#define slic3r_GUI_StatusApiCore_hpp_

// Pure rules of the local, read-only printer status API (docs/features/device/status-api.md).
//
// This header depends on the standard library and DeviceLiveReadings.hpp only, so routing,
// authorization, the door state mapping and the JSON shaping can be compiled and exercised without
// wxWidgets, Boost or a printer. The server (StatusApiServer.cpp) feeds it a parsed request and a
// snapshot of the printers that was taken on the GUI thread, and writes back the response it returns.
//
// Security rules owned here:
//   * the API answers only while it is enabled and a token is configured;
//   * only loopback peers and loopback Host names are served (the DNS rebinding guard);
//   * only GET is served; every other method is 405;
//   * every endpoint needs "Authorization: Bearer <token>", compared in constant time;
//   * no response carries a CORS header.

#include "../DeviceLiveReadings.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace StatusApi {

// ---------------------------------------------------------------------------------
// Limits and defaults
// ---------------------------------------------------------------------------------

constexpr int         kDefaultPort       = 18787;
constexpr int         kMinPort           = 1024;
constexpr int         kMaxPort           = 65535;
constexpr std::size_t kMaxRequestHeader  = 8192; // bytes of request line plus headers
constexpr std::size_t kMaxTargetLength   = 256;
constexpr std::size_t kMaxDeviceIdLength = 64;
constexpr std::size_t kMinTokenLength    = 32;
constexpr std::size_t kMaxTokenLength    = 128;
constexpr int         kReadTimeoutSeconds = 5;

// The address the server binds. It is a constant on purpose: there is no setting that widens it.
constexpr const char *kBindAddress = "127.0.0.1";

// AppConfig keys. The token is never written to a log.
constexpr const char *kEnabledConfigKey = "status_api_enabled";
constexpr const char *kPortConfigKey    = "status_api_port";
constexpr const char *kTokenConfigKey   = "status_api_token";

// ---------------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------------

struct Settings
{
    bool        enabled = false; // off by default
    int         port    = kDefaultPort;
    std::string token;
};

// Parses the stored port text. Anything that is not a whole number in [kMinPort, kMaxPort] gives
// the default, so a hand-edited configuration can never make the server listen on a privileged port.
inline int parse_port(const std::string &text)
{
    if (text.empty() || text.size() > 5) return kDefaultPort;
    long value = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return kDefaultPort;
        value = value * 10 + (c - '0');
    }
    return (value < kMinPort || value > kMaxPort) ? kDefaultPort : static_cast<int>(value);
}

// A token is usable when it is printable, has no spaces and has a length that carries enough entropy.
// The generated token (64 hexadecimal characters) always satisfies this.
inline bool token_usable(const std::string &token)
{
    if (token.size() < kMinTokenLength || token.size() > kMaxTokenLength) return false;
    for (char c : token) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u <= 0x20 || u >= 0x7f) return false;
    }
    return true;
}

inline std::string hex_encode(const unsigned char *bytes, std::size_t count)
{
    static const char digits[] = "0123456789abcdef";
    std::string       out;
    out.reserve(count * 2);
    for (std::size_t i = 0; i < count; ++i) {
        out.push_back(digits[bytes[i] >> 4]);
        out.push_back(digits[bytes[i] & 0x0f]);
    }
    return out;
}

// ---------------------------------------------------------------------------------
// Constant-time comparison and bearer parsing
// ---------------------------------------------------------------------------------

// Compares in time that depends on the longer length only, never on where the texts differ.
inline bool constant_time_equal(const std::string &a, const std::string &b)
{
    const std::size_t length = std::max(a.size(), b.size());
    unsigned char     diff   = static_cast<unsigned char>(a.size() != b.size());
    for (std::size_t i = 0; i < length; ++i) {
        const unsigned char x = i < a.size() ? static_cast<unsigned char>(a[i]) : 0;
        const unsigned char y = i < b.size() ? static_cast<unsigned char>(b[i]) : 0;
        diff |= static_cast<unsigned char>(x ^ y);
    }
    return diff == 0;
}

// Extracts <token> from "Bearer <token>". The scheme is case-insensitive (RFC 7235); an empty
// token, a missing scheme or extra words give an empty string.
inline std::string parse_bearer(const std::string &header)
{
    std::size_t i = 0;
    while (i < header.size() && (header[i] == ' ' || header[i] == '\t')) ++i;
    static const char scheme[] = "bearer";
    for (std::size_t k = 0; k < 6; ++k, ++i)
        if (i >= header.size() || std::tolower(static_cast<unsigned char>(header[i])) != scheme[k]) return {};
    if (i >= header.size() || (header[i] != ' ' && header[i] != '\t')) return {};
    while (i < header.size() && (header[i] == ' ' || header[i] == '\t')) ++i;
    std::size_t end = header.size();
    while (end > i && (header[end - 1] == ' ' || header[end - 1] == '\t')) --end;
    const std::string token = header.substr(i, end - i);
    for (char c : token)
        if (static_cast<unsigned char>(c) <= 0x20 || static_cast<unsigned char>(c) >= 0x7f) return {};
    return token;
}

// ---------------------------------------------------------------------------------
// Loopback Host guard (DNS rebinding)
// ---------------------------------------------------------------------------------

// Accepts "localhost", "127.0.0.1" and "[::1]", each with an optional ":port". A page on another
// origin that resolves its own name to 127.0.0.1 sends its own Host and is refused.
inline bool host_header_is_loopback(const std::string &host)
{
    std::string h = host;
    std::transform(h.begin(), h.end(), h.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::string name = h;
    if (!h.empty() && h[0] == '[') {
        const std::size_t close = h.find(']');
        if (close == std::string::npos) return false;
        name = h.substr(0, close + 1);
        const std::string rest = h.substr(close + 1);
        if (!rest.empty() && (rest[0] != ':' || rest.size() == 1)) return false;
    } else {
        const std::size_t colon = h.rfind(':');
        if (colon != std::string::npos) {
            name = h.substr(0, colon);
            if (colon + 1 >= h.size()) return false;
            for (std::size_t k = colon + 1; k < h.size(); ++k)
                if (h[k] < '0' || h[k] > '9') return false;
        }
    }
    return name == "localhost" || name == "127.0.0.1" || name == "[::1]";
}

// ---------------------------------------------------------------------------------
// Request, response and snapshot
// ---------------------------------------------------------------------------------

struct Request
{
    std::string method;        // "GET"
    std::string target;        // path with an optional query, as received
    std::string authorization; // the Authorization header value, empty when absent
    std::string host;          // the Host header value
    bool        peer_is_loopback = false;
};

struct Response
{
    int         status = 500;
    std::string body;                  // UTF-8 JSON
    std::string allow;                 // value of the Allow header on 405
    bool        www_authenticate = false; // 401 adds WWW-Authenticate: Bearer
};

// One printer as the GUI thread saw it. Door fields mirror MachineObject::door_open_bit and
// MachineObject::support_door_open_check().
struct PrinterSnapshot
{
    std::string dev_id;
    std::string name;
    std::string model;
    bool        connected = false;
    bool        printing  = false;
    bool        door_supported = false;
    int         door_bit       = -1; // -1 unknown, 0 closed, 1 open
    std::string updated_at;          // ISO 8601 UTC of the last observed change
};

// ---------------------------------------------------------------------------------
// JSON shaping
// ---------------------------------------------------------------------------------

inline std::string json_string(const std::string &text)
{
    std::string out = "\"";
    for (char ch : text) {
        const unsigned char c = static_cast<unsigned char>(ch);
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20 || c == 0x7f) {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                out += buffer;
            } else {
                out.push_back(ch); // UTF-8 bytes pass through unchanged
            }
        }
    }
    out += "\"";
    return out;
}

inline const char *json_bool(bool v) { return v ? "true" : "false"; }

inline const char *door_state_text(const PrinterSnapshot &p)
{
    const auto reading = DeviceLive::door_open_reading(p.door_bit, p.door_supported);
    if (!reading) return "unknown";
    return *reading ? "open" : "closed";
}

inline std::string door_json(const PrinterSnapshot &p)
{
    std::string out = "{\"dev_id\":" + json_string(p.dev_id);
    out += ",\"supported\":";
    out += json_bool(p.door_supported);
    out += ",\"state\":\"";
    out += door_state_text(p);
    out += "\",\"source\":\"home_flag bit 23 or print.stat bit 23\",\"beta\":true,\"updated_at\":";
    out += json_string(p.updated_at);
    out += "}";
    return out;
}

inline std::string printer_json(const PrinterSnapshot &p)
{
    std::string out = "{\"dev_id\":" + json_string(p.dev_id);
    out += ",\"name\":" + json_string(p.name);
    out += ",\"model\":" + json_string(p.model);
    out += ",\"connected\":";
    out += json_bool(p.connected);
    out += ",\"printing\":";
    out += json_bool(p.printing);
    out += "}";
    return out;
}

inline std::string printers_json(const std::vector<PrinterSnapshot> &printers)
{
    std::string out = "{\"printers\":[";
    for (std::size_t i = 0; i < printers.size(); ++i) {
        if (i) out += ",";
        out += printer_json(printers[i]);
    }
    out += "]}";
    return out;
}

inline std::string error_json(const char *code, const char *message)
{
    return std::string("{\"error\":") + json_string(code) + ",\"message\":" + json_string(message) + "}";
}

// ---------------------------------------------------------------------------------
// Time stamps
// ---------------------------------------------------------------------------------

inline std::string iso8601_utc(std::time_t t)
{
    std::tm parts{};
#ifdef _WIN32
    gmtime_s(&parts, &t);
#else
    gmtime_r(&t, &parts);
#endif
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ", parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday,
                  parts.tm_hour, parts.tm_min, parts.tm_sec);
    return buffer;
}

// Carries the previous time stamp over when a printer's observable state did not change since the
// previous snapshot, so updated_at means "when this state was last seen to change", not "when the
// application last looked".
inline void carry_updated_at(const std::vector<PrinterSnapshot> &previous, std::vector<PrinterSnapshot> &next, const std::string &now)
{
    for (PrinterSnapshot &n : next) {
        n.updated_at = now;
        for (const PrinterSnapshot &p : previous) {
            if (p.dev_id != n.dev_id) continue;
            if (p.connected == n.connected && p.printing == n.printing && p.door_supported == n.door_supported &&
                p.door_bit == n.door_bit && p.name == n.name && p.model == n.model && !p.updated_at.empty())
                n.updated_at = p.updated_at;
            break;
        }
    }
}

// ---------------------------------------------------------------------------------
// Routing
// ---------------------------------------------------------------------------------

inline bool device_id_valid(const std::string &id)
{
    if (id.empty() || id.size() > kMaxDeviceIdLength) return false;
    for (char c : id) {
        const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_' || c == '.';
        if (!ok) return false;
    }
    return true;
}

inline Response make_error(int status, const char *code, const char *message)
{
    Response r;
    r.status = status;
    r.body   = error_json(code, message);
    return r;
}

// Answers one request. `printers` is the snapshot taken on the GUI thread; nothing here touches a
// MachineObject. The order of the checks is part of the contract: a request that is not from this
// computer, not for this computer, or not a GET is refused before the token is looked at.
inline Response handle_request(const Settings &settings, const std::vector<PrinterSnapshot> &printers, const Request &request)
{
    if (!settings.enabled || !token_usable(settings.token))
        return make_error(503, "disabled", "The status API is switched off.");
    if (!request.peer_is_loopback)
        return make_error(403, "forbidden", "Only requests from this computer are served.");
    if (!host_header_is_loopback(request.host))
        return make_error(403, "forbidden", "The Host header must name this computer.");
    if (request.method != "GET") {
        Response r = make_error(405, "method_not_allowed", "Only GET is supported.");
        r.allow    = "GET";
        return r;
    }
    if (request.target.size() > kMaxTargetLength)
        return make_error(414, "target_too_long", "The request target is too long.");

    const std::string presented = parse_bearer(request.authorization);
    if (presented.empty() || !constant_time_equal(presented, settings.token)) {
        Response r       = make_error(401, "unauthorized", "A valid bearer token is required.");
        r.www_authenticate = true;
        return r;
    }

    std::string path = request.target;
    const std::size_t query = path.find_first_of("?#");
    if (query != std::string::npos) path.erase(query);
    while (path.size() > 1 && path.back() == '/') path.pop_back();

    Response ok;
    ok.status = 200;
    if (path == "/api/v1/health") {
        ok.body = "{\"status\":\"ok\",\"api_version\":1}";
        return ok;
    }
    if (path == "/api/v1/printers") {
        ok.body = printers_json(printers);
        return ok;
    }
    static const std::string prefix = "/api/v1/printers/";
    static const std::string suffix = "/door";
    if (path.size() > prefix.size() + suffix.size() && path.compare(0, prefix.size(), prefix) == 0 &&
        path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0) {
        const std::string id = path.substr(prefix.size(), path.size() - prefix.size() - suffix.size());
        if (device_id_valid(id)) {
            for (const PrinterSnapshot &p : printers) {
                if (p.dev_id == id) {
                    ok.body = door_json(p);
                    return ok;
                }
            }
        }
        return make_error(404, "printer_not_found", "No printer with that id is known to this application.");
    }
    return make_error(404, "not_found", "Unknown endpoint.");
}

inline const char *status_reason(int status)
{
    switch (status) {
    case 200: return "OK";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 414: return "URI Too Long";
    case 503: return "Service Unavailable";
    default: return "Internal Server Error";
    }
}

}}} // namespace Slic3r::GUI::StatusApi

#endif // slic3r_GUI_StatusApiCore_hpp_
