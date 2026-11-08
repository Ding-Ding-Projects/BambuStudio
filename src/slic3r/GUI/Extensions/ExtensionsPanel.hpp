#ifndef slic3r_GUI_Extensions_ExtensionsPanel_hpp_
#define slic3r_GUI_Extensions_ExtensionsPanel_hpp_

// File > Extensions: every extension folder with its state, the consent each
// one was given, the limits every run gets, and what each run did. Also the
// two prompts the person answers: allowing exactly what an extension asks for,
// and replacing an installed version with another from a folder. The decisions
// live in the wx-free ExtensionCore and ExtensionHost; runs go through the
// ExtensionRuntime worker.

#include "ExtensionHost.hpp"

#include <wx/panel.h>

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

class wxBoxSizer;
class wxSimplebook;
class wxSizer;
class wxWindow;
class Button;
class Label;
class MD3DataViewListCtrl;
class SearchField;
class SpinInput;
class StaticBox;
class SwitchButton;
class TextArea;

namespace Slic3r { namespace GUI {

class TextTabbar;

namespace Extensions {

// The consent prompt. Allow and KeepOff are answers to record; Undecided
// (closing the prompt) records nothing, so it asks again next time.
enum class ConsentAnswer { Allow, KeepOff, Undecided };
ConsentAnswer ask_consent(wxWindow *parent, const Installed &extension, const Consent *previous);
// Replacing an installed version with the one in a folder the person picked.
bool ask_replace(wxWindow *parent, const Manifest &installed, const Manifest &incoming, Host::Inspection kind);
// The state in a few words, for the list and the setup wizard.
wxString state_text(State state);

class ExtensionsPanel : public wxPanel
{
public:
    explicit ExtensionsPanel(wxWindow *parent);
    ~ExtensionsPanel() override;

    void select(const std::string &id);

    enum Filter { FilterAll = 0, FilterOn, FilterOff, FilterWaiting, FilterErrors, FilterBuiltin, FilterCount };
    enum Tab { TabOverview = 0, TabPermissions, TabSettings, TabRuns, TabManifest };

private:
    // ---- building
    wxWindow *build_list_side(wxWindow *parent);
    wxWindow *build_detail_side(wxWindow *parent);
    Button   *make_button(wxWindow *parent, const wxString &label, std::uint32_t glyph, bool tonal = false);
    Label    *make_label(wxWindow *parent, const wxFont &font, const wxString &text, bool muted = false, bool wrap = false);
    StaticBox *make_card(wxWindow *parent, int background_role);

    // ---- refreshing
    void refresh_all();
    void refresh_list();
    void refresh_filters();
    void refresh_detail();
    void refresh_activity();
    void refresh_week();
    void refresh_limits();
    void refresh_tabs();
    void fill_overview(const Installed &x);
    void fill_permissions(const Installed &x);
    // The extension's own settings: one control per declared setting, saved as
    // it changes. Rebuilt only when another extension or version is shown, so
    // a run finishing never takes a field away while you type in it.
    void fill_settings(const Installed &x);
    bool save_setting(const std::string &id, const std::string &setting, const nlohmann::json &value);
    void fill_runs(const Installed &x);
    void fill_manifest(const Installed &x);
    void select_tab(int tab);

    // ---- what the list shows
    bool in_filter(const Installed &x, int filter) const;
    bool matches(const Installed &x) const;
    const Installed *selected() const;
    wxString         last_run_text(const std::string &id) const;

    // ---- actions
    void set_master(bool on);
    void toggle_selected(bool on);
    bool review_selected();
    void add_from_folder();
    void remove_selected();
    void open_selected_folder();
    void try_sample();
    void run_action(const std::string &action);
    void export_runs();
    void save_limits();
    void set_filter(int filter);

    int                                      m_token  = 0;
    int                                      m_filter = FilterAll;
    int                                      m_tab    = TabOverview;
    std::string                              m_selected;
    std::string                              m_settings_for; // id and version the Settings page shows
    std::vector<std::string>                 m_rows; // list row -> extension id
    bool                                     m_refreshing = false;

    SwitchButton                            *m_master       = nullptr;
    Label                                   *m_master_label = nullptr;
    Label                                   *m_activity     = nullptr;
    Button                                  *m_stop         = nullptr;
    SearchField                             *m_search       = nullptr;
    std::vector<std::pair<int, Button *>>    m_filter_buttons;
    MD3DataViewListCtrl                     *m_list        = nullptr;
    Label                                   *m_folder      = nullptr;
    SpinInput                               *m_timeout     = nullptr;
    SpinInput                               *m_max_answer  = nullptr;
    SpinInput                               *m_off_after   = nullptr;
    Label                                   *m_week        = nullptr;

    wxWindow                                *m_detail       = nullptr;
    wxWindow                                *m_detail_empty = nullptr;
    wxWindow                                *m_detail_body  = nullptr;
    Label                                   *m_name        = nullptr;
    Label                                   *m_state       = nullptr;
    Label                                   *m_description = nullptr;
    Label                                   *m_id          = nullptr;
    StaticBox                               *m_banner      = nullptr;
    Label                                   *m_banner_text = nullptr;
    Button                                  *m_review      = nullptr;
    SwitchButton                            *m_on          = nullptr;
    Button                                  *m_sample      = nullptr;
    Button                                  *m_open_folder = nullptr;
    Button                                  *m_remove      = nullptr;
    TextTabbar                              *m_tabs        = nullptr;
    wxSimplebook                            *m_book        = nullptr;
    wxWindow                                *m_overview    = nullptr;
    wxWindow                                *m_permissions = nullptr;
    wxWindow                                *m_settings    = nullptr;
    wxWindow                                *m_runs_page   = nullptr;
    MD3DataViewListCtrl                     *m_runs        = nullptr;
    Label                                   *m_runs_note   = nullptr;
    wxWindow                                *m_manifest_page = nullptr;
    Label                                   *m_manifest_state = nullptr;
    TextArea                                *m_manifest     = nullptr;
};

} // namespace Extensions
}} // namespace Slic3r::GUI

#endif // slic3r_GUI_Extensions_ExtensionsPanel_hpp_
