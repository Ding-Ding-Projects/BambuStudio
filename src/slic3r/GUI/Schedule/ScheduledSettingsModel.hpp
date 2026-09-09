#ifndef slic3r_GUI_Schedule_ScheduledSettingsModel_hpp_
#define slic3r_GUI_Schedule_ScheduledSettingsModel_hpp_

// Scheduled settings: the wx-free model. Everything here is plain C++17 plus
// nlohmann::json so the Catch2 suite in tests/scheduled_settings can exercise
// the schema, the matcher, the precedence rule, the API validation and the
// override/restore arithmetic without a GUI runtime. ScheduledSettings.cpp
// owns the timer, the transports and the application of values.
//
// Schema (schemaVersion 1):
//   {
//     "schemaVersion": 1,
//     "rules": [
//       {
//         "id": "r-1a2b3c4d",            stable, never renumbered
//         "label": "Evening dark mode",
//         "enabled": true,
//         "startDate": "2026-09-01",     optional, inclusive, local calendar
//         "endDate":   "2026-12-31",     optional, inclusive
//         "startTime": "20:00",          local wall-clock, minute precision
//         "endTime":   "07:00",          may be earlier than startTime (crosses midnight)
//         "weekdays":  ["mon","tue"],    or "everyday"
//         "source": { "kind": "local" }
//                 | { "kind": "api", "url": "https://...", "allowLoopbackHttp": false }
//                 | { "kind": "homeAssistant", "entityId": "input_boolean.night" },
//         "values": { "dark_color_mode": "1", "ui_density": "compact" }
//       }
//     ]
//   }
// Unknown fields on the document and on each rule are preserved verbatim on
// round trip so a newer build's data survives an older build's edit.
//
// Precedence: rules are evaluated in list order and later matching rules win
// per setting key. The list order is the user's priority order.
//
// Time semantics (all in the user's local timezone, as the C runtime reports
// it; a daylight-saving jump simply moves the wall clock, so a window that
// spans the jump is one hour shorter or longer that night, and a start time
// that does not exist that night is treated as already passed):
//   startTime <  endTime : matches when startTime <= now < endTime, on a listed weekday.
//   startTime >  endTime : crosses midnight; matches when now >= startTime on a
//                          listed weekday, or now < endTime on the day after a
//                          listed weekday (the weekday is the day the window started).
//   startTime == endTime : all day on the listed weekdays.
//   Date bounds are inclusive on both ends and are checked on the day the
//   window started (so a cross-midnight rule ending 2026-09-30 still covers
//   the early hours of 2026-10-01).
//   No rules, or no matching rules: nothing is overridden and the base
//   settings stay in force.

#include "nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Slic3r { namespace GUI { namespace Schedule {

inline constexpr int         kSchemaVersion       = 1;
inline constexpr std::size_t kMaxRules            = 64;
inline constexpr std::size_t kMaxLabelBytes       = 120;
inline constexpr std::size_t kMaxDocumentBytes    = 256 * 1024;
inline constexpr std::size_t kMaxApiResponseBytes = 64 * 1024;
inline constexpr std::size_t kMaxUrlBytes         = 2048;
inline constexpr std::size_t kMaxEntityIdBytes    = 256;
inline constexpr std::size_t kMaxValueBytes       = 200;
inline constexpr long        kApiTimeoutSeconds   = 10;

// The AppConfig keys a rule may set. Anything else in a document or an API
// response is dropped, so remote data can never reach a path, a token or a
// credential key.
inline constexpr const char *kKeyLanguageMode        = "language";
inline constexpr const char *kKeyTheme               = "dark_color_mode";
inline constexpr const char *kKeyDensity             = "ui_density";
inline constexpr const char *kKeyAccentSeed          = "ui_accent_seed";
inline constexpr const char *kKeyFontFamily          = "ui_font_family";
inline constexpr const char *kKeyFontScale           = "ui_font_scale";
inline constexpr const char *kKeyFunnyLevelEnglish   = "funny_level_en";
inline constexpr const char *kKeyFunnyLevelCantonese = "funny_level_yue";
inline constexpr const char *kKeyDisplayName         = "app_display_name";

inline const std::vector<std::string> &allowed_keys()
{
    static const std::vector<std::string> keys = {
        kKeyLanguageMode, kKeyTheme, kKeyDensity, kKeyAccentSeed, kKeyFontFamily,
        kKeyFontScale, kKeyFunnyLevelEnglish, kKeyFunnyLevelCantonese, kKeyDisplayName};
    return keys;
}

inline bool is_allowed_key(const std::string &key)
{
    const auto &keys = allowed_keys();
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

// Validate one value for one allowed key. Returns an empty string when the
// value is acceptable, otherwise a plain-words problem for the UI.
inline std::string validate_value(const std::string &key, const std::string &value)
{
    if (!is_allowed_key(key))
        return "This setting cannot be scheduled.";
    if (value.size() > kMaxValueBytes)
        return "Value is too long.";
    for (unsigned char c : value)
        if (c < 0x20 || c == 0x7f)
            return "Remove line breaks and other control characters.";
    if (key == kKeyLanguageMode) {
        if (value.empty() || value.size() > 32)
            return "Choose a language mode.";
        for (unsigned char c : value)
            if (!(std::isalnum(c) || c == '_' || c == '-'))
                return "Language mode ids use letters, digits, '_' and '-' only.";
        return {};
    }
    if (key == kKeyTheme)
        return (value == "0" || value == "1") ? std::string() : "Theme must be light (0) or dark (1).";
    if (key == kKeyDensity)
        return (value == "comfortable" || value == "compact") ? std::string() : "Density must be comfortable or compact.";
    if (key == kKeyAccentSeed) {
        if (value.empty())
            return {}; // empty = the brand seed
        if (value.size() != 7 || value[0] != '#')
            return "Accent colour must look like #146c2e.";
        for (std::size_t i = 1; i < value.size(); ++i)
            if (!std::isxdigit(static_cast<unsigned char>(value[i])))
                return "Accent colour must look like #146c2e.";
        return {};
    }
    if (key == kKeyFontFamily)
        return value.size() <= 100 ? std::string() : "Font family name is too long.";
    if (key == kKeyFontScale) {
        if (value.empty())
            return "Choose a text size.";
        char  *end   = nullptr;
        double scale = std::strtod(value.c_str(), &end);
        if (end == value.c_str() || *end != '\0')
            return "Text size must be a number between 0.8 and 1.4.";
        return (scale >= 0.8 && scale <= 1.4) ? std::string() : "Text size must be between 0.8 and 1.4.";
    }
    if (key == kKeyFunnyLevelEnglish || key == kKeyFunnyLevelCantonese) {
        if (value.size() != 1 || value[0] < '1' || value[0] > '5')
            return "Funny level must be 1 to 5.";
        return {};
    }
    if (key == kKeyDisplayName) {
        // Count code points; the display-name rules allow 0 (reset) to 40.
        std::size_t points = 0;
        for (unsigned char c : value)
            if ((c & 0xC0) != 0x80)
                ++points;
        return points <= 40 ? std::string() : "App name must be 40 characters or fewer.";
    }
    return "This setting cannot be scheduled.";
}

// ---------------------------------------------------------------------------
// Calendar primitives
// ---------------------------------------------------------------------------

struct LocalDate
{
    int year  = 1970;
    int month = 1; // 1..12
    int day   = 1; // 1..31

    friend bool operator==(const LocalDate &a, const LocalDate &b) { return a.year == b.year && a.month == b.month && a.day == b.day; }
    friend bool operator!=(const LocalDate &a, const LocalDate &b) { return !(a == b); }
    friend bool operator<(const LocalDate &a, const LocalDate &b)
    {
        if (a.year != b.year) return a.year < b.year;
        if (a.month != b.month) return a.month < b.month;
        return a.day < b.day;
    }
    friend bool operator<=(const LocalDate &a, const LocalDate &b) { return !(b < a); }
};

struct LocalTime
{
    int hour   = 0; // 0..23
    int minute = 0; // 0..59

    int minutes_of_day() const { return hour * 60 + minute; }
    friend bool operator==(const LocalTime &a, const LocalTime &b) { return a.hour == b.hour && a.minute == b.minute; }
    friend bool operator!=(const LocalTime &a, const LocalTime &b) { return !(a == b); }
};

// Bit 0 = Monday ... bit 6 = Sunday (ISO order).
using WeekdaySet = std::uint8_t;
inline constexpr WeekdaySet kEveryDay = 0x7F;
inline constexpr WeekdaySet kNoDay    = 0;

inline WeekdaySet weekday_bit(int iso_weekday_0_monday) { return static_cast<WeekdaySet>(1u << (iso_weekday_0_monday % 7)); }
inline bool       weekday_in(WeekdaySet set, int iso_weekday_0_monday) { return (set & weekday_bit(iso_weekday_0_monday)) != 0; }

inline const char *weekday_short_name(int iso_weekday_0_monday)
{
    static const char *names[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
    return names[((iso_weekday_0_monday % 7) + 7) % 7];
}

inline bool is_leap_year(int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

inline int days_in_month(int year, int month)
{
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    return month == 2 && is_leap_year(year) ? 29 : days[month - 1];
}

inline bool is_valid_date(const LocalDate &d)
{
    return d.year >= 1970 && d.year <= 2999 && d.month >= 1 && d.month <= 12 && d.day >= 1 && d.day <= days_in_month(d.year, d.month);
}

inline bool is_valid_time(const LocalTime &t) { return t.hour >= 0 && t.hour < 24 && t.minute >= 0 && t.minute < 60; }

// Previous calendar day (used for cross-midnight windows).
inline LocalDate previous_day(LocalDate d)
{
    if (d.day > 1) { --d.day; return d; }
    if (d.month > 1) { --d.month; d.day = days_in_month(d.year, d.month); return d; }
    --d.year; d.month = 12; d.day = 31;
    return d;
}

// Day of week for a proleptic Gregorian date, 0 = Monday ... 6 = Sunday.
inline int iso_weekday(const LocalDate &d)
{
    int y = d.year, m = d.month;
    if (m < 3) { m += 12; --y; }
    const int k = y % 100, j = y / 100;
    const int h = (d.day + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7; // 0 = Saturday
    return (h + 5) % 7;                                                       // 0 = Monday
}

// ISO strings ("2026-09-08", "07:30"). Parsing is strict: exact widths only.
inline std::optional<LocalDate> parse_date(const std::string &s)
{
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return std::nullopt;
    for (std::size_t i = 0; i < s.size(); ++i)
        if (i != 4 && i != 7 && !std::isdigit(static_cast<unsigned char>(s[i]))) return std::nullopt;
    LocalDate d;
    d.year  = std::stoi(s.substr(0, 4));
    d.month = std::stoi(s.substr(5, 2));
    d.day   = std::stoi(s.substr(8, 2));
    return is_valid_date(d) ? std::optional<LocalDate>(d) : std::nullopt;
}

inline std::optional<LocalTime> parse_time(const std::string &s)
{
    if (s.size() != 5 || s[2] != ':') return std::nullopt;
    for (std::size_t i = 0; i < s.size(); ++i)
        if (i != 2 && !std::isdigit(static_cast<unsigned char>(s[i]))) return std::nullopt;
    LocalTime t;
    t.hour   = std::stoi(s.substr(0, 2));
    t.minute = std::stoi(s.substr(3, 2));
    return is_valid_time(t) ? std::optional<LocalTime>(t) : std::nullopt;
}

inline std::string format_date(const LocalDate &d)
{
    char buf[16];
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02d", d.year, d.month, d.day);
    return buf;
}

inline std::string format_time(const LocalTime &t)
{
    char buf[8];
    std::snprintf(buf, sizeof buf, "%02d:%02d", t.hour, t.minute);
    return buf;
}

// A local wall-clock instant as the evaluator sees it.
struct LocalInstant
{
    LocalDate date;
    LocalTime time;

    static LocalInstant from_tm(const std::tm &tm)
    {
        LocalInstant i;
        i.date.year   = tm.tm_year + 1900;
        i.date.month  = tm.tm_mon + 1;
        i.date.day    = tm.tm_mday;
        i.time.hour   = tm.tm_hour;
        i.time.minute = tm.tm_min;
        return i;
    }
};

// ---------------------------------------------------------------------------
// Rules and documents
// ---------------------------------------------------------------------------

enum class SourceKind { Local, Api, HomeAssistant };

inline const char *source_kind_name(SourceKind kind)
{
    switch (kind) {
    case SourceKind::Api: return "api";
    case SourceKind::HomeAssistant: return "homeAssistant";
    case SourceKind::Local: break;
    }
    return "local";
}

inline std::optional<SourceKind> parse_source_kind(const std::string &name)
{
    if (name == "local") return SourceKind::Local;
    if (name == "api") return SourceKind::Api;
    if (name == "homeAssistant") return SourceKind::HomeAssistant;
    return std::nullopt;
}

struct Rule
{
    std::string              id;
    std::string              label;
    bool                     enabled = true;
    std::optional<LocalDate> start_date;
    std::optional<LocalDate> end_date;
    LocalTime                start_time;
    LocalTime                end_time;
    WeekdaySet               weekdays = kEveryDay;
    SourceKind               source   = SourceKind::Local;
    std::string              source_url;          // Api
    bool                     allow_loopback_http = false; // Api, explicit dev flag
    std::string              source_entity_id;    // HomeAssistant
    std::map<std::string, std::string> values;    // allowlisted keys only
    nlohmann::json           extra = nlohmann::json::object(); // unknown fields, preserved

    bool all_day() const { return start_time == end_time; }
    bool crosses_midnight() const { return end_time.minutes_of_day() < start_time.minutes_of_day(); }
};

struct Document
{
    int               schema_version = kSchemaVersion;
    std::vector<Rule> rules;
    nlohmann::json    extra = nlohmann::json::object();

    const Rule *find(const std::string &id) const
    {
        for (const Rule &r : rules)
            if (r.id == id) return &r;
        return nullptr;
    }
    Rule *find(const std::string &id)
    {
        for (Rule &r : rules)
            if (r.id == id) return &r;
        return nullptr;
    }
};

// Rule ids are short, stable and never reused: "r-" plus 8 hex digits drawn
// from the caller's entropy so the model stays deterministic under test.
inline std::string make_rule_id(std::uint32_t entropy)
{
    char buf[16];
    std::snprintf(buf, sizeof buf, "r-%08x", entropy);
    return buf;
}

inline std::string unique_rule_id(const Document &doc, std::uint32_t entropy)
{
    std::string id = make_rule_id(entropy);
    while (doc.find(id) != nullptr)
        id = make_rule_id(++entropy);
    return id;
}

// Structural validation of one rule, in plain words for the editor.
inline std::vector<std::string> validate_rule(const Rule &rule)
{
    std::vector<std::string> problems;
    if (rule.label.empty())
        problems.push_back("Give the rule a name.");
    if (rule.label.size() > kMaxLabelBytes)
        problems.push_back("The name is too long.");
    if (!is_valid_time(rule.start_time) || !is_valid_time(rule.end_time))
        problems.push_back("Times must be between 00:00 and 23:59.");
    if (rule.start_date && !is_valid_date(*rule.start_date))
        problems.push_back("The start date is not a real calendar date.");
    if (rule.end_date && !is_valid_date(*rule.end_date))
        problems.push_back("The end date is not a real calendar date.");
    if (rule.start_date && rule.end_date && *rule.end_date < *rule.start_date)
        problems.push_back("The end date is before the start date.");
    if ((rule.weekdays & kEveryDay) == kNoDay)
        problems.push_back("Pick at least one weekday, or choose every day.");
    if (rule.source == SourceKind::Api) {
        // reported through validate_api_url so the wording is shared
    } else if (rule.source == SourceKind::HomeAssistant) {
        if (rule.source_entity_id.empty())
            problems.push_back("Enter the Home Assistant entity id, for example input_boolean.night_mode.");
    }
    if (rule.source != SourceKind::Api && rule.values.empty())
        problems.push_back("Choose at least one setting for this rule to change.");
    for (const auto &[key, value] : rule.values) {
        const std::string problem = validate_value(key, value);
        if (!problem.empty())
            problems.push_back(key + ": " + problem);
    }
    return problems;
}

// ---------------------------------------------------------------------------
// URL policy for the API source
// ---------------------------------------------------------------------------

inline std::string lowercase_ascii(std::string s)
{
    for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Returns the host part (without port) of an http(s) URL, or "" when malformed.
inline std::string url_host(const std::string &url)
{
    const std::size_t scheme = url.find("://");
    if (scheme == std::string::npos) return {};
    const std::size_t start = scheme + 3;
    std::size_t       end   = url.find_first_of("/?#", start);
    if (end == std::string::npos) end = url.size();
    std::string authority = url.substr(start, end - start);
    if (authority.find('@') != std::string::npos) return {}; // credentials in URL
    if (!authority.empty() && authority[0] == '[') {          // IPv6 literal
        const std::size_t close = authority.find(']');
        return close == std::string::npos ? std::string() : lowercase_ascii(authority.substr(1, close - 1));
    }
    const std::size_t colon = authority.find(':');
    return lowercase_ascii(colon == std::string::npos ? authority : authority.substr(0, colon));
}

inline bool is_loopback_host(const std::string &host)
{
    return host == "localhost" || host == "::1" || host.rfind("127.", 0) == 0;
}

// Empty string = acceptable. HTTPS is always allowed; HTTP is allowed only to a
// loopback host and only when the rule's explicit development flag is set.
inline std::string validate_api_url(const std::string &url, bool allow_loopback_http)
{
    if (url.empty())
        return "Enter the HTTPS address of the settings API.";
    if (url.size() > kMaxUrlBytes)
        return "The address is too long.";
    for (unsigned char c : url)
        if (c <= 0x20 || c >= 0x7f)
            return "The address contains spaces or characters that are not allowed.";
    const std::string lower = lowercase_ascii(url);
    const bool        https = lower.rfind("https://", 0) == 0;
    const bool        http  = lower.rfind("http://", 0) == 0;
    if (!https && !http)
        return "The address must start with https://.";
    const std::size_t scheme_end = url.find("://") + 3;
    std::size_t       path_start = url.find_first_of("/?#", scheme_end);
    if (path_start == std::string::npos) path_start = url.size();
    if (url.substr(scheme_end, path_start - scheme_end).find('@') != std::string::npos)
        return "Remove the user name and password from the address; credentials never go in a URL.";
    const std::string host = url_host(url);
    if (host.empty())
        return "The address has no host name.";
    if (http && !(allow_loopback_http && is_loopback_host(host)))
        return "Plain http:// is only allowed to localhost, and only with the local development option on.";
    return {};
}

// ---------------------------------------------------------------------------
// JSON (de)serialization
// ---------------------------------------------------------------------------

struct ParseResult
{
    bool        ok = false;
    std::string error;
    Document    document;
};

inline WeekdaySet weekdays_from_json(const nlohmann::json &j, bool &ok)
{
    ok = true;
    if (j.is_string()) {
        if (j.get<std::string>() == "everyday") return kEveryDay;
        ok = false;
        return kNoDay;
    }
    if (!j.is_array()) { ok = false; return kNoDay; }
    WeekdaySet set = kNoDay;
    for (const auto &item : j) {
        if (!item.is_string()) { ok = false; return kNoDay; }
        const std::string name = lowercase_ascii(item.get<std::string>());
        bool              found = false;
        for (int d = 0; d < 7; ++d)
            if (name == weekday_short_name(d)) { set |= weekday_bit(d); found = true; }
        if (!found) { ok = false; return kNoDay; }
    }
    return set;
}

inline nlohmann::json weekdays_to_json(WeekdaySet set)
{
    if ((set & kEveryDay) == kEveryDay) return "everyday";
    nlohmann::json arr = nlohmann::json::array();
    for (int d = 0; d < 7; ++d)
        if (weekday_in(set, d)) arr.push_back(weekday_short_name(d));
    return arr;
}

inline nlohmann::json rule_to_json(const Rule &rule)
{
    nlohmann::json j = rule.extra.is_object() ? rule.extra : nlohmann::json::object();
    j["id"]        = rule.id;
    j["label"]     = rule.label;
    j["enabled"]   = rule.enabled;
    if (rule.start_date) j["startDate"] = format_date(*rule.start_date); else j.erase("startDate");
    if (rule.end_date) j["endDate"] = format_date(*rule.end_date); else j.erase("endDate");
    j["startTime"] = format_time(rule.start_time);
    j["endTime"]   = format_time(rule.end_time);
    j["weekdays"]  = weekdays_to_json(rule.weekdays);
    nlohmann::json source = nlohmann::json::object();
    source["kind"]        = source_kind_name(rule.source);
    if (rule.source == SourceKind::Api) {
        source["url"]               = rule.source_url;
        source["allowLoopbackHttp"] = rule.allow_loopback_http;
    } else if (rule.source == SourceKind::HomeAssistant) {
        source["entityId"] = rule.source_entity_id;
    }
    j["source"] = source;
    nlohmann::json values = nlohmann::json::object();
    for (const auto &[key, value] : rule.values)
        values[key] = value;
    j["values"] = values;
    return j;
}

inline std::string serialize_document(const Document &doc)
{
    nlohmann::json j    = doc.extra.is_object() ? doc.extra : nlohmann::json::object();
    j["schemaVersion"]  = kSchemaVersion;
    nlohmann::json rules = nlohmann::json::array();
    for (const Rule &r : doc.rules)
        rules.push_back(rule_to_json(r));
    j["rules"] = rules;
    return j.dump();
}

// Accept only the allowlisted keys with string (or number/bool, stringified)
// values. Unknown keys are dropped, never an error, so an API can carry more
// than this build understands.
inline std::map<std::string, std::string> values_from_json(const nlohmann::json &j, bool &ok)
{
    ok = true;
    std::map<std::string, std::string> values;
    if (j.is_null()) return values;
    if (!j.is_object()) { ok = false; return values; }
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (!is_allowed_key(it.key())) continue;
        std::string value;
        if (it->is_string()) value = it->get<std::string>();
        else if (it->is_number_integer()) value = std::to_string(it->get<long long>());
        else if (it->is_number_float()) {
            char buf[32];
            std::snprintf(buf, sizeof buf, "%g", it->get<double>());
            value = buf;
        } else if (it->is_boolean()) value = it->get<bool>() ? "1" : "0";
        else { ok = false; return {}; }
        values[it.key()] = value;
    }
    return values;
}

inline bool rule_from_json(const nlohmann::json &j, Rule &rule, std::string &error)
{
    if (!j.is_object()) { error = "rule is not an object"; return false; }
    const auto str = [&](const char *key, std::string &out, bool required) {
        auto it = j.find(key);
        if (it == j.end() || it->is_null()) { if (required) { error = std::string("missing ") + key; return false; } return true; }
        if (!it->is_string()) { error = std::string(key) + " must be a string"; return false; }
        out = it->get<std::string>();
        return true;
    };
    if (!str("id", rule.id, true) || !str("label", rule.label, false)) return false;
    if (rule.id.empty() || rule.id.size() > 64) { error = "rule id is empty or too long"; return false; }
    if (auto it = j.find("enabled"); it != j.end()) {
        if (!it->is_boolean()) { error = "enabled must be true or false"; return false; }
        rule.enabled = it->get<bool>();
    }
    std::string text;
    if (!str("startDate", text, false)) return false;
    if (!text.empty()) { rule.start_date = parse_date(text); if (!rule.start_date) { error = "startDate is not YYYY-MM-DD"; return false; } }
    text.clear();
    if (!str("endDate", text, false)) return false;
    if (!text.empty()) { rule.end_date = parse_date(text); if (!rule.end_date) { error = "endDate is not YYYY-MM-DD"; return false; } }
    text.clear();
    if (!str("startTime", text, false)) return false;
    if (!text.empty()) { auto t = parse_time(text); if (!t) { error = "startTime is not HH:MM"; return false; } rule.start_time = *t; }
    text.clear();
    if (!str("endTime", text, false)) return false;
    if (!text.empty()) { auto t = parse_time(text); if (!t) { error = "endTime is not HH:MM"; return false; } rule.end_time = *t; }
    if (auto it = j.find("weekdays"); it != j.end()) {
        bool ok = false;
        rule.weekdays = weekdays_from_json(*it, ok);
        if (!ok) { error = "weekdays must be \"everyday\" or a list of mon..sun"; return false; }
    }
    if (auto it = j.find("source"); it != j.end() && !it->is_null()) {
        if (!it->is_object()) { error = "source must be an object"; return false; }
        std::string kind = "local";
        if (auto k = it->find("kind"); k != it->end()) {
            if (!k->is_string()) { error = "source.kind must be a string"; return false; }
            kind = k->get<std::string>();
        }
        auto parsed = parse_source_kind(kind);
        if (!parsed) { error = "unknown source kind: " + kind; return false; }
        rule.source = *parsed;
        if (auto u = it->find("url"); u != it->end() && u->is_string()) rule.source_url = u->get<std::string>();
        if (auto a = it->find("allowLoopbackHttp"); a != it->end() && a->is_boolean()) rule.allow_loopback_http = a->get<bool>();
        if (auto e = it->find("entityId"); e != it->end() && e->is_string()) rule.source_entity_id = e->get<std::string>();
        if (rule.source_url.size() > kMaxUrlBytes) { error = "source.url is too long"; return false; }
        if (rule.source_entity_id.size() > kMaxEntityIdBytes) { error = "source.entityId is too long"; return false; }
    }
    if (auto it = j.find("values"); it != j.end()) {
        bool ok = false;
        rule.values = values_from_json(*it, ok);
        if (!ok) { error = "values must map setting keys to strings"; return false; }
    }
    // Everything this build does not understand rides along untouched.
    static const char *known[] = {"id", "label", "enabled", "startDate", "endDate", "startTime", "endTime", "weekdays", "source", "values"};
    rule.extra = nlohmann::json::object();
    for (auto it = j.begin(); it != j.end(); ++it) {
        bool is_known = false;
        for (const char *k : known) if (it.key() == k) { is_known = true; break; }
        if (!is_known) rule.extra[it.key()] = it.value();
    }
    return true;
}

inline ParseResult parse_document(const std::string &text)
{
    ParseResult result;
    if (text.empty()) { result.ok = true; return result; } // no schedule yet
    if (text.size() > kMaxDocumentBytes) { result.error = "schedule document is too large"; return result; }
    nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) { result.error = "schedule document is not a JSON object"; return result; }
    auto version = j.find("schemaVersion");
    if (version == j.end() || !version->is_number_integer()) { result.error = "schemaVersion is missing"; return result; }
    const int v = version->get<int>();
    if (v < 1 || v > kSchemaVersion) { result.error = "schemaVersion " + std::to_string(v) + " is not supported by this build"; return result; }
    result.document.schema_version = v;
    if (auto rules = j.find("rules"); rules != j.end() && !rules->is_null()) {
        if (!rules->is_array()) { result.error = "rules must be an array"; return result; }
        if (rules->size() > kMaxRules) { result.error = "too many rules (limit " + std::to_string(kMaxRules) + ")"; return result; }
        for (const auto &rj : *rules) {
            Rule        rule;
            std::string error;
            if (!rule_from_json(rj, rule, error)) { result.error = error; return result; }
            if (result.document.find(rule.id) != nullptr) { result.error = "duplicate rule id " + rule.id; return result; }
            result.document.rules.push_back(std::move(rule));
        }
    }
    result.document.extra = nlohmann::json::object();
    for (auto it = j.begin(); it != j.end(); ++it)
        if (it.key() != "schemaVersion" && it.key() != "rules") result.document.extra[it.key()] = it.value();
    result.ok = true;
    return result;
}

// ---------------------------------------------------------------------------
// API response contract
//   { "schemaVersion": 1, "values": { "<allowed key>": "<value>", ... } }
// ---------------------------------------------------------------------------

struct ApiResponse
{
    bool                               ok = false;
    std::string                        error;
    std::map<std::string, std::string> values; // validated, allowlisted
};

inline ApiResponse parse_api_response(const std::string &body, std::size_t max_bytes = kMaxApiResponseBytes)
{
    ApiResponse response;
    if (body.size() > max_bytes) { response.error = "response is larger than " + std::to_string(max_bytes) + " bytes"; return response; }
    nlohmann::json j = nlohmann::json::parse(body, nullptr, false);
    if (j.is_discarded() || !j.is_object()) { response.error = "response is not a JSON object"; return response; }
    auto version = j.find("schemaVersion");
    if (version == j.end() || !version->is_number_integer() || version->get<int>() != kSchemaVersion) {
        response.error = "response schemaVersion must be " + std::to_string(kSchemaVersion);
        return response;
    }
    auto values = j.find("values");
    if (values == j.end() || !values->is_object()) { response.error = "response has no values object"; return response; }
    bool ok = false;
    auto parsed = values_from_json(*values, ok);
    if (!ok) { response.error = "values must map setting keys to strings"; return response; }
    for (const auto &[key, value] : parsed) {
        const std::string problem = validate_value(key, value);
        if (!problem.empty()) { response.error = key + ": " + problem; return response; }
    }
    response.values = std::move(parsed);
    response.ok     = true;
    return response;
}

// ---------------------------------------------------------------------------
// Matching and precedence
// ---------------------------------------------------------------------------

inline bool date_in_bounds(const Rule &rule, const LocalDate &day)
{
    if (rule.start_date && day < *rule.start_date) return false;
    if (rule.end_date && *rule.end_date < day) return false;
    return true;
}

// The calendar day this rule's window started for `now`, if `now` is inside
// the window at all; nullopt when the time of day does not match.
inline std::optional<LocalDate> window_start_day(const Rule &rule, const LocalInstant &now)
{
    const int t     = now.time.minutes_of_day();
    const int start = rule.start_time.minutes_of_day();
    const int end   = rule.end_time.minutes_of_day();
    if (rule.all_day()) return now.date;
    if (start < end) return (t >= start && t < end) ? std::optional<LocalDate>(now.date) : std::nullopt;
    // Crosses midnight.
    if (t >= start) return now.date;
    if (t < end) return previous_day(now.date);
    return std::nullopt;
}

// Enabled + weekday + time window + date bounds. Source activity is layered on
// top by resolve(); this is the pure calendar answer.
inline bool rule_matches_calendar(const Rule &rule, const LocalInstant &now)
{
    if (!rule.enabled) return false;
    const auto start_day = window_start_day(rule, now);
    if (!start_day) return false;
    if (!weekday_in(rule.weekdays, iso_weekday(*start_day))) return false;
    return date_in_bounds(rule, *start_day);
}

// What the evaluator knows about a rule's external source right now.
struct SourceState
{
    enum class Kind { Active, Inactive, Unknown } kind = Kind::Unknown;
    std::map<std::string, std::string> values; // Api: the validated remote values
};

struct Resolution
{
    std::map<std::string, std::string> values;         // key -> winning value
    std::map<std::string, std::string> winning_rule;   // key -> rule id
    std::vector<std::string>           active_rule_ids; // in list order
};

// Later matching rules win per key. A rule whose source is Unknown (no answer
// yet, or the last fetch failed) contributes nothing, so a flaky API never
// flips settings back and forth on its own.
template <typename SourceLookup>
Resolution resolve(const Document &doc, const LocalInstant &now, SourceLookup &&lookup)
{
    Resolution out;
    for (const Rule &rule : doc.rules) {
        if (!rule_matches_calendar(rule, now)) continue;
        std::map<std::string, std::string> contribution;
        if (rule.source == SourceKind::Local) {
            contribution = rule.values;
        } else {
            const SourceState state = lookup(rule);
            if (state.kind != SourceState::Kind::Active) continue;
            contribution = rule.source == SourceKind::Api ? state.values : rule.values;
            if (rule.source == SourceKind::Api) {
                // Local values on an API rule are the fallback for keys the
                // response left out, never an override of what it sent.
                for (const auto &[key, value] : rule.values)
                    contribution.emplace(key, value);
            }
        }
        if (contribution.empty()) continue;
        out.active_rule_ids.push_back(rule.id);
        for (const auto &[key, value] : contribution) {
            if (!is_allowed_key(key) || !validate_value(key, value).empty()) continue;
            out.values[key]       = value;
            out.winning_rule[key] = rule.id;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Override bookkeeping: what to write so the live settings equal the
// resolution, and what to restore when a rule stops matching.
// ---------------------------------------------------------------------------

struct OverrideState
{
    // The user's own values, captured the first time a key was overridden and
    // restored when no rule claims the key any more. Remote or scheduled values
    // are never written here.
    std::map<std::string, std::string> base;
    // Keys currently under schedule control, with the value last applied.
    std::map<std::string, std::string> applied;
};

struct Transition
{
    std::map<std::string, std::string> writes;   // key -> value to apply now
    std::vector<std::string>           released; // keys restored to base
    OverrideState                      next;
};

// `live` is the current AppConfig view of the allowed keys. A key already under
// control keeps its captured base; a key newly claimed captures the live value
// (which is the user's own, since nothing scheduled has touched it yet).
inline Transition plan_transition(const OverrideState &state, const Resolution &resolution, const std::map<std::string, std::string> &live)
{
    Transition t;
    t.next = state;
    for (const auto &[key, value] : resolution.values) {
        if (t.next.base.find(key) == t.next.base.end()) {
            auto it = live.find(key);
            t.next.base[key] = it == live.end() ? std::string() : it->second;
        }
        auto applied = state.applied.find(key);
        const auto live_it = live.find(key);
        const std::string live_value = live_it == live.end() ? std::string() : live_it->second;
        if (applied == state.applied.end() || applied->second != value || live_value != value)
            t.writes[key] = value;
        t.next.applied[key] = value;
    }
    for (const auto &[key, value] : state.applied) {
        if (resolution.values.find(key) != resolution.values.end()) continue;
        auto base = state.base.find(key);
        t.writes[key] = base == state.base.end() ? std::string() : base->second;
        t.released.push_back(key);
        t.next.applied.erase(key);
        t.next.base.erase(key);
    }
    return t;
}

inline nlohmann::json override_state_to_json(const OverrideState &s)
{
    nlohmann::json j = nlohmann::json::object();
    j["base"]        = nlohmann::json::object();
    j["applied"]     = nlohmann::json::object();
    for (const auto &[k, v] : s.base) j["base"][k] = v;
    for (const auto &[k, v] : s.applied) j["applied"][k] = v;
    return j;
}

inline OverrideState override_state_from_json(const std::string &text)
{
    OverrideState s;
    if (text.empty()) return s;
    nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return s;
    const auto read = [&](const char *name, std::map<std::string, std::string> &out) {
        auto it = j.find(name);
        if (it == j.end() || !it->is_object()) return;
        for (auto v = it->begin(); v != it->end(); ++v)
            if (v->is_string() && is_allowed_key(v.key())) out[v.key()] = v->get<std::string>();
    };
    read("base", s.base);
    read("applied", s.applied);
    return s;
}

// ---------------------------------------------------------------------------
// Generation guard for asynchronous sources: an answer is accepted only when
// it belongs to the newest request that was issued for that rule.
// ---------------------------------------------------------------------------

class GenerationGuard
{
public:
    std::uint64_t issue()
    {
        return ++m_latest;
    }
    bool is_current(std::uint64_t generation) const { return generation == m_latest; }
    std::uint64_t latest() const { return m_latest; }

private:
    std::uint64_t m_latest = 0;
};

// ---------------------------------------------------------------------------
// Human-readable summary of a rule's window for the list row.
// ---------------------------------------------------------------------------

inline std::string describe_window(const Rule &rule)
{
    std::string text;
    if ((rule.weekdays & kEveryDay) == kEveryDay) text = "every day";
    else {
        for (int d = 0; d < 7; ++d)
            if (weekday_in(rule.weekdays, d)) { if (!text.empty()) text += ' '; text += weekday_short_name(d); }
    }
    if (rule.all_day()) text += ", all day";
    else text += ", " + format_time(rule.start_time) + "-" + format_time(rule.end_time) + (rule.crosses_midnight() ? " (next day)" : "");
    if (rule.start_date || rule.end_date) {
        text += ", " + (rule.start_date ? format_date(*rule.start_date) : std::string("...")) + " to " +
                (rule.end_date ? format_date(*rule.end_date) : std::string("..."));
    }
    return text;
}

} } } // namespace Slic3r::GUI::Schedule

#endif // slic3r_GUI_Schedule_ScheduledSettingsModel_hpp_
