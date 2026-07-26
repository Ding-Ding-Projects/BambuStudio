#ifndef slic3r_BulkFilamentDialog_hpp_
#define slic3r_BulkFilamentDialog_hpp_

#include <string>
#include <vector>

#include "GUI_Utils.hpp"

class Button;
class CheckBox;
class Label;
class SpinInput;
class wxScrolledWindow;

namespace Slic3r {
namespace GUI {

// Staged outcome of the Bulk filament actions dialog. Nothing is applied while
// the dialog runs; Sidebar::bulk_filament_actions() applies the staged actions
// in one batch after wxID_OK. Cancel is a strict no-op.
struct BulkFilamentResult {
    std::vector<size_t> selected_physical;  // 0-based indices into the physical slot list
    bool        do_preset = false;
    bool        do_color  = false;
    bool        do_delete = false;
    std::string preset_name;                // full preset name (never the alias)
    std::string color_hex;                  // "#RRGGBB"
    int         add_count = 0;              // filaments to append (0 = none)
};

// MD3 dialog listing every physical filament slot (checkbox + colour swatch +
// index + preset name) with a Select-all toggle and four bulk actions applying
// to the checked slots: stage a preset, stage a colour, stage deletion, and an
// independent "add N filaments" spinner bounded by the remaining capacity
// (EnforcerBlockerType::ExtruderMax = 32 total; passed in as
// max_filament_count so this header stays libslic3r-free).
class BulkFilamentDialog : public DPIDialog
{
public:
    BulkFilamentDialog(wxWindow* parent,
                       const std::vector<std::string>& physical_colors,
                       const std::vector<std::string>& physical_names,
                       size_t current_filament_count,
                       size_t max_filament_count);

    BulkFilamentResult get_result() const;

protected:
    void on_dpi_changed(const wxRect& suggested_rect) override;

private:
    void collect_preset_choices();
    void on_pick_preset();
    void on_pick_color();
    void set_all_rows(bool checked);
    void sync_select_all_state();
    void update_staged_labels();
    void update_apply_enabled();
    size_t checked_row_count() const;

    std::vector<std::string> m_physical_colors;
    std::vector<std::string> m_physical_names;
    size_t m_max_addable = 0;

    // Preset choices shown by the Set-preset picker: display strings (alias
    // when present) and full names, index-aligned.
    std::vector<std::string> m_preset_display;
    std::vector<std::string> m_preset_full_names;

    // Staged (not yet applied) action payloads; empty = not staged.
    std::string m_staged_preset;   // full preset name
    std::string m_staged_color;    // "#RRGGBB"

    // Controls
    std::vector<CheckBox*> m_row_checks;
    CheckBox*  m_chk_select_all{nullptr};
    Button*    m_btn_set_preset{nullptr};
    Label*     m_lbl_staged_preset{nullptr};
    Button*    m_btn_set_color{nullptr};
    Label*     m_lbl_staged_color{nullptr};
    CheckBox*  m_chk_delete{nullptr};
    CheckBox*  m_chk_add{nullptr};
    SpinInput* m_spin_add{nullptr};
    Button*    m_btn_apply{nullptr};
    bool       m_syncing_checks{false};
};

} // namespace GUI
} // namespace Slic3r

#endif // slic3r_BulkFilamentDialog_hpp_
