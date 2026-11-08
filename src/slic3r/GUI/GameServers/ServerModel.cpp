#include "ServerModel.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <sstream>

// Only the gettext marker is needed here; the page translates with _() at
// display time. Defined locally so the model stays free of wxWidgets.
#ifndef L
#define L(s) s
#endif

namespace Slic3r { namespace GUI { namespace GameServers {

std::string fill(const std::string &templ, const std::vector<std::string> &values)
{
    std::string out;
    for (std::size_t i = 0; i < templ.size(); ++i) {
        if (templ[i] == '{' && i + 2 < templ.size() && templ[i + 2] == '}' && templ[i + 1] >= '1' && templ[i + 1] <= '9') {
            const std::size_t n = std::size_t(templ[i + 1] - '1');
            out += n < values.size() ? values[n] : std::string();
            i += 2;
            continue;
        }
        out += templ[i];
    }
    return out;
}

namespace {

using json = nlohmann::json;

bool all_digits(const std::string &s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
}

int to_int(const std::string &s, int fallback = 0)
{
    if (!all_digits(s) || s.size() > 9) return fallback;
    return std::stoi(s);
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

std::string trim(const std::string &s)
{
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

bool starts_with(const std::string &s, const std::string &p) { return s.size() >= p.size() && s.compare(0, p.size(), p) == 0; }
bool ends_with(const std::string &s, const std::string &p) { return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0; }

std::vector<std::string> split(const std::string &s, char sep)
{
    std::vector<std::string> out;
    std::string              part;
    std::istringstream       in(s);
    while (std::getline(in, part, sep)) out.push_back(part);
    return out;
}

// Releases and what places them: the release date and the data version.
struct ReleaseFact
{
    const char *id;
    const char *date;
    int         data_version;
};
const ReleaseFact kReleases[] = {
    {"1.9", "2016-02-29", 169},     {"1.10", "2016-06-08", 510},    {"1.11", "2016-11-14", 819},   {"1.12", "2017-06-07", 1139},
    {"1.13", "2018-07-18", 1519},   {"1.14", "2019-04-23", 1952},   {"1.15", "2019-12-10", 2225},  {"1.16", "2020-06-23", 2566},
    {"1.17", "2021-06-08", 2724},   {"1.18", "2021-11-30", 2860},   {"1.19", "2022-06-07", 3105},  {"1.19.3", "2022-12-07", 3218},
    {"1.19.4", "2023-03-14", 3337}, {"1.20", "2023-06-07", 3463},   {"1.20.2", "2023-09-21", 3578}, {"1.20.3", "2023-12-05", 3698},
    {"1.20.5", "2024-04-23", 3837}, {"1.21", "2024-06-13", 3953},   {"1.21.2", "2024-10-22", 4080}, {"1.21.4", "2024-12-03", 4189},
    {"1.21.5", "2025-03-25", 4325},
};

const ReleaseFact *release_fact(const std::string &id)
{
    for (const ReleaseFact &f : kReleases)
        if (id == f.id) return &f;
    return nullptr;
}

// "2025-01-22" for a snapshot week: the Monday-ish start of that week.
std::string snapshot_date(int year, int week)
{
    static const int month_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int day = std::max(0, (week - 1) * 7), month = 0;
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    while (month < 12) {
        const int days = month_days[month] + (month == 1 && leap ? 1 : 0);
        if (day < days) break;
        day -= days;
        ++month;
    }
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, std::min(month, 11) + 1, std::min(day, 30) + 1);
    return buf;
}

// The release a snapshot of this date leads to: the first one released after it.
std::string release_after(const std::string &date)
{
    for (const ReleaseFact &f : kReleases)
        if (date < f.date) return f.id;
    return "99";
}

bool parse_release_numbers(const std::string &s, int &a, int &b, int &c)
{
    const std::vector<std::string> parts = split(s, '.');
    if (parts.size() < 2 || parts.size() > 3) return false;
    for (const std::string &p : parts)
        if (!all_digits(p)) return false;
    a = to_int(parts[0]);
    b = to_int(parts[1]);
    c = parts.size() == 3 ? to_int(parts[2]) : 0;
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Versions

VersionId parse_version(const std::string &id_in)
{
    VersionId v;
    v.text = id_in;
    const std::string id = trim(id_in);
    // A snapshot: 25w04a.
    if (id.size() == 6 && std::isdigit((unsigned char) id[0]) && std::isdigit((unsigned char) id[1]) && id[2] == 'w' &&
        std::isdigit((unsigned char) id[3]) && std::isdigit((unsigned char) id[4]) && std::isalpha((unsigned char) id[5])) {
        v.kind   = VersionId::Kind::Snapshot;
        v.year   = 2000 + to_int(id.substr(0, 2));
        v.week   = to_int(id.substr(3, 2));
        v.letter = id[5];
        return v;
    }
    if (id.size() > 1 && (id[0] == 'b' || id[0] == 'a') && std::isdigit((unsigned char) id[1])) {
        v.kind = id[0] == 'b' ? VersionId::Kind::OldBeta : VersionId::Kind::OldAlpha;
        parse_release_numbers(id.substr(1), v.major, v.minor, v.patch);
        return v;
    }
    // 1.21.4, 1.21-pre1, 1.21.4-rc2, and the older "1.14 Pre-Release 3".
    std::string base = id, suffix;
    if (const std::size_t pre = id.find(" Pre-Release "); pre != std::string::npos) {
        base   = id.substr(0, pre);
        suffix = "pre" + id.substr(pre + 13);
    } else if (const std::size_t dash = id.find('-'); dash != std::string::npos) {
        base   = id.substr(0, dash);
        suffix = id.substr(dash + 1);
    }
    if (!parse_release_numbers(base, v.major, v.minor, v.patch)) return v;
    if (suffix.empty()) {
        v.kind = VersionId::Kind::Release;
    } else if (starts_with(suffix, "pre") && all_digits(suffix.substr(3))) {
        v.kind   = VersionId::Kind::PreRelease;
        v.number = to_int(suffix.substr(3));
    } else if (starts_with(suffix, "rc") && all_digits(suffix.substr(2))) {
        v.kind   = VersionId::Kind::ReleaseCandidate;
        v.number = to_int(suffix.substr(2));
    }
    return v;
}

int compare_releases(const std::string &a, const std::string &b)
{
    int a1 = 0, a2 = 0, a3 = 0, b1 = 0, b2 = 0, b3 = 0;
    parse_release_numbers(a, a1, a2, a3);
    parse_release_numbers(b, b1, b2, b3);
    if (a1 != b1) return a1 < b1 ? -1 : 1;
    if (a2 != b2) return a2 < b2 ? -1 : 1;
    if (a3 != b3) return a3 < b3 ? -1 : 1;
    return 0;
}

bool at_least(const ServerVersion &v, const std::string &release)
{
    if (release.empty()) return true;
    if (const ReleaseFact *fact = release_fact(release); fact && v.data_version > 0)
        return v.data_version >= fact->data_version;
    const VersionId id = parse_version(v.id);
    switch (id.kind) {
    case VersionId::Kind::Release:
    case VersionId::Kind::PreRelease:
    case VersionId::Kind::ReleaseCandidate: {
        const std::string own = std::to_string(id.major) + "." + std::to_string(id.minor) + (id.patch ? "." + std::to_string(id.patch) : "");
        return compare_releases(own, release) >= 0;
    }
    case VersionId::Kind::Snapshot: {
        const std::string date   = v.release_time.size() >= 10 ? v.release_time.substr(0, 10) : snapshot_date(id.year, id.week);
        const std::string target = release_after(date);
        return target == "99" || compare_releases(target, release) >= 0;
    }
    default: return false;
    }
}

bool at_least(const std::string &id, const std::string &release)
{
    ServerVersion v;
    v.id = id;
    return at_least(v, release);
}

int java_needed(const ServerVersion &v, int recorded)
{
    if (recorded > 0) return recorded;
    if (at_least(v, "1.20.5")) return 21;
    if (at_least(v, "1.18")) return 17;
    if (at_least(v, "1.17")) return 16;
    return 8;
}

int java_feature_version(const std::string &text)
{
    std::size_t i = 0;
    while (i < text.size() && !std::isdigit((unsigned char) text[i])) ++i;
    std::string first;
    while (i < text.size() && std::isdigit((unsigned char) text[i])) first += text[i++];
    if (first.empty()) return 0;
    if (first == "1" && i < text.size() && text[i] == '.') {
        std::string second;
        ++i;
        while (i < text.size() && std::isdigit((unsigned char) text[i])) second += text[i++];
        return to_int(second);
    }
    return to_int(first);
}

std::string java_version_from_output(const std::string &output)
{
    const std::size_t at = output.find("version \"");
    if (at == std::string::npos) return "";
    const std::size_t start = at + 9, end = output.find('"', start);
    return end == std::string::npos ? "" : output.substr(start, end - start);
}

// ---------------------------------------------------------------------------
// server.properties

namespace {

void append_utf8(std::string &out, std::uint32_t cp)
{
    if (cp < 0x80) out += char(cp);
    else if (cp < 0x800) { out += char(0xC0 | (cp >> 6)); out += char(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { out += char(0xE0 | (cp >> 12)); out += char(0x80 | ((cp >> 6) & 0x3F)); out += char(0x80 | (cp & 0x3F)); }
    else { out += char(0xF0 | (cp >> 18)); out += char(0x80 | ((cp >> 12) & 0x3F)); out += char(0x80 | ((cp >> 6) & 0x3F)); out += char(0x80 | (cp & 0x3F)); }
}

// The next code point of UTF-8 text (an invalid byte reads as itself).
std::uint32_t next_code_point(const std::string &s, std::size_t &i)
{
    const unsigned char c     = (unsigned char) s[i];
    const int           extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    if (extra == 0 || i + extra >= s.size()) { ++i; return c; }
    std::uint32_t cp = c & (0x3F >> extra);
    for (int k = 1; k <= extra; ++k) {
        const unsigned char d = (unsigned char) s[i + k];
        if ((d & 0xC0) != 0x80) { ++i; return c; }
        cp = (cp << 6) | (d & 0x3F);
    }
    i += extra + 1;
    return cp;
}

int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// The line, logically joined: a line ending in an odd number of backslashes continues.
bool continues(const std::string &line)
{
    std::size_t n = 0;
    for (std::size_t i = line.size(); i > 0 && line[i - 1] == '\\'; --i) ++n;
    return n % 2 == 1;
}

PropertyLine parse_line(const std::string &raw, const std::string &logical)
{
    PropertyLine line;
    line.raw = raw;
    std::size_t i = 0;
    while (i < logical.size() && (logical[i] == ' ' || logical[i] == '\t' || logical[i] == '\f')) ++i;
    if (i == logical.size()) { line.kind = PropertyLine::Kind::Blank; return line; }
    if (logical[i] == '#' || logical[i] == '!') { line.kind = PropertyLine::Kind::Comment; return line; }
    line.kind = PropertyLine::Kind::Entry;
    std::string key;
    bool        escaped = false;
    for (; i < logical.size(); ++i) {
        const char c = logical[i];
        if (escaped) { key += '\\'; key += c; escaped = false; continue; }
        if (c == '\\') { escaped = true; continue; }
        if (c == '=' || c == ':' || c == ' ' || c == '\t' || c == '\f') break;
        key += c;
    }
    while (i < logical.size() && (logical[i] == ' ' || logical[i] == '\t' || logical[i] == '\f')) ++i;
    if (i < logical.size() && (logical[i] == '=' || logical[i] == ':')) {
        ++i;
        while (i < logical.size() && (logical[i] == ' ' || logical[i] == '\t' || logical[i] == '\f')) ++i;
    }
    line.key   = unescape_property(key);
    line.value = unescape_property(logical.substr(i));
    return line;
}

} // namespace

std::string unescape_property(const std::string &text)
{
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c != '\\' || i + 1 >= text.size()) { out += c; continue; }
        const char n = text[++i];
        switch (n) {
        case 't': out += '\t'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 'f': out += '\f'; break;
        case 'u': {
            std::uint32_t cp = 0;
            bool          ok = i + 4 < text.size(); // four hex digits follow the u
            for (int k = 1; ok && k <= 4; ++k) {
                const int h = hex_value(text[i + k]);
                if (h < 0) ok = false;
                else cp = (cp << 4) | std::uint32_t(h);
            }
            if (!ok) { out += 'u'; break; }
            i += 4;
            // A high surrogate followed by \uDC00..\uDFFF is one code point.
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 < text.size() && text[i + 1] == '\\' && text[i + 2] == 'u') {
                std::uint32_t low = 0;
                bool          low_ok = true;
                for (int k = 3; k <= 6; ++k) {
                    const int h = hex_value(text[i + k]);
                    if (h < 0) low_ok = false;
                    else low = (low << 4) | std::uint32_t(h);
                }
                if (low_ok && low >= 0xDC00 && low <= 0xDFFF) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    i += 6;
                }
            }
            append_utf8(out, cp);
            break;
        }
        default: out += n;
        }
    }
    return out;
}

std::string escape_property(const std::string &text, bool is_key)
{
    std::string out;
    char        buf[16];
    for (std::size_t i = 0; i < text.size();) {
        const std::size_t   at = i;
        const std::uint32_t cp = next_code_point(text, i);
        switch (cp) {
        case '\\': out += "\\\\"; continue;
        case '\n': out += "\\n"; continue;
        case '\r': out += "\\r"; continue;
        case '\t': out += "\\t"; continue;
        case '\f': out += "\\f"; continue;
        case '=': case ':': case '#': case '!': out += '\\'; out += char(cp); continue;
        case ' ':
            if (is_key || at == 0) out += "\\ ";
            else out += ' ';
            continue;
        default: break;
        }
        if (cp < 0x20 || cp > 0x7E) {
            // Every version reads \u escapes, whatever encoding it reads the file in.
            if (cp > 0xFFFF) {
                const std::uint32_t v = cp - 0x10000;
                std::snprintf(buf, sizeof(buf), "\\u%04X", 0xD800 + (v >> 10));
                out += buf;
                std::snprintf(buf, sizeof(buf), "\\u%04X", 0xDC00 + (v & 0x3FF));
                out += buf;
            } else {
                std::snprintf(buf, sizeof(buf), "\\u%04X", cp);
                out += buf;
            }
            continue;
        }
        out += char(cp);
    }
    return out;
}

PropertiesFile parse_properties(const std::string &text)
{
    PropertiesFile file;
    if (text.find("\r\n") != std::string::npos) file.newline = "\r\n";
    std::vector<std::string> physical = split(text, '\n');
    for (std::string &l : physical)
        if (!l.empty() && l.back() == '\r') l.pop_back();
    for (std::size_t i = 0; i < physical.size(); ++i) {
        std::string raw = physical[i], logical = physical[i];
        // A comment never continues; an entry may, onto the next lines.
        const std::string head = trim(logical);
        const bool        comment = !head.empty() && (head[0] == '#' || head[0] == '!');
        while (!comment && continues(logical) && i + 1 < physical.size()) {
            logical.pop_back();
            std::string next = physical[++i];
            raw += "\n" + next;
            std::size_t k = 0;
            while (k < next.size() && (next[k] == ' ' || next[k] == '\t' || next[k] == '\f')) ++k;
            logical += next.substr(k);
        }
        file.lines.push_back(parse_line(raw, logical));
    }
    // split() keeps no empty tail for a final line break, so nothing extra to drop.
    return file;
}

std::string write_properties(const PropertiesFile &file)
{
    std::string out;
    for (const PropertyLine &line : file.lines) {
#ifndef SLIC3R_GAME_SERVERS_NEGATIVE_FIXTURE
        std::string raw = line.raw;
#else
        // Re-encodes every entry, which loses how another tool wrote it.
        std::string raw = line.kind == PropertyLine::Kind::Entry ? escape_property(line.key, true) + "=" + escape_property(line.value, false) : line.raw;
#endif
        if (file.newline != "\n") {
            std::string converted;
            for (char c : raw) {
                if (c == '\n') converted += file.newline;
                else converted += c;
            }
            raw = converted;
        }
        out += raw + file.newline;
    }
    return out;
}

std::optional<std::string> PropertiesFile::get(const std::string &key) const
{
    // The server reads the last occurrence of a key.
    for (auto it = lines.rbegin(); it != lines.rend(); ++it)
        if (it->kind == PropertyLine::Kind::Entry && it->key == key) return it->value;
    return std::nullopt;
}

void PropertiesFile::set(const std::string &key, const std::string &value)
{
    for (auto it = lines.rbegin(); it != lines.rend(); ++it)
        if (it->kind == PropertyLine::Kind::Entry && it->key == key) {
            if (it->value != value) {
                it->value = value;
                it->raw   = escape_property(key, true) + "=" + escape_property(value, false);
            }
            return;
        }
    PropertyLine line;
    line.kind  = PropertyLine::Kind::Entry;
    line.key   = key;
    line.value = value;
    line.raw   = escape_property(key, true) + "=" + escape_property(value, false);
    lines.push_back(line);
}

std::vector<std::string> PropertiesFile::keys() const
{
    std::vector<std::string> out;
    for (const PropertyLine &l : lines)
        if (l.kind == PropertyLine::Kind::Entry && std::find(out.begin(), out.end(), l.key) == out.end()) out.push_back(l.key);
    return out;
}

namespace {

// A setting, built up one fact at a time.
struct Spec : PropertySpec
{
    Spec(const char *k, const char *l, const char *h, PropertyType t, PropertyGroup g, const char *d)
    {
        key   = k;
        label = l;
        help  = h;
        type  = t;
        group = g;
        def   = d;
    }
    Spec &range(long long a, long long b) { min = a; max = b; return *this; }
    Spec &one_of(std::vector<std::string> v) { values = std::move(v); return *this; }
    Spec &from(const char *release) { since = release; return *this; }
    Spec &to(const char *release) { until = release; return *this; }
    Spec &live(const char *command) { live_command = command; return *this; }
};

} // namespace

const std::vector<PropertySpec> &property_specs()
{
    using T = PropertyType;
    using G = PropertyGroup;
    const long long big = 2147483647LL;
    static const std::vector<PropertySpec> specs = {
        // Gameplay
        Spec("difficulty", L("Difficulty"), L("How hard the world is: hunger, damage and which mobs spawn."), T::Enum, G::Gameplay, "easy").one_of({"peaceful", "easy", "normal", "hard"}).live("difficulty {}"),
        Spec("gamemode", L("Game mode"), L("The mode new players start in."), T::Enum, G::Gameplay, "survival").one_of({"survival", "creative", "adventure", "spectator"}).live("defaultgamemode {}"),
        Spec("force-gamemode", L("Force game mode"), L("Puts every player back in the game mode above each time they join."), T::Bool, G::Gameplay, "false"),
        Spec("hardcore", L("Hardcore"), L("One life: a player who dies becomes a spectator, and the difficulty stays hard."), T::Bool, G::Gameplay, "false"),
        Spec("pvp", L("Player versus player"), L("Whether players can hurt each other."), T::Bool, G::Gameplay, "true"),
        Spec("allow-flight", L("Allow flight"), L("Lets clients with a flying mod stay connected instead of being kicked for flying."), T::Bool, G::Gameplay, "false"),
        Spec("allow-nether", L("Allow the Nether"), L("Whether players can travel to the Nether."), T::Bool, G::Gameplay, "true"),
        Spec("spawn-monsters", L("Spawn monsters"), L("Whether hostile mobs spawn."), T::Bool, G::Gameplay, "true"),
        Spec("spawn-animals", L("Spawn animals"), L("Whether animals spawn."), T::Bool, G::Gameplay, "true").to("1.21.2"),
        Spec("spawn-npcs", L("Spawn villagers"), L("Whether villagers spawn."), T::Bool, G::Gameplay, "true").to("1.21.2"),
        Spec("enable-command-block", L("Command blocks"), L("Whether command blocks run their commands."), T::Bool, G::Gameplay, "false"),
        Spec("spawn-protection", L("Spawn protection"), L("Blocks around spawn that only operators can change; 0 turns it off."), T::Int, G::Gameplay, "16").range(0, 29999984),
        Spec("player-idle-timeout", L("Idle timeout"), L("Minutes before an idle player is kicked; 0 never kicks."), T::Int, G::Gameplay, "0").range(0, 100000),
        Spec("pause-when-empty-seconds", L("Pause when empty"), L("Seconds with nobody online before the server pauses; 0 never pauses."), T::Int, G::Gameplay, "60").range(0, 86400).from("1.21.2"),
        // World
        Spec("level-name", L("World folder"), L("The folder of the world the server runs."), T::Text, G::World, "world"),
        Spec("level-seed", L("Seed"), L("The seed a new world is made from; empty picks one at random."), T::Text, G::World, ""),
        Spec("level-type", L("World type"), L("How a new world is shaped: normal, flat, large biomes, amplified or a single biome."), T::Text, G::World, "minecraft:normal"),
        Spec("generator-settings", L("Generator settings"), L("Extra settings for the world type, such as flat layers."), T::Text, G::World, "{}"),
        Spec("generate-structures", L("Generate structures"), L("Whether villages, temples and other structures are made in new chunks."), T::Bool, G::World, "true"),
        Spec("max-world-size", L("World size limit"), L("The largest radius, in blocks, the world border can reach."), T::Int, G::World, "29999984").range(1, 29999984),
        Spec("initial-enabled-packs", L("Data packs on"), L("Data packs a new world starts with, separated by commas."), T::Text, G::World, "vanilla").from("1.19.3"),
        Spec("initial-disabled-packs", L("Data packs off"), L("Data packs a new world leaves out, separated by commas."), T::Text, G::World, "").from("1.19.3"),
        Spec("region-file-compression", L("Region compression"), L("How world files are compressed."), T::Enum, G::World, "deflate").one_of({"deflate", "lz4", "none"}).from("1.20.5"),
        // Players
        Spec("max-players", L("Max players"), L("How many players can be online at once."), T::Int, G::Players, "20").range(0, 100000),
        Spec("white-list", L("Allowlist only"), L("Only players on the allowlist can join."), T::Bool, G::Players, "false").live("whitelist {onoff}"),
        Spec("enforce-whitelist", L("Enforce the allowlist"), L("Kicks players who are online but not on the allowlist when it is reloaded."), T::Bool, G::Players, "false"),
        Spec("online-mode", L("Online mode"), L("Checks each player's account with Mojang; off lets anyone use any name."), T::Bool, G::Players, "true"),
        Spec("op-permission-level", L("Operator level"), L("The level /op gives a new operator, 0 to 4."), T::Int, G::Players, "4").range(0, 4),
        Spec("function-permission-level", L("Function level"), L("The permission level functions run with, 1 to 4."), T::Int, G::Players, "2").range(1, 4),
        Spec("hide-online-players", L("Hide who is online"), L("Leaves player names out of the server list."), T::Bool, G::Players, "false").from("1.18"),
        Spec("enforce-secure-profile", L("Require signed chat"), L("Only players with a Mojang-signed profile key can join."), T::Bool, G::Players, "true").from("1.19"),
        Spec("prevent-proxy-connections", L("Block proxies"), L("Kicks players whose connection does not come from where their account says."), T::Bool, G::Players, "false"),
        Spec("log-ips", L("Log addresses"), L("Whether player IP addresses are written to the log."), T::Bool, G::Players, "true").from("1.20.2"),
        Spec("broadcast-console-to-ops", L("Console output to operators"), L("Shows console command output to online operators."), T::Bool, G::Players, "true"),
        Spec("broadcast-rcon-to-ops", L("Remote console output to operators"), L("Shows remote console command output to online operators."), T::Bool, G::Players, "true"),
        // Network
        Spec("server-port", L("Port"), L("The port players connect to."), T::Port, G::Network, "25565").range(1, 65535),
        Spec("server-ip", L("Address to listen on"), L("Leave empty to listen on every address of this PC."), T::Text, G::Network, ""),
        Spec("motd", L("Message of the day"), L("The line under the server's name in the server list; colour codes start with \xC2\xA7."), T::Text, G::Network, "A Minecraft Server"),
        Spec("enable-status", L("Show in the server list"), L("Whether the server answers server list pings."), T::Bool, G::Network, "true"),
        Spec("enable-query", L("Query protocol"), L("Answers GameSpy4 queries on the query port."), T::Bool, G::Network, "false"),
        Spec("query.port", L("Query port"), L("The port the query protocol listens on."), T::Port, G::Network, "25565").range(1, 65535),
        Spec("enable-rcon", L("Remote console"), L("Lets tools send commands over the network with the password below."), T::Bool, G::Network, "false"),
        Spec("rcon.port", L("Remote console port"), L("The port the remote console listens on."), T::Port, G::Network, "25575").range(1, 65535),
        Spec("rcon.password", L("Remote console password"), L("The password the remote console asks for. It is kept in this file in plain text."), T::Password, G::Network, ""),
        Spec("network-compression-threshold", L("Compression threshold"), L("Packets larger than this many bytes are compressed; -1 turns compression off."), T::Int, G::Network, "256").range(-1, 1048576),
        Spec("rate-limit", L("Packet rate limit"), L("Packets a client may send each second before it is kicked; 0 has no limit."), T::Int, G::Network, "0").range(0, 1000000),
        Spec("use-native-transport", L("Native networking"), L("Uses Linux's faster network transport where it can."), T::Bool, G::Network, "true"),
        Spec("accepts-transfers", L("Accept transfers"), L("Lets players arrive from another server with /transfer."), T::Bool, G::Network, "false").from("1.20.5"),
        Spec("bug-report-link", L("Bug report link"), L("A link the game shows players for reporting problems with this server."), T::Text, G::Network, "").from("1.21"),
        // Performance
        Spec("view-distance", L("View distance"), L("How many chunks around each player are sent to them, 3 to 32."), T::Int, G::Performance, "10").range(3, 32),
        Spec("simulation-distance", L("Simulation distance"), L("How many chunks around each player keep running, 3 to 32."), T::Int, G::Performance, "10").range(3, 32).from("1.18"),
        Spec("entity-broadcast-range-percentage", L("Entity range"), L("How far away entities are sent to players, as a percentage."), T::Int, G::Performance, "100").range(10, 1000),
        Spec("max-tick-time", L("Watchdog"), L("Milliseconds a single tick may take before the server stops itself; -1 turns this off."), T::Int, G::Performance, "60000").range(-1, big),
        Spec("sync-chunk-writes", L("Write chunks at once"), L("Writes each chunk before going on; safer, slower."), T::Bool, G::Performance, "true"),
        Spec("max-chained-neighbor-updates", L("Chained updates"), L("How many block updates may chain before the rest are skipped."), T::Int, G::Performance, "1000000").range(-1, big).from("1.19"),
        Spec("enable-jmx-monitoring", L("JMX monitoring"), L("Exposes tick times to Java monitoring tools."), T::Bool, G::Performance, "false"),
        // Packs
        Spec("resource-pack", L("Resource pack link"), L("A link to a resource pack players are offered."), T::Text, G::Packs, ""),
        Spec("resource-pack-sha1", L("Resource pack SHA-1"), L("The pack's SHA-1, so players' games can check it."), T::Text, G::Packs, ""),
        Spec("resource-pack-id", L("Resource pack id"), L("A UUID that names the pack for players' games."), T::Text, G::Packs, "").from("1.20.3"),
        Spec("resource-pack-prompt", L("Resource pack message"), L("What players read when they are offered the pack."), T::Text, G::Packs, "").from("1.17"),
        Spec("require-resource-pack", L("Require the pack"), L("Players who decline the pack are disconnected."), T::Bool, G::Packs, "false").from("1.17"),
        // Advanced
        Spec("text-filtering-config", L("Text filtering"), L("Settings for a chat filtering service."), T::Text, G::Advanced, ""),
    };
    return specs;
}

const PropertySpec *find_property(const std::string &key)
{
    for (const PropertySpec &s : property_specs())
        if (s.key == key) return &s;
    return nullptr;
}

bool property_known_in(const PropertySpec &spec, const ServerVersion &v)
{
    if (v.id.empty()) return true;
    if (!spec.since.empty() && !at_least(v, spec.since)) return false;
    if (!spec.until.empty() && at_least(v, spec.until)) return false;
    return true;
}

std::string normalise_enum(const PropertySpec &spec, const std::string &value)
{
    if (spec.type != PropertyType::Enum) return value;
    const std::string v = lower(trim(value));
    if (all_digits(v) && (spec.key == "difficulty" || spec.key == "gamemode")) {
        const int n = to_int(v, -1);
        if (n >= 0 && n < int(spec.values.size())) return spec.values[std::size_t(n)];
    }
    return v;
}

Message property_problem(const PropertySpec &spec, const std::string &value)
{
    switch (spec.type) {
    case PropertyType::Bool:
        return value == "true" || value == "false" ? Message() : Message{L("Use true or false."), {}};
    case PropertyType::Int:
    case PropertyType::Port: {
        std::string v = trim(value);
        const bool  negative = !v.empty() && v[0] == '-';
        if (negative) v = v.substr(1);
        if (!all_digits(v) || v.size() > 12) return {L("Use a whole number."), {}};
        const long long n = (negative ? -1 : 1) * std::stoll(v);
        if (n < spec.min || n > spec.max) return {L("Use {1} to {2}."), {std::to_string(spec.min), std::to_string(spec.max)}};
        return {};
    }
    case PropertyType::Enum: {
        const std::string v = normalise_enum(spec, value);
        return std::find(spec.values.begin(), spec.values.end(), v) != spec.values.end() ? Message() : Message{L("Not one of this setting's values."), {}};
    }
    default: return {};
    }
}

std::string live_command_for(const PropertySpec &spec, const std::string &value)
{
    if (spec.live_command.empty() || !property_problem(spec, value).empty()) return "";
    std::string cmd = spec.live_command;
    if (const std::size_t at = cmd.find("{onoff}"); at != std::string::npos) cmd.replace(at, 7, value == "true" ? "on" : "off");
    if (const std::size_t at = cmd.find("{}"); at != std::string::npos) cmd.replace(at, 2, normalise_enum(spec, value));
    return cmd;
}

// ---------------------------------------------------------------------------
// The server folder's own files

Eula parse_eula(const std::string &text)
{
    Eula e;
    e.present = !text.empty();
    for (const std::string &raw : split(text, '\n')) {
        const std::string l = trim(raw);
        if (l.empty() || l[0] == '#') continue;
        const std::size_t eq = l.find('=');
        if (eq == std::string::npos) continue;
        if (lower(trim(l.substr(0, eq))) == "eula") e.accepted = lower(trim(l.substr(eq + 1))) == "true";
    }
    return e;
}

std::string eula_text(bool accepted, const std::string &when)
{
    return "#By changing the setting below to TRUE you are indicating your agreement to our EULA (https://aka.ms/MinecraftEULA).\n#" + when +
           "\neula=" + (accepted ? "true" : "false") + "\n";
}

bool parse_players(const std::string &text, std::vector<Player> &out)
{
    out.clear();
    const json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_array()) return false;
    for (const json &e : j) {
        if (!e.is_object()) continue;
        Player p;
        p.uuid    = e.value("uuid", std::string());
        p.name    = e.value("name", std::string());
        p.level   = e.contains("level") && e["level"].is_number_integer() ? e["level"].get<int>() : 0;
        p.bypass  = e.contains("bypassesPlayerLimit") && e["bypassesPlayerLimit"].is_boolean() && e["bypassesPlayerLimit"].get<bool>();
        p.created = e.value("created", std::string());
        p.source  = e.value("source", std::string());
        p.expires = e.value("expires", std::string());
        p.reason  = e.value("reason", std::string());
        out.push_back(p);
    }
    return true;
}

std::string write_allowlist(const std::vector<Player> &players)
{
    json j = json::array();
    for (const Player &p : players) j.push_back(json::object({{"uuid", p.uuid}, {"name", p.name}}));
    return j.dump(2);
}

std::string write_operators(const std::vector<Player> &players)
{
    json j = json::array();
    for (const Player &p : players)
        j.push_back(json::object({{"uuid", p.uuid}, {"name", p.name}, {"level", std::clamp(p.level, 1, 4)}, {"bypassesPlayerLimit", p.bypass}}));
    return j.dump(2);
}

std::string write_bans(const std::vector<Player> &players)
{
    json j = json::array();
    for (const Player &p : players)
        j.push_back(json::object({{"uuid", p.uuid}, {"name", p.name}, {"created", p.created}, {"source", p.source},
                                  {"expires", p.expires.empty() ? std::string("forever") : p.expires}, {"reason", p.reason}}));
    return j.dump(2);
}

bool valid_player_name(const std::string &name)
{
    if (name.size() < 3 || name.size() > 16) return false;
    return std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isalnum(c) != 0 || c == '_'; });
}

const char *operator_level_meaning(int level)
{
    switch (level) {
    case 1: return L("Can build inside spawn protection.");
    case 2: return L("Also cheat commands and command blocks.");
    case 3: return L("Also kick, ban and make other operators.");
    case 4: return L("Can stop the server and change every setting.");
    default: return L("Not an operator.");
    }
}

const char *flavour_name(Flavour f)
{
    switch (f) {
    case Flavour::Vanilla: return "Vanilla";
    case Flavour::Paper: return "Paper";
    case Flavour::Purpur: return "Purpur";
    case Flavour::Spigot: return "Spigot";
    case Flavour::Fabric: return "Fabric";
    case Flavour::Forge: return "Forge";
    case Flavour::NeoForge: return "NeoForge";
    case Flavour::Velocity: return "Velocity";
    default: return "Unknown";
    }
}

namespace {

// The first "1.21.4"-like release id inside a text.
std::string release_in(const std::string &s)
{
    for (std::size_t i = 0; i + 2 < s.size(); ++i) {
        if (s[i] != '1' || s[i + 1] != '.' || !std::isdigit((unsigned char) s[i + 2])) continue;
        // Not the tail of a longer number ("21.4", "0.16.9").
        if (i > 0 && std::isdigit((unsigned char) s[i - 1])) continue;
        if (i > 1 && s[i - 1] == '.' && std::isdigit((unsigned char) s[i - 2])) continue;
        std::size_t j = i + 2;
        while (j < s.size() && (std::isdigit((unsigned char) s[j]) || (s[j] == '.' && j + 1 < s.size() && std::isdigit((unsigned char) s[j + 1])))) ++j;
        return s.substr(i, j - i);
    }
    return "";
}

std::string manifest_value(const std::string &manifest, const std::string &key)
{
    for (const std::string &raw : split(manifest, '\n')) {
        std::string l = raw;
        if (!l.empty() && l.back() == '\r') l.pop_back();
        if (starts_with(l, key + ":")) return trim(l.substr(key.size() + 1));
    }
    return "";
}

} // namespace

JarInfo identify_jar(const std::string &file_name, const std::string &version_json, const std::string &manifest)
{
    JarInfo           info;
    const std::string name = lower(file_name);
    const std::string main = manifest_value(manifest, "Main-Class");
    const std::string title = lower(manifest_value(manifest, "Implementation-Title"));

    if (starts_with(name, "paper-") || title.find("paper") != std::string::npos) info.flavour = Flavour::Paper;
    else if (starts_with(name, "purpur-") || title.find("purpur") != std::string::npos) info.flavour = Flavour::Purpur;
    else if (starts_with(name, "spigot-") || main.find("org.bukkit") == 0) info.flavour = Flavour::Spigot;
    else if (name.find("fabric-server") != std::string::npos || main.find("net.fabricmc") == 0) info.flavour = Flavour::Fabric;
    else if (starts_with(name, "neoforge-") || main.find("net.neoforged") == 0) info.flavour = Flavour::NeoForge;
    else if (starts_with(name, "forge-") || main.find("net.minecraftforge") == 0) info.flavour = Flavour::Forge;
    else if (starts_with(name, "velocity-") || main.find("com.velocitypowered") == 0) info.flavour = Flavour::Velocity;
    else if (main.find("net.minecraft.") == 0 || name == "server.jar" || starts_with(name, "minecraft_server")) info.flavour = Flavour::Vanilla;

    const json v = json::parse(version_json, nullptr, false);
    if (!v.is_discarded() && v.is_object()) {
        info.version = v.value("id", v.value("name", std::string()));
        if (v.contains("world_version") && v["world_version"].is_number_integer()) info.data_version = v["world_version"].get<int>();
        if (v.contains("java_version") && v["java_version"].is_number_integer()) info.java_version = v["java_version"].get<int>();
        if (info.flavour == Flavour::Unknown) info.flavour = Flavour::Vanilla;
    }
    if (info.version.empty() && info.flavour != Flavour::Velocity) {
        if (info.flavour == Flavour::NeoForge) {
            // neoforge-21.4.88: Minecraft 1.21.4.
            std::string rest = name.substr(std::min<std::size_t>(9, name.size()));
            int a = 0, b = 0, c = 0;
            const std::vector<std::string> parts = split(rest, '.');
            if (parts.size() >= 2 && all_digits(parts[0]) && all_digits(parts[1])) {
                a = to_int(parts[0]);
                b = to_int(parts[1]);
                (void) c;
                info.version = "1." + std::to_string(a) + (b ? "." + std::to_string(b) : "");
            }
        } else {
            info.version = release_in(name);
        }
    }
    if (info.flavour == Flavour::Paper || info.flavour == Flavour::Purpur) {
        // paper-1.21.4-232.jar: the build after the version.
        const std::size_t at = name.rfind('-'), dot = name.rfind(".jar");
        if (at != std::string::npos && dot != std::string::npos && dot > at + 1 && all_digits(name.substr(at + 1, dot - at - 1)))
            info.build = name.substr(at + 1, dot - at - 1);
    }
    return info;
}

std::string pick_server_jar(const std::vector<std::string> &names)
{
    std::vector<std::string> jars;
    for (const std::string &n : names) {
        const std::string l = lower(n);
        if (ends_with(l, ".jar") && l.find("installer") == std::string::npos) jars.push_back(n);
    }
    static const char *prefer[] = {"server.jar", "minecraft_server", "paper-", "purpur-", "spigot-", "fabric-server", "neoforge-", "forge-", "velocity-"};
    for (const char *p : prefer)
        for (const std::string &j : jars)
            if (lower(j) == p || starts_with(lower(j), p)) return j;
    return jars.size() == 1 ? jars.front() : "";
}

// ---------------------------------------------------------------------------
// The console

LogEvent parse_log_line(const std::string &raw)
{
    LogEvent    e;
    std::string line = raw;
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    std::string text = line;
    if (starts_with(line, "[")) {
        const std::size_t close = line.find(']');
        if (close != std::string::npos) {
            const std::string inside = line.substr(1, close - 1);
            std::size_t       rest   = close + 1;
            if (const std::size_t sp = inside.find(' '); sp != std::string::npos) {
                e.time  = inside.substr(0, sp);
                e.level = inside.substr(sp + 1);
            } else {
                e.time = inside;
                if (line.compare(rest, 2, " [") == 0) {
                    const std::size_t end = line.find(']', rest + 2);
                    if (end != std::string::npos) {
                        const std::string thread = line.substr(rest + 2, end - rest - 2);
                        const std::size_t slash  = thread.rfind('/');
                        e.level                  = slash == std::string::npos ? thread : thread.substr(slash + 1);
                        rest                     = end + 1;
                    }
                }
            }
            const std::size_t colon = line.find(": ", rest);
            text                    = colon == std::string::npos ? trim(line.substr(rest)) : line.substr(colon + 2);
        }
    }
    e.text = text;
    const std::string lt = lower(text);
    if (starts_with(text, "Done (") && text.find(")! For help") != std::string::npos) {
        e.kind = LogEvent::Kind::Started;
        const std::string n = text.substr(6, text.find(')') - 6);
        try { e.seconds = std::stod(n); } catch (...) { e.seconds = 0; }
    } else if (ends_with(text, " joined the game")) {
        e.kind   = LogEvent::Kind::Joined;
        e.player = text.substr(0, text.size() - 16);
    } else if (ends_with(text, " left the game")) {
        e.kind   = LogEvent::Kind::Left;
        e.player = text.substr(0, text.size() - 14);
    } else if (starts_with(text, "Saved the game") || starts_with(text, "Saved the world")) {
        e.kind = LogEvent::Kind::Saved;
    } else if (starts_with(text, "Stopping server") || starts_with(text, "Stopping the server")) {
        e.kind = LogEvent::Kind::Stopping;
    } else if (starts_with(text, "There are ") && text.find(" players online") != std::string::npos) {
        e.kind = LogEvent::Kind::PlayerList;
        // "There are 2 of a max of 20 players online: Mei, Kit" or the older "There are 2/20 players online:".
        std::string       numbers = text.substr(10, text.find(" players online") - 10);
        const std::size_t of      = numbers.find(" of a max of ");
        if (of != std::string::npos) {
            e.count = to_int(numbers.substr(0, of));
            e.max   = to_int(numbers.substr(of + 13));
        } else if (const std::size_t slash = numbers.find('/'); slash != std::string::npos) {
            e.count = to_int(numbers.substr(0, slash));
            e.max   = to_int(numbers.substr(slash + 1));
        }
        if (const std::size_t colon = text.find(':'); colon != std::string::npos)
            for (const std::string &p : split(text.substr(colon + 1), ',')) {
                const std::string name = trim(p);
                if (!name.empty()) e.players.push_back(name);
            }
    } else if (starts_with(text, "Test passed")) {
        e.kind = LogEvent::Kind::TestPassed;
    } else if (starts_with(text, "Test failed")) {
        e.kind = LogEvent::Kind::TestFailed;
    } else if (lt.find("force loaded") != std::string::npos) {
        e.kind = LogEvent::Kind::ForceLoaded;
    } else if (lt.find("failed to bind to port") != std::string::npos) {
        e.kind = LogEvent::Kind::PortInUse;
    } else if (lt.find("you need to agree to the eula") != std::string::npos) {
        e.kind = LogEvent::Kind::EulaNeeded;
    } else if (starts_with(text, "<") && text.find("> ") != std::string::npos) {
        e.kind   = LogEvent::Kind::Chat;
        e.player = text.substr(1, text.find("> ") - 1);
    } else if (text.find(" has made the advancement [") != std::string::npos || text.find(" has completed the challenge [") != std::string::npos ||
               text.find(" has reached the goal [") != std::string::npos) {
        e.kind   = LogEvent::Kind::Advancement;
        e.player = text.substr(0, text.find(" has "));
    } else if (e.level == "WARN") {
        e.kind = LogEvent::Kind::Warning;
    } else if (e.level == "ERROR" || e.level == "FATAL") {
        e.kind = LogEvent::Kind::Error;
    }
    return e;
}

std::vector<std::string> take_lines(std::string &pending, const std::string &chunk)
{
    pending += chunk;
    std::vector<std::string> lines;
    std::size_t              start = 0;
    for (std::size_t nl = pending.find('\n'); nl != std::string::npos; nl = pending.find('\n', start)) {
        std::string l = pending.substr(start, nl - start);
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
        start = nl + 1;
    }
    pending.erase(0, start);
    // A runaway line without a break is passed on rather than held forever.
    if (pending.size() > 64 * 1024) {
        lines.push_back(pending);
        pending.clear();
    }
    return lines;
}

// ---------------------------------------------------------------------------
// The Mojang version list

bool parse_manifest(const std::string &text, Manifest &out)
{
    out = Manifest();
    const json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object() || !j.contains("versions") || !j["versions"].is_array()) return false;
    if (j.contains("latest") && j["latest"].is_object()) {
        out.latest_release  = j["latest"].value("release", std::string());
        out.latest_snapshot = j["latest"].value("snapshot", std::string());
    }
    for (const json &v : j["versions"]) {
        if (!v.is_object()) continue;
        ManifestVersion m;
        m.id           = v.value("id", std::string());
        m.type         = v.value("type", std::string());
        m.url          = v.value("url", std::string());
        m.sha1         = v.value("sha1", std::string());
        m.time         = v.value("time", std::string());
        m.release_time = v.value("releaseTime", std::string());
        if (!m.id.empty()) out.versions.push_back(m);
    }
    return true;
}

bool parse_version_record(const std::string &text, VersionRecord &out)
{
    out = VersionRecord();
    const json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return false;
    out.id           = j.value("id", std::string());
    out.release_time = j.value("releaseTime", std::string());
    if (j.contains("downloads") && j["downloads"].is_object() && j["downloads"].contains("server") && j["downloads"]["server"].is_object()) {
        const json &s   = j["downloads"]["server"];
        out.server_url  = s.value("url", std::string());
        out.server_sha1 = s.value("sha1", std::string());
        out.server_size = s.contains("size") && s["size"].is_number_unsigned() ? s["size"].get<std::uint64_t>() : 0;
    }
    if (j.contains("javaVersion") && j["javaVersion"].is_object() && j["javaVersion"].contains("majorVersion") &&
        j["javaVersion"]["majorVersion"].is_number_integer())
        out.java = j["javaVersion"]["majorVersion"].get<int>();
    return !out.id.empty();
}

// ---------------------------------------------------------------------------
// Starting a server

std::vector<std::string> split_arguments(const std::string &typed)
{
    std::vector<std::string> out;
    std::string              cur;
    bool                     quoted = false, any = false;
    for (std::size_t i = 0; i < typed.size(); ++i) {
        const char c = typed[i];
        if (c == '"') { quoted = !quoted; any = true; continue; }
        if (!quoted && std::isspace((unsigned char) c)) {
            if (any) out.push_back(cur);
            cur.clear();
            any = false;
            continue;
        }
        cur += c;
        any = true;
    }
    if (any) out.push_back(cur);
    return out;
}

LaunchCheck check_launch(const LaunchSettings &s, int java_needed, bool eula_accepted, bool jar_exists, bool port_free, int port,
                         std::uint64_t physical_memory_mb)
{
    LaunchCheck c;
    const std::string needed = std::to_string(java_needed);
    if (s.java.empty()) c.errors.push_back({L("No Java was found. This server needs Java {1}."), {needed}});
    else if (s.java_version > 0 && s.java_version < java_needed)
        c.errors.push_back({L("This version needs Java {1}; the chosen Java is {2}."), {needed, std::to_string(s.java_version)}});
    else if (s.java_version == 0) c.notes.push_back({L("The chosen Java did not say its version."), {}});
    if (!jar_exists) c.errors.push_back({L("The server file {1} is missing."), {s.jar.empty() ? std::string("-") : s.jar}});
    if (!eula_accepted) c.errors.push_back({L("The Minecraft EULA has not been accepted for this server."), {}});
    if (s.memory_min_mb < 256) c.errors.push_back({L("Give the server at least 256 MB to start with."), {}});
    if (s.memory_max_mb < s.memory_min_mb) c.errors.push_back({L("The most memory is less than the memory it starts with."), {}});
    if (physical_memory_mb > 0) {
        if (std::uint64_t(s.memory_max_mb) + 1024 > physical_memory_mb)
            c.errors.push_back({L("This PC has {1} GB; leave at least 1 GB for everything else."), {std::to_string(physical_memory_mb / 1024)}});
        else if (std::uint64_t(s.memory_max_mb) * 4 > physical_memory_mb * 3)
            c.notes.push_back({L("The server may use most of this PC's memory."), {}});
    }
    if (port < 1 || port > 65535) c.errors.push_back({L("The port must be 1 to 65535."), {}});
    else if (!port_free) c.errors.push_back({L("Port {1} is already in use by another program."), {std::to_string(port)}});
    for (const std::string &a : split_arguments(s.jvm_args))
        if (starts_with(a, "-Xmx") || starts_with(a, "-Xms")) {
            c.notes.push_back({L("The typed arguments set the memory too; the last one Java reads wins."), {}});
            break;
        }
    return c;
}

std::vector<std::string> launch_arguments(const LaunchSettings &s)
{
    std::vector<std::string> argv = {s.java, "-Xms" + std::to_string(s.memory_min_mb) + "M", "-Xmx" + std::to_string(s.memory_max_mb) + "M"};
    if (s.safe_console)
        for (const char *a : {"-Dfile.encoding=UTF-8", "-Dstdout.encoding=UTF-8", "-Dstderr.encoding=UTF-8", "-Dlog4j2.formatMsgNoLookups=true"})
            argv.push_back(a);
    for (const std::string &a : split_arguments(s.jvm_args)) argv.push_back(a);
    argv.push_back("-jar");
    argv.push_back(s.jar);
    if (s.nogui) argv.push_back("nogui");
    return argv;
}

// ---------------------------------------------------------------------------
// Backups and schedules

std::string safe_file_name(const std::string &name)
{
    std::string out;
    for (unsigned char c : name) {
        if (c < 0x20 || std::strchr("<>:\"/\\|?*", c)) out += '-';
        else out += char(c);
    }
    while (!out.empty() && (out.back() == '.' || out.back() == ' ')) out.pop_back();
    while (!out.empty() && out.front() == ' ') out.erase(out.begin());
    if (out.empty()) out = "server";
    static const char *reserved[] = {"con", "prn", "aux", "nul", "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
                                     "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"};
    const std::string base = lower(out.substr(0, out.find('.')));
    for (const char *r : reserved)
        if (base == r) { out += "-"; break; }
    return out;
}

std::string backup_file_name(const std::string &server_name, std::time_t when, bool automatic)
{
    char      stamp[32] = {};
    std::tm   tm{};
#ifdef _WIN32
    localtime_s(&tm, &when);
#else
    localtime_r(&when, &tm);
#endif
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H-%M-%S", &tm);
    return safe_file_name(server_name) + " " + stamp + (automatic ? " auto" : "") + ".zip";
}

bool is_automatic_backup(const std::string &file_name) { return ends_with(lower(file_name), " auto.zip"); }

std::vector<std::string> backup_entries(const std::string &level_name, const std::vector<std::string> &existing)
{
    std::vector<std::string> out;
    const auto               has = [&](const std::string &n) { return std::find(existing.begin(), existing.end(), n) != existing.end(); };
    for (const std::string &w : {level_name, level_name + "_nether", level_name + "_the_end"})
        if (has(w)) out.push_back(w);
    for (const char *f : {"server.properties", "eula.txt", "whitelist.json", "ops.json", "banned-players.json", "banned-ips.json", "usercache.json"})
        if (has(f)) out.push_back(f);
    return out;
}

std::vector<std::string> backups_to_prune(const std::vector<std::string> &names, int keep)
{
    // The stamp sits just before " auto.zip": "YYYY-MM-DD HH-MM-SS".
    const auto stamp = [](const std::string &n) {
        const std::size_t end = n.size() >= 9 ? n.size() - 9 : 0;
        return end >= 19 ? n.substr(end - 19, 19) : n;
    };
    std::vector<std::string> sorted = names;
    std::sort(sorted.begin(), sorted.end(), [&](const std::string &a, const std::string &b) { return stamp(a) < stamp(b); });
    std::vector<std::string> out;
    const int                extra = int(sorted.size()) - std::max(1, keep);
    for (int i = 0; i < extra; ++i) out.push_back(sorted[std::size_t(i)]);
    return out;
}

std::time_t next_backup(const Schedule &s, std::time_t last, std::time_t now)
{
    if (!s.backup_on) return 0;
    const std::time_t interval = std::time_t(std::clamp(s.backup_hours, 1, 168)) * 3600;
    if (last <= 0) return now + interval;
    const std::time_t t = last + interval;
    return t <= now ? now : t;
}

std::time_t next_restart(const Schedule &s, std::time_t now)
{
    if (!s.restart_on) return 0;
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    tm.tm_hour  = std::clamp(s.restart_hour, 0, 23);
    tm.tm_min   = std::clamp(s.restart_minute, 0, 59);
    tm.tm_sec   = 0;
    tm.tm_isdst = -1;
    std::time_t t = std::mktime(&tm);
    if (t <= now) {
        tm.tm_mday += 1;
        tm.tm_isdst = -1;
        t           = std::mktime(&tm);
    }
    return t;
}

// ---------------------------------------------------------------------------
// The world generator

std::string level_type_for(const std::string &version, WorldPreset preset)
{
    if (version.empty()) return "";
    if (at_least(version, "1.19")) {
        switch (preset) {
        case WorldPreset::Normal: return "minecraft:normal";
        case WorldPreset::Flat: return "minecraft:flat";
        case WorldPreset::LargeBiomes: return "minecraft:large_biomes";
        case WorldPreset::Amplified: return "minecraft:amplified";
        case WorldPreset::SingleBiome: return "minecraft:single_biome_surface";
        }
    }
    if (at_least(version, "1.13")) {
        switch (preset) {
        case WorldPreset::Normal: return "default";
        case WorldPreset::Flat: return "flat";
        case WorldPreset::LargeBiomes: return "largeBiomes";
        case WorldPreset::Amplified: return "amplified";
        default: return "";
        }
    }
    switch (preset) {
    case WorldPreset::Normal: return "DEFAULT";
    case WorldPreset::Flat: return "FLAT";
    case WorldPreset::LargeBiomes: return "LARGEBIOMES";
    case WorldPreset::Amplified: return at_least(version, "1.7") ? "AMPLIFIED" : "";
    default: return "";
    }
}

std::string generator_settings_for(const GeneratorSettings &g)
{
    if (!at_least(g.version, "1.19")) return "";
    if (g.preset == WorldPreset::SingleBiome) return json::object({{"biome", g.biome}}).dump();
    if (g.preset != WorldPreset::Flat) return "";
    json layers = json::array();
    // Shown top first; the server reads them bottom first.
    for (auto it = g.layers.rbegin(); it != g.layers.rend(); ++it)
        layers.push_back(json::object({{"block", block_id(it->block)}, {"height", it->height}}));
    json sets = json::array();
    if (g.structures)
        for (const std::string &s : g.structure_sets) sets.push_back(s);
    return json::object({{"biome", g.biome}, {"features", g.features}, {"lakes", g.lakes}, {"layers", layers}, {"structure_overrides", sets}}).dump();
}

bool can_pregenerate(const std::string &version) { return !version.empty() && at_least(version, "1.19.4"); }

std::vector<Message> generator_problems(const GeneratorSettings &g)
{
    std::vector<Message> out;
    if (g.version.empty()) {
        out.push_back({L("Choose the version that makes the world."), {}});
        return out;
    }
    if (level_type_for(g.version, g.preset).empty()) out.push_back({L("This version cannot make this kind of world."), {}});
    const bool new_format = at_least(g.version, "1.19");
    if (g.preset == WorldPreset::Flat) {
        if (g.layers.empty()) out.push_back({L("A flat world needs at least one layer."), {}});
        int total = 0;
        for (std::size_t i = 0; i < g.layers.size(); ++i) {
            if (block_id(g.layers[i].block).empty()) out.push_back({L("Layer {1} is not a block id."), {std::to_string(i + 1)}});
            if (g.layers[i].height < 1) out.push_back({L("Layer {1} must be at least 1 block tall."), {std::to_string(i + 1)}});
            total += std::max(0, g.layers[i].height);
        }
        const int limit = at_least(g.version, "1.18") ? 384 : 256;
        if (total > limit)
            out.push_back({L("The layers are {1} blocks tall; this version allows {2}."), {std::to_string(total), std::to_string(limit)}});
        if (!new_format) {
            const std::vector<FlatLayer> classic = classic_flat();
            bool                         same    = classic.size() == g.layers.size();
            for (std::size_t i = 0; same && i < classic.size(); ++i)
                same = block_id(classic[i].block) == block_id(g.layers[i].block) && classic[i].height == g.layers[i].height;
            if (!same) out.push_back({L("Your own flat layers need 1.19 or newer; this version makes the classic flat world."), {}});
        }
    }
    if ((g.preset == WorldPreset::Flat || g.preset == WorldPreset::SingleBiome) && new_format && block_id(g.biome).empty())
        out.push_back({L("The biome is not an id."), {}});
    const PropertySpec *gm = find_property("gamemode");
    const PropertySpec *df = find_property("difficulty");
    if (gm && !property_problem(*gm, g.gamemode).empty()) out.push_back({L("Not a game mode."), {}});
    if (df && !property_problem(*df, g.difficulty).empty()) out.push_back({L("Not a difficulty."), {}});
    if (g.width_chunks < 0 || g.depth_chunks < 0 || g.width_chunks > 1000 || g.depth_chunks > 1000)
        out.push_back({L("The area can be 0 to 1,000 chunks each way."), {}});
    if ((g.width_chunks > 0) != (g.depth_chunks > 0)) out.push_back({L("Give the area both a width and a depth, or neither."), {}});
    if (g.width_chunks > 0 && g.depth_chunks > 0 && !can_pregenerate(g.version))
        out.push_back({L("Making an area ahead of time needs 1.19.4 or newer; this version makes the spawn area only."), {}});
    const std::vector<const GameRule *> rules = game_rules_for(g.version);
    for (const auto &rule : g.game_rules) {
        const auto it = std::find_if(rules.begin(), rules.end(), [&](const GameRule *r) { return r->name == rule.first; });
        if (it == rules.end()) {
            out.push_back({L("This version has no game rule {1}."), {rule.first}});
            continue;
        }
        const GameRule &r = **it;
        if (r.is_bool && rule.second != "true" && rule.second != "false") out.push_back({L("{1} is true or false."), {rule.first}});
        if (!r.is_bool) {
            std::string v   = rule.second;
            const bool  neg = !v.empty() && v[0] == '-';
            if (neg) v = v.substr(1);
            const long long n = all_digits(v) && v.size() < 12 ? (neg ? -1 : 1) * std::stoll(v) : r.min - 1;
            if (n < r.min || n > r.max)
                out.push_back({L("{1} must be {2} to {3}."), {rule.first, std::to_string(r.min), std::to_string(r.max)}});
        }
    }
    return out;
}

std::map<std::string, std::string> generator_properties(const GeneratorSettings &g, int port)
{
    std::map<std::string, std::string> p = {
        {"level-name", "world"},
        {"level-seed", g.seed},
        {"level-type", level_type_for(g.version, g.preset)},
        {"generate-structures", g.structures ? "true" : "false"},
        {"hardcore", g.hardcore ? "true" : "false"},
        {"gamemode", g.gamemode},
        {"difficulty", g.hardcore ? "hard" : g.difficulty},
        // Nobody joins the generation run: it listens on this PC only and lets nobody in.
        {"server-ip", "127.0.0.1"},
        {"server-port", std::to_string(port)},
        {"online-mode", "false"},
        {"white-list", "true"},
        {"enforce-whitelist", "true"},
        {"max-players", "1"},
        {"enable-query", "false"},
        {"enable-rcon", "false"},
        {"enable-status", "false"},
        {"spawn-protection", "0"},
        {"motd", "World generation"},
    };
    const std::string settings = generator_settings_for(g);
    if (!settings.empty()) p["generator-settings"] = settings;
    return p;
}

std::string ChunkBatch::add_command() const
{
    return "forceload add " + std::to_string(chunk_x1 * 16) + " " + std::to_string(chunk_z1 * 16) + " " + std::to_string(chunk_x2 * 16 + 15) + " " +
           std::to_string(chunk_z2 * 16 + 15);
}

std::vector<std::string> ChunkBatch::probe_commands() const
{
    const int xs[] = {chunk_x1, chunk_x2, chunk_x1, chunk_x2, (chunk_x1 + chunk_x2) / 2};
    const int zs[] = {chunk_z1, chunk_z1, chunk_z2, chunk_z2, (chunk_z1 + chunk_z2) / 2};
    std::vector<std::string> out;
    for (int i = 0; i < 5; ++i) {
        const std::string cmd = "execute if loaded " + std::to_string(xs[i] * 16 + 8) + " 64 " + std::to_string(zs[i] * 16 + 8);
        if (std::find(out.begin(), out.end(), cmd) == out.end()) out.push_back(cmd);
    }
    return out;
}

std::vector<ChunkBatch> plan_batches(const GeneratorSettings &g)
{
    std::vector<ChunkBatch> out;
    if (g.width_chunks <= 0 || g.depth_chunks <= 0) return out;
    const auto floor_div = [](int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); };
    const int  cx = floor_div(g.centre_x, 16), cz = floor_div(g.centre_z, 16);
    const int  x1 = cx - g.width_chunks / 2, z1 = cz - g.depth_chunks / 2;
    const int  x2 = x1 + g.width_chunks - 1, z2 = z1 + g.depth_chunks - 1;
#ifndef SLIC3R_GAME_SERVERS_NEGATIVE_FIXTURE
    const int  side = 16; // 16 x 16 = 256, the most one forceload command takes
#else
    const int  side = 17;
#endif
    for (int z = z1; z <= z2; z += side)
        for (int x = x1; x <= x2; x += side) {
            ChunkBatch b;
            b.chunk_x1 = x;
            b.chunk_z1 = z;
            b.chunk_x2 = std::min(x + side - 1, x2);
            b.chunk_z2 = std::min(z + side - 1, z2);
            out.push_back(b);
        }
    return out;
}

std::uint64_t estimate_bytes(int chunks) { return std::uint64_t(std::max(0, chunks)) * 12ull * 1024ull; }
int           estimate_seconds(int chunks) { return chunks <= 0 ? 0 : std::max(1, chunks / 40); }

const std::vector<GameRule> &game_rules()
{
    static const std::vector<GameRule> rules = {
        {"announceAdvancements", true, "true", 0, 0, "", L("Advancements are announced in chat.")},
        {"blockExplosionDropDecay", true, "true", 0, 0, "1.19.3", L("Some blocks destroyed by block explosions drop nothing.")},
        {"commandBlockOutput", true, "true", 0, 0, "", L("Command blocks tell operators what they did.")},
        {"commandModificationBlockLimit", false, "32768", 1, 2147483647, "1.19.4", L("How many blocks one command may change.")},
        {"disableElytraMovementCheck", true, "false", 0, 0, "", L("The server stops checking how fast players fly with elytra.")},
        {"disablePlayerMovementCheck", true, "false", 0, 0, "1.21.2", L("The server stops checking how fast players move.")},
        {"disableRaids", true, "false", 0, 0, "1.14", L("No raids happen.")},
        {"doDaylightCycle", true, "true", 0, 0, "", L("Time of day moves on.")},
        {"doEntityDrops", true, "true", 0, 0, "", L("Minecarts, boats and other entities drop items.")},
        {"doFireTick", true, "true", 0, 0, "", L("Fire spreads and burns out.")},
        {"doImmediateRespawn", true, "false", 0, 0, "1.15", L("Players respawn without the death screen.")},
        {"doInsomnia", true, "true", 0, 0, "1.13", L("Phantoms appear for players who do not sleep.")},
        {"doLimitedCrafting", true, "false", 0, 0, "1.12", L("Players can only craft recipes they have unlocked.")},
        {"doMobLoot", true, "true", 0, 0, "", L("Mobs drop items and experience.")},
        {"doMobSpawning", true, "true", 0, 0, "", L("Mobs spawn on their own.")},
        {"doPatrolSpawning", true, "true", 0, 0, "1.15", L("Pillager patrols spawn.")},
        {"doTileDrops", true, "true", 0, 0, "", L("Broken blocks drop items.")},
        {"doTraderSpawning", true, "true", 0, 0, "1.15", L("Wandering traders spawn.")},
        {"doVinesSpread", true, "true", 0, 0, "1.19.4", L("Vines spread to nearby blocks.")},
        {"doWardenSpawning", true, "true", 0, 0, "1.19", L("Wardens spawn.")},
        {"doWeatherCycle", true, "true", 0, 0, "1.11", L("Weather changes.")},
        {"drowningDamage", true, "true", 0, 0, "1.15", L("Players can drown.")},
        {"enderPearlsVanishOnDeath", true, "true", 0, 0, "1.20.2", L("Thrown ender pearls vanish when their player dies.")},
        {"fallDamage", true, "true", 0, 0, "1.15", L("Players take fall damage.")},
        {"fireDamage", true, "true", 0, 0, "1.15", L("Players take fire damage.")},
        {"forgiveDeadPlayers", true, "true", 0, 0, "1.16", L("Angry neutral mobs calm down when their target dies.")},
        {"freezeDamage", true, "true", 0, 0, "1.17", L("Players take freezing damage in powder snow.")},
        {"globalSoundEvents", true, "true", 0, 0, "1.19.3", L("Some sounds are heard by everyone.")},
        {"keepInventory", true, "false", 0, 0, "", L("Players keep their items and experience when they die.")},
        {"lavaSourceConversion", true, "false", 0, 0, "1.19.3", L("Flowing lava between two sources becomes a source.")},
        {"logAdminCommands", true, "true", 0, 0, "", L("Operator commands are written to the log.")},
        {"maxCommandChainLength", false, "65536", 0, 2147483647, "1.12", L("How many commands one chain may run.")},
        {"maxCommandForkCount", false, "65536", 0, 2147483647, "1.20.3", L("How many contexts one command may split into.")},
        {"maxEntityCramming", false, "24", 0, 2147483647, "1.11", L("Mobs squeezed together past this number take damage.")},
        {"mobExplosionDropDecay", true, "true", 0, 0, "1.19.3", L("Some blocks destroyed by mob explosions drop nothing.")},
        {"mobGriefing", true, "true", 0, 0, "", L("Mobs can change blocks and pick up items.")},
        {"naturalRegeneration", true, "true", 0, 0, "", L("Players heal when they are not hungry.")},
        {"playersNetherPortalCreativeDelay", false, "0", 0, 2147483647, "1.21.2", L("Ticks a creative player waits in a Nether portal.")},
        {"playersNetherPortalDefaultDelay", false, "80", 0, 2147483647, "1.21.2", L("Ticks a player waits in a Nether portal.")},
        {"playersSleepingPercentage", false, "100", 0, 100, "1.17", L("The share of players who must sleep to skip the night.")},
        {"projectilesCanBreakBlocks", true, "true", 0, 0, "1.20.5", L("Projectiles can break blocks such as decorated pots.")},
        {"randomTickSpeed", false, "3", 0, 4096, "", L("How fast crops grow, leaves fall and other random changes happen.")},
        {"reducedDebugInfo", true, "false", 0, 0, "", L("The debug screen shows less.")},
        {"sendCommandFeedback", true, "true", 0, 0, "", L("Players see what their commands did.")},
        {"showDeathMessages", true, "true", 0, 0, "", L("Deaths are announced in chat.")},
        {"snowAccumulationHeight", false, "1", 0, 8, "1.19.3", L("How many layers snow builds up to while it snows.")},
        {"spawnChunkRadius", false, "2", 0, 32, "1.20.5", L("How many chunks around spawn stay loaded.")},
        {"spawnRadius", false, "10", 0, 2147483647, "", L("How far from spawn new players appear.")},
        {"spectatorsGenerateChunks", true, "true", 0, 0, "", L("Spectators make new chunks.")},
        {"tntExplosionDropDecay", true, "false", 0, 0, "1.19.3", L("Some blocks destroyed by TNT drop nothing.")},
        {"universalAnger", true, "false", 0, 0, "1.16", L("Angry neutral mobs attack every player nearby.")},
        {"waterSourceConversion", true, "true", 0, 0, "1.19.3", L("Flowing water between two sources becomes a source.")},
    };
    return rules;
}

std::vector<const GameRule *> game_rules_for(const std::string &version)
{
    std::vector<const GameRule *> out;
    for (const GameRule &r : game_rules())
        if (version.empty() || r.since.empty() || at_least(version, r.since)) out.push_back(&r);
    return out;
}

const std::vector<NamedId> &overworld_biomes()
{
    static const std::vector<NamedId> biomes = {
        {"minecraft:plains", L("Plains"), ""}, {"minecraft:sunflower_plains", L("Sunflower plains"), ""}, {"minecraft:snowy_plains", L("Snowy plains"), ""},
        {"minecraft:ice_spikes", L("Ice spikes"), ""}, {"minecraft:desert", L("Desert"), ""}, {"minecraft:swamp", L("Swamp"), ""},
        {"minecraft:mangrove_swamp", L("Mangrove swamp"), "1.19"}, {"minecraft:forest", L("Forest"), ""}, {"minecraft:flower_forest", L("Flower forest"), ""},
        {"minecraft:birch_forest", L("Birch forest"), ""}, {"minecraft:dark_forest", L("Dark forest"), ""}, {"minecraft:pale_garden", L("Pale garden"), "1.21.4"},
        {"minecraft:old_growth_birch_forest", L("Old growth birch forest"), "1.18"}, {"minecraft:old_growth_pine_taiga", L("Old growth pine taiga"), "1.18"},
        {"minecraft:old_growth_spruce_taiga", L("Old growth spruce taiga"), "1.18"}, {"minecraft:taiga", L("Taiga"), ""},
        {"minecraft:snowy_taiga", L("Snowy taiga"), ""}, {"minecraft:savanna", L("Savanna"), ""}, {"minecraft:savanna_plateau", L("Savanna plateau"), ""},
        {"minecraft:windswept_hills", L("Windswept hills"), "1.18"}, {"minecraft:windswept_forest", L("Windswept forest"), "1.18"},
        {"minecraft:windswept_savanna", L("Windswept savanna"), "1.18"}, {"minecraft:jungle", L("Jungle"), ""}, {"minecraft:sparse_jungle", L("Sparse jungle"), "1.18"},
        {"minecraft:bamboo_jungle", L("Bamboo jungle"), "1.14"}, {"minecraft:badlands", L("Badlands"), ""}, {"minecraft:eroded_badlands", L("Eroded badlands"), ""},
        {"minecraft:wooded_badlands", L("Wooded badlands"), "1.18"}, {"minecraft:meadow", L("Meadow"), "1.18"}, {"minecraft:cherry_grove", L("Cherry grove"), "1.20"},
        {"minecraft:grove", L("Grove"), "1.18"}, {"minecraft:snowy_slopes", L("Snowy slopes"), "1.18"}, {"minecraft:frozen_peaks", L("Frozen peaks"), "1.18"},
        {"minecraft:jagged_peaks", L("Jagged peaks"), "1.18"}, {"minecraft:stony_peaks", L("Stony peaks"), "1.18"}, {"minecraft:river", L("River"), ""},
        {"minecraft:frozen_river", L("Frozen river"), ""}, {"minecraft:beach", L("Beach"), ""}, {"minecraft:snowy_beach", L("Snowy beach"), ""},
        {"minecraft:stony_shore", L("Stony shore"), "1.18"}, {"minecraft:warm_ocean", L("Warm ocean"), "1.13"}, {"minecraft:lukewarm_ocean", L("Lukewarm ocean"), "1.13"},
        {"minecraft:ocean", L("Ocean"), ""}, {"minecraft:deep_ocean", L("Deep ocean"), ""}, {"minecraft:cold_ocean", L("Cold ocean"), "1.13"},
        {"minecraft:frozen_ocean", L("Frozen ocean"), ""}, {"minecraft:mushroom_fields", L("Mushroom fields"), ""},
        {"minecraft:dripstone_caves", L("Dripstone caves"), "1.18"}, {"minecraft:lush_caves", L("Lush caves"), "1.18"}, {"minecraft:deep_dark", L("Deep dark"), "1.19"},
    };
    return biomes;
}

const std::vector<NamedId> &structure_sets()
{
    static const std::vector<NamedId> sets = {
        {"minecraft:villages", L("Villages"), ""},           {"minecraft:strongholds", L("Strongholds"), ""},
        {"minecraft:mineshafts", L("Mineshafts"), ""},       {"minecraft:ancient_cities", L("Ancient cities"), "1.19"},
        {"minecraft:trial_chambers", L("Trial chambers"), "1.21"}, {"minecraft:shipwrecks", L("Shipwrecks"), ""},
        {"minecraft:woodland_mansions", L("Woodland mansions"), ""}, {"minecraft:ruined_portals", L("Ruined portals"), ""},
        {"minecraft:desert_pyramids", L("Desert pyramids"), ""}, {"minecraft:jungle_temples", L("Jungle temples"), ""},
        {"minecraft:swamp_huts", L("Swamp huts"), ""},       {"minecraft:igloos", L("Igloos"), ""},
        {"minecraft:pillager_outposts", L("Pillager outposts"), ""}, {"minecraft:ocean_monuments", L("Ocean monuments"), ""},
        {"minecraft:ocean_ruins", L("Ocean ruins"), ""},     {"minecraft:buried_treasures", L("Buried treasure"), ""},
        {"minecraft:trail_ruins", L("Trail ruins"), "1.20"}, {"minecraft:nether_complexes", L("Nether fortresses and bastions"), ""},
        {"minecraft:nether_fossils", L("Nether fossils"), ""}, {"minecraft:end_cities", L("End cities"), ""},
    };
    return sets;
}

const std::vector<NamedId> &flat_blocks()
{
    static const std::vector<NamedId> blocks = {
        {"minecraft:grass_block", L("Grass block"), ""}, {"minecraft:dirt", L("Dirt"), ""},         {"minecraft:coarse_dirt", L("Coarse dirt"), ""},
        {"minecraft:stone", L("Stone"), ""},             {"minecraft:deepslate", L("Deepslate"), "1.17"}, {"minecraft:bedrock", L("Bedrock"), ""},
        {"minecraft:sand", L("Sand"), ""},               {"minecraft:sandstone", L("Sandstone"), ""}, {"minecraft:gravel", L("Gravel"), ""},
        {"minecraft:snow_block", L("Snow block"), ""},   {"minecraft:water", L("Water"), ""},       {"minecraft:lava", L("Lava"), ""},
        {"minecraft:air", L("Air"), ""},                 {"minecraft:clay", L("Clay"), ""},         {"minecraft:moss_block", L("Moss block"), "1.17"},
    };
    return blocks;
}

std::string block_id(const std::string &typed)
{
    std::string id = lower(trim(typed));
    if (id.empty()) return "";
    if (id.find(':') == std::string::npos) id = "minecraft:" + id;
    const std::size_t colon = id.find(':');
    const std::string ns = id.substr(0, colon), path = id.substr(colon + 1);
    const auto        ok_ns   = [](unsigned char c) { return std::islower(c) || std::isdigit(c) || c == '_' || c == '.' || c == '-'; };
    const auto        ok_path = [&](unsigned char c) { return ok_ns(c) || c == '/'; };
    if (ns.empty() || path.empty() || !std::all_of(ns.begin(), ns.end(), ok_ns) || !std::all_of(path.begin(), path.end(), ok_path)) return "";
    return id;
}

std::vector<FlatLayer> classic_flat()
{
    return {{"minecraft:grass_block", 1}, {"minecraft:dirt", 2}, {"minecraft:bedrock", 1}};
}

std::string random_seed(std::uint64_t entropy)
{
    // A splitmix64 step spreads a weak entropy source over all 64 bits.
    std::uint64_t z = entropy + 0x9E3779B97F4A7C15ull;
    z               = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z               = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z               = z ^ (z >> 31);
    return std::to_string(static_cast<long long>(z));
}

// ---------------------------------------------------------------------------
// The library and its history

void Library::record(HistoryEntry e)
{
    history.push_back(std::move(e));
    if (history.size() > 300) history.erase(history.begin(), history.begin() + std::ptrdiff_t(history.size() - 300));
}

ServerRecord *Library::find(const std::string &id)
{
    for (ServerRecord &s : servers)
        if (s.id == id) return &s;
    return nullptr;
}

const ServerRecord *Library::find(const std::string &id) const
{
    for (const ServerRecord &s : servers)
        if (s.id == id) return &s;
    return nullptr;
}

std::string Library::unique_id(const std::string &name) const
{
    std::string base;
    for (unsigned char c : lower(name)) {
        if (std::isalnum(c)) base += char(c);
        else if (!base.empty() && base.back() != '-') base += '-';
    }
    while (!base.empty() && base.back() == '-') base.pop_back();
    if (base.empty()) base = "server";
    std::string id = base;
    for (int n = 2; find(id); ++n) id = base + "-" + std::to_string(n);
    return id;
}

std::string to_json(const Library &library)
{
    json servers = json::array();
    for (const ServerRecord &s : library.servers)
        servers.push_back(json::object({
            {"id", s.id}, {"name", s.name}, {"folder", s.folder}, {"version", s.version}, {"flavour", s.flavour},
            {"java_choice", s.java_choice}, {"last_backup", static_cast<long long>(s.last_backup)},
            {"launch", json::object({{"java", s.launch.java}, {"java_version", s.launch.java_version}, {"jar", s.launch.jar},
                                     {"memory_min_mb", s.launch.memory_min_mb}, {"memory_max_mb", s.launch.memory_max_mb},
                                     {"jvm_args", s.launch.jvm_args}})},
            {"schedule", json::object({{"backup_on", s.schedule.backup_on}, {"backup_hours", s.schedule.backup_hours},
                                       {"backup_keep", s.schedule.backup_keep}, {"restart_on", s.schedule.restart_on},
                                       {"restart_hour", s.schedule.restart_hour}, {"restart_minute", s.schedule.restart_minute},
                                       {"warn_minutes", s.schedule.warn_minutes}})},
        }));
    json history = json::array();
    for (const HistoryEntry &h : library.history)
        history.push_back(json::object({{"when", static_cast<long long>(h.when)}, {"server", h.server}, {"action", h.action},
                                        {"detail", h.detail}, {"outcome", h.outcome}}));
    return json::object({{"version", 1}, {"servers", servers}, {"history", history}, {"backup_folder", library.backup_folder}}).dump(1);
}

bool from_json(const std::string &text, Library &out)
{
    out = Library();
    const json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return false;
    out.backup_folder = j.value("backup_folder", std::string());
    if (j.contains("servers") && j["servers"].is_array())
        for (const json &e : j["servers"]) {
            if (!e.is_object()) continue;
            ServerRecord s;
            s.id          = e.value("id", std::string());
            s.name        = e.value("name", std::string());
            s.folder      = e.value("folder", std::string());
            s.version     = e.value("version", std::string());
            s.flavour     = e.value("flavour", std::string());
            s.java_choice = e.value("java_choice", std::string("auto"));
            s.last_backup = static_cast<std::time_t>(e.value("last_backup", 0LL));
            if (e.contains("launch") && e["launch"].is_object()) {
                const json &l          = e["launch"];
                s.launch.java          = l.value("java", std::string());
                s.launch.java_version  = l.value("java_version", 0);
                s.launch.jar           = l.value("jar", std::string());
                s.launch.memory_min_mb = l.value("memory_min_mb", 1024);
                s.launch.memory_max_mb = l.value("memory_max_mb", 4096);
                s.launch.jvm_args      = l.value("jvm_args", std::string());
            }
            if (e.contains("schedule") && e["schedule"].is_object()) {
                const json &c             = e["schedule"];
                s.schedule.backup_on      = c.value("backup_on", false);
                s.schedule.backup_hours   = c.value("backup_hours", 6);
                s.schedule.backup_keep    = c.value("backup_keep", 8);
                s.schedule.restart_on     = c.value("restart_on", false);
                s.schedule.restart_hour   = c.value("restart_hour", 5);
                s.schedule.restart_minute = c.value("restart_minute", 0);
                s.schedule.warn_minutes   = c.value("warn_minutes", 5);
            }
            if (!s.id.empty() && !s.folder.empty()) out.servers.push_back(s);
        }
    if (j.contains("history") && j["history"].is_array())
        for (const json &e : j["history"]) {
            if (!e.is_object()) continue;
            HistoryEntry h;
            h.when    = static_cast<std::time_t>(e.value("when", 0LL));
            h.server  = e.value("server", std::string());
            h.action  = e.value("action", std::string());
            h.detail  = e.value("detail", std::string());
            h.outcome = e.value("outcome", std::string());
            out.record(h);
        }
    return true;
}

std::string unique_name(const std::string &wanted, const std::vector<std::string> &taken)
{
    const auto is_taken = [&](const std::string &n) {
        return std::any_of(taken.begin(), taken.end(), [&](const std::string &t) { return lower(t) == lower(n); });
    };
    if (!is_taken(wanted)) return wanted;
    for (int n = 1;; ++n) {
        const std::string candidate = wanted + " (" + std::to_string(n) + ")";
        if (!is_taken(candidate)) return candidate;
    }
}

// ---------------------------------------------------------------------------
// SHA-1

namespace {
std::uint32_t rotl(std::uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }
} // namespace

Sha1::Sha1() : m_h{0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u}, m_buffer{} {}

void Sha1::block(const std::uint8_t *p)
{
    std::uint32_t w[80];
    for (int i = 0; i < 16; ++i)
        w[i] = (std::uint32_t(p[i * 4]) << 24) | (std::uint32_t(p[i * 4 + 1]) << 16) | (std::uint32_t(p[i * 4 + 2]) << 8) | std::uint32_t(p[i * 4 + 3]);
    for (int i = 16; i < 80; ++i) w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    std::uint32_t a = m_h[0], b = m_h[1], c = m_h[2], d = m_h[3], e = m_h[4];
    for (int i = 0; i < 80; ++i) {
        std::uint32_t f, k;
        if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999u; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1u; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
        else { f = b ^ c ^ d; k = 0xCA62C1D6u; }
        const std::uint32_t t = rotl(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rotl(b, 30);
        b = a;
        a = t;
    }
    m_h[0] += a;
    m_h[1] += b;
    m_h[2] += c;
    m_h[3] += d;
    m_h[4] += e;
}

void Sha1::update(const void *data, std::size_t size)
{
    const auto *p = static_cast<const std::uint8_t *>(data);
    m_length += size;
    while (size > 0) {
        const std::size_t take = std::min(size, sizeof(m_buffer) - m_used);
        std::memcpy(m_buffer + m_used, p, take);
        m_used += take;
        p += take;
        size -= take;
        if (m_used == sizeof(m_buffer)) {
            block(m_buffer);
            m_used = 0;
        }
    }
}

std::string Sha1::hex()
{
    const std::uint64_t bits = m_length * 8;
    const std::uint8_t  one  = 0x80, zero = 0;
    update(&one, 1);
    while (m_used != 56) update(&zero, 1);
    std::uint8_t len[8];
    for (int i = 0; i < 8; ++i) len[i] = std::uint8_t(bits >> (56 - 8 * i));
    update(len, 8);
    char out[41];
    for (int i = 0; i < 5; ++i) std::snprintf(out + i * 8, 9, "%08x", m_h[i]);
    return std::string(out, 40);
}

std::string sha1_hex(const std::string &data)
{
    Sha1 s;
    s.update(data.data(), data.size());
    return s.hex();
}

}}} // namespace Slic3r::GUI::GameServers
