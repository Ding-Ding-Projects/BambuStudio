#ifndef slic3r_GUI_ProjectTabBar_hpp_
#define slic3r_GUI_ProjectTabBar_hpp_

// Session file-tabs strip (SPIKE-APPROVED design). ProjectTabBar owns the tab
// UI, the ProjectTab/TabGroup data model and its AppConfig persistence. It has
// NO Plater dependency: it is pure GUI chrome. MainFrame (Group 3) listens to
// the three events below and orchestrates the save-outgoing/load-selected
// snapshot round-trip through Plater; ProjectTabBar never touches a project.
//
// The chrome is modelled on ButtonsListCtrl (Notebook.cpp): kit Button widgets
// in a horizontal box sizer, MD3 token colours (StateColor::semantic), labels
// in Label::Body_14 so they track Label::rebuild_fonts(), and a custom OnPaint
// that draws the contiguous-group indicator and the active-tab underline. The
// tab-bar accent is chrome above the per-workspace data scheme, so it is pinned
// to ColorScheme::Brand (MD3 sec.4/sec.13), exactly like the notebook tab bar.

#include <cstdint>
#include <string>
#include <vector>

#include <wx/colour.h>
#include <wx/event.h>
#include <wx/panel.h>
#include <wx/string.h>

class Button;
class wxBoxSizer;

namespace Slic3r { namespace GUI {

// One open project. snapshot_path is the per-tab temp .3mf MainFrame serialises
// the outgoing (dirty) project to; file_path is the on-disk project (may be
// empty for an unsaved "+" tab). title is the display label, dirty the unsaved
// indicator, group_id the owning TabGroup::id (-1 = ungrouped).
struct ProjectTab
{
    std::string file_path;
    std::string snapshot_path;
    wxString    title;
    bool        dirty    = false;
    int         group_id = -1;
};

// A named, coloured tab group. color drives both the per-tab chip and the
// contiguous-group underline the bar paints behind same-group runs.
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

// Internal per-tab composite widget (defined in ProjectTabBar.cpp). Forward
// declared here so the bar can hold typed pointers without exposing the widget
// as public API.
class ProjectTabButton;

class ProjectTabBar : public wxPanel
{
public:
    ProjectTabBar(wxWindow *parent);
    ~ProjectTabBar() override;

    // Model + UI mutation ---------------------------------------------------
    int  AddTab(const std::string &file_path, const wxString &title, bool activate = true); // returns index
    void CloseTab(int i);           // removes from model + re-layouts (no save/load)
    void SetActive(int i);          // visual + model active index (no event)
    int  GetActive() const;
    int  Count() const;
    ProjectTab &      TabAt(int i);
    void SetActiveDirty(bool dirty);        // update active tab dirty dot
    void SetActiveTitle(const wxString &t); // update active tab label
    void Reorder(int from, int to);         // drag-reorder: permute + relayout

    // Grouping --------------------------------------------------------------
    int  CreateGroup(const wxString &name, const wxColour &color);
    void AssignGroup(int tab_i, int group_id); // -1 = ungrouped

    // Persistence via AppConfig sections [project_tabs] / [tab_groups] ------
    void SaveToConfig();
    void LoadFromConfig(); // repopulates tabs+groups (paths lazily loaded by MainFrame)

    // DPI + re-fetch fonts --------------------------------------------------
    void Rescale();

private:
    friend class ProjectTabButton;

    // Event helpers used by the per-tab widgets.
    void EmitSwitch(int index);
    void EmitClose(int index);
    void OnTabPressed(int index);        // click (not a drag) -> switch
    void OnTabDragEnd(int from, int screen_x); // drop -> compute target + reorder

    // Layout / paint --------------------------------------------------------
    void OnPaint(wxPaintEvent &evt);
    void Relayout();
    void RestyleAll();

    // Lookup helpers.
    wxColour GroupColor(int group_id) const; // invalid colour when not found / -1
    int      IndexOfTab(const ProjectTabButton *btn) const;

    std::vector<ProjectTab>        m_tabs;    // data model (order == visual order)
    std::vector<ProjectTabButton *> m_buttons; // parallel UI widgets
    std::vector<TabGroup>          m_groups;
    int                            m_active = -1;
    int                            m_next_group_id = 1;

    wxBoxSizer *m_root_sizer    = nullptr;
    wxBoxSizer *m_buttons_sizer = nullptr;
    wxBoxSizer *m_actions_sizer = nullptr;
    Button *    m_add_btn       = nullptr;
};

}} // namespace Slic3r::GUI

#endif // !slic3r_GUI_ProjectTabBar_hpp_
