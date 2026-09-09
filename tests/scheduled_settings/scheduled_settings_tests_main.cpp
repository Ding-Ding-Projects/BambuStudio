#include <catch_main.hpp>

#include "slic3r/GUI/Schedule/ScheduledSettingsModel.hpp"

#include <string>

using namespace Slic3r::GUI::Schedule;

namespace {

LocalInstant at(int y, int m, int d, int hh, int mm)
{
    LocalInstant i;
    i.date = {y, m, d};
    i.time = {hh, mm};
    return i;
}

Rule dark_rule()
{
    Rule r;
    r.id         = "r-00000001";
    r.label      = "Evening dark";
    r.start_time = {20, 0};
    r.end_time   = {7, 0};
    r.weekdays   = kEveryDay;
    r.values[kKeyTheme] = "1";
    return r;
}

SourceState local_only(const Rule &) { return SourceState{SourceState::Kind::Unknown, {}}; }

} // namespace

// 2026-09-08 is a Tuesday.
TEST_CASE("iso_weekday is Monday-based", "[calendar]")
{
    CHECK(iso_weekday({2026, 9, 7}) == 0);  // Monday
    CHECK(iso_weekday({2026, 9, 8}) == 1);  // Tuesday
    CHECK(iso_weekday({2026, 9, 13}) == 6); // Sunday
    CHECK(iso_weekday({2024, 2, 29}) == 3); // Thursday, leap day
    CHECK(previous_day({2026, 3, 1}) == LocalDate{2026, 2, 28});
    CHECK(previous_day({2024, 3, 1}) == LocalDate{2024, 2, 29});
    CHECK(previous_day({2026, 1, 1}) == LocalDate{2025, 12, 31});
}

TEST_CASE("strict ISO date and time parsing", "[calendar]")
{
    CHECK(parse_date("2026-09-08").has_value());
    CHECK_FALSE(parse_date("2026-9-8").has_value());
    CHECK_FALSE(parse_date("2026-02-30").has_value());
    CHECK_FALSE(parse_date("").has_value());
    CHECK(parse_time("07:30").has_value());
    CHECK_FALSE(parse_time("7:30").has_value());
    CHECK_FALSE(parse_time("24:00").has_value());
    CHECK(format_date({2026, 1, 5}) == "2026-01-05");
    CHECK(format_time({7, 5}) == "07:05");
}

TEST_CASE("schema round trip preserves unknown fields", "[schema]")
{
    const std::string text = R"({
        "schemaVersion": 1,
        "futureTopLevel": {"keep": true},
        "rules": [{
            "id": "r-abc", "label": "Night", "enabled": false,
            "startDate": "2026-09-01", "endDate": "2026-09-30",
            "startTime": "22:00", "endTime": "06:00",
            "weekdays": ["mon", "fri"],
            "source": {"kind": "homeAssistant", "entityId": "input_boolean.night"},
            "values": {"dark_color_mode": "1", "not_allowed_key": "x", "ui_density": "compact"},
            "futureRuleField": 42
        }]
    })";
    ParseResult parsed = parse_document(text);
    REQUIRE(parsed.ok);
    REQUIRE(parsed.document.rules.size() == 1);
    const Rule &r = parsed.document.rules[0];
    CHECK(r.id == "r-abc");
    CHECK_FALSE(r.enabled);
    CHECK(r.start_date == LocalDate{2026, 9, 1});
    CHECK(r.end_date == LocalDate{2026, 9, 30});
    CHECK(r.start_time == LocalTime{22, 0});
    CHECK(r.weekdays == (weekday_bit(0) | weekday_bit(4)));
    CHECK(r.source == SourceKind::HomeAssistant);
    CHECK(r.source_entity_id == "input_boolean.night");
    CHECK(r.values.size() == 2); // the unknown key is dropped, never stored
    CHECK(r.values.count("not_allowed_key") == 0);
    CHECK(r.extra["futureRuleField"] == 42);

    const std::string out = serialize_document(parsed.document);
    nlohmann::json    j   = nlohmann::json::parse(out);
    CHECK(j["futureTopLevel"]["keep"] == true);
    CHECK(j["rules"][0]["futureRuleField"] == 42);
    CHECK(j["rules"][0]["weekdays"] == nlohmann::json::array({"mon", "fri"}));
    CHECK(j["schemaVersion"] == 1);

    // Second pass is identical.
    ParseResult again = parse_document(out);
    REQUIRE(again.ok);
    CHECK(serialize_document(again.document) == out);
}

TEST_CASE("schema rejects what it cannot trust", "[schema]")
{
    CHECK(parse_document("").ok); // empty = no schedule
    CHECK_FALSE(parse_document("not json").ok);
    CHECK_FALSE(parse_document(R"({"rules": []})").ok);                   // no version
    CHECK_FALSE(parse_document(R"({"schemaVersion": 2, "rules": []})").ok); // newer than this build
    CHECK_FALSE(parse_document(R"({"schemaVersion": 1, "rules": [{"id": "a"}, {"id": "a"}]})").ok);
    CHECK_FALSE(parse_document(R"({"schemaVersion": 1, "rules": [{"id": "a", "startTime": "9am"}]})").ok);
    CHECK_FALSE(parse_document(R"({"schemaVersion": 1, "rules": [{"id": "a", "weekdays": ["funday"]}]})").ok);
    CHECK_FALSE(parse_document(R"({"schemaVersion": 1, "rules": [{"id": "a", "source": {"kind": "carrier pigeon"}}]})").ok);
    std::string big(kMaxDocumentBytes + 1, ' ');
    CHECK_FALSE(parse_document(big).ok);
    CHECK(parse_document(R"({"schemaVersion": 1, "rules": [{"id": "a", "weekdays": "everyday"}]})").document.rules[0].weekdays == kEveryDay);
}

TEST_CASE("value validation per allowed key", "[schema]")
{
    CHECK(validate_value(kKeyTheme, "1").empty());
    CHECK_FALSE(validate_value(kKeyTheme, "dark").empty());
    CHECK(validate_value(kKeyDensity, "compact").empty());
    CHECK_FALSE(validate_value(kKeyDensity, "dense").empty());
    CHECK(validate_value(kKeyAccentSeed, "#146c2e").empty());
    CHECK(validate_value(kKeyAccentSeed, "").empty());
    CHECK_FALSE(validate_value(kKeyAccentSeed, "146c2e").empty());
    CHECK_FALSE(validate_value(kKeyAccentSeed, "#zzzzzz").empty());
    CHECK(validate_value(kKeyFontScale, "1.15").empty());
    CHECK_FALSE(validate_value(kKeyFontScale, "3").empty());
    CHECK_FALSE(validate_value(kKeyFontScale, "abc").empty());
    CHECK(validate_value(kKeyFunnyLevelEnglish, "5").empty());
    CHECK_FALSE(validate_value(kKeyFunnyLevelEnglish, "6").empty());
    CHECK(validate_value(kKeyLanguageMode, "bilingual_en_yue_HK").empty());
    CHECK_FALSE(validate_value(kKeyLanguageMode, "en US").empty());
    CHECK(validate_value(kKeyDisplayName, "My Slicer").empty());
    CHECK_FALSE(validate_value(kKeyDisplayName, std::string(41, 'a')).empty());
    CHECK_FALSE(validate_value(kKeyDisplayName, "a\nb").empty());
    CHECK_FALSE(validate_value("ha_token", "x").empty());
}

TEST_CASE("weekday sets and every day", "[matching]")
{
    Rule r = dark_rule();
    r.start_time = {9, 0};
    r.end_time   = {17, 0};
    r.weekdays   = weekday_bit(0) | weekday_bit(2); // Mon, Wed
    CHECK(rule_matches_calendar(r, at(2026, 9, 7, 10, 0)));       // Monday
    CHECK_FALSE(rule_matches_calendar(r, at(2026, 9, 8, 10, 0))); // Tuesday
    CHECK(rule_matches_calendar(r, at(2026, 9, 9, 10, 0)));       // Wednesday
    r.weekdays = kEveryDay;
    CHECK(rule_matches_calendar(r, at(2026, 9, 8, 10, 0)));
    CHECK(rule_matches_calendar(r, at(2026, 9, 13, 10, 0))); // Sunday
    r.weekdays = kNoDay;
    CHECK_FALSE(rule_matches_calendar(r, at(2026, 9, 8, 10, 0)));
    r.weekdays = kEveryDay;
    r.enabled  = false;
    CHECK_FALSE(rule_matches_calendar(r, at(2026, 9, 8, 10, 0)));
}

TEST_CASE("same-day window is start-inclusive and end-exclusive", "[matching]")
{
    Rule r       = dark_rule();
    r.start_time = {9, 0};
    r.end_time   = {17, 0};
    CHECK_FALSE(rule_matches_calendar(r, at(2026, 9, 8, 8, 59)));
    CHECK(rule_matches_calendar(r, at(2026, 9, 8, 9, 0)));
    CHECK(rule_matches_calendar(r, at(2026, 9, 8, 16, 59)));
    CHECK_FALSE(rule_matches_calendar(r, at(2026, 9, 8, 17, 0)));
}

TEST_CASE("cross-midnight window belongs to the day it started", "[matching]")
{
    Rule r = dark_rule(); // 20:00 -> 07:00
    r.weekdays = weekday_bit(1); // Tuesday only
    CHECK(r.crosses_midnight());
    CHECK(rule_matches_calendar(r, at(2026, 9, 8, 20, 0)));       // Tue 20:00
    CHECK(rule_matches_calendar(r, at(2026, 9, 8, 23, 59)));
    CHECK(rule_matches_calendar(r, at(2026, 9, 9, 0, 0)));        // Wed 00:00, window started Tue
    CHECK(rule_matches_calendar(r, at(2026, 9, 9, 6, 59)));
    CHECK_FALSE(rule_matches_calendar(r, at(2026, 9, 9, 7, 0)));  // window closed
    CHECK_FALSE(rule_matches_calendar(r, at(2026, 9, 9, 20, 0))); // Wed evening: not a listed day
    CHECK_FALSE(rule_matches_calendar(r, at(2026, 9, 8, 6, 0)));  // Tue morning belongs to Monday's window
    CHECK_FALSE(rule_matches_calendar(r, at(2026, 9, 8, 12, 0)));
}

TEST_CASE("date bounds are inclusive and follow the window start day", "[matching]")
{
    Rule r       = dark_rule();
    r.start_date = LocalDate{2026, 9, 10};
    r.end_date   = LocalDate{2026, 9, 12};
    CHECK_FALSE(rule_matches_calendar(r, at(2026, 9, 9, 21, 0)));
    CHECK(rule_matches_calendar(r, at(2026, 9, 10, 21, 0)));
    CHECK(rule_matches_calendar(r, at(2026, 9, 12, 21, 0)));
    CHECK(rule_matches_calendar(r, at(2026, 9, 13, 3, 0)));        // started on the 12th
    CHECK_FALSE(rule_matches_calendar(r, at(2026, 9, 13, 21, 0))); // past the end date
    r.start_date.reset();
    CHECK(rule_matches_calendar(r, at(2020, 1, 1, 21, 0)));
    r.end_date.reset();
    CHECK(rule_matches_calendar(r, at(2999, 1, 1, 21, 0)));
}

TEST_CASE("equal start and end means all day", "[matching]")
{
    Rule r       = dark_rule();
    r.start_time = {8, 0};
    r.end_time   = {8, 0};
    CHECK(r.all_day());
    CHECK(rule_matches_calendar(r, at(2026, 9, 8, 0, 0)));
    CHECK(rule_matches_calendar(r, at(2026, 9, 8, 7, 59)));
    CHECK(rule_matches_calendar(r, at(2026, 9, 8, 8, 0)));
    CHECK(rule_matches_calendar(r, at(2026, 9, 8, 23, 59)));
    r.weekdays = weekday_bit(0);
    CHECK_FALSE(rule_matches_calendar(r, at(2026, 9, 8, 12, 0)));
}

TEST_CASE("empty schedule resolves to nothing", "[precedence]")
{
    Document   doc;
    Resolution res = resolve(doc, at(2026, 9, 8, 12, 0), local_only);
    CHECK(res.values.empty());
    CHECK(res.active_rule_ids.empty());
}

TEST_CASE("later rule in the list wins per key", "[precedence]")
{
    Document doc;
    Rule a       = dark_rule();
    a.id         = "a";
    a.start_time = {0, 0};
    a.end_time   = {0, 0};
    a.values     = {{kKeyTheme, "1"}, {kKeyDensity, "compact"}};
    Rule b       = a;
    b.id         = "b";
    b.values     = {{kKeyTheme, "0"}};
    doc.rules    = {a, b};
    Resolution res = resolve(doc, at(2026, 9, 8, 12, 0), local_only);
    CHECK(res.values[kKeyTheme] == "0");
    CHECK(res.values[kKeyDensity] == "compact");
    CHECK(res.winning_rule[kKeyTheme] == "b");
    CHECK(res.winning_rule[kKeyDensity] == "a");
    REQUIRE(res.active_rule_ids.size() == 2);
    CHECK(res.active_rule_ids[0] == "a");

    // Swap the list order and the answer flips: order is the priority.
    doc.rules = {b, a};
    res       = resolve(doc, at(2026, 9, 8, 12, 0), local_only);
    CHECK(res.values[kKeyTheme] == "1");
}

TEST_CASE("home assistant source gates the rule's values", "[precedence][home-assistant]")
{
    Document doc;
    Rule r             = dark_rule();
    r.start_time       = {0, 0};
    r.end_time         = {0, 0};
    r.source           = SourceKind::HomeAssistant;
    r.source_entity_id = "input_boolean.night";
    doc.rules          = {r};
    const LocalInstant now = at(2026, 9, 8, 12, 0);

    auto on  = [](const Rule &) { return SourceState{SourceState::Kind::Active, {}}; };
    auto off = [](const Rule &) { return SourceState{SourceState::Kind::Inactive, {}}; };
    auto unknown = [](const Rule &) { return SourceState{SourceState::Kind::Unknown, {}}; };
    CHECK(resolve(doc, now, on).values[kKeyTheme] == "1");
    CHECK(resolve(doc, now, off).values.empty());
    CHECK(resolve(doc, now, unknown).values.empty()); // no answer (missing token, offline) = no override
}

TEST_CASE("api source supplies values with local fallback and validation", "[precedence][api]")
{
    Document doc;
    Rule r       = dark_rule();
    r.start_time = {0, 0};
    r.end_time   = {0, 0};
    r.source     = SourceKind::Api;
    r.source_url = "https://example.test/settings";
    r.values     = {{kKeyDensity, "compact"}, {kKeyTheme, "0"}};
    doc.rules    = {r};
    const LocalInstant now = at(2026, 9, 8, 12, 0);
    auto api = [](const Rule &) {
        SourceState s;
        s.kind   = SourceState::Kind::Active;
        s.values = {{kKeyTheme, "1"}, {kKeyFunnyLevelEnglish, "9"}};
        return s;
    };
    Resolution res = resolve(doc, now, api);
    CHECK(res.values[kKeyTheme] == "1");        // remote wins over the rule's own value
    CHECK(res.values[kKeyDensity] == "compact"); // local fallback for a key the API left out
    CHECK(res.values.count(kKeyFunnyLevelEnglish) == 0); // invalid remote value dropped
}

TEST_CASE("generation guard rejects stale answers", "[generation]")
{
    GenerationGuard guard;
    const auto      first  = guard.issue();
    const auto      second = guard.issue();
    CHECK_FALSE(guard.is_current(first));
    CHECK(guard.is_current(second));
    CHECK(guard.latest() == second);
}

TEST_CASE("api url policy", "[api]")
{
    CHECK(validate_api_url("https://example.test/v1/settings", false).empty());
    CHECK_FALSE(validate_api_url("", false).empty());
    CHECK_FALSE(validate_api_url("ftp://example.test/x", false).empty());
    CHECK_FALSE(validate_api_url("http://example.test/x", false).empty());
    CHECK_FALSE(validate_api_url("http://example.test/x", true).empty()); // dev flag never opens LAN http
    CHECK_FALSE(validate_api_url("http://127.0.0.1:8000/x", false).empty());
    CHECK(validate_api_url("http://127.0.0.1:8000/x", true).empty());
    CHECK(validate_api_url("http://localhost/x", true).empty());
    CHECK_FALSE(validate_api_url("https://user:pw@example.test/x", false).empty());
    CHECK_FALSE(validate_api_url("https://exa mple.test/x", false).empty());
    CHECK_FALSE(validate_api_url("https://", false).empty());
    CHECK_FALSE(validate_api_url("https://" + std::string(kMaxUrlBytes, 'a'), false).empty());
}

TEST_CASE("api response validation", "[api]")
{
    ApiResponse ok = parse_api_response(R"({"schemaVersion": 1, "values": {"dark_color_mode": "1", "ui_density": "compact", "ha_token": "leak"}})");
    REQUIRE(ok.ok);
    CHECK(ok.values.size() == 2);
    CHECK(ok.values.count("ha_token") == 0);

    CHECK_FALSE(parse_api_response("[]").ok);
    CHECK_FALSE(parse_api_response(R"({"values": {}})").ok);                       // no version
    CHECK_FALSE(parse_api_response(R"({"schemaVersion": 2, "values": {}})").ok);   // wrong version
    CHECK_FALSE(parse_api_response(R"({"schemaVersion": 1})").ok);                 // no values
    CHECK_FALSE(parse_api_response(R"({"schemaVersion": 1, "values": {"dark_color_mode": "purple"}})").ok);
    CHECK_FALSE(parse_api_response(R"({"schemaVersion": 1, "values": {"dark_color_mode": [1]}})").ok);
    std::string big = R"({"schemaVersion": 1, "values": {"ui_font_family": ")" + std::string(kMaxApiResponseBytes, 'a') + "\"}}";
    CHECK_FALSE(parse_api_response(big).ok);
    // Numbers and booleans are stringified the way AppConfig stores them.
    ApiResponse typed = parse_api_response(R"({"schemaVersion": 1, "values": {"funny_level_en": 3, "dark_color_mode": true}})");
    REQUIRE(typed.ok);
    CHECK(typed.values["funny_level_en"] == "3");
    CHECK(typed.values["dark_color_mode"] == "1");
}

TEST_CASE("override transition captures base once and restores it", "[override]")
{
    OverrideState state;
    std::map<std::string, std::string> live = {{kKeyTheme, "0"}, {kKeyDensity, "comfortable"}};

    Resolution night;
    night.values = {{kKeyTheme, "1"}};
    Transition t1 = plan_transition(state, night, live);
    CHECK(t1.writes == std::map<std::string, std::string>{{kKeyTheme, "1"}});
    CHECK(t1.next.base[kKeyTheme] == "0");
    CHECK(t1.next.applied[kKeyTheme] == "1");
    CHECK(t1.released.empty());
    live[kKeyTheme] = "1";

    // Same resolution again: nothing to write, base untouched.
    Transition t2 = plan_transition(t1.next, night, live);
    CHECK(t2.writes.empty());
    CHECK(t2.next.base[kKeyTheme] == "0");

    // A second rule adds density; theme's base stays the user's original.
    Resolution both;
    both.values = {{kKeyTheme, "1"}, {kKeyDensity, "compact"}};
    Transition t3 = plan_transition(t2.next, both, live);
    CHECK(t3.writes == std::map<std::string, std::string>{{kKeyDensity, "compact"}});
    CHECK(t3.next.base[kKeyDensity] == "comfortable");
    live[kKeyDensity] = "compact";

    // Everything ends: both keys go back to base and the bookkeeping empties.
    Resolution none;
    Transition t4 = plan_transition(t3.next, none, live);
    CHECK(t4.writes == std::map<std::string, std::string>{{kKeyTheme, "0"}, {kKeyDensity, "comfortable"}});
    CHECK(t4.released.size() == 2);
    CHECK(t4.next.base.empty());
    CHECK(t4.next.applied.empty());
}

TEST_CASE("override state survives a JSON round trip and ignores junk", "[override]")
{
    OverrideState s;
    s.base    = {{kKeyTheme, "0"}};
    s.applied = {{kKeyTheme, "1"}};
    OverrideState back = override_state_from_json(override_state_to_json(s).dump());
    CHECK(back.base == s.base);
    CHECK(back.applied == s.applied);
    OverrideState junk = override_state_from_json(R"({"base": {"ha_token": "x"}, "applied": 5})");
    CHECK(junk.base.empty());
    CHECK(junk.applied.empty());
    CHECK(override_state_from_json("").base.empty());
}

TEST_CASE("rule validation speaks plainly", "[schema]")
{
    Rule r;
    auto problems = validate_rule(r);
    CHECK_FALSE(problems.empty()); // no label, no values
    r.label  = "x";
    r.values = {{kKeyTheme, "1"}};
    CHECK(validate_rule(r).empty());
    r.weekdays = kNoDay;
    CHECK(validate_rule(r).size() == 1);
    r.weekdays   = kEveryDay;
    r.start_date = LocalDate{2026, 9, 10};
    r.end_date   = LocalDate{2026, 9, 1};
    CHECK(validate_rule(r).size() == 1);
    r.end_date.reset();
    r.source = SourceKind::HomeAssistant;
    CHECK(validate_rule(r).size() == 1); // entity id missing
    CHECK(unique_rule_id(Document{}, 0x1234) == "r-00001234");
    Document doc;
    Rule     taken;
    taken.id = "r-00001234";
    doc.rules.push_back(taken);
    CHECK(unique_rule_id(doc, 0x1234) == "r-00001235");
    CHECK(describe_window(dark_rule()) == "every day, 20:00-07:00 (next day)");
}
