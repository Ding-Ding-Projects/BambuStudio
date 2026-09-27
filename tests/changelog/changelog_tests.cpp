#include <catch_main.hpp>

#include "libslic3r/Changelog.hpp"

#include <algorithm>
#include <cctype>
#include <string>

using namespace Slic3r::Changelog;

#ifndef CHANGELOG_TEST_RESOURCE
#error "CHANGELOG_TEST_RESOURCE must identify the committed resources/changelog/changelog.json"
#endif

namespace {

const char *SAMPLE_JSON = R"JSON({
  "schema": 1,
  "repository": "Example/Repo",
  "commitUrlTemplate": "https://github.com/Example/Repo/commit/{sha}",
  "generated": "2026-09-08T00:00:00Z",
  "categoryDerivation": "leading-verb of the commit subject",
  "releases": [
    {
      "tag": "md3-v3", "version": "v3", "ordinal": 3, "date": "2026-09-07",
      "published": "2026-09-07T10:00:00Z",
      "codeName": {"en": "Chive Dumpling", "yue": "韭菜餃"},
      "url": "https://example.test/releases/md3-v3", "commit": "cccccccccccccccccccccccccccccccccccccccc",
      "entries": [
        {"sha": "cccccccccccccccccccccccccccccccccccccccc", "short": "ccccccccc", "text": "Fix the calendar clipping", "category": "fixed"},
        {"sha": "dddddddddddddddddddddddddddddddddddddddd", "short": "ddddddddd", "text": "Add an export button", "category": "added"}
      ]
    },
    {
      "tag": "md3-v2", "version": "v2", "ordinal": 2, "date": "2026-06-15",
      "published": "2026-06-15T10:00:00Z",
      "codeName": {"en": "Har Gow", "yue": "蝦餃"},
      "entries": [
        {"sha": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", "text": "Document the regex builder"}
      ]
    },
    {
      "tag": "md3-v1", "version": "v1", "ordinal": 1, "date": "2025-12-31",
      "published": "2025-12-31T23:59:59Z", "baseline": true,
      "codeName": {"en": "", "yue": ""},
      "entries": []
    }
  ]
})JSON";

bool contains_ci(const std::string &haystack, const std::string &needle)
{
    std::string h = haystack;
    std::string n = needle;
    for (auto &c : h) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (auto &c : n) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return h.find(n) != std::string::npos;
}

} // namespace

TEST_CASE("Changelog JSON parses versions, dates, code names and entries", "[Changelog]")
{
    const Document document = parse_document(SAMPLE_JSON);
    REQUIRE(document.schema == 1);
    REQUIRE(document.repository == "Example/Repo");
    REQUIRE(document.releases.size() == 3);

    const Release &newest = document.releases.front();
    REQUIRE(newest.version == "v3");
    REQUIRE(newest.date == CivilDate{2026, 9, 7});
    REQUIRE(newest.code_name_en == "Chive Dumpling");
    REQUIRE(newest.code_name_yue == "韭菜餃");
    REQUIRE(newest.entries.size() == 2);
    REQUIRE(newest.entries[0].category == "fixed");
    REQUIRE(newest.entries[0].short_sha == "ccccccccc");

    // Defaults fill what the exporter may omit.
    const Release &middle = document.releases[1];
    REQUIRE(middle.entries[0].short_sha == "bbbbbbbbb");
    REQUIRE(middle.entries[0].category == "changed");

    REQUIRE(document.releases.back().baseline);
    REQUIRE(commit_url(document, "abc") == "https://github.com/Example/Repo/commit/abc");
}

TEST_CASE("Changelog JSON rejects malformed documents with a named cause", "[Changelog]")
{
    REQUIRE_THROWS_WITH(parse_document("not json"), Catch::Contains("not valid JSON"));
    REQUIRE_THROWS_WITH(parse_document(R"({"schema": 2, "repository": "x", "commitUrlTemplate": "{sha}", "releases": []})"),
                        Catch::Contains("unsupported schema"));
    REQUIRE_THROWS_WITH(parse_document(R"({"schema": 1, "repository": "x", "commitUrlTemplate": "{sha}"})"),
                        Catch::Contains("releases"));
    // A short SHA is refused: the viewer must only ever link full commit ids.
    REQUIRE_THROWS_WITH(parse_document(R"({"schema": 1, "repository": "x", "commitUrlTemplate": "{sha}",
        "releases": [{"tag": "t", "version": "v", "date": "2026-01-01",
        "entries": [{"sha": "abcdef0", "text": "x"}]}]})"),
                        Catch::Contains("not a full 40-hex"));
    REQUIRE_THROWS_WITH(parse_document(R"({"schema": 1, "repository": "x", "commitUrlTemplate": "{sha}",
        "releases": [{"tag": "t", "version": "v", "date": "2026-13-01", "entries": []}]})"),
                        Catch::Contains("not an ISO calendar date"));
}

TEST_CASE("The committed changelog.json parses and every entry carries a full SHA", "[Changelog]")
{
    const Document document = load_document(CHANGELOG_TEST_RESOURCE);
    REQUIRE(!document.releases.empty());
    for (const Release &release : document.releases) {
        REQUIRE(release.date.is_valid());
        REQUIRE(!release.version.empty());
        for (const Entry &entry : release.entries) {
            REQUIRE(entry.sha.size() == 40);
            REQUIRE(!entry.text.empty());
        }
    }
    // Newest first, as the viewer renders it.
    for (std::size_t i = 1; i < document.releases.size(); ++i)
        REQUIRE(document.releases[i - 1].date >= document.releases[i].date);
}

TEST_CASE("Typed dates: ISO always, locale order otherwise, partial input rejected", "[Changelog][dates]")
{
    REQUIRE(parse_typed_date("2026-09-08", DateOrder::DayMonthYear) == CivilDate{2026, 9, 8});
    REQUIRE(parse_typed_date(" 2026/9/8 ", DateOrder::MonthDayYear) == CivilDate{2026, 9, 8});
    REQUIRE(parse_typed_date("8/9/2026", DateOrder::DayMonthYear) == CivilDate{2026, 9, 8});
    REQUIRE(parse_typed_date("8/9/2026", DateOrder::MonthDayYear) == CivilDate{2026, 8, 9});
    REQUIRE(parse_typed_date("08.09.2026", DateOrder::DayMonthYear) == CivilDate{2026, 9, 8});
    REQUIRE(parse_typed_date("2026 09 08", DateOrder::YearMonthDay) == CivilDate{2026, 9, 8});
    REQUIRE(parse_typed_date("29/2/2024", DateOrder::DayMonthYear) == CivilDate{2024, 2, 29});

    // Partial input never becomes a guessed date.
    REQUIRE_FALSE(parse_typed_date("2026-09", DateOrder::DayMonthYear).has_value());
    REQUIRE_FALSE(parse_typed_date("2026", DateOrder::DayMonthYear).has_value());
    REQUIRE_FALSE(parse_typed_date("2026-09-", DateOrder::DayMonthYear).has_value());
    REQUIRE_FALSE(parse_typed_date("", DateOrder::DayMonthYear).has_value());
    REQUIRE_FALSE(parse_typed_date("   ", DateOrder::DayMonthYear).has_value());
    // Two-digit years, letters, out-of-range fields, impossible days.
    REQUIRE_FALSE(parse_typed_date("8/9/26", DateOrder::DayMonthYear).has_value());
    REQUIRE_FALSE(parse_typed_date("8 Sep 2026", DateOrder::DayMonthYear).has_value());
    REQUIRE_FALSE(parse_typed_date("2026-13-01", DateOrder::YearMonthDay).has_value());
    REQUIRE_FALSE(parse_typed_date("2026-02-30", DateOrder::YearMonthDay).has_value());
    REQUIRE_FALSE(parse_typed_date("29/2/2026", DateOrder::DayMonthYear).has_value());
    REQUIRE_FALSE(parse_typed_date("2026-09-08-01", DateOrder::YearMonthDay).has_value());
    REQUIRE_FALSE(parse_typed_date("2026--09-08", DateOrder::YearMonthDay).has_value());
}

TEST_CASE("Civil date arithmetic round-trips and orders", "[Changelog][dates]")
{
    const CivilDate d{2026, 9, 8};
    REQUIRE(CivilDate::from_days(d.to_days()) == d);
    REQUIRE(d.plus_days(-29) == CivilDate{2026, 8, 10});
    REQUIRE(CivilDate{2024, 2, 28}.plus_days(1) == CivilDate{2024, 2, 29});
    REQUIRE(CivilDate{2025, 12, 31}.plus_days(1) == CivilDate{2026, 1, 1});
    REQUIRE(CivilDate{2026, 1, 1} > CivilDate{2025, 12, 31});
    REQUIRE(d.to_iso() == "2026-09-08");
    REQUIRE_FALSE(CivilDate{2026, 2, 29}.is_valid());
}

TEST_CASE("Date ranges: presets and inclusive bounds", "[Changelog][dates]")
{
    const CivilDate today{2026, 9, 8};
    const DateRange last30 = DateRange::last_days(today, 30);
    REQUIRE(last30.from == CivilDate{2026, 8, 10});
    REQUIRE(last30.to == today);
    REQUIRE(last30.contains(CivilDate{2026, 8, 10}));
    REQUIRE(last30.contains(today));
    REQUIRE_FALSE(last30.contains(CivilDate{2026, 8, 9}));
    REQUIRE_FALSE(last30.contains(CivilDate{2026, 9, 9}));

    const DateRange year = DateRange::this_year(today);
    REQUIRE(year.from == CivilDate{2026, 1, 1});
    REQUIRE(year.describe() == "2026-01-01 to 2026-09-08");

    REQUIRE(DateRange::all().is_unbounded());
    REQUIRE(DateRange::all().describe() == "all versions");
    DateRange open_end;
    open_end.from = CivilDate{2026, 1, 1};
    REQUIRE(open_end.describe() == "from 2026-01-01");
    REQUIRE(open_end.contains(CivilDate{2999, 1, 1}));
}

TEST_CASE("Filtering composes the date range with the text matcher", "[Changelog][filter]")
{
    const Document document = parse_document(SAMPLE_JSON);

    SECTION("no filter keeps every release and entry")
    {
        const auto all = filter_releases(document, DateRange::all(), nullptr);
        REQUIRE(all.size() == 3);
        REQUIRE(count_entries(all) == 3);
        REQUIRE(all.back().entries.empty()); // the baseline release stays visible
    }

    SECTION("date range alone")
    {
        DateRange range;
        range.from = CivilDate{2026, 1, 1};
        range.to   = CivilDate{2026, 6, 30};
        const auto filtered = filter_releases(document, range, nullptr);
        REQUIRE(filtered.size() == 1);
        REQUIRE(filtered[0].release->version == "v2");
    }

    SECTION("text alone matches entry text and drops releases without a hit")
    {
        const auto filtered = filter_releases(document, DateRange::all(),
                                              [](const std::string &s) { return contains_ci(s, "export"); });
        REQUIRE(filtered.size() == 1);
        REQUIRE(filtered[0].release->version == "v3");
        REQUIRE(filtered[0].entries.size() == 1);
        REQUIRE(filtered[0].entries[0]->text == "Add an export button");
    }

    SECTION("a release header hit keeps all of its entries")
    {
        const auto filtered = filter_releases(document, DateRange::all(),
                                              [](const std::string &s) { return contains_ci(s, "chive"); });
        REQUIRE(filtered.size() == 1);
        REQUIRE(filtered[0].entries.size() == 2);
    }

    SECTION("date and text compose")
    {
        DateRange range;
        range.to = CivilDate{2026, 8, 31};
        const auto filtered = filter_releases(document, range,
                                              [](const std::string &s) { return contains_ci(s, "e"); });
        // v3 is outside the range; v2 has a match; v1 has no entries and no header match.
        REQUIRE(filtered.size() == 1);
        REQUIRE(filtered[0].release->version == "v2");
    }

    SECTION("nothing matches")
    {
        const auto filtered = filter_releases(document, DateRange::all(),
                                              [](const std::string &s) { return contains_ci(s, "zzz-no-such"); });
        REQUIRE(filtered.empty());
    }
}

TEST_CASE("Markdown export states the range and keeps full SHAs", "[Changelog][export]")
{
    const Document document = parse_document(SAMPLE_JSON);
    DateRange range;
    range.from = CivilDate{2026, 1, 1};
    range.to   = CivilDate{2026, 12, 31};
    const auto filtered = filter_releases(document, range, nullptr);
    REQUIRE(filtered.size() == 2);

    const std::string markdown = export_text(document, filtered, range, "", ExportFormat::Markdown);
    REQUIRE(markdown.find("# Changelog — Example/Repo") != std::string::npos);
    REQUIRE(markdown.find("Exported range: 2026-01-01 to 2026-12-31") != std::string::npos);
    REQUIRE(markdown.find("Search: none") != std::string::npos);
    REQUIRE(markdown.find("Versions: 2, changes: 3") != std::string::npos);
    REQUIRE(markdown.find("## v3 — Chive Dumpling 韭菜餃 (2026-09-07)") != std::string::npos);
    REQUIRE(markdown.find("cccccccccccccccccccccccccccccccccccccccc") != std::string::npos);
    REQUIRE(markdown.find("https://github.com/Example/Repo/commit/dddddddddddddddddddddddddddddddddddddddd") != std::string::npos);
    REQUIRE(markdown.find("**Fixed**: Fix the calendar clipping") != std::string::npos);
    // v1 is outside the range and must not appear.
    REQUIRE(markdown.find("## v1") == std::string::npos);

    const std::string plain = export_text(document, filtered, range, "calendar (regex)", ExportFormat::PlainText);
    REQUIRE(plain.find("Search: calendar (regex)") != std::string::npos);
    REQUIRE(plain.find("[Fixed] Fix the calendar clipping (cccccccccccccccccccccccccccccccccccccccc)") != std::string::npos);
    REQUIRE(plain.find("**") == std::string::npos);

    const std::string empty = export_text(document, {}, DateRange::all(), "", ExportFormat::Markdown);
    REQUIRE(empty.find("No versions match") != std::string::npos);
}
