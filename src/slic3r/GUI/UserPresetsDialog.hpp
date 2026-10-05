#pragma once

#include "GUI_Utils.hpp"
#include "Widgets/MD3Dialog.hpp"
#include "Bulk/BulkSelection.hpp"

#include <vector>
#include <map>
#include <unordered_set>

class TabCtrl;
class SwitchButton;
class SearchField;
class CheckBox;
class Label;
class Button;

namespace Slic3r {

class Preset;

namespace GUI {

// Batch management of user presets (printer / filament / process).
//
// The checked state lives in a BulkSelection keyed by preset name, so a
// selection survives a search filter change: presets that are selected but
// hidden by the filter stay selected and the counts line says how many are
// hidden. "Select visible" adds the rows matching the current search,
// "Select all N" adds every preset in the collection regardless of the
// search, "Invert selection" flips the visible rows only.
//
// Bulk actions (Delete, Export selected, Rename selected) always run through
// the shared reviewable preview / progress helpers in Bulk/.
class UserPresetsDialog : public MD3Dialog
{
public:
    UserPresetsDialog(wxWindow * parent);

private:
    void init_preset_list();

    void create_preset_list(wxWindow *parent);

    wxSizer *create_preset_line(wxWindow *parent, std::string const & preset);

    wxSizer *create_filament_group(wxWindow *parent, std::pair<std::string const, std::vector<std::string>> const &filament);

    void layout_preset_list(bool delete_old = false);

    void on_collection_changed(int collection);

    void on_search(wxString const & keyword);

    void on_preset_checked(std::string const &preset, bool checked, bool from_user);

    void on_filament_checked(std::string const &preset, bool checked, bool from_user);

    void on_all_checked(bool checked, bool from_user);

    void update_preset_counts();

    void update_checked();

    void delete_checked();

    bool delete_presets(int collection, std::vector<std::string> &presets);

    bool delete_confirm(int collection, int preset_num);

    bool delete_confirm(int collection, int filament_preset_num, int print_preset_num);

    void on_dpi_changed(const wxRect &suggested_rect) override;

    bool is_filament_list() const;

    // Every selectable preset name in the current view (all matches, search ignored).
    std::vector<std::string> all_ids() const;
    // Preset names whose row is currently shown (the "page" behind the search).
    std::vector<std::string> visible_ids() const;
    // Selected names in display order, sorted (the delete path expects a sorted vector).
    std::vector<std::string> selected_sorted() const;

    void select_visible();
    void select_all_matches();
    void invert_visible();
    // Push the selection model back into every row / group checkbox.
    void sync_checkboxes();
    void update_filament_group_checkbox(std::string const &filament);
    void on_char_hook(wxKeyEvent &event);

    void export_selected();
    void rename_selected();
    // Re-read the preset bundle and rebuild the current list (after a rename).
    void rebuild_from_bundle();

private:
    TabCtrl * m_tab_ctrl;
    SwitchButton * m_switch_button;
    SearchField * m_search;
    wxPanel * m_empty_panel;
    wxScrolledWindow * m_scrolled;
    std::map<std::string, wxSizer *> m_preset_sizers;
    std::map<std::string, wxSizer *> m_filament_sizers;
    std::unordered_set<wxSizer*> m_hiden_sizers;
    CheckBox * m_check_all;
    Label * m_label_check_count;
    Button * m_button_select_visible { nullptr };
    Button * m_button_select_all { nullptr };
    Button * m_button_invert { nullptr };
    Button * m_button_export { nullptr };
    Button * m_button_rename { nullptr };
    Button * m_button_delete;

private:
    std::vector<std::vector<std::string>> m_presets;
    std::map<std::string, std::string> m_filament_names;
    std::map<std::string, std::vector<std::string>> m_filament_presets;

    int m_collection = 0;
    Bulk::BulkSelection<std::string> m_selection;
    std::map<std::string, size_t> m_checked_filaments;
    // Set while a bulk delete runs after the reviewable preview + two-key
    // gate, so the legacy confirmation dialogs do not prompt a second time.
    bool m_bulk_confirmed = false;
};

}}
