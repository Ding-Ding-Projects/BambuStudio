#include <catch_main.hpp>

#include "slic3r/GUI/NotificationHistory.hpp"

#include <filesystem>
#include <fstream>
#include <random>

#include <nlohmann/json.hpp>

using Slic3r::GUI::NotificationHistory;
using Slic3r::GUI::NotificationHistoryEntry;
using Filter    = NotificationHistory::Filter;
using Selection = NotificationHistory::Selection;
using Format    = NotificationHistory::ExportFormat;

namespace {

// Level values mirror NotificationManager::NotificationLevel.
constexpr int LEVEL_REGULAR   = 3;
constexpr int LEVEL_IMPORTANT = 6;
constexpr int LEVEL_WARNING   = 7;
constexpr int LEVEL_ERROR     = 9;

std::filesystem::path temp_file(const char *stem)
{
    std::random_device rd;
    return std::filesystem::temp_directory_path() /
           (std::string("bambu_notification_center_") + stem + "_" + std::to_string(rd()) + ".json");
}

} // namespace

TEST_CASE("History appends in order, records dismissals and actions", "[NotificationHistory]")
{
    NotificationHistory history;
    int changes = 0;
    history.set_on_change([&changes] { ++changes; });

    const auto a = history.append(LEVEL_REGULAR, "CustomNotification", "Saved project\nto disk", 1000);
    const auto b = history.append(LEVEL_ERROR, "SlicingError", "Slicing failed", 2000);
    REQUIRE(a != b);
    REQUIRE(history.size() == 2);
    REQUIRE(changes == 2);

    const auto *first = history.find(a);
    REQUIRE(first != nullptr);
    CHECK(first->title == "Saved project");
    CHECK(first->text == "Saved project\nto disk");
    CHECK(first->level_name == "regular");
    CHECK(first->type_name == "CustomNotification");
    CHECK_FALSE(first->dismissed);
    CHECK_FALSE(first->seen);

    CHECK(history.unread_count() == 2);
    CHECK(history.active_count() == 2);

    REQUIRE(history.mark_dismissed(a, 3000));
    CHECK(history.find(a)->dismissed);
    CHECK(history.find(a)->dismissed_ms == 3000);
    CHECK(history.active_count() == 1);
    // Dismissing twice is idempotent and does not bump the revision.
    const auto rev = history.revision();
    REQUIRE(history.mark_dismissed(a, 4000));
    CHECK(history.revision() == rev);
    CHECK(history.find(a)->dismissed_ms == 3000);

    CHECK_FALSE(history.mark_dismissed(9999));
    REQUIRE(history.record_action(b, "Open log"));
    CHECK(history.find(b)->action == "Open log");

    REQUIRE(history.mark_all_seen());
    CHECK(history.unread_count() == 0);
    CHECK_FALSE(history.mark_all_seen());

    // Entries stay in append order (oldest first).
    REQUIRE(history.entries().size() == 2);
    CHECK(history.entries()[0].id == a);
    CHECK(history.entries()[1].id == b);
}

TEST_CASE("History is bounded and drops the oldest entries", "[NotificationHistory]")
{
    NotificationHistory history(5);
    std::vector<std::uint64_t> ids;
    for (int i = 0; i < 12; ++i)
        ids.push_back(history.append(LEVEL_REGULAR, "T", "n" + std::to_string(i), 1000 + i));
    REQUIRE(history.size() == 5);
    CHECK(history.entries().front().text == "n7");
    CHECK(history.entries().back().text == "n11");
    CHECK(history.find(ids[0]) == nullptr);
    CHECK(history.find(ids[11]) != nullptr);
    // Ids never repeat even after trimming.
    const auto next = history.append(LEVEL_REGULAR, "T", "n12", 2000);
    CHECK(next > ids[11]);
    CHECK(history.max_entries() == 5);
    CHECK(NotificationHistory::DEFAULT_MAX_ENTRIES == 500);
}

TEST_CASE("History erase removes only the requested ids", "[NotificationHistory]")
{
    NotificationHistory history;
    const auto a = history.append(LEVEL_REGULAR, "T", "a", 1);
    const auto b = history.append(LEVEL_REGULAR, "T", "b", 2);
    const auto c = history.append(LEVEL_REGULAR, "T", "c", 3);
    CHECK(history.erase({a, c, 12345}) == 2);
    REQUIRE(history.size() == 1);
    CHECK(history.entries()[0].id == b);
    CHECK(history.erase({}) == 0);
}

TEST_CASE("Persistence round trip preserves every field", "[NotificationHistory][persistence]")
{
    NotificationHistory history(50);
    const auto a = history.append(LEVEL_WARNING, "PlaterWarning", "Object outside \"bed\", comma, and\nnewline", 1700000000123);
    const auto b = history.append(LEVEL_ERROR, "SlicingError", "Boom", 1700000001000);
    history.mark_dismissed(a, 1700000002000);
    history.record_action(b, "Jump to object");
    history.mark_all_seen();

    const auto file = temp_file("roundtrip");
    std::string err;
    REQUIRE(history.save(file, &err));
    CHECK(err.empty());

    NotificationHistory loaded(50);
    REQUIRE(loaded.load(file, &err));
    CHECK(err.empty());
    REQUIRE(loaded.size() == 2);
    const auto *la = loaded.find(a);
    const auto *lb = loaded.find(b);
    REQUIRE(la != nullptr);
    REQUIRE(lb != nullptr);
    CHECK(la->timestamp_ms == 1700000000123);
    CHECK(la->level == LEVEL_WARNING);
    CHECK(la->level_name == "warning");
    CHECK(la->type_name == "PlaterWarning");
    CHECK(la->title == "Object outside \"bed\", comma, and");
    CHECK(la->text == "Object outside \"bed\", comma, and\nnewline");
    CHECK(la->dismissed);
    CHECK(la->dismissed_ms == 1700000002000);
    CHECK(la->seen);
    CHECK(lb->action == "Jump to object");
    CHECK_FALSE(lb->dismissed);
    // Ids continue after the highest loaded id.
    const auto c = loaded.append(LEVEL_REGULAR, "T", "after load", 1);
    CHECK(c > b);

    std::filesystem::remove(file);
    // A missing file is not an error and leaves the history empty.
    NotificationHistory fresh;
    REQUIRE(fresh.load(file, &err));
    CHECK(fresh.empty());
}

TEST_CASE("Persistence rejects malformed input without touching existing entries", "[NotificationHistory][persistence]")
{
    NotificationHistory history;
    history.append(LEVEL_REGULAR, "T", "keep me", 1);
    std::string err;
    CHECK_FALSE(history.from_json("not json", &err));
    CHECK_FALSE(err.empty());
    CHECK_FALSE(history.from_json("{\"schema\":\"something.else\",\"entries\":[]}", &err));
    CHECK_FALSE(history.from_json("{\"schema\":\"bambustudio.notification_history\",\"version\":999,\"entries\":[]}", &err));
    CHECK_FALSE(history.from_json("{\"schema\":\"bambustudio.notification_history\",\"version\":1}", &err));
    REQUIRE(history.size() == 1);
    CHECK(history.entries()[0].text == "keep me");

    // Loading trims to the receiving history's bound.
    NotificationHistory big;
    for (int i = 0; i < 10; ++i)
        big.append(LEVEL_REGULAR, "T", std::to_string(i), i + 1);
    NotificationHistory small(3);
    REQUIRE(small.from_json(big.to_json(), &err));
    REQUIRE(small.size() == 3);
    CHECK(small.entries().front().text == "7");
}

TEST_CASE("Filters compose query, level and status; results are newest first", "[NotificationHistory][filter]")
{
    NotificationHistory history;
    const auto info    = history.append(LEVEL_REGULAR, "CustomNotification", "Project saved", 1000);
    const auto warn    = history.append(LEVEL_WARNING, "PlaterWarning", "Object outside bed", 2000);
    const auto error   = history.append(LEVEL_ERROR, "SlicingError", "Slicing failed: object outside", 3000);
    const auto import_ = history.append(LEVEL_IMPORTANT, "PresetUpdateAvailable", "Configuration can update now.", 4000);
    history.mark_dismissed(warn, 5000);

    Filter all;
    CHECK(history.filtered_ids(all) == std::vector<std::uint64_t>{import_, error, warn, info});

    Filter by_query;
    by_query.query = "OUTSIDE"; // default matcher is case-insensitive substring
    CHECK(history.filtered_ids(by_query) == std::vector<std::uint64_t>{error, warn});

    Filter by_level;
    by_level.levels = {LEVEL_ERROR, LEVEL_WARNING};
    CHECK(history.filtered_ids(by_level) == std::vector<std::uint64_t>{error, warn});

    Filter composed;
    composed.query             = "outside";
    composed.levels            = {LEVEL_ERROR, LEVEL_WARNING};
    composed.include_dismissed = false;
    CHECK(history.filtered_ids(composed) == std::vector<std::uint64_t>{error});

    Filter dismissed_only;
    dismissed_only.include_active = false;
    CHECK(history.filtered_ids(dismissed_only) == std::vector<std::uint64_t>{warn});

    // A custom matcher (the SearchField regex path) replaces the substring test.
    Filter custom;
    custom.query   = "^Slicing";
    custom.matcher = [](const std::string &q, const std::string &hay) {
        return hay.rfind("Slicing failed", 0) == 0 && q == "^Slicing";
    };
    CHECK(history.filtered_ids(custom) == std::vector<std::uint64_t>{error});

    // The haystack covers level and type names so "error" finds error toasts.
    Filter by_level_word;
    by_level_word.query = "error";
    CHECK(history.filtered_ids(by_level_word) == std::vector<std::uint64_t>{error});

    CHECK(composed.describe() == "query: \"outside\"; levels: warning, error; status: active only");
    CHECK(all.describe() == "query: (none); levels: all; status: active and dismissed");
}

TEST_CASE("Selection distinguishes this page from all matches and supports inverse and ranges", "[NotificationHistory][selection]")
{
    NotificationHistory history;
    std::vector<std::uint64_t> ids;
    for (int i = 0; i < 10; ++i)
        ids.push_back(history.append(LEVEL_REGULAR, "T", "n" + std::to_string(i), 1000 + i));

    const std::vector<std::uint64_t> matches = history.filtered_ids(Filter{}); // newest first, 10 ids
    const std::vector<std::uint64_t> page(matches.begin(), matches.begin() + 4);

    Selection sel;
    sel.select_page(page);
    CHECK(sel.size() == 4);
    CHECK(sel.count_within(matches) == 4);
    CHECK(sel.count_within(page) == 4);

    sel.select_all_matches(matches);
    CHECK(sel.size() == 10);

    sel.clear();
    sel.select_page(page);
    sel.invert(matches);
    CHECK(sel.size() == 6);
    for (auto id : page)
        CHECK_FALSE(sel.contains(id));
    for (std::size_t i = 4; i < matches.size(); ++i)
        CHECK(sel.contains(matches[i]));

    // Inverting leaves ids outside the universe alone.
    sel.clear();
    sel.set(matches[0], true);
    sel.invert(page);
    CHECK_FALSE(sel.contains(matches[0]));
    CHECK(sel.contains(matches[3]));
    CHECK(sel.size() == 3);

    // Shift-click range in list order, either direction.
    sel.clear();
    sel.select_range(matches, matches[2], matches[6]);
    CHECK(sel.size() == 5);
    CHECK(sel.contains(matches[2]));
    CHECK(sel.contains(matches[6]));
    CHECK_FALSE(sel.contains(matches[7]));
    sel.clear();
    sel.select_range(matches, matches[6], matches[2]);
    CHECK(sel.size() == 5);
    // Unknown anchor falls back to a single selection.
    sel.clear();
    sel.select_range(matches, 424242, matches[1]);
    CHECK(sel.size() == 1);

    // toggle / retain
    sel.toggle(matches[1]);
    CHECK(sel.empty());
    sel.toggle(matches[1]);
    sel.toggle(matches[2]);
    sel.retain({matches[2]});
    CHECK(sel.size() == 1);
    CHECK(sel.contains(matches[2]));
}

TEST_CASE("CSV export quotes fields and states the exported range", "[NotificationHistory][export]")
{
    NotificationHistory history;
    const auto a = history.append(LEVEL_WARNING, "PlaterWarning", "Object \"cube\", outside\nbed", 1700000000000);
    const auto b = history.append(LEVEL_ERROR, "SlicingError", "Boom", 1700000001000);
    history.append(LEVEL_REGULAR, "T", "unrelated", 1700000002000);
    history.mark_dismissed(a, 1700000003000);
    history.record_action(b, "Open log");

    Filter f;
    f.levels = {LEVEL_WARNING, LEVEL_ERROR};
    const auto ids = history.filtered_ids(f);
    REQUIRE(ids == std::vector<std::uint64_t>{b, a});

    const std::string csv = history.export_entries(ids, Format::Csv, f);
    CHECK(csv.rfind("# Exported 2 of 3 recorded notifications; query: (none); levels: warning, error; status: active and dismissed; time span 2023-11-14T22:13:20.000Z to 2023-11-14T22:13:21.000Z", 0) == 0);
    CHECK(csv.find("\nid,timestamp,level,level_name,type,title,text,dismissed,dismissed_at,action\n") != std::string::npos);
    CHECK(csv.find(std::to_string(b) + ",2023-11-14T22:13:21.000Z,9,error,SlicingError,Boom,Boom,false,,Open log\n") != std::string::npos);
    CHECK(csv.find(std::to_string(a) + ",2023-11-14T22:13:20.000Z,7,warning,PlaterWarning,\"Object \"\"cube\"\", outside\",\"Object \"\"cube\"\", outside\nbed\",true,2023-11-14T22:13:23.000Z,\n") != std::string::npos);
    CHECK(NotificationHistory::export_extension(Format::Csv) == std::string("csv"));
}

TEST_CASE("JSON export carries the range header and one object per entry", "[NotificationHistory][export]")
{
    NotificationHistory history;
    const auto a = history.append(LEVEL_IMPORTANT, "PresetUpdateAvailable", "Configuration can update now.", 1700000000000);
    const auto b = history.append(LEVEL_ERROR, "SlicingError", "Boom", 1700000001000);
    history.mark_dismissed(b, 1700000002000);

    Filter f;
    f.query = "update";
    const auto ids = history.filtered_ids(f);
    REQUIRE(ids == std::vector<std::uint64_t>{a});

    const auto doc = nlohmann::json::parse(history.export_entries(ids, Format::Json, f));
    CHECK(doc["schema"] == "bambustudio.notification_history.export");
    CHECK(doc["version"] == 1);
    CHECK(doc["exported"] == 1);
    CHECK(doc["total"] == 2);
    CHECK(doc["filter"] == "query: \"update\"; levels: all; status: active and dismissed");
    CHECK(doc["range"].get<std::string>().rfind("Exported 1 of 2 recorded notifications", 0) == 0);
    REQUIRE(doc["entries"].size() == 1);
    const auto &e = doc["entries"][0];
    CHECK(e["id"] == a);
    CHECK(e["timestamp"] == "2023-11-14T22:13:20.000Z");
    CHECK(e["level"] == LEVEL_IMPORTANT);
    CHECK(e["level_name"] == "important");
    CHECK(e["type"] == "PresetUpdateAvailable");
    CHECK(e["title"] == "Configuration can update now.");
    CHECK(e["dismissed"] == false);
    CHECK(e["dismissed_at"] == "");
    CHECK(e["action"] == "");

    // The full set: dismissed entries carry their dismissal time.
    const auto all = nlohmann::json::parse(history.export_entries(history.filtered_ids(Filter{}), Format::Json, Filter{}));
    REQUIRE(all["entries"].size() == 2);
    CHECK(all["entries"][0]["id"] == b);
    CHECK(all["entries"][0]["dismissed"] == true);
    CHECK(all["entries"][0]["dismissed_at"] == "2023-11-14T22:13:22.000Z");
}

TEST_CASE("Markdown and plain-text exports escape and state the range", "[NotificationHistory][export]")
{
    NotificationHistory history;
    history.append(LEVEL_ERROR, "SlicingError", "a | b\nsecond line", 1700000000000);
    const auto ids = history.filtered_ids(Filter{});

    const std::string md = history.export_entries(ids, Format::Markdown, Filter{});
    CHECK(md.rfind("# Notification history\n\nExported 1 of 1 recorded notifications", 0) == 0);
    CHECK(md.find("| Time (UTC) | Level | Type | Title | Text | Status | Action |") != std::string::npos);
    CHECK(md.find("| 2023-11-14T22:13:20.000Z | Error | SlicingError | a \\| b | a \\| b second line | active |  |") != std::string::npos);

    const std::string txt = history.export_entries(ids, Format::PlainText, Filter{});
    CHECK(txt.rfind("Exported 1 of 1 recorded notifications", 0) == 0);
    CHECK(txt.find("2023-11-14T22:13:20.000Z  [Error]  SlicingError  (active)\n    a | b second line\n") != std::string::npos);

    // Empty export still states the (empty) range.
    const std::string none = history.export_entries({}, Format::PlainText, Filter{});
    CHECK(none.find("Exported 0 of 1 recorded notifications") != std::string::npos);
    CHECK(none.find("time span (no entries)") != std::string::npos);
}

TEST_CASE("Level names follow the NotificationLevel ordering", "[NotificationHistory]")
{
    CHECK(NotificationHistory::level_name(1) == "progress");
    CHECK(NotificationHistory::level_name(2) == "hint");
    CHECK(NotificationHistory::level_name(3) == "regular");
    CHECK(NotificationHistory::level_name(6) == "important");
    CHECK(NotificationHistory::level_name(7) == "warning");
    CHECK(NotificationHistory::level_name(8) == "serious_warning");
    CHECK(NotificationHistory::level_name(9) == "error");
    CHECK(NotificationHistory::level_name(42) == "level_42");
    CHECK(NotificationHistory::level_display(9) == "Error");
    CHECK(NotificationHistory::level_display(3) == "Info");
    CHECK(NotificationHistory::format_iso8601(0) == "1970-01-01T00:00:00.000Z");
    CHECK(NotificationHistory::format_iso8601(1700000000123) == "2023-11-14T22:13:20.123Z");
    CHECK(NotificationHistory::first_line("a\r\nb") == "a");
}
