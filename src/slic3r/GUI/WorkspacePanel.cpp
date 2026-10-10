#include "HumanDate.hpp"
#include "WorkspacePanel.hpp"
#include "Widgets/MD3DataView.hpp"
#include "Widgets/MD3ScrolledWindow.hpp"

#include "I18N.hpp"
#include "GUI_App.hpp"
#include "MsgDialog.hpp"
#include "NotificationManager.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/WorkspacePlanner.hpp"
#include "wxExtensions.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/ListBox.hpp"
#include "Widgets/StateColor.hpp"
#include "Widgets/TextArea.hpp"
#include "Widgets/TextTabbar.hpp"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>

#include <wx/dataview.h>
#include <wx/containr.h>
#include <wx/filedlg.h>
// The generic calendar's header declares only the control: its base class,
// styles and events come from <wx/calctrl.h>, which has to come first.
#include <wx/calctrl.h>
#include <wx/generic/calctrlg.h>
#include <wx/msgdlg.h>
#include <wx/simplebook.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/stdpaths.h>
#include <wx/textctrl.h>
#include <wx/wrapsizer.h>

namespace workspace_fs = std::filesystem;

namespace Slic3r::GUI {
namespace {

wxString display(const std::string &text) { return wxString::FromUTF8(text); }
std::string utf8(const wxString &text) { return std::string(text.ToUTF8()); }

wxString local_time_error(const std::string &error)
{
    if (error == "Invalid local date or time")
        return _L("Invalid local date or time");
    if (error == "Unknown or unavailable IANA time zone")
        return _L("Unknown or unavailable IANA time zone");
    if (error == "Could not resolve time-zone rules")
        return _L("Could not resolve time-zone rules");
    if (error == "IANA time-zone rules are unavailable on this platform")
        return _L("IANA time-zone rules are unavailable on this platform");
    return _L("Time zone rules are unavailable");
}

std::int64_t parse_wall_utc(const std::string &text, int offset_minutes)
{
    std::tm value{};
    std::istringstream input(text);
    input >> std::get_time(&value, "%Y-%m-%d %H:%M");
    if (input.fail() || !input.eof() || value.tm_year < 70 || offset_minutes < -840 || offset_minutes > 840)
        return -1;
#ifdef _WIN32
    const std::time_t instant = _mkgmtime(&value);
#else
    const std::time_t instant = timegm(&value);
#endif
    return instant < 0 ? -1 : static_cast<std::int64_t>(instant) - offset_minutes * 60;
}

std::optional<Workspace::LocalInstant> choose_local_time(wxWindow *parent, const std::string &zone,
                                                         const wxString &wall)
{
    const auto resolved = Workspace::resolve_local_time(zone, utf8(wall));
    if (!resolved.error.empty() || resolved.candidates.empty()) {
        md3_message_box(resolved.error.empty() ? _L("This local time does not exist because the clock jumps forward.") :
                     local_time_error(resolved.error), _L("Planned print time"), wxOK | wxICON_WARNING, parent);
        return std::nullopt;
    }
    if (resolved.candidates.size() == 1) return resolved.candidates.front();
    const auto choice = md3_message_box(
        wxString::Format(_L("This local time occurs twice. Use the earlier occurrence (UTC offset %d minutes)? Choose No for the later occurrence (UTC offset %d minutes)."),
            resolved.candidates.front().offset_minutes, resolved.candidates.back().offset_minutes),
        _L("Choose daylight-saving occurrence"), wxYES_NO | wxCANCEL | wxICON_QUESTION, parent);
    if (choice == wxYES) return resolved.candidates.front();
    if (choice == wxNO) return resolved.candidates.back();
    return std::nullopt;
}

bool ask_text(wxWindow *parent, const wxString &caption, const wxString &prompt, wxString &value)
{
    TextEntryDialog dialog(parent, prompt, caption, value);
    if (dialog.ShowModal() != wxID_OK) return false;
    value = dialog.GetValue();
    return true;
}

bool write_export(wxWindow *parent, const wxString &name, const wxString &filter, const std::string &content)
{
    wxFileDialog dialog(parent, name, wxEmptyString, wxEmptyString, filter, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return false;
    std::ofstream output(workspace_fs::u8path(utf8(dialog.GetPath())), std::ios::binary | std::ios::trunc);
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (output.good()) return true;
    md3_message_box(_L("Could not write the selected export file."), name, wxOK | wxICON_WARNING, parent);
    return false;
}

} // namespace

WorkspacePanel::WorkspacePanel(wxWindow *parent) : wxPanel(parent)
{
    m_staging_root = workspace_fs::u8path(utf8(wxStandardPaths::Get().GetUserLocalDataDir())) / "workspace-staging";
    std::error_code ignored;
    workspace_fs::create_directories(m_staging_root, ignored);
    m_workspace.id = Workspace::new_id();
    m_workspace.title = "New workspace";
    create_ui();
    refresh_all();
    m_reminder_timer.SetOwner(this);
    Bind(wxEVT_TIMER, [this](wxTimerEvent &) { check_reminders(); }, m_reminder_timer.GetId());
    Bind(wxEVT_SHOW, [this](wxShowEvent &event) {
        if (event.IsShown()) check_reminders();
        event.Skip();
    });
    m_reminder_timer.Start(60000);
}

WorkspacePanel::~WorkspacePanel()
{
    m_reminder_timer.Stop();
    if (!m_loaded_staging.empty()) {
        std::error_code ignored;
        workspace_fs::remove_all(m_loaded_staging, ignored);
    }
    for (const auto &file : m_owned_member_files) {
        std::error_code ignored;
        workspace_fs::remove(file, ignored);
    }
}

void WorkspacePanel::create_ui()
{
    const int padding = FromDIP(MD3::Metrics::active().padding);
    const int gap = FromDIP(MD3::Metrics::active().gap);
    SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    auto *root = new wxBoxSizer(wxVERTICAL);
    auto *actions = new wxWrapSizer(wxHORIZONTAL);
    const auto style_action = [this](Button *control, Button::Variant variant) {
        control->SetVariant(variant);
        control->SetButtonSize(MD3::Metrics::isCompact() ? Button::Size::Medium : Button::Size::Large);
        control->SetName(control->GetLabel());
        m_action_buttons.push_back(control);
    };
    const auto button = [this, actions, &style_action, gap](const wxString &label, void (WorkspacePanel::*action)()) {
        auto *control = new Button(this, label);
        style_action(control, action == &WorkspacePanel::choose_save ? Button::Variant::Filled : Button::Variant::Outlined);
        m_spacing.emplace_back(actions->Add(control, 0, wxRIGHT | wxBOTTOM, gap), true);
        control->Bind(wxEVT_BUTTON, [this, action](wxCommandEvent &) { (this->*action)(); });
    };
    button(_L("New workspace"), &WorkspacePanel::create_new);
    button(_L("Open workspace"), &WorkspacePanel::choose_open);
    button(_L("Save workspace"), &WorkspacePanel::choose_save);
    m_spacing.emplace_back(root->Add(actions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, padding), false);

    m_section_tabs = new TextTabbar(this, TextTabbar::Align::Left);
    m_sections = new wxSimplebook(this, wxID_ANY);
    m_sections->SetBackgroundColour(GetBackgroundColour());
    // Each real page retains its controls inside a card. The outer scroll owner
    // keeps wrapped action rows reachable when their measured height grows.
    const auto make_page = [this, padding]() {
        auto *scroll = new MD3ScrolledWindow(m_sections, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                             wxVSCROLL | wxTAB_TRAVERSAL);
        scroll->SetScrollRate(0, FromDIP(16));
        scroll->SetBackgroundColour(GetBackgroundColour());
        auto *page = new wxNavigationEnabled<StaticBox>();
        page->Create(scroll, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        page->SetBackgroundColor(MD3::Light::scLowest);
        page->SetBorderColor(MD3::Light::outlineVariant);
        auto *body = new wxBoxSizer(wxVERTICAL);
        m_spacing.emplace_back(body->Add(page, 1, wxEXPAND | wxALL, padding), false);
        scroll->SetSizer(body);
        scroll->Bind(wxEVT_SIZE, [this](wxSizeEvent &event) { reflow(); event.Skip(); });
        m_pages.push_back(scroll);
        m_cards.push_back(page);
        return page;
    };
    const auto add_section = [this](wxWindow *page, const wxString &label) {
        m_sections->AddPage(page->GetParent(), label);
        m_section_tabs->AddTab(label);
    };
    const auto heading = [this, padding](wxWindow *page, wxSizer *sizer, const wxString &label) {
        auto *title = new Label(page, ::Label::Head_16, label, LB_AUTO_WRAP);
        // The sizer owns the width; the label wraps to whatever it is given.
        title->SetMinSize(wxSize(0, -1));
        m_headings.push_back(title);
        m_spacing.emplace_back(sizer->Add(title, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, padding), false);
    };
    auto *overview_page = make_page();
    auto *overview_sizer = new wxBoxSizer(wxVERTICAL);
    heading(overview_page, overview_sizer, _L("Overview"));
    m_overview = new Label(overview_page, wxEmptyString, LB_AUTO_WRAP);
    m_overview->SetMinSize(wxSize(0, -1));
    m_spacing.emplace_back(overview_sizer->Add(m_overview, 0, wxEXPAND | wxALL, padding), false);
    auto *overview_actions = new wxWrapSizer(wxHORIZONTAL);
    auto *rename = new Button(overview_page, _L("Rename workspace"));
    style_action(rename, Button::Variant::Tonal);
    m_spacing.emplace_back(overview_actions->Add(rename, 0, wxRIGHT | wxBOTTOM, gap), true);
    rename->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        wxString title = display(m_workspace.title);
        if (ask_text(this, _L("Workspace title"), _L("Name this workspace"), title)) {
            m_workspace.title = utf8(title); m_dirty = true; refresh_overview();
        }
    });
    auto *preferences = new Button(overview_page, _L("Time zone and reminders"));
    style_action(preferences, Button::Variant::Outlined);
    m_spacing.emplace_back(overview_actions->Add(preferences, 0, wxRIGHT | wxBOTTOM, gap), true);
    preferences->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { edit_preferences(); });
    m_spacing.emplace_back(overview_sizer->Add(overview_actions, 0, wxEXPAND | wxALL, padding), false);
    overview_page->SetSizer(overview_sizer);
    add_section(overview_page, _L("Overview"));

    auto *files_page = make_page();
    auto *files_sizer = new wxBoxSizer(wxVERTICAL);
    heading(files_page, files_sizer, _L("Files"));
    m_files = new MD3DataViewListCtrl(files_page, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                     wxDV_SINGLE | wxDV_ROW_LINES | wxBORDER_NONE);
    m_files->AppendTextColumn(_L("Member"), wxDATAVIEW_CELL_INERT, FromDIP(200), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_files->AppendTextColumn(_L("Project 3MF"), wxDATAVIEW_CELL_INERT, FromDIP(260), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_files->AppendTextColumn(_L("Editable sources"), wxDATAVIEW_CELL_INERT, FromDIP(140), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    wxGetApp().UpdateDVCDarkUI(m_files); // native header follows the theme
    md3_style_data_view(m_files);
    m_files->SetMinSize(FromDIP(wxSize(1, 160)));
    m_spacing.emplace_back(files_sizer->Add(m_files, 1, wxEXPAND | wxALL, padding), false);
    auto *file_actions = new wxWrapSizer(wxHORIZONTAL);
    auto *add_project = new Button(files_page, _L("Add project 3MF"));
    auto *add_editable = new Button(files_page, _L("Add editable source"));
    auto *open_project = new Button(files_page, _L("Open selected project"));
    style_action(add_project, Button::Variant::Tonal);
    style_action(add_editable, Button::Variant::Outlined);
    style_action(open_project, Button::Variant::Filled);
    m_spacing.emplace_back(file_actions->Add(add_project, 0, wxRIGHT | wxBOTTOM, gap), true);
    m_spacing.emplace_back(file_actions->Add(add_editable, 0, wxRIGHT | wxBOTTOM, gap), true);
    m_spacing.emplace_back(file_actions->Add(open_project, 0, wxRIGHT | wxBOTTOM, gap), true);
    add_project->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { add_member(); });
    add_editable->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { add_source(); });
    open_project->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { open_selected_member(); });
    m_files->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, [this](wxDataViewEvent &event) {
        const int row = m_files->ItemToRow(event.GetItem());
        if (row == wxNOT_FOUND) return;
        m_files->SelectRow(static_cast<unsigned>(row));
        open_selected_member();
    });
    m_spacing.emplace_back(files_sizer->Add(file_actions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, padding), false);
    files_page->SetSizer(files_sizer);
    add_section(files_page, _L("Files"));

    auto *list_page = make_page();
    auto *list_sizer = new wxBoxSizer(wxVERTICAL);
    heading(list_page, list_sizer, _L("Checklist"));
    m_checklist = new ListBox(list_page, wxID_ANY);
    m_checklist->EnableChecks();
    m_checklist->SetMinSize(FromDIP(wxSize(1, 160)));
    m_spacing.emplace_back(list_sizer->Add(m_checklist, 1, wxEXPAND | wxALL, padding), false);
    m_checklist->Bind(wxEVT_CHECKLISTBOX, [this](wxCommandEvent &event) {
        const int index = event.GetInt();
        if (index >= 0 && static_cast<std::size_t>(index) < m_workspace.checklist.size()) {
            m_workspace.checklist[index].completed = m_checklist->IsChecked(index);
            m_dirty = true;
        }
    });
    auto *list_actions = new wxWrapSizer(wxHORIZONTAL);
    const auto list_button = [this, list_page, list_actions, &style_action, gap](const wxString &label, void (WorkspacePanel::*action)()) {
        auto *control = new Button(list_page, label);
        style_action(control, action == &WorkspacePanel::add_checklist ? Button::Variant::Filled : Button::Variant::Tonal);
        m_spacing.emplace_back(list_actions->Add(control, 0, wxRIGHT | wxBOTTOM, gap), true);
        control->Bind(wxEVT_BUTTON, [this, action](wxCommandEvent &) { (this->*action)(); });
    };
    list_button(_L("Add"), &WorkspacePanel::add_checklist);
    list_button(_L("Edit / due date / link"), &WorkspacePanel::edit_checklist);
    auto *up = new Button(list_page, _L("Move up"));
    auto *down = new Button(list_page, _L("Move down"));
    style_action(up, Button::Variant::Outlined);
    style_action(down, Button::Variant::Outlined);
    m_spacing.emplace_back(list_actions->Add(up, 0, wxRIGHT | wxBOTTOM, gap), true);
    m_spacing.emplace_back(list_actions->Add(down, 0, wxRIGHT | wxBOTTOM, gap), true);
    up->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { move_checklist(-1); });
    down->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { move_checklist(1); });
    auto *json_button = new Button(list_page, _L("Export JSON"));
    auto *csv_button = new Button(list_page, _L("Export CSV"));
    style_action(json_button, Button::Variant::Outlined);
    style_action(csv_button, Button::Variant::Outlined);
    m_spacing.emplace_back(list_actions->Add(json_button, 0, wxRIGHT | wxBOTTOM, gap), true);
    m_spacing.emplace_back(list_actions->Add(csv_button, 0, wxRIGHT | wxBOTTOM, gap), true);
    json_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { export_checklist(false); });
    csv_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { export_checklist(true); });
    m_spacing.emplace_back(list_sizer->Add(list_actions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, padding), false);
    list_page->SetSizer(list_sizer);
    add_section(list_page, _L("Checklist"));

    auto *notes_page = make_page();
    auto *notes_sizer = new wxBoxSizer(wxVERTICAL);
    heading(notes_page, notes_sizer, _L("Notes"));
    // m_notes stays a wxTextCtrl*: it points at the kit TextArea's native
    // editor, so every existing GetValue()/ChangeValue() caller below keeps
    // working unchanged. The wrapper itself is owned by notes_page's sizer.
    auto *notes_area = new TextArea(notes_page, wxEmptyString, wxDefaultSize, wxTE_MULTILINE);
    notes_area->SetMinLines(8);
    m_notes = notes_area->GetTextCtrl();
    m_notes->SetName(_L("Notes"));
    m_spacing.emplace_back(notes_sizer->Add(notes_area, 1, wxEXPAND | wxALL, padding), false);
    m_notes->Bind(wxEVT_TEXT, [this](wxCommandEvent &) {
        m_workspace.notes = utf8(m_notes->GetValue()); m_dirty = true;
    });
    notes_page->SetSizer(notes_sizer);
    add_section(notes_page, _L("Notes"));

    auto *calendar_page = make_page();
    auto *calendar_sizer = new wxBoxSizer(wxVERTICAL);
    heading(calendar_page, calendar_sizer, _L("Calendar"));
    m_calendar_columns = new wxBoxSizer(wxHORIZONTAL);
    // The generic calendar paints itself in the colours it is given; the native one
    // was the system month control. Sequential month selection draws its own month
    // header with arrows instead of a native choice and spin control.
    m_month = new wxGenericCalendarCtrl(calendar_page, wxID_ANY, wxDefaultDateTime, wxDefaultPosition, wxDefaultSize,
                                        wxCAL_SEQUENTIAL_MONTH_SELECTION | wxCAL_SHOW_SURROUNDING_WEEKS);
    m_month->SetFont(::Label::Body_13);
    m_month->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    m_month->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    m_month->SetHeaderColours(StateColor::semantic(MD3::Role::OnSurfaceVariant), StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    m_month->SetHighlightColours(StateColor::semantic(MD3::Role::OnPrimary), StateColor::semantic(MD3::Role::Primary));
    m_calendar_month_item = m_calendar_columns->Add(m_month, 0, wxRIGHT, gap);
    m_month->Bind(wxEVT_CALENDAR_SEL_CHANGED, [this](wxCalendarEvent &) { refresh_calendar(); });
    m_agenda = new MD3DataViewListCtrl(calendar_page, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                      wxDV_SINGLE | wxDV_ROW_LINES | wxBORDER_NONE);
    m_agenda->AppendTextColumn(_L("Planned print"), wxDATAVIEW_CELL_INERT, FromDIP(200), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_agenda->AppendTextColumn(_L("Printer"), wxDATAVIEW_CELL_INERT, FromDIP(140), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_agenda->AppendTextColumn(_L("Start (local time)"), wxDATAVIEW_CELL_INERT, FromDIP(160), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_agenda->AppendTextColumn(_L("Status"), wxDATAVIEW_CELL_INERT, FromDIP(180), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    wxGetApp().UpdateDVCDarkUI(m_agenda);
    md3_style_data_view(m_agenda);
    m_agenda->SetMinSize(FromDIP(wxSize(1, 160)));
    m_calendar_columns->Add(m_agenda, 1, wxEXPAND);
    m_spacing.emplace_back(calendar_sizer->Add(m_calendar_columns, 1, wxEXPAND | wxALL, padding), false);
    auto *calendar_actions = new wxWrapSizer(wxHORIZONTAL);
    const auto calendar_button = [this, calendar_page, calendar_actions, &style_action, gap](const wxString &label, void (WorkspacePanel::*action)()) {
        auto *control = new Button(calendar_page, label);
        style_action(control, action == &WorkspacePanel::add_slot ? Button::Variant::Filled : Button::Variant::Outlined);
        m_spacing.emplace_back(calendar_actions->Add(control, 0, wxRIGHT | wxBOTTOM, gap), true);
        control->Bind(wxEVT_BUTTON, [this, action](wxCommandEvent &) { (this->*action)(); });
    };
    calendar_button(_L("Add planned print"), &WorkspacePanel::add_slot);
    calendar_button(_L("Snooze reminder"), &WorkspacePanel::snooze_selected_slot);
    calendar_button(_L("Dismiss reminder"), &WorkspacePanel::dismiss_selected_slot);
    calendar_button(_L("Enable / disable"), &WorkspacePanel::toggle_selected_slot);
    calendar_button(_L("Export ICS"), &WorkspacePanel::export_calendar);
    m_spacing.emplace_back(calendar_sizer->Add(calendar_actions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, padding), false);
    calendar_page->SetSizer(calendar_sizer);
    add_section(calendar_page, _L("Calendar"));

    m_section_tabs->Bind(wxEVT_CHOICE, [this](wxCommandEvent &event) { m_sections->SetSelection(event.GetInt()); });
    m_section_tabs->SetSelection(0);
    m_sections->SetSelection(0);
    m_spacing.emplace_back(root->Add(m_section_tabs, 0, wxEXPAND | wxLEFT | wxRIGHT, padding), false);
    root->Add(m_sections, 1, wxEXPAND);
    SetSizer(root);
    m_ui_ready = true;
    Bind(wxEVT_SIZE, [this](wxSizeEvent &event) { reflow(); event.Skip(); });
    Bind(wxEVT_DPI_CHANGED, [this](wxDPIChangedEvent &event) { refresh_appearance(); event.Skip(); });
    Bind(wxEVT_SYS_COLOUR_CHANGED, [this](wxSysColourChangedEvent &event) { refresh_appearance(); event.Skip(); });
    refresh_appearance();
}

void WorkspacePanel::refresh_appearance()
{
    SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    m_sections->SetBackgroundColour(GetBackgroundColour());
    m_section_tabs->SetBackgroundColour(GetBackgroundColour());
    for (auto *page : m_pages) {
        page->SetBackgroundColour(GetBackgroundColour());
        page->SetScrollRate(0, FromDIP(16));
    }
    for (auto *card : m_cards) {
        card->SetDensity(MD3::Metrics::isCompact() ? StaticBox::Density::Compact : StaticBox::Density::Comfortable);
        card->SyncWindowBackground();
    }
    for (const auto &spacing : m_spacing)
        spacing.first->SetBorder(FromDIP(spacing.second ? MD3::Metrics::active().gap : MD3::Metrics::active().padding));
    const wxFont body = MD3::Metrics::isCompact() ? ::Label::Body_13 : ::Label::Body_14;
    for (auto *title : m_headings) {
        title->SetFont(::Label::Head_16);
        title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
        title->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    }
    m_overview->SetFont(body);
    m_overview->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    m_overview->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    m_notes->SetFont(body);
    m_checklist->SetFont(body);
    for (auto *table : {m_files, m_agenda}) {
        md3_style_data_view(table);
        table->SetFont(body);
        table->SetRowHeight(std::max(FromDIP(MD3::Metrics::active().row_height),
                                    table->GetCharHeight() + FromDIP(MD3::Metrics::active().gap)));
        table->SetMinSize(FromDIP(wxSize(1, 160)));
    }
    m_checklist->SetMinSize(FromDIP(wxSize(1, 160)));
    m_month->SetFont(body);
    m_month->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    m_month->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    m_month->SetHeaderColours(StateColor::semantic(MD3::Role::OnSurfaceVariant), StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    m_month->SetHighlightColours(StateColor::semantic(MD3::Role::OnPrimary), StateColor::semantic(MD3::Role::Primary));
    for (auto *control : m_action_buttons) {
        control->SetButtonSize(MD3::Metrics::isCompact() ? Button::Size::Medium : Button::Size::Large);
        control->Rescale();
    }
    m_section_tabs->Rescale();
    reflow();
    Refresh(false);
}

void WorkspacePanel::reflow()
{
    if (!m_ui_ready || m_reflowing || GetClientSize().x <= 0) return;
    m_reflowing = true;
    const int padding = FromDIP(MD3::Metrics::active().padding);
    const int gap = FromDIP(MD3::Metrics::active().gap);
    Layout();
    const int calendar_width = m_month->GetParent()->GetClientSize().x - 2 * padding;
    const int columns_min = m_month->GetBestSize().x + gap + FromDIP(360);
    const bool stacked = calendar_width < columns_min;
    m_calendar_columns->SetOrientation(stacked ? wxVERTICAL : wxHORIZONTAL);
    m_calendar_month_item->SetFlag(stacked ? wxBOTTOM : wxRIGHT);
    m_calendar_month_item->SetBorder(gap);
    // SetLabel can change wrapped height without a size event. Refresh the
    // Overview card's cached best size before recomputing its page extent.
    auto *overview_card = m_overview->GetParent();
    overview_card->InvalidateBestSize();
    overview_card->Layout();
    for (auto *page : m_pages) {
        page->FitInside();
        page->Layout();
    }
    m_reflowing = false;
}

void WorkspacePanel::refresh_overview()
{
    m_overview->SetLabel(display(m_workspace.title) + "\n" +
        wxString::Format(_L("%zu projects, %zu checklist items, %zu planned prints"),
                         m_workspace.members.size(), m_workspace.checklist.size(), m_workspace.slots.size()));
    reflow();
}

void WorkspacePanel::refresh_files()
{
    m_files->DeleteAllItems();
    for (std::size_t index = 0; index < m_workspace.members.size(); ++index) {
        const auto &member = m_workspace.members[index];
        wxVector<wxVariant> row;
        row.push_back(wxVariant(display(member.name)));
        row.push_back(wxVariant(display(member.project_path.filename().u8string())));
        row.push_back(wxVariant(wxString::Format("%zu", member.editable_sources.size())));
        m_files->AppendItem(row);
    }
}

void WorkspacePanel::refresh_checklist()
{
    m_checklist->Clear();
    for (const auto &item : m_workspace.checklist) {
        wxString label = display(item.text);
        if (!item.due_date.empty()) label += "  [" + HumanDate::format(HumanDate::calendar(display(item.due_date))) + "]";
        if (!item.due_date.empty()) {
            const auto resolved = Workspace::resolve_local_time(m_workspace.time_zone, item.due_date + " " +
                (m_workspace.deadline_reminder_hour < 10 ? "0" : "") +
                std::to_string(m_workspace.deadline_reminder_hour) + ":00");
            if (resolved.error.empty() && resolved.candidates.size() == 1 &&
                resolved.candidates.front().offset_minutes != item.due_utc_offset_minutes)
                label += _L(" [saved offset differs]");
            else if (!resolved.error.empty() || resolved.candidates.size() != 1)
                label += _L(" [time zone unavailable]");
        }
        m_checklist->Append(label);
        m_checklist->Check(m_checklist->GetCount() - 1, item.completed);
    }
}

void WorkspacePanel::refresh_calendar()
{
    m_agenda->DeleteAllItems();
    int date_width = std::max(FromDIP(160), m_agenda->GetTextExtent(_L("Start (local time)")).x + FromDIP(24));
    const wxDateTime selected = m_month->GetDate();
    const auto slots = Workspace::month_slots(m_workspace, selected.GetYear(), static_cast<int>(selected.GetMonth()) + 1, 0);
    for (std::size_t index = 0; index < slots.size(); ++index) {
        const auto &slot = slots[index];
        wxString state = !slot.enabled ? _L("Disabled") : slot.completed ? _L("Completed") : _L("Planned");
        int actual_offset = 0;
        if (!Workspace::zone_offset_at_utc(slot.time_zone, slot.start_utc, actual_offset))
            state += _L("; time zone unavailable");
        else if (actual_offset != slot.utc_offset_minutes)
            state += _L("; saved offset differs");
        wxVector<wxVariant> row;
        row.push_back(wxVariant(display(slot.title)));
        row.push_back(wxVariant(display(slot.printer_id)));
        const wxString start_label = HumanDate::date_time(wxDateTime(static_cast<time_t>(slot.start_utc)));
        date_width = std::max(date_width, m_agenda->GetTextExtent(start_label).x + FromDIP(24));
        row.push_back(wxVariant(start_label));
        row.push_back(wxVariant(state));
        // The row keeps the slot's index in m_workspace.slots for the actions below.
        const auto position = std::find_if(m_workspace.slots.begin(), m_workspace.slots.end(),
            [&](const auto &candidate) { return candidate.id == slot.id; }) - m_workspace.slots.begin();
        m_agenda->AppendItem(row, static_cast<wxUIntPtr>(position));
    }
}

void WorkspacePanel::refresh_all()
{
    refresh_overview(); refresh_files(); refresh_checklist();
    m_notes->ChangeValue(display(m_workspace.notes));
    refresh_calendar();
}

void WorkspacePanel::create_new()
{
    if (m_dirty && md3_message_box(_L("Discard unsaved workspace changes?"), _L("New workspace"),
                                wxYES_NO | wxICON_QUESTION, this) != wxYES) return;
    m_workspace = {};
    m_workspace.id = Workspace::new_id();
    m_workspace.title = "New workspace";
    m_bundle_path.clear();
    if (!m_loaded_staging.empty()) { std::error_code ignored; workspace_fs::remove_all(m_loaded_staging, ignored); }
    m_loaded_staging.clear();
    for (const auto &file : m_owned_member_files) { std::error_code ignored; workspace_fs::remove(file, ignored); }
    m_owned_member_files.clear();
    m_dirty = false;
    m_last_reminder_check_utc = 0;
    m_timezone_warning_shown = false;
    refresh_all();
}

void WorkspacePanel::edit_preferences()
{
    wxString zone = display(m_workspace.time_zone);
    wxString hour = wxString::Format("%d", m_workspace.deadline_reminder_hour);
    wxString enabled = m_workspace.reminders_enabled ? "yes" : "no";
    if (!ask_text(this, _L("Workspace time zone"), _L("IANA time zone name"), zone)) return;
    if (!ask_text(this, _L("Deadline reminder"), _L("Local hour, 0 to 23"), hour)) return;
    if (!ask_text(this, _L("Reminders"), _L("Enable reminders? yes or no"), enabled)) return;
    long parsed = 0;
    if (!hour.ToLong(&parsed) || parsed < 0 || parsed > 23 ||
        (enabled.Lower() != "yes" && enabled.Lower() != "no")) return;
    const auto probe = Workspace::resolve_local_time(utf8(zone), "2026-01-15 12:00");
    if (!probe.error.empty() || probe.candidates.size() != 1) {
        md3_message_box(local_time_error(probe.error),
                     _L("Workspace time zone"), wxOK | wxICON_WARNING, this);
        return;
    }
    for (auto &item : m_workspace.checklist) {
        if (item.due_date.empty()) continue;
        const auto resolved = Workspace::resolve_local_time(utf8(zone), item.due_date + " " +
            (parsed < 10 ? "0" : "") + std::to_string(parsed) + ":00");
        if (!resolved.error.empty() || resolved.candidates.size() != 1) {
            md3_message_box(_L("A checklist deadline cannot be resolved in this time zone."),
                         _L("Workspace time zone"), wxOK | wxICON_WARNING, this);
            return;
        }
    }
    m_workspace.time_zone = utf8(zone);
    m_workspace.deadline_reminder_hour = static_cast<int>(parsed);
    for (auto &item : m_workspace.checklist) {
        if (item.due_date.empty()) continue;
        const auto resolved = Workspace::resolve_local_time(m_workspace.time_zone, item.due_date + " " +
            (parsed < 10 ? "0" : "") + std::to_string(parsed) + ":00");
        item.due_utc_offset_minutes = resolved.candidates.front().offset_minutes;
    }
    m_workspace.reminders_enabled = enabled.Lower() == "yes";
    m_timezone_warning_shown = false;
    m_dirty = true;
    refresh_overview();
    if (!m_bundle_path.empty()) save_bundle();
}

void WorkspacePanel::choose_open()
{
    wxFileDialog dialog(this, _L("Open workspace"), wxEmptyString, wxEmptyString,
                        _L("Bambu workspace (*.bambu-workspace)|*.bambu-workspace"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() == wxID_OK) open_bundle(workspace_fs::u8path(utf8(dialog.GetPath())));
}

bool WorkspacePanel::open_bundle(const workspace_fs::path &path)
{
    if (m_dirty && md3_message_box(_L("Discard unsaved workspace changes?"), _L("Open workspace"),
                                wxYES_NO | wxICON_QUESTION, this) != wxYES) return false;
    const auto loaded = Workspace::load_bundle(path, m_staging_root);
    if (!loaded.ok()) {
        md3_message_box(display(loaded.error), _L("Could not open workspace"), wxOK | wxICON_WARNING, this);
        return false;
    }
    if (!m_loaded_staging.empty()) { std::error_code ignored; workspace_fs::remove_all(m_loaded_staging, ignored); }
    for (const auto &file : m_owned_member_files) { std::error_code ignored; workspace_fs::remove(file, ignored); }
    m_owned_member_files.clear();
    m_workspace = loaded.workspace;
    m_bundle_path = path;
    m_loaded_staging = loaded.staging_directory;
    m_dirty = false;
    m_timezone_warning_shown = false;
    refresh_all();
    if (!loaded.warnings.empty()) {
        wxString warning = display(loaded.warnings.front());
        if (loaded.warnings.size() > 1)
            warning += wxString::Format(_L(" (%zu additional affected members)"), loaded.warnings.size() - 1);
        if (auto *notifications = wxGetApp().notification_manager())
            notifications->push_notification(NotificationType::CustomNotification,
                NotificationManager::NotificationLevel::WarningNotificationLevel, utf8(warning));
        else
            md3_message_box(warning, _L("Workspace member warning"), wxOK | wxICON_WARNING, this);
    }
    reset_reminder_cursor();
    check_reminders();
    return true;
}

void WorkspacePanel::reset_reminder_cursor()
{
    const auto now = static_cast<std::int64_t>(std::time(nullptr));
    m_last_reminder_check_utc = now - 86400;
    if (m_bundle_path.empty() || !wxGetApp().app_config) return;
    const std::string saved = wxGetApp().app_config->get("workspace_reminders", m_workspace.id);
    try {
        if (!saved.empty()) {
            const auto parsed = std::stoll(saved);
            if (parsed > 0 && parsed <= now) m_last_reminder_check_utc = parsed;
        }
    } catch (const std::exception &) {
        // An invalid local cursor falls back to the bounded first-open window.
    }
}

void WorkspacePanel::check_reminders()
{
    if (m_bundle_path.empty()) return;
    const auto now = static_cast<std::int64_t>(std::time(nullptr));
    if (m_last_reminder_check_utc <= 0 || m_last_reminder_check_utc > now) reset_reminder_cursor();
    auto *notifications = wxGetApp().notification_manager();
    if (!m_timezone_warning_shown && notifications && m_workspace.reminders_enabled) {
        const auto warnings = Workspace::validate_plan(m_workspace, {});
        const auto timezone_warning = std::find_if(warnings.begin(), warnings.end(), [](const auto &warning) {
            return warning.code == "unverified_deadline_timezone" || warning.code == "deadline_offset_mismatch" ||
                   warning.code == "unverified_slot_timezone" || warning.code == "slot_offset_mismatch";
        });
        if (timezone_warning != warnings.end()) {
            notifications->push_notification(NotificationType::CustomNotification,
                NotificationManager::NotificationLevel::WarningNotificationLevel,
                utf8(timezone_warning->code == "deadline_offset_mismatch"
                    ? _L("A saved deadline offset differs from its named time zone. The reminder uses the resolved due-date offset; review this item.")
                    : timezone_warning->code == "slot_offset_mismatch"
                    ? _L("A saved planned-print offset differs from its named time zone. The saved UTC time remains in use; review this slot.")
                    : _L("A calendar time zone could not be resolved. Affected deadline reminders are paused; saved planned-print UTC times remain in use.")));
            m_timezone_warning_shown = true;
        }
    }
    const auto due = Workspace::due_reminders(m_workspace, m_last_reminder_check_utc, now);
    if (due.empty()) { m_last_reminder_check_utc = now; return; }
    if (!notifications) return;
    const bool catch_up = now - m_last_reminder_check_utc > 120;
    wxString text;
    if (catch_up || due.size() > 3) {
        text = wxString::Format(_L("%zu workspace reminders became due while you were away."), due.size());
    } else {
        const auto &reminder = due.front();
        const auto slot = std::find_if(m_workspace.slots.begin(), m_workspace.slots.end(),
            [&](const Workspace::CalendarSlot &item) { return item.id == reminder.item_id; });
        const auto item = std::find_if(m_workspace.checklist.begin(), m_workspace.checklist.end(),
            [&](const Workspace::ChecklistItem &entry) { return entry.id == reminder.item_id; });
        text = reminder.kind == "planned_print" && slot != m_workspace.slots.end()
            ? _L("Planned print reminder: ") + display(slot->title)
            : _L("Checklist deadline reminder: ") +
                (item == m_workspace.checklist.end() ? _L("Workspace item") : display(item->text));
        if (due.size() > 1)
            text += wxString::Format(_L(" (%zu more due)"), due.size() - 1);
    }
    notifications->push_notification(NotificationType::CustomNotification,
        NotificationManager::NotificationLevel::RegularNotificationLevel, utf8(text));
    m_last_reminder_check_utc = now;
    if (wxGetApp().app_config) {
        wxGetApp().app_config->set("workspace_reminders", m_workspace.id, std::to_string(now));
        wxGetApp().app_config->save();
    }
}

void WorkspacePanel::choose_save()
{
    if (m_bundle_path.empty()) {
        wxFileDialog dialog(this, _L("Save workspace"), wxEmptyString, wxEmptyString,
                            _L("Bambu workspace (*.bambu-workspace)|*.bambu-workspace"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dialog.ShowModal() != wxID_OK) return;
        m_bundle_path = workspace_fs::u8path(utf8(dialog.GetPath()));
    }
    save_bundle();
}

bool WorkspacePanel::save_bundle()
{
    if (m_bundle_path.empty()) return false;
    const auto saved = Workspace::save_bundle(m_workspace, m_bundle_path);
    if (!saved.ok()) {
        md3_message_box(display(saved.error), _L("Could not save workspace"), wxOK | wxICON_WARNING, this);
        return false;
    }
    m_dirty = false;
    return true;
}

std::optional<workspace_fs::path> WorkspacePanel::stage_member_file(const workspace_fs::path &source)
{
    std::string validation_error;
    if (!Workspace::validate_member_3mf(source, &validation_error)) {
        md3_message_box(display(validation_error), _L("Invalid project 3MF"), wxOK | wxICON_WARNING, this);
        return std::nullopt;
    }
    const workspace_fs::path staged = m_staging_root / ("member-" + Workspace::new_id() + ".3mf");
    std::error_code error;
    if (!workspace_fs::copy_file(source, staged, workspace_fs::copy_options::none, error) || error) {
        workspace_fs::remove(staged, error);
        return std::nullopt;
    }
    if (!Workspace::validate_member_3mf(staged, &validation_error)) {
        workspace_fs::remove(staged, error);
        md3_message_box(display(validation_error), _L("Invalid staged project 3MF"), wxOK | wxICON_WARNING, this);
        return std::nullopt;
    }
    return staged;
}

std::optional<WorkspaceMemberSelection> WorkspacePanel::selected_member() const
{
    const long index = m_files->GetSelectedRow();
    if (index < 0 || static_cast<std::size_t>(index) >= m_workspace.members.size()) return std::nullopt;
    const auto &member = m_workspace.members[static_cast<std::size_t>(index)];
    if (!workspace_fs::is_regular_file(member.project_path)) return std::nullopt;
    return WorkspaceMemberSelection{m_workspace.id, member.id, m_bundle_path, member.project_path};
}

void WorkspacePanel::open_selected_member()
{
    const auto selected = selected_member();
    if (!selected) return;
    if (m_member_open_handler) m_member_open_handler(*selected);
    else md3_message_box(_L("Opening this member in the print canvas is not connected yet."),
                      _L("Open selected project"), wxOK | wxICON_INFORMATION, this);
}

bool WorkspacePanel::save_member(const std::string &bundle_id, const std::string &member_id,
                                 const workspace_fs::path &history_bearing_3mf)
{
    if (m_bundle_path.empty() || bundle_id != m_workspace.id || member_id.empty()) return false;
    const auto member = std::find_if(m_workspace.members.begin(), m_workspace.members.end(),
        [&](const Workspace::Member &item) { return item.id == member_id; });
    if (member == m_workspace.members.end()) return false;
    const auto staged = stage_member_file(history_bearing_3mf);
    if (!staged) return false;
    const workspace_fs::path previous = member->project_path;
    member->project_path = *staged;
    if (!save_bundle()) {
        member->project_path = previous;
        m_pending_member_recovery = *staged;
        return false;
    }
    m_owned_member_files.push_back(*staged);
    refresh_files();
    return true;
}

void WorkspacePanel::add_member()
{
    wxFileDialog dialog(this, _L("Add project 3MF"), wxEmptyString, wxEmptyString,
                        _L("Project 3MF (*.3mf)|*.3mf"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) return;
    Workspace::Member member;
    member.id = Workspace::new_id();
    const workspace_fs::path chosen = workspace_fs::u8path(utf8(dialog.GetPath()));
    const auto staged = stage_member_file(chosen);
    if (!staged) {
        return;
    }
    member.project_path = *staged;
    member.name = chosen.stem().u8string();
    m_owned_member_files.push_back(*staged);
    m_workspace.members.push_back(std::move(member));
    m_dirty = true;
    refresh_overview(); refresh_files();
}

void WorkspacePanel::add_source()
{
    const long index = m_files->GetSelectedRow();
    if (index < 0 || static_cast<std::size_t>(index) >= m_workspace.members.size()) return;
    wxFileDialog dialog(this, _L("Add editable source"), wxEmptyString, wxEmptyString,
                        _L("All files (*.*)|*.*"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) return;
    Workspace::SourceFile source;
    source.local_path = workspace_fs::u8path(utf8(dialog.GetPath()));
    source.relative_path = source.local_path.filename().u8string();
    m_workspace.members[static_cast<std::size_t>(index)].editable_sources.push_back(std::move(source));
    m_dirty = true;
    refresh_files();
}

void WorkspacePanel::add_checklist()
{
    wxString text;
    if (!ask_text(this, _L("Add checklist item"), _L("Task"), text) || text.IsEmpty()) return;
    Workspace::ChecklistItem item;
    item.id = Workspace::new_id();
    item.text = utf8(text);
    if (Workspace::add_checklist_item(m_workspace, std::move(item))) {
        m_dirty = true; refresh_overview(); refresh_checklist();
    }
}

void WorkspacePanel::edit_checklist()
{
    const int index = m_checklist->GetSelection();
    if (index < 0 || static_cast<std::size_t>(index) >= m_workspace.checklist.size()) return;
    auto item = m_workspace.checklist[static_cast<std::size_t>(index)];
    wxString text = display(item.text), due = display(item.due_date), member = display(item.linked_member_id);
    wxString slot = display(item.linked_slot_id);
    if (!ask_text(this, _L("Edit checklist item"), _L("Task"), text)) return;
    if (!ask_text(this, _L("Due date"), _L("Date only, YYYY-MM-DD (optional)"), due)) return;
    if (!ask_text(this, _L("Linked project"), _L("Member ID (optional)"), member)) return;
    if (!ask_text(this, _L("Linked planned print"), _L("Planned-print ID (optional)"), slot)) return;
    int resolved_offset = 0;
    if (!due.IsEmpty()) {
        const auto resolved = Workspace::resolve_local_time(m_workspace.time_zone,
            utf8(due) + " " + (m_workspace.deadline_reminder_hour < 10 ? "0" : "") +
            std::to_string(m_workspace.deadline_reminder_hour) + ":00");
        if (!resolved.error.empty() || resolved.candidates.size() != 1) {
            md3_message_box(resolved.error.empty() ? _L("The deadline reminder time is ambiguous or nonexistent.") :
                         local_time_error(resolved.error), _L("Due date"), wxOK | wxICON_WARNING, this);
            return;
        }
        resolved_offset = resolved.candidates.front().offset_minutes;
    }
    item.text = utf8(text); item.due_date = utf8(due); item.linked_member_id = utf8(member);
    item.linked_slot_id = utf8(slot); item.due_utc_offset_minutes = resolved_offset;
    if (!Workspace::edit_checklist_item(m_workspace, item)) return;
    m_timezone_warning_shown = false;
    m_dirty = true; refresh_checklist(); m_checklist->SetSelection(index);
    check_reminders();
}

void WorkspacePanel::move_checklist(int direction)
{
    const int index = m_checklist->GetSelection();
    const int target = index + direction;
    if (index < 0 || target < 0 || static_cast<std::size_t>(target) >= m_workspace.checklist.size()) return;
    if (Workspace::reorder_checklist_item(m_workspace, m_workspace.checklist[index].id, target)) {
        m_dirty = true; refresh_checklist(); m_checklist->SetSelection(target);
    }
}

void WorkspacePanel::add_slot()
{
    wxString title, printer, member, start, end;
    if (!ask_text(this, _L("Planned print"), _L("Title"), title) || title.IsEmpty()) return;
    if (!ask_text(this, _L("Printer"), _L("Printer ID"), printer)) return;
    if (!ask_text(this, _L("Linked project"), _L("Member ID (optional)"), member)) return;
    if (!ask_text(this, _L("Start time"), _L("Local time, YYYY-MM-DD HH:MM"), start)) return;
    if (!ask_text(this, _L("End time"), _L("Local time, YYYY-MM-DD HH:MM"), end)) return;
    const auto begin = choose_local_time(this, m_workspace.time_zone, start);
    if (!begin) return;
    const auto finish = choose_local_time(this, m_workspace.time_zone, end);
    if (!finish) return;
    if (finish->utc <= begin->utc) {
        md3_message_box(_L("The end time must be after the start time."),
                     _L("Planned print"), wxOK | wxICON_WARNING, this);
        return;
    }
    Workspace::CalendarSlot slot;
    slot.id = Workspace::new_id(); slot.title = utf8(title); slot.printer_id = utf8(printer);
    slot.member_id = utf8(member);
    slot.start_utc = begin->utc; slot.end_utc = finish->utc; slot.utc_offset_minutes = begin->offset_minutes;
    slot.time_zone = m_workspace.time_zone;
    m_workspace.slots.push_back(std::move(slot));
    m_dirty = true; refresh_overview(); refresh_calendar();
}

void WorkspacePanel::snooze_selected_slot()
{
    const int row = m_agenda->GetSelectedRow();
    if (row == wxNOT_FOUND) return;
    const long index = static_cast<long>(m_agenda->GetItemData(m_agenda->RowToItem(row)));
    if (index < 0 || static_cast<std::size_t>(index) >= m_workspace.slots.size()) return;
    wxString when;
    if (!ask_text(this, _L("Snooze reminder"), _L("New reminder time, UTC YYYY-MM-DD HH:MM"), when)) return;
    const auto instant = parse_wall_utc(utf8(when), 0);
    if (instant > 0 && Workspace::snooze_slot(m_workspace, m_workspace.slots[index].id, instant)) {
        m_dirty = true;
        if (!m_bundle_path.empty()) save_bundle();
    }
}

void WorkspacePanel::dismiss_selected_slot()
{
    const int row = m_agenda->GetSelectedRow();
    if (row == wxNOT_FOUND) return;
    const long index = static_cast<long>(m_agenda->GetItemData(m_agenda->RowToItem(row)));
    if (index >= 0 && static_cast<std::size_t>(index) < m_workspace.slots.size() &&
        Workspace::dismiss_slot_reminder(m_workspace, m_workspace.slots[index].id)) {
        m_dirty = true;
        if (!m_bundle_path.empty()) save_bundle();
    }
}

void WorkspacePanel::toggle_selected_slot()
{
    const int row = m_agenda->GetSelectedRow();
    if (row == wxNOT_FOUND) return;
    const long index = static_cast<long>(m_agenda->GetItemData(m_agenda->RowToItem(row)));
    if (index < 0 || static_cast<std::size_t>(index) >= m_workspace.slots.size()) return;
    auto &slot = m_workspace.slots[static_cast<std::size_t>(index)];
    slot.enabled = !slot.enabled;
    m_dirty = true;
    refresh_calendar();
    if (!m_bundle_path.empty()) save_bundle();
}

void WorkspacePanel::export_checklist(bool csv)
{
    write_export(this, _L("Export checklist"), csv ? _L("CSV (*.csv)|*.csv") : _L("JSON (*.json)|*.json"),
                 csv ? Workspace::checklist_csv(m_workspace) : Workspace::checklist_json(m_workspace));
}

void WorkspacePanel::export_calendar()
{
    write_export(this, _L("Export calendar"), _L("iCalendar (*.ics)|*.ics"), Workspace::calendar_ics(m_workspace));
}

} // namespace Slic3r::GUI
