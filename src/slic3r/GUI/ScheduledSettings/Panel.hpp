#pragma once
#include "libslic3r/ScheduledSettings/Service.hpp"
#include <wx/panel.h>
#include <wx/timer.h>
#include <functional>
class SearchField;
class TabCtrl;
class wxListBox;
class wxTextCtrl;
class wxCheckBox;
class wxSpinCtrl;
class wxDatePickerCtrl;
class wxTimePickerCtrl;
class wxRadioBox;
class wxStaticText;
class wxScrolledWindow;
namespace Slic3r::GUI::ScheduledSettingsUI {
using Translate=std::function<wxString(const char* english,const char* cantonese)>;
using Visible=std::function<bool(const std::string& setting_key)>;
// Embed inside the application's existing settings tab. Service lifetime exceeds this panel.
class Panel final : public wxPanel {
public:
    Panel(wxWindow*,ScheduledSettings::Service&,Translate,Visible={});
    void refresh_presentation();
private:
    wxString tr(const char*,const char*) const;
    void rebuild_rules();
    void select_rule();
    void load_editor();
    bool read_editor(ScheduledSettings::Rule&);
    void save();
    bool retain_editor();
    void setting_editor();
    void settings_filter();
    void show_status(const wxString&);
    ScheduledSettings::Service& m_service;
    Translate m_translate;
    Visible m_visible;
    mutable std::map<wxString,std::pair<std::string,std::string>> m_copy;
    ScheduledSettings::Schedule m_draft;
    std::string m_selected;
    std::vector<std::string> m_rule_ids,m_setting_keys;
    SearchField *m_search=nullptr,*m_setting_search=nullptr,*m_zone_search=nullptr;
    wxListBox *m_rules=nullptr,*m_settings=nullptr,*m_zones=nullptr;
    wxTextCtrl *m_label=nullptr,*m_url=nullptr,*m_entity=nullptr,*m_secret=nullptr;
    wxCheckBox *m_enabled=nullptr,*m_every_day=nullptr,*m_days[7]={},*m_consent=nullptr,*m_private=nullptr,*m_loopback=nullptr;
    wxSpinCtrl *m_priority=nullptr,*m_interval=nullptr;
    wxDatePickerCtrl *m_first=nullptr,*m_last=nullptr;
    wxTimePickerCtrl *m_start=nullptr,*m_end=nullptr;
    wxRadioBox *m_source=nullptr;
    wxStaticText* m_status=nullptr;
    wxStaticText* m_live=nullptr;
    wxTimer m_live_timer;
    wxPanel* m_value_host=nullptr;
    wxWindow* m_value=nullptr;
    std::vector<ScheduledSettings::Zone> m_zone_values;
    std::vector<size_t> m_zone_indices;
    void zones_filter();
};
// Owner calls pulse on startup, every minute, after resume, and after base-setting changes.
// The timer only executes local evaluation and queues bounded work; it never waits on HTTP.
class Driver final : public wxEvtHandler {
public:
    explicit Driver(ScheduledSettings::Service&);
    ~Driver() override;
    bool pulse();
private:
    ScheduledSettings::Service& m_service;
    wxTimer m_timer;
};
}
