#ifndef slic3r_GUI_Schedule_ScheduledSettingsPanel_hpp_
#define slic3r_GUI_Schedule_ScheduledSettingsPanel_hpp_

// Preferences > Schedules: the rule list (searchable), the add/edit dialog
// with native date and time pickers, weekday toggles, the source picker and
// value pickers populated from the app's real choices, plus per-rule status
// from the Scheduler. The panel is a plain scrolled page so Preferences can
// host it like every other section and index its labels for settings search.

#include "ScheduledSettingsModel.hpp"

#include <wx/scrolwin.h>

#include <functional>
#include <string>
#include <vector>

class Button;
class Label;
class SearchField;

namespace Slic3r { namespace GUI {

class ListBox;

namespace Schedule {

class ScheduledSettingsPanel final : public wxScrolledWindow
{
public:
    explicit ScheduledSettingsPanel(wxWindow *parent);
    ~ScheduledSettingsPanel() override;

    // Rows Preferences should index for its settings search (label text lives
    // in the windows themselves; this returns the top-level sizers).
    const std::vector<wxSizer *> &search_rows() const { return m_search_rows; }

    void Rescale();

private:
    void rebuild_list();
    void refresh_status();
    void add_rule();
    void edit_selected();
    void toggle_selected();
    void move_selected(int delta);
    void delete_selected();
    int  selected_document_index() const; // -1 when nothing selected
    bool commit(const Document &document);

    SearchField          *m_search { nullptr };
    ListBox              *m_list { nullptr };
    Label                *m_empty { nullptr };
    Label                *m_status { nullptr };
    Label                *m_detail { nullptr };
    Label                *m_timezone { nullptr };
    Button               *m_edit { nullptr };
    Button               *m_toggle { nullptr };
    Button               *m_up { nullptr };
    Button               *m_down { nullptr };
    Button               *m_delete { nullptr };
    std::vector<int>      m_visible; // list row -> document index
    std::vector<wxSizer *> m_search_rows;
};

} } } // namespace Slic3r::GUI::Schedule

#endif // slic3r_GUI_Schedule_ScheduledSettingsPanel_hpp_
