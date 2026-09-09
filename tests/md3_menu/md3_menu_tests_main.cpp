#include <catch_main.hpp>

#include <wx/init.h>
#include <wx/menu.h>

#include "slic3r/GUI/Widgets/MD3MenuModel.hpp"

using namespace MD3::Menu;

// wxMenu / wxMenuItem need the wx library initialised (MSW creates a real
// HMENU behind each wxMenu). catch_main.hpp enables external interfaces, so a
// run listener brackets the whole run with wxInitialize / wxUninitialize.
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

const Item *find_by_id(const std::vector<Item> &rows, int id)
{
    for (const Item &row : rows)
        if (row.id == id)
            return &row;
    return nullptr;
}

std::function<bool(const wxString &)> contains(const wxString &needle)
{
    return [needle](const wxString &s) { return s.Lower().Contains(needle.Lower()); };
}

const wxRect kDisplay(0, 0, 1920, 1080);
const wxSize kWanted(240, 400);

bool inside(const wxRect &r, const wxRect &display)
{
    return r.x >= display.x && r.y >= display.y && r.GetRight() <= display.GetRight() &&
           r.GetBottom() <= display.GetBottom();
}

} // namespace

TEST_CASE("split_label_and_shortcut strips mnemonics and splits on tab", "[MD3Menu]")
{
    const auto split = split_label_and_shortcut("&Save Project\tCtrl+S");
    REQUIRE(split.first == "Save Project");
    REQUIRE(split.second == "Ctrl+S");

    const auto plain = split_label_and_shortcut("Export &G-code");
    REQUIRE(plain.first == "Export G-code");
    REQUIRE(plain.second.IsEmpty());

    const auto literal_amp = split_label_and_shortcut("Slice && Print\t  F5 ");
    REQUIRE(literal_amp.first == "Slice & Print");
    REQUIRE(literal_amp.second == "F5");
}

TEST_CASE("snapshot mirrors every wxMenu item kind and enabled state", "[MD3Menu]")
{
    wxMenu  menu;
    wxMenu *sub = new wxMenu;
    sub->Append(1001, "Child &A");
    sub->Append(1002, "Child B");

    menu.Append(101, "&Open\tCtrl+O", "Open a project");
    menu.AppendCheckItem(102, "Show &Grid");
    menu.AppendRadioItem(103, "Small");
    menu.AppendRadioItem(104, "Large");
    menu.AppendSeparator();
    menu.AppendSubMenu(sub, "&Recent");
    menu.Enable(104, false);
    menu.Check(102, true);
    menu.Check(103, true);

    const std::vector<Item> rows = snapshot(menu);
    REQUIRE(rows.size() == 6);

    const Item *open = find_by_id(rows, 101);
    REQUIRE(open != nullptr);
    REQUIRE(open->kind == Item::Normal);
    REQUIRE(open->label == "Open");
    REQUIRE(open->shortcut == "Ctrl+O");
    REQUIRE(open->help == "Open a project");
    REQUIRE(open->enabled);
    REQUIRE(open->source == menu.FindItem(101));

    const Item *grid = find_by_id(rows, 102);
    REQUIRE(grid->kind == Item::Check);
    REQUIRE(grid->checked);

    const Item *small = find_by_id(rows, 103);
    REQUIRE(small->kind == Item::Radio);
    REQUIRE(small->checked);

    const Item *large = find_by_id(rows, 104);
    REQUIRE(large->kind == Item::Radio);
    REQUIRE_FALSE(large->checked);
    REQUIRE_FALSE(large->enabled);

    REQUIRE(rows[4].kind == Item::Separator);
    REQUIRE_FALSE(rows[4].actionable());

    const Item &recent = rows[5];
    REQUIRE(recent.kind == Item::Submenu);
    REQUIRE(recent.label == "Recent");
    REQUIRE(recent.submenu == sub);
}

TEST_CASE("snapshot fills the bilingual secondary from the lookup hook", "[MD3Menu]")
{
    wxMenu menu;
    menu.Append(201, "&Language");
    menu.Append(202, "Untranslated");

    const std::vector<Item> rows = snapshot(menu, [](const wxString &label) -> wxString {
        if (label == "Language")
            return wxString::FromUTF8("\xE8\xAA\x9E\xE8\xA8\x80");
        return wxString();
    });
    REQUIRE(rows[0].secondary == wxString::FromUTF8("\xE8\xAA\x9E\xE8\xA8\x80"));
    REQUIRE(rows[1].secondary.IsEmpty());
}

TEST_CASE("filter hides non-matching rows and collapses orphan separators", "[MD3Menu]")
{
    wxMenu menu;
    menu.AppendSeparator();          // 0: leading
    menu.Append(301, "Alpha");       // 1
    menu.AppendSeparator();          // 2
    menu.AppendSeparator();          // 3: doubled
    menu.Append(302, "Beta");        // 4
    menu.AppendSeparator();          // 5
    menu.Append(303, "Alphabet");    // 6
    menu.AppendSeparator();          // 7: trailing

    const std::vector<Item> rows = snapshot(menu);

    SECTION("no predicate keeps every actionable row and one separator per gap")
    {
        const std::vector<int> visible = filter(rows, {});
        REQUIRE(visible == std::vector<int>{1, 3, 4, 5, 6});
    }

    SECTION("a query removes rows and the separators they orphan")
    {
        const std::vector<int> visible = filter(rows, contains("alpha"));
        // Alpha, separator, Alphabet -- Beta and the doubled/leading/trailing
        // separators are gone.
        REQUIRE(visible.size() == 3);
        REQUIRE(visible[0] == 1);
        REQUIRE_FALSE(rows[visible[1]].actionable());
        REQUIRE(visible[2] == 6);
    }

    SECTION("a query matching nothing yields no rows at all")
    {
        REQUIRE(filter(rows, contains("zzz")).empty());
    }
}

TEST_CASE("filter keeps a submenu whose descendant matches", "[MD3Menu]")
{
    wxMenu  menu;
    wxMenu *tools = new wxMenu;
    wxMenu *deep  = new wxMenu;
    deep->Append(403, "Calibrate Flow");
    tools->Append(402, "Arrange");
    tools->AppendSubMenu(deep, "Calibration");
    menu.Append(401, "Open");
    menu.AppendSubMenu(tools, "Tools");
    menu.Append(404, "Quit");

    const std::vector<Item> rows = snapshot(menu);
    const std::vector<int>  visible = filter(rows, contains("flow"));
    REQUIRE(visible.size() == 1);
    REQUIRE(rows[visible[0]].kind == Item::Submenu);
    REQUIRE(rows[visible[0]].label == "Tools");

    REQUIRE(filter(rows, contains("quit")) == std::vector<int>{2});
}

TEST_CASE("place_root avoids the anchor and stays on the display at every corner", "[MD3Menu]")
{
    const wxSize anchor_size(120, 32);
    const wxRect anchors[] = {
        wxRect(wxPoint(0, 0), anchor_size),                                                     // top-left
        wxRect(wxPoint(kDisplay.GetRight() - anchor_size.x + 1, 0), anchor_size),               // top-right
        wxRect(wxPoint(0, kDisplay.GetBottom() - anchor_size.y + 1), anchor_size),              // bottom-left
        wxRect(wxPoint(kDisplay.GetRight() - anchor_size.x + 1, kDisplay.GetBottom() - anchor_size.y + 1),
               anchor_size),                                                                    // bottom-right
        wxRect(900, 500, 120, 32),                                                              // middle
    };

    for (const wxRect &anchor : anchors) {
        const Placement p = place_root(anchor, kWanted, kDisplay);
        INFO("anchor " << anchor.x << "," << anchor.y);
        REQUIRE(inside(p.rect, kDisplay));
        REQUIRE_FALSE(p.rect.Intersects(anchor));
        REQUIRE_FALSE(p.clipped);
        REQUIRE(p.rect.GetSize() == kWanted);
    }

    SECTION("below is preferred when it fits, above otherwise")
    {
        const Placement below = place_root(anchors[0], kWanted, kDisplay);
        REQUIRE(below.rect.y == anchors[0].GetBottom() + 1);
        REQUIRE_FALSE(below.flipped_up);

        const Placement above = place_root(anchors[2], kWanted, kDisplay);
        REQUIRE(above.flipped_up);
        REQUIRE(above.rect.GetBottom() == anchors[2].y - 1);
    }

    SECTION("a surface taller than the free space is clipped rather than overlapping")
    {
        const wxRect    display(0, 0, 800, 300);
        const wxRect    anchor(0, 140, 800, 20); // full-width bar mid-screen
        const Placement p = place_root(anchor, wxSize(240, 1000), display);
        REQUIRE(p.clipped);
        REQUIRE(inside(p.rect, display));
        REQUIRE_FALSE(p.rect.Intersects(anchor));
    }
}

TEST_CASE("place_submenu opens to the right and flips left at the display edge", "[MD3Menu]")
{
    SECTION("right of the row, top-aligned")
    {
        const wxRect    row(100, 200, 240, 40);
        const Placement p = place_submenu(row, kWanted, kDisplay);
        REQUIRE(p.rect.x == row.GetRight() + 1);
        REQUIRE(p.rect.y == row.y);
        REQUIRE(inside(p.rect, kDisplay));
    }

    SECTION("flips to the left when the right edge would overflow")
    {
        const wxRect    row(kDisplay.GetRight() - 240 + 1 - 50, 200, 240, 40);
        const Placement p = place_submenu(row, kWanted, kDisplay);
        REQUIRE(p.rect.GetRight() == row.x - 1);
        REQUIRE(inside(p.rect, kDisplay));
    }

    SECTION("clamps vertically near the bottom")
    {
        const wxRect    row(100, kDisplay.GetBottom() - 60, 240, 40);
        const Placement p = place_submenu(row, kWanted, kDisplay);
        REQUIRE(inside(p.rect, kDisplay));
        REQUIRE(p.rect.GetBottom() == kDisplay.GetBottom());
    }
}
