#ifndef slic3r_GUI_MD3MenuModel_hpp_
#define slic3r_GUI_MD3MenuModel_hpp_

// Header-only, GUI_App-free model behind the Material Design 3 popup menu
// (MD3Menu.hpp). It turns a wxMenu into a flat row list, filters that list
// against a search predicate, and places the popup surface on screen. Keeping
// this separate from the widget lets tests/md3_menu exercise every rule with
// only wx core linked and no running application object.

#include <wx/bitmap.h>
#include <wx/gdicmn.h>
#include <wx/menu.h>
#include <wx/string.h>

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace MD3 { namespace Menu {

struct Item
{
    enum Kind { Normal, Check, Radio, Separator, Submenu };

    int         id { wxID_NONE };
    wxString    label;     // mnemonic-free label without the accelerator part
    wxString    secondary; // Cantonese counterpart when bilingual, else empty
    wxString    shortcut;  // "Ctrl+S" style text, may be empty
    wxString    help;      // wxMenuItem help string (tooltip)
    wxBitmap    icon;
    Kind        kind { Normal };
    bool        enabled { true };
    bool        checked { false };
    wxMenu *    submenu { nullptr }; // non-null only for Kind::Submenu
    wxMenuItem *source { nullptr };  // the wx item this row mirrors

    bool actionable() const { return kind != Separator; }
};

// Strips mnemonic ampersands ("&Save" -> "Save", "&&" -> "&") and splits the
// accelerator part off at the first tab: "&Save Project\tCtrl+S" ->
// {"Save Project", "Ctrl+S"}. Whitespace around both halves is trimmed.
inline std::pair<wxString, wxString> split_label_and_shortcut(const wxString &raw)
{
    wxString label = raw;
    wxString shortcut;
    const int tab = label.Find('\t');
    if (tab != wxNOT_FOUND) {
        shortcut = label.Mid(tab + 1);
        label    = label.Left(tab);
    }
    // wxMenuItem::GetLabelText already does the mnemonic stripping we want
    // (handles "&&" as a literal ampersand).
    label = wxMenuItem::GetLabelText(label);
    label.Trim(true).Trim(false);
    shortcut.Trim(true).Trim(false);
    return {label, shortcut};
}

// Optional bilingual secondary-label provider. The widget layer wires this to
// the language-mode service; tests can pass a lambda or leave it empty.
using SecondaryLookup = std::function<wxString(const wxString &label)>;

// Flatten one menu level into rows. Calls menu.UpdateUI() first so the
// wxEVT_UPDATE_UI lambdas registered by append_menu_item (wxExtensions.cpp)
// have refreshed enabled/checked state before it is read.
inline std::vector<Item> snapshot(wxMenu &menu, const SecondaryLookup &secondary = {})
{
    menu.UpdateUI();

    std::vector<Item> rows;
    for (wxMenuItem *item : menu.GetMenuItems()) {
        if (!item)
            continue;
        Item row;
        row.id     = item->GetId();
        row.source = item;

        if (item->IsSeparator()) {
            row.kind = Item::Separator;
            rows.push_back(row);
            continue;
        }

        auto split  = split_label_and_shortcut(item->GetItemLabel());
        row.label   = split.first;
        row.shortcut = split.second;
#if wxUSE_ACCEL
        if (wxAcceleratorEntry *accel = item->GetAccel()) {
            const wxString text = accel->ToString();
            if (!text.IsEmpty())
                row.shortcut = text;
            delete accel;
        }
#endif
        row.help    = item->GetHelp();
        row.enabled = item->IsEnabled();
        row.checked = item->IsCheckable() && item->IsChecked();
        if (item->GetBitmap().IsOk())
            row.icon = item->GetBitmap();

        if (item->GetSubMenu()) {
            row.kind    = Item::Submenu;
            row.submenu = item->GetSubMenu();
        } else if (item->GetKind() == wxITEM_CHECK) {
            row.kind = Item::Check;
        } else if (item->GetKind() == wxITEM_RADIO) {
            row.kind = Item::Radio;
        } else {
            row.kind = Item::Normal;
        }

        if (secondary && !row.label.IsEmpty()) {
            const wxString s = secondary(row.label);
            if (!s.IsEmpty() && s != row.label)
                row.secondary = s;
        }
        rows.push_back(row);
    }
    return rows;
}

namespace detail {

// True when this row, or any actionable row reachable through its submenu
// chain, satisfies the predicate. Submenus are snapshotted on demand.
inline bool row_or_descendant_matches(const Item &row,
                                      const std::function<bool(const wxString &)> &matches,
                                      const SecondaryLookup &secondary,
                                      int depth)
{
    if (!row.actionable())
        return false;
    wxString haystack = row.label;
    if (!row.secondary.IsEmpty())
        haystack << ' ' << row.secondary;
    if (matches(haystack))
        return true;
    if (row.kind == Item::Submenu && row.submenu && depth < 8) {
        for (const Item &child : snapshot(*row.submenu, secondary))
            if (row_or_descendant_matches(child, matches, secondary, depth + 1))
                return true;
    }
    return false;
}

} // namespace detail

// Returns the indices of rows that stay visible under the predicate. Non-
// matching actionable rows are hidden; a Submenu row survives when any
// descendant label matches; separators are kept only when they sit between two
// visible actionable rows (no leading, trailing or doubled separators).
inline std::vector<int> filter(const std::vector<Item> &rows,
                               const std::function<bool(const wxString &)> &matches,
                               const SecondaryLookup &secondary = {})
{
    std::vector<int> visible;
    visible.reserve(rows.size());
    bool pending_separator = false;
    bool have_actionable   = false;
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const Item &row = rows[i];
        if (!row.actionable()) {
            // Remember it; emit only once the next actionable row is confirmed.
            if (have_actionable)
                pending_separator = true;
            continue;
        }
        const bool keep = !matches || detail::row_or_descendant_matches(row, matches, secondary, 0);
        if (!keep)
            continue;
        if (pending_separator) {
            // Find the most recent separator index preceding i.
            for (int j = i - 1; j >= 0; --j) {
                if (!rows[j].actionable()) {
                    visible.push_back(j);
                    break;
                }
            }
            pending_separator = false;
        }
        visible.push_back(i);
        have_actionable = true;
    }
    return visible;
}

// Where a popup surface should go. `clipped` reports that the wanted height
// did not fit and the caller must scroll internally.
struct Placement
{
    wxRect rect;
    bool   flipped_up { false }; // opened above the anchor instead of below
    bool   clipped { false };    // height was reduced to fit the display
};

namespace detail {

inline wxRect clamp_into(wxRect r, const wxRect &display)
{
    if (r.width > display.width)
        r.width = display.width;
    if (r.height > display.height)
        r.height = display.height;
    if (r.GetRight() > display.GetRight())
        r.x = display.GetRight() - r.width + 1;
    if (r.x < display.x)
        r.x = display.x;
    if (r.GetBottom() > display.GetBottom())
        r.y = display.GetBottom() - r.height + 1;
    if (r.y < display.y)
        r.y = display.y;
    return r;
}

} // namespace detail

// Root menu placement. Preference order: below the anchor (left-aligned),
// above it, to its right, to its left. The surface never overlaps the anchor
// unless no side has room for even a shrunken surface; then it is clamped
// into the display with the height reduced (clipped = true).
inline Placement place_root(const wxRect &anchor, const wxSize &wanted, const wxRect &display)
{
    Placement p;
    const int w = std::min(wanted.x, display.width);
    const int h = std::min(wanted.y, display.height);

    // 1. Below, left-aligned; horizontal position may slide, vertical must fit.
    {
        wxRect r(anchor.x, anchor.GetBottom() + 1, w, h);
        if (r.GetBottom() <= display.GetBottom()) {
            r = detail::clamp_into(r, display);
            if (!r.Intersects(anchor)) {
                p.rect = r;
                return p;
            }
        }
    }
    // 2. Above.
    {
        wxRect r(anchor.x, anchor.y - h, w, h);
        if (r.y >= display.y) {
            r = detail::clamp_into(r, display);
            if (!r.Intersects(anchor)) {
                p.rect       = r;
                p.flipped_up = true;
                return p;
            }
        }
    }
    // 3. Right of the anchor, top-aligned.
    {
        wxRect r(anchor.GetRight() + 1, anchor.y, w, h);
        if (r.GetRight() <= display.GetRight()) {
            r = detail::clamp_into(r, display);
            if (!r.Intersects(anchor)) {
                p.rect = r;
                return p;
            }
        }
    }
    // 4. Left of the anchor, top-aligned.
    {
        wxRect r(anchor.x - w, anchor.y, w, h);
        if (r.x >= display.x) {
            r = detail::clamp_into(r, display);
            if (!r.Intersects(anchor)) {
                p.rect = r;
                return p;
            }
        }
    }
    // 5. Nothing fits at full height: shrink into the larger of the spaces
    //    below / above the anchor, still not overlapping it.
    {
        const int below = display.GetBottom() - anchor.GetBottom();
        const int above = anchor.y - display.y;
        if (below > 0 || above > 0) {
            p.clipped = true;
            if (below >= above) {
                wxRect r(anchor.x, anchor.GetBottom() + 1, w, below);
                p.rect = detail::clamp_into(r, display);
            } else {
                wxRect r(anchor.x, display.y, w, above);
                p.rect       = detail::clamp_into(r, display);
                p.flipped_up = true;
            }
            return p;
        }
    }
    // 6. The anchor covers the whole display height; overlap is unavoidable.
    p.clipped = true;
    p.rect    = detail::clamp_into(wxRect(anchor.x, display.y, w, h), display);
    return p;
}

// Submenu placement: to the right of its parent row, top edges aligned; flips
// to the left of the row at the right display edge; clamps vertically and
// shrinks the height when it still does not fit.
inline Placement place_submenu(const wxRect &row, const wxSize &wanted, const wxRect &display)
{
    Placement p;
    const int w = std::min(wanted.x, display.width);
    int       h = std::min(wanted.y, display.height);
    if (h < wanted.y)
        p.clipped = true;

    wxRect r(row.GetRight() + 1, row.y, w, h);
    if (r.GetRight() > display.GetRight()) {
        r.x = row.x - w;
        if (r.x < display.x)
            r.x = display.x; // neither side fits fully: hug the left edge
    }
    if (r.GetBottom() > display.GetBottom()) {
        r.y          = display.GetBottom() - h + 1;
        p.flipped_up = true;
    }
    if (r.y < display.y)
        r.y = display.y;
    p.rect = detail::clamp_into(r, display);
    return p;
}

}} // namespace MD3::Menu

#endif // slic3r_GUI_MD3MenuModel_hpp_
