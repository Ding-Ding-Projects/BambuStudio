#ifndef slic3r_GUI_Bulk_BulkRenameDialog_hpp_
#define slic3r_GUI_Bulk_BulkRenameDialog_hpp_

#include <set>
#include <string>
#include <vector>

#include "BulkRenamePattern.hpp"
#include "../Widgets/MD3Dialog.hpp"

class Button;
class CheckBox;
class Label;
class SearchField;
class SpinInput;
class TextInput;
class wxDataViewListCtrl;

namespace Slic3r { namespace GUI { namespace Bulk {

// "Bulk rename..." for any collection of named items.
//
// Body: a pattern field ({name}, {n}, {i}, {stem}, {ext}), a find field
// (a SearchField, so its ".*" toggle and anchored regex builder supply the
// regex mode, case sensitivity and pattern), a replace field, a start-index
// and zero-pad pair, and a live before -> after preview list that re-plans on
// every keystroke through BulkRenamePattern::plan_rename. The counts line
// states "N selected / M will rename / K skipped" and each skipped row names
// its reason (unchanged, collision, invalid). Apply is enabled only when at
// least one row will rename and no row collides; nothing is renamed by the
// dialog itself - the caller reads the accepted plan and applies the Changed
// rows through its own undoable path.
class BulkRenameDialog final : public MD3Dialog
{
public:
    // Blocking. `names` are the selected items' current names in display
    // order; `reserved` are names in the same collection that are not being
    // renamed (they count for collisions). Returns true and fills `plan`
    // when the user applied.
    static bool Run(wxWindow *parent, const wxString &title, const std::vector<std::string> &names,
                    const std::set<std::string> &reserved, RenamePlan &plan);

private:
    BulkRenameDialog(wxWindow *parent, const wxString &title, std::vector<std::string> names,
                     std::set<std::string> reserved);

    void build();
    void replan();
    RenameSpec current_spec() const;

    std::vector<std::string> m_names;
    std::set<std::string>    m_reserved;
    RenamePlan               m_plan;
    bool                     m_applied { false };

    TextInput *          m_pattern { nullptr };
    SearchField *        m_find { nullptr };
    TextInput *          m_replace { nullptr };
    SpinInput *          m_start { nullptr };
    SpinInput *          m_pad { nullptr };
    Label *              m_counts { nullptr };
    Label *              m_error { nullptr };
    wxDataViewListCtrl * m_preview { nullptr };
    Button *             m_apply { nullptr };
};

} } } // namespace Slic3r::GUI::Bulk

#endif // slic3r_GUI_Bulk_BulkRenameDialog_hpp_
