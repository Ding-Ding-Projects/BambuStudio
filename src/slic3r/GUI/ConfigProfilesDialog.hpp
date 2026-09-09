#ifndef slic3r_GUI_ConfigProfilesDialog_hpp_
#define slic3r_GUI_ConfigProfilesDialog_hpp_

#include "GUI_Utils.hpp"
#include "Bulk/BulkSelection.hpp"
#include "libslic3r/ProjectHistoryManager.hpp"

#include <filesystem>
#include <future>
#include <string>
#include <vector>

#include <wx/timer.h>

class Button;
class Label;
class SearchField;
class SlideToConfirm;
class StaticBox;
class wxDataViewEvent;
class wxDataViewListCtrl;

namespace Slic3r::GUI {

// Configuration profiles, whole-data-directory backup, and per-profile local
// Git history.
//
//   * Export packs the ENTIRE active data directory (presets, physical printer
//     hosts, access codes, tokens, cached login state - i.e. secrets included)
//     into one archive for migrating to another PC. The action sits behind an
//     explicit slide-to-confirm gate with a secrets warning.
//   * Import unpacks such an archive into a NEW profile (never over the live
//     one), behind the same gate.
//   * Profiles are unlimited sibling data directories under
//     "<data-dir-parent>/<app>-profiles/"; any profile can be launched with
//     one click (a second app instance starts on that profile's data
//     directory).
//   * Every profile - including the active one - has a local, Git-backed
//     snapshot history driven by the same engine as project version history
//     (complete snapshots, isolated bare repo, restore-as-new-profile only).
//   * The list is multi-select through a BulkSelection keyed by the profile's
//     data-folder path: "Select visible" = rows matching the search, "Select
//     all N profiles" = every profile, "Invert selection" = visible rows.
//     Snapshot selected runs the single snapshot routine per selected profile
//     behind the reviewable preview and a cancellable progress dialog;
//     Export list writes the profile table as JSON or CSV. Launch stays a
//     single-row action (it starts another instance). There is no delete on
//     this surface, single or bulk: profiles are removed by deleting their
//     folder outside the app.
class ConfigProfilesDialog final : public DPIDialog
{
public:
    explicit ConfigProfilesDialog(wxWindow *parent);
    ~ConfigProfilesDialog() override;

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;
    void on_sys_color_changed() override;

private:
    struct ProfileRow
    {
        wxString              name;
        std::filesystem::path data_dir;
        bool                  active { false };
    };

    void create_ui();
    void apply_theme();
    void refresh_profiles();
    void populate_profiles();
    void update_buttons();
    void poll_operation(wxTimerEvent &event);

    void on_export(wxCommandEvent &event);
    void on_import(wxCommandEvent &event);
    void on_launch(wxCommandEvent &event);
    void on_snapshot(wxCommandEvent &event);
    void on_history(wxCommandEvent &event);
    void on_prefs_history(wxCommandEvent &event);
    void on_bulk_snapshot(wxCommandEvent &event);
    void on_export_list(wxCommandEvent &event);
    void on_char_hook(wxKeyEvent &event);

    // Selection model helpers. Ids are data-folder paths (stable across a
    // repopulate); "page" = rows shown after the search, "all" = every profile.
    std::vector<std::string> page_ids() const;
    std::vector<std::string> all_ids() const;
    void sync_selection_from_view();
    void sync_selection_to_view();
    void select_page();
    void select_all_matches();
    void invert_page();
    // Records one complete snapshot of `row` (blocking). Empty string = success.
    static wxString record_snapshot(const ProfileRow &row, const std::filesystem::path &identity,
                                    const std::filesystem::path &staging, ProjectHistoryManager *history);

    // The single selected profile, or nullptr when zero or several rows are selected.
    const ProfileRow *selected_profile() const;
    std::filesystem::path profiles_root() const;
    // Stable per-profile archive path: both the snapshot input and the
    // history identity for ProjectHistoryManager.
    std::filesystem::path profile_archive_path(const ProfileRow &row) const;

    std::vector<ProfileRow> m_profiles;
    std::vector<std::size_t> m_filtered_rows;
    Bulk::BulkSelection<std::string> m_selection;
    bool                             m_syncing_selection { false };

    std::unique_ptr<ProjectHistoryManager> m_history;
    std::future<wxString>                  m_busy_future; // empty string = success, else error text
    std::function<void(wxString)>          m_busy_done;
    wxTimer                                m_poll_timer;
    bool                                   m_busy { false };

    Label              *m_title_label { nullptr };
    Label              *m_subtitle_label { nullptr };
    SearchField        *m_search_field { nullptr };
    wxDataViewListCtrl *m_profile_list { nullptr };
    StaticBox          *m_list_card { nullptr };
    StaticBox          *m_transfer_card { nullptr };
    Label              *m_secrets_label { nullptr };
    SlideToConfirm     *m_confirm_slider { nullptr };
    Label              *m_status_label { nullptr };
    Button             *m_export_button { nullptr };
    Button             *m_import_button { nullptr };
    Button             *m_launch_button { nullptr };
    Button             *m_snapshot_button { nullptr };
    Button             *m_history_button { nullptr };
    Button             *m_prefs_history_button { nullptr };
    Button             *m_select_page_button { nullptr };
    Button             *m_select_all_button { nullptr };
    Button             *m_invert_button { nullptr };
    Button             *m_bulk_snapshot_button { nullptr };
    Button             *m_export_list_button { nullptr };
    Button             *m_close_button { nullptr };
};

} // namespace Slic3r::GUI

#endif // slic3r_GUI_ConfigProfilesDialog_hpp_
