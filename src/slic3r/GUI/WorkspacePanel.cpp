#include "WorkspacePanel.hpp"

#include "I18N.hpp"
#include "libslic3r/WorkspacePlanner.hpp"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <wx/button.h>
#include <wx/calctrl.h>
#include <wx/checklst.h>
#include <wx/filedlg.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/stdpaths.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>

namespace fs = std::filesystem;

namespace Slic3r::GUI {
namespace {

wxString display(const std::string &text) { return wxString::FromUTF8(text); }
std::string utf8(const wxString &text) { return std::string(text.ToUTF8()); }

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

bool ask_text(wxWindow *parent, const wxString &caption, const wxString &prompt, wxString &value)
{
    wxTextEntryDialog dialog(parent, prompt, caption, value);
    if (dialog.ShowModal() != wxID_OK) return false;
    value = dialog.GetValue();
    return true;
}

bool write_export(wxWindow *parent, const wxString &name, const wxString &filter, const std::string &content)
{
    wxFileDialog dialog(parent, name, wxEmptyString, wxEmptyString, filter, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return false;
    std::ofstream output(fs::u8path(utf8(dialog.GetPath())), std::ios::binary | std::ios::trunc);
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (output.good()) return true;
    wxMessageBox(_L("Could not write the selected export file."), name, wxOK | wxICON_WARNING, parent);
    return false;
}

} // namespace

WorkspacePanel::WorkspacePanel(wxWindow *parent) : wxPanel(parent)
{
    m_staging_root = fs::u8path(utf8(wxStandardPaths::Get().GetUserLocalDataDir())) / "workspace-staging";
    std::error_code ignored;
    fs::create_directories(m_staging_root, ignored);
    m_workspace.id = Workspace::new_id();
    m_workspace.title = "New workspace";
    create_ui();
    refresh_all();
}

WorkspacePanel::~WorkspacePanel()
{
    if (!m_loaded_staging.empty()) {
        std::error_code ignored;
        fs::remove_all(m_loaded_staging, ignored);
    }
}

void WorkspacePanel::create_ui()
{
    auto *root = new wxBoxSizer(wxVERTICAL);
    auto *actions = new wxBoxSizer(wxHORIZONTAL);
    const auto button = [this, actions](const wxString &label, void (WorkspacePanel::*action)()) {
        auto *control = new wxButton(this, wxID_ANY, label);
        actions->Add(control, 0, wxALL, FromDIP(4));
        control->Bind(wxEVT_BUTTON, [this, action](wxCommandEvent &) { (this->*action)(); });
    };
    button(_L("New workspace"), &WorkspacePanel::create_new);
    button(_L("Open workspace"), &WorkspacePanel::choose_open);
    button(_L("Save workspace"), &WorkspacePanel::choose_save);
    root->Add(actions, 0, wxEXPAND | wxALL, FromDIP(4));

    m_sections = new wxNotebook(this, wxID_ANY);
    auto *overview_page = new wxPanel(m_sections);
    auto *overview_sizer = new wxBoxSizer(wxVERTICAL);
    m_overview = new wxStaticText(overview_page, wxID_ANY, wxEmptyString);
    overview_sizer->Add(m_overview, 0, wxALL, FromDIP(12));
    auto *rename = new wxButton(overview_page, wxID_ANY, _L("Rename workspace"));
    overview_sizer->Add(rename, 0, wxALL, FromDIP(8));
    rename->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        wxString title = display(m_workspace.title);
        if (ask_text(this, _L("Workspace title"), _L("Name this workspace"), title)) {
            m_workspace.title = utf8(title); m_dirty = true; refresh_overview();
        }
    });
    auto *preferences = new wxButton(overview_page, wxID_ANY, _L("Time zone and reminders"));
    overview_sizer->Add(preferences, 0, wxALL, FromDIP(8));
    preferences->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { edit_preferences(); });
    overview_page->SetSizer(overview_sizer);
    m_sections->AddPage(overview_page, _L("Overview"));

    auto *files_page = new wxPanel(m_sections);
    auto *files_sizer = new wxBoxSizer(wxVERTICAL);
    m_files = new wxListCtrl(files_page, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    m_files->InsertColumn(0, _L("Member"));
    m_files->InsertColumn(1, _L("Project 3MF"));
    m_files->InsertColumn(2, _L("Editable sources"));
    files_sizer->Add(m_files, 1, wxEXPAND | wxALL, FromDIP(8));
    auto *file_actions = new wxBoxSizer(wxHORIZONTAL);
    auto *add_project = new wxButton(files_page, wxID_ANY, _L("Add project 3MF"));
    auto *add_editable = new wxButton(files_page, wxID_ANY, _L("Add editable source"));
    file_actions->Add(add_project, 0, wxALL, FromDIP(4));
    file_actions->Add(add_editable, 0, wxALL, FromDIP(4));
    add_project->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { add_member(); });
    add_editable->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { add_source(); });
    files_sizer->Add(file_actions, 0, wxALL, FromDIP(4));
    files_page->SetSizer(files_sizer);
    m_sections->AddPage(files_page, _L("Files"));

    auto *list_page = new wxPanel(m_sections);
    auto *list_sizer = new wxBoxSizer(wxVERTICAL);
    m_checklist = new wxCheckListBox(list_page, wxID_ANY);
    list_sizer->Add(m_checklist, 1, wxEXPAND | wxALL, FromDIP(8));
    m_checklist->Bind(wxEVT_CHECKLISTBOX, [this](wxCommandEvent &event) {
        const int index = event.GetInt();
        if (index >= 0 && static_cast<std::size_t>(index) < m_workspace.checklist.size()) {
            m_workspace.checklist[index].completed = m_checklist->IsChecked(index);
            m_dirty = true;
        }
    });
    auto *list_actions = new wxBoxSizer(wxHORIZONTAL);
    const auto list_button = [this, list_page, list_actions](const wxString &label, void (WorkspacePanel::*action)()) {
        auto *control = new wxButton(list_page, wxID_ANY, label);
        list_actions->Add(control, 0, wxALL, FromDIP(4));
        control->Bind(wxEVT_BUTTON, [this, action](wxCommandEvent &) { (this->*action)(); });
    };
    list_button(_L("Add"), &WorkspacePanel::add_checklist);
    list_button(_L("Edit / due date / link"), &WorkspacePanel::edit_checklist);
    auto *up = new wxButton(list_page, wxID_ANY, _L("Move up"));
    auto *down = new wxButton(list_page, wxID_ANY, _L("Move down"));
    list_actions->Add(up, 0, wxALL, FromDIP(4));
    list_actions->Add(down, 0, wxALL, FromDIP(4));
    up->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { move_checklist(-1); });
    down->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { move_checklist(1); });
    auto *json_button = new wxButton(list_page, wxID_ANY, _L("Export JSON"));
    auto *csv_button = new wxButton(list_page, wxID_ANY, _L("Export CSV"));
    list_actions->Add(json_button, 0, wxALL, FromDIP(4));
    list_actions->Add(csv_button, 0, wxALL, FromDIP(4));
    json_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { export_checklist(false); });
    csv_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { export_checklist(true); });
    list_sizer->Add(list_actions, 0, wxALL, FromDIP(4));
    list_page->SetSizer(list_sizer);
    m_sections->AddPage(list_page, _L("Checklist"));

    auto *notes_page = new wxPanel(m_sections);
    auto *notes_sizer = new wxBoxSizer(wxVERTICAL);
    m_notes = new wxTextCtrl(notes_page, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
    notes_sizer->Add(m_notes, 1, wxEXPAND | wxALL, FromDIP(8));
    m_notes->Bind(wxEVT_TEXT, [this](wxCommandEvent &) {
        m_workspace.notes = utf8(m_notes->GetValue()); m_dirty = true;
    });
    notes_page->SetSizer(notes_sizer);
    m_sections->AddPage(notes_page, _L("Notes"));

    auto *calendar_page = new wxPanel(m_sections);
    auto *calendar_sizer = new wxBoxSizer(wxVERTICAL);
    m_month = new wxCalendarCtrl(calendar_page, wxID_ANY);
    calendar_sizer->Add(m_month, 0, wxALL, FromDIP(8));
    m_month->Bind(wxEVT_CALENDAR_SEL_CHANGED, [this](wxCalendarEvent &) { refresh_calendar(); });
    m_agenda = new wxListCtrl(calendar_page, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    m_agenda->InsertColumn(0, _L("Planned print"));
    m_agenda->InsertColumn(1, _L("Printer"));
    m_agenda->InsertColumn(2, _L("Start (UTC)"));
    m_agenda->InsertColumn(3, _L("Status"));
    calendar_sizer->Add(m_agenda, 1, wxEXPAND | wxALL, FromDIP(8));
    auto *calendar_actions = new wxBoxSizer(wxHORIZONTAL);
    const auto calendar_button = [this, calendar_page, calendar_actions](const wxString &label, void (WorkspacePanel::*action)()) {
        auto *control = new wxButton(calendar_page, wxID_ANY, label);
        calendar_actions->Add(control, 0, wxALL, FromDIP(4));
        control->Bind(wxEVT_BUTTON, [this, action](wxCommandEvent &) { (this->*action)(); });
    };
    calendar_button(_L("Add planned print"), &WorkspacePanel::add_slot);
    calendar_button(_L("Snooze reminder"), &WorkspacePanel::snooze_selected_slot);
    calendar_button(_L("Dismiss reminder"), &WorkspacePanel::dismiss_selected_slot);
    calendar_button(_L("Enable / disable"), &WorkspacePanel::toggle_selected_slot);
    calendar_button(_L("Export ICS"), &WorkspacePanel::export_calendar);
    calendar_sizer->Add(calendar_actions, 0, wxALL, FromDIP(4));
    calendar_page->SetSizer(calendar_sizer);
    m_sections->AddPage(calendar_page, _L("Calendar"));

    root->Add(m_sections, 1, wxEXPAND);
    SetSizer(root);
}

void WorkspacePanel::refresh_overview()
{
    m_overview->SetLabel(display(m_workspace.title) + "\n" +
        wxString::Format(_L("%zu projects, %zu checklist items, %zu planned prints"),
                         m_workspace.members.size(), m_workspace.checklist.size(), m_workspace.slots.size()));
}

void WorkspacePanel::refresh_files()
{
    m_files->DeleteAllItems();
    for (std::size_t index = 0; index < m_workspace.members.size(); ++index) {
        const auto &member = m_workspace.members[index];
        const long row = m_files->InsertItem(static_cast<long>(index), display(member.name));
        m_files->SetItem(row, 1, display(member.project_path.filename().u8string()));
        m_files->SetItem(row, 2, wxString::Format("%zu", member.editable_sources.size()));
    }
}

void WorkspacePanel::refresh_checklist()
{
    m_checklist->Clear();
    for (const auto &item : m_workspace.checklist) {
        wxString label = display(item.text);
        if (!item.due_date.empty()) label += "  [" + display(item.due_date) + "]";
        m_checklist->Append(label);
        m_checklist->Check(m_checklist->GetCount() - 1, item.completed);
    }
}

void WorkspacePanel::refresh_calendar()
{
    m_agenda->DeleteAllItems();
    const wxDateTime selected = m_month->GetDate();
    const auto slots = Workspace::month_slots(m_workspace, selected.GetYear(), static_cast<int>(selected.GetMonth()) + 1, 0);
    for (std::size_t index = 0; index < slots.size(); ++index) {
        const auto &slot = slots[index];
        const long row = m_agenda->InsertItem(static_cast<long>(index), display(slot.title));
        m_agenda->SetItem(row, 1, display(slot.printer_id));
        m_agenda->SetItem(row, 2, wxDateTime(static_cast<time_t>(slot.start_utc)).ToUTC().FormatISOCombined(' '));
        m_agenda->SetItem(row, 3, !slot.enabled ? _L("Disabled") :
                          slot.completed ? _L("Completed") : _L("Planned"));
        m_agenda->SetItemData(row, static_cast<long>(std::find_if(m_workspace.slots.begin(), m_workspace.slots.end(),
            [&](const auto &candidate) { return candidate.id == slot.id; }) - m_workspace.slots.begin()));
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
    if (m_dirty && wxMessageBox(_L("Discard unsaved workspace changes?"), _L("New workspace"),
                                wxYES_NO | wxICON_QUESTION, this) != wxYES) return;
    m_workspace = {};
    m_workspace.id = Workspace::new_id();
    m_workspace.title = "New workspace";
    m_bundle_path.clear();
    m_dirty = false;
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
    m_workspace.time_zone = utf8(zone);
    m_workspace.deadline_reminder_hour = static_cast<int>(parsed);
    m_workspace.reminders_enabled = enabled.Lower() == "yes";
    m_dirty = true;
    refresh_overview();
}

void WorkspacePanel::choose_open()
{
    wxFileDialog dialog(this, _L("Open workspace"), wxEmptyString, wxEmptyString,
                        _L("Bambu workspace (*.bambu-workspace)|*.bambu-workspace"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() == wxID_OK) open_bundle(fs::u8path(utf8(dialog.GetPath())));
}

bool WorkspacePanel::open_bundle(const fs::path &path)
{
    if (m_dirty && wxMessageBox(_L("Discard unsaved workspace changes?"), _L("Open workspace"),
                                wxYES_NO | wxICON_QUESTION, this) != wxYES) return false;
    const auto loaded = Workspace::load_bundle(path, m_staging_root);
    if (!loaded.ok()) {
        wxMessageBox(display(loaded.error), _L("Could not open workspace"), wxOK | wxICON_WARNING, this);
        return false;
    }
    if (!m_loaded_staging.empty()) { std::error_code ignored; fs::remove_all(m_loaded_staging, ignored); }
    m_workspace = loaded.workspace;
    m_bundle_path = path;
    m_loaded_staging = loaded.staging_directory;
    m_dirty = false;
    refresh_all();
    return true;
}

void WorkspacePanel::choose_save()
{
    if (m_bundle_path.empty()) {
        wxFileDialog dialog(this, _L("Save workspace"), wxEmptyString, wxEmptyString,
                            _L("Bambu workspace (*.bambu-workspace)|*.bambu-workspace"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dialog.ShowModal() != wxID_OK) return;
        m_bundle_path = fs::u8path(utf8(dialog.GetPath()));
    }
    save_bundle();
}

bool WorkspacePanel::save_bundle()
{
    if (m_bundle_path.empty()) return false;
    const auto saved = Workspace::save_bundle(m_workspace, m_bundle_path);
    if (!saved.ok()) {
        wxMessageBox(display(saved.error), _L("Could not save workspace"), wxOK | wxICON_WARNING, this);
        return false;
    }
    m_dirty = false;
    return true;
}

void WorkspacePanel::add_member()
{
    wxFileDialog dialog(this, _L("Add project 3MF"), wxEmptyString, wxEmptyString,
                        _L("Project 3MF (*.3mf)|*.3mf"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) return;
    Workspace::Member member;
    member.id = Workspace::new_id();
    member.project_path = fs::u8path(utf8(dialog.GetPath()));
    member.name = member.project_path.stem().u8string();
    m_workspace.members.push_back(std::move(member));
    m_dirty = true;
    refresh_overview(); refresh_files();
}

void WorkspacePanel::add_source()
{
    const long index = m_files->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (index < 0 || static_cast<std::size_t>(index) >= m_workspace.members.size()) return;
    wxFileDialog dialog(this, _L("Add editable source"), wxEmptyString, wxEmptyString,
                        _L("All files (*.*)|*.*"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) return;
    Workspace::SourceFile source;
    source.local_path = fs::u8path(utf8(dialog.GetPath()));
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
    wxString offset = wxString::Format("%d", item.due_utc_offset_minutes);
    if (!ask_text(this, _L("Edit checklist item"), _L("Task"), text)) return;
    if (!ask_text(this, _L("Due date"), _L("Date only, YYYY-MM-DD (optional)"), due)) return;
    if (!ask_text(this, _L("Linked project"), _L("Member ID (optional)"), member)) return;
    if (!ask_text(this, _L("Linked planned print"), _L("Planned-print ID (optional)"), slot)) return;
    if (!ask_text(this, _L("Deadline UTC offset"), _L("Offset in minutes for 09:00 on that date, including daylight saving"), offset)) return;
    long parsed_offset = 0;
    if (!offset.ToLong(&parsed_offset) || parsed_offset < -840 || parsed_offset > 840) return;
    item.text = utf8(text); item.due_date = utf8(due); item.linked_member_id = utf8(member);
    item.linked_slot_id = utf8(slot); item.due_utc_offset_minutes = static_cast<int>(parsed_offset);
    if (!Workspace::edit_checklist_item(m_workspace, item)) return;
    m_dirty = true; refresh_checklist(); m_checklist->SetSelection(index);
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
    wxString title, printer, member, start, end, offset = "0";
    if (!ask_text(this, _L("Planned print"), _L("Title"), title) || title.IsEmpty()) return;
    if (!ask_text(this, _L("Printer"), _L("Printer ID"), printer)) return;
    if (!ask_text(this, _L("Linked project"), _L("Member ID (optional)"), member)) return;
    if (!ask_text(this, _L("Start time"), _L("Local time, YYYY-MM-DD HH:MM"), start)) return;
    if (!ask_text(this, _L("End time"), _L("Local time, YYYY-MM-DD HH:MM"), end)) return;
    if (!ask_text(this, _L("UTC offset"), _L("UTC offset in minutes for this date, including daylight saving"), offset)) return;
    long minutes = 0;
    if (!offset.ToLong(&minutes)) return;
    const auto begin = parse_wall_utc(utf8(start), static_cast<int>(minutes));
    const auto finish = parse_wall_utc(utf8(end), static_cast<int>(minutes));
    if (begin < 0 || finish <= begin) {
        wxMessageBox(_L("Enter valid start and end times, with the offset for the chosen date."),
                     _L("Planned print"), wxOK | wxICON_WARNING, this);
        return;
    }
    Workspace::CalendarSlot slot;
    slot.id = Workspace::new_id(); slot.title = utf8(title); slot.printer_id = utf8(printer);
    slot.member_id = utf8(member);
    slot.start_utc = begin; slot.end_utc = finish; slot.utc_offset_minutes = static_cast<int>(minutes);
    slot.time_zone = m_workspace.time_zone;
    m_workspace.slots.push_back(std::move(slot));
    m_dirty = true; refresh_overview(); refresh_calendar();
}

void WorkspacePanel::snooze_selected_slot()
{
    const long row = m_agenda->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row < 0) return;
    const long index = m_agenda->GetItemData(row);
    if (index < 0 || static_cast<std::size_t>(index) >= m_workspace.slots.size()) return;
    wxString when;
    if (!ask_text(this, _L("Snooze reminder"), _L("New reminder time, UTC YYYY-MM-DD HH:MM"), when)) return;
    const auto instant = parse_wall_utc(utf8(when), 0);
    if (instant > 0 && Workspace::snooze_slot(m_workspace, m_workspace.slots[index].id, instant)) m_dirty = true;
}

void WorkspacePanel::dismiss_selected_slot()
{
    const long row = m_agenda->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row < 0) return;
    const long index = m_agenda->GetItemData(row);
    if (index >= 0 && static_cast<std::size_t>(index) < m_workspace.slots.size() &&
        Workspace::dismiss_slot_reminder(m_workspace, m_workspace.slots[index].id)) m_dirty = true;
}

void WorkspacePanel::toggle_selected_slot()
{
    const long row = m_agenda->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row < 0) return;
    const long index = m_agenda->GetItemData(row);
    if (index < 0 || static_cast<std::size_t>(index) >= m_workspace.slots.size()) return;
    auto &slot = m_workspace.slots[static_cast<std::size_t>(index)];
    slot.enabled = !slot.enabled;
    m_dirty = true;
    refresh_calendar();
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
