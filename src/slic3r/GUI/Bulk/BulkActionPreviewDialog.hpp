#ifndef slic3r_GUI_Bulk_BulkActionPreviewDialog_hpp_
#define slic3r_GUI_Bulk_BulkActionPreviewDialog_hpp_

#include <functional>
#include <vector>

#include "BulkActionPlan.hpp"
#include "../Widgets/MD3Dialog.hpp"

class Button;
class Label;
class SearchField;
class wxDataViewListCtrl;

namespace Slic3r { namespace GUI { namespace Bulk {

// Reviewable preview shown before every bulk action.
//
// Header: the action name. Body: the counts line
// "N selected / M will change / K skipped (reason)", the consequence sentence,
// a SearchField (plain by default, regex via its builder) filtering the
// reviewable list, and the list itself: item, detail, outcome ("Will change"
// or the exact skip reason) and, for transforms, the "after" value. Footer:
// Cancel and Proceed. Proceed is disabled while nothing will change.
//
// For a destructive plan, Proceed does not fire the callback directly: it
// opens the two-key SuperConfirmGate anchored on the Proceed button with the
// changed labels as the affected list, and only the gate's authorization
// invokes `on_proceed`. Nothing is applied by this dialog itself; the caller
// applies the plan's `will_change` rows in its own callback.
class BulkActionPreviewDialog final : public MD3Dialog
{
public:
    // Blocking: returns true when the user proceeded (and, when destructive,
    // completed the gate). The caller then applies the plan.
    static bool Run(wxWindow *parent, const BulkActionPlan &plan);

    // Callback form for surfaces that prefer not to block.
    static void Show(wxWindow *parent, const BulkActionPlan &plan, std::function<void()> on_proceed,
                     std::function<void()> on_cancel = nullptr);

    // Long-running apply helper: runs `step(i)` for i in [0, count) behind a
    // cancellable progress dialog that names the current item. `step` returns
    // false to report that item as failed; the run continues. Returns the
    // number of steps completed (cancelled runs return how far they got).
    // `label(i)` supplies the progress message for step i.
    static std::size_t RunWithProgress(wxWindow *parent, const wxString &title, std::size_t count,
                                       const std::function<wxString(std::size_t)> &label,
                                       const std::function<bool(std::size_t)> &step, bool *cancelled = nullptr,
                                       std::size_t *failed = nullptr);

private:
    BulkActionPreviewDialog(wxWindow *parent, const BulkActionPlan &plan);

    void build();
    void populate();
    void update_counts();
    void on_proceed();

    BulkActionPlan       m_plan;
    bool                 m_proceeded { false };
    Label *              m_counts { nullptr };
    Label *              m_consequence { nullptr };
    SearchField *        m_search { nullptr };
    wxDataViewListCtrl * m_list { nullptr };
    Button *             m_proceed { nullptr };
    Button *             m_cancel { nullptr };
};

} } } // namespace Slic3r::GUI::Bulk

#endif // slic3r_GUI_Bulk_BulkActionPreviewDialog_hpp_
