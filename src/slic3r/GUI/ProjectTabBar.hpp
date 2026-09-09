#ifndef slic3r_GUI_ProjectTabBar_hpp_
#define slic3r_GUI_ProjectTabBar_hpp_

// Session file-tabs strip. ProjectTabBar is the project-surface face of the
// shared browser-style TabStrip (Widgets/TabStrip.hpp): it keeps the
// index-based API MainFrame orchestrates against (AddTab / CloseTab /
// SetActive / TabAt ...) and the three EVT_PROJECT_TAB_* events, while the
// strip itself supplies the whole tab contract -- dock edge (top by default
// for projects), overflow menu, drag and keyboard reorder, pinning, grouping,
// the four tab searches, the two bulk-close actions, persistence and the
// tablist accessibility roles.
//
// It has NO Plater dependency. MainFrame listens to the three events below
// and orchestrates the save-outgoing / load-selected snapshot round-trip
// through Plater; ProjectTabBar never touches a project. "Close" on this
// surface is a real close: the strip only asks (EVT_PROJECT_TAB_CLOSE) and
// MainFrame decides after its unsaved-work checks.

#include <map>
#include <string>
#include <vector>

#include <wx/colour.h>
#include <wx/event.h>
#include <wx/string.h>

#include "Widgets/TabStrip.hpp"

namespace Slic3r { namespace GUI {

// One open project. snapshot_path is the per-tab temp .3mf MainFrame serialises
// the outgoing (dirty) project to; file_path is the on-disk project (may be
// empty for an unsaved "+" tab). title is the display label, dirty the unsaved
// indicator, group_id the owning group id (-1 = ungrouped).
struct ProjectTab
{
    std::string file_path;
    std::string snapshot_path;
    wxString    title;
    bool        dirty    = false;
    int         group_id = -1;
};

// Kept for source compatibility with callers that name it; groups now live in
// the strip model (MD3::Tabs::Group).
struct TabGroup
{
    int      id;
    wxString name;
    wxColour color;
};

// GetInt() carries the target/subject tab index for all three.
wxDECLARE_EVENT(EVT_PROJECT_TAB_SWITCH, wxCommandEvent); // GetInt()=target index
wxDECLARE_EVENT(EVT_PROJECT_TAB_CLOSE,  wxCommandEvent); // GetInt()=index to close
wxDECLARE_EVENT(EVT_PROJECT_TAB_NEW,    wxCommandEvent); // "+" new tab

class ProjectTabBar : public TabStrip
{
public:
    ProjectTabBar(wxWindow *parent);
    ~ProjectTabBar() override;

    // Model + UI mutation ---------------------------------------------------
    int  AddTab(const std::string &file_path, const wxString &title, bool activate = true); // returns index
    void CloseTab(int i);           // removes from model + re-layouts (no save/load)
    void SetActive(int i);          // visual + model active index (no event)
    int  GetActive() const;
    ProjectTab &      TabAt(int i);
    void SetActiveDirty(bool dirty);        // update active tab dirty dot
    void SetActiveTitle(const wxString &t); // update active tab label
    void Reorder(int from, int to);         // permute + relayout

    // Grouping (index-based conveniences over the strip API) ----------------
    int  CreateGroup(const wxString &name, const wxColour &color);
    void AssignGroup(int tab_i, int group_id); // -1 = ungrouped

    // Persistence: the strip layout JSON (AppConfig [tab_strips] projects)
    // carries file paths as tab payloads. LoadFromConfig also imports the
    // legacy [project_tabs] / [tab_groups] sections once.
    void SaveToConfig();
    void LoadFromConfig();

private:
    std::string IdAt(int i) const;
    std::string NewId(const std::string &file_path);

    std::map<std::string, ProjectTab> m_projects; // keyed by strip tab id
    int                               m_next_untitled = 1;
};

}} // namespace Slic3r::GUI

#endif // !slic3r_GUI_ProjectTabBar_hpp_
