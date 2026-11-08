// Standalone assertions for the Every feature catalogue and its command
// registry (src/slic3r/GUI/FeatureFinder). Built and run by
// tests/feature_catalogue.test.mjs, which also builds it once with
// SLIC3R_FEATURE_CATALOGUE_NEGATIVE_FIXTURE defined: in that build the
// production touch_recent appends a feature instead of moving it to the front,
// and this suite must fail.
#include "src/slic3r/GUI/FeatureFinder/FeatureCatalogue.cpp"
#include "src/slic3r/GUI/FeatureFinder/FeatureCommands.cpp"

#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>

using namespace Slic3r::GUI::FeatureFinder;

static int g_failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                           \
        }                                                                           \
    } while (0)

// touch() is the production touch_recent; the negative build changes the
// production code itself (see FeatureCatalogue.cpp), never this file.
static std::vector<std::string> touch(std::vector<std::string> recent, std::string_view id)
{
    return touch_recent(std::move(recent), id);
}

static void test_catalogue_integrity()
{
    std::set<std::string> category_ids;
    for (const Category &c : categories()) {
        CHECK(c.id != nullptr && *c.id != '\0');
        CHECK(c.title != nullptr && *c.title != '\0');
        CHECK(category_ids.insert(c.id).second);
        CHECK(count_in(c.id) > 0);
    }
    std::set<std::string> ids;
    std::size_t           ready = 0, not_built = 0;
    for (const Feature &f : features()) {
        CHECK(ids.insert(f.id).second);
        CHECK(find_category(f.category) != nullptr);
        CHECK(*f.title != '\0');
        CHECK(*f.where != '\0');
        // Material Symbols live in the Basic Multilingual Plane private use area.
        CHECK(f.glyph >= 0xE000 && f.glyph <= 0xF8FF);
        const Action a = parse_action(f.action);
        if (f.state == State::Ready) {
            ++ready;
            CHECK(a.kind != ActionKind::None);
            if (a.kind != ActionKind::Palette) CHECK(!a.target.empty());
        } else {
            ++not_built;
            CHECK(std::string(f.action).empty());
            CHECK(a.kind == ActionKind::None);
        }
        // A "New" badge never sits on something that is not built.
        if (f.badge == Badge::New) CHECK(f.state == State::NotBuilt || f.state == State::Ready);
        CHECK(find(f.id) == &f);
    }
    CHECK(ready == count_state(State::Ready));
    CHECK(not_built == count_state(State::NotBuilt));
    CHECK(ready + not_built == features().size());
    CHECK(find("no-such-feature") == nullptr);
    CHECK(find_category("no-such-category") == nullptr);

    // The palette row is the one with the palette chord.
    const Feature *palette = find("command-palette");
    CHECK(palette != nullptr && std::string(palette->shortcut) == "Ctrl+Shift+F");
    CHECK(palette != nullptr && parse_action(palette->action).kind == ActionKind::Palette);
}

static void test_parse_action()
{
    CHECK(parse_action("prefs:language").kind == ActionKind::Preference);
    CHECK(parse_action("prefs:language").target == "language");
    CHECK(parse_action("command:version-history").kind == ActionKind::Command);
    CHECK(parse_action("command:version-history").target == "version-history");
    CHECK(parse_action("docs:windows/regex-builder.md").kind == ActionKind::Documentation);
    CHECK(parse_action("docs:windows/regex-builder.md").target == "windows/regex-builder.md");
    CHECK(parse_action("palette").kind == ActionKind::Palette);
    CHECK(parse_action("palette").target.empty());
    CHECK(parse_action("").kind == ActionKind::None);
    CHECK(parse_action("prefs:").kind == ActionKind::None);     // empty target
    CHECK(parse_action("command:").kind == ActionKind::None);
    CHECK(parse_action("launch:rockets").kind == ActionKind::None);
    CHECK(parse_action("PREFS:language").kind == ActionKind::None); // prefixes are exact
}

static void test_filters()
{
    const Feature *ready = find("version-history");
    CHECK(ready != nullptr);
    if (!ready) return;
    CHECK(passes_filter(*ready, ""));
    CHECK(passes_filter(*ready, "records"));
    CHECK(!passes_filter(*ready, "tools"));
    CHECK(!passes_filter(*ready, kNotBuiltFilter));
    // Every feature in this build is built, so a designed-only row is made up here.
    Feature not_built = *find("minecraft");
    not_built.state   = State::NotBuilt;
    not_built.action  = "";
    CHECK(passes_filter(not_built, kNotBuiltFilter));
    CHECK(passes_filter(not_built, "tools"));
    CHECK(!passes_filter(*find("minecraft"), kNotBuiltFilter));
}

static void test_sorting()
{
    CHECK(parse_sort("group") == SortOrder::Group);
    CHECK(parse_sort("a-z") == SortOrder::AToZ);
    CHECK(parse_sort("recent") == SortOrder::Recent);
    CHECK(parse_sort("") == SortOrder::Group);
    CHECK(parse_sort("sideways") == SortOrder::Group);
    for (SortOrder s : {SortOrder::Group, SortOrder::AToZ, SortOrder::Recent})
        CHECK(parse_sort(sort_value(s)) == s);

    const auto title = [](const Feature &f) { return std::string(f.title); };
    const auto &all  = features();

    // Group: every feature exactly once, categories in their order.
    const std::vector<std::size_t> group = order(SortOrder::Group, {}, title);
    CHECK(group.size() == all.size());
    std::set<std::size_t> seen(group.begin(), group.end());
    CHECK(seen.size() == all.size());
    std::size_t last_category = 0;
    for (std::size_t i : group) {
        std::size_t category_index = 0;
        for (std::size_t c = 0; c < categories().size(); ++c)
            if (std::string_view(categories()[c].id) == all[i].category) category_index = c;
        CHECK(category_index >= last_category);
        last_category = category_index;
    }

    // A to Z: case-insensitive by the given title.
    const std::vector<std::size_t> az = order(SortOrder::AToZ, {}, title);
    CHECK(az.size() == all.size());
    for (std::size_t k = 1; k < az.size(); ++k) {
        std::string a = all[az[k - 1]].title, b = all[az[k]].title;
        for (char &c : a) c = char(std::tolower((unsigned char) c));
        for (char &c : b) c = char(std::tolower((unsigned char) c));
        CHECK(a <= b);
    }
    // A to Z follows the supplied (translated) title, not the source title.
    const std::vector<std::size_t> by_id = order(SortOrder::AToZ, {}, [](const Feature &f) { return std::string(f.id); });
    CHECK(by_id.size() == all.size());
    for (std::size_t k = 1; k < by_id.size(); ++k)
        CHECK(std::string_view(all[by_id[k - 1]].id) < std::string_view(all[by_id[k]].id));

    // Recent: recently opened first, most recent first, then group order.
    const std::vector<std::string> recent = {"melody", "version-history"};
    const std::vector<std::size_t> rec    = order(SortOrder::Recent, recent, title);
    CHECK(rec.size() == all.size());
    CHECK(std::string(all[rec[0]].id) == "melody");
    CHECK(std::string(all[rec[1]].id) == "version-history");
    std::set<std::size_t> rec_seen(rec.begin(), rec.end());
    CHECK(rec_seen.size() == all.size());
}

static void test_recent()
{
    CHECK(parse_recent("").empty());
    // Unknown ids and duplicates are dropped; order kept. (Not-built features are
    // dropped too; this build has none left to show it with.)
    const std::vector<std::string> parsed =
        parse_recent("melody,unknown,melody,minecraft,converter");
    CHECK(parsed.size() == 3);
    CHECK(parsed.size() == 3 && parsed[0] == "melody" && parsed[1] == "minecraft" && parsed[2] == "converter");
    CHECK(serialize_recent(parsed) == "melody,minecraft,converter");
    CHECK(count_state(State::NotBuilt) == 0);
    CHECK(serialize_recent({}).empty());

    // Capped at kRecentLimit.
    std::string many;
    std::size_t ready_count = 0;
    for (const Feature &f : features())
        if (f.state == State::Ready) {
            if (!many.empty()) many += ',';
            many += f.id;
            ++ready_count;
        }
    CHECK(ready_count > kRecentLimit);
    CHECK(parse_recent(many).size() == kRecentLimit);

    // Touch moves to the front and trims.
    std::vector<std::string> r = {"melody", "converter"};
    r = touch(r, "converter");
    CHECK(r.size() == 2 && r[0] == "converter" && r[1] == "melody");
    r = touch(r, "authenticator");
    CHECK(r.size() == 3 && r[0] == "authenticator");
    r = touch(r, "minecraft");
    CHECK(r.size() == 4 && r[0] == "minecraft");
    r = touch(r, "unknown"); // unknown: unchanged
    CHECK(r.size() == 4);
    std::vector<std::string> full;
    for (const Feature &f : features())
        if (f.state == State::Ready) full = touch(full, f.id);
    CHECK(full.size() == kRecentLimit);
}

static void test_commands()
{
    Commands &commands = Commands::instance();
    commands.clear();
    int runs = 0;
    CHECK(!commands.has("melody"));
    CHECK(!commands.run("melody"));
    commands.add("melody", [&runs]() { ++runs; });
    CHECK(commands.has("melody"));
    CHECK(commands.run("melody"));
    CHECK(runs == 1);
    commands.add("melody", [&runs]() { runs += 10; }); // replaced, not duplicated
    CHECK(commands.run("melody"));
    CHECK(runs == 11);
    CHECK(commands.ids().size() == 1);
    commands.add("", [&runs]() { ++runs; });  // ignored
    commands.add("empty", {});                // ignored
    CHECK(!commands.has(""));
    CHECK(!commands.has("empty"));
    // A command that re-registers itself while it runs (a menu rebuild) is safe.
    commands.add("rebuild", [&commands, &runs]() {
        commands.add("rebuild", [&runs]() { runs += 100; });
        ++runs;
    });
    CHECK(commands.run("rebuild"));
    CHECK(runs == 12);
    CHECK(commands.run("rebuild"));
    CHECK(runs == 112);
    commands.clear();
    CHECK(commands.ids().empty());
}

int main()
{
    test_catalogue_integrity();
    test_parse_action();
    test_filters();
    test_sorting();
    test_recent();
    test_commands();
    if (g_failures != 0) {
        std::fprintf(stderr, "feature catalogue: %d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("feature catalogue: all checks passed (%zu features, %zu not built yet)\n", features().size(),
                count_state(State::NotBuilt));
    return 0;
}
