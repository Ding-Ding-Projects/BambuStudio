// Standalone assertions for src/slic3r/GUI/FeatureFinder/LocalToolsHub.cpp:
// the tools the Local tools page lists and their chips, the pinned tools and
// their order, the chip filter, and Recent activity (cleaning, the file
// format, the 200-entry limit and the newest-first view). Built and run by
// tests/local_tools_hub.test.mjs, which also builds it with
// SLIC3R_LOCAL_TOOLS_HUB_NEGATIVE_FIXTURE: in that build a full log drops the
// newest entry instead of the oldest, and the checks below must fail.
#include "src/slic3r/GUI/FeatureFinder/FeatureCatalogue.cpp"
#include "src/slic3r/GUI/FeatureFinder/LocalToolsHub.cpp"

#include <cstdio>
#include <cstdlib>
#include <set>

using namespace Slic3r::GUI;
using namespace Slic3r::GUI::LocalTools;

static int g_failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                           \
        }                                                                           \
    } while (0)

static void test_catalogue()
{
    std::set<std::string> ids, group_ids;
    for (const Group &g : groups()) {
        CHECK(group_ids.insert(g.id).second);
        CHECK(count_in(g.id) > 0);
    }
    for (const Tool &t : tools()) {
        CHECK(ids.insert(t.feature).second);
        CHECK(find_group(t.group) != nullptr);
        CHECK(std::string(t.summary).size() > 10);
        // Every tool is a ready Every feature entry that opens a tool.
        const FeatureFinder::Feature *f = FeatureFinder::find(t.feature);
        CHECK(f != nullptr);
        if (f == nullptr) continue;
        CHECK(f->state == FeatureFinder::State::Ready);
        CHECK(FeatureFinder::parse_action(f->action).kind == FeatureFinder::ActionKind::Command);
    }
    CHECK(tools().size() >= 15);
    // The page itself is in Every feature but is not one of its own tools.
    CHECK(FeatureFinder::find("local-tools") != nullptr);
    CHECK(find_tool("local-tools") == nullptr);
    CHECK(find_tool("nope") == nullptr);
    std::size_t total = 0;
    for (const Group &g : groups()) total += count_in(g.id);
    CHECK(total == tools().size());
}

static void test_pins()
{
    const std::vector<std::string> defaults = default_pins();
    CHECK(defaults.size() == 9);
    CHECK(defaults.size() <= kPinLimit);
    CHECK(std::set<std::string>(defaults.begin(), defaults.end()).size() == defaults.size());
    for (const std::string &id : defaults) CHECK(find_tool(id) != nullptr);

    // Never written: the defaults. Written empty: nothing pinned.
    CHECK(parse_pins("", false) == defaults);
    CHECK(parse_pins("", true).empty());
    CHECK((parse_pins("minecraft,nope,base64,minecraft,,drawing", true) == std::vector<std::string>{"minecraft", "base64", "drawing"}));
    CHECK(serialize_pins({"minecraft", "base64"}) == "minecraft,base64");
    CHECK(parse_pins(serialize_pins(defaults), true) == defaults);

    std::string many;
    for (const Tool &t : tools()) many += std::string(t.feature) + ",";
    CHECK(parse_pins(many, true).size() == kPinLimit);

    std::vector<std::string> pins = {"base64"};
    pins = toggle_pin(pins, "drawing");
    CHECK((pins == std::vector<std::string>{"base64", "drawing"}));
    pins = toggle_pin(pins, "base64");
    CHECK((pins == std::vector<std::string>{"drawing"}));
    CHECK(toggle_pin(pins, "nope") == pins);
    CHECK(is_pinned(pins, "drawing") && !is_pinned(pins, "base64"));

    std::vector<std::string> full = parse_pins(many, true);
    std::string              outside;
    for (const Tool &t : tools())
        if (!is_pinned(full, t.feature)) outside = t.feature;
    CHECK(!outside.empty());
    CHECK(toggle_pin(full, outside) == full); // full: nothing is added
    CHECK(toggle_pin(full, full.front()).size() == kPinLimit - 1);

    const std::vector<std::string> order = {"a-tool", "base64", "drawing", "minecraft"};
    CHECK((move_pin(order, "drawing", -1) == std::vector<std::string>{"a-tool", "drawing", "base64", "minecraft"}));
    CHECK((move_pin(order, "base64", 1) == std::vector<std::string>{"a-tool", "drawing", "base64", "minecraft"}));
    CHECK((move_pin(order, "base64", -9) == std::vector<std::string>{"base64", "a-tool", "drawing", "minecraft"}));
    CHECK((move_pin(order, "base64", 9) == std::vector<std::string>{"a-tool", "drawing", "minecraft", "base64"}));
    CHECK(move_pin(order, "nope", 1) == order);
    CHECK(move_pin(order, "base64", 0) == order);
}

static void test_filters()
{
    CHECK(parse_filter("pinned") == kPinnedFilter);
    CHECK(parse_filter("games") == "games");
    CHECK(parse_filter("").empty());
    CHECK(parse_filter("not-a-group").empty());

    const std::vector<std::string> pins = {"minecraft", "base64"};
    const std::vector<std::size_t> pinned = visible(kPinnedFilter, pins);
    CHECK(pinned.size() == 2);
    CHECK(pinned.size() == 2 && std::string(tools()[pinned[0]].feature) == "minecraft" && std::string(tools()[pinned[1]].feature) == "base64");
    CHECK(visible("", pins).size() == tools().size());
    for (std::size_t i : visible("safety", pins)) CHECK(std::string(tools()[i].group) == "safety");
    CHECK(visible("safety", pins).size() == count_in("safety"));
    CHECK(visible(kPinnedFilter, {}).empty());
}

static void test_cleaning()
{
    CHECK(clean_text("  Encoded\t\treport.pdf \n") == "Encoded report.pdf");
    CHECK(clean_text("a\x01" "b\x7F" "c") == "a b c");
    CHECK(clean_text("") .empty());
    CHECK(clean_text("abcdefghij", 8) == "abcde...");
    // A two-byte character at the cut is never cut in half.
    const std::string accented = "abcd\xC3\xA9" "fgh"; // abcdéfgh, 9 bytes
    const std::string cut      = clean_text(accented, 8);
    CHECK(cut == "abcd...");
    CHECK(clean_text(std::string(500, 'x')).size() == kActivityTextLimit);

    CHECK(clean_result("Failed") == "Failed");
    CHECK(clean_result("Started") == "Started");
    CHECK(clean_result("whatever") == "Done");
    CHECK(clean_result("") == "Done");
}

static void test_lines()
{
    Activity a;
    a.time    = 1762560000;
    a.tool    = "base64";
    a.action  = "Encoded";
    a.subject = "report.pdf";
    a.result  = "Done";
    Activity back;
    CHECK(from_line(to_line(a), back));
    CHECK(back.time == a.time && back.tool == a.tool && back.action == a.action && back.subject == a.subject && back.result == a.result);

    CHECK(!from_line("", back));
    CHECK(!from_line("[1,2]", back));
    CHECK(!from_line(R"({"time":1,"tool":"nope","action":"Encoded"})", back));
    CHECK(!from_line(R"({"time":1,"tool":"base64","action":""})", back));
    CHECK(!from_line(R"({"time":"soon","tool":"base64","action":"Encoded"})", back));
    CHECK(from_line(R"({"time":5,"tool":"minecraft","action":"Started","subject":"Family\nsurvival","result":"odd"})", back));
    CHECK(back.subject == "Family survival" && back.result == "Done");

    // A name that is not valid UTF-8 is written, not thrown.
    a.subject = "bad\xFF" "name";
    const std::string line = to_line(a);
    CHECK(!line.empty() && from_line(line, back));
}

static void test_log()
{
    const std::filesystem::path file = std::filesystem::temp_directory_path() / "local-tools-hub-test" / "activity.jsonl";
    std::error_code             ec;
    std::filesystem::remove_all(file.parent_path(), ec);
    {
        ActivityLog log(file);
        log.load();
        CHECK(log.entries().empty());
        CHECK(!log.append(Activity{1, "nope", "Encoded", "x", "Done"}));
        CHECK(!log.append(Activity{1, "base64", "   ", "x", "Done"}));
        CHECK(log.entries().empty());
        for (int i = 0; i < int(kActivityLimit) + 5; ++i)
            CHECK(log.append(Activity{1000 + i, i % 2 ? "minecraft" : "base64", "Started", "entry " + std::to_string(i), "Done"}));
        CHECK(log.entries().size() == kActivityLimit);
        // The oldest go first: the newest entry is still there.
        CHECK(!log.entries().empty() && log.entries().back().subject == "entry " + std::to_string(kActivityLimit + 4));
        CHECK(!log.entries().empty() && log.entries().front().subject == "entry 5");
    }
    {
        ActivityLog log(file);
        log.load();
        CHECK(log.entries().size() == kActivityLimit);
        const std::vector<Activity> three = log.newest(3);
        CHECK(three.size() == 3);
        CHECK(three.size() == 3 && three[0].time > three[1].time && three[1].time > three[2].time);
        const std::vector<Activity> servers = log.newest(500, "minecraft");
        CHECK(!servers.empty());
        for (const Activity &a : servers) CHECK(a.tool == "minecraft");
        CHECK(servers.size() == kActivityLimit / 2);
        log.clear();
        CHECK(log.entries().empty());
        CHECK(!std::filesystem::exists(file));
    }
    std::filesystem::remove_all(file.parent_path(), ec);
}

int main()
{
    test_catalogue();
    test_pins();
    test_filters();
    test_cleaning();
    test_lines();
    test_log();
    if (g_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("local tools hub: all checks passed\n");
    return 0;
}
