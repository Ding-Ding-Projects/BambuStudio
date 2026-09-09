#ifndef slic3r_GUI_Bulk_BulkSelection_hpp_
#define slic3r_GUI_Bulk_BulkSelection_hpp_

#include <algorithm>
#include <cstddef>
#include <set>
#include <vector>

namespace Slic3r { namespace GUI { namespace Bulk {

// Shared multi-select model for every list, table, grid and collection.
//
// The selection is expressed over stable item ids (never row indices) so it
// survives a repopulate, a re-sort and a filter change. Two universes matter:
//
//   * the "page"        - the slice of matches the surface has rendered;
//   * the "all matches" - every id the current filter yields, rendered or not.
//
// The two select-all variants are deliberately distinct so a surface can name
// which one it offers ("Select this page (12)" vs "Select all 340 matches"),
// and `invert()` is scoped to a universe so ids outside the current filter are
// never toggled behind the user's back. `retain()` drops ids that no longer
// exist so a stale id can never be acted on. `select_range()` is the
// shift-click contract: the inclusive run between the anchor and the clicked
// id in the surface's current display order.
//
// Header-only, no wxWidgets dependency, so it is unit-testable in isolation.
template <typename Id>
class BulkSelection
{
public:
    using id_type = Id;

    void toggle(const Id &id)
    {
        if (!m_ids.erase(id))
            m_ids.insert(id);
    }

    void set(const Id &id, bool on)
    {
        if (on)
            m_ids.insert(id);
        else
            m_ids.erase(id);
    }

    // Shift-click: select the inclusive range between `anchor` and `id` in
    // `order` (the surface's display order). An unknown anchor selects only
    // `id`; an unknown `id` selects nothing.
    void select_range(const std::vector<Id> &order, const Id &anchor, const Id &id)
    {
        const auto a = std::find(order.begin(), order.end(), anchor);
        const auto b = std::find(order.begin(), order.end(), id);
        if (a == order.end() || b == order.end()) {
            if (b != order.end())
                m_ids.insert(id);
            return;
        }
        const auto lo = std::min(a, b);
        const auto hi = std::max(a, b);
        for (auto it = lo; it <= hi; ++it)
            m_ids.insert(*it);
    }

    // "Select this page": adds every rendered id.
    void select_page(const std::vector<Id> &page_ids) { m_ids.insert(page_ids.begin(), page_ids.end()); }

    // "Select all N matches": adds every id the filter yields, rendered or not.
    void select_all_matches(const std::vector<Id> &match_ids) { m_ids.insert(match_ids.begin(), match_ids.end()); }

    // Invert within `universe` (normally the current matches): selected ids in
    // the universe become unselected and vice versa; ids outside are untouched.
    void invert(const std::vector<Id> &universe)
    {
        for (const Id &id : universe)
            toggle(id);
    }

    void clear() { m_ids.clear(); }

    // Drop ids no longer present in `existing` (the surface's live id set).
    void retain(const std::set<Id> &existing)
    {
        for (auto it = m_ids.begin(); it != m_ids.end();) {
            if (existing.count(*it) == 0)
                it = m_ids.erase(it);
            else
                ++it;
        }
    }
    // Same, from a display-order list. Named apart from retain() so a braced
    // list never makes the call ambiguous.
    void retain_listed(const std::vector<Id> &existing) { retain(std::set<Id>(existing.begin(), existing.end())); }

    bool        contains(const Id &id) const { return m_ids.count(id) != 0; }
    std::size_t size() const { return m_ids.size(); }
    bool        empty() const { return m_ids.empty(); }
    const std::set<Id> &ids() const { return m_ids; }

    // Count of selected ids that are inside `universe`.
    std::size_t count_within(const std::vector<Id> &universe) const
    {
        return static_cast<std::size_t>(
            std::count_if(universe.begin(), universe.end(), [this](const Id &id) { return m_ids.count(id) != 0; }));
    }

    // Selected ids in `universe` order (so an action runs in display order).
    std::vector<Id> ordered_within(const std::vector<Id> &universe) const
    {
        std::vector<Id> out;
        for (const Id &id : universe)
            if (m_ids.count(id) != 0)
                out.push_back(id);
        return out;
    }

    // True when every id in `universe` is selected (and the universe is not empty).
    bool covers(const std::vector<Id> &universe) const
    {
        return !universe.empty() && count_within(universe) == universe.size();
    }

private:
    std::set<Id> m_ids;
};

// Which select-all a surface just performed; used to word the count label so
// the user always knows whether "everything" meant the visible page or every
// match behind the filter.
enum class SelectAllScope { Page, AllMatches };

} } } // namespace Slic3r::GUI::Bulk

#endif // slic3r_GUI_Bulk_BulkSelection_hpp_
