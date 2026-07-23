#ifndef slic3r_Preferences_hpp_
#define slic3r_Preferences_hpp_

#include "GUI.hpp"
#include "GUI_Utils.hpp"

#include <wx/simplebook.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/timer.h>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include "Widgets/ComboBox.hpp"
#include "Widgets/CheckBox.hpp"
#include "Widgets/TextInput.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/RadioBox.hpp"
#include "Widgets/LinkLabel.hpp"

// Global-namespace shared widgets used by the Preferences dialog. Forward-declared
// (members are pointers) so the header stays light; the .cpp includes the full
// definitions (SwitchButton.hpp / SearchField.hpp).
class SwitchButton;
class MultiSwitchButton;
class SearchField;

namespace Slic3r { namespace GUI {

class Selector
{
public:
    int       m_index;
    wxWindow *m_tab_button;
    wxWindow *m_tab_text;
};
WX_DECLARE_HASH_MAP(int, Selector *, wxIntegerHash, wxIntegerEqual, SelectorHash);

class RadioBox;
class RadioSelector
{
public:
    wxString  m_param_name;
    int       m_groupid;
    RadioBox *m_radiobox;
    bool      m_selected = false;
};

WX_DECLARE_LIST(RadioSelector, RadioSelectorList);
class CheckBox;
class TextInput;
class PreferenceTabbar;

class PreferencesDialog : public DPIDialog
{
private:
    AppConfig *app_config;

protected:
    PreferenceTabbar *m_tabbar = nullptr;
    wxSimplebook *    m_book   = nullptr;
    SearchField *     m_search = nullptr;
    std::vector<MultiSwitchButton *> m_segmented_list; // Appearance segmented controls (rescale)

    // --- Live settings search (SearchField -> row filtering) ----------------
    // One entry per direct row (sizer or window) of every settings page,
    // indexed once after the pages are built. `haystack` is the original-case
    // concatenation of the row's wxStaticText labels (the shared SearchField
    // matcher folds case itself, and needs the true text for regex);
    // `baseline_shown` snapshots the row's construction-time visibility (e.g.
    // the model-mall rows) so a search reset never reveals rows another gate hid.
    struct SearchRow
    {
        int                          page = 0;         // m_book page (nav section)
        wxSizerItem                 *item = nullptr;   // row item in the page sizer
        std::vector<wxStaticText *>  labels;           // label windows inside the row
        wxString                     haystack;         // original-case label text
        bool                         is_title = false; // Head_16 section header row
        bool                         baseline_shown = true;
    };
    std::vector<SearchRow>                       m_search_rows;
    std::unordered_map<wxStaticText *, wxColour> m_search_saved_colours; // pre-highlight foregrounds
    wxStaticText                                *m_search_empty_hint = nullptr;
    bool                                         m_search_active     = false;
    wxString                                     m_search_last_query; // trimmed active query ("" when inactive)

    void build_search_index();
    void apply_search_filter(const wxString &query);
    void reset_search_filter();
    void clear_search_highlights();
    void scroll_search_row_into_view(const SearchRow &row);

    bool m_seq_top_layer_only_changed{false};
    bool m_recreate_GUI{false};
    bool m_use_12h_time_format_changed{false};
    std::string m_original_use_12h_time_format;

public:
    bool seq_top_layer_only_changed() const { return m_seq_top_layer_only_changed; }
    bool recreate_GUI() const { return m_recreate_GUI; }
    bool use_12h_time_format_changed() const { return m_use_12h_time_format_changed; }
    void on_dpi_changed(const wxRect &suggested_rect) override;

public:
    PreferencesDialog(wxWindow *      parent,
                      wxWindowID      id    = wxID_ANY,
                      const wxString &title = wxT(""),
                      const wxPoint & pos   = wxDefaultPosition,
                      const wxSize &  size  = wxDefaultSize,
                      long            style = wxSYSTEM_MENU | wxCAPTION | wxCLOSE_BOX);

    ~PreferencesDialog();

    wxString m_backup_interval_time;

    void      create();

    // debug mode — the boolean preference rows are now MD3 SwitchButtons.
    ::SwitchButton * m_developer_mode_ckeckbox   = {nullptr};
    ::SwitchButton * m_internal_developer_mode_ckeckbox = {nullptr};

    wxString m_developer_mode_def;
    wxString m_internal_developer_mode_def;
    wxString m_iot_environment_def;

    SelectorHash      m_hash_selector;
    RadioSelectorList m_radio_group;
    // ComboBoxSelectorList    m_comxbo_group;

    wxBoxSizer *create_item_title(wxString title, wxWindow *parent, wxString tooltip);
    wxBoxSizer *create_item_combobox(wxString title, wxWindow *parent, wxString tooltip, std::string param,const std::vector<wxString>& label_list, const std::vector<std::string>& value_list, std::function<void(int)> callback = nullptr, int title_width = 0, int combox_width = 0);
    wxBoxSizer *create_item_region_combobox(wxString title, wxWindow *parent, wxString tooltip, std::vector<wxString> vlist);
    wxBoxSizer *create_item_language_combobox(wxString title, wxWindow *parent, wxString tooltip, int padding_left, std::string param, std::vector<const wxLanguageInfo *> vlist);
    wxBoxSizer *create_item_language_mode_combobox(wxString title, wxWindow *parent, wxString tooltip, std::string param,
                                                   const std::vector<std::pair<std::string, wxString>> &choices);
    wxBoxSizer *create_item_loglevel_combobox(wxString title, wxWindow *parent, wxString tooltip, std::vector<wxString> vlist);
    wxBoxSizer *create_item_checkbox(wxString title, wxWindow *parent, wxString tooltip, int padding_left, std::string param);
    void        set_dark_mode();
    // Apply a theme switch + fan out the dark-mode side effects (Appearance Theme control).
    void        apply_dark_mode(bool dark);
    wxWindow* create_item_downloads(wxWindow* parent, int padding_left, std::string param);
    wxWindow* create_item_external_editor(wxWindow* parent, int padding_left, std::string param);
    wxBoxSizer *create_item_input(wxString title, wxString title2, wxWindow *parent, wxString tooltip, std::string param, std::function<void(wxString)> onchange = {});
    wxBoxSizer *create_item_range_input(
        wxString title, wxWindow *parent, wxString tooltip, std::string param, float range_min, float range_max, int keep_digital,std::function<void(wxString)> onchange = {});
    wxBoxSizer *create_item_range_two_input(wxString                      title,
                                            wxWindow *                    parent,
                                            wxString                      tooltip,
                                            std::string                   param,
                                            std::string                   param1,
                                            float                         range_min,
                                            float                         range_max,
                                            int                           keep_digital,
                                            std::function<void(wxString)> onchange = {},
                                            std::function<void(wxString)> onchange1 = {});
    wxBoxSizer *create_item_multiple_combobox(
        wxString title, wxWindow *parent, wxString tooltip, int padding_left, std::string parama, std::vector<wxString> vlista, std::vector<wxString> vlistb);
    wxBoxSizer *create_item_switch(wxString title, wxWindow *parent, wxString tooltip, std::string param);
    wxSizer    *create_item_radiobox(wxString title, wxWindow *parent, wxString tooltip, int padding_left, int groupid, std::string param);

    wxWindow* create_appearance_tab();
    wxWindow* create_general_tab();
    wxWindow* create_user_tab();
    wxWindow* create_3d_tab();
    wxWindow* create_other_tab();
    wxWindow* create_developer_tab();
    wxBoxSizer *create_bottom_buttons();
    void on_reset_all_warnings();
    void on_reset_preferences();

    void     on_select_radio(std::string param);
    wxString get_select_radio(int groupid);
    // BBS
    void create_select_domain_widget();

    void Split(const std::string &src, const std::string &separator, std::vector<wxString> &dest);
    int m_current_language_selected = {0};

    std::unordered_map<int, Button *> m_button_list;
    // The boolean preference rows are MD3 SwitchButtons (icon-mode) rescaled on DPI change.
    std::unordered_map<int, ::SwitchButton *> m_checkbox_list;
    std::unordered_map<int, RadioBox *>   m_radiobox_list;
    std::unordered_map<int, ::ComboBox *> m_combobox_list;
    int                                   m_screen_height;

protected:
    void OnSelectRadio(wxMouseEvent &event);
};

}} // namespace Slic3r::GUI

#endif /* slic3r_Preferences_hpp_ */
