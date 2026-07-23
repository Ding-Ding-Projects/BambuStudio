#include "Preferences.hpp"
#include "OptionsGroup.hpp"
#include "GUI_App.hpp"
#include "MainFrame.hpp"
#include "Plater.hpp"
#include "MsgDialog.hpp"
#include "I18N.hpp"
#include "UxProgramTermsDialog.hpp"
#include "Widgets/StateColor.hpp"
#include "libslic3r/AppConfig.hpp"
#include "../Utils/ExternalEditor.hpp"
#include <cassert>
#include <string>
#include <vector>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/simplebook.h>
#include <wx/filedlg.h>
#include <wx/colordlg.h>
#include "OG_CustomCtrl.hpp"
#include "fila_manager/wgtFilaManagerFeature.h"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "Widgets/SwitchButton.hpp"
#include "Widgets/SearchField.hpp"
#include "Widgets/StaticBox.hpp"
#include "Widgets/MaterialIcon.hpp"
#include "wx/graphics.h"
#include <wx/dcgraph.h>
#include <wx/fontenum.h>

#include <wx/listimpl.cpp>
#include <map>
#include <wx/sizer.h>
#include "Gizmos/GLGizmoBase.hpp"
#include "OpenGLManager.hpp"
#ifdef __WINDOWS__
#ifdef _MSW_DARK_MODE
#include "dark_mode.hpp"
#endif // _MSW_DARK_MODE
#endif //__WINDOWS__

namespace Slic3r { namespace GUI {

WX_DEFINE_LIST(RadioSelectorList);

// Raw (pre-DPI) control widths for Preferences rows. Height is -1 (auto) unless
// noted. Wrap in FromDIP(...) at each use site, e.g. wxSize(FromDIP(TITLE_WIDTH), -1).
static constexpr int TITLE_WIDTH          = 100; // row label column
static constexpr int COMBOBOX_WIDTH       = 140;
static constexpr int LARGE_COMBOBOX_WIDTH = 160;
static constexpr int LANGUAGE_COMBOBOX_WIDTH = 260;
static constexpr int INPUT_WIDTH          = 100;
static constexpr int BTN_WIDTH            = 58; // small action button (reset / browse)
static constexpr int BTN_HEIGHT           = 22;
static constexpr int TITLE_PADDING        = 48;
static constexpr int ITEM_LEFT_PADDING    = 48 + 16;
static constexpr int ITEM_RIGHT_PADDING   = 24;
// Minimum settings-row height. Must exceed the 24px MD3 switch pill so
// adjacent single-line toggle rows keep a visible gap (24 made the pills
// touch — see the Other-tab Online Models pair in the screenshot matrix).
static constexpr int ITEM_MIN_HEIGHT      = 32;

static wxString language_display_name(const wxLanguageInfo *info)
{
    if (info == nullptr)
        return {};

    const std::map<wxLanguage, wxString> names {
        {wxLANGUAGE_CHINESE_SIMPLIFIED, wxString::FromUTF8("中文(简体)")},
        {wxLANGUAGE_CHINESE_TRADITIONAL, wxString::FromUTF8("中文(繁體)")},
        {wxLANGUAGE_SPANISH, wxString::FromUTF8("Español")},
        {wxLANGUAGE_GERMAN, wxString::FromUTF8("Deutsch")},
        {wxLANGUAGE_SWEDISH, wxString::FromUTF8("Svenska")},
        {wxLANGUAGE_DUTCH, wxString::FromUTF8("Nederlands")},
        {wxLANGUAGE_FRENCH, wxString::FromUTF8("Français")},
        {wxLANGUAGE_HUNGARIAN, wxString::FromUTF8("Magyar")},
        {wxLANGUAGE_JAPANESE, wxString::FromUTF8("日本語")},
        {wxLANGUAGE_ITALIAN, wxString::FromUTF8("italiano")},
        {wxLANGUAGE_KOREAN, wxString::FromUTF8("한국어")},
        {wxLANGUAGE_RUSSIAN, wxString::FromUTF8("Русский")},
        {wxLANGUAGE_CZECH, wxString::FromUTF8("čeština")},
        {wxLANGUAGE_UKRAINIAN, wxString::FromUTF8("Українська")},
        {wxLANGUAGE_PORTUGUESE_BRAZILIAN, wxString::FromUTF8("Português (Brasil)")},
        {wxLANGUAGE_TURKISH, wxString::FromUTF8("Türkçe")},
        {wxLANGUAGE_POLISH, wxString::FromUTF8("Polski")},
    };
    const auto found = names.find(static_cast<wxLanguage>(info->Language));
    return found == names.end() ? info->Description : found->second;
}

// Scrolled panel used for every Preferences tab. wxScrolledWindow's default
// behavior is to scroll to whatever child receives focus, which makes the
// dialog jump around when the user tabs between combobox/checkbox rows.
// Suppressing that and presetting a sensible scroll rate keeps tab pages stable.
class ScrollPanel : public wxScrolledWindow
{
public:
    explicit ScrollPanel(wxWindow *parent) : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL)
    {
        SetScrollRate(5, 5);
        // Content pane surface — driven by role so dark resolves via semantic()
        // instead of the legacy White->dark swap map.
        SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    }

    bool ShouldScrollToChildOnFocus(wxWindow* child) override { return false; }
};

// Confirm dialog for "Reset all warning dialogs". A "Check details" button
// expands a panel listing the warning settings that get cleared
class ResetWarningsDialog : public DPIDialog
{
public:
    explicit ResetWarningsDialog(wxWindow *parent);
    ~ResetWarningsDialog() override = default;
    void on_dpi_changed(const wxRect &suggested_rect) override {}

private:
    void toggle_details();

    Button   *m_details_btn   = nullptr;
    wxWindow *m_details_panel = nullptr;
    bool      m_expanded      = false;
};

wxBoxSizer *PreferencesDialog::create_item_title(wxString title, wxWindow *parent, wxString tooltip)
{
    wxBoxSizer *m_sizer_title = new wxBoxSizer(wxHORIZONTAL);

    // MD3 content section title: 16px / 700 in OnSurface (kit Settings section
    // header), replacing the legacy Head_13 in TextSecondary.
    auto m_title = new wxStaticText(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, 0);
    m_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    m_title->SetFont(::Label::Head_16);

    // The Preferences dialog has no native default push button (every visible button
    // is a custom-drawn ::Button, i.e. a plain wxWindow, not a Win32 BUTTON control).
    // Without a designated default button the Win32 dialog manager's xxxRemoveDefaultButton
    // walk (run on every WM_ACTIVATE / focus save) has no fixed target and enumerates the
    // child windows probing them with SendMessage(WM_GETDLGCODE). With an endpoint-DLP / IME
    // DLL injected into the process, that probe can be redirected cross-thread to a
    // non-pumping injected window and deadlock the UI thread. Give the dialog one real,
    // hidden, zero-size native button as a stable in-thread default so the walk finds its
    // target immediately and never leaves our own windows.
    // The hang is first found on windows, but keep it on other platforms is no harm
    auto line = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));

    m_sizer_title->AddSpacer(FromDIP(TITLE_PADDING));
    m_sizer_title->Add(m_title, wxSizerFlags().CenterVertical());

    return m_sizer_title;
}

wxBoxSizer *PreferencesDialog::create_item_combobox(wxString title, wxWindow *parent, wxString tooltip, std::string param, const std::vector<wxString>& label_list, const std::vector<std::string>& value_list, std::function<void(int)> callback, int title_width, int combox_width)
{
    assert(label_list.size() == value_list.size());

    auto find_nearst_by_value = [value_list](const std::string value) -> int {
        try {
            std::vector<int> values;
            for (const auto &v : value_list) values.push_back(stoi(v));
            int target = stoi(value);

            auto it = std::min_element(values.begin(), values.end(), [target](int a, int b) { return std::abs(a - target) < std::abs(b - target); });
            return std::distance(values.begin(), it);

        } catch (...) {
            return 0;
        }
    };

    auto get_value_idx = [value_list, find_nearst_by_value](const std::string value) -> int {
        auto iter = std::find(value_list.begin(), value_list.end(), value);
        if (iter != value_list.end()) return std::distance(value_list.begin(), iter);
        return find_nearst_by_value(value);
    };

    wxBoxSizer *m_sizer_combox = new wxBoxSizer(wxHORIZONTAL);
    m_sizer_combox->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    m_sizer_combox->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));

    auto combo_title = new wxStaticText(parent, wxID_ANY, title, wxDefaultPosition, title_width == 0 ? wxSize(FromDIP(TITLE_WIDTH), -1) : wxSize(title_width, -1), 0);
    combo_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    combo_title->SetFont(::Label::Body_13);
    combo_title->SetToolTip(tooltip);
    combo_title->Wrap(-1);
    m_sizer_combox->Add(combo_title, wxSizerFlags().CenterVertical().Proportion(1));

    auto combobox = new ::ComboBox(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, combox_width == 0 ? wxSize(FromDIP(LARGE_COMBOBOX_WIDTH), -1) : wxSize(combox_width, -1),
                                   0, nullptr, wxCB_READONLY);
    m_combobox_list[m_combobox_list.size()] = combobox;
    combobox->SetFont(::Label::Body_13);
    combobox->GetDropDown().SetFont(::Label::Body_13);
    combobox->SetCornerRadius(FromDIP(10)); // MD3 SelectField r10

    std::vector<wxString>::iterator iter;
    for (auto label : label_list)
        combobox->Append(label);

    auto old_value = app_config->get(param);
    if (!old_value.empty()) {
        combobox->SetSelection(get_value_idx(old_value));
    }
    else {
        combobox->SetSelection(0);
    }

    m_sizer_combox->Add(combobox, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(ITEM_RIGHT_PADDING)));

    //// save config
    combobox->GetDropDown().Bind(wxEVT_COMBOBOX, [this, param, value_list, callback](wxCommandEvent &e) {
        app_config->set(param, value_list[e.GetSelection()]);
        app_config->save();
        if (callback) {
            callback(e.GetSelection());
        }
        e.Skip();
    });
    return m_sizer_combox;
}

wxBoxSizer *PreferencesDialog::create_item_language_combobox(
    wxString title, wxWindow *parent, wxString tooltip, int padding_left, std::string param, std::vector<const wxLanguageInfo *> vlist)
{
    wxBoxSizer *m_sizer_combox = new wxBoxSizer(wxHORIZONTAL);
    m_sizer_combox->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    m_sizer_combox->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));

    auto combo_title = new wxStaticText(parent, wxID_ANY, title, wxDefaultPosition, wxSize(FromDIP(TITLE_WIDTH), -1), 0);
    combo_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    combo_title->SetFont(::Label::Body_13);
    combo_title->SetToolTip(tooltip);
    combo_title->Wrap(-1);
    m_sizer_combox->Add(combo_title, wxSizerFlags().CenterVertical().Proportion(1));

    auto combobox = new ::ComboBox(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(LARGE_COMBOBOX_WIDTH), -1), 0, nullptr, wxCB_READONLY);
    m_combobox_list[m_combobox_list.size()] = combobox;
    combobox->SetFont(::Label::Body_13);
    combobox->GetDropDown().SetFont(::Label::Body_13);
    combobox->SetCornerRadius(FromDIP(10)); // MD3 SelectField r10
    auto language = app_config->get(param);
    m_current_language_selected = -1;
    std::vector<wxString>::iterator iter;
    for (size_t i = 0; i < vlist.size(); ++i) {
        auto language_name = vlist[i]->Description;

        if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_CHINESE_SIMPLIFIED)) {
            language_name = wxString::FromUTF8("\xe4\xb8\xad\xe6\x96\x87\x28\xe7\xae\x80\xe4\xbd\x93\x29");
        }
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_CHINESE_TRADITIONAL)) {
            language_name = wxString::FromUTF8("\xe4\xb8\xad\xe6\x96\x87\x28\xe7\xb9\x81\xe9\xab\x94\x29");
        }
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_SPANISH)) {
            language_name = wxString::FromUTF8("\x45\x73\x70\x61\xc3\xb1\x6f\x6c");
        }
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_GERMAN)) {
            language_name = wxString::FromUTF8("Deutsch");
        }
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_SWEDISH)) {
            language_name = wxString::FromUTF8("\x53\x76\x65\x6e\x73\x6b\x61"); //Svenska
        }
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_DUTCH)) {
            language_name = wxString::FromUTF8("Nederlands");
        }
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_FRENCH)) {
            language_name = wxString::FromUTF8("\x46\x72\x61\x6E\xC3\xA7\x61\x69\x73");
        }
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_HUNGARIAN)) {
            language_name = wxString::FromUTF8("Magyar");
        }
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_JAPANESE)) {
            language_name = wxString::FromUTF8("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E");
        }
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_ITALIAN)) {
            language_name = wxString::FromUTF8("\x69\x74\x61\x6c\x69\x61\x6e\x6f");
        }
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_KOREAN)) {
            language_name = wxString::FromUTF8("\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4");
        }
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_RUSSIAN)) {
            language_name = wxString::FromUTF8("\xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9");
        }
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_CZECH)) {
            if (wxGetApp().app_config->get("language") == "ja_JP") {
                language_name = wxString::FromUTF8("\x43\x7A\x65\x63\x68");
            }
            else{
                language_name = wxString::FromUTF8("\xC4\x8D\x65\xC5\xA1\x74\x69\x6E\x61");
            }
        }
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_UKRAINIAN)) {
            if (wxGetApp().app_config->get("language") == "ja_JP") {
                language_name = wxString::FromUTF8("\x55\x6B\x72\x61\x69\x6E\x69\x61\x6E");
            } else {
                language_name = wxString::FromUTF8("\xD0\xA3\xD0\xBA\xD1\x80\xD0\xB0\xD1\x97\xD0\xBD\xD1\x81\xD1\x8C\xD0\xBA\xD0\xB0");
            }
        }
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_PORTUGUESE_BRAZILIAN)) {
            language_name = wxString::FromUTF8("\x50\x6F\x72\x74\x75\x67\x75\xC3\xAA\x73\x20\x28\x42\x72\x61\x73\x69\x6C\x29");
        }
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_TURKISH)) {
            language_name = wxString::FromUTF8("\x54\xC3\xBC\x72\x6B\xC3\xA7\x65");
        }
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_POLISH)) {
            language_name = wxString::FromUTF8("Polski");
        }

        if (language == vlist[i]->CanonicalName) {
            m_current_language_selected = i;
        }
        combobox->Append(language_name);
    }
    if (m_current_language_selected == -1 && language.size() >= 5) {
        language = language.substr(0, 2);
        for (size_t i = 0; i < vlist.size(); ++i) {
            if (vlist[i]->CanonicalName.StartsWith(language)) {
                m_current_language_selected = i;
                break;
            }
        }
    }
    combobox->SetSelection(m_current_language_selected);

    m_sizer_combox->Add(combobox, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(ITEM_RIGHT_PADDING)));

    combobox->Bind(wxEVT_LEFT_DOWN, [this, combobox](wxMouseEvent &e) {
        m_current_language_selected = combobox->GetSelection();
        e.Skip();
    });

    combobox->Bind(wxEVT_COMBOBOX, [this, param, vlist, combobox](wxCommandEvent &e) {
        if (combobox->GetSelection() == m_current_language_selected)
            return;

        if (e.GetString().mb_str() != app_config->get(param)) {
            {
                //check if the project has changed
                if (wxGetApp().plater()->is_project_dirty()) {
                    auto result = MessageDialog(static_cast<wxWindow*>(this), _L("The current project has unsaved changes, save it before continuing?"),
                        wxString(SLIC3R_APP_FULL_NAME) + " - " + _L("Save"), wxYES_NO | wxCANCEL | wxYES_DEFAULT | wxCENTRE).ShowModal();

                    if (result == wxID_YES) {
                        wxGetApp().plater()->save_project();
                    }
                }


                // the dialog needs to be destroyed before the call to switch_language()
                // or sometimes the application crashes into wxDialogBase() destructor
                // so we put it into an inner scope
                MessageDialog msg_wingow(nullptr, _L("Switching the language requires application restart.\n") + "\n" + _L("Do you want to continue?"),
                                         _L("Language selection"), wxICON_QUESTION | wxOK | wxCANCEL);
                if (msg_wingow.ShowModal() == wxID_CANCEL) {
                    combobox->SetSelection(m_current_language_selected);
                    return;
                }
            }

            auto check = [this](bool yes_or_no) {
                // if (yes_or_no)
                //    return true;
                int act_btns = UnsavedChangesDialog::ActionButtons::SAVE;
                return wxGetApp().check_and_keep_current_preset_changes(_L("Switching application language"),
                                                                        _L("Switching application language while some presets are modified."), act_btns);
            };

            m_current_language_selected = combobox->GetSelection();
            if (m_current_language_selected >= 0 && m_current_language_selected < vlist.size()) {
                app_config->set(param, vlist[m_current_language_selected]->CanonicalName.ToUTF8().data());
                app_config->save();

                wxGetApp().load_language(vlist[m_current_language_selected]->CanonicalName, false);
                Close();
                // Reparent(nullptr);
                GetParent()->RemoveChild(this);
                Label::initSysFont(I18N::language_mode_profile().font_language);
                wxGetApp().recreate_GUI(_L("Changing application language"));
            }
        }

        e.Skip();
    });

    return m_sizer_combox;
}

wxBoxSizer *PreferencesDialog::create_item_language_mode_combobox(
    wxString title, wxWindow *parent, wxString tooltip, std::string param,
    const std::vector<std::pair<std::string, wxString>> &choices)
{
    assert(!choices.empty());

    auto *row = new wxBoxSizer(wxHORIZONTAL);
    row->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    row->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));

    auto *combo_title = new wxStaticText(parent, wxID_ANY, title, wxDefaultPosition,
                                          wxSize(FromDIP(TITLE_WIDTH), -1), 0);
    combo_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    combo_title->SetFont(::Label::Body_13);
    combo_title->SetToolTip(tooltip);
    combo_title->Wrap(-1);
    row->Add(combo_title, wxSizerFlags().CenterVertical().Proportion(1));

    auto *combobox = new ::ComboBox(parent, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                     wxSize(FromDIP(LANGUAGE_COMBOBOX_WIDTH), -1),
                                     0, nullptr, wxCB_READONLY);
    m_combobox_list[m_combobox_list.size()] = combobox;
    combobox->SetFont(::Label::Body_13);
    combobox->GetDropDown().SetFont(::Label::Body_13);
    combobox->SetCornerRadius(FromDIP(10)); // MD3 SelectField r10

    const std::string configured = I18N::normalize_language_mode_id(app_config->get(param));
    m_current_language_selected = -1;
    for (size_t index = 0; index < choices.size(); ++index) {
        combobox->Append(choices[index].second);
        const std::string candidate = I18N::normalize_language_mode_id(choices[index].first);
        if (candidate == configured ||
            (I18N::is_baseline_language_mode(candidate) && I18N::is_baseline_language_mode(configured)))
            m_current_language_selected = static_cast<int>(index);
    }
    if (m_current_language_selected < 0)
        m_current_language_selected = 0;
    combobox->SetSelection(m_current_language_selected);
    row->Add(combobox, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(ITEM_RIGHT_PADDING)));

    combobox->Bind(wxEVT_LEFT_DOWN, [this, combobox](wxMouseEvent &event) {
        m_current_language_selected = combobox->GetSelection();
        event.Skip();
    });

    combobox->Bind(wxEVT_COMBOBOX, [this, param, choices, combobox](wxCommandEvent &event) {
        const int selected = combobox->GetSelection();
        if (selected == m_current_language_selected || selected < 0 ||
            selected >= static_cast<int>(choices.size())) {
            event.Skip();
            return;
        }

        if (wxGetApp().plater()->is_project_dirty()) {
            const auto result = MessageDialog(
                static_cast<wxWindow *>(this),
                _L("The current project has unsaved changes, save it before continuing?"),
                wxString(SLIC3R_APP_FULL_NAME) + " - " + _L("Save"),
                wxYES_NO | wxCANCEL | wxYES_DEFAULT | wxCENTRE).ShowModal();
            if (result == wxID_CANCEL) {
                combobox->SetSelection(m_current_language_selected);
                return;
            }
            if (result == wxID_YES)
                wxGetApp().plater()->save_project();
        }

        auto restart_copy = I18N::translate_mode(L("Switching the language requires application restart.\n"));
        restart_copy.primary.Trim();
        restart_copy.secondary.Trim();
        const auto restart_text = I18N::render_localized_text_stacked(
            restart_copy.finalize_without_arguments());
        const auto continue_text = I18N::render_localized_text_stacked(
            I18N::translate_mode(L("Do you want to continue?")).finalize_without_arguments());
        const auto caption_text = I18N::render_localized_text_compact(
            I18N::translate_mode(L("Language selection")).finalize_without_arguments());
        MessageDialog confirm(nullptr, restart_text.label + "\n\n" + continue_text.label,
                              caption_text.label, wxICON_QUESTION | wxOK | wxCANCEL);
        if (confirm.ShowModal() == wxID_CANCEL) {
            combobox->SetSelection(m_current_language_selected);
            return;
        }

        int action_buttons = UnsavedChangesDialog::ActionButtons::SAVE;
        if (!wxGetApp().check_and_keep_current_preset_changes(
                _L("Switching application language"),
                _L("Switching application language while some presets are modified."), action_buttons)) {
            combobox->SetSelection(m_current_language_selected);
            return;
        }

        const std::string previous = app_config->get(param);
        const std::string next = I18N::normalize_language_mode_id(choices[selected].first);
        app_config->set(param, next);
        app_config->save();
        if (!wxGetApp().load_language(from_u8(next), false)) {
            app_config->set(param, previous);
            app_config->save();
            combobox->SetSelection(m_current_language_selected);
            return;
        }

        m_current_language_selected = selected;
        Close();
        GetParent()->RemoveChild(this);
        Label::initSysFont(I18N::language_mode_profile().font_language);
        wxGetApp().recreate_GUI(_L("Changing application language"));
        event.Skip();
    });

    return row;
}

wxBoxSizer *PreferencesDialog::create_item_region_combobox(wxString title, wxWindow *parent, wxString tooltip, std::vector<wxString> vlist)
{
    std::vector<wxString> local_regions = {"Asia-Pacific", "China", "Europe", "North America", "Others"};

    wxBoxSizer *m_sizer_combox = new wxBoxSizer(wxHORIZONTAL);
    m_sizer_combox->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    m_sizer_combox->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));

    auto combo_title = new wxStaticText(parent, wxID_ANY, title, wxDefaultPosition, wxSize(FromDIP(TITLE_WIDTH), -1), 0);
    combo_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    combo_title->SetFont(::Label::Body_13);
    combo_title->SetToolTip(tooltip);
    combo_title->Wrap(-1);
    m_sizer_combox->Add(combo_title, wxSizerFlags().CenterVertical().Proportion(1));

    auto combobox = new ::ComboBox(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(LARGE_COMBOBOX_WIDTH), -1), 0, nullptr, wxCB_READONLY);
    m_combobox_list[m_combobox_list.size()] = combobox;
    combobox->SetFont(::Label::Body_13);
    combobox->GetDropDown().SetFont(::Label::Body_13);
    combobox->SetCornerRadius(FromDIP(10)); // MD3 SelectField r10
    m_sizer_combox->Add(combobox, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(ITEM_RIGHT_PADDING)));

    std::vector<wxString>::iterator iter;
    for (iter = vlist.begin(); iter != vlist.end(); iter++) { combobox->Append(*iter); }

    AppConfig * config       = GUI::wxGetApp().app_config;

    int         current_region = 0;
    if (!config->get("region").empty()) {
        std::string country_code = config->get("region");
        for (auto i = 0; i < vlist.size(); i++) {
            if (local_regions[i].ToStdString() == country_code) {
                combobox->SetSelection(i);
                current_region = i;
            }
        }
    }

    combobox->GetDropDown().Bind(wxEVT_COMBOBOX, [this, combobox, current_region, local_regions](wxCommandEvent &e) {
        auto region_index = e.GetSelection();
        auto region       = local_regions[region_index];

        combobox->SetSelection(region_index);
        NetworkAgent* agent = wxGetApp().getAgent();
        AppConfig* config = GUI::wxGetApp().app_config;
        if (agent) {
            MessageDialog msg_wingow(this, _L("Changing the region will log out your account.\n") + "\n" + _L("Do you want to continue?"), _L("Region selection"),
                                     wxICON_QUESTION | wxOK | wxCANCEL);
            if (msg_wingow.ShowModal() == wxID_CANCEL) {
                combobox->SetSelection(current_region);
                return;
            } else {
                wxGetApp().request_user_logout();
                config->set("region", region.ToStdString());
                wxGetApp().update_log_sink_region();
                auto area = config->get_country_code();
                if (agent) {
                    agent->set_country_code(area);
                }
                EndModal(wxID_CANCEL);
            }
        } else {
            config->set("region", region.ToStdString());
            wxGetApp().update_log_sink_region();
        }
        wxGetApp().update_publish_status();
        //e.Skip();
    });

    return m_sizer_combox;
}

wxBoxSizer *PreferencesDialog::create_item_loglevel_combobox(wxString title, wxWindow *parent, wxString tooltip, std::vector<wxString> vlist)
{
    wxBoxSizer *m_sizer_combox = new wxBoxSizer(wxHORIZONTAL);
    m_sizer_combox->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    m_sizer_combox->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));

    auto combo_title = new wxStaticText(parent, wxID_ANY, title, wxDefaultPosition, wxSize(FromDIP(TITLE_WIDTH), -1), 0);
    combo_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    combo_title->SetFont(::Label::Body_13);
    combo_title->SetToolTip(tooltip);
    combo_title->Wrap(-1);
    m_sizer_combox->Add(combo_title, wxSizerFlags().CenterVertical().Proportion(1));

    auto combobox                           = new ::ComboBox(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(COMBOBOX_WIDTH), -1), 0, nullptr, wxCB_READONLY);
    m_combobox_list[m_combobox_list.size()] = combobox;
    combobox->SetFont(::Label::Body_13);
    combobox->GetDropDown().SetFont(::Label::Body_13);
    combobox->SetCornerRadius(FromDIP(10)); // MD3 SelectField r10

    std::vector<wxString>::iterator iter;
    for (iter = vlist.begin(); iter != vlist.end(); iter++) { combobox->Append(*iter); }

    auto severity_level = app_config->get("severity_level");
    if (!severity_level.empty()) { combobox->SetValue(severity_level); }

    m_sizer_combox->Add(combobox, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(ITEM_RIGHT_PADDING)));

    // save config
    combobox->GetDropDown().Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &e) {
        auto level = Slic3r::get_string_logging_level(e.GetSelection());
        Slic3r::set_logging_level(Slic3r::level_string_to_boost(level));
        app_config->set("severity_level",level);
        app_config->save();
        e.Skip();
     });
    return m_sizer_combox;
}


wxBoxSizer *PreferencesDialog::create_item_multiple_combobox(
    wxString title, wxWindow *parent, wxString tooltip, int padding_left, std::string param, std::vector<wxString> vlista, std::vector<wxString> vlistb)
{
    std::vector<wxString> params;
    Split(app_config->get(param), "/", params);

    std::vector<wxString>::iterator iter;

   wxBoxSizer *m_sizer_tcombox= new wxBoxSizer(wxHORIZONTAL);
   m_sizer_tcombox->Add(0, 0, 0, wxEXPAND | wxLEFT, 23);
   m_sizer_tcombox->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));

   auto combo_title = new wxStaticText(parent, wxID_ANY, title, wxDefaultPosition, wxSize(FromDIP(TITLE_WIDTH), -1), 0);
   combo_title->SetToolTip(tooltip);
   combo_title->Wrap(-1);
   combo_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
   combo_title->SetFont(::Label::Body_13);
   m_sizer_tcombox->Add(combo_title, 0, wxALIGN_CENTER | wxALL, 3);

   auto combobox_left                      = new ::ComboBox(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(COMBOBOX_WIDTH), -1), 0, nullptr, wxCB_READONLY);
   m_combobox_list[m_combobox_list.size()] = combobox_left;
   combobox_left->SetFont(::Label::Body_13);
   combobox_left->GetDropDown().SetFont(::Label::Body_13);


   for (iter = vlista.begin(); iter != vlista.end(); iter++) { combobox_left->Append(*iter); }
   combobox_left->SetValue(std::string(params[0].mb_str()));
   m_sizer_tcombox->Add(combobox_left, 0, wxALIGN_CENTER, 0);

   auto combo_title_add = new wxStaticText(parent, wxID_ANY, wxT("+"), wxDefaultPosition, wxDefaultSize, 0);
   combo_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
   combo_title->SetFont(::Label::Body_13);
   combo_title_add->Wrap(-1);
   m_sizer_tcombox->Add(combo_title_add, 0, wxALIGN_CENTER | wxALL, 3);

   auto combobox_right                     = new ::ComboBox(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(COMBOBOX_WIDTH), -1), 0, nullptr, wxCB_READONLY);
   m_combobox_list[m_combobox_list.size()] = combobox_right;
   combobox_right->SetFont(::Label::Body_13);
   combobox_right->GetDropDown().SetFont(::Label::Body_13);

   for (iter = vlistb.begin(); iter != vlistb.end(); iter++) { combobox_right->Append(*iter); }
   combobox_right->SetValue(std::string(params[1].mb_str()));
   m_sizer_tcombox->Add(combobox_right, 0, wxALIGN_CENTER, 0);

    // save config
    combobox_left->GetDropDown().Bind(wxEVT_COMBOBOX, [this, param, combobox_right](wxCommandEvent &e) {
        auto config = e.GetString() + wxString("/") + combobox_right->GetValue();
        app_config->set(param, std::string(config.mb_str()));
        app_config->save();
        e.Skip();
    });

    combobox_right->GetDropDown().Bind(wxEVT_COMBOBOX, [this, param, combobox_left](wxCommandEvent &e) {
        auto config = combobox_left->GetValue() + wxString("/") + e.GetString();
        app_config->set(param, std::string(config.mb_str()));
        app_config->save();
        e.Skip();
    });

    return m_sizer_tcombox;
}

wxBoxSizer *PreferencesDialog::create_item_input(wxString title, wxString title2, wxWindow *parent, wxString tooltip, std::string param, std::function<void(wxString)> onchange)
{
    wxBoxSizer *sizer_input = new wxBoxSizer(wxHORIZONTAL);
    sizer_input->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));
    auto        input_title   = new wxStaticText(parent, wxID_ANY, title);
    input_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    input_title->SetFont(::Label::Body_13);
    input_title->SetToolTip(tooltip);
    input_title->Wrap(-1);

    auto       input = new ::TextInput(parent, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(INPUT_WIDTH), -1), wxTE_PROCESS_ENTER);
    // MD3 ValueField fill: SurfaceContainerHighest (enabled) / -High (disabled),
    // resolved by role so it re-themes in dark — replaces the Grey250/White literal.
    StateColor input_bg(std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHigh), StateColor::Disabled), std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHighest), StateColor::Enabled));
    input->SetBackgroundColor(input_bg);
    input->SetCornerRadius(FromDIP(10));
    input->GetTextCtrl()->SetFont(::Label::Mono_13);
    input->GetTextCtrl()->SetValue(app_config->get(param));
    wxTextValidator validator(wxFILTER_DIGITS);
    input->GetTextCtrl()->SetValidator(validator);

    wxStaticText *second_title = nullptr;
    if (!title2.empty()) {
        second_title = new wxStaticText(parent, wxID_ANY, title2, wxDefaultPosition, wxSize(FromDIP(TITLE_WIDTH), -1), 0);
        second_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
        second_title->SetFont(::Label::Body_13);
        second_title->SetToolTip(tooltip);
        second_title->Wrap(-1);
    }

    sizer_input->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    sizer_input->Add(input_title, wxSizerFlags().CenterVertical().Proportion(1));
    sizer_input->Add(input, wxSizerFlags().CenterVertical().Border(wxRIGHT, ITEM_RIGHT_PADDING));
    if (second_title) sizer_input->Add(second_title, 0, wxALIGN_CENTER_VERTICAL | wxALL, 3);

    input->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, [this, param, input, onchange](wxCommandEvent &e) {
        auto value = input->GetTextCtrl()->GetValue();
        app_config->set(param, std::string(value.mb_str()));
        app_config->save();
        onchange(value);
        e.Skip();
    });

    input->GetTextCtrl()->Bind(wxEVT_KILL_FOCUS, [this, param, input, onchange](wxFocusEvent &e) {
        auto value = input->GetTextCtrl()->GetValue();
        app_config->set(param, std::string(value.mb_str()));
        app_config->save();
        onchange(value);
        e.Skip();
    });

    return sizer_input;
}

wxBoxSizer *PreferencesDialog::create_item_range_input(
    wxString title, wxWindow *parent, wxString tooltip, std::string param, float range_min, float range_max, int keep_digital, std::function<void(wxString)> onchange)
{
    wxBoxSizer *sizer_input = new wxBoxSizer(wxHORIZONTAL);
    sizer_input->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));
    auto        input_title = new wxStaticText(parent, wxID_ANY, title);
    input_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    input_title->SetFont(::Label::Body_13);
    input_title->SetToolTip(tooltip);
    input_title->Wrap(-1);

    auto float_value = std::atof(app_config->get(param).c_str());
    if (float_value < range_min || float_value > range_max) {
        float_value = range_min;
        app_config->set(param, std::to_string(range_min));
        app_config->save();
    }
    auto       input = new ::TextInput(parent, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(INPUT_WIDTH), -1), wxTE_PROCESS_ENTER);
    // MD3 ValueField fill: SurfaceContainerHighest (enabled) / -High (disabled),
    // resolved by role so it re-themes in dark — replaces the Grey250/White literal.
    StateColor input_bg(std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHigh), StateColor::Disabled), std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHighest), StateColor::Enabled));
    input->SetBackgroundColor(input_bg);
    input->SetCornerRadius(FromDIP(10));
    input->GetTextCtrl()->SetFont(::Label::Mono_13);
    input->GetTextCtrl()->SetValue(app_config->get(param));
    wxTextValidator validator(wxFILTER_NUMERIC);
    input->GetTextCtrl()->SetValidator(validator);

    sizer_input->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    sizer_input->Add(input_title, wxSizerFlags().CenterVertical().Proportion(1));
    sizer_input->Add(input, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(ITEM_RIGHT_PADDING)));
    auto format_str=[](int keep_digital,float val){
        std::stringstream ss;
        ss << std::fixed << std::setprecision(keep_digital) << val;
        return ss.str();
    };
    auto set_value_to_app = [this, param, onchange, input, range_min, range_max, format_str, keep_digital](float value, bool update_slider) {
        if (value < range_min) { value = range_min; }
        if (value > range_max) { value = range_max; }
        auto str = format_str(keep_digital, value);
        app_config->set(param, str);
        app_config->save();
        if (onchange) {
            onchange(str);
        }
        input->GetTextCtrl()->SetValue(str);
    };
    input->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, [this, set_value_to_app, input](wxCommandEvent &e) {
        auto value = std::atof(input->GetTextCtrl()->GetValue().c_str());
        set_value_to_app(value,true);
        e.Skip();
    });

    input->GetTextCtrl()->Bind(wxEVT_KILL_FOCUS, [this, set_value_to_app, input](wxFocusEvent &e) {
        auto value = std::atof(input->GetTextCtrl()->GetValue().c_str());
        set_value_to_app(value, true);
        e.Skip();
    });

    return sizer_input;
}

wxBoxSizer *PreferencesDialog::create_item_range_two_input(wxString                      title,
                                                           wxWindow *                    parent,
                                                           wxString                      tooltip,
                                                           std::string                   param,
                                                           std::string                   param1,
                                                           float                         range_min,
                                                           float                         range_max,
                                                           int                           keep_digital,
                                                           std::function<void(wxString)> onchange,
                                                           std::function<void(wxString)> onchange1)
{
    wxBoxSizer *sizer_input = new wxBoxSizer(wxHORIZONTAL);
    sizer_input->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));
    auto        input_title = new wxStaticText(parent, wxID_ANY, title);
    input_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    input_title->SetFont(::Label::Body_13);
    input_title->SetToolTip(tooltip);
    input_title->Wrap(-1);

    auto float_value = std::atof(app_config->get(param).c_str());
    if (float_value < range_min || float_value > range_max) {
        float_value = range_min;
        app_config->set(param, std::to_string(range_min));
        app_config->save();
    }
    float_value = std::atof(app_config->get(param1).c_str());
    if (float_value < range_min || float_value > range_max) {
        float_value = range_min;
        app_config->set(param1, std::to_string(range_min));
        app_config->save();
    }
    auto       input = new ::TextInput(parent, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(INPUT_WIDTH), -1), wxTE_PROCESS_ENTER);
    // MD3 ValueField fill: SurfaceContainerHighest (enabled) / -High (disabled),
    // resolved by role so it re-themes in dark — replaces the Grey250/White literal.
    StateColor input_bg(std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHigh), StateColor::Disabled), std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHighest), StateColor::Enabled));
    input->SetBackgroundColor(input_bg);
    input->SetCornerRadius(FromDIP(10));
    input->GetTextCtrl()->SetFont(::Label::Mono_13);
    input->GetTextCtrl()->SetValue(app_config->get(param));
    wxTextValidator validator(wxFILTER_NUMERIC);
    input->GetTextCtrl()->SetValidator(validator);

    auto input1 = new ::TextInput(parent, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(INPUT_WIDTH), -1), wxTE_PROCESS_ENTER);
    input1->SetBackgroundColor(input_bg);
    input1->SetCornerRadius(FromDIP(10));
    input1->GetTextCtrl()->SetFont(::Label::Mono_13);
    input1->GetTextCtrl()->SetValue(app_config->get(param1));
    input1->GetTextCtrl()->SetValidator(validator);

    sizer_input->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    sizer_input->Add(input_title, wxSizerFlags().CenterVertical().Proportion(1));
    sizer_input->Add(input, 0, wxALIGN_CENTER_VERTICAL, 0);

    sizer_input->AddSpacer(FromDIP(8));
    sizer_input->Add(input1, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(ITEM_RIGHT_PADDING)));
    auto format_str = [](int keep_digital, float val) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(keep_digital) << val;
        return ss.str();
    };
    auto set_value_to_app = [this, param, onchange, input, range_min, range_max, format_str, keep_digital](float value, bool update_slider) {
        if (value < range_min) { value = range_min; }
        if (value > range_max) { value = range_max; }
        auto str = format_str(keep_digital, value);
        app_config->set(param, str);
        app_config->save();
        if (onchange) { onchange(str); }
        input->GetTextCtrl()->SetValue(str);
    };
    input->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, [this, set_value_to_app, input](wxCommandEvent &e) {
        auto value = std::atof(input->GetTextCtrl()->GetValue().c_str());
        set_value_to_app(value, true);
        e.Skip();
    });

    input->GetTextCtrl()->Bind(wxEVT_KILL_FOCUS, [this, set_value_to_app, input](wxFocusEvent &e) {
        auto value = std::atof(input->GetTextCtrl()->GetValue().c_str());
        set_value_to_app(value, true);
        e.Skip();
    });

    auto set_value1_to_app = [this, param1, onchange1, input1, range_min, range_max, format_str, keep_digital](float value, bool update_slider) {
        if (value < range_min) { value = range_min; }
        if (value > range_max) { value = range_max; }
        auto str = format_str(keep_digital, value);
        app_config->set(param1, str);
        app_config->save();
        if (onchange1) { onchange1(str); }
        input1->GetTextCtrl()->SetValue(str);
    };
    input1->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, [this, set_value1_to_app, input1](wxCommandEvent &e) {
        auto value = std::atof(input1->GetTextCtrl()->GetValue().c_str());
        set_value1_to_app(value, true);
        e.Skip();
    });

    input1->GetTextCtrl()->Bind(wxEVT_KILL_FOCUS, [this, set_value1_to_app, input1](wxFocusEvent &e) {
        auto value = std::atof(input1->GetTextCtrl()->GetValue().c_str());
        set_value1_to_app(value, true);
        e.Skip();
    });

    return sizer_input;
}

wxBoxSizer *PreferencesDialog::create_item_switch(wxString title, wxWindow *parent, wxString tooltip ,std::string param)
{
    wxBoxSizer *m_sizer_switch = new wxBoxSizer(wxHORIZONTAL);
    m_sizer_switch->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));
    auto        switch_title   = new wxStaticText(parent, wxID_ANY, title, wxDefaultPosition, wxSize(FromDIP(TITLE_WIDTH), -1), 0);
    switch_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    switch_title->SetFont(::Label::Body_13);
    switch_title->SetToolTip(tooltip);
    switch_title->Wrap(-1);
    auto switchbox = new ::SwitchButton(parent, wxID_ANY);
    switchbox->SetValue(app_config->get(param) == "true");

    m_sizer_switch->Add(0, 0, 0, wxEXPAND | wxLEFT, 23);
    m_sizer_switch->Add(switch_title, 0, wxALIGN_CENTER | wxALL, 3);
    m_sizer_switch->Add( 0, 0, 1, wxEXPAND, 0 );
    m_sizer_switch->Add(switchbox, 0, wxALIGN_CENTER, 0);
    m_sizer_switch->Add( 0, 0, 0, wxEXPAND|wxLEFT, 40 );

    //// save config — the handler was previously a no-op stub; wire it to the
    //// backing AppConfig key so the MD3 Switch actually persists its value.
    switchbox->Bind(wxEVT_TOGGLEBUTTON, [this, switchbox, param](wxCommandEvent &e) {
        app_config->set_bool(param, switchbox->GetValue());
        app_config->save();
        e.Skip();
    });
    return m_sizer_switch;
}

// Apply a dark/light theme switch and fan out the same side effects the legacy
// "Enable dark mode" checkbox performed (dark-mode flag, native repaint on MSW,
// and the GL canvas colour-mode event). Driven by the Appearance Theme
// SegmentedControl; cross-platform (the MSW-only repaint stays behind its guard).
void PreferencesDialog::apply_dark_mode(bool dark)
{
    wxGetApp().Update_dark_mode_flag();

    //dark mode
#ifdef _MSW_DARK_MODE
    wxGetApp().force_colors_update();
    wxGetApp().update_ui_from_settings();
    set_dark_mode();
#endif
    SimpleEvent evt = SimpleEvent(EVT_GLCANVAS_COLOR_MODE_CHANGED);
    wxPostEvent(wxGetApp().plater(), evt);
}

void PreferencesDialog::set_dark_mode()
{
#ifdef __WINDOWS__
#ifdef _MSW_DARK_MODE
    NppDarkMode::SetDarkExplorerTheme(this->GetHWND());
    NppDarkMode::SetDarkTitleBar(this->GetHWND());
    wxGetApp().UpdateDlgDarkUI(this);
    SetActiveWindow(wxGetApp().mainframe->GetHWND());
    SetActiveWindow(GetHWND());
#endif
#endif
}

// Sync the MD3 runtime density + accent token state (MD3Tokens.hpp) to the
// persisted Appearance choices. Called at Preferences construction so the tokens
// reflect the user's saved selection; widgets/dialogs built afterwards read the
// active density metrics and the accent-recoloured roles. The same persisted
// state is also applied at process startup in GUI_App::on_init_inner (before
// the first window is built), so a saved choice takes effect on a fresh launch;
// this construction-time sync keeps the tokens honest if the config changed
// underneath a running process.
static void apply_persisted_md3_appearance()
{
    auto *cfg = wxGetApp().app_config;
    if (!cfg)
        return;
    MD3::Metrics::setDensity(cfg->get("ui_density") == "compact" ? MD3::Metrics::Density::Compact
                                                                 : MD3::Metrics::Density::Comfortable);
    std::string seed = cfg->get("ui_accent_seed");
    if (seed.empty())
        seed = "#146c2e"; // Brand seed clears the override -> pristine Brand tones
    wxColour seed_colour(wxString::FromUTF8(seed));
    // A hand-corrupted persisted value parses to an invalid wxColour whose RGB
    // reads as black; fall back to the Brand seed (clears the override) rather
    // than seeding a near-black accent.
    if (!seed_colour.IsOk())
        seed_colour = wxColour(wxString::FromUTF8("#146c2e"));
    MD3::setAccentSeed(seed_colour);
}

// Re-theme the live UI after an Appearance accent/density change, reusing the
// same fan-out the light/dark toggle performs (see apply_dark_mode): push
// freshly-resolved MD3 role colours to the wx widget tree, refresh the 3D
// viewport chrome, then repaint/relayout the open Preferences dialog so its
// swatches, nav pills and segmented controls update at once. Accent (colour)
// changes propagate live; a density change fully re-lays-out only windows built
// after the change (restart-scoped for already-open windows — see the followup).
static void refresh_md3_appearance(wxWindow *dialog)
{
#ifdef _MSW_DARK_MODE
    wxGetApp().force_colors_update();
    wxGetApp().update_ui_from_settings();
#endif
    if (wxGetApp().plater()) {
        SimpleEvent evt = SimpleEvent(EVT_GLCANVAS_COLOR_MODE_CHANGED);
        wxPostEvent(wxGetApp().plater(), evt);
    }
    if (wxGetApp().mainframe)
        wxGetApp().mainframe->Refresh();
    if (dialog) {
        dialog->Refresh();
        dialog->Layout();
    }
}

wxBoxSizer *PreferencesDialog::create_item_checkbox(wxString title, wxWindow *parent, wxString tooltip, int padding_left, std::string param)
{
    wxBoxSizer *m_sizer_checkbox  = new wxBoxSizer(wxHORIZONTAL);
    m_sizer_checkbox->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));

    // MD3 general-settings row: the boolean preference is a right-aligned MD3
    // Switch (44x24 icon-mode SwitchButton) instead of the legacy leading
    // CheckBox. The AppConfig read/write bindings below are unchanged — only the
    // control class and the row anatomy change.
    auto checkbox = new ::SwitchButton(parent, wxID_ANY);
    m_checkbox_list[m_checkbox_list.size()] = checkbox;
    if (param == "privacyuse") {
        checkbox->SetValue((app_config->get("firstguide", param) == "true") ? true : false);
    } else if (param == "auto_stop_liveview") {
        checkbox->SetValue((app_config->get("liveview", param) == "true") ? false : true);
    } else {
        checkbox->SetValue((app_config->get(param) == "true") ? true : false);
    }

    // Two-line label column: primary 13.5/OnSurface over an optional secondary
    // description (12/OnSurfaceVariant) sourced from the tooltip when it adds
    // information beyond the primary label.
    auto *text_col = new wxBoxSizer(wxVERTICAL);
    auto  checkbox_title = new wxStaticText(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, 0);
    checkbox_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    checkbox_title->SetFont(::Label::Body_13);
    checkbox_title->Wrap(FromDIP(320));
    text_col->Add(checkbox_title, 0);
    if (!tooltip.empty() && tooltip != title) {
        auto *checkbox_desc = new wxStaticText(parent, wxID_ANY, tooltip, wxDefaultPosition, wxDefaultSize, 0);
        checkbox_desc->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        checkbox_desc->SetFont(::Label::Body_12);
        checkbox_desc->Wrap(FromDIP(320));
        text_col->Add(checkbox_desc, 0, wxTOP, FromDIP(2));
    }

    m_sizer_checkbox->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    m_sizer_checkbox->Add(text_col, wxSizerFlags().CenterVertical().Proportion(1));
    m_sizer_checkbox->Add(checkbox, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(ITEM_RIGHT_PADDING)));

    //// save config
    checkbox->Bind(wxEVT_TOGGLEBUTTON, [this, checkbox, param](wxCommandEvent &e) {
        if (param == "privacyuse") {
            app_config->set("firstguide", param, checkbox->GetValue());
            NetworkAgent* agent = GUI::wxGetApp().getAgent();
            if (!checkbox->GetValue()) {
                if (agent) {
                    agent->track_enable(false);
                    agent->track_remove_files();
                }
            }
            wxGetApp().save_privacy_policy_history(checkbox->GetValue(), "preferences");
            app_config->save();
        }
        else if (param == "auto_stop_liveview") {
            app_config->set("liveview", param, !checkbox->GetValue());
        }
        else {
            app_config->set_bool(param, checkbox->GetValue());
            app_config->save();
        }

        if (param == "staff_pick_switch") {
            bool pbool = app_config->get("staff_pick_switch") == "true";
            wxGetApp().switch_staff_pick(pbool);
        }

        if (param == "sync_user_preset") {
            bool sync = app_config->get("sync_user_preset") == "true" ? true : false;
            if (sync) {
                wxGetApp().start_sync_user_preset();
            } else {
                wxGetApp().stop_sync_user_preset();
            }
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << " sync_user_preset: " << (sync ? "true" : "false");
        }

        #ifdef __WXMSW__
        if (param == "associate_3mf") {
             bool pbool = app_config->get("associate_3mf") == "true" ? true : false;
             if (pbool) {
                 wxGetApp().associate_files(L"3mf");
             } else {
                 wxGetApp().disassociate_files(L"3mf");
             }
        }

        if (param == "associate_stl") {
            bool pbool = app_config->get("associate_stl") == "true" ? true : false;
            if (pbool) {
                wxGetApp().associate_files(L"stl");
            } else {
                wxGetApp().disassociate_files(L"stl");
            }
        }

        if (param == "associate_step") {
            bool pbool = app_config->get("associate_step") == "true" ? true : false;
            if (pbool) {
                wxGetApp().associate_files(L"step");
            } else {
                wxGetApp().disassociate_files(L"step");
            }
        }

        #endif // __WXMSW__

        if (param == "developer_mode")
        {
            m_developer_mode_def = app_config->get("developer_mode");
            if (m_developer_mode_def == "true") {
                Slic3r::GUI::wxGetApp().save_mode(comDevelop);
            } else {
                Slic3r::GUI::wxGetApp().save_mode(comAdvanced);
            }
        }

        // webview  dump_vedio
        if (param == "internal_developer_mode") {
            m_internal_developer_mode_def = app_config->get("internal_developer_mode");
            if (m_internal_developer_mode_def == "true") {
                Slic3r::GUI::wxGetApp().update_internal_development();
                Slic3r::GUI::wxGetApp().mainframe->show_log_window();
            } else {
                Slic3r::GUI::wxGetApp().update_internal_development();
            }
        }

        if (param == "show_print_history") {
            auto show_history = app_config->get_bool("show_print_history");
            if (show_history == true) {
                if (wxGetApp().mainframe && wxGetApp().mainframe->m_webview) { wxGetApp().mainframe->m_webview->ShowUserPrintTask(true,true); }
            } else {
                if (wxGetApp().mainframe && wxGetApp().mainframe->m_webview) { wxGetApp().mainframe->m_webview->ShowUserPrintTask(false); }
            }
        }

        if (param == "enable_lod") {
            if (wxGetApp().plater()->is_project_dirty()) {
                auto result = MessageDialog(static_cast<wxWindow *>(this), _L("The current project has unsaved changes, save it before continuing?"),
                                            wxString(SLIC3R_APP_FULL_NAME) + " - " + _L("Save"), wxYES_NO  | wxYES_DEFAULT | wxCENTRE)
                                  .ShowModal();
                if (result == wxID_YES) {
                    wxGetApp().plater()->save_project();
                }
            }
            MessageDialog msg_wingow(nullptr, _L("Please note that the model show will undergo certain changes at small pixels case.\nEnabled LOD requires application restart.") + "\n" + _L("Do you want to continue?"), _L("Enable LOD"),
                wxYES| wxYES_DEFAULT | wxCANCEL | wxCENTRE);
            if (msg_wingow.ShowModal() == wxID_YES) {
                Close();
                GetParent()->RemoveChild(this);
                wxGetApp().recreate_GUI(_L("Enable LOD"));
            } else {
                checkbox->SetValue(!checkbox->GetValue());
                app_config->set_bool(param, checkbox->GetValue());
                app_config->save();
            }
        }

        if (param == "enable_record_gcodeviewer_option_item"){
            SimpleEvent evt(EVT_ENABLE_GCODE_OPTION_ITEM_CHANGED);
            wxPostEvent(wxGetApp().plater(), evt);
        }

        if (param == "enable_high_low_temp_mixed_printing") {
            if (checkbox->GetValue()) {
                const wxString warning_title = _L("Bed Temperature Difference Warning");
                const wxString warning_message =
                    _L("Using filaments with significantly different temperatures may cause:\n"
                        "• Extruder clogging\n"
                        "• Nozzle damage\n"
                        "• Layer adhesion issues\n\n"
                        "Continue with enabling this feature?");
                std::function<void(const wxString&)> link_callback = [](const wxString&) {
                            const std::string lang_code = wxGetApp().app_config->get("language");
                            const wxString region = (lang_code.find("zh") != std::string::npos) ? L"zh" : L"en";
                            const wxString wiki_url = wxString::Format(
                                L"https://wiki.bambulab.com/%s/filament-acc/filament/h2d-filament-config-limit",
                                region
                            );
                            wxGetApp().open_browser_with_warning_dialog(wiki_url);
                            };

                MessageDialog msg_dialog(
                    nullptr,
                    warning_message,
                    warning_title,
                    wxICON_WARNING | wxYES_NO | wxCANCEL | wxYES_DEFAULT | wxCENTRE,
                    wxEmptyString,
                    _L("Click Wiki for help."),
                    link_callback
                );

                if (msg_dialog.ShowModal() != wxID_YES) {
                    checkbox->SetValue(false);
                    app_config->set_bool(param, false);
                    app_config->save();
                }
            }
        }
        e.Skip();
    });

    //// for debug mode
    if (param == "developer_mode") { m_developer_mode_ckeckbox = checkbox; }
    if (param == "internal_developer_mode") { m_internal_developer_mode_ckeckbox = checkbox; }


    checkbox->SetToolTip(tooltip);
    return m_sizer_checkbox;
}

wxWindow* PreferencesDialog::create_item_downloads(wxWindow* parent, int padding_left, std::string param)
{
    wxString download_path = wxString::FromUTF8(app_config->get("download_path"));
    auto item_panel = new wxWindow(parent, wxID_ANY);
    item_panel->SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));

    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    sizer->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));

    auto m_staticTextTitle = new wxStaticText(item_panel, wxID_ANY, _L("Download path"), wxDefaultPosition, wxDefaultSize, 0);
    m_staticTextTitle->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    m_staticTextTitle->SetFont(::Label::Body_13);
    m_staticTextTitle->Wrap(-1);

    auto m_staticTextPath = new ::TextInput(item_panel, download_path, wxEmptyString, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    // m_staticTextPath->SetBackgroundColor(ThemeColor::Grey250);
    // m_staticTextPath->SetBorderColor(ThemeColor::Grey350);
    m_staticTextPath->SetCornerRadius(FromDIP(4));
    m_staticTextPath->GetTextCtrl()->SetFont(::Label::Body_13);

    // MD3 outlined button (kit actions/Button): transparent interior + 1px
    // Outline ring with OnSurface text and a pill radius, resolved through
    // semantic roles by Button::applyMD3Style() — replaces the White/BrandGreen
    // StateColor literals.
    auto m_button_download = new Button(item_panel, _L("Browse"));
    m_button_list[m_button_list.size()] = m_button_download;
    m_button_download->SetVariant(Button::Variant::Outlined);
    m_button_download->SetButtonSize(Button::Size::Small);

    m_button_download->Bind(wxEVT_BUTTON, [this, m_staticTextPath, item_panel](auto& e) {
        wxString defaultPath = wxT("/");
        wxDirDialog dialog(this, _L("Choose Download Directory"), defaultPath, wxDD_NEW_DIR_BUTTON);

        if (dialog.ShowModal() == wxID_OK) {
            wxString download_path = dialog.GetPath();
            std::string download_path_str = download_path.ToUTF8().data();
            app_config->set("download_path", download_path_str);
            m_staticTextPath->GetTextCtrl()->SetValue(download_path);
            item_panel->Layout();
        }
        });

    sizer->Add(m_staticTextTitle, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(8)));
    sizer->Add(m_staticTextPath, wxSizerFlags().CenterVertical().Proportion(1).Border(wxRIGHT, FromDIP(8)));
    sizer->Add(m_button_download, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(ITEM_RIGHT_PADDING)));

    item_panel->SetSizer(sizer);
    item_panel->Layout();

    return item_panel;
}

// External-editor executable row (General > "External editor" set to Custom…):
// a read-only path field + an outlined Browse button opening a wxFileDialog
// whose selection persists to AppConfig key `param` ("external_editor_path").
// Same row anatomy as create_item_downloads above.
wxWindow* PreferencesDialog::create_item_external_editor(wxWindow* parent, int padding_left, std::string param)
{
    wxString editor_path = wxString::FromUTF8(app_config->get(param));
    auto item_panel = new wxWindow(parent, wxID_ANY);
    item_panel->SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));

    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    sizer->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));

    auto m_staticTextTitle = new wxStaticText(item_panel, wxID_ANY, _L("External editor path"), wxDefaultPosition, wxDefaultSize, 0);
    m_staticTextTitle->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    m_staticTextTitle->SetFont(::Label::Body_13);
    m_staticTextTitle->Wrap(-1);

    auto m_staticTextPath = new ::TextInput(item_panel, editor_path, wxEmptyString, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    m_staticTextPath->SetCornerRadius(FromDIP(4));
    m_staticTextPath->GetTextCtrl()->SetFont(::Label::Body_13);

    // MD3 outlined button, matching the Download-path Browse button above.
    auto m_button_browse = new Button(item_panel, _L("Browse"));
    m_button_list[m_button_list.size()] = m_button_browse;
    m_button_browse->SetVariant(Button::Variant::Outlined);
    m_button_browse->SetButtonSize(Button::Size::Small);

    m_button_browse->Bind(wxEVT_BUTTON, [this, m_staticTextPath, item_panel, param](auto& e) {
#ifdef __WXMSW__
        const wxString wildcard = _L("Executable files") + " (*.exe)|*.exe";
#else
        const wxString wildcard = _L("All files") + " (*.*)|*.*";
#endif
        wxFileDialog dialog(this, _L("Choose the external editor executable"), wxEmptyString, wxEmptyString, wildcard, wxFD_OPEN | wxFD_FILE_MUST_EXIST);

        if (dialog.ShowModal() == wxID_OK) {
            wxString editor_path = dialog.GetPath();
            app_config->set(param, std::string(editor_path.ToUTF8().data()));
            app_config->save();
            m_staticTextPath->GetTextCtrl()->SetValue(editor_path);
            item_panel->Layout();
        }
        });

    sizer->Add(m_staticTextTitle, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(8)));
    sizer->Add(m_staticTextPath, wxSizerFlags().CenterVertical().Proportion(1).Border(wxRIGHT, FromDIP(8)));
    sizer->Add(m_button_browse, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(ITEM_RIGHT_PADDING)));

    item_panel->SetSizer(sizer);
    item_panel->Layout();

    return item_panel;
}

wxSizer *PreferencesDialog::create_item_radiobox(wxString title, wxWindow *parent, wxString tooltip, int padding_left, int groupid, std::string param)
{
    RadioBox *radiobox                      = new RadioBox(parent);
    m_radiobox_list[m_radiobox_list.size()] = radiobox;
    radiobox->Bind(wxEVT_LEFT_DOWN, &PreferencesDialog::OnSelectRadio, this);

    RadioSelector *rs = new RadioSelector;
    rs->m_groupid     = groupid;
    rs->m_param_name  = param;
    rs->m_radiobox    = radiobox;
    rs->m_selected    = false;
    m_radio_group.Append(rs);

    wxStaticText *text = new wxStaticText(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize);

    radiobox->SetToolTip(tooltip);
    text->SetToolTip(tooltip);

    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    sizer->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));
    sizer->Add(text, wxSizerFlags().CenterVertical().Proportion(1));
    sizer->Add(radiobox, wxSizerFlags().CenterVertical().Border(wxRIGHT, ITEM_RIGHT_PADDING));
    return sizer;
}

PreferencesDialog::PreferencesDialog(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint &pos, const wxSize &size, long style)
    : DPIDialog(parent, id, _L("Preferences"), pos, size, style)
{
    // Root dialog surface (kit Settings root = Surface); resolves by role in dark.
    SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    SetSize(wxSize(780, 580));
    m_original_use_12h_time_format = wxGetApp().app_config->get("use_12h_time_format");
    // Sync the MD3 density/accent token state to the persisted Appearance choices
    // before the tabs are built so this dialog and later-constructed surfaces
    // resolve the saved density metrics and accent roles.
    apply_persisted_md3_appearance();
    create();
    wxGetApp().UpdateDlgDarkUI(this);
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& event) {
        try {
            NetworkAgent* agent = GUI::wxGetApp().getAgent();
            if (agent) {
                json j;
                std::string value;
                value = wxGetApp().app_config->get("auto_calculate_flush");
                j["auto_flushing"] = value;
                agent->track_event("preferences_changed", j.dump());
            }
        } catch(...) {}

        // Check if time format changed
        std::string current_use_12h_time_format = wxGetApp().app_config->get("use_12h_time_format");
        m_use_12h_time_format_changed = (m_original_use_12h_time_format != current_use_12h_time_format);

        event.Skip();
        });
}

//  PrefNavItem — one MD3 NavItem pill (kit navigation/NavItem): a 44px-tall
//  stadium (r = h/2) with a 20px leading Material Symbol and a label. Selected =
//  SecondaryContainer fill + OnSecondaryContainer 600; hover = SurfaceContainerHigh;
//  idle = transparent + OnSurfaceVariant 400. Fully custom-drawn so the glyph and
//  label share one role colour and the pill re-themes/re-DPIs live. Capability-
//  gated: with no Material Symbols face the label still renders (no leading glyph).
class PrefNavItem : public wxWindow
{
public:
    PrefNavItem(wxWindow *parent, uint32_t glyph, const wxString &label, std::function<void()> on_click)
        : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS),
          m_glyph(glyph), m_label(label), m_on_click(std::move(on_click))
    {
        SetBackgroundColour(StaticBox::GetParentBackgroundColor(parent));
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetMinSize(wxSize(-1, FromDIP(44)));
        // Accessible name so screen readers announce the nav pill by its section label.
        SetName(label);
        Bind(wxEVT_PAINT, &PrefNavItem::OnPaint, this);
        Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &) { SetFocus(); if (m_on_click) m_on_click(); });
        Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent &e) { m_hover = true; SetCursor(wxCURSOR_HAND); Refresh(); e.Skip(); });
        Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent &e) { m_hover = false; Refresh(); e.Skip(); });
        // Keyboard: focusable pill activates on Enter/Space (MD3 nav-item contract).
        Bind(wxEVT_SET_FOCUS,  [this](wxFocusEvent &e) { m_focused = true;  Refresh(); e.Skip(); });
        Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &e) { m_focused = false; Refresh(); e.Skip(); });
        Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent &e) {
            const int k = e.GetKeyCode();
            if (k == WXK_RETURN || k == WXK_NUMPAD_ENTER || k == WXK_SPACE) {
                if (m_on_click) m_on_click();
            } else {
                e.Skip();
            }
        });
    }

    void SetSelected(bool s) { if (m_selected == s) return; m_selected = s; Refresh(); }

    bool AcceptsFocus() const override { return true; }
    bool AcceptsFocusFromKeyboard() const override { return IsShown() && IsThisEnabled(); }

private:
    void OnPaint(wxPaintEvent &)
    {
        wxPaintDC pdc(this);
        const wxSize sz = GetSize();
        pdc.SetBackground(wxBrush(GetBackgroundColour()));
        pdc.Clear();
#ifdef __WXMSW__
        wxGCDC dc(pdc);
#else
        wxDC &dc = pdc;
#endif
        // Pill background: selected -> SecondaryContainer, hover -> SurfaceContainerHigh.
        wxColour pill;
        bool     draw_pill = false;
        if (m_selected) { pill = StateColor::semantic(MD3::Role::SecondaryContainer); draw_pill = true; }
        else if (m_hover) { pill = StateColor::semantic(MD3::Role::SurfaceContainerHigh); draw_pill = true; }
        if (draw_pill) {
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(pill));
            dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, sz.y / 2.0); // pill r = height/2
        }

        const wxColour fg = m_selected ? StateColor::semantic(MD3::Role::OnSecondaryContainer)
                                       : StateColor::semantic(MD3::Role::OnSurfaceVariant);
        const int pad_l   = FromDIP(14);
        const int gap     = FromDIP(10);
        const int icon_px = 20; // logical px; the icon font scales with the DC

        dc.SetFont(m_selected ? ::Label::Head_13 : ::Label::Body_13);
        dc.SetTextForeground(fg);
        wxCoord tw = 0, th = 0;
        dc.GetTextExtent(m_label, &tw, &th);

        wxSize is(0, 0);
        const bool has_icon = m_glyph && MaterialIcon::available();
        if (has_icon) is = MaterialIcon::measure(dc, m_glyph, icon_px);

        const int content_h = std::max<int>(th, is.y);
        const int y0        = (sz.y - content_h) / 2;
        int       x         = pad_l;
        if (has_icon) {
            const int iy = y0 + (content_h - is.y) / 2;
            MaterialIcon::draw(dc, m_glyph, icon_px, fg, wxPoint(x, iy));
            x += is.x + gap;
        }
        const int ty = y0 + (content_h - th) / 2;
        dc.DrawText(m_label, x, ty);

        // Keyboard focus ring: a Primary stadium outline inset inside the pill so
        // it stays visible over both the selected fill and the idle background.
        if (m_focused) {
            const int inset = FromDIP(2);
            const int pw    = std::max(1, FromDIP(2));
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.SetPen(wxPen(StateColor::semantic(MD3::Role::Primary), pw));
            dc.DrawRoundedRectangle(inset, inset, sz.x - 2 * inset, sz.y - 2 * inset,
                                    (sz.y - 2 * inset) / 2.0);
        }
    }

    uint32_t              m_glyph;
    wxString              m_label;
    std::function<void()> m_on_click;
    bool                  m_selected = false;
    bool                  m_hover    = false;
    bool                  m_focused  = false;
};

//  PreferenceTabbar — the fixed 230px vertical NavRail (kit Settings.jsx): a
//  SurfaceContainerLow strip with a 1px OutlineVariant right edge, 16/10 padding
//  and a 2px gap between NavItem pills. Emits the standard wxEVT_CHOICE
//  (int = selected index) when the user picks a section — the same contract the
//  legacy horizontal tab-bar exposed, so create()'s wiring is unchanged.
class PreferenceTabbar : public wxWindow
{
public:
    PreferenceTabbar(wxWindow *parent);
    void AddTab(const wxString &label, uint32_t glyph);
    void SetSelection(int sel);
    int  GetSelection() const { return m_selection; }
    void Rescale();

private:
    std::vector<PrefNavItem *> m_items;
    wxBoxSizer                *m_itemsV   = nullptr;
    int                        m_selection = -1;
};

PreferenceTabbar::PreferenceTabbar(wxWindow *parent) : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
{
    // Nav-rail surface (kit Settings nav = SurfaceContainerLow), one container
    // step off the Surface content pane; resolves by role in dark.
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));

    auto *outer = new wxBoxSizer(wxHORIZONTAL);
    m_itemsV    = new wxBoxSizer(wxVERTICAL);
    // 16px top/bottom, 10px left/right padding around the pill stack.
    outer->Add(m_itemsV, 1, wxEXPAND | wxTOP | wxBOTTOM | wxLEFT | wxRIGHT, FromDIP(10));
    // 1px OutlineVariant right edge separating the rail from the content pane.
    auto *line = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(1), -1));
    line->SetBackgroundColour(StateColor::semantic(MD3::Role::OutlineVariant));
    outer->Add(line, 0, wxEXPAND);
    SetSizer(outer);
    SetMinSize(wxSize(FromDIP(MD3::Metrics::settings_nav_width), -1));
}

void PreferenceTabbar::AddTab(const wxString &label, uint32_t glyph)
{
    const int index = (int) m_items.size();
    auto     *item  = new PrefNavItem(this, glyph, label, [this, index]() {
        SetSelection(index);
        wxCommandEvent evt(wxEVT_CHOICE, GetId());
        evt.SetEventObject(this);
        evt.SetInt(index);
        wxPostEvent(this, evt);
    });
    m_items.push_back(item);
    m_itemsV->Add(item, 0, wxEXPAND | wxBOTTOM, FromDIP(2)); // 2px inter-item gap
    if (m_selection < 0) SetSelection(0);
    Layout();
}

void PreferenceTabbar::SetSelection(int sel)
{
    if (sel < 0 || sel >= (int) m_items.size()) return;
    m_selection = sel;
    for (int i = 0; i < (int) m_items.size(); ++i)
        m_items[i]->SetSelected(i == m_selection);
}

void PreferenceTabbar::Rescale()
{
    for (auto *item : m_items) {
        item->SetMinSize(wxSize(-1, FromDIP(44)));
        item->Refresh();
    }
    SetMinSize(wxSize(FromDIP(MD3::Metrics::settings_nav_width), -1));
    Layout();
    Refresh();
}

void PreferencesDialog::create()
{
    app_config             = get_app_config();

    // backup switch has two option in the old versions:
    // 1. switch to turn on/off
    // 2. interval
    // in the new verison we use 0 for `not backup`
    if (app_config->get("backup_switch") != "true") { app_config->set("backup_interval", "0"); }
    m_backup_interval_time = app_config->get("backup_interval");

    // set icon for dialog
    std::string icon_path = (boost::format("%1%/images/BambuStudioTitle.ico") % resources_dir()).str();
    SetIcon(wxIcon(encode_path(icon_path.c_str()), wxBITMAP_TYPE_ICO));
    SetSizeHints(wxDefaultSize, wxDefaultSize);

    auto main_sizer = new wxBoxSizer(wxVERTICAL);

    m_tabbar = new PreferenceTabbar(this);
    m_book   = new wxSimplebook(this, wxID_ANY);

    // Right-hand content pane: a top MD3 SearchField pill over the section book.
    auto *content_pane = new wxBoxSizer(wxVERTICAL);
    m_search = new SearchField(this, _L("Search settings"));
    content_pane->Add(m_search, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(16));
    // Inline "no results" hint under the search pill; hidden until an active
    // query matches nothing (see apply_search_filter).
    m_search_empty_hint = new wxStaticText(this, wxID_ANY, _L("No settings match your search."));
    m_search_empty_hint->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    m_search_empty_hint->SetFont(::Label::Body_13);
    m_search_empty_hint->Hide();
    content_pane->Add(m_search_empty_hint, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(16));
    content_pane->Add(m_book, 1, wxEXPAND | wxTOP, FromDIP(12));

    // Section rail (left) + content pane (right).
    auto *body_row = new wxBoxSizer(wxHORIZONTAL);
    body_row->Add(m_tabbar, 0, wxEXPAND);
    body_row->Add(content_pane, 1, wxEXPAND);

    auto add_tab = [this](const wxString &label, uint32_t glyph, wxWindow *page) {
        m_tabbar->AddTab(label, glyph);
        m_book->AddPage(page, label);
    };
    // Sections map to MD3 NavItem glyphs (kit Settings.jsx). "Appearance" is the
    // new theme/density/accent section; a person glyph for "User" is not yet in
    // the MaterialIcon set (falls back to Sync — see followups).
    add_tab(_L("Appearance"), MaterialIcon::Palette, create_appearance_tab());
    add_tab(_CTX(L_CONTEXT("General", "Preference"), "Preference"), MaterialIcon::Settings, create_general_tab());
    add_tab(_CTX(L_CONTEXT("User", "Preference"), "Preference"), MaterialIcon::Sync, create_user_tab());
    add_tab(_CTX(L_CONTEXT("3D", "Preference"), "Preference"), MaterialIcon::ViewInAr, create_3d_tab());
    add_tab(_CTX(L_CONTEXT("Other", "Preference"), "Preference"), MaterialIcon::Tune, create_other_tab());

#if !BBL_RELEASE_TO_PUBLIC
    add_tab(_L("Developer Tools"), MaterialIcon::Build, create_developer_tab());
#endif

    m_tabbar->SetSelection(0);
    m_book->SetSelection(0);
    m_tabbar->Bind(wxEVT_CHOICE, [this](wxCommandEvent &e) { m_book->SetSelection(e.GetInt()); });

    // Wire the search pill to live row filtering across every section. The row
    // index is built once here, after all pages and their gates (e.g. the
    // model-mall visibility toggle) have settled.
    build_search_index();
    m_search->SetOnQuery([this](const wxString &query) { apply_search_filter(query); });
    // Re-run the live filter when the regex / case / whole-word toggle changes so
    // the visible rows reflect the new matching mode without a fresh keystroke.
    // Passing the raw field value keeps the empty-query reset path intact.
    m_search->SetOnRegexToggle([this](bool) { apply_search_filter(m_search->GetValue()); });

    main_sizer->Add(body_row, 1, wxEXPAND);
    main_sizer->Add(create_bottom_buttons(), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

    SetSizer(main_sizer);
    Layout();
    Fit();

    // Fixed dialog size (~780x640). The nav-rail + content-pane layout widens the
    // dialog vs. the old horizontal tab bar so the 230px rail leaves a comfortable
    // content column. Tabs are scrollable, so a tab taller than this simply
    // scrolls; cap the height to the screen so it still fits on small displays.
    int screen_height = std::numeric_limits<int>::max();
    int count = wxDisplay::GetCount();
    for (int i = 0; i < count; ++i) {
        wxDisplay display(i);
        wxRect rect = display.GetGeometry();
        screen_height  = std::min(screen_height, rect.GetHeight());
    }
    if (screen_height == std::numeric_limits<int>::max()) screen_height = wxGetDisplaySize().GetY();

    const int max_height = int(screen_height * 0.7); // never exceed most of the screen
    this->SetSize(FromDIP(780), std::min(FromDIP(640), max_height));

    CenterOnParent();
    wxPoint start_pos = this->GetPosition();
    if (start_pos.y < 0) { this->SetPosition(wxPoint(start_pos.x, 0)); }
}

// ============================================================================
//  Live settings search — SearchField-driven row filtering across all pages.
// ============================================================================

// Collect every wxStaticText descendant of a window (row labels, descriptions,
// link labels — ::Label derives from wxStaticText) for the search index.
static void collect_search_labels_from_window(wxWindow *win, std::vector<wxStaticText *> &labels)
{
    if (win == nullptr) return;
    if (auto *text = dynamic_cast<wxStaticText *>(win)) labels.push_back(text);
    for (auto *child : win->GetChildren()) collect_search_labels_from_window(child, labels);
}

static void collect_search_labels_from_sizer(wxSizer *sizer, std::vector<wxStaticText *> &labels)
{
    if (sizer == nullptr) return;
    for (auto *item : sizer->GetChildren()) {
        if (item->IsWindow()) collect_search_labels_from_window(item->GetWindow(), labels);
        else if (item->IsSizer()) collect_search_labels_from_sizer(item->GetSizer(), labels);
    }
}

// Normalized (wrap-break-free) label text for matching. wxStaticText::Wrap
// rewrites the label with '\n' at the break points; fold those back to spaces
// so multi-word queries match across a wrapped line.
static wxString search_label_text(const wxStaticText *label)
{
    wxString text = label->GetLabelText();
    text.Replace("\n", " ");
    return text;
}

void PreferencesDialog::build_search_index()
{
    m_search_rows.clear();
    if (m_book == nullptr) return;

    for (size_t page = 0; page < m_book->GetPageCount(); ++page) {
        wxWindow *page_win = m_book->GetPage(page);
        wxSizer  *sizer    = page_win ? page_win->GetSizer() : nullptr;
        if (sizer == nullptr) continue;

        for (auto *item : sizer->GetChildren()) {
            if (item == nullptr || item->IsSpacer()) continue; // inter-row spacers stay put

            SearchRow row;
            row.page           = int(page);
            row.item           = item;
            row.baseline_shown = item->IsShown();
            if (item->IsWindow()) collect_search_labels_from_window(item->GetWindow(), row.labels);
            else if (item->IsSizer()) collect_search_labels_from_sizer(item->GetSizer(), row.labels);

            wxString haystack;
            for (auto *label : row.labels) {
                if (!haystack.empty()) haystack << ' ';
                haystack << search_label_text(label);
            }
            row.haystack = haystack; // original case; the matcher folds case itself
            // Section headers are the Head_16 titles from create_item_title().
            row.is_title = !row.labels.empty() && row.labels.front()->GetFont() == ::Label::Head_16;
            m_search_rows.push_back(std::move(row));
        }
    }
}

void PreferencesDialog::clear_search_highlights()
{
    for (auto &entry : m_search_saved_colours) {
        if (entry.first == nullptr) continue;
        entry.first->SetForegroundColour(entry.second);
        entry.first->Refresh();
    }
    m_search_saved_colours.clear();
}

void PreferencesDialog::scroll_search_row_into_view(const SearchRow &row)
{
    auto *scrolled = dynamic_cast<wxScrolledWindow *>(m_book->GetPage(row.page));
    if (scrolled == nullptr) return;

    wxWindow *anchor = nullptr;
    if (!row.labels.empty()) anchor = row.labels.front();
    else if (row.item && row.item->IsWindow()) anchor = row.item->GetWindow();
    if (anchor == nullptr) return;

    // Anchor position relative to the scrolled page (current view coords) ->
    // virtual coords -> scroll units.
    wxPoint   pos = anchor->GetPosition();
    wxWindow *w   = anchor->GetParent();
    while (w != nullptr && w != scrolled) {
        pos += w->GetPosition();
        w = w->GetParent();
    }
    if (w == nullptr) return; // anchor is not a descendant of the page

    int virtual_x = 0, virtual_y = 0;
    scrolled->CalcUnscrolledPosition(pos.x, pos.y, &virtual_x, &virtual_y);
    int unit_x = 0, unit_y = 0;
    scrolled->GetScrollPixelsPerUnit(&unit_x, &unit_y);
    if (unit_y <= 0) return;
    scrolled->Scroll(-1, std::max(0, virtual_y - FromDIP(8)) / unit_y);
}

void PreferencesDialog::apply_search_filter(const wxString &raw_query)
{
    wxString query = raw_query;
    query.Trim(true).Trim(false);
    if (query.empty()) {
        reset_search_filter();
        return;
    }

    m_search_active     = true;
    m_search_last_query = query;
    clear_search_highlights();

    const bool     regex     = m_search && m_search->IsRegexEnabled();
    const bool     case_sens = m_search && m_search->IsCaseSensitive();
    const bool     whole     = m_search && m_search->IsWholeWord();
    const wxColour highlight = StateColor::semantic(MD3::Role::Primary);
    const size_t   count     = m_search_rows.size();

    // Pass 1: per-row match against the label haystack. Baseline-hidden rows
    // (e.g. model-mall entries without a mall) never participate. Matching runs
    // through the shared SearchField matcher so the regex / case / whole-word
    // toggles apply here exactly as on every other search surface; with regex
    // off and case-sensitivity off this is the same case-insensitive substring
    // test as before.
    std::vector<bool> matched(count, false);
    for (size_t i = 0; i < count; ++i) {
        const SearchRow &row = m_search_rows[i];
        if (!row.baseline_shown || row.haystack.empty()) continue;
        matched[i] = SearchField::textMatches(query, row.haystack, regex, case_sens, whole);
    }

    // Pass 2: group visibility. A group is a Head_16 title row plus the rows
    // that follow it on the same page (up to the next title). A matching title
    // keeps its whole group visible for context; otherwise the title stays
    // only when at least one of its rows matches, and only those rows remain.
    size_t i = 0;
    while (i < count) {
        const int page  = m_search_rows[i].page;
        size_t    title = count;
        size_t    start = i;
        if (m_search_rows[i].is_title) { title = i; start = i + 1; }
        size_t end = start;
        while (end < count && m_search_rows[end].page == page && !m_search_rows[end].is_title) ++end;

        const bool title_matched   = title < count && matched[title];
        bool       any_row_matched = false;
        for (size_t j = start; j < end; ++j)
            if (matched[j]) any_row_matched = true;

        if (title < count)
            m_search_rows[title].item->Show(m_search_rows[title].baseline_shown && (title_matched || any_row_matched));
        for (size_t j = start; j < end; ++j)
            m_search_rows[j].item->Show(m_search_rows[j].baseline_shown && (title_matched || matched[j]));

        i = end;
    }

    // Highlight the matched labels (Primary tint); the pre-highlight colour is
    // saved so an emptied query restores the exact original foreground.
    auto tint = [this, &highlight](wxStaticText *label) {
        if (label == nullptr) return;
        if (m_search_saved_colours.find(label) == m_search_saved_colours.end())
            m_search_saved_colours.emplace(label, label->GetForegroundColour());
        label->SetForegroundColour(highlight);
        label->Refresh();
    };
    for (size_t k = 0; k < count; ++k) {
        if (!matched[k]) continue;
        const SearchRow &row      = m_search_rows[k];
        bool             any_tint = false;
        for (auto *label : row.labels) {
            if (SearchField::textMatches(query, search_label_text(label), regex, case_sens, whole)) {
                tint(label);
                any_tint = true;
            }
        }
        if (!any_tint && !row.labels.empty()) tint(row.labels.front()); // matched across labels
    }

    // Relayout every page for the new row visibility.
    for (size_t p = 0; p < m_book->GetPageCount(); ++p) {
        wxWindow *page_win = m_book->GetPage(p);
        if (page_win == nullptr) continue;
        page_win->Layout();
        if (auto *scrolled = dynamic_cast<wxScrolledWindow *>(page_win)) scrolled->FitInside();
    }

    // First match in page order; prefer the section already on screen so
    // typing does not yank the user away from a page that also matches.
    size_t first_match = count;
    for (size_t k = 0; k < count; ++k)
        if (matched[k]) { first_match = k; break; }
    size_t    nav_match    = count;
    const int current_page = m_book->GetSelection();
    for (size_t k = 0; k < count; ++k)
        if (matched[k] && m_search_rows[k].page == current_page) { nav_match = k; break; }
    if (nav_match == count) nav_match = first_match;

    const bool has_match = first_match < count;
    if (m_search_empty_hint) m_search_empty_hint->Show(!has_match);
    Layout();

    if (has_match) {
        const SearchRow &target = m_search_rows[nav_match];
        if (m_book->GetSelection() != target.page) {
            m_book->SetSelection(target.page);
            m_tabbar->SetSelection(target.page);
        }
        // Re-layout the now-visible page before measuring the anchor position.
        if (wxWindow *page_win = m_book->GetPage(target.page)) page_win->Layout();
        scroll_search_row_into_view(target);
    }
}

void PreferencesDialog::reset_search_filter()
{
    if (!m_search_active) return;
    m_search_active = false;
    m_search_last_query.clear();

    clear_search_highlights();
    for (auto &row : m_search_rows)
        if (row.item) row.item->Show(row.baseline_shown);
    if (m_search_empty_hint) m_search_empty_hint->Hide();

    for (size_t p = 0; p < m_book->GetPageCount(); ++p) {
        wxWindow *page_win = m_book->GetPage(p);
        if (page_win == nullptr) continue;
        page_win->Layout();
        if (auto *scrolled = dynamic_cast<wxScrolledWindow *>(page_win)) {
            scrolled->FitInside();
            scrolled->Scroll(0, 0);
        }
    }
    Layout();
}

PreferencesDialog::~PreferencesDialog()
{
    m_radio_group.DeleteContents(true);
    m_hash_selector.clear();
}

void PreferencesDialog::on_dpi_changed(const wxRect &suggested_rect) {
    for (auto item : m_button_list) {
        item.second->Rescale();
        item.second->SetMinSize(wxSize(FromDIP(BTN_WIDTH), FromDIP(BTN_HEIGHT)));
        item.second->SetCornerRadius(FromDIP(12));
    }
    for (auto item : m_checkbox_list) {
        item.second->Rescale();
    }
    for (auto item : m_radiobox_list) {
        item.second->Rescale();
    }
    for (auto item : m_combobox_list) {
        item.second->Rescale();
    }
    for (auto *seg : m_segmented_list) {
        if (seg) seg->Rescale();
    }
    if (m_search) m_search->Rescale();
    if (m_tabbar) m_tabbar->Rescale();
    this->Refresh();
    Layout();
    Fit();
    int displayIndex = wxDisplay::GetFromWindow(this);
    if (displayIndex != wxNOT_FOUND) {
        wxDisplay display(displayIndex);
        wxRect    screenRect = display.GetGeometry();
        if (m_screen_height != screenRect.GetHeight()) {
            m_screen_height = screenRect.GetHeight();
            // Keep the fixed size (capped to the screen) on a DPI/monitor switch
            // instead of stretching to a fraction of the screen.
            this->SetSize(FromDIP(780), std::min(FromDIP(640), int(m_screen_height * 0.7)));
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << " The display screen has switched";
        }
    }
}

void PreferencesDialog::Split(const std::string &src, const std::string &separator, std::vector<wxString> &dest)
{
    std::string            str = src;
    std::string            substring;
    std::string::size_type start = 0, index;
    dest.clear();
    index = str.find_first_of(separator, start);
    do {
        if (index != std::string::npos) {
            substring = str.substr(start, index - start);
            dest.push_back(substring);
            start = index + separator.size();
            index = str.find(separator, start);
            if (start == std::string::npos) break;
        }
    } while (index != std::string::npos);

    substring = str.substr(start);
    dest.push_back(substring);
}

//  AccentSwatch — a 32px filled circle (kit Settings.jsx accent row). Selected =
//  a 2px OnSurface ring + a white check glyph. The fill is a user-chosen accent
//  seed (data colour, exempt), custom-drawn so the ring/check re-theme by role.
class AccentSwatch : public wxWindow
{
public:
    AccentSwatch(wxWindow *parent, const wxColour &color, bool selected, std::function<void()> on_click)
        : wxWindow(parent, wxID_ANY), m_color(color), m_selected(selected), m_on_click(std::move(on_click))
    {
        SetBackgroundColour(StaticBox::GetParentBackgroundColor(parent));
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetMinSize(wxSize(FromDIP(32), FromDIP(32)));
        SetCursor(wxCURSOR_HAND);
        Bind(wxEVT_PAINT, &AccentSwatch::OnPaint, this);
        Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &) { if (m_on_click) m_on_click(); });
    }

    void SetSelected(bool s) { if (m_selected == s) return; m_selected = s; Refresh(); }

    const wxColour &colour() const { return m_color; }

private:
    void OnPaint(wxPaintEvent &)
    {
        wxPaintDC pdc(this);
        pdc.SetBackground(wxBrush(GetBackgroundColour()));
        pdc.Clear();
#ifdef __WXMSW__
        wxGCDC dc(pdc);
#else
        wxDC &dc = pdc;
#endif
        const wxSize sz = GetSize();
        const int    cx = sz.x / 2, cy = sz.y / 2;
        const int    r  = std::min(sz.x, sz.y) / 2 - FromDIP(2);
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(m_color)); // accent seed = data colour
        dc.DrawCircle(cx, cy, r);
        if (m_selected) {
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.SetPen(wxPen(StateColor::semantic(MD3::Role::OnSurface), FromDIP(2)));
            dc.DrawCircle(cx, cy, r);
            if (MaterialIcon::available())
                MaterialIcon::drawCentered(dc, MaterialIcon::Check, 16, wxColour(255, 255, 255), wxRect(0, 0, sz.x, sz.y));
        }
    }

    wxColour              m_color;
    bool                  m_selected;
    std::function<void()> m_on_click;
};

//  MD3AppearancePreview — a small always-live sample of the active MD3 tokens:
//  a Surface card with an Outline hairline ring, a Primary pill with an
//  OnPrimary "Sample" label, and a SecondaryContainer chip. Every paint
//  re-resolves its colours through StateColor::semantic() and re-reads the
//  active density metrics, so the accent/density handlers' fan-out (which
//  Refresh()es the whole dialog via refresh_md3_appearance) repaints it with
//  the freshly-recomputed roles — no per-change hook needed.
class MD3AppearancePreview : public wxPanel
{
public:
    explicit MD3AppearancePreview(wxWindow *parent) : wxPanel(parent, wxID_ANY)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetMinSize(wxSize(-1, FromDIP(72)));
        Bind(wxEVT_PAINT, &MD3AppearancePreview::OnPaint, this);
    }

private:
    void OnPaint(wxPaintEvent &)
    {
        wxPaintDC pdc(this);
        pdc.SetBackground(wxBrush(StateColor::semantic(MD3::Role::Surface)));
        pdc.Clear();
#ifdef __WXMSW__
        wxGCDC dc(pdc);
#else
        wxDC &dc = pdc;
#endif
        const MD3::DensityMetrics &metrics      = MD3::Metrics::active();
        const int                  radius       = FromDIP(metrics.radius);
        const int                  small_radius = FromDIP(metrics.small_radius);
        const int                  padding      = FromDIP(metrics.padding);

        // Surface card + Outline hairline ring (the density radius shows live).
        wxRect card(GetClientSize());
        card.Deflate(FromDIP(1), FromDIP(1));
        dc.SetPen(wxPen(StateColor::semantic(MD3::Role::Outline), FromDIP(1)));
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::Surface)));
        dc.DrawRoundedRectangle(card, radius);

        dc.SetFont(::Label::Body_13); // rebuilt by the font controls; read fresh

        // Primary pill with an OnPrimary sample label.
        const wxString pill_text   = _L("Sample");
        const wxSize   pill_extent = dc.GetTextExtent(pill_text);
        const int      pill_h      = pill_extent.y + FromDIP(12);
        const int      pill_w      = pill_extent.x + FromDIP(28);
        const int      pill_x      = card.x + padding;
        const int      pill_y      = card.y + (card.height - pill_h) / 2;
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::Primary)));
        dc.DrawRoundedRectangle(pill_x, pill_y, pill_w, pill_h, MD3::Metrics::pill_radius(pill_h));
        dc.SetTextForeground(StateColor::semantic(MD3::Role::OnPrimary));
        dc.DrawText(pill_text, pill_x + (pill_w - pill_extent.x) / 2, pill_y + (pill_h - pill_extent.y) / 2);

        // SecondaryContainer chip (small radius follows density too).
        const wxString chip_text   = wxString::FromUTF8("Aa");
        const wxSize   chip_extent = dc.GetTextExtent(chip_text);
        const int      chip_h      = chip_extent.y + FromDIP(10);
        const int      chip_w      = chip_extent.x + FromDIP(24);
        const int      chip_x      = pill_x + pill_w + FromDIP(12);
        const int      chip_y      = card.y + (card.height - chip_h) / 2;
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::SecondaryContainer)));
        dc.DrawRoundedRectangle(chip_x, chip_y, chip_w, chip_h, small_radius);
        dc.SetTextForeground(StateColor::semantic(MD3::Role::OnSecondaryContainer));
        dc.DrawText(chip_text, chip_x + (chip_w - chip_extent.x) / 2, chip_y + (chip_h - chip_extent.y) / 2);
    }
};

wxWindow *PreferencesDialog::create_appearance_tab()
{
    auto        scrolled = new ScrollPanel(m_book);
    wxBoxSizer *sizer    = new wxBoxSizer(wxVERTICAL);

    auto title = create_item_title(_L("Appearance"), scrolled, _L("Appearance"));

    // Fixed-label + control row (kit Settings.jsx: 150px label + segmented).
    auto make_row = [this, scrolled](const wxString &label, wxWindow *control) -> wxBoxSizer * {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        row->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
        auto *lbl = new wxStaticText(scrolled, wxID_ANY, label, wxDefaultPosition, wxSize(FromDIP(150), -1), 0);
        lbl->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
        lbl->SetFont(::Label::Body_13);
        row->Add(lbl, wxSizerFlags().CenterVertical());
        row->Add(control, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(ITEM_RIGHT_PADDING));
        return row;
    };

    // Theme: light / dark SegmentedControl bound to dark_color_mode (cross-platform).
    auto *theme = new MultiSwitchButton(scrolled);
    m_segmented_list.push_back(theme);
    theme->SetOptions({_L("Light"), _L("Dark")});
    theme->SetMinSize(wxSize(FromDIP(200), FromDIP(30)));
    const bool is_dark = app_config->get("dark_color_mode") == "1";
    theme->SetSelection(is_dark ? 1 : 0); // set before Bind so init does not re-fire the handler
    theme->Bind(wxCUSTOMEVT_MULTISWITCH_SELECTION, [this](wxCommandEvent &e) {
        const bool dark = e.GetInt() == 1;
        app_config->set("dark_color_mode", dark ? "1" : "0");
        app_config->save();
        // An active search tints matched labels and remembers their pre-tint
        // foregrounds. Restore those before the theme fan-out recolours the
        // tree, then re-run the query afterwards so the highlights — and the
        // saved-colour map — capture the new theme's colours instead of stale
        // pre-toggle ones (which a later cleared query would otherwise restore).
        const bool     search_was_active = m_search_active;
        const wxString saved_query       = m_search_last_query;
        if (search_was_active) clear_search_highlights();
        apply_dark_mode(dark);
        if (search_was_active) apply_search_filter(saved_query);
        e.Skip();
    });

    // Reentrancy guard shared by the appearance controls: MultiSwitchButton::
    // SetSelection DOES emit wxCUSTOMEVT_MULTISWITCH_SELECTION (SwitchButton.cpp
    // send_selection_event), so the reset button's programmatic re-selection
    // would re-fire these handlers mid-reset. The guard makes reset the single
    // writer: handlers only persist/apply on genuine user interaction.
    auto resetting = std::make_shared<bool>(false);

    // Density: comfortable / compact SegmentedControl. Persists ui_density and
    // drives the MD3 runtime density state (MD3::Metrics::setDensity) so later-
    // built surfaces reflect the choice; a live re-theme refresh follows.
    auto *density = new MultiSwitchButton(scrolled);
    m_segmented_list.push_back(density);
    density->SetOptions({_L("Comfortable"), _L("Compact")});
    density->SetMinSize(wxSize(FromDIP(220), FromDIP(30)));
    const std::string density_val = app_config->get("ui_density");
    density->SetSelection(density_val == "compact" ? 1 : 0);
    density->Bind(wxCUSTOMEVT_MULTISWITCH_SELECTION, [this, resetting](wxCommandEvent &e) {
        if (*resetting) { e.Skip(); return; }
        const bool compact = e.GetInt() == 1;
        app_config->set("ui_density", compact ? "compact" : "comfortable");
        app_config->save();
        MD3::Metrics::setDensity(compact ? MD3::Metrics::Density::Compact : MD3::Metrics::Density::Comfortable);
        refresh_md3_appearance(this);
        e.Skip();
    });

    // Accent: swatch row. Persists ui_accent_seed and applies the seed to the MD3
    // accent roles at runtime (MD3::setAccentSeed recomputes Primary/*Container
    // for light+dark); a live re-theme refresh then repaints the UI.
    const std::vector<std::pair<wxString, wxString>> seeds = {
        {"#146c2e", _L("Green")}, {"#7c5cff", _L("Purple")}, {"#14b8a6", _L("Teal")},
        {"#2563eb", _L("Blue")},  {"#d81b60", _L("Pink")},   {"#ea580c", _L("Orange")},
    };
    std::string cur_seed = app_config->get("ui_accent_seed");
    if (cur_seed.empty()) cur_seed = "#146c2e";

    auto *accent_row = new wxBoxSizer(wxHORIZONTAL);
    auto  swatches   = std::make_shared<std::vector<AccentSwatch *>>();
    for (size_t i = 0; i < seeds.size(); ++i) {
        const std::string hex = seeds[i].first.ToStdString();
        const bool        sel = seeds[i].first.IsSameAs(wxString::FromUTF8(cur_seed), false);
        auto             *sw  = new AccentSwatch(scrolled, wxColour(seeds[i].first), sel, [this, hex, swatches, i]() {
            app_config->set("ui_accent_seed", hex);
            app_config->save();
            MD3::setAccentSeed(wxColour(wxString::FromUTF8(hex)));
            for (size_t j = 0; j < swatches->size(); ++j)
                (*swatches)[j]->SetSelected(j == i);
            refresh_md3_appearance(this);
        });
        sw->SetToolTip(seeds[i].second);
        swatches->push_back(sw);
        accent_row->Add(sw, 0, wxRIGHT, FromDIP(10));
    }

    // Custom accent: a wxColourDialog seeded from the persisted seed. Any colour
    // is a valid seed (the six swatches are just shortcuts into the same
    // MD3::setAccentSeed pipeline), so a pick persists + applies exactly like a
    // preset; the preset rings deselect since none of them is the active seed.
    auto *accent_custom = new Button(scrolled, _L("Custom") + dots);
    m_button_list[m_button_list.size()] = accent_custom;
    accent_custom->SetVariant(Button::Variant::Outlined);
    accent_custom->SetButtonSize(Button::Size::Small);
    accent_custom->Bind(wxEVT_BUTTON, [this, swatches](wxCommandEvent &) {
        std::string seed = app_config->get("ui_accent_seed");
        if (seed.empty()) seed = "#146c2e";
        wxColour initial(wxString::FromUTF8(seed));
        if (!initial.IsOk()) initial = wxColour(wxString::FromUTF8("#146c2e"));
        wxColourData data;
        data.SetChooseFull(true);
        data.SetColour(initial);
        wxColourDialog dlg(this, &data);
        if (dlg.ShowModal() != wxID_OK) return;
        const wxColour picked = dlg.GetColourData().GetColour();
        if (!picked.IsOk()) return;
        const wxString picked_hex = picked.GetAsString(wxC2S_HTML_SYNTAX);
        app_config->set("ui_accent_seed", into_u8(picked_hex));
        app_config->save();
        MD3::setAccentSeed(picked);
        // Keep the preset rings truthful: light up the matching swatch when the
        // picked colour IS one of the six seeds (matches the reopen behaviour,
        // which re-derives the selection case-insensitively), else clear all.
        for (size_t j = 0; j < swatches->size(); ++j)
            (*swatches)[j]->SetSelected((*swatches)[j]->colour().GetAsString(wxC2S_HTML_SYNTAX).IsSameAs(picked_hex, false));
        refresh_md3_appearance(this);
    });
    accent_row->Add(accent_custom, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(2));

    // ---- UI font (family + size) --------------------------------------------
    // Live preview label. Its font is re-fetched from the freshly rebuilt static
    // Body_16 after every change so the sample repaints in the chosen family/scale
    // even before the tree-wide re-theme walk runs. (Latin phrase is translatable;
    // the CJK + digits sample is fixed so all three language modes exercise CJK
    // fallback.)
    auto *font_preview = new wxStaticText(
        scrolled, wxID_ANY,
        _L("The quick brown fox") + wxString::FromUTF8(" / \xE4\xB8\xAD\xE6\x96\x87\xE7\xA4\xBA\xE4\xBE\x8B 123"));
    font_preview->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    font_preview->SetFont(::Label::Body_16);

    // Shared apply step: persist has already happened at the call site; here we
    // rebuild the static font table from AppConfig (family+scale) for the current
    // language mode, refresh the preview's own font, then run the exact re-theme
    // walk Density/Accent use so the live UI (and this dialog) repaints.
    auto apply_fonts = [this, font_preview, scrolled]() {
        ::Label::rebuild_fonts(I18N::language_mode_profile().font_language);
        font_preview->SetFont(::Label::Body_16);
        refresh_md3_appearance(this);
        scrolled->Layout();
        scrolled->FitInside();
    };

    // Font family: "Default" (ui_font_family "") + bundled Roboto + the installed
    // system UI families (enumerated, '@'-vertical variants dropped, sorted
    // case-insensitively, deduped, Roboto removed since it is listed explicitly).
    std::vector<wxString>    font_labels = {_L("Default"), wxString::FromUTF8("Roboto")};
    std::vector<std::string> font_values = {"", "Roboto"};
    {
        wxArrayString         faces = wxFontEnumerator::GetFacenames();
        std::vector<wxString> sys;
        for (const wxString &f : faces) {
            if (f.IsEmpty() || f[0] == '@') continue;         // skip vertical CJK variants
            if (f.IsSameAs(wxString::FromUTF8("Roboto"), false)) continue; // already listed
            sys.push_back(f);
        }
        std::sort(sys.begin(), sys.end(), [](const wxString &a, const wxString &b) { return a.CmpNoCase(b) < 0; });
        sys.erase(std::unique(sys.begin(), sys.end(), [](const wxString &a, const wxString &b) { return a.CmpNoCase(b) == 0; }), sys.end());
        for (const wxString &f : sys) {
            font_labels.push_back(f);
            font_values.push_back(into_u8(f));
        }
    }

    const std::string cur_family = app_config->get("ui_font_family");
    int               family_idx = 0;
    for (size_t i = 0; i < font_values.size(); ++i)
        if (font_values[i] == cur_family) { family_idx = (int) i; break; }

    auto *font_combo = new ::ComboBox(scrolled, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(LARGE_COMBOBOX_WIDTH), -1), 0, nullptr, wxCB_READONLY);
    m_combobox_list[m_combobox_list.size()] = font_combo;
    font_combo->SetFont(::Label::Body_13);
    font_combo->GetDropDown().SetFont(::Label::Body_13);
    font_combo->SetCornerRadius(FromDIP(10)); // MD3 SelectField r10
    for (const wxString &l : font_labels) font_combo->Append(l);
    font_combo->SetSelection(family_idx); // set before Bind so init does not re-fire
    font_combo->GetDropDown().Bind(wxEVT_COMBOBOX, [this, font_values, apply_fonts](wxCommandEvent &e) {
        const int idx = e.GetSelection();
        if (idx >= 0 && idx < (int) font_values.size()) {
            app_config->set("ui_font_family", font_values[idx]); // "" = default
            app_config->save();
            apply_fonts();
        }
        e.Skip();
    });

    // Text size: Small / Default / Large SegmentedControl -> ui_font_scale
    // 0.9 / 1.0 / 1.15 (the font factory clamps 0.8..1.4 at read time).
    static const std::vector<double>      kScaleVals = {0.9, 1.0, 1.15};
    static const std::vector<std::string> kScaleStrs = {"0.9", "1.0", "1.15"};
    auto *text_size = new MultiSwitchButton(scrolled);
    m_segmented_list.push_back(text_size);
    text_size->SetOptions({_L("Small"), _L("Default"), _L("Large")});
    text_size->SetMinSize(wxSize(FromDIP(220), FromDIP(30)));
    double cur_scale = 1.0;
    try {
        const std::string s = app_config->get("ui_font_scale");
        if (!s.empty()) cur_scale = std::stod(s);
    } catch (...) { cur_scale = 1.0; }
    int    scale_idx  = 1;
    double scale_best = 1e9;
    for (size_t i = 0; i < kScaleVals.size(); ++i) {
        const double diff = kScaleVals[i] - cur_scale;
        const double d    = diff < 0 ? -diff : diff;
        if (d < scale_best) { scale_best = d; scale_idx = (int) i; }
    }
    text_size->SetSelection(scale_idx); // set before Bind so init does not re-fire
    text_size->Bind(wxCUSTOMEVT_MULTISWITCH_SELECTION, [this, apply_fonts, resetting](wxCommandEvent &e) {
        if (*resetting) { e.Skip(); return; }
        const int idx = e.GetInt();
        if (idx >= 0 && idx < (int) kScaleStrs.size()) {
            app_config->set("ui_font_scale", kScaleStrs[idx]);
            app_config->save();
            apply_fonts();
        }
        e.Skip();
    });

    // Live MD3 token preview: repainted by the same refresh_md3_appearance walk
    // the controls above trigger (its paint handler re-resolves roles/metrics).
    auto *md3_preview = new MD3AppearancePreview(scrolled);

    // Reset appearance to defaults. Theme is intentionally excluded — a light/
    // dark flip mid-reset is disruptive; the reset covers the customized look:
    // density, accent seed, font family and text size. Writes the defaults,
    // re-syncs the MD3 runtime state, then re-selects the controls
    // programmatically under the `resetting` guard: MultiSwitchButton::
    // SetSelection emits its selection event, and the guard keeps those
    // re-fired handlers from re-persisting/re-applying mid-reset, so this
    // handler stays the single writer.
    auto *reset_btn = new Button(scrolled, _L("Reset appearance to defaults"));
    m_button_list[m_button_list.size()] = reset_btn;
    reset_btn->SetVariant(Button::Variant::Outlined);
    reset_btn->SetButtonSize(Button::Size::Small);
    reset_btn->Bind(wxEVT_BUTTON, [this, density, font_combo, text_size, swatches, apply_fonts, resetting](wxCommandEvent &) {
        app_config->set("ui_density", "comfortable");
        app_config->set("ui_accent_seed", "#146c2e");
        app_config->set("ui_font_family", "");
        app_config->set("ui_font_scale", "1.0");
        app_config->save();
        MD3::Metrics::setDensity(MD3::Metrics::Density::Comfortable);
        MD3::setAccentSeed(wxColour(wxString::FromUTF8("#146c2e"))); // Brand seed clears the accent override
        *resetting = true;
        density->SetSelection(0);   // Comfortable
        font_combo->SetSelection(0); // Default family ("")
        text_size->SetSelection(1); // Default scale (1.0)
        *resetting = false;
        for (size_t j = 0; j < swatches->size(); ++j)
            (*swatches)[j]->SetSelected(j == 0); // Green = the Brand seed
        apply_fonts(); // rebuild_fonts + preview re-font + refresh_md3_appearance + relayout
    });

    sizer->Add(title, wxSizerFlags().Expand().Border(wxTOP, FromDIP(24)));
    sizer->AddSpacer(FromDIP(8));
    auto flags = wxSizerFlags().Expand().Border(wxTOP, FromDIP(8));
    sizer->Add(make_row(_L("Theme"), theme), flags);
    sizer->Add(make_row(_L("Density"), density), flags);

    // Accent row (label + swatches).
    auto *accent_line = new wxBoxSizer(wxHORIZONTAL);
    accent_line->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    auto *accent_lbl = new wxStaticText(scrolled, wxID_ANY, _L("Accent color"), wxDefaultPosition, wxSize(FromDIP(150), -1), 0);
    accent_lbl->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    accent_lbl->SetFont(::Label::Body_13);
    accent_line->Add(accent_lbl, wxSizerFlags().CenterVertical());
    accent_line->Add(accent_row, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(ITEM_RIGHT_PADDING)));
    sizer->Add(accent_line, flags);

    // Live token preview row (label-column indented, stretches to the row edge).
    auto *md3_preview_line = new wxBoxSizer(wxHORIZONTAL);
    md3_preview_line->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    md3_preview_line->Add(md3_preview, 1, wxEXPAND | wxRIGHT, FromDIP(ITEM_RIGHT_PADDING));
    sizer->Add(md3_preview_line, flags);

    // UI font rows (family + size + live preview), consistent with the rows above.
    sizer->Add(make_row(_L("Font"), font_combo), flags);
    sizer->Add(make_row(_L("Text size"), text_size), flags);
    sizer->Add(make_row(_L("Preview"), font_preview), flags);

    // Reset row (indent-aligned with the rows above, no leading label).
    auto *reset_line = new wxBoxSizer(wxHORIZONTAL);
    reset_line->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    reset_line->Add(reset_btn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(ITEM_RIGHT_PADDING));
    sizer->Add(reset_line, flags);

    sizer->AddSpacer(FromDIP(20));
    scrolled->SetSizer(sizer);
    scrolled->FitInside();
    return scrolled;
}

wxWindow *PreferencesDialog::create_general_tab()
{
    auto        scrolled = new ScrollPanel(m_book);
    wxBoxSizer *sizer    = new wxBoxSizer(wxVERTICAL);

    auto title_basic = create_item_title(_L("General Settings"), scrolled, _L("General Settings"));

    // Language list (same source as before).
    auto available_translations = wxTranslations::Get()->GetAvailableTranslations(SLIC3R_APP_KEY);
    std::vector<const wxLanguageInfo *> language_infos;
    language_infos.emplace_back(wxLocale::GetLanguageInfo(wxLANGUAGE_ENGLISH));
    for (size_t i = 0; i < available_translations.GetCount(); ++i) {
        const wxLanguageInfo *available_lan = wxLocale::FindLanguageInfo(available_translations[i]);
        if (available_lan == nullptr) continue;
        for (auto si = 0; si < s_supported_languages.size(); si++) {
            auto* supported_lan = wxLocale::GetLanguageInfo(s_supported_languages[si]);
            if (available_lan->CanonicalName == supported_lan->CanonicalName) {
                language_infos.emplace_back(supported_lan);
                break;
            }
        }
    }
    sort_remove_duplicates(language_infos);
    std::sort(language_infos.begin(), language_infos.end(), [](const wxLanguageInfo *l, const wxLanguageInfo *r) { return l->Description < r->Description; });

    std::vector<std::pair<std::string, wxString>> language_choices {
        {I18N::LANGUAGE_MODE_ENGLISH, wxString::FromUTF8("English")},
        {I18N::LANGUAGE_MODE_CANTONESE_HONG_KONG, wxString::FromUTF8("廣東話（香港，預覽版）")},
        {I18N::LANGUAGE_MODE_ENGLISH_CANTONESE_HK, wxString::FromUTF8("English + 廣東話（香港，預覽版）")},
    };
    for (const wxLanguageInfo *info : language_infos) {
        const std::string id = into_u8(info->CanonicalName);
        if (info->CanonicalName.BeforeFirst('_') == "en" ||
            I18N::is_baseline_language_mode(id) || I18N::is_custom_language_mode(id))
            continue;
        language_choices.emplace_back(id, language_display_name(info));
    }
    auto item_language = create_item_language_mode_combobox(
        _L("Language"), scrolled, _L("Language"), "language", language_choices);

    std::vector<wxString> Regions     = {_L("Asia-Pacific"), _L("Chinese Mainland"), _L("Europe"), _L("North America"), _L("Others")};
    auto                  item_region = create_item_region_combobox(_L("Login Region"), scrolled, _L("Login Region"), Regions);

    std::vector<wxString> Units         = {_L("Metric") + " (mm, g)", _L("Imperial") + " (in, oz)"};
    auto                  item_currency = create_item_combobox(_L("Units"), scrolled, _L("Units"), "use_inches", Units, {"0", "1"});

    // Theme (dark mode) now lives in the Appearance section's Theme
    // SegmentedControl (bound to dark_color_mode), so the legacy Windows-only
    // "Enable dark mode" checkbox is no longer created here.

    std::vector<wxString>    FlushOptionLabels = {_L("All"), _L("Color change"), _L("Disabled")};
    std::vector<std::string> FlushOptionValues = {"all", "color change", "disabled"};
    auto item_auto_flush = create_item_combobox(_L("Auto Flush"), scrolled, _L("Auto calculate flush volumes"), "auto_calculate_flush", FlushOptionLabels, FlushOptionValues);

    // Prepare panel dock edge — applies live to the Prepare workspace sidebar.
    std::vector<wxString>    SidebarDockLabels = {_L("Left"), _L("Right"), _L("Top"), _L("Bottom")};
    std::vector<std::string> SidebarDockValues = {"left", "right", "top", "bottom"};
    auto item_sidebar_dock = create_item_combobox(
        _L("Prepare panel position"), scrolled, _L("Dock the Prepare panel on the left, right, top, or bottom of the workspace."),
        "prepare_sidebar_dock", SidebarDockLabels, SidebarDockValues, [](int) {
            if (auto *plater = wxGetApp().plater())
                plater->apply_sidebar_dock();
        });

    auto item_single_instance = create_item_checkbox(_L("Keep only one Bambu Studio instance"), scrolled,
#if __APPLE__
                                                     _L("On OSX there is always only one instance of app running by default. However it is allowed to run multiple instances "
                                                        "of same app from the command line. In such case this settings will allow only one instance."),
#else
                                                     _L("If this is enabled, when starting Bambu Studio and another instance of the same Bambu Studio is already running, that "
                                                        "instance will be reactivated instead."),
#endif
                                                     50, "single_instance");

    auto item_fila_manager = create_item_checkbox(
        _L("Filament Manager") + " (" + _L("Take effect after restarting Studio") + ")", scrolled,
#if __APPLE__
        _L("The Filament Manager is turned off by default on macOS because compatibility issues on some systems may cause the application to become unresponsive."),
#else
        wxEmptyString,
#endif
        50, FilaManagerEnabledConfigKey);

    auto item_multi_machine = create_item_checkbox(_L("Multi-device Management(Take effect after restarting Studio)."), scrolled,
                                                   _L("With this option enabled, you can send a task to multiple devices at the same time and manage multiple devices."), 50,
                                                   "enable_multi_machine");

    auto item_beta_version_update = create_item_checkbox(_L("Support beta version update."), scrolled, _L("With this option enabled, you can receive beta version updates."), 50,
                                                         "enable_beta_version_update");

    // User Experience Improvement Program + "what data" hyperlink.
    auto  item_priv_policy = create_item_checkbox(_L("Join the User Experience Improvement Program."), scrolled, "", 50, "privacyuse");
    auto *hyperlink        = new Label(scrolled, wxString::FromUTF8(_CTX_utf8(L_CONTEXT("Learn more", "Preferences"), "Preferences")));
    hyperlink->SetFont(Label::Head_13);
    hyperlink->SetForegroundColour(ThemeColor::Link);
    hyperlink->Bind(wxEVT_ENTER_WINDOW, [this](auto& e) { SetCursor(wxCURSOR_HAND); });
    hyperlink->Bind(wxEVT_LEAVE_WINDOW, [this](auto& e) { SetCursor(wxCURSOR_ARROW); });
    hyperlink->Bind(wxEVT_LEFT_DOWN, [this](auto &e) {
        UxProgramTermsDialog dlg(this);
        dlg.ShowModal();
    });
    item_priv_policy->GetItem(1)->SetProportion(0);
    item_priv_policy->Insert(item_priv_policy->GetItemCount() - 1, hyperlink, wxSizerFlags().CenterVertical().Proportion(1));

    // Download path row lives inside the General Settings section (Figma:
    // "下载地址" as a plain row, no separate "Downloads" section title).
    auto item_downloads = create_item_downloads(scrolled, 50, "download_path");

    // External editor: the editors detected on this machine by friendly name,
    // plus a trailing "Custom…" entry that uses the executable picked in the
    // row below. Consumed by File > Open in External Editor.
    std::vector<wxString>    EditorLabels;
    std::vector<std::string> EditorValues;
    for (const FoundEditor &editor : get_available_editors()) {
        EditorLabels.push_back(wxString::FromUTF8(editor.name));
        EditorValues.push_back(editor.name);
    }
    EditorLabels.push_back(_L("Custom") + dots);
    EditorValues.push_back("custom");
    auto item_external_editor = create_item_combobox(
        _L("External editor"), scrolled,
        _L("Editor used by File > Open in External Editor; \"Custom\" uses the executable chosen below."),
        "external_editor", EditorLabels, EditorValues);
    auto item_external_editor_path = create_item_external_editor(scrolled, 50, "external_editor_path");

    sizer->Add(title_basic, wxSizerFlags().Expand().Border(wxTOP, FromDIP(24)));
    sizer->AddSpacer(FromDIP(8));
    auto flags = wxSizerFlags().Expand().Border(wxTOP, FromDIP(4));

    sizer->Add(item_language, flags);
    sizer->Add(item_region, flags);
    sizer->Add(item_currency, flags);
    sizer->Add(item_auto_flush, flags);
    sizer->Add(item_sidebar_dock, flags);
    sizer->Add(item_single_instance, flags);
    sizer->Add(item_fila_manager, flags);
    sizer->Add(item_multi_machine, flags);
    sizer->Add(item_beta_version_update, flags);
    sizer->Add(item_priv_policy, flags);
    sizer->Add(item_downloads, flags);
    sizer->Add(item_external_editor, flags);
    sizer->Add(item_external_editor_path, flags);
    scrolled->SetSizer(sizer);
    scrolled->FitInside();
    return scrolled;
}

wxWindow *PreferencesDialog::create_user_tab()
{
    auto        scrolled = new ScrollPanel(m_book);
    wxBoxSizer *sizer    = new wxBoxSizer(wxVERTICAL);

    auto title_user = create_item_title(_L("User Settings"), scrolled, _L("User Settings"));

    auto item_bed_type_follow_preset = create_item_checkbox(_L("Auto plate type"), scrolled, _L("Studio will remember build plate selected last time for certain printer model."),
                                                            50, "user_bed_type");

    // 打印进度计时方式 — 2-option select (12h / 24h) over use_12h_time_format.
    std::vector<wxString>    time_labels = {_L("12-hour"), _L("24-hour")};
    std::vector<std::string> time_values = {"1", "0"};
    auto item_time_format                = create_item_combobox(_L("time format for print progress"), scrolled, _L("Display time in 12-hour format with AM/PM instead of 24-hour format"),
                                                                "use_12h_time_format", time_labels, time_values);

    auto item_auto_stop_liveview =
        create_item_checkbox(_L("Keep liveview when printing."), scrolled,
                             _L("By default, Liveview will pause after 15 minutes of inactivity on the computer. Check this box to disable this feature during printing."), 50,
                             "auto_stop_liveview");

    auto item_auto_transfer = create_item_checkbox(_L("Automatically transfer modified value when switching process and filament presets"), scrolled,
                                                   _L("After closing, a popup will appear to ask each time"), 50, "auto_transfer_when_switch_preset");

    auto item_mix_print_high_low_temp = create_item_checkbox(_L("Remove the restriction on mixed printing of high and low temperature filaments."), scrolled,
                                                             _L("With this option enabled, you can print materials with a large temperature difference together."), 50,
                                                             "enable_high_low_temp_mixed_printing");

    auto item_user_sync = create_item_checkbox(_L("Auto sync user presets(Printer/Filament/Process)"), scrolled,
                                               _L("If enabled, auto sync user presets with cloud after Bambu Studio startup or presets modified."), 50, "sync_user_preset");

    auto item_system_sync = create_item_checkbox(_L("Auto check for system presets updates"), scrolled,
                                                 _L("If enabled, auto check whether there are system presets updates after Bambu Studio startup."), 50, "sync_system_preset");

#ifdef _WIN32
    auto item_webview_auto_fill = create_item_checkbox(_L("Auto-fill previously logged-in accounts."), scrolled, _L(""), 50, "webview_auto_fill");
#endif

    sizer->Add(title_user, wxSizerFlags().Expand().Border(wxTOP, FromDIP(24)));
    sizer->AddSpacer(FromDIP(8));
    auto flags = wxSizerFlags().Expand().Border(wxTOP, FromDIP(4));

    sizer->Add(item_time_format, flags);
    sizer->Add(item_bed_type_follow_preset, flags);
    sizer->Add(item_auto_stop_liveview, flags);
    sizer->Add(item_auto_transfer, flags);
    sizer->Add(item_mix_print_high_low_temp, flags);
    sizer->Add(item_user_sync, flags);
    sizer->Add(item_system_sync, flags);
#ifdef _WIN32
    sizer->Add(item_webview_auto_fill, flags);
#endif

    scrolled->SetSizer(sizer);
    scrolled->FitInside();
    return scrolled;
}

wxWindow *PreferencesDialog::create_3d_tab()
{
    auto        scrolled = new ScrollPanel(m_book);
    wxBoxSizer *sizer    = new wxBoxSizer(wxVERTICAL);

    auto title_3d = create_item_title(_L("3D Settings"), scrolled, _L("3D Settings"));

    auto item_zoom_to_mouse = create_item_checkbox(_L("Zoom to mouse position"), scrolled,
                                                   _L("Zoom in towards the mouse pointer's position in the 3D view, rather than the 2D window center."), 50, "zoom_to_mouse");

    std::vector<wxString> assemble_view_preview_options = {_L("Auto"), _L("Open"), _L("Close")};
    auto                  enable_assemble_view_preview  = create_item_combobox(
        _L("Display overview"), scrolled, _L("Display overview"), "enable_assemble_view_preview", assemble_view_preview_options, {"Auto", "Open", "Close"},
        [](int idx) {
            wxGetApp().app_config->set("enable_assemble_view_preview", idx == 0 ? "Auto" : idx == 1 ? "Open" : "Close");
            if (wxGetApp().app_config->get("enable_assemble_view_preview") == "Auto")
                wxGetApp().app_config->set_bool("enable_bvh", true);
            else if (wxGetApp().app_config->get("enable_assemble_view_preview") == "Open")
                wxGetApp().app_config->set_bool("enable_bvh", false);
        },
        FromDIP(150), FromDIP(120));

    float range_min = 1.0f, range_max = 2.5f;
    auto  item_grabber_size = create_item_range_input(_L("Grabber scale"), scrolled,
                                                      _L("Set grabber size for move,rotate,scale tool.") + _L("Value range") + ":[" + std::to_string(range_min) + "," +
                                                          std::to_string(range_max) + "]",
                                                      "grabber_size_factor", range_min, range_max, 1, [](wxString value) {
                                                         double d_value = 0;
                                                         if (value.ToDouble(&d_value)) GLGizmoBase::Grabber::GrabberSizeFactor = d_value;
                                                     });

    range_min                = 0.0f;
    range_max                = 150.0f;
    auto item_tooltip_offset = create_item_range_two_input(_L("Tooltip offset"), scrolled,
                                                           _L("Set tooltip offset for different mouse size.") + _L("Value range") + ":[" + std::to_string(range_min) + "," +
                                                               std::to_string(range_max) + "]",
                                                           "3d_middle_tooltip_offset_x", "3d_middle_tooltip_offset_y", range_min, range_max, 1, nullptr, nullptr);

    std::vector<wxString> toolbar_style = {_L("Collapsible"), _L("Uncollapsible")};
    auto item_toolbar_style = create_item_combobox(_L("Toolbar Style"), scrolled, _L("Toolbar Style"), "toolbar_style", toolbar_style, {"0", "1"}, [](int idx) -> void {
        const auto &p_ogl_manager = wxGetApp().get_opengl_manager();
        p_ogl_manager->set_toolbar_rendering_style(idx);
    });

    auto item_show_shells = create_item_checkbox(_L("Always show shells in preview"), scrolled,
                                                 _L("Always show shells or not in preview view tab. If you change this value, you should reslice."), 50,
                                                 "show_shells_in_preview");

    auto item_step_mesh_setting = create_item_checkbox(_L("Show the step mesh parameter setting dialog."), scrolled,
                                                       _L("If enabled,a parameter settings dialog will appear during STEP file import."), 50, "enable_step_mesh_setting");

    auto item_import_svg = create_item_checkbox(_L("Import a single SVG and split it"), scrolled, _L("Import a single SVG and then split it to several parts."), 50,
                                                "import_single_svg_and_split");

    auto item_gamma_obj = create_item_checkbox(_L("Enable gamma correction for the imported obj file"), scrolled,
                                               _L("Perform gamma correction on color after importing the obj model."), 50, "gamma_correct_in_import_obj");

    auto item_enable_record_gcodeviewer =
        create_item_checkbox(_L("Remember last used color scheme"), scrolled,
                             _L("When enabled, the last used color scheme (e.g., Line Type, Speed) will be automatically applied on next startup."), 50,
                             "enable_record_gcodeviewer_option_item");

    auto item_enable_lod = create_item_checkbox(_L("Improve rendering performance by lod"), scrolled,
                                                _L("Improved rendering performance under the scene of multiple plates and many models."), 50, "enable_lod");

    auto item_advanced_gcode = create_item_checkbox(_L("Enable advanced gcode viewer"), scrolled, _L("Enable advanced gcode viewer."), 50, "enable_advanced_gcode_viewer_");

    sizer->Add(title_3d, wxSizerFlags().Expand().Border(wxTOP, FromDIP(24)));
    sizer->AddSpacer(FromDIP(8));
    auto flags = wxSizerFlags().Expand().Border(wxTOP, FromDIP(4));

    sizer->Add(enable_assemble_view_preview, flags);
    sizer->Add(item_grabber_size, flags);
    sizer->Add(item_tooltip_offset, flags);
    sizer->Add(item_toolbar_style, flags);
    sizer->Add(item_zoom_to_mouse, flags);
    sizer->Add(item_show_shells, flags);
#if !BBL_RELEASE_TO_PUBLIC
    auto item_show_bvh_bounds = create_item_checkbox(_L("Show assembly BVH primary bounds"), scrolled, _L("Display the BVH primary bounding box wireframe in assembly view."), 50,
                                                     "show_assembly_bvh_bounds");
    sizer->Add(item_show_bvh_bounds, flags);
#endif
    sizer->Add(item_step_mesh_setting, flags);
    sizer->Add(item_import_svg, flags);
    sizer->Add(item_gamma_obj, flags);
    sizer->Add(item_enable_record_gcodeviewer, flags);
    sizer->Add(item_enable_lod, flags);
    sizer->Add(item_advanced_gcode, flags);

    // [refactor-review] Not in Figma v2 3D tab; camera-fullscreen kept here (a 3D/
    // viewport-adjacent toggle). Reviewer: confirm placement.
    auto item_camera_fullscreen = create_item_checkbox(_L("Open full screen camera view on active monitor only."), scrolled,
                                                       _L("When enabled, the camera full screen view opens only on the monitor that contains Bambu Studio."), 50,
                                                       "camera_fullscreen_active_monitor_only");
    sizer->Add(item_camera_fullscreen, flags); // [refactor-review]

    sizer->AddSpacer(FromDIP(20));
    scrolled->SetSizer(sizer);
    scrolled->FitInside();
    return scrolled;
}

wxWindow *PreferencesDialog::create_other_tab()
{
    auto        scrolled = new ScrollPanel(m_book);
    wxBoxSizer *sizer    = new wxBoxSizer(wxVERTICAL);

    // ---- Project ----
    auto title_project         = create_item_title(_L("Project"), scrolled, "");
    auto item_max_recent_count = create_item_input(_L("Maximum recent projects"), "", scrolled, _L("Maximum count of recent projects"), "max_recent_count", [](wxString value) {
        long max = 0;
        if (value.ToLong(&max)) wxGetApp().mainframe->set_max_recent_count(max);
    });
    auto item_gcodes_warning = create_item_checkbox(_L("No warnings when loading 3MF with modified G-codes"), scrolled, _L("No warnings when loading 3MF with modified G-codes"),
                                                    50, "no_warn_when_modified_gcodes");
    std::vector<wxString>    backup_labels = {_L("10 seconds"), _L("20 seconds"), _L("30 seconds"), _L("1 minute"), _L("2 minutes"),
                                              _L("5 minutes"),  _L("10 minutes"), _L("30 minutes"), _L("never")};
    std::vector<std::string> backup_values = {"10", "20", "30", "60", "120", "300", "600", "1800", "0"};
    auto item_auto_backup = create_item_combobox(_L("Auto-Backup"), scrolled, _L("The peroid of backup in seconds."), "backup_interval", backup_labels, backup_values,
                                                 [this](int) {
                                                     m_backup_interval_time = app_config->get("backup_interval");
                                                     long backup_interval   = 0;
                                                     m_backup_interval_time.ToLong(&backup_interval);
                                                     Slic3r::set_backup_interval(backup_interval);
                                                 });

    sizer->Add(title_project, wxSizerFlags().Expand().Border(wxTOP, FromDIP(16)));
    sizer->AddSpacer(FromDIP(8));
    auto flags = wxSizerFlags().Expand().Border(wxTOP, FromDIP(4));

    sizer->Add(item_max_recent_count, flags);
    sizer->Add(item_auto_backup, flags);
    sizer->Add(item_gcodes_warning, flags);

    // ---- Online Models (visible only when has_model_mall()) ----
    auto title_modelmall   = create_item_title(_L("Online Models"), scrolled, _L("Online Models"));
    auto item_modelmall    = create_item_checkbox(_L("Show online staff-picked models on the home page"), scrolled, _L("Show online staff-picked models on the home page"), 50,
                                                  "staff_pick_switch");
    auto item_show_history = create_item_checkbox(_L("Show history on the home page"), scrolled, _L("Show history on the home page"), 50, "show_print_history");

    auto title_modelmall_item   = sizer->Add(title_modelmall, wxSizerFlags().Expand().Border(wxTOP, FromDIP(16)));
    auto item_modelmall_item    = sizer->Add(item_modelmall, flags);
    auto item_show_history_item = sizer->Add(item_show_history, flags);

    auto update_modelmall = [scrolled, title_modelmall_item, item_modelmall_item, item_show_history_item](wxEvent &) {
        bool has_model_mall = wxGetApp().has_model_mall();
        title_modelmall_item->Show(has_model_mall);
        item_modelmall_item->Show(has_model_mall);
        item_show_history_item->Show(has_model_mall);
        scrolled->Layout();
        scrolled->FitInside();
    };
    wxCommandEvent dummy(wxEVT_COMBOBOX);
    update_modelmall(dummy);

    // ---- Developer Mode (Figma keeps these two here, in the Other tab) ----
    auto title_dev           = create_item_title(_L("Developer Mode"), scrolled, _L("Developer Mode"));
    auto item_dev_mode       = create_item_checkbox(_L("Develop mode"), scrolled, _L("Develop mode"), 50, "developer_mode");
    auto item_skip_blacklist = create_item_checkbox(_L("Skip AMS blacklist check"), scrolled, _L("Skip AMS blacklist check"), 50, "skip_ams_blacklist_check");
    sizer->Add(title_dev, wxSizerFlags().Expand().Border(wxTOP, FromDIP(16)));
    sizer->Add(item_dev_mode, flags);
    sizer->Add(item_skip_blacklist, flags);

#ifdef _WIN32
    // ---- Associate Files To Bambu Studio (Windows only) ----
    auto title_associate_file = create_item_title(_L("Associate Files To Bambu Studio"), scrolled, _L("Associate Files To Bambu Studio"));
    auto item_associate_3mf   = create_item_checkbox(_L("Associate .3mf files to Bambu Studio"), scrolled,
                                                     _L("If enabled, sets Bambu Studio as default application to open .3mf files"), 50, "associate_3mf");
    auto item_associate_stl   = create_item_checkbox(_L("Associate .stl files to Bambu Studio"), scrolled,
                                                     _L("If enabled, sets Bambu Studio as default application to open .stl files"), 50, "associate_stl");
    auto item_associate_step  = create_item_checkbox(_L("Associate .step/.stp files to Bambu Studio"), scrolled,
                                                     _L("If enabled, sets Bambu Studio as default application to open .step files"), 50, "associate_step");
    sizer->Add(title_associate_file, wxSizerFlags().Expand().Border(wxTOP, FromDIP(16)));
    sizer->Add(item_associate_3mf, flags);
    sizer->Add(item_associate_stl, flags);
    sizer->Add(item_associate_step, flags);
#endif

    sizer->AddSpacer(FromDIP(20));
    scrolled->SetSizer(sizer);
    scrolled->FitInside();
    return scrolled;
}

wxWindow *PreferencesDialog::create_developer_tab()
{
    auto        scrolled = new ScrollPanel(m_book);
    wxBoxSizer *sizer    = new wxBoxSizer(wxVERTICAL);

    m_internal_developer_mode_def = app_config->get("internal_developer_mode");
    m_iot_environment_def         = app_config->get("iot_environment");

    // ---- Log ----
    auto title_log  = create_item_title(_L("Log"), scrolled, _L("Log"));
    auto log_levels = std::vector<wxString>{_L("fatal"), _L("error"), _L("warning"), _L("info"), _L("debug"), _L("trace")};
    auto item_log   = create_item_loglevel_combobox(_L("Log Level"), scrolled, _L("Log Level"), log_levels);
    sizer->Add(title_log, wxSizerFlags().Expand().Border(wxTOP, FromDIP(24)));
    sizer->AddSpacer(FromDIP(8));
    auto flags = wxSizerFlags().Expand().Border(wxTOP, FromDIP(4));

    sizer->AddSpacer(FromDIP(4));
    sizer->Add(item_log, flags);

    auto title_dev         = create_item_title(_L("Developer Tools"), scrolled, _L("Developer Tools"));
    auto item_internal_dev = create_item_checkbox(_L("Internal developer mode"), scrolled, _L("Internal developer mode"), 50, "internal_developer_mode");
    auto item_ssl_mqtt     = create_item_checkbox(_L("Enable SSL(MQTT)"), scrolled, _L("Enable SSL(MQTT)"), 50, "enable_ssl_for_mqtt");
    auto item_ssl_ftp      = create_item_checkbox(_L("Enable SSL(FTP)"), scrolled, _L("Enable SSL(FTP)"), 50, "enable_ssl_for_ftp");

    auto title_host = create_item_title(_L("Host Setting"), scrolled, _L("Host Setting"));
    auto radio1     = create_item_radiobox(_L("DEV host: api-dev.bambu-lab.com/v1"), scrolled, wxEmptyString, 50, 1, "dev_host");
    auto radio2     = create_item_radiobox(_L("QA  host: api-qa.bambu-lab.com/v1"), scrolled, wxEmptyString, 50, 1, "qa_host");
    auto radio3     = create_item_radiobox(_L("PRE host: api-pre.bambu-lab.com/v1"), scrolled, wxEmptyString, 50, 1, "pre_host");
    auto radio4     = create_item_radiobox(_L("Product host"), scrolled, wxEmptyString, 50, 1, "product_host");

    if (m_iot_environment_def == ENV_DEV_HOST) {
        on_select_radio("dev_host");
    } else if (m_iot_environment_def == ENV_QAT_HOST) {
        on_select_radio("qa_host");
    } else if (m_iot_environment_def == ENV_PRE_HOST) {
        on_select_radio("pre_host");
    } else if (m_iot_environment_def == ENV_PRODUCT_HOST) {
        on_select_radio("product_host");
    }

    StateColor btn_bg_white(std::pair<wxColour, int>(ThemeColor::Grey400, StateColor::Disabled), std::pair<wxColour, int>(ThemeColor::Grey400, StateColor::Pressed),
                            std::pair<wxColour, int>(ThemeColor::Grey300, StateColor::Hovered), std::pair<wxColour, int>(ThemeColor::White, StateColor::Normal));
    StateColor btn_bd_white(std::pair<wxColour, int>(ThemeColor::White, StateColor::Disabled), std::pair<wxColour, int>(ThemeColor::TextPrimary, StateColor::Enabled));

    Button *debug_button                = new Button(scrolled, _L("debug save button"));
    m_button_list[m_button_list.size()] = debug_button;
    debug_button->SetBackgroundColor(btn_bg_white);
    debug_button->SetBorderColor(btn_bd_white);
    debug_button->SetFont(Label::Body_13);
    debug_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
        // success message box
        MessageDialog dialog(this, _L("save debug settings"), _L("DEBUG settings have saved successfully!"), wxNO_DEFAULT | wxYES_NO | wxICON_INFORMATION);
        dialog.SetSize(400,-1);
        switch (dialog.ShowModal()) {
        case wxID_NO: {
            //if (m_developer_mode_def != app_config->get("developer_mode")) {
            //    app_config->set_bool("developer_mode", m_developer_mode_def == "true" ? true : false);
            //    m_developer_mode_ckeckbox->SetValue(m_developer_mode_def == "true" ? true : false);
            //}
            //if (m_internal_developer_mode_def != app_config->get("internal_developer_mode")) {
            //    app_config->set_bool("internal_developer_mode", m_internal_developer_mode_def == "true" ? true : false);
            //    m_internal_developer_mode_ckeckbox->SetValue(m_internal_developer_mode_def == "true" ? true : false);
            //}

            if (m_iot_environment_def == ENV_DEV_HOST) {
                on_select_radio("dev_host");
            } else if (m_iot_environment_def == ENV_QAT_HOST) {
                on_select_radio("qa_host");
            } else if (m_iot_environment_def == ENV_PRE_HOST) {
                on_select_radio("pre_host");
            } else if (m_iot_environment_def == ENV_PRODUCT_HOST) {
                on_select_radio("product_host");
            }

            break;
        }

        case wxID_YES: {
            // bbs  domain changed
            auto param = get_select_radio(1);

            std::map<wxString, wxString> iot_environment_map;
            iot_environment_map["dev_host"] = ENV_DEV_HOST;
            iot_environment_map["qa_host"]  = ENV_QAT_HOST;
            iot_environment_map["pre_host"] = ENV_PRE_HOST;
            iot_environment_map["product_host"] = ENV_PRODUCT_HOST;

            //if (iot_environment_map[param] != m_iot_environment_def) {
            if (true) {
                NetworkAgent* agent = wxGetApp().getAgent();
                if (param == "dev_host") {
                    app_config->set("iot_environment", ENV_DEV_HOST);
                }
                else if (param == "qa_host") {
                    app_config->set("iot_environment", ENV_QAT_HOST);
                }
                else if (param == "pre_host") {
                    app_config->set("iot_environment", ENV_PRE_HOST);
                }
                else if (param == "product_host") {
                    app_config->set("iot_environment", ENV_PRODUCT_HOST);
                }


                wxGetApp().update_publish_status();

                AppConfig* config = GUI::wxGetApp().app_config;
                std::string country_code = config->get_country_code();
                if (agent) {
                    wxGetApp().request_user_logout();
                    agent->set_country_code(country_code);
                }
                ConfirmBeforeSendDialog confirm_dlg(this, wxID_ANY, _L("Warning"), ConfirmBeforeSendDialog::ButtonStyle::ONLY_CONFIRM);
                confirm_dlg.update_text(_L("Switch cloud environment, Please login again!"));
                confirm_dlg.on_show();
            }

            // bbs  backup
            //app_config->set("backup_interval", std::string(m_backup_interval_time.mb_str()));
            app_config->save();
            Slic3r::set_backup_interval(boost::lexical_cast<long>(app_config->get("backup_interval")));

            this->Close();
            break;
        }
        }
    });

    sizer->Add(title_dev, wxSizerFlags().Expand().Border(wxTOP, FromDIP(24)));
    sizer->AddSpacer(FromDIP(8));

    sizer->Add(item_internal_dev, flags);
    sizer->Add(item_ssl_mqtt, flags);
    sizer->Add(item_ssl_ftp, flags);

    sizer->Add(title_host, wxSizerFlags().Expand().Border(wxTOP, FromDIP(24)));
    sizer->Add(radio1, flags);
    sizer->Add(radio2, flags);
    sizer->Add(radio3, flags);
    sizer->Add(radio4, flags);
    sizer->Add(debug_button, wxSizerFlags().Center());

    scrolled->SetSizer(sizer);
    scrolled->FitInside();
    return scrolled;
}

// ============================================================================
//  Bottom actions: Reset all warning dialogs / Reset preferences.
// ============================================================================
wxBoxSizer *PreferencesDialog::create_bottom_buttons()
{
    auto *row = new wxBoxSizer(wxHORIZONTAL);

    auto *btn_reset_warnings            = new Button(this, _L("Reset all warning dialogs"));
    auto *btn_reset_prefs               = new Button(this, _L("Reset preferences"));
    m_button_list[m_button_list.size()] = btn_reset_warnings;
    m_button_list[m_button_list.size()] = btn_reset_prefs;

    // MD3 outlined buttons: transparent interior + 1px Outline ring with an
    // OnSurface label, pill radius (height/2) and a SurfaceContainerHigh hover
    // wash — geometry, font and colours are all resolved through semantic roles
    // by Button::applyMD3Style(), replacing the Grey300/Grey400/BrandGreen r6 look.
    for (Button *b : {btn_reset_warnings, btn_reset_prefs}) {
        b->SetVariant(Button::Variant::Outlined);
        b->SetButtonSize(Button::Size::Small);
    }

    btn_reset_warnings->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { on_reset_all_warnings(); });
    btn_reset_prefs->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { on_reset_preferences(); });

    row->AddStretchSpacer();
    row->Add(btn_reset_warnings, 0, wxRIGHT, FromDIP(8));
    row->Add(btn_reset_prefs, 0, 0, 0);
    row->AddStretchSpacer();
    return row;
}

ResetWarningsDialog::ResetWarningsDialog(wxWindow *parent) : DPIDialog(parent, wxID_ANY, _L("Reset"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
{
    SetBackgroundColour(ThemeColor::White);
    std::string icon_path = (boost::format("%1%/images/BambuStudioTitle.ico") % resources_dir()).str();
    SetIcon(wxIcon(encode_path(icon_path.c_str()), wxBITMAP_TYPE_ICO));

    const int content_width = FromDIP(417);

    auto *main_sizer = new wxBoxSizer(wxVERTICAL);

    // Body text.
    auto *msg = new wxStaticText(this, wxID_ANY,
                                 _L("All warning dialogs that you have disabled by checking \"Don't show again\" "
                                    "are now re-enabled and will show next time they apply."));
    msg->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    msg->SetFont(::Label::Body_14);
    msg->Wrap(content_width);
    msg->SetMinSize(wxSize(content_width, -1));
    main_sizer->Add(msg, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(20));

    StateColor btn_bg_gray(std::pair<wxColour, int>(ThemeColor::Grey400, StateColor::Pressed), std::pair<wxColour, int>(ThemeColor::Grey200, StateColor::Hovered),
                           std::pair<wxColour, int>(ThemeColor::White, StateColor::Normal));
    m_details_btn = new Button(this, _L("Check details"));
    m_details_btn->SetBackgroundColor(btn_bg_gray);
    m_details_btn->SetBorderColor(ThemeColor::Grey450);
    m_details_btn->SetMinSize(wxSize(FromDIP(58), FromDIP(24)));
    m_details_btn->SetFont(::Label::Body_12);
    m_details_btn->SetCornerRadius(FromDIP(6));
    m_details_btn->SetCanFocus(false);
    m_details_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { toggle_details(); });
    main_sizer->Add(m_details_btn, 0, wxLEFT | wxTOP, FromDIP(16));

    // Collapsible grey details panel (hidden initially).
    m_details_panel = new wxPanel(this, wxID_ANY);
    m_details_panel->SetBackgroundColour(ThemeColor::Grey300);
    auto *det_sizer = new wxBoxSizer(wxVERTICAL);
    auto *det_text  = new wxStaticText(m_details_panel, wxID_ANY,
                                       _L("- Sync printer presets after loading a file\n"
                                          "- \"Load 3MF\" dialog settings\n"
                                          "- Executing post-processing scripts\n"
                                          "- Support structure recommendation prompt\n"
                                          "- Unsaved projects.\n"
                                          "- Mixed color sublayer with variable layer height warning"));
    det_text->SetForegroundColour(ThemeColor::TextSecondary);
    det_text->SetFont(::Label::Body_13);
    det_sizer->Add(det_text, 0, wxALL, FromDIP(12));
    m_details_panel->SetSizer(det_sizer);
    m_details_panel->Hide();
    main_sizer->Add(m_details_panel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(20));

    // Footer buttons.
    auto *btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    btn_sizer->AddStretchSpacer(1);
    auto      *cancel_btn = new Button(this, _L("Cancel"));
    cancel_btn->SetBackgroundColor(btn_bg_gray);
    cancel_btn->SetBorderColor(ThemeColor::Grey450);
    cancel_btn->SetTextColor(ThemeColor::TextPrimary);
    cancel_btn->SetFont(::Label::Body_12);
    cancel_btn->SetMinSize(wxSize(FromDIP(58), FromDIP(24)));
    cancel_btn->SetCornerRadius(FromDIP(6));
    cancel_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
    btn_sizer->Add(cancel_btn, 0, wxRIGHT, FromDIP(8));

    StateColor btn_bg_green(std::pair<wxColour, int>(ThemeColor::BrandGreenPressed, StateColor::Pressed),
                            std::pair<wxColour, int>(ThemeColor::BrandGreenHovered, StateColor::Hovered), std::pair<wxColour, int>(ThemeColor::BrandGreen, StateColor::Normal));
    auto      *confirm_btn = new Button(this, _L("Confirm"));
    confirm_btn->SetBackgroundColor(btn_bg_green);
    confirm_btn->SetBorderColor(ThemeColor::BrandGreen);
    confirm_btn->SetTextColor(StateColor::semantic(MD3::Role::OnPrimary));
    confirm_btn->SetFont(::Label::Body_12);
    confirm_btn->SetMinSize(wxSize(FromDIP(58), FromDIP(24)));
    confirm_btn->SetCornerRadius(FromDIP(6));
    confirm_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_OK); });
    btn_sizer->Add(confirm_btn, 0, 0, 0);

    main_sizer->Add(btn_sizer, 0, wxEXPAND | wxALL, FromDIP(20));

    SetSizer(main_sizer);
    Layout();
    Fit();
    CentreOnParent();
    wxGetApp().UpdateDlgDarkUI(this);
}

void ResetWarningsDialog::toggle_details()
{
    m_expanded = !m_expanded;
    m_details_panel->Show(m_expanded);
    m_details_btn->SetLabel(m_expanded ? _L("Collapse") : _L("Check details"));
    Layout();
    Fit();
}

void PreferencesDialog::on_reset_all_warnings()
{
    // Expandable confirm dialog (Figma 4293-4971 collapsed / 4293-4972 expanded):
    // "Check details" reveals the list of settings that get cleared.
    ResetWarningsDialog dlg(this);
    if (dlg.ShowModal() != wxID_OK) return;

    // Keys that the various "Don't show again"-style dialogs persist their
    // dismissal choice under. Erasing them re-enables the corresponding dialog.
    app_config->erase("app", "sync_after_load_file_show_flag");
    app_config->erase("app", "skip_non_bambu_3mf_warning");
    app_config->erase("app", "post_process_script_choice");
    app_config->erase("app", "no_warn_mixed_sublayer_variable_layer");
    app_config->set("show_support_recommend_dialog", "true");
    app_config->set("save_project_choise", "");
    if (wxGetApp().plater()) wxGetApp().plater()->reset_post_process_script_choice();
    app_config->save();
}

void PreferencesDialog::on_reset_preferences()
{
    MessageDialog dlg(this, _L("Are you sure you want to reset all preferences? Changes will take effect after restart."), _L("Reset"), wxICON_QUESTION | wxOK | wxCANCEL);
    if (dlg.ShowModal() != wxID_OK) return;

    // Reset to factory defaults. Touch only UI preference keys — keep vendor /
    // printer / preset state intact, which AppConfig::reset() would also clear.
    static const char *kPrefKeys[] = {
        "language",
        // "region", keep this intensinaly to avoid re-login
        "use_inches",
        "dark_color_mode",
        "ui_density",
        "ui_accent_seed",
        "auto_calculate_flush",
        "single_instance",
        FilaManagerEnabledConfigKey,
        "enable_multi_machine",
        "enable_beta_version_update",
        "privacyuse",
        "download_path",
        "webview_auto_fill",
        "associate_3mf",
        "associate_stl",
        "associate_step",
        "user_bed_type",
        "use_12h_time_format",
        "auto_stop_liveview",
        "auto_transfer_when_switch_preset",
        "enable_high_low_temp_mixed_printing",
        "sync_user_preset",
        "sync_system_preset",
        "disable_fins_extrude_safe_temp",
        "zoom_to_mouse",
        "enable_assemble_view_preview",
        "grabber_size_factor",
        "3d_middle_tooltip_offset_x",
        "3d_middle_tooltip_offset_y",
        "toolbar_style",
        "show_shells_in_preview",
        "enable_step_mesh_setting",
        "import_single_svg_and_split",
        "gamma_correct_in_import_obj",
        "enable_lod",
        "enable_advanced_gcode_viewer_",
        "camera_fullscreen_active_monitor_only",
        "max_recent_count",
        "no_warn_when_modified_gcodes",
        "backup_switch",
        "backup_interval",
        "staff_pick_switch",
        "show_print_history",
        "developer_mode",
        "skip_ams_blacklist_check",
        "severity_level",
    };
    for (const char *k : kPrefKeys) app_config->erase("app", k);
    app_config->set_defaults();
    app_config->save();
}

void PreferencesDialog::on_select_radio(std::string param)
{
    RadioSelectorList::Node *node    = m_radio_group.GetFirst();
    auto                     groupid = 0;

    while (node) {
        RadioSelector *rs = node->GetData();
        if (rs->m_param_name == param) groupid = rs->m_groupid;
        node = node->GetNext();
    }

    node = m_radio_group.GetFirst();
    while (node) {
        RadioSelector *rs = node->GetData();
        if (rs->m_groupid == groupid && rs->m_param_name == param) rs->m_radiobox->SetValue(true);
        if (rs->m_groupid == groupid && rs->m_param_name != param) rs->m_radiobox->SetValue(false);
        node = node->GetNext();
    }
}

wxString PreferencesDialog::get_select_radio(int groupid)
{
    RadioSelectorList::Node *node = m_radio_group.GetFirst();
    while (node) {
        RadioSelector *rs = node->GetData();
        if (rs->m_groupid == groupid && rs->m_radiobox->GetValue()) { return rs->m_param_name; }
        node = node->GetNext();
    }

    return wxEmptyString;
}

void PreferencesDialog::OnSelectRadio(wxMouseEvent &event)
{
    RadioSelectorList::Node *node    = m_radio_group.GetFirst();
    auto                     groupid = 0;

    while (node) {
        RadioSelector *rs = node->GetData();
        if (rs->m_radiobox->GetId() == event.GetId()) groupid = rs->m_groupid;
        node = node->GetNext();
    }

    node = m_radio_group.GetFirst();
    while (node) {
        RadioSelector *rs = node->GetData();
        if (rs->m_groupid == groupid && rs->m_radiobox->GetId() == event.GetId()) rs->m_radiobox->SetValue(true);
        if (rs->m_groupid == groupid && rs->m_radiobox->GetId() != event.GetId()) rs->m_radiobox->SetValue(false);
        node = node->GetNext();
    }
}


}} // namespace Slic3r::GUI
