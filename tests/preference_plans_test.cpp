// Standalone assertions for src/slic3r/GUI/PreferencesRemake/PreferencePlans.cpp:
// what Reset this section changes and leaves alone, what an imported
// preferences file would change, when the newest change can be undone, the
// related settings under the same heading, and how many settings each section
// holds. Built and run by tests/preference_plans.test.mjs, which also builds it
// with SLIC3R_PREFERENCE_PLANS_NEGATIVE_FIXTURE: in that build a section reset
// also changes settings that restart the app or answer a consent question, and
// the checks below must fail.
#include "src/slic3r/GUI/PreferencesRemake/PreferenceHistory.cpp"
#include "src/slic3r/GUI/PreferencesRemake/PreferencePlans.cpp"

#include <cstdio>

using namespace Slic3r::GUI;
using namespace Slic3r::GUI::PreferencePlans;

static int g_failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                           \
        }                                                                           \
    } while (0)

static Setting setting(const char *key, std::optional<std::string> stored, std::optional<std::string> def, bool settable = true)
{
    return Setting{key, std::move(stored), std::move(def), settable};
}

static bool has(const std::vector<std::string> &list, const std::string &key)
{
    return std::find(list.begin(), list.end(), key) != list.end();
}

static void section_reset()
{
    const std::vector<Setting> section = {
        setting("dark_color_mode", std::string("1"), std::string("0")),          // changed, has a control: reset
        setting("ui_density", std::string("comfortable"), std::string("comfortable")), // already the default
        setting("ui_font_scale", std::nullopt, std::string("1.0")),             // nothing stored: the default is in effect
        setting("backup_interval", std::string("20"), std::string("10"), false), // changed, no control here: by hand
        setting("language", std::string("de_DE"), std::string("en_US")),        // needs a restart: by hand
        setting("enable_lod", std::string("true"), std::string("false")),       // recreates the window: by hand
        setting("status_api_token", std::string("abc"), std::string("")),       // private: never
        setting("no_default_here", std::string("x"), std::nullopt),             // no declared default: left alone
        setting("dark_color_mode", std::string("1"), std::string("0")),          // the same key twice: once
    };
    const ResetPlan plan = plan_section_reset(section);
    CHECK(plan.reset == std::vector<std::string>({"dark_color_mode"}));
    CHECK(plan.by_hand == std::vector<std::string>({"backup_interval", "language", "enable_lod"}));
    CHECK(plan.private_keys == std::vector<std::string>({"status_api_token"}));
    CHECK(has(plan.unchanged, "ui_density") && has(plan.unchanged, "ui_font_scale") && has(plan.unchanged, "no_default_here"));
    CHECK(plan.unchanged.size() == 3);
    for (const char *key : {"language", "single_instance", "enable_lod", "region", "privacyuse", "developer_mode"}) CHECK(by_hand_only(key));
    CHECK(!by_hand_only("dark_color_mode") && !by_hand_only("ui_accent_seed") && !by_hand_only(""));
    CHECK(plan_section_reset({}).reset.empty());
}

static void import()
{
    const std::vector<Setting> shown = {
        setting("dark_color_mode", std::string("0"), std::string("0")),
        setting("ui_density", std::nullopt, std::string("comfortable")),
        setting("ui_accent_seed", std::string("#146c2e"), std::string("#146c2e")),
        setting("backup_interval", std::string("10"), std::string("10"), false),
        setting("language", std::string("en_US"), std::string("en_US")),
        setting("max_recent_count", std::string("18"), std::string("18")),
    };
    const std::string exported = R"({
      "schema": "bambustudio.app-config", "schemaVersion": 1, "dataset": "Preferences", "kind": "structured",
      "encoding": "UTF-8", "lineEnding": "LF", "generator": "Bambu Studio", "exclusions": [],
      "data": { "app": {
        "dark_color_mode": "1",
        "ui_density": "comfortable",
        "ui_accent_seed": "#2563eb",
        "backup_interval": "30",
        "language": "zh_CN",
        "lan_drop_station_key": "secret",
        "some_cache_path": "C:/somewhere",
        "max_recent_count": 25,
        "ui_font_scale": [1]
      } }
    })";
    ImportPlan plan = plan_import(exported, shown);
    CHECK(plan.problem.empty());
    CHECK(plan.apply.size() == 3);
    if (plan.apply.size() == 3) {
        CHECK(plan.apply[0].key == "dark_color_mode" && plan.apply[0].value == "1");
        CHECK(plan.apply[1].key == "max_recent_count" && plan.apply[1].value == "25"); // a whole number is read as text
        CHECK(plan.apply[2].key == "ui_accent_seed" && plan.apply[2].value == "#2563eb");
    }
    CHECK(plan.by_hand.size() == 2 && plan.by_hand[0].key == "backup_interval" && plan.by_hand[1].key == "language");
    CHECK(plan.same == std::vector<std::string>({"ui_density"})); // nothing stored, and the file says the default
    CHECK(plan.private_keys == std::vector<std::string>({"lan_drop_station_key"}));
    CHECK(plan.not_here == std::vector<std::string>({"some_cache_path", "ui_font_scale"}));

    // Written without the schema header: the bare data.
    plan = plan_import(R"({"app": {"dark_color_mode": true, "ui_accent_seed": 1.5}})", shown);
    CHECK(plan.problem.empty() && plan.apply.size() == 1 && plan.apply[0].value == "true");
    CHECK(plan.not_text == std::vector<std::string>({"ui_accent_seed"}));

    CHECK(plan_import("not json", shown).problem == "not-json");
    CHECK(plan_import("[1,2]", shown).problem == "not-preferences");
    CHECK(plan_import(R"({"schema": "bambustudio.preset", "data": {"app": {}}})", shown).problem == "not-preferences");
    CHECK(plan_import(R"({"schema": "bambustudio.app-config", "schemaVersion": 2, "data": {"app": {"a": "b"}}})", shown).problem == "newer-version");
    CHECK(plan_import(R"({"schema": "bambustudio.app-config", "schemaVersion": 1})", shown).problem == "not-preferences");
    CHECK(plan_import(R"({"app": {}})", shown).problem == "empty");
    CHECK(plan_import(R"({"app": "dark"})", shown).problem == "not-preferences");
    CHECK(plan_import(std::string(kImportLimit + 1, ' '), shown).problem == "too-large");
    // Nothing is planned for a file that cannot be used.
    const ImportPlan bad = plan_import(R"({"schema": "other", "data": {"app": {"dark_color_mode": "1"}}})", shown);
    CHECK(bad.apply.empty() && bad.by_hand.empty());
}

static void undo()
{
    using PreferenceHistory::Change;
    const std::vector<Setting> shown = {
        setting("dark_color_mode", std::string("1"), std::string("0")),
        setting("ui_density", std::string("compact"), std::string("comfortable")),
        setting("backup_interval", std::string("20"), std::string("10"), false),
        setting("language", std::string("de_DE"), std::string("en_US")),
    };
    CHECK(!plan_undo({}, shown).possible && plan_undo({}, shown).reason == "nothing");
    UndoPlan p = plan_undo({Change{100, "dark_color_mode", "0", "1", false}}, shown);
    CHECK(p.possible && p.key == "dark_color_mode" && p.value == "0");
    // Nothing was written down before: the built-in default goes back.
    p = plan_undo({Change{100, "ui_density", "", "compact", false}}, shown);
    CHECK(p.possible && p.value == "comfortable");
    CHECK(plan_undo({Change{100, "status_api_token", "", "", true}}, shown).reason == "private");
    CHECK(plan_undo({Change{100, "backup_interval", "10", "20", false}}, shown).reason == "not-settable");
    CHECK(plan_undo({Change{100, "language", "en_US", "de_DE", false}}, shown).reason == "by-hand");
    CHECK(plan_undo({Change{100, "gone_key", "a", "b", false}}, shown).reason == "not-shown");
    // Only the newest change: an older undoable one behind it is not reached.
    p = plan_undo({Change{200, "backup_interval", "10", "20", false}, Change{100, "dark_color_mode", "0", "1", false}}, shown);
    CHECK(!p.possible && p.key == "backup_interval");
}

static void related_and_counts()
{
    const std::vector<Row> rows = {
        {0, true, {}},            // Appearance
        {0, false, {"dark_color_mode"}},
        {0, false, {"ui_density"}},
        {0, false, {"ui_accent_seed"}},
        {0, false, {"ui_font_family"}},
        {0, true, {}},            // Identity
        {0, false, {"app_display_name"}},
        {0, false, {"app_logo"}},
        {1, true, {}},            // General
        {1, false, {"region"}},
        {1, false, {"backup_switch", "backup_interval"}},
        {1, true, {}},
        {1, false, {"max_recent_count"}},
    };
    // Same heading, nearest first; never itself.
    CHECK(related(rows, "ui_density") == std::vector<std::string>({"dark_color_mode", "ui_accent_seed", "ui_font_family"}));
    CHECK(related(rows, "ui_accent_seed", 2) == std::vector<std::string>({"ui_density", "ui_font_family"}));
    // One other under its heading: the rest of the page fills in, never another page.
    const std::vector<std::string> logo = related(rows, "app_logo");
    CHECK(!logo.empty() && logo.front() == "app_display_name");
    CHECK(has(logo, "ui_font_family") && !has(logo, "region") && !has(logo, "app_logo"));
    // A row with two keys gives both.
    const std::vector<std::string> region = related(rows, "region");
    CHECK(region.size() >= 2 && region[0] == "backup_switch" && region[1] == "backup_interval");
    CHECK(related(rows, "missing").empty() && related(rows, "ui_density", 0).empty());

    const std::map<int, int> counts = settings_per_page(rows);
    CHECK(counts.at(0) == 6 && counts.at(1) == 4);
    CHECK(settings_per_page({{0, false, {"a"}}, {0, false, {"a"}}}).at(0) == 1);
}

int main()
{
    section_reset();
    import();
    undo();
    related_and_counts();
    if (g_failures) {
        std::fprintf(stderr, "%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("preference plans: all checks passed\n");
    return 0;
}
