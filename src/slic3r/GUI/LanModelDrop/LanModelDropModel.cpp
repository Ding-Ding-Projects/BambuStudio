#include "LanModelDropModel.hpp"

#include "../AppUpdateCheckPolicy.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace Slic3r { namespace GUI { namespace LanModelDrop {

namespace {

char ascii_lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

std::string lower(std::string_view text)
{
    std::string out(text);
    for (char &c : out) c = ascii_lower(c);
    return out;
}

bool ends_with_ci(std::string_view text, std::string_view suffix)
{
    if (text.size() < suffix.size()) return false;
    for (std::size_t i = 0; i < suffix.size(); ++i)
        if (ascii_lower(text[text.size() - suffix.size() + i]) != ascii_lower(suffix[i])) return false;
    return true;
}

bool starts_with(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

bool is_lower_hex(std::string_view text, std::size_t length)
{
    if (text.size() != length) return false;
    return std::all_of(text.begin(), text.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

bool visible_ascii(std::string_view text)
{
    return std::all_of(text.begin(), text.end(), [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return u >= 0x21 && u <= 0x7E;
    });
}

// One decoded code point and the bytes it used. `ok` is false for a malformed or overlong sequence,
// a surrogate, or a value above U+10FFFF.
struct Decoded
{
    char32_t    code = 0;
    std::size_t length = 1;
    bool        ok = false;
};

Decoded decode_utf8(std::string_view text, std::size_t at)
{
    Decoded d;
    const auto lead = static_cast<unsigned char>(text[at]);
    std::size_t need = 0;
    char32_t    code = 0;
    if (lead < 0x80) { d.code = lead; d.ok = true; return d; }
    if ((lead & 0xE0) == 0xC0) { need = 1; code = lead & 0x1F; }
    else if ((lead & 0xF0) == 0xE0) { need = 2; code = lead & 0x0F; }
    else if ((lead & 0xF8) == 0xF0) { need = 3; code = lead & 0x07; }
    else return d;
    if (at + need >= text.size()) return d;
    for (std::size_t i = 1; i <= need; ++i) {
        const auto next = static_cast<unsigned char>(text[at + i]);
        if ((next & 0xC0) != 0x80) return d;
        code = (code << 6) | (next & 0x3F);
    }
    static const char32_t minimum[] = {0, 0x80, 0x800, 0x10000};
    if (code < minimum[need] || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) return d;
    d.code   = code;
    d.length = need + 1;
    d.ok     = true;
    return d;
}

void append_utf8(std::string &out, char32_t code)
{
    if (code < 0x80) out.push_back(static_cast<char>(code));
    else if (code < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (code >> 6)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else if (code < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (code >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (code >> 18)));
        out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    }
}

bool is_control(char32_t c) { return c < 0x20 || (c >= 0x7F && c <= 0x9F); }

// Characters that reorder or hide text: they could make a notification show a different name than
// the file really has.
bool is_format_override(char32_t c)
{
    return (c >= 0x202A && c <= 0x202E) || (c >= 0x2066 && c <= 0x2069) || c == 0x200E || c == 0x200F ||
           c == 0x061C || c == 0xFEFF;
}

bool is_space(char32_t c) { return c == ' ' || c == 0xA0 || c == 0x3000 || (c >= 0x2000 && c <= 0x200B); }

// Decode to code points, keeping those `keep` accepts. Returns false on invalid UTF-8 when strict.
bool decode_all(std::string_view text, std::vector<char32_t> &out, bool strict)
{
    for (std::size_t at = 0; at < text.size();) {
        const Decoded d = decode_utf8(text, at);
        if (!d.ok) {
            if (strict) return false;
            ++at;
            continue;
        }
        out.push_back(d.code);
        at += d.length;
    }
    return true;
}

std::string encode_all(const std::vector<char32_t> &codes)
{
    std::string out;
    for (char32_t c : codes) append_utf8(out, c);
    return out;
}

void trim(std::vector<char32_t> &codes)
{
    while (!codes.empty() && is_space(codes.back())) codes.pop_back();
    std::size_t first = 0;
    while (first < codes.size() && is_space(codes[first])) ++first;
    codes.erase(codes.begin(), codes.begin() + static_cast<std::ptrdiff_t>(first));
}

// Windows takes a last path component for a device when the part before its first dot, with trailing
// spaces removed, is a device name in any case: "COM3 .x.stl" opens the serial port COM3. The names
// are CON, PRN, AUX, NUL, CONIN$, CONOUT$, COM and LPT followed by a digit or by a superscript one,
// two or three (CPython's ntpath.isreserved encodes the same rule; COM0 and LPT0 are on Microsoft's
// list of names not to use, so they count too). Any space-like character counts as a space here.
bool is_device_name(const std::vector<char32_t> &name)
{
    std::size_t end = static_cast<std::size_t>(std::find(name.begin(), name.end(), U'.') - name.begin());
    while (end > 0 && is_space(name[end - 1])) --end;
    std::u32string base;
    for (std::size_t i = 0; i < end; ++i) {
        const char32_t c = name[i];
        base.push_back(c >= U'a' && c <= U'z' ? static_cast<char32_t>(c - U'a' + U'A') : c);
    }
    if (base == U"CON" || base == U"PRN" || base == U"AUX" || base == U"NUL" || base == U"CONIN$" || base == U"CONOUT$")
        return true;
    if (base.size() != 4 || (base.compare(0, 3, U"COM") != 0 && base.compare(0, 3, U"LPT") != 0)) return false;
    const char32_t n = base[3];
    return (n >= U'0' && n <= U'9') || n == 0xB9 || n == 0xB2 || n == 0xB3;
}

void trim_trailing_dots_and_spaces(std::vector<char32_t> &codes)
{
    while (!codes.empty() && (codes.back() == U'.' || is_space(codes.back()))) codes.pop_back();
}

// How much of a received file the content checks look at, as on the drop site.
constexpr std::size_t kSniffHeadBytes = 256 * 1024;

// Text as the drop site defines it: tab, line feed, form feed, carriage return, visible ASCII and
// any byte of 0x80 or above (UTF-8 sequences are not validated). NUL and other controls are not.
bool is_model_text(std::string_view bytes)
{
    return std::all_of(bytes.begin(), bytes.end(), [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return (u >= 0x20 && u != 0x7F) || u == '\t' || u == '\n' || u == '\f' || u == '\r';
    });
}

// The text after an optional UTF-8 byte order mark and leading tabs, line ends, form feeds and spaces.
std::string_view leading_text(std::string_view head)
{
    if (starts_with(head, "\xEF\xBB\xBF")) head.remove_prefix(3);
    while (!head.empty() && (head.front() == '\t' || head.front() == '\n' || head.front() == '\f' || head.front() == '\r' ||
                             head.front() == ' '))
        head.remove_prefix(1);
    return head;
}

// A vertex line: at the start or after a line feed or carriage return, optional tabs or spaces,
// then "v" and a tab or space.
bool has_obj_vertex_line(std::string_view head)
{
    for (std::size_t at = 0; at < head.size(); ++at) {
        if (at > 0 && head[at - 1] != '\n' && head[at - 1] != '\r') continue;
        std::size_t i = at;
        while (i < head.size() && (head[i] == ' ' || head[i] == '\t')) ++i;
        if (i + 1 < head.size() && head[i] == 'v' && (head[i + 1] == ' ' || head[i + 1] == '\t')) return true;
    }
    return false;
}

using json = nlohmann::json;

std::optional<json> parse_object(std::string_view body)
{
    json value = json::parse(body.begin(), body.end(), nullptr, false);
    if (value.is_discarded() || !value.is_object()) return std::nullopt;
    return value;
}

bool read_string(const json &object, const char *key, std::string &out)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string()) return false;
    out = it->get<std::string>();
    return true;
}

bool read_count(const json &object, const char *key, std::uint64_t &out)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_number_unsigned()) return false;
    out = it->get<std::uint64_t>();
    return true;
}

bool protocol_is_supported(const json &object)
{
    const auto it = object.find("protocol");
    return it != object.end() && it->is_number_integer() && it->get<long long>() == kProtocolVersion;
}

int parse_port(std::string_view digits)
{
    if (digits.empty() || digits.size() > 5) return -1;
    int port = 0;
    for (char c : digits) {
        if (c < '0' || c > '9') return -1;
        port = port * 10 + (c - '0');
    }
    return port >= 1 && port <= 65535 ? port : -1;
}

bool valid_hostname(std::string_view host)
{
    if (host.empty() || host.size() > 253 || host.front() == '.' || host.front() == '-' || host.back() == '-') return false;
    std::size_t label = 0;
    for (char c : host) {
        if (c == '.') {
            if (label == 0) return false;
            label = 0;
            continue;
        }
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
        if (!ok || ++label > 63) return false;
    }
    return label > 0 || host.back() == '.';
}

bool valid_ipv6_literal(std::string_view inner)
{
    if (inner.empty() || inner.size() > 45) return false;
    return std::all_of(inner.begin(), inner.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || c == ':' || c == '.';
    }) && inner.find(':') != std::string_view::npos;
}

std::optional<unsigned> ipv4_octet(std::string_view part)
{
    if (part.empty() || part.size() > 3 || (part.size() > 1 && part.front() == '0')) return std::nullopt;
    unsigned value = 0;
    for (char c : part) {
        if (c < '0' || c > '9') return std::nullopt;
        value = value * 10 + static_cast<unsigned>(c - '0');
    }
    if (value > 255) return std::nullopt;
    return value;
}

std::optional<std::vector<unsigned>> ipv4_octets(std::string_view text)
{
    std::vector<unsigned> octets;
    std::size_t start = 0;
    for (int i = 0; i < 4; ++i) {
        const std::size_t dot = text.find('.', start);
        const std::string_view part = i < 3 ? text.substr(start, dot == std::string_view::npos ? std::string_view::npos : dot - start)
                                            : text.substr(start);
        if (i < 3 && dot == std::string_view::npos) return std::nullopt;
        const auto octet = ipv4_octet(part);
        if (!octet) return std::nullopt;
        octets.push_back(*octet);
        start = dot + 1;
    }
    return octets;
}

} // namespace

// ---------------------------------------------------------------------------------------------

std::optional<FileType> type_from_name(std::string_view name)
{
    if (name == "3mf") return FileType::ThreeMF;
    if (name == "stl") return FileType::Stl;
    if (name == "step") return FileType::Step;
    if (name == "obj") return FileType::Obj;
    if (name == "amf") return FileType::Amf;
    return std::nullopt;
}

const char *type_name(FileType type)
{
    switch (type) {
    case FileType::ThreeMF: return "3mf";
    case FileType::Stl: return "stl";
    case FileType::Step: return "step";
    case FileType::Obj: return "obj";
    case FileType::Amf: return "amf";
    }
    return "stl";
}

std::optional<FileType> type_from_extension(std::string_view file_name)
{
    if (ends_with_ci(file_name, ".3mf")) return FileType::ThreeMF;
    if (ends_with_ci(file_name, ".stl")) return FileType::Stl;
    if (ends_with_ci(file_name, ".step") || ends_with_ci(file_name, ".stp")) return FileType::Step;
    if (ends_with_ci(file_name, ".obj")) return FileType::Obj;
    if (ends_with_ci(file_name, ".amf")) return FileType::Amf;
    return std::nullopt;
}

bool content_matches(FileType type, std::string_view bytes)
{
    // The drop site's rules (lan-model-drop/server/sniff.mjs): markers are looked for in the first
    // 256 KiB, after an optional UTF-8 byte order mark and leading white space.
    static constexpr std::string_view zip("PK\x03\x04", 4);
    const std::string_view head = bytes.substr(0, kSniffHeadBytes);
    const std::string_view lead = leading_text(head);
    switch (type) {
    case FileType::ThreeMF: return starts_with(bytes, zip);
    case FileType::Step: return starts_with(lead, "ISO-10303-21");
    case FileType::Amf: return starts_with(bytes, zip) || (starts_with(lead, "<") && lead.find("<amf") != std::string_view::npos);
    case FileType::Stl: {
        // Binary STL headers may also begin with "solid", so the exact length rule comes first.
        if (bytes.size() >= 84) {
            const auto *p = reinterpret_cast<const unsigned char *>(bytes.data()) + 80;
            const std::uint64_t triangles = std::uint64_t(p[0]) | (std::uint64_t(p[1]) << 8) | (std::uint64_t(p[2]) << 16) |
                                            (std::uint64_t(p[3]) << 24);
            if (84 + 50 * triangles == bytes.size()) return true;
        }
        return is_model_text(bytes) && starts_with(lead, "solid") && lead.find("facet") != std::string_view::npos;
    }
    case FileType::Obj: return is_model_text(bytes) && has_obj_vertex_line(head);
    }
    return false;
}

// ---------------------------------------------------------------------------------------------

std::string sanitize_file_name(std::string_view raw)
{
    std::vector<char32_t> codes;
    if (!decode_all(raw, codes, /*strict*/ true)) return {};
    // Base name only: whatever follows the last path separator of either kind.
    const auto separator = std::find_if(codes.rbegin(), codes.rend(), [](char32_t c) { return c == '/' || c == '\\'; });
    if (separator != codes.rend()) codes.erase(codes.begin(), separator.base());
    std::vector<char32_t> kept;
    for (char32_t c : codes) {
        if (is_control(c) || is_format_override(c)) continue;
        if (c == '<' || c == '>' || c == ':' || c == '"' || c == '|' || c == '?' || c == '*') continue;
        kept.push_back(c);
    }
    trim(kept);
    // Windows drops trailing dots and spaces from a name, which could change its extension.
    trim_trailing_dots_and_spaces(kept);

    const std::string name = encode_all(kept);
    const auto        type = type_from_extension(name);
    if (!type) return {};
    const std::string extension = name.substr(name.rfind('.'));
    std::vector<char32_t> stem(kept.begin(), kept.end() - static_cast<std::ptrdiff_t>(extension.size()));
    if (std::all_of(stem.begin(), stem.end(), is_space)) return {};
    const std::size_t room = kMaxFileNameChars - extension.size();
    if (stem.size() > room) {
        stem.resize(room);
        trim_trailing_dots_and_spaces(stem);
        if (stem.empty()) return {};
    }
    // After the cut, which can leave a device name ("COM1" followed by spaces): a leading underscore
    // makes it an ordinary name, since no device name starts with one.
    if (is_device_name(stem)) {
        stem.insert(stem.begin(), U'_');
        if (stem.size() > room) {
            stem.pop_back();
            trim_trailing_dots_and_spaces(stem);
        }
    }
    return encode_all(stem) + extension;
}

bool is_windows_reserved_name(std::string_view name)
{
    std::vector<char32_t> codes;
    if (name.empty() || name == "." || name == ".." || !decode_all(name, codes, /*strict*/ true)) return true;
    if (codes.back() == U'.' || codes.back() == U' ') return true;
    for (char32_t c : codes)
        if (c < 0x20 || c == U'<' || c == U'>' || c == U':' || c == U'"' || c == U'/' || c == U'\\' || c == U'|' || c == U'?' ||
            c == U'*')
            return true;
    return is_device_name(codes);
}

bool is_clean_file_name(std::string_view name)
{
    return !name.empty() && sanitize_file_name(name) == name && !is_windows_reserved_name(name);
}

std::string fallback_file_name(FileType type)
{
    return std::string("model.") + type_name(type);
}

std::string sanitize_sender(std::string_view raw)
{
    std::vector<char32_t> codes;
    decode_all(raw, codes, /*strict*/ false);
    std::vector<char32_t> kept;
    for (char32_t c : codes)
        if (!is_control(c) && !is_format_override(c)) kept.push_back(c);
    trim(kept);
    if (kept.size() > kMaxSenderChars) {
        kept.resize(kMaxSenderChars);
        trim(kept);
    }
    return encode_all(kept);
}

bool is_item_id(std::string_view id) { return is_lower_hex(id, 32); }
bool is_sha256_hex(std::string_view digest) { return is_lower_hex(digest, 64); }

bool is_utc_stamp(std::string_view stamp)
{
    long long seconds = 0;
    return AppUpdateCheckPolicy::parse_utc(std::string(stamp), seconds);
}

bool is_station_key(std::string_view key) { return key.size() >= 16 && key.size() <= 512 && visible_ascii(key); }
bool is_drop_code(std::string_view code) { return !code.empty() && code.size() <= 64 && visible_ascii(code); }

std::string to_hex(const unsigned char *bytes, std::size_t size)
{
    static const char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        out.push_back(digits[bytes[i] >> 4]);
        out.push_back(digits[bytes[i] & 0x0F]);
    }
    return out;
}

std::string header_value(std::string_view headers, std::string_view name)
{
    std::string value;
    std::size_t start = 0;
    while (start < headers.size()) {
        std::size_t end = headers.find('\n', start);
        if (end == std::string_view::npos) end = headers.size();
        std::string_view line = headers.substr(start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        const std::size_t colon = line.find(':');
        if (colon == std::string_view::npos || colon != name.size()) continue;
        bool same = true;
        for (std::size_t i = 0; i < colon && same; ++i) same = ascii_lower(line[i]) == ascii_lower(name[i]);
        if (!same) continue;
        std::string_view rest = line.substr(colon + 1);
        while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) rest.remove_prefix(1);
        while (!rest.empty() && (rest.back() == ' ' || rest.back() == '\t')) rest.remove_suffix(1);
        value.assign(rest.begin(), rest.end());
    }
    return value;
}

// ---------------------------------------------------------------------------------------------

InboxListing parse_inbox(std::string_view body)
{
    InboxListing listing;
    const auto object = parse_object(body);
    if (!object) return listing;
    const auto items = object->find("items");
    if (items == object->end() || !items->is_array()) return listing;
    listing.ok = true;
    std::set<std::string> seen;
    std::size_t index = 0;
    for (const json &entry : *items) {
        if (++index > kMaxInboxItems) break;
        std::string id;
        if (!entry.is_object() || !read_string(entry, "id", id) || !is_item_id(id) || seen.count(id) != 0) {
            ++listing.unusable;
            continue;
        }
        seen.insert(id);
        InboxItem   item;
        item.id = id;
        std::string raw_name, raw_sender, type;
        const bool fields = read_string(entry, "fileName", raw_name) && read_count(entry, "bytes", item.bytes) &&
                            read_string(entry, "sha256", item.sha256) && read_string(entry, "receivedAt", item.received_at) &&
                            read_string(entry, "type", type);
        // The sender is optional: absent or null means nobody gave a name.
        bool sender_ok = true;
        const auto sender = entry.find("sender");
        if (sender != entry.end() && !sender->is_null()) {
            sender_ok = sender->is_string();
            if (sender_ok) raw_sender = sender->get<std::string>();
        }
        item.file_name = sanitize_file_name(raw_name);
        item.sender    = sanitize_sender(raw_sender);
        const auto declared = type_from_name(type);
        const auto by_name  = type_from_extension(item.file_name);
        const bool valid = fields && sender_ok && !item.file_name.empty() && item.bytes > 0 && item.bytes <= kMaxItemBytes &&
                           is_sha256_hex(item.sha256) && is_utc_stamp(item.received_at) && declared && by_name &&
                           *declared == *by_name;
        if (!valid) {
            listing.invalid_ids.push_back(id);
            continue;
        }
        item.type = *declared;
        listing.items.push_back(std::move(item));
    }
    return listing;
}

std::optional<StationStatus> parse_status(std::string_view body)
{
    const auto object = parse_object(body);
    if (!object || !protocol_is_supported(*object)) return std::nullopt;
    StationStatus status;
    if (!read_string(*object, "stationName", status.station_name) || !read_string(*object, "dropCode", status.drop_code) ||
        !read_count(*object, "queued", status.queued) || !read_count(*object, "queuedBytes", status.queued_bytes) ||
        !read_count(*object, "maxBytes", status.max_bytes))
        return std::nullopt;
    const auto ttl = object->find("ttlHours");
    if (ttl == object->end() || !ttl->is_number() || !(ttl->get<double>() > 0.0)) return std::nullopt;
    status.ttl_hours = ttl->get<double>();
    if (!is_drop_code(status.drop_code)) return std::nullopt;
    // Optional and never fatal: a value that is not an acceptable link base is ignored, and the
    // invite falls back to the configured or the LAN address.
    std::string public_url;
    if (read_string(*object, "publicUrl", public_url)) status.public_url = normalize_public_url(public_url);
    status.station_name = sanitize_sender(status.station_name);
    return status;
}

std::optional<std::string> parse_drop_code(std::string_view body)
{
    const auto object = parse_object(body);
    std::string code;
    if (!object || !read_string(*object, "dropCode", code) || !is_drop_code(code)) return std::nullopt;
    return code;
}

bool parse_health(std::string_view body)
{
    const auto object = parse_object(body);
    if (!object || !protocol_is_supported(*object)) return false;
    const auto ok = object->find("ok");
    std::string service;
    return ok != object->end() && ok->is_boolean() && ok->get<bool>() && read_string(*object, "service", service) &&
           service == "lan-model-drop";
}

// ---------------------------------------------------------------------------------------------

LinkState classify_failure(const Exchange &exchange)
{
    if (!exchange.completed) return LinkState::NotReachable;
    if (exchange.http_status == 401 || exchange.http_status == 403) return LinkState::WrongStationKey;
    if (exchange.http_status >= 500) return LinkState::NotReachable;
    return LinkState::ProtocolNotSupported;
}

LinkState classify_status(const Exchange &exchange, StationStatus *status)
{
    if (!exchange.completed || exchange.http_status < 200 || exchange.http_status >= 300) return classify_failure(exchange);
    const auto parsed = parse_status(exchange.body);
    if (!parsed) return LinkState::ProtocolNotSupported;
    if (status) *status = *parsed;
    return LinkState::Connected;
}

bool is_error_state(LinkState state)
{
    switch (state) {
    case LinkState::NotReachable:
    case LinkState::WrongStationKey:
    case LinkState::ProtocolNotSupported:
    case LinkState::NeedsStationKey:
    case LinkState::InvalidAddress:
    case LinkState::KeyStorageUnavailable: return true;
    case LinkState::Off:
    case LinkState::Checking:
    case LinkState::Connected: return false;
    }
    return true;
}

int next_poll_delay_seconds(int consecutive_failures)
{
    if (consecutive_failures <= 0) return kPollSeconds;
    int delay = kPollSeconds;
    for (int i = 0; i < consecutive_failures && delay < kMaxBackoffSeconds; ++i) delay *= 2;
    return std::min(delay, kMaxBackoffSeconds);
}

// ---------------------------------------------------------------------------------------------

std::optional<BaseAddress> parse_base_address(std::string_view input)
{
    std::string text(input);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.pop_back();
    std::size_t lead = 0;
    while (lead < text.size() && (text[lead] == ' ' || text[lead] == '\t')) ++lead;
    text.erase(0, lead);
    if (text.empty() || text.size() > 300 || !visible_ascii(text)) return std::nullopt;

    BaseAddress address;
    std::string rest;
    const std::size_t scheme_end = text.find("://");
    if (scheme_end == std::string::npos) {
        address.scheme = "http";
        rest           = text;
    } else {
        address.scheme = lower(text.substr(0, scheme_end));
        rest           = text.substr(scheme_end + 3);
    }
    if (address.scheme != "http" && address.scheme != "https") return std::nullopt;
    if (!rest.empty() && rest.back() == '/') rest.pop_back();
    if (rest.empty() || rest.find_first_of("/?#@\\") != std::string::npos) return std::nullopt;

    std::string host;
    std::string port_text;
    if (rest.front() == '[') {
        const std::size_t close = rest.find(']');
        if (close == std::string::npos) return std::nullopt;
        host = lower(rest.substr(0, close + 1));
        const std::string after = rest.substr(close + 1);
        if (!after.empty()) {
            if (after.front() != ':') return std::nullopt;
            port_text = after.substr(1);
            if (port_text.empty()) return std::nullopt;
        }
        if (!valid_ipv6_literal(std::string_view(host).substr(1, host.size() - 2))) return std::nullopt;
    } else {
        const std::size_t colon = rest.find(':');
        host = lower(rest.substr(0, colon));
        if (colon != std::string::npos) {
            port_text = rest.substr(colon + 1);
            if (port_text.empty()) return std::nullopt;
        }
        if (!is_ipv4(host) && !valid_hostname(host)) return std::nullopt;
    }
    if (!port_text.empty()) {
        address.port = parse_port(port_text);
        if (address.port < 0) return std::nullopt;
        address.explicit_port = true;
    } else
        address.port = address.scheme == "https" ? 443 : 80;
    address.host     = host;
    address.loopback = host == "localhost" || host == "[::1]" || host == "[::]" || host == "0.0.0.0" ||
                       (is_ipv4(host) && starts_with(host, "127."));
    address.base     = address.scheme + "://" + host + (address.explicit_port ? ":" + std::to_string(address.port) : std::string());
    return address;
}

std::string health_url(const BaseAddress &address) { return address.base + "/healthz"; }
std::string status_url(const BaseAddress &address) { return address.base + "/api/station/status"; }
std::string inbox_url(const BaseAddress &address) { return address.base + "/api/station/inbox"; }
std::string file_url(const BaseAddress &address, const std::string &id)
{
    // Only a validated id ever reaches a URL path.
    return is_item_id(id) ? address.base + "/api/station/files/" + id : std::string();
}
std::string drop_code_url(const BaseAddress &address) { return address.base + "/api/station/drop-code"; }

bool is_ipv4(std::string_view text) { return ipv4_octets(text).has_value(); }

bool is_private_ipv4(std::string_view text)
{
    const auto o = ipv4_octets(text);
    if (!o) return false;
    const auto &v = *o;
    return v[0] == 10 || (v[0] == 172 && v[1] >= 16 && v[1] <= 31) || (v[0] == 192 && v[1] == 168);
}

bool looks_virtual_adapter(std::string_view adapter_name)
{
    const std::string name = lower(adapter_name);
    for (const char *marker : {"vethernet", "virtual", "vmware", "virtualbox", "hyper-v", "docker", "wsl", "loopback",
                               "tap-", "tunnel", "vpn", "zerotier", "tailscale", "npcap", "bluetooth"})
        if (name.find(marker) != std::string::npos) return true;
    return false;
}

std::vector<std::string> lan_ipv4_candidates(const std::vector<AdapterAddress> &addresses)
{
    struct Scored
    {
        int         score;
        std::size_t order;
        std::string ipv4;
    };
    std::vector<Scored> scored;
    for (const AdapterAddress &a : addresses) {
        // Private ranges only: loopback, link-local, multicast and public addresses are never offered.
        if (!a.up || a.loopback || !is_private_ipv4(a.ipv4)) continue;
        if (std::any_of(scored.begin(), scored.end(), [&a](const Scored &s) { return s.ipv4 == a.ipv4; })) continue;
        const int score = (a.has_gateway ? 2 : 0) + (looks_virtual_adapter(a.adapter_name) ? 0 : 1);
        scored.push_back({score, scored.size(), a.ipv4});
    }
    std::stable_sort(scored.begin(), scored.end(), [](const Scored &l, const Scored &r) { return l.score > r.score; });
    std::vector<std::string> out;
    for (const Scored &s : scored) out.push_back(s.ipv4);
    return out;
}

std::string choose_lan_ipv4(const std::vector<AdapterAddress> &addresses)
{
    const auto candidates = lan_ipv4_candidates(addresses);
    return candidates.empty() ? std::string() : candidates.front();
}

// ---------------------------------------------------------------------------------------------

std::string normalize_public_url(std::string_view url)
{
    std::string text(url);
    if (text.empty() || text.size() > 1024 || !visible_ascii(text)) return {};
    const std::size_t scheme_end = text.find("://");
    if (scheme_end == std::string::npos) return {};
    const std::string scheme = lower(text.substr(0, scheme_end));
    if (scheme != "http" && scheme != "https") return {};
    std::string rest = text.substr(scheme_end + 3);
    // No query, fragment or backslash anywhere; no user name in the authority.
    if (rest.find_first_of("?#\\") != std::string::npos) return {};
    const std::size_t slash = rest.find('/');
    const std::string authority = rest.substr(0, slash);
    std::string       path      = slash == std::string::npos ? std::string() : rest.substr(slash);
    if (authority.empty() || authority.find('@') != std::string::npos) return {};
    const auto parsed = parse_base_address(scheme + "://" + authority);
    if (!parsed) return {};
    while (!path.empty() && path.back() == '/') path.pop_back();
    for (std::size_t i = 0; i < path.size(); ++i) {
        const char c = path[i];
        const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                           std::string_view("-._~/!$&'()*+,;=:@%").find(c) != std::string_view::npos;
        if (!plain) return {};
        if (c == '%' && (i + 2 >= path.size() || !std::isxdigit(static_cast<unsigned char>(path[i + 1])) ||
                         !std::isxdigit(static_cast<unsigned char>(path[i + 2]))))
            return {};
    }
    if (path.find("//") != std::string::npos) return {};
    return parsed->base + path;
}

InviteBase choose_invite_base(const std::string &public_url, const BaseAddress &configured,
                              const std::vector<std::string> &lan_candidates, const std::string &preferred_ipv4)
{
    InviteBase invite;
    const std::string public_base = normalize_public_url(public_url);
    if (!public_base.empty()) {
        invite.source = InviteSource::PublicUrl;
        invite.base   = public_base;
        return invite;
    }
    if (!configured.loopback && !configured.base.empty()) {
        invite.source = InviteSource::Configured;
        invite.base   = configured.base;
        return invite;
    }
    for (const std::string &candidate : lan_candidates)
        if (is_private_ipv4(candidate) &&
            std::find(invite.choices.begin(), invite.choices.end(), candidate) == invite.choices.end())
            invite.choices.push_back(candidate);
    if (invite.choices.empty() || configured.scheme.empty()) {
        invite.choices.clear();
        return invite;
    }
    const auto preferred = std::find(invite.choices.begin(), invite.choices.end(), preferred_ipv4);
    invite.chosen = preferred != invite.choices.end() ? *preferred : invite.choices.front();
    invite.source = InviteSource::LanAddress;
    invite.base   = configured.scheme + "://" + invite.chosen +
                  (configured.explicit_port ? ":" + std::to_string(configured.port) : std::string());
    return invite;
}

std::string invite_link(const std::string &base, const std::string &drop_code)
{
    if (base.empty() || !is_drop_code(drop_code)) return {};
    static const char digits[] = "0123456789ABCDEF";
    std::string code;
    for (const char c : drop_code) {
        const bool unreserved = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                                std::string_view("-_.!~*'()").find(c) != std::string_view::npos;
        if (unreserved) code.push_back(c);
        else {
            const auto u = static_cast<unsigned char>(c);
            code.push_back('%');
            code.push_back(digits[u >> 4]);
            code.push_back(digits[u & 0x0F]);
        }
    }
    return base + "/#code=" + code;
}

// ---------------------------------------------------------------------------------------------

std::string format_size(std::uint64_t bytes)
{
    char buffer[32];
    if (bytes < 1024ULL) std::snprintf(buffer, sizeof(buffer), "%llu B", static_cast<unsigned long long>(bytes));
    else if (bytes < 1024ULL * 1024ULL) std::snprintf(buffer, sizeof(buffer), "%.1f KB", double(bytes) / 1024.0);
    else if (bytes < 1024ULL * 1024ULL * 1024ULL) std::snprintf(buffer, sizeof(buffer), "%.1f MB", double(bytes) / (1024.0 * 1024.0));
    else std::snprintf(buffer, sizeof(buffer), "%.2f GB", double(bytes) / (1024.0 * 1024.0 * 1024.0));
    return buffer;
}

std::string distinct_text(const std::string &text, const std::vector<std::string> &existing)
{
    const auto used = [&existing](const std::string &candidate) {
        return std::find(existing.begin(), existing.end(), candidate) != existing.end();
    };
    if (!used(text)) return text;
    for (std::size_t n = 2;; ++n) {
        std::string candidate = text + " (" + std::to_string(n) + ")";
        if (!used(candidate)) return candidate;
    }
}

// ---------------------------------------------------------------------------------------------

namespace {

// A base address exactly as parse_base_address writes it.
bool is_normal_base(std::string_view base)
{
    const auto parsed = parse_base_address(base);
    return parsed && parsed->base == base;
}

} // namespace

std::string format_pending_deletes(const std::vector<PendingDelete> &pending)
{
    std::string out;
    for (const PendingDelete &p : pending) out += p.id + " " + p.base + "\n";
    return out;
}

std::vector<PendingDelete> parse_pending_deletes(std::string_view text)
{
    std::vector<PendingDelete> out;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos) end = text.size();
        std::string_view line = text.substr(start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        const std::size_t space = line.find(' ');
        if (space == std::string_view::npos) continue;
        PendingDelete entry{std::string(line.substr(0, space)), std::string(line.substr(space + 1))};
        if (!is_item_id(entry.id) || !is_normal_base(entry.base)) continue;
        if (std::find(out.begin(), out.end(), entry) != out.end()) continue;
        out.push_back(std::move(entry));
    }
    if (out.size() > kMaxPendingDeletes) out.erase(out.begin(), out.end() - static_cast<std::ptrdiff_t>(kMaxPendingDeletes));
    return out;
}

std::vector<InboxItem> InboxTracker::fresh(const std::vector<InboxItem> &listed) const
{
    std::vector<InboxItem> out;
    for (const InboxItem &item : listed)
        if (m_taken.count(item.id) == 0 && !delete_pending(item.id)) out.push_back(item);
    return out;
}

void InboxTracker::take(const std::string &id) { m_taken.insert(id); }

bool InboxTracker::retry_later(const std::string &id)
{
    const int attempts = ++m_attempts[id];
    if (attempts >= kMaxDownloadAttempts) {
        m_attempts.erase(id);
        return false;
    }
    m_taken.erase(id);
    return true;
}

void InboxTracker::queue_delete(const std::string &id, const std::string &base)
{
    if (!is_item_id(id) || !is_normal_base(base)) return;
    m_taken.insert(id);
    m_attempts.erase(id);
    const PendingDelete entry{id, base};
    if (std::find(m_deletes.begin(), m_deletes.end(), entry) != m_deletes.end()) return;
    m_deletes.push_back(entry);
    if (m_deletes.size() > kMaxPendingDeletes) m_deletes.erase(m_deletes.begin());
}

std::vector<std::string> InboxTracker::pending_deletes(const std::string &base) const
{
    std::vector<std::string> out;
    for (const PendingDelete &p : m_deletes)
        if (p.base == base) out.push_back(p.id);
    return out;
}

bool InboxTracker::delete_pending(const std::string &id) const
{
    return std::any_of(m_deletes.begin(), m_deletes.end(), [&id](const PendingDelete &p) { return p.id == id; });
}

void InboxTracker::delete_confirmed(const std::string &id, const std::string &base)
{
    m_deletes.erase(std::remove(m_deletes.begin(), m_deletes.end(), PendingDelete{id, base}), m_deletes.end());
}

void InboxTracker::prune(const std::vector<InboxItem> &listed)
{
    std::set<std::string> present;
    for (const InboxItem &item : listed) present.insert(item.id);
    for (auto it = m_taken.begin(); it != m_taken.end();) {
        if (present.count(*it) == 0 && !delete_pending(*it)) {
            m_attempts.erase(*it);
            it = m_taken.erase(it);
        } else
            ++it;
    }
}

}}} // namespace Slic3r::GUI::LanModelDrop
