#include "ScheduledSettingsPanel.hpp"
#include "ScheduledSettings.hpp"

#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/LanguageMode.hpp"
#include "slic3r/GUI/MsgDialog.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/CheckBox.hpp"
#include "slic3r/GUI/Widgets/ComboBox.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/Widgets/ListBox.hpp"
#include "slic3r/GUI/Widgets/MD3Dialog.hpp"
#include "slic3r/GUI/Widgets/MD3Tokens.hpp"
#include "slic3r/GUI/Widgets/SearchField.hpp"
#include "slic3r/GUI/Widgets/StateColor.hpp"
#include "slic3r/GUI/Widgets/TextInput.hpp"

#include "libslic3r/AppConfig.hpp"

#include <wx/datectrl.h>
#include <wx/datetime.h>
#include <wx/dateevt.h>
#include <wx/fontenum.h>
#include <wx/sizer.h>
#include <wx/timectrl.h>

#include <algorithm>
#include <chrono>
#include <map>
#include <random>

namespace Slic3r { namespace GUI { namespace Schedule {

namespace {

std::uint32_t entropy_now()
{
    std::random_device rd;
    return static_cast<std::uint32_t>(rd()) ^ static_cast<std::uint32_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
}

LocalDate date_from_wx(const wxDateTime &dt)
{
    LocalDate d;
    d.year  = dt.GetYear();
    d.month = int(dt.GetMonth()) + 1;
    d.day   = dt.GetDay();
    return d;
}

wxDateTime wx_from_date(const LocalDate &d)
{
    return wxDateTime(static_cast<wxDateTime::wxDateTime_t>(d.day), static_cast<wxDateTime::Month>(d.month - 1), d.year);
}

wxDateTime wx_from_time(const LocalTime &t)
{
    wxDateTime dt = wxDateTime::Today();
    dt.SetHour(t.hour);
    dt.SetMinute(t.minute);
    dt.SetSecond(0);
    return dt;
}

LocalTime time_from_wx(const wxDateTime &dt)
{
    LocalTime t;
    t.hour   = dt.GetHour();
    t.minute = dt.GetMinute();
    return t;
}

wxString weekday_label(int d)
{
    static const char *names[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    return _L(names[d]);
}

// One value row in the editor: a "set" checkbox gating a picker or a text box.
struct ValueRow
{
    std::string                key;
    CheckBox                  *enabled { nullptr };
    ComboBox                  *combo { nullptr };   // choice-based keys
    TextInput                 *text { nullptr };    // free-entry keys
    std::vector<std::string>   choice_values;       // parallel to combo items
};

// ---------------------------------------------------------------------------
// Rule editor
// ---------------------------------------------------------------------------

class ScheduleRuleDialog final : public MD3Dialog
{
public:
    ScheduleRuleDialog(wxWindow *parent, const Rule &rule, bool is_new);

    const Rule &result() const { return m_rule; }

private:
    bool collect(Rule &out, std::vector<std::string> &problems);
    void refresh_source_rows();
    void refresh_weekday_rows();
    void show_problems(const std::vector<std::string> &problems);

    Rule                  m_rule;
    wxBoxSizer           *m_body { nullptr };
    TextInput            *m_label_input { nullptr };
    CheckBox             *m_enabled { nullptr };
    CheckBox             *m_has_start_date { nullptr };
    CheckBox             *m_has_end_date { nullptr };
    wxDatePickerCtrl     *m_start_date { nullptr };
    wxDatePickerCtrl     *m_end_date { nullptr };
    wxTimePickerCtrl     *m_start_time { nullptr };
    wxTimePickerCtrl     *m_end_time { nullptr };
    Label                *m_window_note { nullptr };
    CheckBox             *m_every_day { nullptr };
    std::vector<CheckBox *> m_weekdays;
    ComboBox             *m_source { nullptr };
    wxSizer              *m_api_rows { nullptr };
    wxSizer              *m_ha_rows { nullptr };
    TextInput            *m_api_url { nullptr };
    CheckBox             *m_api_loopback { nullptr };
    TextInput            *m_ha_entity { nullptr };
    Label                *m_source_note { nullptr };
    std::vector<ValueRow> m_values;
    Label                *m_problems { nullptr };
};

ScheduleRuleDialog::ScheduleRuleDialog(wxWindow *parent, const Rule &rule, bool is_new)
    : MD3Dialog(parent, is_new ? _L("New schedule rule") : _L("Edit schedule rule"),
                _L("Choose when the rule applies, where its values come from, and which settings it changes."),
                MaterialIcon::Schedule, MD3Dialog::Options{true, false})
    , m_rule(rule)
{
    const wxColour bg = GetBackgroundColour();
    auto *scroll = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxTAB_TRAVERSAL | wxBORDER_NONE);
    scroll->SetBackgroundColour(bg);
    scroll->SetScrollRate(0, FromDIP(12));
    m_body = new wxBoxSizer(wxVERTICAL);
    scroll->SetSizer(m_body);
    GetContentSizer()->Add(scroll, 1, wxEXPAND);
    // Every child below is parented to `scroll`; the helpers read this member.
    wxWindow *host = scroll;

    auto make_label = [&](const wxString &text, bool secondary = false, bool wrap = false) {
        auto *l = new Label(host, secondary ? Label::Body_12 : Label::Body_13, text, wrap ? LB_AUTO_WRAP : 0);
        l->SetBackgroundColour(bg);
        l->SetForegroundColour(StateColor::semantic(secondary ? MD3::Role::OnSurfaceVariant : MD3::Role::OnSurface));
        if (wrap)
            l->SetMinSize(wxSize(0, -1));
        return l;
    };
    auto make_text = [&](const wxString &value, const wxString &name, const wxString &hint, int width) {
        auto *input = new TextInput(host, value, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(width), -1));
        StateColor fill(std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHigh), StateColor::Disabled),
                        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHighest), StateColor::Enabled));
        input->SetBackgroundColor(fill);
        input->SetCornerRadius(FromDIP(10));
        input->GetTextCtrl()->SetFont(Label::Body_13);
        input->GetTextCtrl()->SetName(name);
        input->GetTextCtrl()->SetHint(hint);
        input->GetTextCtrl()->SetMaxLength(1024);
        return input;
    };
    auto make_check = [&](const wxString &name, bool value) {
        auto *box = new CheckBox(host);
        box->SetMinSize(FromDIP(wxSize(44, 44)));
        box->SetName(name);
        box->SetToolTip(name);
        box->SetValue(value);
        return box;
    };
    auto make_combo_here = [&](const wxString &name, const std::vector<wxString> &items, int width) {
        auto *combo = new ComboBox(host, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(width), -1), 0, nullptr, wxCB_READONLY);
        combo->SetFont(Label::Body_13);
        combo->GetDropDown().SetFont(Label::Body_13);
        combo->SetCornerRadius(FromDIP(10));
        combo->SetName(name);
        for (const wxString &item : items)
            combo->Append(item);
        return combo;
    };
    auto check_row = [&](CheckBox *box, const wxString &text, wxSizer *into, int top = 4) {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(box, 0, wxALIGN_CENTER_VERTICAL);
        auto *l = make_label(text);
        l->SetCursor(wxCursor(wxCURSOR_HAND));
        l->Bind(wxEVT_LEFT_UP, [box](wxMouseEvent &) {
            box->SetValue(!box->GetValue());
            wxCommandEvent evt(wxEVT_TOGGLEBUTTON, box->GetId());
            evt.SetEventObject(box);
            box->GetEventHandler()->ProcessEvent(evt);
        });
        row->Add(l, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
        into->Add(row, 0, wxEXPAND | wxTOP, FromDIP(top));
        return row;
    };

    // --- name + enabled ------------------------------------------------------
    m_body->Add(make_label(_L("Name")), 0, wxBOTTOM, FromDIP(4));
    m_label_input = make_text(wxString::FromUTF8(rule.label), _L("Rule name"), _L("Evening dark mode"), 360);
    m_body->Add(m_label_input, 0, wxEXPAND);
    m_enabled = make_check(_L("Rule is enabled"), rule.enabled);
    check_row(m_enabled, _L("Enabled"), m_body, 8);

    // --- dates ---------------------------------------------------------------
    m_body->Add(make_label(_L("Dates (optional, inclusive)")), 0, wxTOP, FromDIP(16));
    {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        m_has_start_date = make_check(_L("Limit to a start date"), rule.start_date.has_value());
        row->Add(m_has_start_date, 0, wxALIGN_CENTER_VERTICAL);
        row->Add(make_label(_L("From")), 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(8));
        m_start_date = new wxDatePickerCtrl(host, wxID_ANY, rule.start_date ? wx_from_date(*rule.start_date) : wxDateTime::Today(),
                                            wxDefaultPosition, wxSize(FromDIP(140), -1), wxDP_DROPDOWN | wxDP_SHOWCENTURY);
        m_start_date->SetName(_L("Start date"));
        row->Add(m_start_date, 0, wxALIGN_CENTER_VERTICAL);
        row->AddSpacer(FromDIP(16));
        m_has_end_date = make_check(_L("Limit to an end date"), rule.end_date.has_value());
        row->Add(m_has_end_date, 0, wxALIGN_CENTER_VERTICAL);
        row->Add(make_label(_L("Until")), 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(8));
        m_end_date = new wxDatePickerCtrl(host, wxID_ANY, rule.end_date ? wx_from_date(*rule.end_date) : wxDateTime::Today(),
                                          wxDefaultPosition, wxSize(FromDIP(140), -1), wxDP_DROPDOWN | wxDP_SHOWCENTURY);
        m_end_date->SetName(_L("End date"));
        row->Add(m_end_date, 0, wxALIGN_CENTER_VERTICAL);
        m_body->Add(row, 0, wxEXPAND | wxTOP, FromDIP(4));
        auto sync_dates = [this](wxCommandEvent &e) {
            m_start_date->Enable(m_has_start_date->GetValue());
            m_end_date->Enable(m_has_end_date->GetValue());
            e.Skip();
        };
        m_has_start_date->Bind(wxEVT_TOGGLEBUTTON, sync_dates);
        m_has_end_date->Bind(wxEVT_TOGGLEBUTTON, sync_dates);
        m_start_date->Enable(rule.start_date.has_value());
        m_end_date->Enable(rule.end_date.has_value());
    }

    // --- times ---------------------------------------------------------------
    m_body->Add(make_label(_L("Time window (local time)")), 0, wxTOP, FromDIP(16));
    {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(make_label(_L("From")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
        m_start_time = new wxTimePickerCtrl(host, wxID_ANY, wx_from_time(rule.start_time), wxDefaultPosition, wxSize(FromDIP(110), -1));
        m_start_time->SetName(_L("Start time"));
        row->Add(m_start_time, 0, wxALIGN_CENTER_VERTICAL);
        row->Add(make_label(_L("to")), 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(8));
        m_end_time = new wxTimePickerCtrl(host, wxID_ANY, wx_from_time(rule.end_time), wxDefaultPosition, wxSize(FromDIP(110), -1));
        m_end_time->SetName(_L("End time"));
        row->Add(m_end_time, 0, wxALIGN_CENTER_VERTICAL);
        m_body->Add(row, 0, wxEXPAND | wxTOP, FromDIP(4));
        m_window_note = make_label(wxEmptyString, true, true);
        m_body->Add(m_window_note, 0, wxEXPAND | wxTOP, FromDIP(4));
        auto refresh_note = [this](wxEvent &e) {
            const LocalTime s = time_from_wx(m_start_time->GetValue());
            const LocalTime t = time_from_wx(m_end_time->GetValue());
            wxString        note;
            if (s == t)
                note = _L("Same start and end: the rule applies all day on the chosen days.");
            else if (t.minutes_of_day() < s.minutes_of_day())
                note = _L("The end is earlier than the start, so the window crosses midnight. The weekday is the day the window starts.");
            else
                note = _L("The window includes the start minute and ends just before the end minute.");
            note += " " + wxString::Format(_L("Times are local (%s). When the clocks change, a window spanning the change is one hour shorter or longer that night."),
                                          wxString::FromUTF8(Scheduler::timezone_name()));
            m_window_note->SetLabel(note);
            m_window_note->Wrap(FromDIP(520));
            Layout();
            e.Skip();
        };
        m_start_time->Bind(wxEVT_TIME_CHANGED, refresh_note);
        m_end_time->Bind(wxEVT_TIME_CHANGED, refresh_note);
        wxCommandEvent init;
        refresh_note(init);
    }

    // --- weekdays ------------------------------------------------------------
    m_body->Add(make_label(_L("Days")), 0, wxTOP, FromDIP(16));
    {
        m_every_day = make_check(_L("Every day"), (rule.weekdays & kEveryDay) == kEveryDay);
        check_row(m_every_day, _L("Every day"), m_body);
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        for (int d = 0; d < 7; ++d) {
            auto *box = make_check(weekday_label(d), weekday_in(rule.weekdays, d));
            m_weekdays.push_back(box);
            row->Add(box, 0, wxALIGN_CENTER_VERTICAL);
            row->Add(make_label(weekday_label(d)), 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(4));
            box->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &e) {
                bool all = true;
                for (CheckBox *b : m_weekdays)
                    all = all && b->GetValue();
                m_every_day->SetValue(all);
                e.Skip();
            });
        }
        m_body->Add(row, 0, wxEXPAND | wxTOP, FromDIP(4));
        m_every_day->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &e) {
            refresh_weekday_rows();
            e.Skip();
        });
        refresh_weekday_rows();
    }

    // --- source --------------------------------------------------------------
    m_body->Add(make_label(_L("Where the values come from")), 0, wxTOP, FromDIP(16));
    m_source = make_combo_here(_L("Value source"), {_L("This rule's own values"), _L("A settings API over HTTPS"), _L("A Home Assistant switch")}, 300);
    m_source->SetSelection(rule.source == SourceKind::Api ? 1 : rule.source == SourceKind::HomeAssistant ? 2 : 0);
    m_body->Add(m_source, 0, wxTOP, FromDIP(4));
    m_source_note = make_label(wxEmptyString, true, true);
    m_body->Add(m_source_note, 0, wxEXPAND | wxTOP, FromDIP(4));

    m_api_rows = new wxBoxSizer(wxVERTICAL);
    m_api_rows->Add(make_label(_L("API address")), 0, wxTOP, FromDIP(8));
    m_api_url = make_text(wxString::FromUTF8(rule.source_url), _L("Settings API address"), "https://example.com/bambustudio/settings", 420);
    m_api_rows->Add(m_api_url, 0, wxEXPAND | wxTOP, FromDIP(4));
    m_api_loopback = make_check(_L("Allow plain http:// to localhost for local development"), rule.allow_loopback_http);
    check_row(m_api_loopback, _L("Allow plain http:// to localhost (development only)"), m_api_rows);
    m_body->Add(m_api_rows, 0, wxEXPAND);

    m_ha_rows = new wxBoxSizer(wxVERTICAL);
    m_ha_rows->Add(make_label(_L("Home Assistant entity id")), 0, wxTOP, FromDIP(8));
    m_ha_entity = make_text(wxString::FromUTF8(rule.source_entity_id), _L("Home Assistant entity id"), "input_boolean.night_mode", 420);
    m_ha_rows->Add(m_ha_entity, 0, wxEXPAND | wxTOP, FromDIP(4));
    m_body->Add(m_ha_rows, 0, wxEXPAND);
    m_source->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &e) {
        refresh_source_rows();
        e.Skip();
    });

    // --- values --------------------------------------------------------------
    m_body->Add(make_label(_L("Settings this rule changes")), 0, wxTOP, FromDIP(16));
    m_body->Add(make_label(_L("Tick a setting to include it. Untouched settings keep your own value."), true, true), 0, wxEXPAND | wxTOP, FromDIP(2));

    AppConfig *cfg = wxGetApp().app_config;
    auto current = [&](const std::string &key) {
        auto it = rule.values.find(key);
        if (it != rule.values.end()) return it->second;
        return cfg ? cfg->get(key) : std::string();
    };
    auto add_choice = [&](const std::string &key, const wxString &title, const std::vector<wxString> &labels, const std::vector<std::string> &values) {
        ValueRow row;
        row.key           = key;
        row.choice_values = values;
        row.enabled       = make_check(wxString::Format(_L("Change %s"), title), rule.values.count(key) != 0);
        row.combo         = make_combo_here(title, labels, 260);
        const std::string cur = current(key);
        int               idx = 0;
        for (size_t i = 0; i < values.size(); ++i)
            if (values[i] == cur) { idx = int(i); break; }
        row.combo->SetSelection(idx);
        auto *line = new wxBoxSizer(wxHORIZONTAL);
        line->Add(row.enabled, 0, wxALIGN_CENTER_VERTICAL);
        auto *l = make_label(title);
        l->SetMinSize(wxSize(FromDIP(150), -1));
        line->Add(l, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
        line->Add(row.combo, 0, wxALIGN_CENTER_VERTICAL);
        m_body->Add(line, 0, wxEXPAND | wxTOP, FromDIP(4));
        m_values.push_back(row);
    };
    auto add_text = [&](const std::string &key, const wxString &title, const wxString &hint) {
        ValueRow row;
        row.key     = key;
        row.enabled = make_check(wxString::Format(_L("Change %s"), title), rule.values.count(key) != 0);
        row.text    = make_text(wxString::FromUTF8(current(key)), title, hint, 260);
        auto *line  = new wxBoxSizer(wxHORIZONTAL);
        line->Add(row.enabled, 0, wxALIGN_CENTER_VERTICAL);
        auto *l = make_label(title);
        l->SetMinSize(wxSize(FromDIP(150), -1));
        line->Add(l, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
        line->Add(row.text, 0, wxALIGN_CENTER_VERTICAL);
        m_body->Add(line, 0, wxEXPAND | wxTOP, FromDIP(4));
        m_values.push_back(row);
    };

    add_choice(kKeyLanguageMode, _L("Language mode"),
               {_L("English"), _L("Cantonese (Hong Kong)"), _L("Bilingual: English and Cantonese")},
               {I18N::LANGUAGE_MODE_ENGLISH_US, I18N::LANGUAGE_MODE_CANTONESE_HONG_KONG, I18N::LANGUAGE_MODE_ENGLISH_CANTONESE_HK});
    add_choice(kKeyTheme, _L("Theme"), {_L("Light"), _L("Dark")}, {"0", "1"});
    add_choice(kKeyDensity, _L("Density"), {_L("Comfortable"), _L("Compact")}, {"comfortable", "compact"});
    add_text(kKeyAccentSeed, _L("Accent color"), "#146c2e");
    {
        // Installed faces, the same list the Appearance section offers.
        std::vector<wxString>    labels = {_L("Default (Roboto)")};
        std::vector<std::string> values = {""};
        wxArrayString            faces  = wxFontEnumerator::GetFacenames();
        std::vector<wxString>    sorted(faces.begin(), faces.end());
        std::sort(sorted.begin(), sorted.end(), [](const wxString &a, const wxString &b) { return a.CmpNoCase(b) < 0; });
        sorted.erase(std::unique(sorted.begin(), sorted.end(), [](const wxString &a, const wxString &b) { return a.CmpNoCase(b) == 0; }), sorted.end());
        for (const wxString &f : sorted) {
            if (f.StartsWith("@")) continue;
            labels.push_back(f);
            values.push_back(into_u8(f));
        }
        // A face saved earlier but not installed here still round-trips.
        const std::string cur = current(kKeyFontFamily);
        if (!cur.empty() && std::find(values.begin(), values.end(), cur) == values.end()) {
            labels.push_back(wxString::FromUTF8(cur) + " " + _L("(not installed)"));
            values.push_back(cur);
        }
        add_choice(kKeyFontFamily, _L("Font"), labels, values);
    }
    add_choice(kKeyFontScale, _L("Text size"), {_L("Small"), _L("Default"), _L("Large")}, {"0.9", "1.0", "1.15"});
    add_choice(kKeyFunnyLevelEnglish, _L("Funny level (English)"), {"1", "2", "3", "4", "5"}, {"1", "2", "3", "4", "5"});
    add_choice(kKeyFunnyLevelCantonese, _L("Funny level (Cantonese)"), {"1", "2", "3", "4", "5"}, {"1", "2", "3", "4", "5"});
    add_text(kKeyDisplayName, _L("App name"), wxString::FromUTF8(SLIC3R_APP_FULL_NAME));

    m_problems = make_label(wxEmptyString, false, true);
    m_problems->SetForegroundColour(StateColor::semantic(MD3::Role::Error));
    m_body->Add(m_problems, 0, wxEXPAND | wxTOP, FromDIP(12));

    refresh_source_rows();

    // --- footer --------------------------------------------------------------
    auto *cancel = AddFooterButton(new Button(this, _L("Cancel")));
    cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
    auto *save = AddFooterButton(new Button(this, _L("Save rule")));
    save->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        Rule                     out = m_rule;
        std::vector<std::string> problems;
        if (!collect(out, problems)) {
            show_problems(problems);
            return;
        }
        m_rule = out;
        EndModal(wxID_OK);
    });

    SetMinSize(wxSize(FromDIP(640), FromDIP(520)));
    SetSize(wxSize(FromDIP(680), FromDIP(700)));
    Layout();
    CenterOnParent();
}

void ScheduleRuleDialog::refresh_weekday_rows()
{
    const bool every = m_every_day->GetValue();
    for (CheckBox *b : m_weekdays) {
        if (every)
            b->SetValue(true);
        b->Enable(!every);
    }
}

void ScheduleRuleDialog::refresh_source_rows()
{
    const int sel = m_source->GetSelection();
    m_body->Show(m_api_rows, sel == 1, true);
    m_body->Show(m_ha_rows, sel == 2, true);
    wxString note;
    switch (sel) {
    case 1:
        note = _L("Inside the time window the API is read every minute. It must answer {\"schemaVersion\": 1, \"values\": {...}} with only the settings listed below; anything else is ignored. The values ticked below are the fallback for settings the API leaves out. Redirects and addresses with a user name or password are refused.");
        break;
    case 2:
        note = _L("Inside the time window the switch is read every minute through the Home Assistant connection from Smart home. \"on\" applies the ticked values; \"off\" or no answer leaves your own settings in force.");
        break;
    default:
        note = _L("The ticked values apply for the whole time window.");
        break;
    }
    m_source_note->SetLabel(note);
    m_source_note->Wrap(FromDIP(520));
    Layout();
}

void ScheduleRuleDialog::show_problems(const std::vector<std::string> &problems)
{
    wxString text;
    for (const std::string &p : problems) {
        if (!text.empty()) text += "\n";
        text += wxString::FromUTF8(p);
    }
    m_problems->SetLabel(text);
    m_problems->Wrap(FromDIP(520));
    Layout();
}

bool ScheduleRuleDialog::collect(Rule &out, std::vector<std::string> &problems)
{
    out.label   = into_u8(m_label_input->GetTextCtrl()->GetValue().Strip(wxString::both));
    out.enabled = m_enabled->GetValue();
    out.start_date.reset();
    out.end_date.reset();
    if (m_has_start_date->GetValue())
        out.start_date = date_from_wx(m_start_date->GetValue());
    if (m_has_end_date->GetValue())
        out.end_date = date_from_wx(m_end_date->GetValue());
    out.start_time = time_from_wx(m_start_time->GetValue());
    out.end_time   = time_from_wx(m_end_time->GetValue());
    if (m_every_day->GetValue()) {
        out.weekdays = kEveryDay;
    } else {
        out.weekdays = kNoDay;
        for (int d = 0; d < 7; ++d)
            if (m_weekdays[d]->GetValue()) out.weekdays |= weekday_bit(d);
    }
    switch (m_source->GetSelection()) {
    case 1: out.source = SourceKind::Api; break;
    case 2: out.source = SourceKind::HomeAssistant; break;
    default: out.source = SourceKind::Local; break;
    }
    out.source_url          = into_u8(m_api_url->GetTextCtrl()->GetValue().Strip(wxString::both));
    out.allow_loopback_http = m_api_loopback->GetValue();
    out.source_entity_id    = into_u8(m_ha_entity->GetTextCtrl()->GetValue().Strip(wxString::both).Lower());
    out.values.clear();
    for (const ValueRow &row : m_values) {
        if (!row.enabled->GetValue()) continue;
        std::string value;
        if (row.combo) {
            const int idx = row.combo->GetSelection();
            if (idx >= 0 && idx < int(row.choice_values.size())) value = row.choice_values[idx];
        } else if (row.text) {
            value = into_u8(row.text->GetTextCtrl()->GetValue().Strip(wxString::both));
        }
        out.values[row.key] = value;
    }
    problems = validate_rule(out);
    if (out.source == SourceKind::Api) {
        const std::string url_problem = validate_api_url(out.source_url, out.allow_loopback_http);
        if (!url_problem.empty()) problems.push_back(url_problem);
    }
    return problems.empty();
}

// ---------------------------------------------------------------------------
// Panel
// ---------------------------------------------------------------------------

} // namespace

ScheduledSettingsPanel::ScheduledSettingsPanel(wxWindow *parent)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL)
{
    SetScrollRate(0, FromDIP(12));
    const wxColour bg = StateColor::semantic(MD3::Role::Surface);
    SetBackgroundColour(bg);
    auto *sizer = new wxBoxSizer(wxVERTICAL);
    constexpr int kLeft = 48 + 16, kRight = 24;

    auto make_label = [&](const wxString &text, const wxFont &font, MD3::Role role, bool wrap = false) {
        auto *l = new Label(this, font, text, wrap ? LB_AUTO_WRAP : 0);
        l->SetBackgroundColour(bg);
        l->SetForegroundColour(StateColor::semantic(role));
        if (wrap)
            l->SetMinSize(wxSize(0, -1));
        return l;
    };
    auto add_row = [&](wxSizer *row, int top) {
        auto *line = new wxBoxSizer(wxHORIZONTAL);
        line->AddSpacer(FromDIP(kLeft));
        line->Add(row, 1, wxEXPAND | wxRIGHT, FromDIP(kRight));
        sizer->Add(line, 0, wxEXPAND | wxTOP, FromDIP(top));
        m_search_rows.push_back(line);
        return line;
    };
    auto wrap_window = [&](wxWindow *w) {
        auto *s = new wxBoxSizer(wxHORIZONTAL);
        s->Add(w, 1, wxEXPAND);
        return s;
    };

    auto *title = make_label(_L("Schedules"), Label::Head_14, MD3::Role::OnSurface);
    add_row(wrap_window(title), 24);
    auto *intro = make_label(
        _L("Change the language mode, theme, density, accent, font, text size, funny levels or app name at chosen times, from a settings API, or from a Home Assistant switch. When a rule ends, your own values come back. Later rules in the list win when two rules set the same setting."),
        Label::Body_12, MD3::Role::OnSurfaceVariant, true);
    add_row(wrap_window(intro), 8);
    m_timezone = make_label(wxEmptyString, Label::Body_12, MD3::Role::OnSurfaceVariant, true);
    add_row(wrap_window(m_timezone), 4);

    m_search = new SearchField(this, _L("Search rules"));
    m_search->SetName(_L("Search rules"));
    add_row(wrap_window(m_search), 12);
    m_search->SetOnQuery([this](const wxString &) { rebuild_list(); });
    m_search->SetOnRegexToggle([this](bool) { rebuild_list(); });

    m_list = new ListBox(this, wxID_ANY, wxSize(-1, FromDIP(180)));
    m_list->SetName(_L("Schedule rules"));
    add_row(wrap_window(m_list), 8);
    m_empty = make_label(_L("No rules yet. Add one to change a setting on a schedule."), Label::Body_12, MD3::Role::OnSurfaceVariant, true);
    add_row(wrap_window(m_empty), 4);

    auto *buttons = new wxBoxSizer(wxHORIZONTAL);
    auto  make_button = [&](const wxString &text, Button *&store) {
        store = new Button(this, text);
        store->SetName(text);
        buttons->Add(store, 0, wxRIGHT, FromDIP(8));
        return store;
    };
    Button *add = nullptr;
    make_button(_L("Add rule"), add);
    make_button(_L("Edit"), m_edit);
    make_button(_L("Disable"), m_toggle);
    make_button(_L("Move up"), m_up);
    make_button(_L("Move down"), m_down);
    make_button(_L("Delete"), m_delete);
    add_row(buttons, 8);

    m_detail = make_label(wxEmptyString, Label::Body_12, MD3::Role::OnSurfaceVariant, true);
    add_row(wrap_window(m_detail), 8);
    m_status = make_label(wxEmptyString, Label::Body_12, MD3::Role::OnSurfaceVariant, true);
    add_row(wrap_window(m_status), 4);

    add->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { add_rule(); });
    m_edit->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { edit_selected(); });
    m_toggle->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { toggle_selected(); });
    m_up->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { move_selected(-1); });
    m_down->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { move_selected(1); });
    m_delete->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { delete_selected(); });
    m_list->Bind(wxEVT_LISTBOX, [this](wxCommandEvent &) { refresh_status(); });
    m_list->Bind(wxEVT_LISTBOX_DCLICK, [this](wxCommandEvent &) { edit_selected(); });

    SetSizer(sizer);
    Scheduler::instance().set_change_listener([this]() {
        rebuild_list();
    });
    rebuild_list();
}

ScheduledSettingsPanel::~ScheduledSettingsPanel()
{
    Scheduler::instance().set_change_listener({});
}

void ScheduledSettingsPanel::Rescale()
{
    if (m_list) m_list->Rescale();
}

int ScheduledSettingsPanel::selected_document_index() const
{
    const int row = m_list ? m_list->GetSelection() : wxNOT_FOUND;
    if (row < 0 || row >= int(m_visible.size())) return -1;
    return m_visible[row];
}

void ScheduledSettingsPanel::rebuild_list()
{
    const int previous = selected_document_index();
    const Document &doc = Scheduler::instance().document();
    Scheduler       &scheduler = Scheduler::instance();
    const wxString   query = m_search->GetValue();
    SearchField::MatchPass pass(query, m_search->IsRegexEnabled(), m_search->IsCaseSensitive(), m_search->IsWholeWord(), m_search->IsMultiline());

    m_visible.clear();
    std::vector<wxString> rows;
    for (size_t i = 0; i < doc.rules.size(); ++i) {
        const Rule      &rule   = doc.rules[i];
        const RuleStatus status = scheduler.status(rule.id);
        wxString         source;
        switch (rule.source) {
        case SourceKind::Api: source = _L("API"); break;
        case SourceKind::HomeAssistant: source = _L("Home Assistant"); break;
        case SourceKind::Local: source = _L("local"); break;
        }
        wxString state = !rule.enabled ? _L("disabled") : status.active ? _L("active now") : status.calendar_matches ? _L("in window, waiting for source") : _L("waiting for its window");
        wxString row = wxString::Format("%s  |  %s  |  %s  |  %s", wxString::FromUTF8(rule.label), wxString::FromUTF8(describe_window(rule)), source, state);
        if (!query.empty() && !pass.matches(row)) continue;
        m_visible.push_back(int(i));
        rows.push_back(row);
    }
    m_list->Set(rows);
    m_empty->Show(doc.rules.empty());
    if (!doc.rules.empty() && rows.empty())
        m_empty->SetLabel(_L("No rules match your search."));
    else
        m_empty->SetLabel(_L("No rules yet. Add one to change a setting on a schedule."));
    m_empty->Show(rows.empty());
    for (size_t r = 0; r < m_visible.size(); ++r)
        if (m_visible[r] == previous) { m_list->SetSelection(int(r)); break; }
    m_timezone->SetLabel(wxString::Format(_L("Times are your computer's local time, currently %s. Daylight-saving changes move the wall clock; a window spanning the change is an hour shorter or longer that night."),
                                          wxString::FromUTF8(Scheduler::timezone_name())));
    refresh_status();
    Layout();
}

void ScheduledSettingsPanel::refresh_status()
{
    const int        index = selected_document_index();
    const Document  &doc   = Scheduler::instance().document();
    const bool       has   = index >= 0 && index < int(doc.rules.size());
    m_edit->Enable(has);
    m_toggle->Enable(has);
    m_delete->Enable(has);
    m_up->Enable(has && index > 0);
    m_down->Enable(has && index + 1 < int(doc.rules.size()));
    const wxString why = _L("Select a rule first.");
    m_edit->SetToolTip(has ? _L("Edit the selected rule") : why);
    m_toggle->SetToolTip(has ? _L("Turn the selected rule on or off") : why);
    m_delete->SetToolTip(has ? _L("Delete the selected rule") : why);
    m_up->SetToolTip(!has ? why : index > 0 ? _L("Move the rule earlier; later rules win conflicts") : _L("Already first"));
    m_down->SetToolTip(!has ? why : index + 1 < int(doc.rules.size()) ? _L("Move the rule later; later rules win conflicts") : _L("Already last"));
    if (has) {
        const Rule      &rule   = doc.rules[index];
        const RuleStatus status = Scheduler::instance().status(rule.id);
        m_toggle->SetLabel(rule.enabled ? _L("Disable") : _L("Enable"));
        wxString detail;
        wxString values;
        for (const auto &[key, value] : rule.values) {
            if (!values.empty()) values += ", ";
            values += wxString::FromUTF8(key) + "=" + wxString::FromUTF8(value);
        }
        detail = _L("Sets:") + " " + (values.empty() ? _L("(values come from the API)") : values);
        if (!status.source_note.empty())
            detail += "\n" + wxString::FromUTF8(status.source_note);
        if (!status.owned_keys.empty())
            detail += "\n" + _L("Currently controlling:") + " " + wxString::FromUTF8(status.owned_keys);
        m_detail->SetLabel(detail);
    } else {
        m_toggle->SetLabel(_L("Disable"));
        m_detail->SetLabel(wxEmptyString);
    }
    m_status->SetLabel(wxString::FromUTF8(Scheduler::instance().last_evaluated_text()) + ". " + _L("Rules are checked every minute."));
    Layout();
}

bool ScheduledSettingsPanel::commit(const Document &document)
{
    std::string error;
    if (!Scheduler::instance().save_document(document, error)) {
        m_status->SetLabel(wxString::FromUTF8(error));
        Layout();
        return false;
    }
    rebuild_list();
    return true;
}

void ScheduledSettingsPanel::add_rule()
{
    Rule rule;
    rule.id         = unique_rule_id(Scheduler::instance().document(), entropy_now());
    rule.start_time = {20, 0};
    rule.end_time   = {7, 0};
    ScheduleRuleDialog dlg(this, rule, true);
    if (dlg.ShowModal() != wxID_OK) return;
    Document doc = Scheduler::instance().document();
    doc.rules.push_back(dlg.result());
    if (commit(doc)) {
        for (size_t r = 0; r < m_visible.size(); ++r)
            if (m_visible[r] == int(doc.rules.size()) - 1) m_list->SetSelection(int(r));
        refresh_status();
    }
}

void ScheduledSettingsPanel::edit_selected()
{
    const int index = selected_document_index();
    Document  doc   = Scheduler::instance().document();
    if (index < 0 || index >= int(doc.rules.size())) return;
    ScheduleRuleDialog dlg(this, doc.rules[index], false);
    if (dlg.ShowModal() != wxID_OK) return;
    doc.rules[index] = dlg.result();
    commit(doc);
}

void ScheduledSettingsPanel::toggle_selected()
{
    const int index = selected_document_index();
    Document  doc   = Scheduler::instance().document();
    if (index < 0 || index >= int(doc.rules.size())) return;
    doc.rules[index].enabled = !doc.rules[index].enabled;
    commit(doc);
}

void ScheduledSettingsPanel::move_selected(int delta)
{
    const int index = selected_document_index();
    Document  doc   = Scheduler::instance().document();
    const int target = index + delta;
    if (index < 0 || target < 0 || index >= int(doc.rules.size()) || target >= int(doc.rules.size())) return;
    std::swap(doc.rules[index], doc.rules[target]);
    if (commit(doc)) {
        for (size_t r = 0; r < m_visible.size(); ++r)
            if (m_visible[r] == target) m_list->SetSelection(int(r));
        refresh_status();
    }
}

void ScheduledSettingsPanel::delete_selected()
{
    const int index = selected_document_index();
    Document  doc   = Scheduler::instance().document();
    if (index < 0 || index >= int(doc.rules.size())) return;
    const Rule &rule = doc.rules[index];
    // Deleting a rule is irreversible for the rule itself (the preferences
    // history still holds the previous document). Ask before it goes.
    MessageDialog confirm(this,
                          wxString::Format(_L("Delete the schedule rule \"%s\"?\nIts settings go back to your own values at the next check. The rule itself is not recoverable from this screen."),
                                           wxString::FromUTF8(rule.label)),
                          _L("Delete schedule rule"), wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
    if (confirm.ShowModal() != wxID_YES) return;
    doc.rules.erase(doc.rules.begin() + index);
    commit(doc);
}

} } } // namespace Slic3r::GUI::Schedule
