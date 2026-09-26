#pragma once

#include "libslic3r/WorkspaceBundle.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <utility>
#include <vector>
#include <wx/panel.h>
#include <wx/timer.h>

class wxCalendarCtrl;
class wxCheckListBox;
class wxListCtrl;
class wxNotebook;
class wxStaticText;
class wxTextCtrl;

namespace Slic3r::GUI {

struct WorkspaceMemberSelection {
    std::string bundle_id;
    std::string member_id;
    std::filesystem::path bundle_path;
    // Caller must copy this file if it needs it after the panel changes
    // workspace or is destroyed. It remains owned by the private staging area.
    std::filesystem::path staged_project_path;
};

// Native editor for a local workspace bundle. It does not submit prints or
// change the active Plater document. MainFrame owns the later file-tab handoff.
class WorkspacePanel final : public wxPanel
{
public:
    explicit WorkspacePanel(wxWindow *parent);
    ~WorkspacePanel() override;

    bool open_bundle(const std::filesystem::path &path);
    bool save_bundle();
    // Copies a finished history-bearing project into private staging, then
    // atomically publishes the whole bundle. A failed publish restores the
    // in-memory member path and retains the candidate for recovery.
    bool save_member(const std::string &bundle_id, const std::string &member_id,
                     const std::filesystem::path &history_bearing_3mf);
    using MemberOpenHandler = std::function<void(const WorkspaceMemberSelection &)>;
    void set_member_open_handler(MemberOpenHandler handler) { m_member_open_handler = std::move(handler); }
    std::optional<WorkspaceMemberSelection> selected_member() const;
    const std::filesystem::path &pending_member_recovery() const { return m_pending_member_recovery; }
    const Workspace::Workspace &workspace() const { return m_workspace; }
    const std::filesystem::path &bundle_path() const { return m_bundle_path; }

private:
    void create_ui();
    void refresh_all();
    void refresh_overview();
    void refresh_files();
    void refresh_checklist();
    void refresh_calendar();
    void create_new();
    void edit_preferences();
    void choose_open();
    void choose_save();
    void add_member();
    void open_selected_member();
    std::optional<std::filesystem::path> stage_member_file(const std::filesystem::path &source);
    void add_source();
    void add_checklist();
    void edit_checklist();
    void move_checklist(int direction);
    void add_slot();
    void snooze_selected_slot();
    void dismiss_selected_slot();
    void toggle_selected_slot();
    void export_checklist(bool csv);
    void export_calendar();
    void reset_reminder_cursor();
    void check_reminders();

    Workspace::Workspace m_workspace;
    std::filesystem::path m_bundle_path;
    std::filesystem::path m_staging_root;
    std::filesystem::path m_loaded_staging;
    std::vector<std::filesystem::path> m_owned_member_files;
    MemberOpenHandler m_member_open_handler;
    std::filesystem::path m_pending_member_recovery;
    wxTimer m_reminder_timer;
    std::int64_t m_last_reminder_check_utc = 0;
    bool m_dirty = false;
    wxNotebook *m_sections = nullptr;
    wxStaticText *m_overview = nullptr;
    wxListCtrl *m_files = nullptr;
    wxCheckListBox *m_checklist = nullptr;
    wxTextCtrl *m_notes = nullptr;
    wxCalendarCtrl *m_month = nullptr;
    wxListCtrl *m_agenda = nullptr;
};

} // namespace Slic3r::GUI
