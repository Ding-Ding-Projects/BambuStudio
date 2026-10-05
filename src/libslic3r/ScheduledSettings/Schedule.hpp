#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Slic3r::ScheduledSettings {
using Value = std::variant<bool, int64_t, double, std::string>;
using Values = std::map<std::string, Value>;
enum class Kind { Boolean, Integer, Number, Text, Choice, Color };
struct Descriptor {
    std::string key, label_en, label_zh;
    Kind kind = Kind::Text;
    double minimum = 0, maximum = 0;
    std::vector<std::string> choices;
    size_t max_length = 256;
};
class Registry {
public:
    void add(Descriptor descriptor);
    const std::map<std::string, Descriptor>& entries() const { return m_entries; }
    bool validate(const Values&, std::string& error) const;
private:
    std::map<std::string, Descriptor> m_entries;
};
Registry default_registry();
struct Date { int year=1970, month=1, day=1; };
bool valid_date(Date);
int64_t day_number(Date);
Date date_from_day(int64_t);
int weekday(Date); // Monday = 0.
struct LocalTime { Date date; int minute = 0; };
struct Window {
    std::optional<Date> first, last;
    int start_minute=0, end_minute=0;
    unsigned weekdays=127;
};
enum class SourceKind { Local, HttpsApi, HomeAssistant };
struct Source {
    SourceKind kind=SourceKind::Local;
    std::string url, entity;
    bool consent=false, allow_private_network=false, loopback_development=false;
    int refresh_seconds=300;
};
struct Rule {
    std::string id, label;
    bool enabled=true;
    int priority=0;
    Window window;
    Values values;
    Source source;
};
struct Schedule { int version=1; std::string timezone="system"; std::vector<Rule> rules; };
constexpr size_t max_payload=65536, max_rules=128;
bool validate(const Schedule&, const Registry&, std::string& error);
bool matches(const Window&, LocalTime);
std::string serialize(const Schedule&);
bool parse(const std::string&, const Registry&, Schedule&, std::string& error);
// Duplicate fields, unknown fields, unsafe values and partial documents are rejected.
bool parse_api_response(const std::string&, const Registry&, Values&, std::string& error);
bool parse_home_state(const std::string&, const std::string& entity, bool& on, std::string& error);
struct Endpoint { std::string scheme, host, path; unsigned port=443; };
bool endpoint(const Source&, Endpoint&, std::string& error);
// No DNS names are trusted by this check. Apply to every resolved address and pin the result.
bool allowed_address(const std::string& numeric_address, const Source&);
enum class State { Local, Waiting, Active, Inactive, Offline, Invalid, Unauthorized, RateLimited, Stale, Unavailable };
struct ExternalValue { State state=State::Waiting; Values values; bool on=false; int64_t expires_at=0; };
struct Effective { Values values; std::map<std::string,std::string> winners; std::map<std::string,State> sources; };
Effective evaluate(const Schedule&, const Registry&, const Values& base, LocalTime,
                   int64_t monotonic_seconds, const std::map<std::string,ExternalValue>&);
struct FetchResult { State state=State::Unavailable; std::string body; };
using Fetch = std::function<FetchResult(const Source&, const std::string& rule_id, const std::atomic_bool& cancel)>;
// A fetch implementation is privileged: it must enforce the endpoint, DNS pinning, TLS,
// 64 KiB response limit, no redirects, ten-second deadline and credential-vault contract.
}
