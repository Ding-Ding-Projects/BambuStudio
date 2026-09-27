#include <catch_main.hpp>

#include <wx/init.h>

#include "slic3r/GUI/Widgets/TabStripModel.hpp"

using namespace MD3::Tabs;

// wxColour / wxString need the wx library initialised on MSW.
struct WxRuntimeListener : Catch::TestEventListenerBase
{
    using TestEventListenerBase::TestEventListenerBase;
    void testRunStarting(Catch::TestRunInfo const &) override { m_ok = wxInitialize(); }
    void testRunEnded(Catch::TestRunStats const &) override
    {
        if (m_ok)
            wxUninitialize();
    }
    bool m_ok = false;
};
CATCH_REGISTER_LISTENER(WxRuntimeListener)

namespace {

Tab make(const std::string &id, const char *title, bool pinned = false)
{
    Tab t;
    t.id     = id;
    t.title  = wxString::FromUTF8(title);
    t.pinned = pinned;
    return t;
}

Model five()
{
    Model m;
    m.add(make("appearance", "Appearance"));
    m.add(make("general", "General"));
    m.add(make("user", "User"));
    m.add(make("3d", "3D"));
    m.add(make("other", "Other"));
    return m;
}

std::vector<std::string> ids(const Model &m)
{
    std::vector<std::string> out;
    for (const Tab &t : m.tabs())
        out.push_back(t.id);
    return out;
}

// Plain substring, case-insensitive, like SearchField::textMatches(regex=false).
bool plain(const wxString &q, const wxString &c) { return c.Lower().Contains(q.Lower()); }

} // namespace

TEST_CASE("Adding tabs keeps the pinned region leading", "[TabStrip][pin]")
{
    Model m;
    m.add(make("a", "A"));
    m.add(make("b", "B", /*pinned*/ true));
    m.add(make("c", "C"));
    m.add(make("d", "D", true));
    REQUIRE(ids(m) == std::vector<std::string>{"b", "d", "a", "c"});
    REQUIRE(m.pinned_count() == 2);
    REQUIRE(m.active() == "a"); // first added becomes active
}

TEST_CASE("Pinning and unpinning move tabs between regions", "[TabStrip][pin]")
{
    Model m = five();
    REQUIRE(m.set_pinned("3d", true));
    REQUIRE(ids(m) == std::vector<std::string>{"3d", "appearance", "general", "user", "other"});
    REQUIRE(m.set_pinned("other", true));
    REQUIRE(ids(m) == std::vector<std::string>{"3d", "other", "appearance", "general", "user"});
    REQUIRE(m.pinned_count() == 2);
    REQUIRE(m.set_pinned("3d", false));
    // Unpinned tab becomes the first unpinned one.
    REQUIRE(ids(m) == std::vector<std::string>{"other", "3d", "appearance", "general", "user"});
    REQUIRE_FALSE(m.set_pinned("3d", false)); // no-op
}

TEST_CASE("Reorder is clamped to the tab's own region", "[TabStrip][order]")
{
    Model m = five();
    m.set_pinned("other", true); // other | appearance general user 3d
    REQUIRE(m.move(1, 3));       // appearance -> after user
    REQUIRE(ids(m) == std::vector<std::string>{"other", "general", "user", "appearance", "3d"});
    // Dragging an unpinned tab onto the pinned slot lands right after the region.
    REQUIRE(m.move(4, 0));
    REQUIRE(ids(m) == std::vector<std::string>{"other", "3d", "general", "user", "appearance"});
    REQUIRE(m.pinned_count() == 1);
    // A pinned tab cannot leave the pinned region.
    REQUIRE_FALSE(m.move(0, 4));
    REQUIRE(ids(m)[0] == "other");
}

TEST_CASE("Groups: create, assign, collapse, remove", "[TabStrip][group]")
{
    Model     m   = five();
    const int gid = m.create_group("Look", wxColour(0x1C, 0xA7, 0x52));
    REQUIRE(gid >= 1);
    REQUIRE(m.assign_group("appearance", gid));
    REQUIRE(m.assign_group("other", gid));
    // Grouped tabs are kept contiguous: "other" moves next to "appearance".
    REQUIRE(ids(m) == std::vector<std::string>{"appearance", "other", "general", "user", "3d"});
    REQUIRE(m.members(gid) == std::vector<std::string>{"appearance", "other"});

    m.set_active("general");
    REQUIRE(m.set_group_collapsed(gid, true));
    // Collapsed group: no member displays unless active or revealed.
    REQUIRE(m.displayed_indices() == std::vector<int>{2, 3, 4});
    m.set_active("appearance");
    REQUIRE(m.displayed_indices() == std::vector<int>{0, 2, 3, 4});
    m.reveal("other");
    REQUIRE(m.displayed_indices() == std::vector<int>{0, 1, 2, 3, 4});
    REQUIRE(m.group(gid)->collapsed); // reveal never touches the preference
    m.clear_reveal();
    REQUIRE(m.displayed_indices() == std::vector<int>{0, 2, 3, 4});

    REQUIRE(m.rename_group(gid, "Looks"));
    REQUIRE(m.group(gid)->name == "Looks");
    REQUIRE(m.remove_group(gid));
    REQUIRE(m.group(gid) == nullptr);
    REQUIRE(m.find("other")->group_id == -1);
    REQUIRE(m.size() == 5); // removing a group never closes tabs
    REQUIRE_FALSE(m.assign_group("user", 999));
}

TEST_CASE("Hidden tabs stay in order and hand off the active slot", "[TabStrip][hide]")
{
    Model m = five();
    m.set_active("appearance");
    REQUIRE(m.set_hidden("appearance", true));
    REQUIRE(m.active() == "general");
    REQUIRE(m.hidden_indices() == std::vector<int>{0});
    REQUIRE(m.displayed_indices() == std::vector<int>{1, 2, 3, 4});
    REQUIRE(m.set_hidden("appearance", false));
    REQUIRE(ids(m)[0] == "appearance"); // position preserved
}

TEST_CASE("Removing the active tab picks the displayed neighbour", "[TabStrip][order]")
{
    Model m = five();
    m.set_active("user");
    REQUIRE(m.remove("user"));
    REQUIRE(m.active() == "3d");
    m.set_active("other");
    REQUIRE(m.remove("other"));
    REQUIRE(m.active() == "3d");
    REQUIRE(m.index_of("user") == -1);
}

TEST_CASE("JSON round trip preserves order, pins, groups, collapsed state and edge", "[TabStrip][persist]")
{
    Model m = five();
    m.set_edge(DockEdge::Bottom);
    m.set_pinned("3d", true);
    const int gid = m.create_group(wxString::FromUTF8("\xE7\xB4\x85 red"), wxColour(255, 0, 0));
    m.assign_group("general", gid);
    m.assign_group("user", gid);
    m.set_group_collapsed(gid, true);
    m.set_hidden("other", true);
    m.set_active("general");
    m.reveal("user"); // transient: must NOT survive

    const std::string text = m.to_json();
    Model             r;
    REQUIRE(r.from_json(text));
    REQUIRE(ids(r) == ids(m));
    REQUIRE(r.pinned_count() == 1);
    REQUIRE(r.at(0).id == "3d");
    REQUIRE(r.edge() == DockEdge::Bottom);
    REQUIRE(r.active() == "general");
    REQUIRE(r.find("other")->hidden);
    REQUIRE(r.groups().size() == 1);
    REQUIRE(r.group(gid) != nullptr);
    REQUIRE(r.group(gid)->name == m.group(gid)->name);
    REQUIRE(r.group(gid)->color == wxColour(255, 0, 0));
    REQUIRE(r.group(gid)->collapsed);
    REQUIRE(r.members(gid) == std::vector<std::string>{"general", "user"});
    REQUIRE(r.revealed().empty());
    // Same JSON again is byte-identical (stable serialisation).
    REQUIRE(r.to_json() == text);
    // A new group after the round trip never reuses a persisted id.
    REQUIRE(r.create_group("next", wxColour(1, 2, 3)) == gid + 1);
}

TEST_CASE("from_json rejects garbage and drops dangling group references", "[TabStrip][persist]")
{
    Model m = five();
    REQUIRE_FALSE(m.from_json("not json"));
    REQUIRE_FALSE(m.from_json("[1,2,3]"));
    REQUIRE(m.size() == 5); // untouched
    REQUIRE(m.from_json(R"({"version":1,"edge":"sideways","active":"ghost",)"
                        R"("tabs":[{"id":"x","title":"X","group":42},{"id":"x","title":"dup"},{"id":""}],)"
                        R"("groups":[{"id":-3,"name":"bad"}]})"));
    REQUIRE(m.size() == 1);
    REQUIRE(m.at(0).group_id == -1);
    REQUIRE(m.groups().empty());
    REQUIRE(m.active().empty());
    REQUIRE(m.edge() == DockEdge::Left); // unknown edge keeps the prior default
}

TEST_CASE("adopt_layout applies a saved layout to code-built tabs", "[TabStrip][persist]")
{
    Model saved = five();
    saved.set_pinned("other", true);
    saved.set_hidden("user", true);
    const int gid = saved.create_group("Extras", wxColour(0, 0, 255));
    saved.assign_group("3d", gid);
    saved.set_edge(DockEdge::Right);
    saved.move(2, 3); // general after user: other appearance user general 3d
    saved.remove("appearance"); // a section that no longer ships
    saved.set_active("general");

    Model fresh = five();
    fresh.add(make("developer", "Developer Tools")); // a section the save never saw
    fresh.adopt_layout(saved);
    REQUIRE(ids(fresh) == std::vector<std::string>{"other", "user", "general", "3d", "appearance", "developer"});
    REQUIRE(fresh.pinned_count() == 1);
    REQUIRE(fresh.find("user")->hidden);
    REQUIRE(fresh.find("3d")->group_id == gid);
    REQUIRE(fresh.edge() == DockEdge::Right);
    REQUIRE(fresh.active() == "general");
    REQUIRE(fresh.find("appearance")->title == "Appearance"); // title from code, not the save
}

TEST_CASE("Overflow keeps pinned tabs visible and reserves the button", "[TabStrip][overflow]")
{
    SECTION("horizontal strip, widths")
    {
        std::vector<int>  w{120, 120, 120, 120, 120};
        std::vector<bool> pinned{false, false, false, false, false};
        OverflowResult    r = compute_overflow(w, pinned, 700, 40, 4);
        REQUIRE_FALSE(r.needs_button); // 5*120 + 4*4 = 616 fits
        REQUIRE(r.visible.size() == 5);
        r = compute_overflow(w, pinned, 400, 40, 4);
        REQUIRE(r.needs_button);
        // budget 400-40-4 = 356 -> 120 + 124 + 124 = 368 > 356, so two fit.
        REQUIRE(r.visible == std::vector<int>{0, 1});
        REQUIRE(r.hidden == std::vector<int>{2, 3, 4});
    }
    SECTION("vertical strip, heights, pinned always visible")
    {
        std::vector<int>  h{44, 44, 44, 44, 44, 44};
        std::vector<bool> pinned{true, false, false, false, false, true};
        OverflowResult    r = compute_overflow(h, pinned, 150, 44, 2);
        REQUIRE(r.needs_button);
        // budget 150-44-2 = 104: pinned 0 (44), tab1 (46 -> 90), tab2 would be 136.
        REQUIRE(r.visible == std::vector<int>{0, 1, 5});
        REQUIRE(r.hidden == std::vector<int>{2, 3, 4});
    }
    SECTION("nothing fits but pinned")
    {
        std::vector<int>  h{44, 44};
        std::vector<bool> pinned{true, false};
        OverflowResult    r = compute_overflow(h, pinned, 10, 44, 2);
        REQUIRE(r.visible == std::vector<int>{0});
        REQUIRE(r.hidden == std::vector<int>{1});
        REQUIRE(r.needs_button);
    }
}

TEST_CASE("Bulk close refuses empty and invalid patterns", "[TabStrip][bulkclose]")
{
    Model          m = five();
    ClosePredicate p;
    p.text         = "   ";
    ClosePreview r = preview_bulk_close(m, p, plain);
    REQUIRE_FALSE(r.valid);
    REQUIRE(r.ids.empty());
    p.text  = "(";
    p.regex = true;
    r       = preview_bulk_close(m, p, plain, /*pattern_ok*/ false);
    REQUIRE_FALSE(r.valid);
    REQUIRE_FALSE(r.reason.IsEmpty());
}

TEST_CASE("Bulk close predicate and its inverse partition the strip, pinned excluded by default", "[TabStrip][bulkclose]")
{
    Model m = five();
    m.set_pinned("general", true); // "General" contains "e"
    ClosePredicate contains;
    contains.text = "e";           // Appearance, General, User, Other
    ClosePredicate not_contains = contains;
    not_contains.invert         = true; // 3D

    const ClosePreview a = preview_bulk_close(m, contains, plain);
    const ClosePreview b = preview_bulk_close(m, not_contains, plain);
    REQUIRE(a.valid);
    REQUIRE(b.valid);
    REQUIRE(a.ids == std::vector<std::string>{"appearance", "user", "other"});
    REQUIRE(a.protected_pinned == 1);
    REQUIRE(b.ids == std::vector<std::string>{"3d"});
    REQUIRE(b.protected_pinned == 0);
    // Predicate + inverse cover every unpinned tab exactly once.
    std::vector<std::string> all = a.ids;
    all.insert(all.end(), b.ids.begin(), b.ids.end());
    std::sort(all.begin(), all.end());
    std::vector<std::string> expect{"3d", "appearance", "other", "user"};
    REQUIRE(all == expect);
    REQUIRE(a.mode.Contains("Containing text"));
    REQUIRE(b.mode.Contains("Not containing text"));
    REQUIRE(a.mode.Contains("plain"));

    contains.include_pinned = true;
    const ClosePreview c    = preview_bulk_close(m, contains, plain);
    REQUIRE(c.ids == std::vector<std::string>{"general", "appearance", "user", "other"});
    REQUIRE(c.protected_pinned == 0);
}

TEST_CASE("Search results carry surface, strip, group, pinned and hidden state", "[TabStrip][search]")
{
    Model     m   = five();
    const int gid = m.create_group("Look and feel", wxColour(1, 2, 3));
    m.assign_group("appearance", gid);
    m.set_group_collapsed(gid, true);
    m.set_pinned("general", true);
    m.set_hidden("other", true);

    auto hits = search_tabs(m, "Preferences", "Sections", "e", plain);
    REQUIRE(hits.size() == 4);
    const SearchHit &ap = hits[1]; // general(pinned) first, then appearance
    REQUIRE(hits[0].tab_id == "general");
    REQUIRE(hits[0].pinned);
    REQUIRE(ap.tab_id == "appearance");
    REQUIRE(ap.group == "Look and feel");
    REQUIRE(ap.group_id == gid);
    REQUIRE(ap.group_collapsed);
    REQUIRE(ap.surface == "Preferences");
    REQUIRE(ap.strip == "Sections");
    REQUIRE(ap.describe().Contains("(collapsed)"));
    REQUIRE(hits[3].hidden);
    REQUIRE(hits[3].describe().Contains("[hidden]"));

    // Inside one group only.
    auto in_group = search_tabs(m, "Preferences", "Sections", "a", plain, gid);
    REQUIRE(in_group.size() == 1);
    REQUIRE(in_group[0].tab_id == "appearance");

    // Groups by name.
    auto groups = search_groups(m, "Preferences", "Sections", "feel", plain);
    REQUIRE(groups.size() == 1);
    REQUIRE(groups[0].is_group);
    REQUIRE(groups[0].group_id == gid);
    REQUIRE(search_groups(m, "Preferences", "Sections", "zzz", plain).empty());
}

TEST_CASE("Arrow keys follow the strip orientation", "[TabStrip][a11y]")
{
    REQUIRE(arrow_step(DockEdge::Left, WXK_DOWN) == 1);
    REQUIRE(arrow_step(DockEdge::Left, WXK_UP) == -1);
    REQUIRE(arrow_step(DockEdge::Left, WXK_RIGHT) == 0);
    REQUIRE(arrow_step(DockEdge::Right, WXK_DOWN) == 1);
    REQUIRE(arrow_step(DockEdge::Top, WXK_RIGHT) == 1);
    REQUIRE(arrow_step(DockEdge::Top, WXK_LEFT) == -1);
    REQUIRE(arrow_step(DockEdge::Bottom, WXK_DOWN) == 0);
    REQUIRE(is_vertical(DockEdge::Left));
    REQUIRE_FALSE(is_vertical(DockEdge::Bottom));

    Model m = five();
    m.set_hidden("general", true);
    REQUIRE(step_displayed(m, 0, 1) == 2);  // skips the hidden tab
    REQUIRE(step_displayed(m, 0, -1) == 4); // wraps
    REQUIRE(step_displayed(m, 4, 1) == 0);
}

TEST_CASE("Dock edge keys round trip", "[TabStrip][dock]")
{
    for (DockEdge e : {DockEdge::Left, DockEdge::Right, DockEdge::Top, DockEdge::Bottom})
        REQUIRE(dock_edge_from_key(dock_edge_key(e), DockEdge::Top) == e);
    REQUIRE(dock_edge_from_key("", DockEdge::Top) == DockEdge::Top);
}
