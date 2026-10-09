#include <catch_main.hpp>

#include "slic3r/GUI/Widgets/CollapsibleFilterState.hpp"

#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace Slic3r::GUI::CollapsibleFilters;

namespace {

// Stand-in for the application config: one value per (section, key).
struct MemoryConfig
{
    std::map<std::pair<std::string, std::string>, std::string> values;
    int writes = 0;

    Section::Read reader()
    {
        return [this](const std::string &section, const std::string &key) {
            const auto it = values.find({section, key});
            return it == values.end() ? std::string() : it->second;
        };
    }

    Section::Write writer()
    {
        return [this](const std::string &section, const std::string &key, const std::string &value) {
            values[{section, key}] = value;
            ++writes;
        };
    }
};

} // namespace

TEST_CASE("Filter rows start expanded and statistics start collapsed", "[CollapsibleFilters]")
{
    REQUIRE(starts_expanded(Purpose::Narrows));
    REQUIRE_FALSE(starts_expanded(Purpose::Describes));

    MemoryConfig config;
    Section filters("notification_center", Purpose::Narrows);
    Section stats("print_statistics", Purpose::Describes);
    filters.restore(config.reader());
    stats.restore(config.reader());
    REQUIRE(filters.expanded());
    REQUIRE_FALSE(stats.expanded());
    // Restoring with nothing stored writes nothing.
    REQUIRE(config.writes == 0);
}

TEST_CASE("The chosen state survives a restart", "[CollapsibleFilters]")
{
    MemoryConfig config;
    {
        Section filters("changelog", Purpose::Narrows);
        filters.restore(config.reader());
        REQUIRE(filters.toggle(config.writer()) == false);
        Section stats("print_statistics", Purpose::Describes);
        stats.restore(config.reader());
        REQUIRE(stats.toggle(config.writer()) == true);
    }
    REQUIRE(config.values.at({config_section(), "changelog"}) == "collapsed");
    REQUIRE(config.values.at({config_section(), "print_statistics"}) == "expanded");

    // A fresh process reads the same values back.
    Section filters_again("changelog", Purpose::Narrows);
    Section stats_again("print_statistics", Purpose::Describes);
    filters_again.restore(config.reader());
    stats_again.restore(config.reader());
    REQUIRE_FALSE(filters_again.expanded());
    REQUIRE(stats_again.expanded());
}

TEST_CASE("Unknown stored values fall back to the purpose default", "[CollapsibleFilters]")
{
    REQUIRE(decode("expanded", Purpose::Describes));
    REQUIRE_FALSE(decode("collapsed", Purpose::Narrows));
    for (const char *junk : {"", "true", "1", "EXPANDED", " collapsed", "open"}) {
        INFO(junk);
        REQUIRE(decode(junk, Purpose::Narrows));
        REQUIRE_FALSE(decode(junk, Purpose::Describes));
    }
    REQUIRE(std::string(encode(true)) == "expanded");
    REQUIRE(std::string(encode(false)) == "collapsed");
}

TEST_CASE("Only safe surface ids are persisted", "[CollapsibleFilters]")
{
    REQUIRE(valid_surface_id("notification_center"));
    REQUIRE(valid_surface_id("export.formats-2"));
    REQUIRE_FALSE(valid_surface_id(""));
    REQUIRE_FALSE(valid_surface_id("Has Space"));
    REQUIRE_FALSE(valid_surface_id("UPPER"));
    REQUIRE_FALSE(valid_surface_id("9starts_with_digit"));
    REQUIRE_FALSE(valid_surface_id(std::string(65, 'a')));
    REQUIRE(valid_surface_id(std::string(64, 'a')));

    MemoryConfig config;
    Section bad("Not A Key", Purpose::Narrows);
    REQUIRE_FALSE(bad.persistent());
    bad.restore(config.reader());
    REQUIRE(bad.expanded());
    // The bar still collapses for this session, it simply is not stored.
    REQUIRE(bad.toggle(config.writer()) == false);
    REQUIRE_FALSE(bad.expanded());
    REQUIRE(config.writes == 0);
}

TEST_CASE("Setting the same state reports no change", "[CollapsibleFilters]")
{
    MemoryConfig config;
    Section filters("project_history", Purpose::Narrows);
    filters.restore(config.reader());
    REQUIRE_FALSE(filters.set_expanded(true, config.writer()));
    REQUIRE(filters.set_expanded(false, config.writer()));
    REQUIRE_FALSE(filters.set_expanded(false, config.writer()));
    REQUIRE_FALSE(filters.expanded());
    REQUIRE(config.values.at({config_section(), "project_history"}) == "collapsed");
}

TEST_CASE("A collapsed bar never hides an active filter without saying so", "[CollapsibleFilters]")
{
    SECTION("nothing active: no disclosure line")
    {
        const Disclosure d = disclose({}, /*expanded=*/false);
        REQUIRE_FALSE(d.visible);
        REQUIRE(d.active == 0);
        REQUIRE(d.named.empty());
        REQUIRE(d.more == 0);
    }
    SECTION("collapsed with active filters names them")
    {
        const Disclosure d = disclose({"Errors", "Search: abc"}, /*expanded=*/false);
        REQUIRE(d.visible);
        REQUIRE(d.active == 2);
        REQUIRE(d.named == std::vector<std::string>{"Errors", "Search: abc"});
        REQUIRE(d.more == 0);
    }
    SECTION("long lists are counted, never dropped")
    {
        const Disclosure d = disclose({"a", "b", "c", "d", "e"}, /*expanded=*/false, /*max_named=*/3);
        REQUIRE(d.visible);
        REQUIRE(d.active == 5);
        REQUIRE(d.named == std::vector<std::string>{"a", "b", "c"});
        REQUIRE(d.more == 2);
        REQUIRE(d.named.size() + d.more == d.active);
    }
    SECTION("an unnamed active filter is still counted")
    {
        const Disclosure d = disclose({"", "Warnings", "   "}, /*expanded=*/false);
        REQUIRE(d.visible);
        REQUIRE(d.active == 3);
        REQUIRE(d.named == std::vector<std::string>{"Warnings"});
        REQUIRE(d.more == 2);
    }
    SECTION("expanded bars show the controls themselves")
    {
        const Disclosure d = disclose({"Errors"}, /*expanded=*/true);
        REQUIRE_FALSE(d.visible);
        REQUIRE(d.active == 1);
    }
    SECTION("max_named of zero still discloses the count")
    {
        const Disclosure d = disclose({"Errors", "Info"}, /*expanded=*/false, /*max_named=*/0);
        REQUIRE(d.visible);
        REQUIRE(d.named.empty());
        REQUIRE(d.more == 2);
    }
}

TEST_CASE("Accessible state always reports exactly one of expanded or collapsed", "[CollapsibleFilters]")
{
    MemoryConfig config;
    Section stats("status_hub_statistics", Purpose::Describes);
    stats.restore(config.reader());
    REQUIRE(accessible_state(stats.expanded()) == AccessibleState::Collapsed);
    stats.toggle(config.writer());
    REQUIRE(accessible_state(stats.expanded()) == AccessibleState::Expanded);
}
