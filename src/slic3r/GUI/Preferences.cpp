#include <wx/wrapsizer.h>
#include "Preferences.hpp"
#include "AttentionModes/AttentionModel.hpp"
#include "PreferencesGeometry.hpp"
#include "PreferencesGeometryProbe.hpp"
#include "PreferencesSearchTraversal.hpp"
#include "PersonalVocabulary.hpp"
#include "VocabularyDrop.hpp"
#include "ActionSounds.hpp"
#include "TtsNarrator.hpp"
#include "LanModelDrop/LanModelDropStation.hpp"
#include "LanModelDrop/LanModelDropUi.hpp"
#include "StatusApi/StatusApiCore.hpp"
#include "StatusApi/StatusApiServer.hpp"
#include "StatusApi/StatusApiUi.hpp"
#include "PersonalModes/SchoolMode.hpp"
#include "FeatureServices/PresentationRoutes.hpp"
#include "FeatureServices/SchoolCredentials.hpp"
#include "FeatureServices/SurfaceRegistry.hpp"
#include "PersonalModes/SchoolRuntime.hpp"
#include "Widgets/MD3ScrolledWindow.hpp"
#include "Export/ExportDatasets.hpp"
#include "Export/ExportDialog.hpp"
#include "OptionsGroup.hpp"
#include "GUI_App.hpp"
#include "AppDisplayName.hpp"
#include "AppLogo/LogoPanel.hpp"
#include "MainFrame.hpp"
#include "Plater.hpp"
#include "MsgDialog.hpp"
#include "I18N.hpp"
#include "Schedule/ScheduledSettings.hpp"
#include "Schedule/ScheduledSettingsPanel.hpp"
#include "BilingualRegistry.hpp"
#include "UxProgramTermsDialog.hpp"
#include "Widgets/StateColor.hpp"
#include "libslic3r/AppConfig.hpp"
#include "../Utils/ExternalEditor.hpp"
#include <cassert>
#include <ctime>
#include <filesystem>
#include <set>
#include <wx/datetime.h>
#include <string>
#include <vector>
#include <wx/string.h>
#include <wx/tokenzr.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/simplebook.h>
#include <wx/filedlg.h>
#include <wx/colordlg.h>
#include "OG_CustomCtrl.hpp"
#include "fila_manager/wgtFilaManagerFeature.h"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "Widgets/SwitchButton.hpp"
#include "Appearance/ElementStyle.hpp"
#include "Widgets/SearchField.hpp"
#include "Widgets/Slider.hpp"
#include "Widgets/TabStrip.hpp"
#include "SettingsDraftPanel.hpp"
#include "Widgets/MD3ColorPicker.hpp"
#include "Widgets/MD3DialogChrome.hpp"
#include "Widgets/StaticBox.hpp"
#include "Widgets/MaterialIcon.hpp"
#include "wx/graphics.h"
#include <wx/dcgraph.h>
#include <wx/fontenum.h>

#include <wx/listimpl.cpp>
#include <map>
#include <memory>
#include <wx/sizer.h>
#include <wx/slider.h>
#include "Gizmos/GLGizmoBase.hpp"
#include "OpenGLManager.hpp"
#ifdef __WINDOWS__
#ifdef _MSW_DARK_MODE
#include "dark_mode.hpp"
#endif // _MSW_DARK_MODE
#endif //__WINDOWS__
#include "libslic3r/Log.hpp"

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
static constexpr int TITLE_PADDING        = 16;
static constexpr int ITEM_LEFT_PADDING    = 24;
static constexpr int ITEM_RIGHT_PADDING   = 16;
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

static wxString minor_restricted_tip()
{
    return _L("For users under the age of majority, some services will be restricted. "
              "This is managed by your parent or guardian in Family Hub.");
}

// Scrolled panel used for every Preferences tab. wxScrolledWindow's default
// behavior is to scroll to whatever child receives focus, which makes the
// dialog jump around when the user tabs between combobox/checkbox rows.
// Suppressing that and presetting a sensible scroll rate keeps tab pages stable.
class ScrollPanel : public MD3ScrolledWindow
{
public:
    explicit ScrollPanel(wxWindow *parent) : MD3ScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL)
    {
        SetScrollRate(5, 5);
        // Content pane surface — driven by role so dark resolves via semantic()
        // instead of the legacy White->dark swap map.
        SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
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
    auto m_title = new Label(parent, title);
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

wxBoxSizer *PreferencesDialog::create_item_combobox(wxString title, wxWindow *parent, wxString tooltip, std::string param,
                                                    const std::vector<wxString>& label_list, const std::vector<std::string>& value_list,
                                                    const std::vector<wxString>& tooltip_list, std::function<void(int)> callback,
                                                    int title_width, int combox_width, std::shared_ptr<std::vector<std::string>> live_values)
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
            BOOST_LOG_TRIVIAL(warning) << current_exception_message();
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

    auto combo_title = new Label(parent, title, 0, title_width == 0 ? wxSize(FromDIP(TITLE_WIDTH), -1) : wxSize(title_width, -1));
    combo_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    combo_title->SetFont(::Label::Body_13);
    combo_title->SetToolTip(tooltip);
    combo_title->Wrap(-1);
    m_sizer_combox->Add(combo_title, wxSizerFlags().CenterVertical().Proportion(1));

    auto combobox = new ::ComboBox(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, combox_width == 0 ? wxSize(FromDIP(LARGE_COMBOBOX_WIDTH), -1) : wxSize(combox_width, -1),
                                   0, nullptr, wxCB_READONLY);
    m_combobox_list[m_combobox_list.size()] = combobox;
    combobox->SetName(title);
    combobox->SetFont(::Label::Body_13);
    combobox->GetDropDown().SetFont(::Label::Body_13);
    combobox->SetCornerRadius(FromDIP(10)); // MD3 SelectField r10

    for (auto label : label_list) combobox->Append(label);

    assert(tooltip_list.empty() || tooltip_list.size() == label_list.size());
    for (int i = 0; i < static_cast<int>(tooltip_list.size()); ++i) { combobox->SetItemTooltip(i, tooltip_list[i]); }

    // Let the dropdown grow to fit the widest item (never narrower than the combobox),
    // clamped at a fixed max width so long labels aren't ellipsized.
    combobox->GetDropDown().SetUseContentWidth(true, true);

    auto old_value = app_config->get(param);
    if (!old_value.empty()) {
        combobox->SetSelection(get_value_idx(old_value));
    }
    else {
        combobox->SetSelection(0);
    }

    m_sizer_combox->Add(combobox, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(ITEM_RIGHT_PADDING)));

    //// save config
    combobox->GetDropDown().Bind(wxEVT_COMBOBOX, [this, param, value_list, callback, live_values](wxCommandEvent &e) {
        const auto& values = live_values ? *live_values : value_list;
        if (e.GetSelection() < 0 || static_cast<std::size_t>(e.GetSelection()) >= values.size()) return;
        app_config->set(param, values[e.GetSelection()]);
        app_config->save();
        if (callback) {
            callback(e.GetSelection());
        }
        e.Skip();
    });
    register_option_row(param, m_sizer_combox);
    return m_sizer_combox;
}

wxBoxSizer *PreferencesDialog::create_item_language_combobox(
    wxString title, wxWindow *parent, wxString tooltip, int padding_left, std::string param, std::vector<const wxLanguageInfo *> vlist)
{
    wxBoxSizer *m_sizer_combox = new wxBoxSizer(wxHORIZONTAL);
    m_sizer_combox->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    m_sizer_combox->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));

    auto combo_title = new Label(parent, title, 0, wxSize(FromDIP(TITLE_WIDTH), -1));
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
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_THAI)) {
            language_name = wxString::FromUTF8("\xE0\xB9\x84\xE0\xB8\x97\xE0\xB8\xA2"); // ไทย
        }
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_ROMANIAN)) {
            language_name = wxString::FromUTF8("Rom\xC3\xA2n\xC4\x83"); // Română
        }
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_GREEK)) {
            language_name = wxString::FromUTF8("\xCE\x95\xCE\xBB\xCE\xBB\xCE\xB7\xCE\xBD\xCE\xB9\xCE\xBA\xCE\xAC"); // Ελληνικά
        }
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_INDONESIAN)) {
            language_name = wxString::FromUTF8("Bahasa Indonesia");
        }
        else if (vlist[i] == wxLocale::GetLanguageInfo(wxLANGUAGE_VIETNAMESE)) {
            language_name = wxString::FromUTF8("Ti\xE1\xBA\xBFng Vi\xE1\xBB\x87t"); // Tiếng Việt
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
                auto old_value = app_config->get(param);
                if (wxGetApp().load_language(vlist[m_current_language_selected]->CanonicalName, false)) {
                    app_config->set(param, vlist[m_current_language_selected]->CanonicalName.ToUTF8().data());
                    app_config->save();
                    Close();
                    // Reparent(nullptr);
                    GetParent()->RemoveChild(this);
                    Label::initSysFont(I18N::language_mode_profile().font_language);
                    wxGetApp().recreate_GUI(_L("Changing application language"));
                } else {
                    app_config->set(param, old_value);
                    app_config->save();
                    Close();
                }
            }
        }

        e.Skip();
    });

    register_option_row(param, m_sizer_combox);
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

    auto *combo_title = new Label(parent, title, 0, wxSize(FromDIP(TITLE_WIDTH), -1));
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

    register_option_row(param, row);
    return row;
}

wxBoxSizer *PreferencesDialog::create_item_region_combobox(wxString title, wxWindow *parent, wxString tooltip, std::vector<wxString> vlist)
{
    std::vector<wxString> local_regions = {"Asia-Pacific", "China", "Europe", "North America", "Others"};

    wxBoxSizer *m_sizer_combox = new wxBoxSizer(wxHORIZONTAL);
    m_sizer_combox->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    m_sizer_combox->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));

    auto combo_title = new Label(parent, title, 0, wxSize(FromDIP(TITLE_WIDTH), -1));
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
        if (region_index == current_region) {
            // Re-selecting the region already in use: no change, do not prompt/log out.
            e.Skip();
            return;
        }
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

    register_option_row("region", m_sizer_combox);
    return m_sizer_combox;
}

wxBoxSizer *PreferencesDialog::create_item_loglevel_combobox(wxString title, wxWindow *parent, wxString tooltip, std::vector<wxString> vlist)
{
    wxBoxSizer *m_sizer_combox = new wxBoxSizer(wxHORIZONTAL);
    m_sizer_combox->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    m_sizer_combox->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));

    auto combo_title = new Label(parent, title, 0, wxSize(FromDIP(TITLE_WIDTH), -1));
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
    register_option_row("severity_level", m_sizer_combox);
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

   auto combo_title = new Label(parent, title, 0, wxSize(FromDIP(TITLE_WIDTH), -1));
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

   auto combo_title_add = new Label(parent, wxT("+"));
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

    register_option_row(param, m_sizer_tcombox);
    return m_sizer_tcombox;
}

wxBoxSizer *PreferencesDialog::create_item_input(wxString title, wxString title2, wxWindow *parent, wxString tooltip, std::string param, std::function<void(wxString)> onchange)
{
    wxBoxSizer *sizer_input = new wxBoxSizer(wxHORIZONTAL);
    sizer_input->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));
    auto        input_title   = new Label(parent, title);
    input_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    input_title->SetFont(::Label::Body_13);
    input_title->SetToolTip(tooltip);
    input_title->Wrap(-1);

    auto       input = new ::TextInput(parent, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(INPUT_WIDTH), FromDIP(ITEM_MIN_HEIGHT)), wxTE_PROCESS_ENTER);
    StateColor input_bg(std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHigh), StateColor::Disabled), std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHighest), StateColor::Enabled));
    input->SetBackgroundColor(input_bg);
    input->SetCornerRadius(FromDIP(10));
    input->GetTextCtrl()->SetFont(::Label::Mono_13);
    input->GetTextCtrl()->SetValue(app_config->get(param));
    wxTextValidator validator(wxFILTER_DIGITS);
    input->GetTextCtrl()->SetValidator(validator);

    wxStaticText *second_title = nullptr;
    if (!title2.empty()) {
        second_title = new Label(parent, title2, 0, wxSize(FromDIP(TITLE_WIDTH), -1));
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

    register_option_row(param, sizer_input);
    return sizer_input;
}

wxBoxSizer *PreferencesDialog::create_item_int_slider(
    wxString title, wxWindow *parent, wxString tooltip, std::string param, int range_min, int range_max, int default_value)
{
    wxBoxSizer *sizer_input = new wxBoxSizer(wxHORIZONTAL);
    sizer_input->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));
    auto input_title = new Label(parent, title, 0, wxSize(FromDIP(TITLE_WIDTH), -1));
    input_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    input_title->SetFont(::Label::Body_13);
    input_title->SetToolTip(tooltip);
    input_title->Wrap(-1);

    int value = default_value;
    const std::string stored = app_config->get(param);
    long parsed = 0;
    if (!stored.empty() && wxString::FromUTF8(stored).ToLong(&parsed))
        value = static_cast<int>(parsed);
    value = std::clamp(value, range_min, range_max);
    if (stored != std::to_string(value)) {
        app_config->set(param, std::to_string(value));
        app_config->save();
    }

    // 80 + 8 + 56 matches COMBOBOX_WIDTH so the row lines up with other controls.
    auto *slider = new ::Slider(parent, value, range_min, range_max, false, wxDefaultPosition,
                                wxSize(FromDIP(80), FromDIP(ITEM_MIN_HEIGHT)));
    slider->SetName(title); // screen-reader name (SliderAccessible reads GetName())
    auto *input = new ::TextInput(parent, wxString::Format("%d", value), wxEmptyString, wxEmptyString, wxDefaultPosition,
                                  wxSize(FromDIP(56), FromDIP(ITEM_MIN_HEIGHT)), wxTE_PROCESS_ENTER);
    StateColor input_bg(std::pair<wxColour, int>(ThemeColor::Grey250, StateColor::Disabled), std::pair<wxColour, int>(ThemeColor::White, StateColor::Enabled));
    input->SetBackgroundColor(input_bg);
    wxTextValidator validator(wxFILTER_DIGITS);
    input->GetTextCtrl()->SetValidator(validator);

    sizer_input->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    sizer_input->Add(input_title, wxSizerFlags().CenterVertical().Proportion(1).Border(wxRIGHT, FromDIP(8)));
    sizer_input->Add(slider, wxSizerFlags().CenterVertical());
    sizer_input->AddSpacer(FromDIP(8));
    sizer_input->Add(input, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(ITEM_RIGHT_PADDING)));

    auto updating = std::make_shared<bool>(false);
    auto commit = [this, param, slider, input, range_min, range_max, updating](int next) {
        if (*updating)
            return;
        next = std::clamp(next, range_min, range_max);
        *updating = true;
        if (slider->GetValue() != next)
            slider->SetValue(next);
        input->GetTextCtrl()->ChangeValue(wxString::Format("%d", next));
        *updating = false;
        app_config->set(param, std::to_string(next));
        app_config->save();
    };

    slider->SetOnChange([commit](int next) { commit(next); });
    input->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, [commit, input, slider](wxCommandEvent &e) {
        long entered = 0;
        if (!input->GetTextCtrl()->GetValue().ToLong(&entered))
            commit(slider->GetValue());
        else
            commit(static_cast<int>(entered));
        e.Skip();
    });
    input->GetTextCtrl()->Bind(wxEVT_KILL_FOCUS, [commit, input, slider](wxFocusEvent &e) {
        long entered = 0;
        if (!input->GetTextCtrl()->GetValue().ToLong(&entered))
            commit(slider->GetValue());
        else
            commit(static_cast<int>(entered));
        e.Skip();
    });

    return sizer_input;
}

wxBoxSizer *PreferencesDialog::create_item_range_input(
    wxString title, wxWindow *parent, wxString tooltip, std::string param, float range_min, float range_max, int keep_digital, std::function<void(wxString)> onchange)
{
    wxBoxSizer *sizer_input = new wxBoxSizer(wxHORIZONTAL);
    sizer_input->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));
    auto        input_title = new Label(parent, title);
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
    auto       input = new ::TextInput(parent, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(INPUT_WIDTH), FromDIP(ITEM_MIN_HEIGHT)), wxTE_PROCESS_ENTER);
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

    register_option_row(param, sizer_input);
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
    auto        input_title = new Label(parent, title);
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
    auto       input = new ::TextInput(parent, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(INPUT_WIDTH), FromDIP(ITEM_MIN_HEIGHT)), wxTE_PROCESS_ENTER);
    StateColor input_bg(std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHigh), StateColor::Disabled), std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHighest), StateColor::Enabled));
    input->SetBackgroundColor(input_bg);
    input->SetCornerRadius(FromDIP(10));
    input->GetTextCtrl()->SetFont(::Label::Mono_13);
    input->GetTextCtrl()->SetValue(app_config->get(param));
    wxTextValidator validator(wxFILTER_NUMERIC);
    input->GetTextCtrl()->SetValidator(validator);

    auto input1 = new ::TextInput(parent, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(INPUT_WIDTH), FromDIP(ITEM_MIN_HEIGHT)), wxTE_PROCESS_ENTER);
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

    register_option_row(param, sizer_input);
    register_option_row(param1, sizer_input);
    return sizer_input;
}

wxBoxSizer *PreferencesDialog::create_item_switch(wxString title, wxWindow *parent, wxString tooltip ,std::string param)
{
    wxBoxSizer *m_sizer_switch = new wxBoxSizer(wxHORIZONTAL);
    m_sizer_switch->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));
    auto        switch_title   = new Label(parent, title, 0, wxSize(FromDIP(TITLE_WIDTH), -1));
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
    register_option_row(param, m_sizer_switch);
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

// Bilingual label helpers for the funny-level rows. The source strings carry
// their own Cantonese entry in the LanguageMode copy table. In bilingual mode
// the pair goes to the bilingual registry and the label shows its English, so
// the bilingual decorator pairs it only where it fits, as it does every other
// label: on one line when the pair fits, English over Cantonese where the row
// wraps, in the tooltip otherwise. Rendered here as one "English · 廣東話"
// line, these rows ran past the page and pushed every row's control out of
// sight (clipping inventory CJ-021).
static wxString registered_english(const I18N::FormattedLocalizedText &text)
{
    if (text.has_secondary())
        I18N::BilingualRegistry::instance().record(text.primary(), text.secondary());
    return text.primary();
}

static wxString funny_row_label(const char *source)
{
    return registered_english(I18N::translate_mode(source).finalize_without_arguments());
}

static wxString funny_row_label_int(const char *source, int value)
{
    const I18N::LocalizedText copy = I18N::translate_mode(source);
    return registered_english(copy.format_each([value](wxString pattern) { return wxString::Format(pattern, value); }));
}

wxBoxSizer *PreferencesDialog::create_item_funny_level_slider(wxWindow *parent, std::string param, bool cantonese)
{
    const I18N::FunnyLanguage language = cantonese ? I18N::FunnyLanguage::Cantonese : I18N::FunnyLanguage::English;
    const wxString            title    = funny_row_label(cantonese ? "Funny level (Cantonese)" : "Funny level (English)");
    const int                 level    = I18N::parse_funny_level(app_config->get(param));
    // Provenance: the key is only written when the user moves the slider, so an
    // absent key genuinely means the compiled default is in effect.
    auto stored = std::make_shared<bool>(!app_config->get(param).empty());

    auto *row  = new wxBoxSizer(wxVERTICAL);
    auto *head = new wxBoxSizer(wxHORIZONTAL);
    head->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));

    auto *text_col = new wxBoxSizer(wxVERTICAL);
    auto *label    = new Label(parent, title);
    label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    label->SetFont(::Label::Body_13);
    label->Wrap(FromDIP(320));
    text_col->Add(label, 0);

    auto *value_label = new Label(parent, wxEmptyString);
    value_label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    value_label->SetFont(::Label::Body_12);
    text_col->Add(value_label, 0, wxTOP, FromDIP(2));

    auto *provenance = new Label(parent, wxEmptyString);
    provenance->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    provenance->SetFont(::Label::Body_12);
    text_col->Add(provenance, 0, wxTOP, FromDIP(2));

    auto *slider = new ::Slider(parent, level, I18N::FUNNY_LEVEL_MIN, I18N::FUNNY_LEVEL_MAX, false, wxDefaultPosition,
                                wxSize(FromDIP(200), -1));
    slider->SetName(title); // screen-reader name (SliderAccessible reads GetName())
    slider->SetToolTip(funny_row_label("1 = fully serious, 5 = maximum playfulness"));

    head->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    head->Add(text_col, wxSizerFlags().CenterVertical().Proportion(1));
    head->Add(slider, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(ITEM_RIGHT_PADDING)));
    row->Add(head, 0, wxEXPAND);

    // Progressive disclosure: the full explanation stays behind a Text button
    // (keyboard operable: Space / Enter) until the user asks for it.
    auto *details = new ::Button(parent, funny_row_label("What does this change?"));
    details->SetVariant(Button::Variant::Text);
    details->SetButtonSize(Button::Size::Small);
    auto *caption = new Label(parent, funny_row_label(
        "Sets the tone of every message Bambu Studio shows in this language, including errors, warnings and destructive confirmations. It never changes what a message says has happened or what will be affected."));
    caption->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    caption->SetFont(::Label::Body_12);
    caption->Wrap(FromDIP(480));
    caption->Hide();
    row->Add(details, 0, wxLEFT, FromDIP(ITEM_LEFT_PADDING - 12));
    row->Add(caption, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(ITEM_LEFT_PADDING));

    details->Bind(wxEVT_BUTTON, [parent, details, caption](wxCommandEvent &) {
        const bool show = !caption->IsShown();
        caption->Show(show);
        details->SetLabel(funny_row_label(show ? "Hide details" : "What does this change?"));
        parent->Layout();
        if (auto *scrolled = dynamic_cast<wxScrolledWindow *>(parent))
            scrolled->FitInside();
    });

    // Both lines are wrapped like every other Preferences description, again after
    // each new text: a bilingual pair that does not fit one line then goes English
    // over Cantonese instead of leaving the Cantonese in a tooltip ("Not stored
    // yet; ... · 未儲存；...", about 430 px, is wider than the column).
    auto refresh = [value_label, provenance, stored](int value) {
        value_label->SetLabel(funny_row_label_int("Level %d of 5", value));
        value_label->Wrap(value_label->FromDIP(320));
        provenance->SetLabel(*stored ? funny_row_label_int("Stored in BambuStudio.conf as %d.", value)
                                     : funny_row_label_int("Not stored yet; using the compiled default %d.",
                                                           I18N::FUNNY_LEVEL_DEFAULT));
        provenance->Wrap(provenance->FromDIP(320));
    };
    refresh(level);

    slider->SetOnChange([this, param, language, stored, refresh](int value) {
        const int clamped = I18N::clamp_funny_level(value);
        app_config->set(param, std::to_string(clamped));
        app_config->save();
        I18N::language_mode_service().set_funny_level(language, clamped);
        *stored = true;
        refresh(clamped);
    });

    // Teleport target (command palette, Every feature page).
    register_option_row(param, row);
    return row;
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
    checkbox->SetName(title);
    m_checkbox_list[m_checkbox_list.size()] = checkbox;
    if (param == "privacyuse") {
        checkbox->SetValue((app_config->get("firstguide", param) == "true") ? true : false);
    } else if (param == "auto_stop_liveview") {
        checkbox->SetValue(app_config->get("liveview", "keep_liveview") != "false");
    } else {
        checkbox->SetValue((app_config->get(param) == "true") ? true : false);
    }

    // Two-line label column: primary 13.5/OnSurface over an optional secondary
    // description (12/OnSurfaceVariant) sourced from the tooltip when it adds
    // information beyond the primary label.
    auto *text_col = new wxBoxSizer(wxVERTICAL);
    auto  checkbox_title = new Label(parent, title);
    checkbox_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    checkbox_title->SetFont(::Label::Body_13);
    checkbox_title->Wrap(FromDIP(320));
    // Right-click on a setting row -> "Edit appearance..." for that row; the
    // id is the AppConfig key so the override follows the setting, not its
    // position ("preferences.row/<param>" inherits from "preferences.row").
    ElementStyle::apply(checkbox_title, "preferences.row/" + param, title);
    text_col->Add(checkbox_title, 0);
    if (!tooltip.empty() && tooltip != title) {
        auto *checkbox_desc = new Label(parent, tooltip);
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
            app_config->set("liveview", "keep_liveview", checkbox->GetValue());
            app_config->save();
        }
        else {
            app_config->set_bool(param, checkbox->GetValue());
            app_config->save();
        }

        if (param == ActionSounds::enabled_key || param.rfind("narrator_", 0) == 0)
            ActionSounds::settings_changed();

        if (param == I18N::DIALOG_EMOJIS_KEY)
            I18N::language_mode_service().set_dialog_emojis(checkbox->GetValue());

        if (param == "show_bed_heat_soak_area" && wxGetApp().plater())
            wxGetApp().plater()->on_show_bed_heat_soak_area_changed();

        if (param == "staff_pick_switch") {
            bool pbool = app_config->get("staff_pick_switch") == "true";
            wxGetApp().switch_staff_pick(pbool);
        }

        // LAN model drop: start or stop polling at once. The invite right under the switch is
        // the first thing shown once it is on.
        if (param == LanModelDrop::kEnabledConfigKey)
            LanModelDrop::apply_settings();

        // Local status API: start or stop the loopback listener at once.
        if (param == StatusApi::kEnabledConfigKey)
            StatusApi::apply_settings();

        if (param == "sync_user_preset") {
            bool sync = app_config->get("sync_user_preset") == "true" ? true : false;
            if (sync) {
                wxGetApp().start_sync_user_preset();
            } else {
                wxGetApp().stop_sync_user_preset();
            }
            BOOST_LOG_TRIVIAL(info) << "sync_user_preset: " << (sync ? "true" : "false");
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

        e.Skip();
    });

    //// for debug mode
    if (param == "developer_mode") { m_developer_mode_ckeckbox = checkbox; }
    if (param == "internal_developer_mode") { m_internal_developer_mode_ckeckbox = checkbox; }
    if (param == "privacyuse") { m_privacy_checkbox = checkbox; }


    checkbox->SetToolTip(tooltip);
    register_option_row(param, m_sizer_checkbox);
    if (param == "privacyuse" && wxGetApp().is_uep_force_disabled()) {
        checkbox->SetValue(wxGetApp().is_uep_force_checked());
        checkbox->Enable(false);
        checkbox_title->SetToolTip(minor_restricted_tip());
    }
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

    auto m_staticTextTitle = new Label(item_panel, _L("Download path"));
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

    register_option_row(param, nullptr, item_panel);
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

    auto m_staticTextTitle = new Label(item_panel, _L("External editor path"));
    m_staticTextTitle->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    m_staticTextTitle->SetFont(::Label::Body_13);
    m_staticTextTitle->Wrap(-1);

    auto m_staticTextPath = new ::TextInput(item_panel, editor_path, wxEmptyString, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    m_staticTextPath->SetCornerRadius(FromDIP(4));
    m_staticTextPath->GetTextCtrl()->SetFont(::Label::Body_13);

    // MD3 outlined button, matching the Download-path Browse button above.
    auto m_button_browse = new Button(item_panel, _L("Browse"));
    m_button_browse->SetName(_L("External editor browse"));
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

    register_option_row(param, nullptr, item_panel);
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

    wxStaticText *text = new Label(parent, title);

    radiobox->SetToolTip(tooltip);
    text->SetToolTip(tooltip);

    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    sizer->SetMinSize(wxSize(-1, FromDIP(ITEM_MIN_HEIGHT)));
    sizer->Add(text, wxSizerFlags().CenterVertical().Proportion(1));
    sizer->Add(radiobox, wxSizerFlags().CenterVertical().Border(wxRIGHT, ITEM_RIGHT_PADDING));
    register_option_row(param, sizer);
    return sizer;
}

PreferencesDialog::PreferencesDialog(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint &pos, const wxSize &size, long style)
    : DPIDialog(parent, id, _L("Preferences"), pos, size, style | wxRESIZE_BORDER)
{
    // Root dialog surface (kit Settings root = Surface); resolves by role in dark.
    SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    m_original_use_12h_time_format = wxGetApp().app_config->get("use_12h_time_format");
    // Sync the MD3 density/accent token state to the persisted Appearance choices
    // before the tabs are built so this dialog and later-constructed surfaces
    // resolve the saved density metrics and accent roles.
    apply_persisted_md3_appearance();
    create();
    wxGetApp().UpdateDlgDarkUI(this);
    MD3DialogCaption::FinishChrome(this);
    restore_geometry();
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
        } catch(...) {
            BOOST_LOG_TRIVIAL(warning) << current_exception_message();
        }

        // Check if time format changed
        std::string current_use_12h_time_format = wxGetApp().app_config->get("use_12h_time_format");
        m_use_12h_time_format_changed = (m_original_use_12h_time_format != current_use_12h_time_format);

        event.Skip();
        });

    // Register so an async UEP decision update can refresh the checkbox live, and
    // re-fetch the UEP decision on open to keep telemetry enable/disable in sync.
    wxGetApp().set_preferences_dialog(this);
    wxGetApp().fetch_uep_decision();

    // Preferences is modal, so a wording file dropped on it never reaches the
    // main window. It takes the same drop itself: one .json applies personal
    // wording through the shared validator, anything else is refused.
    SetDropTarget(VocabularyDrop::create_drop_target(this));
}

void PreferencesDialog::place_settings_strip()
{
    if (!m_body_row || !m_tabbar)
        return;
    m_body_row->Detach(m_tabbar);
    using MD3::Tabs::DockEdge;
    const DockEdge edge = m_tabbar->GetDockEdge();
    m_body_row->SetOrientation(MD3::Tabs::is_vertical(edge) ? wxHORIZONTAL : wxVERTICAL);
    if (edge == DockEdge::Left || edge == DockEdge::Top)
        m_body_row->Insert(0, m_tabbar, 0, wxEXPAND);
    else
        m_body_row->Add(m_tabbar, 0, wxEXPAND);
    m_body_row->Layout();
}

int PreferencesDialog::page_for_id(const std::string &id) const
{
    for (size_t i = 0; i < m_page_ids.size(); ++i)
        if (m_page_ids[i] == id)
            return int(i);
    return -1;
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

    auto main_sizer = new wxBoxSizer(wxVERTICAL);
    main_sizer->Add(new MD3DialogCaption(this, _L("Preferences")), 0, wxEXPAND);

    TabStrip::Options strip_opts;
    strip_opts.surface_key     = "preferences";
    strip_opts.surface_name    = _L("Preferences");
    strip_opts.strip_name      = _L("Settings sections");
    strip_opts.default_edge    = MD3::Tabs::DockEdge::Left;
    strip_opts.close_mode      = TabStrip::CloseMode::Close; // "close" hides a section; restore from the overflow menu
    strip_opts.show_new_button = true;
    m_tabbar = new TabStrip(this, strip_opts);
    auto *content_card = new StaticBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    content_card->SetBackgroundColor(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    content_card->SetBorderColor(StateColor::semantic(MD3::Role::OutlineVariant));
    content_card->SetDensity(MD3::Metrics::isCompact() ? StaticBox::Density::Compact : StaticBox::Density::Comfortable);
    m_book   = new wxSimplebook(content_card, wxID_ANY);
    // Pages own their scrolling; their content must not dictate the frame size.
    m_book->SetMinSize(FromDIP(wxSize(1, 1)));
    content_card->SetMinSize(FromDIP(wxSize(1, 1)));
    m_book->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));

    // Right-hand content pane: a top MD3 SearchField pill over the section book.
    auto *content_pane = new wxBoxSizer(wxVERTICAL);
    m_search = new SearchField(content_card, _L("Search settings"));
    auto *search_row = new wxBoxSizer(wxHORIZONTAL);
    search_row->Add(m_search, 1, wxALIGN_CENTER_VERTICAL);
    m_changed_chip = new ::Button(content_card, _L("Changed by you"));
    m_changed_chip->SetVariant(::Button::Variant::Outlined);
    m_changed_chip->SetButtonSize(::Button::Size::Small);
    m_changed_chip->SetToolTip(_L("Show only the settings whose value differs from the built-in default"));
    m_changed_chip->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        m_changed_only = !m_changed_only;
        refresh_changed_chip();
        apply_search_filter(m_search->GetValue());
    });
    search_row->Add(m_changed_chip, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
    content_pane->Add(search_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(16));
    // Inline "no results" hint under the search pill; hidden until an active
    // query matches nothing (see apply_search_filter).
    m_search_empty_hint = new Label(content_card, _L("No settings match your search."));
    m_search_empty_hint->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    m_search_empty_hint->SetFont(::Label::Body_13);
    m_search_empty_hint->Hide();
    content_pane->Add(m_search_empty_hint, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(16));
    content_pane->Add(m_book, 1, wxEXPAND | wxTOP, FromDIP(12));

    // Section strip + content pane. The strip's dock edge decides the row's
    // orientation and which side the strip sits on (see place_settings_strip).
    m_body_row = new wxBoxSizer(wxHORIZONTAL);
    content_card->SetSizer(content_pane);
    m_body_row->Add(content_card, 1, wxEXPAND | wxALL, FromDIP(MD3::Metrics::active().gap));
    // The explanation panel opened by each row's info button.
    m_info_panel = new wxPanel(this, wxID_ANY);
    m_info_panel->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainer));
    m_info_panel->SetMinSize(wxSize(FromDIP(320), -1));
    m_info_panel->SetName(_L("About this setting"));
    m_info_sizer = new wxBoxSizer(wxVERTICAL);
    m_info_panel->SetSizer(m_info_sizer);
    m_info_panel->Hide();
    m_body_row->Add(m_info_panel, 0, wxEXPAND | wxTOP | wxBOTTOM | wxRIGHT, FromDIP(MD3::Metrics::active().gap));

    auto add_tab = [this](const std::string &id, const wxString &label, wxWindow *page) {
        m_page_ids.push_back(id);
        m_book->AddPage(page, label);
        m_tabbar->AddTab(id, label);
    };
    // Sections are stable ids so the persisted strip layout (order / pins /
    // groups / hidden / dock edge) survives relabelling and reordering in code.
    add_tab("appearance", _L("Appearance"), create_appearance_tab());
    add_tab("schedules", _L("Schedules"), create_schedules_tab());
    add_tab("general", _CTX(L_CONTEXT("General", "Preference"), "Preference"), create_general_tab());
    add_tab("user", _CTX(L_CONTEXT("User", "Preference"), "Preference"), create_user_tab());
    add_tab("3d", _CTX(L_CONTEXT("3D", "Preference"), "Preference"), create_3d_tab());
    add_tab("other", _CTX(L_CONTEXT("Other", "Preference"), "Preference"), create_other_tab());
    // A section of its own, never behind an advanced toggle (docs/features/application-integration/lan-model-drop.md).
    add_tab("lan_drop", _L("LAN model drop"), create_lan_drop_tab());
    // Off by default; local and read-only (docs/features/device/status-api.md).
    add_tab("status_api", _L("Local status API"), create_status_api_tab());
    // Interface accommodations, each off by default (docs/features/windows/attention-modes.md).
    add_tab("attention", _L("Attention modes"), create_attention_tab());

#if !BBL_RELEASE_TO_PUBLIC
    add_tab("developer", _L("Developer Tools"), create_developer_tab());
#endif

    auto *draft_panel = new SettingsDraftPanel(m_book, m_tabbar, [this](bool draft) {
        if (draft) m_book->SetSelection(m_book->GetPageCount() - 1);
        else if (m_book->GetSelection() == int(m_book->GetPageCount() - 1)) m_book->SetSelection(0);
    });
    m_book->AddPage(draft_panel, _L("Settings draft"));
    draft_panel->Show();

    // Apply the saved layout, then show whichever section the strip made
    // active (the saved one, or the first displayed section).
    m_tabbar->LoadLayout();
    {
        const int page = page_for_id(m_tabbar->ActiveId());
        m_book->SetSelection(page < 0 ? 0 : page);
        if (page < 0 && !m_page_ids.empty())
            m_tabbar->Activate(m_page_ids[0], /*emit*/ false);
    }
    m_tabbar->Bind(EVT_TABSTRIP_ACTIVATE, [this, draft_panel](wxCommandEvent &e) {
        if (draft_panel->Activate(std::string(e.GetString().ToUTF8()))) return;
        const int page = page_for_id(std::string(e.GetString().ToUTF8()));
        if (page >= 0 && page != m_book->GetSelection())
            m_book->SetSelection(page);
    });
    m_tabbar->Bind(EVT_TABSTRIP_DOCK_CHANGED, [this](wxCommandEvent &) {
        place_settings_strip();
        Layout();
    });
    place_settings_strip();

    // Wire the search pill to live row filtering across every section. The row
    // index is built once here, after all pages and their gates (e.g. the
    // model-mall visibility toggle) have settled.
    build_search_index();
    m_search->SetOnQuery([this](const wxString &query) { apply_search_filter(query); });
    // Re-run the live filter when the regex / case / whole-word toggle changes so
    // the visible rows reflect the new matching mode without a fresh keystroke.
    // Passing the raw field value keeps the empty-query reset path intact.
    m_search->SetOnRegexToggle([this](bool) { apply_search_filter(m_search->GetValue()); });
    start_change_tracking();

    main_sizer->Add(m_body_row, 1, wxEXPAND);
    main_sizer->Add(create_bottom_buttons(), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

    SetSizer(main_sizer);
    Layout();
    PreferencesGeometryProbe::bind(this, content_card, m_book, m_search, m_tabbar);
}

// Preferences geometry is independent of page content and stored in DIP.
// Only size is restored: opening follows the parent instead of a removed monitor.
void PreferencesDialog::restore_geometry()
{
    const int width = PreferencesGeometry::restored_dimension(app_config->get("preferences_width"), 780);
    const int height = PreferencesGeometry::restored_dimension(app_config->get("preferences_height"), 640);
    SetSize(FromDIP(wxSize(width, height)));
    CenterOnParent();
    constrain_geometry(GetRect());
}

void PreferencesDialog::constrain_geometry(const wxRect &requested)
{
    const wxPoint centre(requested.x + requested.width / 2, requested.y + requested.height / 2);
    int display_index = wxDisplay::GetFromPoint(centre);
    if (display_index == wxNOT_FOUND && GetParent()) display_index = wxDisplay::GetFromWindow(GetParent());
    if (display_index == wxNOT_FOUND) display_index = wxDisplay::GetFromWindow(this);
    const wxRect work = display_index == wxNOT_FOUND ? wxGetClientDisplayRect() : wxDisplay(display_index).GetClientArea();
    const wxSize preferred = FromDIP(wxSize(640, 480));
    const auto low = PreferencesGeometry::minimum({preferred.x, preferred.y}, {work.width, work.height});
    const auto bounded = PreferencesGeometry::bounded({requested.x, requested.y, requested.width, requested.height},
        {work.x, work.y, work.width, work.height}, {preferred.x, preferred.y});
    SetMinSize(wxSize(low.width, low.height));
    SetSize(bounded.x, bounded.y, bounded.width, bounded.height);
    Layout();
}

void PreferencesDialog::save_geometry()
{
    if (!app_config || IsIconized() || IsMaximized()) return;
    const wxSize size = ToDIP(GetSize());
    if (size.x <= 0 || size.y <= 0) return;
    app_config->set("preferences_width", std::to_string(size.x));
    app_config->set("preferences_height", std::to_string(size.y));
    app_config->save();
}

// ============================================================================
//  Live settings search — SearchField-driven row filtering across all pages.
// ============================================================================

// Collect every wxStaticText descendant of a window (row labels, descriptions,
// link labels — ::Label derives from wxStaticText) for the search index.
static void collect_search_labels_from_window(wxWindow *win, std::vector<wxStaticText *> &labels)
{
    if (win == nullptr) return;
    // Provenance lines change with the value; searching them would match
    // "default" on every row.
    if (win->GetName() == "preferences-provenance") return;
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
    const auto *native_label = dynamic_cast<const ::Label *>(label);
    wxString text = native_label ? native_label->GetUnwrappedLabel() : label->GetLabelText();
    text.Replace("\n", " ");
    return text;
}

void PreferencesDialog::build_search_index()
{
    m_search_rows.clear();
    if (m_book == nullptr) return;

    std::vector<PreferencesSearch::Registration> registrations;
    for (const OptionRow &opt : m_option_rows)
        registrations.push_back({{opt.sizer, opt.window}, opt.key});
    const auto child_items = [](wxSizer *owner) {
        std::vector<wxSizerItem *> items;
        if (owner)
            for (auto *item : owner->GetChildren())
                if (item && !item->IsSpacer()) items.push_back(item);
        return items;
    };
    const auto identity_of = [](wxSizerItem *item) {
        return PreferencesSearch::Identity{item->IsSizer() ? item->GetSizer() : nullptr,
                                           item->IsWindow() ? item->GetWindow() : nullptr};
    };
    const auto children_of = [&child_items](wxSizerItem *item) {
        return child_items(item->IsSizer() ? item->GetSizer() :
                           item->IsWindow() ? item->GetWindow()->GetSizer() : nullptr);
    };
    for (size_t page = 0; page < m_book->GetPageCount(); ++page) {
        wxWindow *page_win = m_book->GetPage(page);
        wxSizer  *sizer    = page_win ? page_win->GetSizer() : nullptr;
        if (sizer == nullptr) continue;

        const auto indexed = PreferencesSearch::collect_rows(child_items(sizer), registrations,
            identity_of, children_of, [](wxSizerItem *item) { return item->IsShown(); });
        for (const auto &indexed_row : indexed) {
            auto *item = indexed_row.item;

            SearchRow row;
            row.page           = int(page);
            row.item           = item;
            row.baseline_shown = indexed_row.baseline_shown;
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
            // Fold the create_item_* key registry into the row: a palette
            // teleport looks rows up by AppConfig key.
            row.keys = indexed_row.keys;
            m_search_rows.push_back(std::move(row));
        }
    }
}

void PreferencesDialog::register_option_row(const std::string &key, wxSizer *sizer, wxWindow *window)
{
    if (key.empty() || (sizer == nullptr && window == nullptr)) return;
    m_option_rows.push_back({key, sizer, window});
    if (sizer != nullptr) decorate_row(key, sizer);
}

// ---------------------------------------------------------------------------
// Where every value comes from
// ---------------------------------------------------------------------------

namespace {

wxString display_value(const std::string &key, const std::string &value)
{
    if (PreferenceHistory::is_sensitive_key(key)) return _L("hidden");
    if (value == "true") return _L("On");
    if (value == "false") return _L("Off");
    if (value.empty()) return _L("empty");
    wxString shown = wxString::FromUTF8(value);
    if (shown.length() > 40) shown = shown.Left(37) + wxString(L"…");
    return shown;
}

wxString format_day(std::int64_t time) { return wxDateTime(time_t(time)).Format("%Y-%m-%d"); }
wxString format_clock(std::int64_t time) { return wxDateTime(time_t(time)).Format("%H:%M:%S"); }

wxString takes_effect_text(const std::string &key)
{
    static const std::set<std::string> restart = {"language", "enable_lod", "single_instance"};
    static const std::set<std::string> live    = {"dark_color_mode", "ui_density", "ui_accent_seed", "ui_font_family", "ui_font_scale",
                                                  "motion_preference", "action_sounds_enabled", "dialog_emojis", "funny_level_en",
                                                  "funny_level_yue", "app_display_name", "app_logo", "lan_drop_enabled",
                                                  "status_api_enabled", "staff_pick_switch", "show_print_history", "sync_user_preset"};
    if (restart.count(key)) return _L("After a restart.");
    if (live.count(key) || key.rfind("narrator_", 0) == 0) return _L("Right away, in every window.");
    return _L("The next time the feature that uses it runs.");
}

::SwitchButton *find_switch(wxSizer *sizer)
{
    if (sizer == nullptr) return nullptr;
    for (auto *item : sizer->GetChildren()) {
        if (item->IsWindow())
            if (auto *sw = dynamic_cast<::SwitchButton *>(item->GetWindow())) return sw;
        if (item->IsSizer())
            if (auto *sw = find_switch(item->GetSizer())) return sw;
    }
    return nullptr;
}

} // namespace

void PreferencesDialog::decorate_row(const std::string &key, wxSizer *row)
{
    if (row == nullptr) return;
    if (!m_decorated_rows.insert(row).second) {
        // A second key on a row that is already decorated: explain it too, but
        // leave the single provenance line to the first key.
        for (const RowInfo &info : m_row_info)
            if (info.row == row) {
                RowInfo extra    = info;
                extra.key        = key;
                extra.provenance = nullptr;
                m_row_info.push_back(std::move(extra));
                return;
            }
        return;
    }
    // The title column: the first vertical box sizer in the row. Rows built
    // another way (a whole panel, a picker) keep their own UI and are not
    // decorated; they stay teleport targets all the same.
    wxBoxSizer *text_col = nullptr;
    for (auto *item : row->GetChildren())
        if (item->IsSizer())
            if (auto *box = dynamic_cast<wxBoxSizer *>(item->GetSizer()); box && box->GetOrientation() == wxVERTICAL) {
                text_col = box;
                break;
            }
    if (text_col == nullptr) return;
    std::vector<wxStaticText *> labels;
    collect_search_labels_from_sizer(text_col, labels);
    if (labels.empty()) return; // nothing to explain
    // New controls must be children of the window that owns the row sizer:
    // the parent of the row's own windows (the sizer is not attached yet).
    wxWindow *parent = labels.front()->GetParent();
    for (auto *item : row->GetChildren())
        if (item->IsWindow()) {
            parent = item->GetWindow()->GetParent();
            break;
        }

    RowInfo info;
    info.key   = key;
    info.row   = row;
    info.title = search_label_text(labels.front());
    if (labels.size() > 1) info.description = search_label_text(labels[1]);

    auto *line = new ::Label(parent, wxEmptyString);
    line->SetName("preferences-provenance");
    line->SetFont(::Label::Body_12);
    line->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    text_col->Add(line, 0, wxTOP, FromDIP(2));
    info.provenance = line;

    auto *about = new ::Button(parent, wxEmptyString);
    about->SetIconButton(::Button::IconShape::Circle, 32);
    about->SetGlyph(MaterialIcon::Info);
    about->SetName(wxString::Format(_L("About %s"), info.title));
    about->SetToolTip(_L("What this setting does and where its value comes from"));
    about->Bind(wxEVT_BUTTON, [this, key](wxCommandEvent &) { show_info(key); });
    row->Add(about, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));

    m_row_info.push_back(std::move(info));
}

const PreferencesDialog::RowInfo *PreferencesDialog::row_info(const std::string &key) const
{
    for (const RowInfo &info : m_row_info)
        if (info.key == key) return &info;
    return nullptr;
}

std::optional<std::string> PreferencesDialog::stored_value(const std::string &key) const
{
    std::string value;
    if (app_config == nullptr || !app_config->get_base("app", key, value)) return std::nullopt;
    return value;
}

std::optional<std::string> PreferencesDialog::default_value(const std::string &key) const
{
    // A configuration built only from AppConfig::set_defaults() holds every
    // built-in default the app declares.
    static const AppConfig defaults;
    std::string value;
    if (!defaults.get_base("app", key, value)) return std::nullopt;
    return value;
}

bool PreferencesDialog::key_changed(const std::string &key) const
{
    return PreferenceHistory::differs_from_default(stored_value(key), default_value(key));
}

bool PreferencesDialog::row_changed(const SearchRow &row) const
{
    for (const std::string &key : row.keys)
        if (key_changed(key)) return true;
    return false;
}

wxString PreferencesDialog::provenance_text(const std::string &key) const
{
    std::optional<std::string> scheduled;
    if (app_config != nullptr && app_config->has_effective_preference(key)) scheduled = app_config->get(key);
    std::optional<PreferenceHistory::Change> last;
    if (m_history) last = m_history->last_for_key(key);
    const PreferenceHistory::Provenance p = PreferenceHistory::describe(stored_value(key), default_value(key), scheduled, last);
    const bool secret = PreferenceHistory::is_sensitive_key(key);

    using PreferenceHistory::Origin;
    if (p.origin == Origin::ChangedByYou)
        return secret ? wxString::Format(_L("You changed this on %s at %s."), format_day(p.changed_at), format_clock(p.changed_at))
                      : wxString::Format(_L("You set %s on %s at %s."), display_value(key, p.value), format_day(p.changed_at),
                                         format_clock(p.changed_at));
    if (secret) return _L("This value is private, so it is not shown here.");
    switch (p.origin) {
    case Origin::Scheduled:
        return wxString::Format(_L("Scheduled settings set this to %s for now."), display_value(key, p.value));
    case Origin::BuiltInDefault:
        return wxString::Format(_L("Built-in default: %s."), display_value(key, p.value));
    case Origin::MatchesDefault:
        return wxString::Format(_L("Built-in default: %s. You have not changed it."), display_value(key, p.value));
    case Origin::Stored:
        return p.default_value ? wxString::Format(_L("Stored as %s. The built-in default is %s."), display_value(key, p.value),
                                                  display_value(key, *p.default_value)) :
                                 wxString::Format(_L("Stored as %s."), display_value(key, p.value));
    case Origin::NoDefault:
    case Origin::ChangedByYou: break;
    }
    return _L("Not stored yet; the app uses its built-in behaviour.");
}

void PreferencesDialog::refresh_provenance()
{
    for (const RowInfo &info : m_row_info) {
        if (info.provenance == nullptr) continue;
        info.provenance->SetLabel(provenance_text(info.key));
        info.provenance->Wrap(FromDIP(320));
    }
    refresh_changed_chip();
}

void PreferencesDialog::refresh_changed_chip()
{
    if (m_changed_chip == nullptr) return;
    std::set<std::string> changed;
    for (const RowInfo &info : m_row_info)
        if (key_changed(info.key)) changed.insert(info.key);
    m_changed_chip->SetLabel(wxString::Format(_L("Changed by you %d"), int(changed.size())));
    m_changed_chip->SetVariant(m_changed_only ? ::Button::Variant::Tonal : ::Button::Variant::Outlined);
    m_changed_chip->Refresh();
}

void PreferencesDialog::start_change_tracking()
{
    m_history = std::make_unique<PreferenceHistory::Store>(std::filesystem::u8path(data_dir()) / "preferences-history.jsonl");
    m_history->load();
    for (const RowInfo &info : m_row_info) m_seen_values[info.key] = stored_value(info.key).value_or(std::string());
    refresh_provenance();
    const int timer_id = wxWindow::NewControlId();
    m_change_timer.SetOwner(this, timer_id);
    Bind(wxEVT_TIMER, [this](wxTimerEvent &) { poll_changes(); }, timer_id);
    m_change_timer.Start(700);
}

void PreferencesDialog::poll_changes()
{
    std::map<std::string, std::string> now;
    for (const auto &entry : m_seen_values) now[entry.first] = stored_value(entry.first).value_or(std::string());
    const std::vector<PreferenceHistory::Change> changes = PreferenceHistory::diff(m_seen_values, now, std::int64_t(std::time(nullptr)));
    if (changes.empty()) return;
    if (m_history && !m_history->append(changes))
        BOOST_LOG_TRIVIAL(warning) << "Preferences: the change record could not be written";
    m_seen_values = std::move(now);
    refresh_provenance();
    if (!m_info_key.empty()) show_info(m_info_key);
    Layout();
}

void PreferencesDialog::hide_info()
{
    m_info_key.clear();
    if (m_info_panel == nullptr || !m_info_panel->IsShown()) return;
    m_info_panel->Hide();
    Layout();
}

void PreferencesDialog::show_info(const std::string &key)
{
    const RowInfo *info = row_info(key);
    if (info == nullptr || m_info_panel == nullptr || m_info_sizer == nullptr) return;
    m_info_key = key;
    const int pad   = FromDIP(16);
    const int wrap  = FromDIP(288);
    const auto text = [&](const wxString &value, const wxFont &font, MD3::Role role, int top) {
        auto *label = new ::Label(m_info_panel, value);
        label->SetFont(font);
        label->SetForegroundColour(StateColor::semantic(role));
        label->Wrap(wrap);
        m_info_sizer->Add(label, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, top);
        return label;
    };
    const auto section = [&](const wxString &heading) { text(heading.Upper(), ::Label::Head_12, MD3::Role::OnSurfaceVariant, FromDIP(14)); };

    m_info_panel->Freeze();
    m_info_sizer->Clear(true);

    auto *head  = new wxBoxSizer(wxHORIZONTAL);
    auto *title = new ::Label(m_info_panel, info->title);
    title->SetFont(::Label::Head_16);
    title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    title->Wrap(FromDIP(240));
    head->Add(title, 1, wxALIGN_CENTER_VERTICAL);
    auto *close = new ::Button(m_info_panel, wxEmptyString);
    close->SetIconButton(::Button::IconShape::Circle, 32);
    close->SetGlyph(MaterialIcon::Close);
    close->SetName(_L("Close the explanation"));
    close->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { hide_info(); });
    head->Add(close, 0, wxALIGN_CENTER_VERTICAL);
    m_info_sizer->Add(head, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    if (!info->description.IsEmpty()) text(info->description, ::Label::Body_13, MD3::Role::OnSurface, FromDIP(8));

    section(_L("Where the value comes from"));
    text(provenance_text(key), ::Label::Body_13, MD3::Role::OnSurface, FromDIP(4));

    const std::optional<std::string> def = default_value(key);
    section(_L("Built-in default"));
    text(def ? display_value(key, *def) : _L("Not declared for this setting"), ::Label::Body_13, MD3::Role::OnSurface, FromDIP(4));

    section(_L("Takes effect"));
    text(takes_effect_text(key), ::Label::Body_13, MD3::Role::OnSurface, FromDIP(4));

    section(_L("Stored in"));
    text(wxString("BambuStudio.conf ") + wxString(L"·") + " " + wxString::FromUTF8(key), ::Label::Body_12,
         MD3::Role::OnSurfaceVariant, FromDIP(4));

    section(_L("Changes to this setting"));
    const std::vector<PreferenceHistory::Change> changes = m_history ? m_history->for_key(key, 5) : std::vector<PreferenceHistory::Change>{};
    if (changes.empty())
        text(_L("None yet. Every change you make here is recorded with its time, on this computer only."), ::Label::Body_13,
             MD3::Role::OnSurfaceVariant, FromDIP(4));
    for (const PreferenceHistory::Change &c : changes) {
        const wxString what = c.redacted ? _L("changed") :
                                           wxString::Format(_L("%s to %s"), display_value(key, c.from), display_value(key, c.to));
        text(format_day(c.time) + " " + format_clock(c.time) + "  " + what, ::Label::Body_12, MD3::Role::OnSurface, FromDIP(4));
    }

    // Reset drives the row's own switch, so its usual side effects run too.
    // Rows without a switch say how to change them back instead of pretending.
    if (def && key_changed(key)) {
        ::SwitchButton *sw = find_switch(info->row);
        if (sw != nullptr && (*def == "true" || *def == "false")) {
            auto *reset = new ::Button(m_info_panel, wxString::Format(_L("Reset to %s"), display_value(key, *def)));
            reset->SetVariant(::Button::Variant::Outlined);
            reset->SetButtonSize(::Button::Size::Small);
            const bool target = *def == "true";
            reset->Bind(wxEVT_BUTTON, [this, sw, target](wxCommandEvent &) {
                sw->SetValue(target);
                wxCommandEvent toggled(wxEVT_TOGGLEBUTTON, sw->GetId());
                toggled.SetEventObject(sw);
                toggled.SetInt(target ? 1 : 0);
                sw->GetEventHandler()->ProcessEvent(toggled);
                CallAfter([this]() { poll_changes(); });
            });
            m_info_sizer->Add(reset, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(16));
        } else {
            text(_L("Change it back with its own control; the built-in default is shown above."), ::Label::Body_12,
                 MD3::Role::OnSurfaceVariant, FromDIP(12));
        }
    }
    m_info_sizer->AddSpacer(pad);

    m_info_panel->Show();
    m_info_panel->Layout();
    m_info_panel->Thaw();
    Layout();
}

void PreferencesDialog::show_history()
{
    wxDialog dialog(this, wxID_ANY, _L("Preferences history"), wxDefaultPosition, wxDefaultSize,
                    wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    dialog.SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerHigh));
    auto *root    = new wxBoxSizer(wxVERTICAL);
    auto *heading = new ::Label(&dialog, _L("Preferences history"));
    heading->SetFont(::Label::Head_16);
    heading->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    root->Add(heading, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(16));
    auto *note = new ::Label(&dialog, _L("Every change made in Preferences, newest first. It stays on this computer; private values are never recorded."));
    note->SetFont(::Label::Body_12);
    note->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    note->Wrap(FromDIP(520));
    root->Add(note, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(8));

    auto *list = new MD3ScrolledWindow(&dialog, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(560), FromDIP(360)), wxVSCROLL | wxBORDER_NONE);
    list->SetBackgroundColour(dialog.GetBackgroundColour());
    list->SetScrollRate(0, FromDIP(20));
    auto *rows = new wxBoxSizer(wxVERTICAL);
    const std::vector<PreferenceHistory::Change> changes = m_history ? m_history->newest(200) : std::vector<PreferenceHistory::Change>{};
    if (changes.empty()) {
        auto *empty = new ::Label(list, _L("No changes recorded yet."));
        empty->SetFont(::Label::Body_13);
        empty->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        rows->Add(empty, 0, wxALL, FromDIP(8));
    }
    for (const PreferenceHistory::Change &c : changes) {
        const RowInfo *info  = row_info(c.key);
        const wxString title = info ? info->title : wxString::FromUTF8(c.key);
        const wxString what  = c.redacted ? _L("changed") :
                                            wxString::Format(_L("%s to %s"), display_value(c.key, c.from), display_value(c.key, c.to));
        auto *line = new ::Label(list, format_day(c.time) + " " + format_clock(c.time) + "   " + title + ": " + what);
        line->SetFont(::Label::Body_13);
        line->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
        rows->Add(line, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(6));
    }
    list->SetSizer(rows);
    list->FitInside();
    root->Add(list, 1, wxEXPAND | wxALL, FromDIP(12));

    auto *done = new ::Button(&dialog, _L("Close"));
    done->SetVariant(::Button::Variant::Filled);
    done->SetButtonSize(::Button::Size::Small);
    done->Bind(wxEVT_BUTTON, [&dialog](wxCommandEvent &) { dialog.EndModal(wxID_OK); });
    root->Add(done, 0, wxALIGN_RIGHT | wxRIGHT | wxBOTTOM, FromDIP(12));
    dialog.SetSizerAndFit(root);
    dialog.CenterOnParent();
    dialog.ShowModal();
}

void PreferencesDialog::select_page(int page)
{
    if (m_book == nullptr || page < 0 || page >= int(m_book->GetPageCount())) return;
    if (m_book->GetSelection() != page) {
        m_book->SetSelection(page);
        if (m_tabbar && page < int(m_page_ids.size()))
            m_tabbar->Activate(m_page_ids[page], /*emit*/ false);
    }
}

void PreferencesDialog::clear_teleport_highlight()
{
    for (auto &entry : m_teleport_saved_colours) {
        if (entry.first == nullptr) continue;
        // A live search may have re-tinted the label meanwhile; leave its
        // colour to the search pass in that case.
        if (m_search_saved_colours.find(entry.first) != m_search_saved_colours.end()) continue;
        entry.first->SetForegroundColour(entry.second);
        entry.first->Refresh();
    }
    m_teleport_saved_colours.clear();
}

// First focusable, non-label descendant of a row: the switch, combo, input
// or button the setting is edited with.
static wxWindow *first_focusable_control(wxWindow *win)
{
    if (win == nullptr) return nullptr;
    if (dynamic_cast<wxStaticText *>(win) == nullptr && win->IsShown() && win->IsEnabled() &&
        win->AcceptsFocus() && win->GetChildren().empty())
        return win;
    for (auto *child : win->GetChildren())
        if (auto *hit = first_focusable_control(child)) return hit;
    return nullptr;
}

static wxWindow *first_focusable_control(wxSizer *sizer)
{
    if (sizer == nullptr) return nullptr;
    for (auto *item : sizer->GetChildren()) {
        wxWindow *hit = nullptr;
        if (item->IsWindow()) hit = first_focusable_control(item->GetWindow());
        else if (item->IsSizer()) hit = first_focusable_control(item->GetSizer());
        if (hit) return hit;
    }
    return nullptr;
}

bool PreferencesDialog::teleport_to_setting(const std::string &key)
{
    if (!FeatureServices::presentation_setting_available(key, PersonalModes::school_presentation_suppressed.load())) return false;
    if (key.empty()) return false;
    if (m_search_rows.empty()) build_search_index();
    const SearchRow *target = nullptr;
    for (const SearchRow &row : m_search_rows)
        if (std::find(row.keys.begin(), row.keys.end(), key) != row.keys.end()) { target = &row; break; }
    if (target == nullptr || target->item == nullptr) return false;

    // Any active search filter would hide the row; teleport wins.
    if (m_changed_only) {
        m_changed_only = false;
        refresh_changed_chip();
    }
    if (m_search) m_search->SetValue(wxEmptyString);
    reset_search_filter();

    select_page(target->page);
    target->item->Show(true);
    if (wxWindow *page_win = m_book->GetPage(target->page)) {
        page_win->Layout();
        if (auto *scrolled = dynamic_cast<wxScrolledWindow *>(page_win)) scrolled->FitInside();
    }
    Layout();
    scroll_search_row_into_view(*target);

    // Focus the row's control so keyboard users land ON the setting.
    wxWindow *control = target->item->IsWindow() ? first_focusable_control(target->item->GetWindow())
                                                 : first_focusable_control(target->item->GetSizer());
    if (control) control->SetFocus();

    // Brief Primary-tint flash on the row's labels, restored by the timer.
    clear_teleport_highlight();
    const wxColour highlight = StateColor::semantic(MD3::Role::Primary);
    for (wxStaticText *label : target->labels) {
        if (label == nullptr) continue;
        m_teleport_saved_colours.emplace(label, label->GetForegroundColour());
        label->SetForegroundColour(highlight);
        label->Refresh();
    }
    if (!m_teleport_saved_colours.empty()) {
        m_teleport_timer.SetOwner(this, wxID_HIGHEST + 91);
        Unbind(wxEVT_TIMER, &PreferencesDialog::on_teleport_timer, this, wxID_HIGHEST + 91);
        Bind(wxEVT_TIMER, &PreferencesDialog::on_teleport_timer, this, wxID_HIGHEST + 91);
        m_teleport_timer.StartOnce(1400);
    }
    return true;
}

void PreferencesDialog::on_teleport_timer(wxTimerEvent &) { clear_teleport_highlight(); }

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
    if (query.empty() && !m_changed_only) {
        reset_search_filter();
        return;
    }

    m_search_active     = true;
    m_search_last_query = query;
    clear_search_highlights();

    const bool     regex     = m_search && m_search->IsRegexEnabled();
    const bool     case_sens = m_search && m_search->IsCaseSensitive();
    const bool     whole     = m_search && m_search->IsWholeWord();
    const bool     multiline = m_search && m_search->IsMultiline();
    const wxColour highlight = StateColor::semantic(MD3::Role::Primary);
    const size_t   count     = m_search_rows.size();
    std::optional<SearchField::MatchPass> match_pass;
    if (!query.empty()) match_pass.emplace(query, regex, case_sens, whole, multiline);

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
        matched[i] = (!match_pass || match_pass->matches(row.haystack)) && (!m_changed_only || row_changed(row));
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
    for (size_t k = 0; k < count && match_pass; ++k) {
        if (!matched[k]) continue;
        const SearchRow &row      = m_search_rows[k];
        bool             any_tint = false;
        for (auto *label : row.labels) {
            if (match_pass->matches(search_label_text(label))) {
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
            // Reveal the section in the strip too (a hidden or collapsed-group
            // section comes back into view without touching the collapsed
            // preference); no event, the book already switched.
            if (target.page >= 0 && target.page < int(m_page_ids.size()))
                m_tabbar->Activate(m_page_ids[target.page], /*emit*/ false);
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
    try { save_geometry(); }
    catch (...) { BOOST_LOG_TRIVIAL(warning) << "Could not save Preferences window size: " << current_exception_message(); }
    m_school_refresh_timer.Stop();
    m_narrator_inventory_timer.Stop();
    m_change_timer.Stop();
    wxGetApp().set_preferences_dialog(nullptr);
    m_radio_group.DeleteContents(true);
    m_hash_selector.clear();
}

void PreferencesDialog::refresh_privacy_checkbox()
{
    if (!m_privacy_checkbox) return;
    if (wxGetApp().is_uep_force_disabled()) {
        m_privacy_checkbox->SetValue(wxGetApp().is_uep_force_checked());
        m_privacy_checkbox->Enable(false);
    } else {
        m_privacy_checkbox->Enable(true);
        m_privacy_checkbox->SetValue(app_config->get("firstguide", "privacyuse") == "true");
    }
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
    // wx supplies the user's existing dimensions scaled for the destination DPI.
    constrain_geometry(suggested_rect.IsEmpty() ? GetRect() : suggested_rect);
    if (m_book) {
        for (size_t i = 0; i < m_book->GetPageCount(); ++i)
            if (auto *page = dynamic_cast<wxScrolledWindow *>(m_book->GetPage(i))) page->FitInside();
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
        SetName(_L("Appearance preview"));
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
        auto *lbl = new Label(scrolled, label, 0, wxSize(FromDIP(150), -1));
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
    theme->SetName(_L("Theme"));
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
    density->SetName(_L("Density"));
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
        // Accessible name: the swatch is a painted wxWindow with no text of its own.
        sw->SetName(wxString::Format(_L("Accent %d"), int(i + 1)));
        swatches->push_back(sw);
        accent_row->Add(sw, 0, wxRIGHT, FromDIP(10));
    }

    // Custom accent: a wxColourDialog seeded from the persisted seed. Any colour
    // is a valid seed (the six swatches are just shortcuts into the same
    // MD3::setAccentSeed pipeline), so a pick persists + applies exactly like a
    // preset; the preset rings deselect since none of them is the active seed.
    // A 32px "+" tile matching the swatch geometry (a full-width labeled button
    // overflows the row at the default dialog size and clipped at the edge).
    auto *accent_custom = new Button(scrolled, "+");
    m_button_list[m_button_list.size()] = accent_custom;
    accent_custom->SetVariant(Button::Variant::Outlined);
    accent_custom->SetButtonSize(Button::Size::Small);
    accent_custom->SetMinSize(wxSize(FromDIP(32), FromDIP(32)));
    accent_custom->SetToolTip(_L("Custom color"));
    accent_custom->SetName(_L("Custom accent"));
    accent_custom->Bind(wxEVT_BUTTON, [this, swatches](wxCommandEvent &) {
        std::string seed = app_config->get("ui_accent_seed");
        if (seed.empty()) seed = "#146c2e";
        wxColour initial(wxString::FromUTF8(seed));
        if (!initial.IsOk()) initial = wxColour(wxString::FromUTF8("#146c2e"));
        // MD3 continuous picker (hue strip + S/V field + Material tonal
        // ladder + colour translator) instead of the native common dialog.
        MD3ColorPickerDialog dlg(this, initial);
        if (dlg.ShowModal() != wxID_OK) return;
        const wxColour picked = dlg.GetColour();
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
    auto *font_preview = new Label(scrolled, _L("The quick brown fox") + wxString::FromUTF8(" / \xE4\xB8\xAD\xE6\x96\x87\xE7\xA4\xBA\xE4\xBE\x8B 123"));
    font_preview->SetName(_L("Appearance preview text"));
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
    text_size->SetName(_L("Text size"));
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
    auto *theme_row = make_row(_L("Theme"), theme);
    register_option_row("dark_color_mode", theme_row);
    sizer->Add(theme_row, flags);
    auto *density_row = make_row(_L("Density"), density);
    register_option_row("ui_density", density_row);
    sizer->Add(density_row, flags);

    // Accent row (label + swatches).
    auto *accent_line = new wxBoxSizer(wxHORIZONTAL);
    accent_line->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    auto *accent_lbl = new Label(scrolled, _L("Accent color"), 0, wxSize(FromDIP(150), -1));
    accent_lbl->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    accent_lbl->SetFont(::Label::Body_13);
    accent_line->Add(accent_lbl, wxSizerFlags().CenterVertical());
    accent_line->Add(accent_row, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(ITEM_RIGHT_PADDING)));
    register_option_row("ui_accent_seed", accent_line);
    sizer->Add(accent_line, flags);

    // Live token preview row (label-column indented, stretches to the row edge).
    auto *md3_preview_line = new wxBoxSizer(wxHORIZONTAL);
    md3_preview_line->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    md3_preview_line->Add(md3_preview, 1, wxEXPAND | wxRIGHT, FromDIP(ITEM_RIGHT_PADDING));
    sizer->Add(md3_preview_line, flags);

    // UI font rows (family + size + live preview), consistent with the rows above.
    auto *font_row = make_row(_L("Font"), font_combo);
    register_option_row("ui_font_family", font_row);
    sizer->Add(font_row, flags);
    auto *text_size_row = make_row(_L("Text size"), text_size);
    register_option_row("ui_font_scale", text_size_row);
    sizer->Add(text_size_row, flags);
    sizer->Add(make_row(_L("Preview"), font_preview), flags);

    // Reset row (indent-aligned with the rows above, no leading label).
    auto *reset_line = new wxBoxSizer(wxHORIZONTAL);
    reset_line->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    reset_line->Add(reset_btn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(ITEM_RIGHT_PADDING));
    sizer->Add(reset_line, flags);

    // ---- App name (user-renamable display label) -----------------------------
    // Persists AppConfig "app_display_name" through GUI_App::set_app_display_name,
    // which sanitizes, validates (1-40 code points, no control characters) and
    // broadcasts the change so the title bar wordmark, window title and dialog
    // captions re-read it live. It is a label only: identity-bound paths (data
    // folder, installer/updater ids, logs, diagnostics) never read this key.
    const std::string shipped_name = SLIC3R_APP_FULL_NAME;
    const std::string stored_name  = app_config->get(AppDisplayName::CONFIG_KEY);
    auto *name_input = new ::TextInput(scrolled, from_u8(AppDisplayName::resolve(stored_name, shipped_name)), wxEmptyString, wxEmptyString,
                                       wxDefaultPosition, wxSize(FromDIP(260), -1), wxTE_PROCESS_ENTER);
    {
        // MD3 ValueField fill, resolved by role so it re-themes in dark.
        StateColor name_bg(std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHigh), StateColor::Disabled),
                           std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHighest), StateColor::Enabled));
        name_input->SetBackgroundColor(name_bg);
        name_input->SetCornerRadius(FromDIP(10));
        name_input->GetTextCtrl()->SetFont(::Label::Body_13);
        name_input->GetTextCtrl()->SetName(_L("App name"));
        // Sanitized suggested default: the shipped name is what the field falls back
        // to, so it is also the hint shown while the box is empty.
        name_input->GetTextCtrl()->SetHint(from_u8(shipped_name));
        // Hard input bound well above the 40-code-point rule so an over-long paste is
        // reported inline instead of silently clipped mid-word by the control.
        name_input->GetTextCtrl()->SetMaxLength(200);
    }

    // Inline status line: the validation problem in plain words while the typed
    // value is invalid, otherwise the provenance of the value in effect.
    auto *name_status = new Label(scrolled, wxEmptyString, 0, wxSize(FromDIP(520), -1));
    name_status->SetFont(::Label::Body_12);
    name_status->SetName(_L("App name status"));

    auto name_problem = [](const std::string &typed) {
        const AppDisplayName::Validation raw = AppDisplayName::validate(typed);
        if (raw.problem == AppDisplayName::Problem::ControlCharacters || raw.problem == AppDisplayName::Problem::TooLong)
            return raw.problem;
        // Empty is judged after sanitizing so whitespace-only input is reported,
        // but "  Name  " (which sanitize() trims) is not.
        return AppDisplayName::sanitize(typed).empty() ? AppDisplayName::Problem::Empty : AppDisplayName::Problem::None;
    };

    auto refresh_name_status = [this, name_input, name_status, name_problem, shipped_name, scrolled]() {
        const std::string typed   = into_u8(name_input->GetTextCtrl()->GetValue());
        const auto        problem = name_problem(typed);
        wxString          text;
        bool              is_error = true;
        switch (problem) {
        case AppDisplayName::Problem::Empty:
            text = wxString::Format(_L("Enter a name of 1 to %d characters, or reset to the shipped name."), int(AppDisplayName::MAX_LENGTH));
            break;
        case AppDisplayName::Problem::TooLong:
            text = wxString::Format(_L("Too long: %d characters. Use %d or fewer."), int(AppDisplayName::utf8_length(typed)), int(AppDisplayName::MAX_LENGTH));
            break;
        case AppDisplayName::Problem::ControlCharacters:
            text = _L("Remove line breaks and other control characters.");
            break;
        case AppDisplayName::Problem::None: {
            is_error                = false;
            const std::string stored = app_config->get(AppDisplayName::CONFIG_KEY);
            if (AppDisplayName::provenance(stored, shipped_name) == AppDisplayName::Provenance::Stored)
                text = wxString::Format(_L("Stored in your settings (%s). Shipped name: %s"), from_u8(AppDisplayName::CONFIG_KEY), from_u8(shipped_name));
            else
                text = wxString::Format(_L("Not set. Showing the shipped name, %s."), from_u8(shipped_name));
            break;
        }
        }
        name_status->SetForegroundColour(StateColor::semantic(is_error ? MD3::Role::Error : MD3::Role::OnSurfaceVariant));
        name_status->SetLabel(text);
        name_status->Wrap(FromDIP(520));
        scrolled->Layout();
    };

    // Commit on Enter / focus loss: sanitize, refuse invalid input (the status
    // line already says why, and the typed text is kept for correction), else
    // persist + broadcast and echo the sanitized form back into the field.
    auto commit_name = [this, name_input, name_problem, refresh_name_status](wxEvent &e) {
        e.Skip();
        const std::string typed = into_u8(name_input->GetTextCtrl()->GetValue());
        if (name_problem(typed) != AppDisplayName::Problem::None) { refresh_name_status(); return; }
        const std::string clean = AppDisplayName::sanitize(typed);
        wxGetApp().set_app_display_name(clean);
        if (clean != typed) name_input->GetTextCtrl()->ChangeValue(from_u8(clean));
        refresh_name_status();
    };
    name_input->GetTextCtrl()->Bind(wxEVT_TEXT, [refresh_name_status](wxCommandEvent &e) { e.Skip(); refresh_name_status(); });
    name_input->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, commit_name);
    name_input->GetTextCtrl()->Bind(wxEVT_KILL_FOCUS, commit_name);

    // Reset: clears the stored value (the accessor then falls back to the shipped
    // name) and shows that name in the field so the result is visible at once.
    auto *name_reset = new Button(scrolled, _L("Reset to shipped name"));
    m_button_list[m_button_list.size()] = name_reset;
    name_reset->SetVariant(Button::Variant::Outlined);
    name_reset->SetButtonSize(Button::Size::Small);
    name_reset->Bind(wxEVT_BUTTON, [name_input, shipped_name, refresh_name_status](wxCommandEvent &) {
        wxGetApp().set_app_display_name(std::string());
        name_input->GetTextCtrl()->ChangeValue(from_u8(shipped_name));
        refresh_name_status();
    });

    // Progressive disclosure: the explanation stays folded behind a toggle so the
    // row reads as one field, and unfolds into a caption that says exactly what
    // the rename does and does not touch.
    auto *name_explain = new Label(scrolled, wxString::Format(
        _L("The name is a label only. It changes the title bar, the window title, the About dialog and the dialog captions that "
           "introduce the app. Your data folder, the installer and updater, log files, crash and diagnostic reports, and file "
           "associations keep the real product name, %s, so support can still tell which software this is."),
        from_u8(shipped_name)), 0, wxSize(FromDIP(520), -1));
    name_explain->SetFont(::Label::Body_12);
    name_explain->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    name_explain->Wrap(FromDIP(520));
    name_explain->Hide();
    auto *name_explain_toggle = new Button(scrolled, _L("What does renaming change?"));
    m_button_list[m_button_list.size()] = name_explain_toggle;
    name_explain_toggle->SetVariant(Button::Variant::Text);
    name_explain_toggle->SetButtonSize(Button::Size::Small);
    name_explain_toggle->Bind(wxEVT_BUTTON, [name_explain, name_explain_toggle, scrolled](wxCommandEvent &) {
        const bool show = !name_explain->IsShown();
        name_explain->Show(show);
        name_explain_toggle->SetLabel(show ? _L("Hide explanation") : _L("What does renaming change?"));
        scrolled->Layout();
        scrolled->FitInside();
    });

    auto *name_row = make_row(_L("App name"), name_input);
    sizer->Add(name_row, flags);
    register_option_row(AppDisplayName::CONFIG_KEY, name_row);
    auto *name_status_line = new wxBoxSizer(wxHORIZONTAL);
    name_status_line->AddSpacer(FromDIP(ITEM_LEFT_PADDING) + FromDIP(150));
    name_status_line->Add(name_status, 1, wxEXPAND | wxRIGHT, FromDIP(ITEM_RIGHT_PADDING));
    sizer->Add(name_status_line, wxSizerFlags().Expand().Border(wxTOP, FromDIP(4)));
    // Actions + folded explanation share one row so a settings-search show/hide
    // of the row never fights the user's own fold state.
    auto *name_actions = new wxBoxSizer(wxVERTICAL);
    auto *name_actions_line = new wxBoxSizer(wxHORIZONTAL);
    name_actions_line->AddSpacer(FromDIP(ITEM_LEFT_PADDING) + FromDIP(150));
    name_actions_line->Add(name_reset, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(ITEM_RIGHT_PADDING));
    name_actions->Add(name_actions_line, 0, wxEXPAND);
    // The explain toggle sits on its own line: beside the reset button the two
    // labels overran the dialog's 780 px width and the toggle was clipped.
    auto *name_toggle_line = new wxBoxSizer(wxHORIZONTAL);
    name_toggle_line->AddSpacer(FromDIP(ITEM_LEFT_PADDING) + FromDIP(150));
    name_toggle_line->Add(name_explain_toggle, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(ITEM_RIGHT_PADDING));
    name_actions->Add(name_toggle_line, 0, wxEXPAND | wxTOP, FromDIP(2));
    auto *name_explain_line = new wxBoxSizer(wxHORIZONTAL);
    name_explain_line->AddSpacer(FromDIP(ITEM_LEFT_PADDING) + FromDIP(150));
    name_explain_line->Add(name_explain, 1, wxEXPAND | wxRIGHT, FromDIP(ITEM_RIGHT_PADDING));
    name_actions->Add(name_explain_line, 0, wxEXPAND | wxTOP, FromDIP(4));
    sizer->Add(name_actions, flags);
    refresh_name_status();

    // ---- App logo (presentation-only mark) ---------------------------------
    // The panel commits through GUI_App::set_app_logo_settings, which persists
    // AppConfig "app_logo" and redraws the title bar tile and the window icon at
    // once; the About dialog and the startup screen read it when they open. The
    // installed icon, shortcuts, installer and updater never read the key.
    auto *logo_title = create_item_title(_L("App logo"), scrolled, _L("App logo"));
    sizer->Add(logo_title, wxSizerFlags().Expand().Border(wxTOP, FromDIP(24)));
    auto *logo_panel = new AppLogoUI::LogoPanel(scrolled, AppLogo::resolve(app_config->get(AppLogo::config_key)),
        [](const AppLogo::Settings &settings) { return wxGetApp().set_app_logo_settings(settings); });
    logo_panel->SetName(_L("App logo"));
    auto *logo_line = new wxBoxSizer(wxHORIZONTAL);
    logo_line->AddSpacer(FromDIP(ITEM_LEFT_PADDING));
    logo_line->Add(logo_panel, 1, wxEXPAND | wxRIGHT, FromDIP(ITEM_RIGHT_PADDING));
    sizer->Add(logo_line, flags);
    register_option_row(AppLogo::config_key, logo_line);

    if (!PersonalModes::school_presentation_suppressed.load()) {
    auto *wording = new StaticBox(scrolled);
    auto *wording_sizer = new wxBoxSizer(wxVERTICAL);
    wording->SetSizer(wording_sizer);
    auto *wording_title = new Label(wording, _L("Personal vocabulary"));
    wording_title->SetName("personal-vocabulary-title");
    wording_sizer->Add(wording_title, 0, wxEXPAND | wxALL, FromDIP(12));
    // Text joined from several translated sentences is registered as interface
    // copy, so the label stays eligible for the display vocabulary.
    auto *wording_description = new Label(wording, PersonalVocabulary::remember(
        _L("Load a version 1 JSON file to change display wording on this device. Original data, logs and exports keep their original wording.") +
            "\n" + wxString::Format(_L("A file can be up to %d KiB, with keys up to %d characters and values up to %d characters."),
                int(PersonalVocabulary::max_file_bytes / 1024), int(PersonalVocabulary::max_key_code_points),
                int(PersonalVocabulary::max_value_code_points))),
        LB_AUTO_WRAP);
    wording_sizer->Add(wording_description, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));
    auto *wording_status = new Label(wording, wxEmptyString, LB_AUTO_WRAP);
    auto *wording_load = new Button(wording, _L("Load JSON"));
    auto *wording_clear = new Button(wording, _L("Clear personal vocabulary"));
    for (Button *button : {wording_load, wording_clear}) {
        button->SetVariant(Button::Variant::Outlined);
        button->SetButtonSize(Button::Size::Small);
        m_button_list[m_button_list.size()] = button;
        wording_sizer->Add(button, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));
    }
    wording_sizer->Add(wording_status, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));
    // problem is empty for the plain no-file and loaded states; otherwise it
    // says what failed and what is shown now.
    auto refresh_wording = [wording_load, wording_clear, wording_status, scrolled](wxString problem) {
        const bool active = PersonalVocabulary::loaded();
        wording_load->SetLabel(active ? _L("Replace JSON") : _L("Load JSON"));
        wording_clear->Enable(true);
        const auto saved = PersonalVocabulary::cache_rejection();
        if (problem.empty() && !active && saved != PersonalVocabulary::Rejection::None)
            problem = _L("The vocabulary saved on this device did not pass validation, so original wording is shown.") + "\n" +
                VocabularyDrop::rejection_text(saved);
        const bool failed = !problem.empty();
        wording_status->SetLabel(failed ? PersonalVocabulary::remember(problem)
            : active ? _L("Personal vocabulary is active on this device.") : _L("Original wording is active."));
        wording_status->SetForegroundColour(StateColor::semantic(failed ? MD3::Role::Error : MD3::Role::OnSurfaceVariant));
        scrolled->Layout();
        scrolled->FitInside();
    };
    wording_load->Bind(wxEVT_BUTTON, [this, refresh_wording](wxCommandEvent &) {
        if (PersonalModes::school_presentation_suppressed.load()) return;
        wxFileDialog picker(this, _L("Load personal vocabulary"), wxEmptyString, wxEmptyString,
            _L("JSON files (*.json)|*.json"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (picker.ShowModal() != wxID_OK) return;
        auto reason = PersonalVocabulary::Rejection::None;
        const bool applied = PersonalVocabulary::load(std::filesystem::path(picker.GetPath().ToStdWstring()), &reason);
        ActionSounds::play(applied ? ActionSounds::Cue::VocabularyApplied : ActionSounds::Cue::Rejected);
        refresh_wording(applied ? wxString()
            : _L("The vocabulary file was not applied, and the wording on screen has not changed.") + "\n" + VocabularyDrop::rejection_text(reason));
    });
    wording_clear->Bind(wxEVT_BUTTON, [refresh_wording](wxCommandEvent &) {
        if (PersonalModes::school_presentation_suppressed.load()) return;
        const bool cleared = PersonalVocabulary::clear();
        ActionSounds::play(cleared ? ActionSounds::Cue::WordingCleared : ActionSounds::Cue::Rejected);
        refresh_wording(cleared ? wxString()
            : _L("The saved vocabulary could not be removed from this device, so nothing changed."));
    });
    sizer->Add(wording, 0, wxEXPAND | wxALL, FromDIP(16));
    register_option_row("personal_vocabulary", nullptr, wording);
    // The only place the vocabulary leaves this computer, and only when asked:
    // the job name a printer shows. Models, G-code and file contents never change.
    sizer->Add(create_item_checkbox(_L("Use my wording in job names sent to printers"), scrolled,
        _L("Applies your personal vocabulary to the print name a printer shows. The model, the G-code and the file contents never change."),
        50, "vocabulary_job_names"), flags);
    refresh_wording(wxString());
    }

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

    if (auto* runtime = wxGetApp().school_runtime()) {
        auto* mode_box = new StaticBox(scrolled);
        auto* mode_rows = new wxBoxSizer(wxVERTICAL);
        mode_box->SetSizer(mode_rows);
        auto* mode_title = new Label(mode_box, wxString::FromUTF8(runtime->mode().record().display_name));
        auto* mode_status = new Label(mode_box, wxEmptyString, LB_AUTO_WRAP);
        auto* mode_name = new ::TextInput(mode_box, wxString::FromUTF8(runtime->mode().record().display_name), _L("Display name"));
        mode_name->GetTextCtrl()->SetMaxLength(128);
        mode_name->GetTextCtrl()->SetName(_L("Display name"));
        auto* old_answer = new ::TextInput(mode_box, wxEmptyString, _L("Current unlock credential"), wxEmptyString,
            wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
        auto* new_answer = new ::TextInput(mode_box, wxEmptyString, _L("New unlock credential"), wxEmptyString,
            wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
        for (auto* input : {old_answer, new_answer}) {
            input->GetTextCtrl()->SetMaxLength(256);
            input->GetTextCtrl()->SetName(input == old_answer ? _L("Current unlock credential") : _L("New unlock credential"));
            FeatureServices::SurfaceRegistry::instance().register_sensitive(input);
        }
        auto* kind = new MultiSwitchButton(mode_box);
        kind->SetOptions({_L("Password"), _L("PIN")});
        kind->SetName(_L("Unlock credential type"));
        kind->SetSelection(0);
        auto* enable = new Button(mode_box, _L("Enable"));
        auto* unlock = new Button(mode_box, _L("Unlock and disable"));
        auto* enroll = new Button(mode_box, _L("Set unlock credential"));
        auto* replace = new Button(mode_box, _L("Replace unlock credential"));
        auto* rename = new Button(mode_box, _L("Rename"));
        for (wxWindow* control : std::vector<wxWindow*>{mode_title, mode_status, mode_name, rename, kind, old_answer, new_answer,
                                                      enroll, replace, enable, unlock})
            mode_rows->Add(control, 0, wxEXPAND | wxALL, FromDIP(8));
        auto* disclosure = new Label(mode_box,
            _L("This is a presentation lock, not a security boundary. Deleting the shared local application-data record resets it. Credentials remain in the operating-system vault."), LB_AUTO_WRAP);
        mode_rows->Add(disclosure, 0, wxEXPAND | wxALL, FromDIP(8));
        auto last_failed = std::make_shared<bool>(false);
        auto refresh = [runtime, mode_title, mode_status, enroll, replace, enable, unlock, scrolled, last_failed](int failed) {
            if (failed >= 0) *last_failed = failed != 0;
            mode_title->SetLabel(wxString::FromUTF8(runtime->mode().record().display_name));
            wxString state = runtime->mode().suppressed() ? _L("Restricted presentation is active.") : _L("Normal presentation is active.");
            if (runtime->mode().status() == PersonalModes::RecordStatus::Corrupt || runtime->mode().status() == PersonalModes::RecordStatus::Unavailable)
                state += "\n" + _L("Shared record is unreadable. Restricted presentation remains active.");
            state += "\n" + (runtime->native_watch_available() ? _L("Native monitoring is active.") : _L("Native monitoring is unavailable; polling is used."));
            bool configured = false, available = true;
            unsigned wait_seconds = 0;
            try {
                configured = wxGetApp().school_credentials().metadata().configured;
                const auto attempts = wxGetApp().school_credentials().attempts();
                wait_seconds = attempts.wait_seconds;
                if (wait_seconds) state += "\n" + wxString::Format(_L("Try again in %u seconds."), wait_seconds);
            }
            catch (const LocalSecurity::Failure&) { available = false; state += "\n" + _L("Credential vault is unavailable."); }
            enroll->Enable(available && !configured);
            replace->Enable(available && configured && !wait_seconds);
            enable->Enable(available && configured && !runtime->mode().record().enabled);
            unlock->Enable(available && configured && runtime->mode().record().enabled && !wait_seconds);
            if (*last_failed) state += "\n" + _L("The change was not applied. Check the credential, input limits and shared-record availability.");
            mode_status->SetLabel(state);
            scrolled->Layout(); scrolled->FitInside();
        };
        auto take_answer = [](::TextInput* input) {
            auto value = input->GetTextCtrl()->GetValue().ToUTF8();
            LocalSecurity::Secret secret(std::string_view(value.data(), value.length()));
            if (value.length()) std::fill_n(value.data(), value.length(), '\0');
            input->GetTextCtrl()->ChangeValue(wxEmptyString);
            return secret;
        };
        auto invoke = [refresh](const std::function<PersonalModes::RecordStatus()>& operation) {
            try { refresh(operation() != PersonalModes::RecordStatus::Ready); }
            catch (const LocalSecurity::Failure&) { refresh(true); }
        };
        rename->Bind(wxEVT_BUTTON, [runtime, mode_name, invoke](wxCommandEvent&) {
            invoke([&] { return runtime->mode().rename(mode_name->GetTextCtrl()->GetValue().ToUTF8().data()); });
        });
        enable->Bind(wxEVT_BUTTON, [invoke](wxCommandEvent&) {
            invoke([] { return wxGetApp().school_credentials().enable(); });
        });
        unlock->Bind(wxEVT_BUTTON, [old_answer, take_answer, invoke](wxCommandEvent&) {
            auto answer = take_answer(old_answer);
            invoke([&] { return wxGetApp().school_credentials().disable(answer); });
        });
        enroll->Bind(wxEVT_BUTTON, [new_answer, kind, take_answer, invoke](wxCommandEvent&) {
            auto answer = take_answer(new_answer);
            invoke([&] { return wxGetApp().school_credentials().enroll(kind->GetSelection() == 1 ? LocalSecurity::CredentialKind::Pin : LocalSecurity::CredentialKind::Password, answer); });
        });
        replace->Bind(wxEVT_BUTTON, [old_answer, new_answer, kind, take_answer, invoke](wxCommandEvent&) {
            auto before = take_answer(old_answer); auto after = take_answer(new_answer);
            invoke([&] { return wxGetApp().school_credentials().replace(before, kind->GetSelection() == 1 ? LocalSecurity::CredentialKind::Pin : LocalSecurity::CredentialKind::Password, after); });
        });
        sizer->Add(mode_box, 0, wxEXPAND | wxALL, FromDIP(16));
        register_option_row("school_mode", nullptr, mode_box);
        FeatureServices::SurfaceRegistry::instance().register_surface(mode_box, "preferences/shared-presentation");
        auto& surfaces = FeatureServices::SurfaceRegistry::instance();
        surfaces.record_name(old_answer->GetTextCtrl(), "Current unlock credential");
        surfaces.record_name(new_answer->GetTextCtrl(), "New unlock credential");
        surfaces.record_name(mode_name->GetTextCtrl(), "Display name");
        surfaces.record_name(kind, "Unlock credential type");
        surfaces.record_label(enable, "Enable");
        surfaces.record_label(unlock, "Unlock and disable");
        surfaces.record_label(enroll, "Set unlock credential");
        surfaces.record_label(replace, "Replace unlock credential");
        surfaces.record_label(rename, "Rename");
        surfaces.record_label(disclosure, "This is a presentation lock, not a security boundary. Deleting the shared local application-data record resets it. Credentials remain in the operating-system vault.");
        refresh(false);
        m_school_refresh_timer.SetOwner(this, wxWindow::NewControlId());
        Bind(wxEVT_TIMER, [refresh](wxTimerEvent&) { refresh(-1); }, m_school_refresh_timer.GetId());
        m_school_refresh_timer.Start(1000);
    }

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
    wxBoxSizer *item_language = nullptr, *item_funny_en = nullptr, *item_funny_yue = nullptr, *item_dialog_emojis = nullptr;
    if (!PersonalModes::school_presentation_suppressed.load()) {
    item_language = create_item_language_mode_combobox(
        _L("Language"), scrolled, _L("Language"), "language", language_choices);

    // Per-language funny levels and the dialog emoji toggle sit directly under
    // the language picker; all three persist in AppConfig and apply live.
    item_funny_en  = create_item_funny_level_slider(scrolled, I18N::FUNNY_LEVEL_ENGLISH_KEY, false);
    item_funny_yue = create_item_funny_level_slider(scrolled, I18N::FUNNY_LEVEL_CANTONESE_KEY, true);
    item_dialog_emojis = create_item_checkbox(
        funny_row_label("Show emojis in dialogs and message boxes"), scrolled,
        funny_row_label("Adds one decorative emoji to a dialog headline. Buttons, action labels and field labels never carry one."),
        50, I18N::DIALOG_EMOJIS_KEY);
    }

    std::vector<wxString> Regions     = {_L("Asia-Pacific"), _L("Chinese Mainland"), _L("Europe"), _L("North America"), _L("Others")};
    auto                  item_region = create_item_region_combobox(_L("Login Region"), scrolled, _L("Login Region"), Regions);

    std::vector<wxString> Units         = {_L("Metric") + " (mm, g)", _L("Imperial") + " (in, oz)"};
    auto                  item_currency = create_item_combobox(_L("Units"), scrolled, _L("Units"), "use_inches", Units, {"0", "1"});
    auto item_motion = create_item_combobox(
        _L("Interface motion"), scrolled,
        _L("Reduce motion settles supported transitions immediately. System follows your operating system preference."),
        "motion_preference", {_L("System"), _L("Reduce motion")}, {"system", "reduced"});

    // Theme (dark mode) now lives in the Appearance section's Theme
    // SegmentedControl (bound to dark_color_mode), so the legacy Windows-only
    // "Enable dark mode" checkbox is no longer created here.

    std::vector<wxString>    FlushOptionLabels{_L("All related changes"), _L("When color changes"), _L("Turn off auto calculate")};
    std::vector<std::string> FlushOptionValues{"all", "color change", "disabled"};
    std::vector<wxString>    FlushOptionTooltips{_L("Auto calculate when color changes, type or configuration changes"), _L("Auto calculate when sync or modify filament colors"),
                                                 _L("Keep current flush volumes, trigger manually when needed")};
    auto item_auto_flush = create_item_combobox(_L("Auto Calculate Flush Volume"), scrolled, _L("Auto calculate flush volumes"), "auto_calculate_flush", FlushOptionLabels,
                                                FlushOptionValues, FlushOptionTooltips);

    // Prepare panel dock edge — applies live to the Prepare workspace sidebar.
    std::vector<wxString>    SidebarDockLabels = {_L("Left"), _L("Right"), _L("Top"), _L("Bottom")};
    std::vector<std::string> SidebarDockValues = {"left", "right", "top", "bottom"};
    auto item_sidebar_dock = create_item_combobox(
        _L("Prepare panel position"), scrolled, _L("Dock the Prepare panel on the left, right, top, or bottom of the workspace."),
        "prepare_sidebar_dock", SidebarDockLabels, SidebarDockValues, {}, [](int) {
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

    // Only a copy installed by the Squirrel installer can update itself; the switch
    // does nothing on a portable or developer build, and its description says so.
    auto item_auto_update = create_item_checkbox(_L("Update automatically"), scrolled,
                                                 _L("Download new versions in the background and offer to restart when one is ready. Only an installed copy updates itself."),
                                                 50, "auto_update");

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

    if (item_language) sizer->Add(item_language, flags);
    if (item_funny_en) sizer->Add(item_funny_en, flags);
    if (item_funny_yue) sizer->Add(item_funny_yue, flags);
    if (item_dialog_emojis) sizer->Add(item_dialog_emojis, flags);
    sizer->Add(item_region, flags);
    sizer->Add(item_currency, flags);
    sizer->Add(item_motion, flags);
    sizer->Add(item_auto_flush, flags);
    sizer->Add(item_sidebar_dock, flags);
    sizer->Add(item_single_instance, flags);
    sizer->Add(item_fila_manager, flags);
    sizer->Add(item_multi_machine, flags);
    sizer->Add(item_beta_version_update, flags);
    sizer->Add(item_auto_update, flags);
    sizer->Add(item_priv_policy, flags);
    sizer->Add(item_downloads, flags);
    sizer->Add(item_external_editor, flags);
    sizer->Add(item_external_editor_path, flags);

    auto title_model_import = create_item_title(_L("Model import"), scrolled, _L("Model import"));
    auto item_auto_simplify_import = create_item_checkbox(
        _L("Automatically simplify large imported models"), scrolled,
        _L("Simplify meshes with at least 1,000,000 triangles before adding them to the scene. Source files are unchanged. Saved projects and meshes with painting or protected metadata keep their original geometry."),
        50, "auto_simplify_import");
    auto item_auto_simplify_detail = create_item_combobox(
        _L("Import simplification detail"), scrolled,
        _L("Uses the same detail levels as the manual Simplify tool. Extra high is the default."),
        "auto_simplify_import_detail",
        {_L("Extra high"), _L("High"), _L("Medium"), _L("Low"), _L("Extra low")},
        {"0.001", "0.01", "0.1", "0.5", "1"});
    sizer->Add(title_model_import, wxSizerFlags().Expand().Border(wxTOP, FromDIP(24)));
    sizer->AddSpacer(FromDIP(8));
    sizer->Add(item_auto_simplify_import, flags);
    sizer->Add(item_auto_simplify_detail, flags);
    scrolled->SetSizer(sizer);
    scrolled->FitInside();
    return scrolled;
}

wxWindow *PreferencesDialog::create_schedules_tab()
{
    auto *panel = new Schedule::ScheduledSettingsPanel(m_book);
    for (wxSizer *row : panel->search_rows())
        register_option_row(Schedule::kDocumentConfigKey, row);
    return panel;
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
        create_item_checkbox(_L("Keep liveview active"), scrolled,
                             _L("Keep the camera streaming while idle, on other tabs, or minimized. This does not enable recording."), 50,
                             "auto_stop_liveview");

    auto item_auto_transfer = create_item_checkbox(_L("Automatically transfer modified value when switching process and filament presets"), scrolled,
                                                   _L("After closing, a popup will appear to ask each time"), 50, "auto_transfer_when_switch_preset");

    auto item_auto_arrange_wipe_tower_on_switch_printer = create_item_checkbox(_L("Automatically optimize wipe tower placement"), scrolled,
                                                             _L("With this option enabled, the wipe tower will be placed at its optimal position."), 50,
                                                             "auto_optimize_wipe_tower_placement");

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
    sizer->Add(item_auto_arrange_wipe_tower_on_switch_printer, flags);
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

    auto title_3d     = create_item_title(_L("3D Settings"), scrolled, _L("3D Settings"));
    auto title_mouse  = create_item_title(_L("Mouse Settings"), scrolled, _L("Mouse Settings"));
    auto title_import = create_item_title(_L("Import Settings"), scrolled, _L("Import Settings"));

    auto item_zoom_to_mouse = create_item_checkbox(_L("Zoom to mouse position"), scrolled,
                                                   _L("Zoom in towards the mouse pointer's position in the 3D view, rather than the 2D window center."), 50, "zoom_to_mouse");
    auto item_reverse_mouse_wheel_zoom = create_item_checkbox(_L("Reverse direction of zoom with mouse wheel"), scrolled,
                                                              _L("If enabled, reverses the direction of zoom with mouse wheel."), 50, "reverse_mouse_wheel_zoom");
    auto item_drag_to_move = create_item_checkbox(_L("Drag with left mouse button to move"), scrolled,
                                                   _L("If enabled, the left mouse button can move models by dragging; if disabled, the left button only selects or clicks."), 50,
                                                   "canvas_drag_to_move");

    std::vector<wxString> assemble_view_preview_options = {_L("Auto"), _L("Open"), _L("Close")};
    auto enable_assemble_view_preview = create_item_combobox(
        _L("Display overview"), scrolled, _L("Display overview"), "enable_assemble_view_preview", assemble_view_preview_options,
        {"Auto", "Open", "Close"}, {}, [](int idx) {
            wxGetApp().app_config->set("enable_assemble_view_preview", idx == 0 ? "Auto" : idx == 1 ? "Open" : "Close");
            if (wxGetApp().app_config->get("enable_assemble_view_preview") == "Auto")
                wxGetApp().app_config->set_bool("enable_bvh", true);
            else if (wxGetApp().app_config->get("enable_assemble_view_preview") == "Open")
                wxGetApp().app_config->set_bool("enable_bvh", false);
        }, FromDIP(150), FromDIP(120));

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
    auto item_toolbar_style = create_item_combobox(_L("Toolbar Style"), scrolled, _L("Toolbar Style"), "toolbar_style", toolbar_style, {"0", "1"}, {}, [](int idx) -> void {
        const auto &p_ogl_manager = wxGetApp().get_opengl_manager();
        p_ogl_manager->set_toolbar_rendering_style(idx);
    });

    auto item_show_shells = create_item_checkbox(_L("Always show shells in preview"), scrolled,
                                                 _L("Always show shells or not in preview view tab. If you change this value, you should reslice."), 50,
                                                 "show_shells_in_preview");

    auto item_show_heat_soak_area = create_item_checkbox(
        _L("Show bed heat soak area"), scrolled,
        _L("Show the heated-bed preconditioning boundaries in the 3D view and preview."), 50,
        "show_bed_heat_soak_area");

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

    // ---- 3D Settings ----
    sizer->Add(enable_assemble_view_preview, flags);
    sizer->Add(item_grabber_size, flags);
    sizer->Add(item_tooltip_offset, flags);
    sizer->Add(item_toolbar_style, flags);
    sizer->Add(item_show_shells, flags);
    sizer->Add(item_show_heat_soak_area, flags);
#if !BBL_RELEASE_TO_PUBLIC
    auto item_show_bvh_bounds = create_item_checkbox(_L("Show assembly BVH primary bounds"), scrolled, _L("Display the BVH primary bounding box wireframe in assembly view."), 50,
                                                     "show_assembly_bvh_bounds");
    sizer->Add(item_show_bvh_bounds, flags);
#endif
    sizer->Add(item_enable_record_gcodeviewer, flags);
    sizer->Add(item_enable_lod, flags);
    sizer->Add(item_advanced_gcode, flags);

    // [refactor-review] Not in Figma v2 3D tab; camera-fullscreen kept here (a 3D/
    // viewport-adjacent toggle). Reviewer: confirm placement.
    auto item_camera_fullscreen = create_item_checkbox(_L("Open full screen camera view on active monitor only."), scrolled,
                                                       _L("When enabled, the camera full screen view opens only on the monitor that contains Bambu Studio."), 50,
                                                       "camera_fullscreen_active_monitor_only");
    sizer->Add(item_camera_fullscreen, flags); // [refactor-review]

    // ---- Mouse Settings ----
    sizer->Add(title_mouse, wxSizerFlags().Expand().Border(wxTOP, FromDIP(24)));
    sizer->AddSpacer(FromDIP(8));
    sizer->Add(item_zoom_to_mouse, flags);
    sizer->Add(item_reverse_mouse_wheel_zoom, flags);
    sizer->Add(item_drag_to_move, flags);

    // ---- Import Settings ----
    sizer->Add(title_import, wxSizerFlags().Expand().Border(wxTOP, FromDIP(24)));
    sizer->AddSpacer(FromDIP(8));
    sizer->Add(item_step_mesh_setting, flags);
    sizer->Add(item_import_svg, flags);
    sizer->Add(item_gamma_obj, flags);

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
    auto item_skip_printer_check = create_item_checkbox(_L("Don't check printer information when loading 3MF"), scrolled,
                                                        _L("No prompt to migrate the preset when the loaded 3MF does not match the connected printer"), 50,
                                                        "skip_printer_check_on_3mf_load");
    std::vector<wxString>    backup_labels = {_L("10 seconds"), _L("20 seconds"), _L("30 seconds"), _L("1 minute"), _L("2 minutes"),
                                              _L("5 minutes"),  _L("10 minutes"), _L("30 minutes"), _L("never")};
    std::vector<std::string> backup_values = {"10", "20", "30", "60", "120", "300", "600", "1800", "0"};
    auto item_auto_backup = create_item_combobox(_L("Auto-Backup"), scrolled, _L("The peroid of backup in seconds."), "backup_interval", backup_labels, backup_values, {},
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
    sizer->Add(item_skip_printer_check, flags);

    // Speech preferences use stable platform IDs. A missing selected voice is
    // retained, while the effective fallback is disclosed separately.
    sizer->Add(create_item_title(_L("Narrator"), scrolled,
        _L("Local speech is serialized. Voice network capability is unknown.")), flags);
    sizer->Add(create_item_checkbox(_L("Sound effects"), scrolled,
        _L("Play short sounds after wording changes. Quiet narration, quiet hours and Focus Assist also silence these sounds. System volume and mute still apply."),
        50, ActionSounds::enabled_key), flags);
    sizer->Add(create_item_checkbox(_L("Enable narrator"), scrolled,
        _L("Off by default. Speak application events using installed voices."), 50, "narrator_enabled"), flags);
    sizer->Add(create_item_checkbox(_L("Quiet narration"), scrolled,
        _L("Pause local speech without changing saved voice choices. An active screen reader also pauses narration."), 50, "narrator_quiet"), flags);
    std::vector<wxString> speech_languages{_L("English")};
    std::vector<std::string> speech_values{"en"};
    if (!PersonalModes::school_presentation_suppressed.load()) {
        speech_languages.push_back(_L("Cantonese")); speech_values.push_back("yue_HK");
        speech_languages.push_back(_L("Both languages")); speech_values.push_back("both");
    }
    sizer->Add(create_item_combobox(_L("Narration language"), scrolled,
        _L("Independent of the interface language. Both speaks English first."),
        "narrator_language", speech_languages, speech_values, {}, [](int) {}), flags);
    // Independent narration tracks: each has its own switch and its own queue
    // bookkeeping, so one kind of event never replaces or silences another.
    sizer->Add(create_item_checkbox(_L("Narrate printer events"), scrolled,
        _L("Print state, filament runout and printer errors."), 50, "narrator_track_printer"), flags);
    sizer->Add(create_item_checkbox(_L("Narrate application events"), scrolled,
        _L("Slicing, export, update and error summaries. File names and account details are never spoken."), 50, "narrator_track_app"), flags);
    sizer->Add(create_item_checkbox(_L("Narrate previews and scanner prompts"), scrolled,
        _L("Voice previews and the filament scanner."), 50, "narrator_track_accessibility"), flags);
    sizer->Add(create_item_checkbox(_L("Narrator quiet hours"), scrolled,
        _L("Stay silent during the hours below. Silence also applies to errors."), 50, "narrator_quiet_hours_enabled"), flags);
    {
        std::vector<wxString> clock_labels;
        std::vector<std::string> clock_values;
        for (int minute = 0; minute < 24 * 60; minute += 30) {
            clock_values.push_back(std::string(wxString::Format("%02d:%02d", minute / 60, minute % 60).ToUTF8().data()));
            clock_labels.push_back(wxString::FromUTF8(clock_values.back()));
        }
        sizer->Add(create_item_combobox(_L("Quiet hours start"), scrolled, _L("Narration is silent from this time."),
            "narrator_quiet_hours_start", clock_labels, clock_values, {}, [](int) { ActionSounds::settings_changed(); }), flags);
        sizer->Add(create_item_combobox(_L("Quiet hours end"), scrolled, _L("Narration resumes at this time."),
            "narrator_quiet_hours_end", clock_labels, clock_values, {}, [](int) { ActionSounds::settings_changed(); }), flags);
    }
    sizer->Add(create_item_checkbox(_L("Respect Focus Assist"), scrolled,
        _L("Only speak errors while Windows Focus Assist, a full-screen app or a presentation is active."), 50, "narrator_respect_focus_assist"), flags);
    const auto installed_voices = TtsNarrator::voices();
    const auto all_voices = TtsNarrator::catalog();
    for (bool cantonese : {false, true}) {
        if (cantonese && PersonalModes::school_presentation_suppressed.load()) continue;
        const std::string voice_key = cantonese ? "narrator_voice_yue" : "narrator_voice_en";
        std::vector<wxString> labels{_L("Automatic voice")};
        std::vector<std::string> values{""};
        for (const auto& voice : installed_voices) {
            if (!(cantonese ? voice.cantonese : voice.english)) continue;
            labels.emplace_back(voice.name);
            values.push_back(wxString(voice.id).ToUTF8().data());
        }
        const auto selected = app_config->get(voice_key);
        if (!selected.empty() && std::find(values.begin(), values.end(), selected) == values.end()) {
            labels.push_back(_L("Selected voice is not installed")); values.push_back(selected);
        }
        auto* status = new Label(scrolled, wxEmptyString);
        status->SetName(cantonese ? _L("Effective Cantonese voice") : _L("Effective English voice"));
        auto refresh_status = [status, cantonese](int) {
            const auto effective = TtsNarrator::voice_status(cantonese);
            wxString text = !effective.available ? _L("No installed voice can speak this language.") :
                _L("Effective voice:") + " " + wxString(effective.effective_name);
            if (effective.selected_missing) text += "\n" + _L("Selected voice is missing; the saved choice is kept.");
            const bool network = Narration::classify_network(effective.effective_name) == Narration::NetworkCapability::NetworkBacked;
            text += "\n" + (network ? _L("This voice uses an online service and may be silent when offline.") : _L("Voice network capability is unknown."));
            status->SetLabel(text);
            status->Wrap(status->FromDIP(420));
            status->GetParent()->Layout();
        };
        auto live_values = std::make_shared<std::vector<std::string>>(values);
        const auto combo_index = m_combobox_list.size();
        sizer->Add(create_item_combobox(cantonese ? _L("Cantonese voice") : _L("English voice"), scrolled,
            _L("Choose an installed voice. Missing selections remain saved."), voice_key, labels, values, {}, refresh_status,
            0, 0, live_values), flags);
        auto* voice_picker = m_combobox_list.at(combo_index);
        m_narrator_inventory_refreshers.push_back([this, voice_picker, voice_key, cantonese, live_values, refresh_status] {
            std::vector<wxString> refreshed_labels{_L("Automatic voice")};
            std::vector<std::string> refreshed_values{""};
            for (const auto& voice : TtsNarrator::voices()) {
                if (!(cantonese ? voice.cantonese : voice.english)) continue;
                refreshed_labels.emplace_back(voice.name);
                refreshed_values.push_back(wxString(voice.id).ToUTF8().data());
            }
            const auto saved = app_config->get(voice_key);
            if (!saved.empty() && std::find(refreshed_values.begin(), refreshed_values.end(), saved) == refreshed_values.end()) {
                refreshed_labels.push_back(_L("Selected voice is not installed")); refreshed_values.push_back(saved);
            }
            if (refreshed_values != *live_values) {
                voice_picker->Clear();
                *live_values = std::move(refreshed_values);
                for (const auto& label : refreshed_labels) voice_picker->Append(label);
                const auto found = std::find(live_values->begin(), live_values->end(), saved);
                voice_picker->SetSelection(found == live_values->end() ? 0 : static_cast<int>(found - live_values->begin()));
            }
            refresh_status(0);
        });
        sizer->Add(status, flags);
        refresh_status(0);
        auto* preview_btn = new Button(scrolled, cantonese ? _L("Preview Cantonese voice") : _L("Preview English voice"));
        m_button_list[m_button_list.size()] = preview_btn;
        preview_btn->SetVariant(Button::Variant::Outlined);
        preview_btn->SetButtonSize(Button::Size::Small);
        preview_btn->SetToolTip(_L("Speak a short sample with the voice that is in effect. Works even when the narrator is off."));
        preview_btn->Bind(wxEVT_BUTTON, [cantonese, refresh_status](wxCommandEvent&) {
            TtsNarrator::preview(cantonese);
            refresh_status(0);
        });
        sizer->Add(preview_btn, flags);
        if (cantonese && Narration::cantonese_installed_but_unusable(all_voices)) {
            auto* note = new Label(scrolled, _L("A Hong Kong Cantonese voice is installed on this computer, but this build can only speak with voices that Windows exposes to the classic speech interface. It is listed by Windows but cannot be selected here yet."));
            note->Wrap(FromDIP(420));
            sizer->Add(note, flags);
        }
        std::vector<wxString> number_labels;
        std::vector<std::string> number_values;
        for (int n = -10; n <= 10; ++n) { number_labels.push_back(wxString::Format("%d", n)); number_values.push_back(std::to_string(n)); }
        sizer->Add(create_item_combobox(cantonese ? _L("Cantonese speech rate") : _L("English speech rate"), scrolled,
            _L("Platform range: -10 to 10. Normal delivery: 0."), cantonese ? "narrator_rate_yue" : "narrator_rate_en",
            number_labels, number_values, {}, [](int) {}), flags);
        sizer->Add(create_item_combobox(cantonese ? _L("Cantonese speech pitch") : _L("English speech pitch"), scrolled,
            _L("Platform range: -10 to 10. Normal delivery: 0."), cantonese ? "narrator_pitch_yue" : "narrator_pitch_en",
            number_labels, number_values, {}, [](int) {}), flags);
    }
    const auto mirror = TtsNarrator::external_mirror_status();
    auto* mirror_status = new Label(scrolled, mirror == TtsNarrator::ExternalMirrorStatus::Unconfigured ?
        _L("External speech mirror is not configured.") :
        _L("External speech mirror is configured. Playback completion is unavailable; local serialization does not cover external playback."));
    mirror_status->Wrap(FromDIP(420));
    sizer->Add(mirror_status, flags);
    m_narrator_inventory_timer.SetOwner(this, wxWindow::NewControlId());
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) {
        for (const auto& refresh : m_narrator_inventory_refreshers) refresh();
    }, m_narrator_inventory_timer.GetId());
    m_narrator_inventory_timer.Start(2000);

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

    // ---- AI printer watch (opt-in, strictly local: Ollama on localhost) ----
    auto title_watch = create_item_title(_L("AI printer watch"), scrolled,
        _L("Summarize the printer's live camera view with a local model. Frames never leave this computer."));
    auto item_watch_enable = create_item_checkbox(
        _L("Watch the live view and notify about progress and failures"), scrolled,
        _L("Every few minutes a frame of the live view is described by a local Ollama model; suspected failures raise a persistent warning with fix suggestions."),
        50, "printer_watch_enabled");
    auto item_watch_model = create_item_input(_L("Local model tag"), "", scrolled,
        _L("Vision-capable Ollama tag, e.g. qwen2.5vl or gemma3. Text-only tags such as gpt-oss cannot read frames."),
        "printer_watch_model", [](wxString) {});
    auto item_watch_interval = create_item_input(_L("Check interval (minutes)"), "", scrolled,
        _L("Minutes between live-view checks (minimum 1)."),
        "printer_watch_interval", [](wxString) {});
    sizer->Add(title_watch, wxSizerFlags().Expand().Border(wxTOP, FromDIP(16)));
    sizer->Add(item_watch_enable, flags);
    sizer->Add(item_watch_model, flags);
    sizer->Add(item_watch_interval, flags);

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

// LAN model drop: receive 3D models that people on the local network send through the drop site
// (a Docker container). Off by default. The switch comes first, then the invite (link, Copy link,
// QR code, New link), then the connection rows. Every row is registered so the settings search and
// the command palette find it.
wxWindow *PreferencesDialog::create_lan_drop_tab()
{
    auto        scrolled = new ScrollPanel(m_book);
    wxBoxSizer *sizer    = new wxBoxSizer(wxVERTICAL);
    auto        flags    = wxSizerFlags().Expand().Border(wxTOP, FromDIP(8));

    sizer->Add(create_item_title(_L("LAN model drop"), scrolled, _L("LAN model drop")), wxSizerFlags().Expand().Border(wxTOP, FromDIP(16)));
    sizer->Add(LanModelDrop::create_intro_row(scrolled), flags);
    sizer->Add(create_item_checkbox(_L("Receive models from the LAN drop site"), scrolled,
                                    _L("Check the drop site every few seconds and show each received model with Open and Discard."), 50,
                                    "lan_drop_enabled"),
               flags);

    LanModelDrop::InvitePanel::Options invite_options;
    auto *invite = new LanModelDrop::InvitePanel(scrolled, invite_options);
    sizer->Add(invite, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(ITEM_LEFT_PADDING)));
    register_option_row("lan_drop_invite", nullptr, invite);

    sizer->Add(create_item_title(_L("Connection to the drop site"), scrolled, _L("Connection to the drop site")),
               wxSizerFlags().Expand().Border(wxTOP, FromDIP(16)));
    auto *status = LanModelDrop::create_status_row(scrolled);
    sizer->Add(status, flags);
    register_option_row("lan_drop_status", nullptr, status);
    auto *address = LanModelDrop::create_address_row(scrolled);
    sizer->Add(address, flags);
    register_option_row("lan_drop_address", nullptr, address);
    auto *key = LanModelDrop::create_key_row(scrolled);
    sizer->Add(key, flags);
    register_option_row("lan_drop_station_key", nullptr, key);

    sizer->AddSpacer(FromDIP(20));
    scrolled->SetSizer(sizer);
    scrolled->FitInside();
    return scrolled;
}

// Local status API: a read-only, loopback-only web interface for the printer list and the door state.
// Off by default. The switch comes first, then the port and the access token.
wxWindow *PreferencesDialog::create_status_api_tab()
{
    auto        scrolled = new ScrollPanel(m_book);
    wxBoxSizer *sizer    = new wxBoxSizer(wxVERTICAL);
    auto        flags    = wxSizerFlags().Expand().Border(wxTOP, FromDIP(8));

    sizer->Add(create_item_title(_L("Local status API"), scrolled, _L("Local status API")), wxSizerFlags().Expand().Border(wxTOP, FromDIP(16)));
    sizer->Add(StatusApi::create_intro_row(scrolled), flags);
    sizer->Add(create_item_checkbox(_L("Serve printer status on this computer"), scrolled,
                                    _L("Let programs on this computer read the printer list and the door state. Off by default."), 50,
                                    StatusApi::kEnabledConfigKey),
               flags);
    sizer->Add(create_item_input(_L("Port"), wxString(), scrolled, _L("The port on 127.0.0.1 that the status API listens on."),
                                 StatusApi::kPortConfigKey,
                                 [](wxString) { StatusApi::apply_settings(); }),
               flags);
    auto *token = StatusApi::create_token_row(scrolled);
    sizer->Add(token, flags);
    register_option_row("status_api_token", nullptr, token);
    auto *status = StatusApi::create_status_row(scrolled);
    sizer->Add(status, flags);
    register_option_row("status_api_state", nullptr, status);

    sizer->AddSpacer(FromDIP(20));
    scrolled->SetSizer(sizer);
    scrolled->FitInside();
    return scrolled;
}

wxWindow *PreferencesDialog::create_attention_tab()
{
    auto        scrolled = new ScrollPanel(m_book);
    wxBoxSizer *sizer    = new wxBoxSizer(wxVERTICAL);
    auto        flags    = wxSizerFlags().Expand().Border(wxTOP, FromDIP(8));

    sizer->Add(create_item_title(_L("Attention modes"), scrolled, _L("Attention modes")), wxSizerFlags().Expand().Border(wxTOP, FromDIP(16)));
    auto *intro = new Label(scrolled, ::Label::Body_13,
                            _L("Interface accommodations, each off by default and independent of the others. They change only how Bambu Studio "
                               "looks and behaves for you; nothing here is measured, scored or shared."),
                            LB_AUTO_WRAP);
    intro->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    sizer->Add(intro, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(12)));
    sizer->Add(create_item_checkbox(_L("Focus"), scrolled,
                                    _L("While something is selected in Prepare, the rest of the plate steps back: drawn muted, never hidden. "
                                       "Clear the selection to see everything again."),
                                    50, AttentionModes::kFocus),
               flags);
    sizer->Add(create_item_checkbox(_L("Low stimulation"), scrolled,
                                    _L("Less motion on top of the Windows setting, no action sounds, and no dim sum surprise at start-up."), 50,
                                    AttentionModes::kLowStimulation),
               flags);
    sizer->Add(create_item_checkbox(_L("Time awareness"), scrolled,
                                    _L("The strip under the title bar states how long this session has been open and how long since the last change."),
                                    50, AttentionModes::kTimeAwareness),
               flags);
    sizer->Add(create_item_checkbox(_L("One thing at a time"), scrolled,
                                    _L("The strip shows the one next step you choose, kept across restarts until you mark it done."), 50,
                                    AttentionModes::kOneThing),
               flags);
    sizer->Add(create_item_input(_L("Next step"), wxString(), scrolled, _L("The one next step the strip shows, at most 120 characters"),
                                 AttentionModes::kNextAction, [](wxString) {}),
               flags);
    sizer->Add(create_item_checkbox(_L("Momentum"), scrolled,
                                    _L("A quiet notification when nothing has changed for a while, once per stretch, with Not now."), 50,
                                    AttentionModes::kMomentum),
               flags);
    sizer->Add(create_item_combobox(_L("Momentum after"), scrolled, _L("How long without a change before Momentum speaks"), AttentionModes::kMomentumAfter,
                                    {_L("15 min"), _L("30 min"), _L("1 h"), _L("1 h 30 min")}, {"15", "30", "60", "90"}),
               flags);
    sizer->Add(create_item_combobox(_L("Not now lasts"), scrolled, _L("How long Momentum stays quiet after Not now"), AttentionModes::kNotNowFor,
                                    {_L("30 min"), _L("1 h"), _L("2 h"), _L("4 h")}, {"30", "60", "120", "240"}),
               flags);
    auto *school = new Label(scrolled, ::Label::Body_12,
                             _L("School mode leaves these modes as they are: they are accommodations, not presentation choices."), LB_AUTO_WRAP);
    school->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    sizer->Add(school, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(12)));

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

    auto title_texture = create_item_title(_L("Import Texture Model"), scrolled, _L("Import Texture Model"));
    auto item_color_tolerance = create_item_int_slider(
        _L("Color Tolerance of Filament Matching"), scrolled,
        _L("CIEDE2000 difference used when matching imported texture colors to filaments. Filaments within this band are treated as equally close. This does not merge colors and does not change the match threshold in the texture import dialog."),
        "texture_import_color_tolerance", 1, 100, 5);

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
    debug_button->SetVariant(Button::Variant::Outlined);
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

    sizer->Add(title_texture, wxSizerFlags().Expand().Border(wxTOP, FromDIP(24)));
    sizer->AddSpacer(FromDIP(8));
    sizer->Add(item_color_tolerance, flags);

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
    auto *row = new wxWrapSizer(wxHORIZONTAL);

    auto *btn_reset_warnings            = new Button(this, _L("Reset all warning dialogs"));
    auto *btn_reset_prefs               = new Button(this, _L("Reset preferences"));
    // TRN: Opens the shared Export dialog with every preference section.
    auto *btn_export_prefs              = new Button(this, _L("Export preferences..."));
    auto *btn_history                   = new Button(this, _L("Preferences history"));
    btn_history->SetVariant(Button::Variant::Outlined);
    btn_history->SetButtonSize(Button::Size::Small);
    btn_history->SetToolTip(_L("Every change made in Preferences, with its time"));
    btn_history->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { show_history(); });
    m_button_list[m_button_list.size()] = btn_reset_warnings;
    m_button_list[m_button_list.size()] = btn_reset_prefs;
    m_button_list[m_button_list.size()] = btn_export_prefs;

    // MD3 outlined buttons: transparent interior + 1px Outline ring with an
    // OnSurface label, pill radius (height/2) and a SurfaceContainerHigh hover
    // wash — geometry, font and colours are all resolved through semantic roles
    // by Button::applyMD3Style(), replacing the Grey300/Grey400/BrandGreen r6 look.
    for (Button *b : {btn_reset_warnings, btn_reset_prefs, btn_export_prefs}) {
        b->SetVariant(Button::Variant::Outlined);
        b->SetButtonSize(Button::Size::Small);
    }
    btn_export_prefs->SetVariant(Button::Variant::Tonal);
    btn_reset_prefs->SetVariant(Button::Variant::Text);
    btn_export_prefs->SetToolTip(_L("Export every preference section as JSON, YAML, TOML, XML, CSV, Markdown, HTML or an archive"));
    btn_export_prefs->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        if (wxGetApp().app_config != nullptr)
            ExportDialog::run(this, Export::app_config_dataset(*wxGetApp().app_config));
    });

    btn_reset_warnings->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { on_reset_all_warnings(); });
    btn_reset_prefs->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { on_reset_preferences(); });

    row->Add(btn_reset_warnings, 0, wxRIGHT, FromDIP(8));
    row->Add(btn_reset_prefs, 0, wxRIGHT, FromDIP(8));
    row->Add(btn_history, 0, wxRIGHT, FromDIP(8));
    row->Add(btn_export_prefs, 0, 0, 0);
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
    auto *msg = new Label(this, _L("All warning dialogs that you have disabled by checking \"Don't show again\" "
                                    "are now re-enabled and will show next time they apply."));
    msg->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    msg->SetFont(::Label::Body_14);
    msg->Wrap(content_width);
    msg->SetMinSize(wxSize(content_width, -1));
    main_sizer->Add(msg, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(20));

    StateColor btn_bg_gray(std::pair<wxColour, int>(ThemeColor::Grey400, StateColor::Pressed), std::pair<wxColour, int>(ThemeColor::Grey200, StateColor::Hovered),
                           std::pair<wxColour, int>(ThemeColor::White, StateColor::Normal));
    m_details_btn = new Button(this, _L("Check details"));
    m_details_btn->SetVariant(Button::Variant::Outlined);
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
    auto *det_text  = new Label(m_details_panel, _L("- Sync printer presets after loading a file\n"
                                          "- \"Load 3MF\" dialog settings\n"
                                          "- Executing post-processing scripts\n"
                                          "- Support structure recommendation prompt\n"
                                          "- Unsaved projects.\n"
                                          "- Delete a filament used by a model"));
    det_text->SetForegroundColour(ThemeColor::TextSecondary);
    det_text->SetFont(::Label::Body_13);
    det_sizer->Add(det_text, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(12));
    // The two answers that "Don't show again" can remember on the printer prompts. They sit in a
    // label of their own so the list above keeps its catalogue entry and its translations.
    auto *det_prompts = new Label(m_details_panel, PersonalVocabulary::remember_composed(
        {_L("- Sync nozzle and AMS information before slicing"), _L("- New filament badge and prompt")}, "\n"));
    det_prompts->SetForegroundColour(ThemeColor::TextSecondary);
    det_prompts->SetFont(::Label::Body_13);
    det_sizer->Add(det_prompts, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));
    m_details_panel->SetSizer(det_sizer);
    m_details_panel->Hide();
    main_sizer->Add(m_details_panel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(20));

    // Footer buttons.
    auto *btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    btn_sizer->AddStretchSpacer(1);
    auto      *cancel_btn = new Button(this, _L("Cancel"));
    cancel_btn->SetVariant(Button::Variant::Outlined);
    cancel_btn->SetFont(::Label::Body_12);
    cancel_btn->SetMinSize(wxSize(FromDIP(58), FromDIP(24)));
    cancel_btn->SetCornerRadius(FromDIP(6));
    cancel_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
    btn_sizer->Add(cancel_btn, 0, wxRIGHT, FromDIP(8));

    StateColor btn_bg_green(std::pair<wxColour, int>(ThemeColor::BrandGreenPressed, StateColor::Pressed),
                            std::pair<wxColour, int>(ThemeColor::BrandGreenHovered, StateColor::Hovered), std::pair<wxColour, int>(ThemeColor::BrandGreen, StateColor::Normal));
    auto      *confirm_btn = new Button(this, _L("Confirm"));
    confirm_btn->SetVariant(Button::Variant::Filled);
    confirm_btn->SetFont(::Label::Body_12);
    confirm_btn->SetMinSize(wxSize(FromDIP(58), FromDIP(24)));
    confirm_btn->SetCornerRadius(FromDIP(6));
    confirm_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_OK); });
    btn_sizer->Add(confirm_btn, 0, 0, 0);

    main_sizer->Add(btn_sizer, 0, wxEXPAND | wxALL, FromDIP(20));

    SetSizer(main_sizer);
    Layout();
    Fit();
    wxGetApp().UpdateDlgDarkUI(this);
    MD3DialogCaption::Adopt(this);
    CentreOnParent();
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
    app_config->erase("app", "skip_printer_check_on_3mf_load");
    app_config->erase("app", "skip_non_bambu_3mf_warning");
    app_config->erase("app", "post_process_script_choice");
    app_config->erase("app", "no_warn_delete_used_filament");
    app_config->erase("app", "sync_ams_info_choice");
    app_config->erase("app", "hide_new_filament_prompt");
    app_config->erase("app", "no_show_tpu_tips");
    app_config->erase("app", "skip_pre_print_warning_dialog");
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
        "auto_update",
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
        "sync_user_preset",
        "sync_system_preset",
        "zoom_to_mouse",
        "reverse_mouse_wheel_zoom",
        "canvas_drag_to_move",
        "enable_assemble_view_preview",
        "grabber_size_factor",
        "3d_middle_tooltip_offset_x",
        "3d_middle_tooltip_offset_y",
        "toolbar_style",
        "show_shells_in_preview",
        "auto_simplify_import",
        "auto_simplify_import_detail",
        "enable_step_mesh_setting",
        "import_single_svg_and_split",
        "gamma_correct_in_import_obj",
        "enable_lod",
        "enable_advanced_gcode_viewer_",
        "camera_fullscreen_active_monitor_only",
        "max_recent_count",
        "no_warn_when_modified_gcodes",
        "skip_printer_check_on_3mf_load",
        "backup_switch",
        "backup_interval",
        "staff_pick_switch",
        "show_print_history",
        "developer_mode",
        "skip_ams_blacklist_check",
        "severity_level",
        "texture_import_color_tolerance",
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
