#ifndef slic3r_GUI_NotificationCenterPanel_hpp_
#define slic3r_GUI_NotificationCenterPanel_hpp_

#include "NotificationHistory.hpp"
#include "Widgets/MD3Dialog.hpp"

#include <cstdint>
#include <set>
#include <vector>

#include <wx/timer.h>

class Button;
class Label;
class SearchField;
class SlideToConfirm;
class StaticBox;
class wxDataViewEvent;
class wxDataViewListCtrl;
class wxCommandEvent;
class wxKeyEvent;

namespace Slic3r { namespace GUI {

class NotificationManager;

// Notification centre: the reviewable history of every toast the app has shown
// (docs/features/workspace/notification-center.md). A non-modal MD3 popover
// anchored under the top-bar bell; it reads NotificationManager::history() and
// polls its revision while open so new toasts appear without a reopen.
//
// Anatomy (top to bottom): SearchField (plain by default, regex via the
// builder) + level chips + "show dismissed" chip; status line stating the
// page / match / selection counts; a multi-select wxDataViewListCtrl (click,
// Shift+click ranges, Ctrl+click, Ctrl+A for the page, arrow keys); a selection
// row whose two select-all actions are named "this page" versus "all matches";
// a bulk row (dismiss, export honouring the active filter, delete behind a
// SlideToConfirm gate); and an empty-state label when nothing matches.
class NotificationCenterPanel : public MD3Dialog
{
public:
    static constexpr int PAGE_SIZE = 100;

    NotificationCenterPanel(wxWindow *parent, NotificationManager *manager);
    ~NotificationCenterPanel() override;

    // Position the popover under `screen_anchor` (the bell's screen rect),
    // clamped to the display so it never leaves the viewport.
    void AnchorBelow(const wxRect &screen_anchor);
    // Re-read the history immediately (the timer does this every second).
    void RefreshNow();

protected:
    void OnHeaderClose() override;
    void on_dpi_changed(const wxRect &suggested_rect) override;
    void on_sys_color_changed() override;

private:
    enum class LevelChip { All, Info, Important, Warning, Error, Count };

    void build_ui();
    void apply_theme();
    NotificationHistory::Filter current_filter() const;
    void recompute_matches();
    void populate_list();
    void sync_selection_from_view();
    void sync_selection_to_view();
    void update_status();
    void update_bulk_buttons();
    void update_level_chips();
    static std::set<int> chip_levels(LevelChip chip);
    static wxString level_label(int level);
    wxString format_time(std::int64_t timestamp_ms) const;

    void on_timer(wxTimerEvent &event);
    void on_selection_changed(wxDataViewEvent &event);
    void on_list_key(wxKeyEvent &event);
    void on_select_page(wxCommandEvent &event);
    void on_select_all_matches(wxCommandEvent &event);
    void on_invert_selection(wxCommandEvent &event);
    void on_clear_selection(wxCommandEvent &event);
    void on_load_more(wxCommandEvent &event);
    void on_dismiss_selected(wxCommandEvent &event);
    void on_export(wxCommandEvent &event);
    void on_delete_requested(wxCommandEvent &event);
    void on_delete_cancelled(wxCommandEvent &event);
    void on_delete_confirmed();
    void hide_delete_gate();

    NotificationManager *          m_manager{nullptr};
    NotificationHistory::Selection m_selection;
    std::vector<std::uint64_t>     m_matches;   // every id the filter yields, newest first
    std::vector<std::uint64_t>     m_page;      // the rendered slice of m_matches
    std::size_t                    m_page_limit{PAGE_SIZE};
    std::uint64_t                  m_seen_revision{0};
    LevelChip                      m_level_chip{LevelChip::All};
    bool                           m_show_dismissed{true};
    bool                           m_syncing_selection{false};
    wxTimer                        m_timer;

    SearchField *        m_search{nullptr};
    Button *             m_level_buttons[static_cast<int>(LevelChip::Count)]{};
    Button *             m_dismissed_chip{nullptr};
    Label *              m_status_label{nullptr};
    wxDataViewListCtrl * m_list{nullptr};
    Label *              m_empty_label{nullptr};
    Button *             m_select_page_button{nullptr};
    Button *             m_select_all_button{nullptr};
    Button *             m_invert_button{nullptr};
    Button *             m_clear_button{nullptr};
    Button *             m_load_more_button{nullptr};
    Button *             m_dismiss_button{nullptr};
    Button *             m_export_button{nullptr};
    Button *             m_delete_button{nullptr};
    StaticBox *          m_delete_card{nullptr};
    Label *              m_delete_label{nullptr};
    SlideToConfirm *     m_delete_gate{nullptr};
    Button *             m_delete_cancel_button{nullptr};
};

} } // namespace Slic3r::GUI

#endif // slic3r_GUI_NotificationCenterPanel_hpp_
