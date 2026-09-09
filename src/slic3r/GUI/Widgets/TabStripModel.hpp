#ifndef slic3r_GUI_TabStripModel_hpp_
#define slic3r_GUI_TabStripModel_hpp_

// Header-only, GUI_App-free model behind the shared browser-style tab strip
// (TabStrip.hpp). It owns tab order, pinning, grouping, hidden tabs, the dock
// edge, JSON persistence, overflow arithmetic, bulk-close previews, tab and
// group search, and the keyboard orientation rule. Keeping it separate from the
// widget lets tests/tab_strip exercise every rule with only wx core linked and
// no running application object.
//
// Ordering invariants:
//   * the vector order IS the visual order;
//   * pinned tabs form a contiguous leading region ([0, pinned_count()));
//   * a tab moves between the two regions only through set_pinned().
// Hidden tabs (a settings tab "closed" from its strip) stay in the vector so
// their position survives, but are not displayed; the overflow menu lists
// them for restoring. A collapsed group displays only its active member, or a
// member that was temporarily revealed by a search result (reveal() is
// transient and never persisted, so the collapsed preference survives).

#include <wx/colour.h>
#include <wx/defs.h>
#include <wx/string.h>

#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "nlohmann/json.hpp"

namespace MD3 { namespace Tabs {

// Edge of the surface the strip docks to. Left is the settings default (a
// screen is wider than it is tall and a label is wider than it is high, so a
// vertical strip shows more tabs legibly); Top is the project-tabs default.
enum class DockEdge { Left, Right, Top, Bottom };

inline bool is_vertical(DockEdge e) { return e == DockEdge::Left || e == DockEdge::Right; }

inline const char *dock_edge_key(DockEdge e)
{
    switch (e) {
    case DockEdge::Left: return "left";
    case DockEdge::Right: return "right";
    case DockEdge::Top: return "top";
    case DockEdge::Bottom: return "bottom";
    }
    return "left";
}

inline DockEdge dock_edge_from_key(const std::string &key, DockEdge fallback)
{
    if (key == "left") return DockEdge::Left;
    if (key == "right") return DockEdge::Right;
    if (key == "top") return DockEdge::Top;
    if (key == "bottom") return DockEdge::Bottom;
    return fallback;
}

struct Tab
{
    std::string id;       // stable per-surface identifier (persisted)
    wxString    title;    // visible label
    std::string payload;  // opaque per-surface data (project path, ...)
    bool        pinned = false;
    bool        hidden = false; // settings "closed" tab: kept, not displayed
    bool        dirty  = false;
    int         group_id = -1;  // Group::id or -1 = ungrouped
};

struct Group
{
    int      id = -1;
    wxString name;
    wxColour color;
    bool     collapsed = false;
};

class Model
{
public:
    // --- tabs ----------------------------------------------------------------
    // Appends a tab (pinned tabs land at the end of the pinned region) and
    // returns its index. A duplicate id replaces the title/payload in place.
    int add(Tab tab)
    {
        if (int existing = index_of(tab.id); existing >= 0) {
            m_tabs[existing].title   = tab.title;
            m_tabs[existing].payload = tab.payload;
            return existing;
        }
        const bool pinned = tab.pinned;
        int index;
        if (pinned) {
            index = pinned_count();
            m_tabs.insert(m_tabs.begin() + index, std::move(tab));
        } else {
            m_tabs.push_back(std::move(tab));
            index = int(m_tabs.size()) - 1;
        }
        if (m_active.empty())
            m_active = m_tabs[index].id;
        return index;
    }

    bool remove(const std::string &id)
    {
        const int i = index_of(id);
        if (i < 0)
            return false;
        m_tabs.erase(m_tabs.begin() + i);
        if (m_active == id) {
            m_active.clear();
            // Prefer the displayed neighbour at the same slot, else the previous.
            for (int k = i; k < int(m_tabs.size()); ++k)
                if (is_displayed(m_tabs[k])) { m_active = m_tabs[k].id; break; }
            if (m_active.empty())
                for (int k = i - 1; k >= 0; --k)
                    if (is_displayed(m_tabs[k])) { m_active = m_tabs[k].id; break; }
        }
        if (m_revealed == id)
            m_revealed.clear();
        return true;
    }

    int index_of(const std::string &id) const
    {
        for (int k = 0; k < int(m_tabs.size()); ++k)
            if (m_tabs[k].id == id)
                return k;
        return -1;
    }

    Tab *      find(const std::string &id) { const int i = index_of(id); return i < 0 ? nullptr : &m_tabs[i]; }
    const Tab *find(const std::string &id) const { const int i = index_of(id); return i < 0 ? nullptr : &m_tabs[i]; }

    const std::vector<Tab> &tabs() const { return m_tabs; }
    int                     size() const { return int(m_tabs.size()); }
    bool                    empty() const { return m_tabs.empty(); }
    Tab &                   at(int i) { return m_tabs[i]; }
    const Tab &             at(int i) const { return m_tabs[i]; }

    int pinned_count() const
    {
        int n = 0;
        while (n < int(m_tabs.size()) && m_tabs[n].pinned)
            ++n;
        return n;
    }

    // Move the tab at `from` so it ends at index `to`. The move is clamped to
    // the region (pinned / unpinned) the tab belongs to, so a drag can never
    // smuggle an unpinned tab into the pinned region.
    bool move(int from, int to)
    {
        const int n = int(m_tabs.size());
        if (from < 0 || from >= n)
            return false;
        const int pc = pinned_count();
        int lo = m_tabs[from].pinned ? 0 : pc;
        int hi = m_tabs[from].pinned ? pc - 1 : n - 1;
        to = std::max(lo, std::min(hi, to));
        if (from == to)
            return false;
        Tab tab = m_tabs[from];
        m_tabs.erase(m_tabs.begin() + from);
        m_tabs.insert(m_tabs.begin() + to, tab);
        return true;
    }

    // Pin: the tab joins the end of the pinned region. Unpin: it becomes the
    // first unpinned tab, right after the region it just left.
    bool set_pinned(const std::string &id, bool pinned)
    {
        const int i = index_of(id);
        if (i < 0 || m_tabs[i].pinned == pinned)
            return false;
        Tab tab    = m_tabs[i];
        tab.pinned = pinned;
        m_tabs.erase(m_tabs.begin() + i);
        const int pc = pinned_count();
        m_tabs.insert(m_tabs.begin() + pc, tab);
        return true;
    }

    bool set_hidden(const std::string &id, bool hidden)
    {
        Tab *t = find(id);
        if (!t || t->hidden == hidden)
            return false;
        t->hidden = hidden;
        if (hidden && m_active == id) {
            m_active.clear();
            for (const Tab &c : m_tabs)
                if (is_displayed(c)) { m_active = c.id; break; }
        }
        return true;
    }

    bool set_title(const std::string &id, const wxString &title)
    {
        Tab *t = find(id);
        if (!t)
            return false;
        t->title = title;
        return true;
    }

    bool set_dirty(const std::string &id, bool dirty)
    {
        Tab *t = find(id);
        if (!t)
            return false;
        t->dirty = dirty;
        return true;
    }

    // --- active / reveal -----------------------------------------------------
    const std::string &active() const { return m_active; }
    int                active_index() const { return index_of(m_active); }
    bool set_active(const std::string &id)
    {
        if (!find(id))
            return false;
        m_active = id;
        return true;
    }

    // Reveal a tab that is hidden inside a collapsed group (search result
    // activation). Transient: it is cleared by the next reveal and never saved.
    void               reveal(const std::string &id) { m_revealed = id; }
    void               clear_reveal() { m_revealed.clear(); }
    const std::string &revealed() const { return m_revealed; }

    // --- groups --------------------------------------------------------------
    int create_group(const wxString &name, const wxColour &color)
    {
        Group g;
        g.id    = m_next_group_id++;
        g.name  = name;
        g.color = color;
        m_groups.push_back(g);
        return g.id;
    }

    Group *      group(int id) { for (Group &g : m_groups) if (g.id == id) return &g; return nullptr; }
    const Group *group(int id) const { for (const Group &g : m_groups) if (g.id == id) return &g; return nullptr; }
    const std::vector<Group> &groups() const { return m_groups; }

    bool rename_group(int id, const wxString &name) { Group *g = group(id); if (!g) return false; g->name = name; return true; }
    bool set_group_color(int id, const wxColour &c) { Group *g = group(id); if (!g) return false; g->color = c; return true; }
    bool set_group_collapsed(int id, bool collapsed)
    {
        Group *g = group(id);
        if (!g)
            return false;
        g->collapsed = collapsed;
        return true;
    }

    // Removes the group; members become ungrouped (never closed).
    bool remove_group(int id)
    {
        const auto it = std::find_if(m_groups.begin(), m_groups.end(), [id](const Group &g) { return g.id == id; });
        if (it == m_groups.end())
            return false;
        m_groups.erase(it);
        for (Tab &t : m_tabs)
            if (t.group_id == id)
                t.group_id = -1;
        return true;
    }

    // Assign a tab to a group (-1 = ungroup). Grouped tabs are kept contiguous:
    // the tab moves to the end of its new group's run when that run exists and
    // lies in the same pinned region.
    bool assign_group(const std::string &id, int group_id)
    {
        const int i = index_of(id);
        if (i < 0)
            return false;
        if (group_id >= 0 && !group(group_id))
            return false;
        m_tabs[i].group_id = group_id;
        if (group_id < 0)
            return true;
        int last = -1;
        for (int k = 0; k < int(m_tabs.size()); ++k)
            if (k != i && m_tabs[k].group_id == group_id && m_tabs[k].pinned == m_tabs[i].pinned)
                last = k;
        if (last >= 0) {
            const int target = last > i ? last : last + 1;
            move(i, target);
        }
        return true;
    }

    std::vector<std::string> members(int group_id) const
    {
        std::vector<std::string> out;
        for (const Tab &t : m_tabs)
            if (t.group_id == group_id)
                out.push_back(t.id);
        return out;
    }

    // --- display -------------------------------------------------------------
    bool is_displayed(const Tab &t) const
    {
        if (t.hidden)
            return false;
        if (t.group_id >= 0) {
            const Group *g = group(t.group_id);
            if (g && g->collapsed)
                return t.id == m_active || t.id == m_revealed;
        }
        return true;
    }

    // Indices (into tabs()) of every displayed tab, in visual order.
    std::vector<int> displayed_indices() const
    {
        std::vector<int> out;
        for (int k = 0; k < int(m_tabs.size()); ++k)
            if (is_displayed(m_tabs[k]))
                out.push_back(k);
        return out;
    }

    std::vector<int> hidden_indices() const
    {
        std::vector<int> out;
        for (int k = 0; k < int(m_tabs.size()); ++k)
            if (m_tabs[k].hidden)
                out.push_back(k);
        return out;
    }

    // --- dock edge -----------------------------------------------------------
    DockEdge edge() const { return m_edge; }
    void     set_edge(DockEdge e) { m_edge = e; }

    // --- persistence ---------------------------------------------------------
    // Layout JSON: order, pins, hidden state, groups, collapsed state, dock
    // edge and active id. Titles and payloads are included so a surface that
    // restores tabs from the file (project tabs) can rebuild them; a surface
    // that builds its tabs in code (settings) uses adopt_layout() instead.
    std::string to_json() const
    {
        nlohmann::json j;
        j["version"] = 1;
        j["edge"]    = dock_edge_key(m_edge);
        j["active"]  = m_active;
        j["tabs"]    = nlohmann::json::array();
        for (const Tab &t : m_tabs) {
            nlohmann::json jt;
            jt["id"]      = t.id;
            jt["title"]   = std::string(t.title.ToUTF8());
            jt["payload"] = t.payload;
            jt["pinned"]  = t.pinned;
            jt["hidden"]  = t.hidden;
            jt["group"]   = t.group_id;
            j["tabs"].push_back(jt);
        }
        j["groups"] = nlohmann::json::array();
        for (const Group &g : m_groups) {
            nlohmann::json jg;
            jg["id"]        = g.id;
            jg["name"]      = std::string(g.name.ToUTF8());
            jg["color"]     = std::string(g.color.GetAsString(wxC2S_HTML_SYNTAX).ToUTF8());
            jg["collapsed"] = g.collapsed;
            j["groups"].push_back(jg);
        }
        return j.dump();
    }

    // Replaces the whole model. Returns false (leaving the model untouched) on
    // malformed input.
    bool from_json(const std::string &text)
    {
        nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
        if (j.is_discarded() || !j.is_object())
            return false;
        Model m;
        m.m_edge   = dock_edge_from_key(j.value("edge", std::string()), m_edge);
        m.m_active = j.value("active", std::string());
        if (j.contains("groups") && j["groups"].is_array()) {
            for (const auto &jg : j["groups"]) {
                if (!jg.is_object())
                    continue;
                Group g;
                g.id        = jg.value("id", -1);
                g.name      = wxString::FromUTF8(jg.value("name", std::string()));
                g.color     = wxColour(wxString::FromUTF8(jg.value("color", std::string())));
                g.collapsed = jg.value("collapsed", false);
                if (g.id < 0)
                    continue;
                m.m_groups.push_back(g);
                m.m_next_group_id = std::max(m.m_next_group_id, g.id + 1);
            }
        }
        if (j.contains("tabs") && j["tabs"].is_array()) {
            for (const auto &jt : j["tabs"]) {
                if (!jt.is_object())
                    continue;
                Tab t;
                t.id       = jt.value("id", std::string());
                t.title    = wxString::FromUTF8(jt.value("title", std::string()));
                t.payload  = jt.value("payload", std::string());
                t.pinned   = jt.value("pinned", false);
                t.hidden   = jt.value("hidden", false);
                t.group_id = jt.value("group", -1);
                if (t.id.empty() || m.index_of(t.id) >= 0)
                    continue;
                if (t.group_id >= 0 && !m.group(t.group_id))
                    t.group_id = -1;
                m.m_tabs.push_back(t);
            }
        }
        // Re-establish the pinned-leading invariant defensively.
        std::stable_partition(m.m_tabs.begin(), m.m_tabs.end(), [](const Tab &t) { return t.pinned; });
        if (!m.m_active.empty() && !m.find(m.m_active))
            m.m_active.clear();
        *this = std::move(m);
        return true;
    }

    // Apply a saved layout to tabs that were created by code: order, pinned,
    // hidden and group membership follow the saved model for every id both
    // sides know; tabs the saved layout never saw keep their creation order
    // after the known ones; saved ids that no longer exist are dropped. Groups,
    // collapsed state and the dock edge are copied wholesale.
    void adopt_layout(const Model &saved)
    {
        m_groups        = saved.m_groups;
        m_next_group_id = saved.m_next_group_id;
        m_edge          = saved.m_edge;
        std::vector<Tab> ordered;
        for (const Tab &s : saved.m_tabs) {
            const int i = index_of(s.id);
            if (i < 0)
                continue;
            Tab t      = m_tabs[i];
            t.pinned   = s.pinned;
            t.hidden   = s.hidden;
            t.group_id = group(s.group_id) ? s.group_id : -1;
            ordered.push_back(t);
        }
        for (const Tab &t : m_tabs)
            if (saved.index_of(t.id) < 0)
                ordered.push_back(t);
        m_tabs = std::move(ordered);
        std::stable_partition(m_tabs.begin(), m_tabs.end(), [](const Tab &t) { return t.pinned; });
        if (!saved.m_active.empty() && find(saved.m_active) && is_displayed(*find(saved.m_active)))
            m_active = saved.m_active;
        else if (!find(m_active) || !is_displayed(*find(m_active))) {
            m_active.clear();
            for (const Tab &t : m_tabs)
                if (is_displayed(t)) { m_active = t.id; break; }
        }
    }

private:
    std::vector<Tab>   m_tabs;
    std::vector<Group> m_groups;
    int                m_next_group_id = 1;
    std::string        m_active;
    std::string        m_revealed;
    DockEdge           m_edge = DockEdge::Left;
};

// ---------------------------------------------------------------------------
// Overflow: which displayed tabs fit along the strip's main axis.
// ---------------------------------------------------------------------------
struct OverflowResult
{
    std::vector<int> visible; // positions into the displayed list
    std::vector<int> hidden;  // positions that go to the overflow menu
    bool             needs_button = false;
};

// extents[k] is the main-axis size (width for a horizontal strip, height for
// a vertical one) of displayed tab k; pinned[k] marks the protected region,
// which is always visible. `available` is the strip's main-axis size,
// `button_extent` the space the overflow button takes when needed, `gap` the
// spacing between consecutive tabs. Works identically for both orientations:
// the caller passes heights instead of widths for a vertical strip.
inline OverflowResult compute_overflow(const std::vector<int> &extents,
                                       const std::vector<bool> &pinned,
                                       int available, int button_extent, int gap)
{
    OverflowResult r;
    const int n = int(extents.size());
    auto fits = [&](int budget) {
        int used = 0;
        std::vector<int> vis;
        for (int k = 0; k < n; ++k) {
            const int need = extents[k] + (vis.empty() ? 0 : gap);
            const bool is_pinned = k < int(pinned.size()) && pinned[k];
            if (is_pinned || used + need <= budget) {
                used += need;
                vis.push_back(k);
            }
        }
        return vis;
    };
    std::vector<int> vis = fits(available);
    if (int(vis.size()) < n) {
        r.needs_button = true;
        vis            = fits(std::max(0, available - button_extent - gap));
    }
    r.visible = vis;
    for (int k = 0; k < n; ++k)
        if (!std::binary_search(vis.begin(), vis.end(), k))
            r.hidden.push_back(k);
    return r;
}

// ---------------------------------------------------------------------------
// Bulk close: "Close tabs containing text" / "Close tabs not containing text".
// ---------------------------------------------------------------------------
// The text matcher is injected so the model stays free of the regex engine:
// the widget passes a closure over SearchField::textMatches with the field's
// regex / case / whole-word / multiline flags. matcher(query, candidate).
using Matcher = std::function<bool(const wxString &query, const wxString &candidate)>;

struct ClosePredicate
{
    wxString text;
    bool     regex          = false;
    bool     invert         = false; // false: containing; true: NOT containing
    bool     include_pinned = false;
};

struct ClosePreview
{
    bool                     valid = false;
    wxString                 reason;       // why invalid (empty query / bad pattern)
    std::vector<std::string> ids;          // tabs that would close
    std::vector<wxString>    titles;       // parallel to ids
    int                      protected_pinned = 0; // pinned matches excluded
    wxString                 mode;         // "Contains text · plain" etc.
};

// Matches against the visible label only. Never runs on an empty query.
// `pattern_ok` reports whether the regex compiled (ignored when !regex).
inline ClosePreview preview_bulk_close(const Model &model, const ClosePredicate &p,
                                       const Matcher &matcher, bool pattern_ok = true)
{
    ClosePreview out;
    wxString     q = p.text;
    q.Trim(true).Trim(false);
    if (q.IsEmpty()) {
        out.reason = wxString::FromUTF8("Enter text to match before closing tabs.");
        return out;
    }
    if (p.regex && !pattern_ok) {
        out.reason = wxString::FromUTF8("The pattern is not a valid regular expression.");
        return out;
    }
    out.valid = true;
    out.mode  = wxString(p.invert ? "Not containing text" : "Containing text") +
               wxString::FromUTF8(" \xC2\xB7 ") + wxString(p.regex ? "regex" : "plain text");
    for (const Tab &t : model.tabs()) {
        const bool hit   = matcher(q, t.title);
        const bool close = p.invert ? !hit : hit;
        if (!close)
            continue;
        if (t.pinned && !p.include_pinned) {
            ++out.protected_pinned;
            continue;
        }
        out.ids.push_back(t.id);
        out.titles.push_back(t.title);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Search: current strip, inside a group, groups by name, master over strips.
// ---------------------------------------------------------------------------
struct SearchHit
{
    wxString    surface;   // "Preferences", "Projects", ...
    wxString    strip;     // strip name inside the surface
    wxString    group;     // group name or empty
    int         group_id = -1;
    bool        group_collapsed = false;
    bool        pinned = false;
    bool        hidden = false;
    bool        is_group = false; // a group-name hit rather than a tab hit
    wxString    title;
    std::string tab_id;

    // One-line description for a results list.
    wxString describe() const
    {
        wxString s = surface;
        if (!strip.IsEmpty() && strip != surface)
            s << wxString::FromUTF8(" \xE2\x80\xBA ") << strip;
        if (!group.IsEmpty())
            s << wxString::FromUTF8(" \xE2\x80\xBA ") << group << (group_collapsed ? " (collapsed)" : "");
        s << wxString::FromUTF8(" \xE2\x80\xBA ") << title;
        if (is_group)
            s << " [group]";
        if (pinned)
            s << " [pinned]";
        if (hidden)
            s << " [hidden]";
        return s;
    }
};

// Tabs whose title matches. group_filter >= 0 restricts to that group.
inline std::vector<SearchHit> search_tabs(const Model &model, const wxString &surface,
                                          const wxString &strip, const wxString &query,
                                          const Matcher &matcher, int group_filter = -1)
{
    std::vector<SearchHit> hits;
    for (const Tab &t : model.tabs()) {
        if (group_filter >= 0 && t.group_id != group_filter)
            continue;
        if (!matcher(query, t.title))
            continue;
        SearchHit h;
        h.surface = surface;
        h.strip   = strip;
        if (const Group *g = model.group(t.group_id)) {
            h.group           = g->name;
            h.group_id        = g->id;
            h.group_collapsed = g->collapsed;
        }
        h.pinned = t.pinned;
        h.hidden = t.hidden;
        h.title  = t.title;
        h.tab_id = t.id;
        hits.push_back(h);
    }
    return hits;
}

// Groups whose visible name matches.
inline std::vector<SearchHit> search_groups(const Model &model, const wxString &surface,
                                            const wxString &strip, const wxString &query,
                                            const Matcher &matcher)
{
    std::vector<SearchHit> hits;
    for (const Group &g : model.groups()) {
        if (!matcher(query, g.name))
            continue;
        SearchHit h;
        h.surface         = surface;
        h.strip           = strip;
        h.group           = g.name;
        h.group_id        = g.id;
        h.group_collapsed = g.collapsed;
        h.is_group        = true;
        h.title           = g.name;
        hits.push_back(h);
    }
    return hits;
}

// ---------------------------------------------------------------------------
// Keyboard orientation: a vertical strip (aria-orientation="vertical") moves
// selection with Up/Down, a horizontal one with Left/Right. Returns +1 / -1
// for a step, 0 when the key does not move along the strip's axis.
// ---------------------------------------------------------------------------
inline int arrow_step(DockEdge edge, int keycode)
{
    if (is_vertical(edge)) {
        if (keycode == WXK_DOWN) return 1;
        if (keycode == WXK_UP) return -1;
        return 0;
    }
    if (keycode == WXK_RIGHT) return 1;
    if (keycode == WXK_LEFT) return -1;
    return 0;
}

// Next displayed tab index (into tabs()) from `from_index`, stepping through
// displayed tabs only, wrapping at the ends. Returns -1 when nothing displays.
inline int step_displayed(const Model &model, int from_index, int step)
{
    const std::vector<int> disp = model.displayed_indices();
    if (disp.empty())
        return -1;
    int pos = 0;
    for (int k = 0; k < int(disp.size()); ++k)
        if (disp[k] == from_index) { pos = k; break; }
    const int n = int(disp.size());
    pos         = ((pos + step) % n + n) % n;
    return disp[pos];
}

}} // namespace MD3::Tabs

#endif // slic3r_GUI_TabStripModel_hpp_
