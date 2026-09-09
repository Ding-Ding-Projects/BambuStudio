#include <catch_main.hpp>

#include <set>
#include <string>
#include <vector>

#include "slic3r/GUI/Bulk/BulkActionPlan.hpp"
#include "slic3r/GUI/Bulk/BulkRenamePattern.hpp"
#include "slic3r/GUI/Bulk/BulkSelection.hpp"

using Slic3r::GUI::Bulk::BulkActionPlan;
using Slic3r::GUI::Bulk::BulkItem;
using Slic3r::GUI::Bulk::BulkSelection;
using Slic3r::GUI::Bulk::plan_rename;
using Slic3r::GUI::Bulk::RenameOutcome;
using Slic3r::GUI::Bulk::RenamePlan;
using Slic3r::GUI::Bulk::RenameSpec;
using Slic3r::GUI::Bulk::validate_rename_spec;

// ---------------------------------------------------------------------------
// BulkSelection

TEST_CASE("Select-all distinguishes this page from every match", "[BulkSelection]")
{
    BulkSelection<int> sel;
    const std::vector<int> matches{1, 2, 3, 4, 5, 6};
    const std::vector<int> page{1, 2, 3};

    sel.select_page(page);
    REQUIRE(sel.size() == 3);
    REQUIRE(sel.count_within(matches) == 3);
    REQUIRE(sel.covers(page));
    REQUIRE_FALSE(sel.covers(matches));

    sel.select_all_matches(matches);
    REQUIRE(sel.size() == 6);
    REQUIRE(sel.covers(matches));
}

TEST_CASE("Invert is scoped to the universe and leaves outside ids alone", "[BulkSelection]")
{
    BulkSelection<int> sel;
    sel.set(1, true);
    sel.set(9, true); // outside the universe (hidden by the filter)
    const std::vector<int> universe{1, 2, 3};

    sel.invert(universe);
    REQUIRE_FALSE(sel.contains(1));
    REQUIRE(sel.contains(2));
    REQUIRE(sel.contains(3));
    REQUIRE(sel.contains(9));
    REQUIRE(sel.count_within(universe) == 2);
}

TEST_CASE("Shift-click selects the inclusive range in display order, both directions", "[BulkSelection]")
{
    BulkSelection<std::string> sel;
    const std::vector<std::string> order{"a", "b", "c", "d", "e"};

    sel.select_range(order, "b", "d");
    REQUIRE(sel.size() == 3);
    REQUIRE(sel.contains("b"));
    REQUIRE(sel.contains("c"));
    REQUIRE(sel.contains("d"));

    sel.clear();
    sel.select_range(order, "d", "b");
    REQUIRE(sel.size() == 3);

    // Unknown anchor: only the clicked id. Unknown id: nothing.
    sel.clear();
    sel.select_range(order, "zz", "e");
    REQUIRE(sel.size() == 1);
    REQUIRE(sel.contains("e"));
    sel.clear();
    sel.select_range(order, "a", "zz");
    REQUIRE(sel.empty());
}

TEST_CASE("Selection is retained across a filter change and dropped for vanished ids", "[BulkSelection]")
{
    BulkSelection<int> sel;
    sel.select_all_matches({1, 2, 3, 4});
    // A narrower filter hides 3 and 4 but they still exist: nothing is lost.
    const std::vector<int> narrower{1, 2};
    REQUIRE(sel.count_within(narrower) == 2);
    REQUIRE(sel.size() == 4);
    // Entry 4 is deleted from the collection: retain() drops only that one.
    sel.retain(std::set<int>{1, 2, 3});
    REQUIRE(sel.size() == 3);
    REQUIRE_FALSE(sel.contains(4));
    sel.retain_listed(std::vector<int>{2});
    REQUIRE(sel.size() == 1);
}

TEST_CASE("ordered_within yields selected ids in display order and toggle flips", "[BulkSelection]")
{
    BulkSelection<int> sel;
    sel.toggle(5);
    sel.toggle(1);
    sel.toggle(3);
    sel.toggle(3);
    const std::vector<int> order{1, 2, 3, 4, 5};
    REQUIRE(sel.ordered_within(order) == std::vector<int>{1, 5});
}

// ---------------------------------------------------------------------------
// Rename pattern engine

TEST_CASE("Pattern placeholders expand with index, padding, stem and extension", "[BulkRename]")
{
    RenameSpec spec;
    spec.pattern     = "part_{n}_{stem}{ext}";
    spec.start_index = 7;
    spec.pad         = 3;
    const RenamePlan plan = plan_rename({"cube.stl", "sphere.3mf", "noext"}, spec);
    REQUIRE(plan.error.empty());
    REQUIRE(plan.rows.size() == 3);
    REQUIRE(plan.rows[0].after == "part_007_cube.stl");
    REQUIRE(plan.rows[1].after == "part_008_sphere.3mf");
    REQUIRE(plan.rows[2].after == "part_009_noext");
    REQUIRE(plan.will_change() == 3);
    REQUIRE(plan.skipped() == 0);
    REQUIRE(plan.applicable());
}

TEST_CASE("{i} counts from zero and {{ }} are literal braces", "[BulkRename]")
{
    RenameSpec spec;
    spec.pattern = "{{{i}}} {name}";
    const RenamePlan plan = plan_rename({"a", "b"}, spec);
    REQUIRE(plan.rows[0].after == "{0} a");
    REQUIRE(plan.rows[1].after == "{1} b");
}

TEST_CASE("Plain find/replace replaces every occurrence and honours case sensitivity", "[BulkRename]")
{
    RenameSpec spec;
    spec.find    = "Cube";
    spec.replace = "Box";
    RenamePlan plan = plan_rename({"Cube cube Cube"}, spec);
    REQUIRE(plan.rows[0].after == "Box cube Box");

    spec.case_sensitive = false;
    plan                = plan_rename({"Cube cube CUBE"}, spec);
    REQUIRE(plan.rows[0].after == "Box Box Box");
}

TEST_CASE("Regex find/replace supports back-references and reports a malformed pattern as Invalid", "[BulkRename]")
{
    RenameSpec spec;
    spec.regex   = true;
    spec.find    = "^(\\w+)-(\\d+)$";
    spec.replace = "$2_$1";
    RenamePlan plan = plan_rename({"part-12", "no match here"}, spec);
    REQUIRE(plan.error.empty());
    REQUIRE(plan.rows[0].outcome == RenameOutcome::Changed);
    REQUIRE(plan.rows[0].after == "12_part");
    REQUIRE(plan.rows[1].outcome == RenameOutcome::Unchanged);
    REQUIRE(plan.will_change() == 1);
    REQUIRE(plan.skipped() == 1);

    spec.find = "(unclosed";
    REQUIRE_FALSE(validate_rename_spec(spec).empty());
    plan = plan_rename({"x"}, spec);
    REQUIRE_FALSE(plan.error.empty());
    REQUIRE(plan.rows[0].outcome == RenameOutcome::Invalid);
    REQUIRE_FALSE(plan.applicable());
}

TEST_CASE("Unknown placeholder and unbalanced braces are spec errors", "[BulkRename]")
{
    RenameSpec spec;
    spec.pattern = "{nope}";
    REQUIRE(validate_rename_spec(spec).find("Unknown placeholder") != std::string::npos);
    spec.pattern = "{name";
    REQUIRE(validate_rename_spec(spec).find("Unbalanced") != std::string::npos);
    spec.pattern = "name}";
    REQUIRE(validate_rename_spec(spec).find("Unbalanced") != std::string::npos);
    spec.pattern = std::string(600, 'x');
    REQUIRE_FALSE(validate_rename_spec(spec).empty());
}

TEST_CASE("Collisions against reserved names and between planned names are refused on both sides", "[BulkRename]")
{
    RenameSpec spec;
    spec.pattern = "item";
    // Two rows both become "item": neither may land.
    RenamePlan plan = plan_rename({"a", "b"}, spec);
    REQUIRE(plan.rows[0].outcome == RenameOutcome::Collision);
    REQUIRE(plan.rows[1].outcome == RenameOutcome::Collision);
    REQUIRE(plan.has_collisions());
    REQUIRE_FALSE(plan.applicable());

    // One row becomes a name an unselected item already holds.
    spec.pattern = "{name}_x";
    plan         = plan_rename({"a"}, spec, std::set<std::string>{"a_x"});
    REQUIRE(plan.rows[0].outcome == RenameOutcome::Collision);
    REQUIRE(plan.rows[0].reason.find("already exists") != std::string::npos);

    // A planned name equal to another selected row's untouched name collides too.
    spec.pattern.clear();
    spec.find    = "a";
    spec.replace = "b";
    plan         = plan_rename({"a", "b"}, spec);
    REQUIRE(plan.rows[0].outcome == RenameOutcome::Collision);
    REQUIRE(plan.rows[1].outcome == RenameOutcome::Unchanged);
}

TEST_CASE("Empty result and unchanged names are skipped with a reason", "[BulkRename]")
{
    RenameSpec spec;
    spec.find = "gone";
    spec.replace.clear();
    RenamePlan plan = plan_rename({"gone", "kept"}, spec);
    REQUIRE(plan.rows[0].outcome == RenameOutcome::Invalid);
    REQUIRE(plan.rows[0].reason.find("empty") != std::string::npos);
    REQUIRE(plan.rows[1].outcome == RenameOutcome::Unchanged);
    REQUIRE_FALSE(plan.rows[1].reason.empty());
    REQUIRE(plan.selected() == 2);
    REQUIRE(plan.will_change() == 0);
}

TEST_CASE("Find/replace runs before the pattern sees {name}", "[BulkRename]")
{
    RenameSpec spec;
    spec.find    = " ";
    spec.replace = "_";
    spec.pattern = "{n}-{name}";
    const RenamePlan plan = plan_rename({"my part"}, spec);
    REQUIRE(plan.rows[0].after == "1-my_part");
}

// ---------------------------------------------------------------------------
// BulkActionPlan counts

TEST_CASE("Plan counts are derived so selected, will-change and skipped always add up", "[BulkActionPlan]")
{
    BulkActionPlan plan;
    plan.action = "Delete objects";
    plan.items.push_back(BulkItem::changed("cube"));
    plan.items.push_back(BulkItem::skipped("locked", "Object is locked"));
    plan.items.push_back(BulkItem::changed("sphere", "detail", "sphere_2"));
    REQUIRE(plan.selected() == 3);
    REQUIRE(plan.will_change() == 2);
    REQUIRE(plan.skipped() == 1);
    REQUIRE(plan.selected() == plan.will_change() + plan.skipped());
    REQUIRE(plan.applicable());
    REQUIRE(plan.has_transform());
    REQUIRE(plan.changed_labels() == std::vector<std::string>{"cube", "sphere"});

    BulkActionPlan nothing;
    nothing.items.push_back(BulkItem::skipped("x", "reason"));
    REQUIRE_FALSE(nothing.applicable());
    REQUIRE_FALSE(nothing.has_transform());
}
