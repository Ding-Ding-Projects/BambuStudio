#ifndef slic3r_GUI_Bulk_BulkActionPlan_hpp_
#define slic3r_GUI_Bulk_BulkActionPlan_hpp_

#include <cstddef>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace Bulk {

// One row of a bulk action preview: what the item is called, whether the
// action will touch it, and why not when it will not.
struct BulkItem
{
    std::string label;        // primary text (object name, preset name, ...)
    std::string detail;       // secondary text (path, type, timestamp, ...)
    std::string after;        // what the item becomes ("" when not a transform)
    bool        will_change = true;
    std::string skip_reason;  // "" when will_change

    static BulkItem changed(std::string label, std::string detail = {}, std::string after = {})
    {
        BulkItem it;
        it.label  = std::move(label);
        it.detail = std::move(detail);
        it.after  = std::move(after);
        return it;
    }
    static BulkItem skipped(std::string label, std::string reason, std::string detail = {})
    {
        BulkItem it;
        it.label       = std::move(label);
        it.detail      = std::move(detail);
        it.will_change = false;
        it.skip_reason = std::move(reason);
        return it;
    }
};

// A reviewable plan for one bulk action: the surface builds it from the
// selection, the BulkActionPreviewDialog shows it, and only after the user
// proceeds (and, for a destructive plan, passes the two-key gate) does the
// surface apply the `will_change` rows. The counts are derived, never typed
// by hand, so "N selected" and "M will change" cannot drift apart.
struct BulkActionPlan
{
    std::string action;       // imperative label, e.g. "Delete objects"
    std::string consequence;  // one plain sentence of what happens to a changed row
    bool        destructive = false; // routes Proceed through SuperConfirmGate
    std::vector<BulkItem> items;

    std::size_t selected() const { return items.size(); }
    std::size_t will_change() const
    {
        std::size_t n = 0;
        for (const BulkItem &it : items)
            if (it.will_change)
                ++n;
        return n;
    }
    std::size_t skipped() const { return items.size() - will_change(); }
    bool        empty() const { return items.empty(); }
    bool        applicable() const { return will_change() > 0; }
    bool        has_transform() const
    {
        for (const BulkItem &it : items)
            if (!it.after.empty())
                return true;
        return false;
    }
    // Labels of every row that will change, in plan order.
    std::vector<std::string> changed_labels() const
    {
        std::vector<std::string> out;
        for (const BulkItem &it : items)
            if (it.will_change)
                out.push_back(it.label);
        return out;
    }
};

} } } // namespace Slic3r::GUI::Bulk

#endif // slic3r_GUI_Bulk_BulkActionPlan_hpp_
