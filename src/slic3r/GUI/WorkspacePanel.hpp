#pragma once

#include "libslic3r/WorkspaceBundle.hpp"

#include <filesystem>
#include <wx/panel.h>

class wxCalendarCtrl;
class wxCheckListBox;
class wxListCtrl;
class wxNotebook;
class wxStaticText;
class wxTextCtrl;

namespace Slic3r::GUI {

// Native editor for a local workspace bundle. It does not submit prints or
// change the active Plater document. MainFrame owns the later file-tab handoff.
class WorkspacePanel final : public wxPanel
{
public:
    explicit WorkspacePanel(wxWindow *parent);
    ~WorkspacePanel() override;

    bool open_bundle(const std::filesystem::path &path);
    bool save_bundle();
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

    Workspace::Workspace m_workspace;
    std::filesystem::path m_bundle_path;
    std::filesystem::path m_staging_root;
    std::filesystem::path m_loaded_staging;
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
