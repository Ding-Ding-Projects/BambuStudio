#ifndef slic3r_GUI_ProjectHistoryDialog_hpp_
#define slic3r_GUI_ProjectHistoryDialog_hpp_

#include "GUI_Utils.hpp"

#include "libslic3r/ProjectHistoryManager.hpp"

#include <filesystem>
#include <future>
#include <string>
#include <vector>

#include <wx/timer.h>

class Button;
class Label;
class StaticBox;
class wxCloseEvent;
class wxCommandEvent;
class wxDataViewEvent;
class wxDataViewListCtrl;
class wxSizeEvent;

namespace Slic3r::GUI {

class Plater;

// Presents the immutable, app-local Git history for the currently open
// project. Git work stays on ProjectHistoryManager's serialized worker; this
// dialog only polls completed futures from the UI thread.
class ProjectHistoryDialog final : public DPIDialog
{
public:
    ProjectHistoryDialog(wxWindow *parent, Plater *plater);
    ~ProjectHistoryDialog() override;

    // Transfers ownership of the temporary restored archive to the caller.
    // The caller must delete the archive after Plater has loaded it.
    std::filesystem::path release_restored_snapshot();

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;
    void on_sys_color_changed() override;

private:
    enum class PendingOperation { None, List, Restore };

    void create_ui();
    void apply_theme();
    void refresh_versions();
    // Surfaces terminally-failed / quarantined project-history snapshots kept on
    // disk, with a Retry action that re-drives them through the Plater.
    void refresh_retained_failures();
    void begin_restore();
    void poll_operation(wxTimerEvent &event);
    void finish_list(ProjectHistoryListResult result);
    void finish_restore(ProjectHistoryRestoreResult result);
    void populate_versions();
    void update_history_status();
    void update_responsive_layout();
    void update_window_constraints(bool initialize_size);
    void set_busy(PendingOperation operation, const wxString &message);
    void show_empty_state();
    void show_error(const wxString &message);
    void update_selection();
    void cleanup_restore_temp();

    void on_refresh(wxCommandEvent &event);
    void on_retry_failures(wxCommandEvent &event);
    void on_restore(wxCommandEvent &event);
    void on_close_button(wxCommandEvent &event);
    void on_close_window(wxCloseEvent &event);
    void on_selection_changed(wxDataViewEvent &event);
    void on_item_activated(wxDataViewEvent &event);
    void on_load_all(wxCommandEvent &event);
    void on_size(wxSizeEvent &event);

    std::filesystem::path make_restore_destination();
    static wxString        format_timestamp(const std::chrono::system_clock::time_point &timestamp);
    static wxString        format_size(std::uint64_t bytes);
    static wxString        display_message(const std::string &message);

private:
    Plater                        *m_plater{nullptr};
    ProjectHistoryManager        *m_manager{nullptr};
    std::filesystem::path         m_project_identity;
    std::vector<ProjectHistoryVersion> m_versions;

    std::future<ProjectHistoryListResult>    m_list_future;
    std::future<ProjectHistoryRestoreResult> m_restore_future;
    wxTimer                                  m_poll_timer;
    PendingOperation                         m_pending{PendingOperation::None};

    std::filesystem::path m_restore_temp_dir;
    std::filesystem::path m_restored_snapshot;
    bool                  m_show_all{false};
    bool                  m_list_truncated{false};

    Label                  *m_title_label{nullptr};
    Label                  *m_subtitle_label{nullptr};
    Label                  *m_project_label{nullptr};
    Label                  *m_status_label{nullptr};
    Label                  *m_safety_label{nullptr};
    Label                  *m_failure_title_label{nullptr};
    Label                  *m_failure_detail_label{nullptr};
    StaticBox              *m_info_card{nullptr};
    StaticBox              *m_failure_card{nullptr};
    StaticBox              *m_list_card{nullptr};
    wxDataViewListCtrl     *m_version_list{nullptr};
    Button                 *m_refresh_button{nullptr};
    Button                 *m_load_all_button{nullptr};
    Button                 *m_retry_failures_button{nullptr};
    Button                 *m_restore_button{nullptr};
    Button                 *m_close_button{nullptr};
};

} // namespace Slic3r::GUI

#endif // slic3r_GUI_ProjectHistoryDialog_hpp_
