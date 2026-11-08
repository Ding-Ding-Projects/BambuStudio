// Standalone assertions for the Preferences change record and the provenance
// rules (src/slic3r/GUI/PreferencesRemake/PreferenceHistory.cpp). Built and run
// by tests/preference_history.test.mjs, which also builds it with
// SLIC3R_PREFERENCE_HISTORY_NEGATIVE_FIXTURE: in that build no key counts as
// secret, and the redaction checks below must fail.
#include "src/slic3r/GUI/PreferencesRemake/PreferenceHistory.cpp"

#include <cstdio>
#include <fstream>
#include <sstream>

using namespace Slic3r::GUI::PreferenceHistory;
namespace fs = std::filesystem;

static int g_failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                           \
        }                                                                           \
    } while (0)

static std::string slurp(const fs::path &file)
{
    std::ifstream in(file, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static std::size_t line_count(const fs::path &file)
{
    std::ifstream in(file, std::ios::binary);
    std::size_t   n = 0;
    std::string   line;
    while (std::getline(in, line))
        if (!line.empty()) ++n;
    return n;
}

static void test_sensitive_keys()
{
    for (const char *key : {"status_api_token", "lan_drop_station_key", "lan_drop_code", "printer_password", "Some_API_KEY",
                            "cloud_secret", "rtsp_access_code"})
        CHECK(is_sensitive_key(key));
    for (const char *key : {"dark_color_mode", "language", "backup_interval", "narrator_enabled", "ui_accent_seed", "app_logo"})
        CHECK(!is_sensitive_key(key));
}

static void test_diff()
{
    const std::map<std::string, std::string> before = {{"a", "1"}, {"b", "2"}, {"status_api_token", "old-secret"}};
    const std::map<std::string, std::string> after  = {{"a", "1"}, {"b", "3"}, {"c", "x"}, {"status_api_token", "new-secret"}};
    const std::vector<Change> changes = diff(before, after, 1700000000);
    CHECK(changes.size() == 3);
    if (changes.size() != 3) return;
    CHECK(changes[0].key == "b" && changes[0].from == "2" && changes[0].to == "3" && !changes[0].redacted);
    CHECK(changes[1].key == "c" && changes[1].from.empty() && changes[1].to == "x");
    CHECK(changes[2].key == "status_api_token" && changes[2].redacted);
    CHECK(changes[2].from.empty() && changes[2].to.empty());
    CHECK(changes[0].time == 1700000000);
    CHECK(diff(after, after, 1).empty());
}

static void test_lines()
{
    Change c{1700000123, "dark_color_mode", "0", "1", false};
    const std::string line = to_line(c);
    CHECK(line.find('\n') == std::string::npos);
    const auto back = from_line(line);
    CHECK(back && back->time == c.time && back->key == c.key && back->from == "0" && back->to == "1" && !back->redacted);

    Change secret{1700000124, "status_api_token", "abc", "def", false};
    secret.redacted = true;
    secret.from.clear();
    secret.to.clear();
    const std::string secret_line = to_line(secret);
    CHECK(secret_line.find("abc") == std::string::npos && secret_line.find("def") == std::string::npos);
    const auto secret_back = from_line(secret_line);
    CHECK(secret_back && secret_back->redacted && secret_back->from.empty());

    // A hand-edited line that carries values for a secret key loses them.
    const auto forced = from_line(R"({"t":1,"k":"status_api_token","f":"abc","v":"def"})");
    CHECK(forced && forced->redacted && forced->from.empty() && forced->to.empty());

    for (const char *bad : {"", "not json", "[]", R"({"t":"x","k":"a","f":"","v":""})", R"({"t":1,"k":"","f":"","v":""})",
                            R"({"t":1,"k":"a"})", R"({"t":1,"k":"a","f":1,"v":""})", R"({"k":"a","f":"","v":""})"})
        CHECK(!from_line(bad));
}

static void test_store()
{
    const fs::path dir = fs::temp_directory_path() / ("bambustudio-prefs-history-" + std::to_string(std::rand()));
    fs::remove_all(dir);
    const fs::path file = dir / "preferences-history.jsonl";
    {
        Store store(file);
        store.load(); // missing file: empty, not an error
        CHECK(store.changes().empty());
        CHECK(store.append({{10, "language", "en", "yue", false}, {11, "status_api_token", "abc", "def", false}}));
        CHECK(store.append({{12, "language", "yue", "en", false}}));
        CHECK(store.changes().size() == 3);
        CHECK(store.changes()[1].redacted);
    }
    const std::string text = slurp(file);
    CHECK(text.find("abc") == std::string::npos && text.find("def") == std::string::npos);
    {
        // CRLF endings and junk lines are tolerated.
        std::ofstream out(file, std::ios::binary | std::ios::app);
        out << "garbage\r\n" << to_line({13, "dark_color_mode", "0", "1", false}) << "\r\n";
    }
    Store store(file);
    store.load();
    CHECK(store.changes().size() == 4);
    const auto newest = store.newest(2);
    CHECK(newest.size() == 2 && newest[0].time == 13 && newest[1].time == 12);
    const auto lang = store.for_key("language", 5);
    CHECK(lang.size() == 2 && lang[0].to == "en" && lang[1].to == "yue");
    CHECK(store.last_for_key("language") && store.last_for_key("language")->time == 12);
    CHECK(!store.last_for_key("never_changed"));

    // Compaction keeps the newest kKeep entries and rewrites the file.
    std::vector<Change> many;
    for (std::size_t i = 0; i < kCompactAt; ++i) many.push_back({std::int64_t(100 + i), "backup_interval", "0", std::to_string(i), false});
    CHECK(store.append(many));
    CHECK(store.changes().size() == kKeep);
    CHECK(store.changes().back().to == std::to_string(kCompactAt - 1));
    CHECK(line_count(file) == kKeep);
    Store reloaded(file);
    reloaded.load();
    CHECK(reloaded.changes().size() == kKeep);
    fs::remove_all(dir);
}

static void test_describe()
{
    using O = std::optional<std::string>;
    const std::optional<Change> none;
    CHECK(describe(O("1"), O("0"), O("2"), none).origin == Origin::Scheduled);
    CHECK(describe(O("1"), O("0"), O("2"), none).value == "2");
    CHECK(describe(std::nullopt, O("0"), std::nullopt, none).origin == Origin::BuiltInDefault);
    CHECK(describe(std::nullopt, O("0"), std::nullopt, none).value == "0");
    CHECK(describe(std::nullopt, std::nullopt, std::nullopt, none).origin == Origin::NoDefault);
    CHECK(describe(O("0"), O("0"), std::nullopt, none).origin == Origin::MatchesDefault);
    CHECK(describe(O("1"), O("0"), std::nullopt, none).origin == Origin::Stored);
    CHECK(describe(O("1"), std::nullopt, std::nullopt, none).origin == Origin::Stored);

    const Change mine{1700000000, "dark_color_mode", "0", "1", false};
    const Provenance p = describe(O("1"), O("0"), std::nullopt, mine);
    CHECK(p.origin == Origin::ChangedByYou && p.changed_at == 1700000000 && p.value == "1");
    // Setting it back to the default yourself is still yours.
    const Change back{1700000500, "dark_color_mode", "1", "0", false};
    CHECK(describe(O("0"), O("0"), std::nullopt, back).origin == Origin::ChangedByYou);
    // Changed somewhere else since the last record: the record no longer explains it.
    CHECK(describe(O("2"), O("0"), std::nullopt, mine).origin == Origin::Stored);
    CHECK(describe(O("0"), O("0"), std::nullopt, mine).origin == Origin::MatchesDefault);

    CHECK(differs_from_default(O("1"), O("0")));
    CHECK(!differs_from_default(O("0"), O("0")));
    CHECK(!differs_from_default(O("1"), std::nullopt));
    CHECK(!differs_from_default(std::nullopt, O("0")));
}

int main()
{
    test_sensitive_keys();
    test_diff();
    test_lines();
    test_store();
    test_describe();
    if (g_failures != 0) {
        std::fprintf(stderr, "preference history: %d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("preference history: all checks passed\n");
    return 0;
}
