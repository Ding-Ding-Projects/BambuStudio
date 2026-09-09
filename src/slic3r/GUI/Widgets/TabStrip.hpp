#ifndef slic3r_GUI_TabStrip_hpp_
#define slic3r_GUI_TabStrip_hpp_

// Shared browser-style tab strip (Material Design 3), generalised from the
// project tab bar so every tabbed surface -- project tabs, the Preferences
// sections and any future settings-like surface -- carries the same contract:
//
//   * dock edge (Left / Right / Top / Bottom; persisted per surface, reachable
//     from the strip's context menu);
//   * an overflow surface: a Material menu listing the tabs that no longer fit
//     the strip's main axis (never silently clipped) plus, for hide-on-close
//     surfaces, the tabs hidden from the strip so they can be restored;
//   * drag reorder and keyboard reorder (Ctrl+Shift+Arrow along the axis);
//   * pinning: a dedicated leading region, protected from bulk closes and from
//     the per-tab close affordance;
//   * grouping: create / rename / colour / collapse / expand / remove groups,
//     with a "Move into group..." picker dialog carrying its own SearchField;
//   * persistence of order / pins / groups / collapsed state / dock edge per
//     surface through AppConfig (section "tab_strips", key = surface_key);
//   * tablist / tab accessibility roles with orientation semantics: a vertical
//     strip moves selection with Up / Down, a horizontal one with Left / Right;
//   * four tab-discovery searches (current strip, inside one group, groups by
//     name, master over every strip the app owns), each a SearchField with its
//     own anchored regex builder;
//   * two bulk-close actions ("Close tabs containing text" / "Close tabs not
//     containing text") with a preview, pinned excluded by default;
//   * context menus that show their shortcuts and carry "Edit tab appearance..."
//     and "Edit group appearance..." through a weak hook set at startup.
//
// The data rules (ordering, overflow arithmetic, bulk-close preview, search
// result shape, keyboard orientation) live in TabStripModel.hpp so they are
// unit-tested without a running application.

#include <functional>
#include <string>
#include <vector>

#include <wx/colour.h>
#include <wx/event.h>
#include <wx/panel.h>
#include <wx/string.h>

#include "TabStripModel.hpp"

class Button;
class SearchField;

namespace Slic3r { namespace GUI {

// GetString() carries the tab id; GetInt() its model index (or -1).
wxDECLARE_EVENT(EVT_TABSTRIP_ACTIVATE, wxCommandEvent);
// The user asked to close a tab on a CloseMode::Close surface. The host
// decides (unsaved-work protection) and calls RemoveTab() itself.
wxDECLARE_EVENT(EVT_TABSTRIP_CLOSE_REQUEST, wxCommandEvent);
// The trailing "+" action.
wxDECLARE_EVENT(EVT_TABSTRIP_NEW, wxCommandEvent);
// Layout changed (order / pin / group / hidden / edge). Already persisted.
wxDECLARE_EVENT(EVT_TABSTRIP_CHANGED, wxCommandEvent);
// Dock edge changed: hosts re-place the strip in their sizer.
wxDECLARE_EVENT(EVT_TABSTRIP_DOCK_CHANGED, wxCommandEvent);

class TabStripButton;
class TabStripGroupHeader;

class TabStrip : public wxPanel
{
public:
    // What "close" means on this surface. Hide keeps the tab in the model,
    // drops it from the strip and lists it in the overflow menu for restoring
    // (settings sections). Close raises EVT_TABSTRIP_CLOSE_REQUEST and lets
    // the host close for real, with its own unsaved-work protection.
    enum class CloseMode { Hide, Close };

    struct Options
    {
        std::string         surface_key;   // AppConfig key, e.g. "preferences"
        wxString            surface_name;  // shown in search results
        wxString            strip_name;    // shown in search results
        MD3::Tabs::DockEdge default_edge = MD3::Tabs::DockEdge::Left;
        CloseMode           close_mode   = CloseMode::Hide;
        bool                show_new_button = false;
        bool                allow_close     = true;
        // When true, a press / Enter / search hit / overflow pick does NOT
        // change the active tab itself: the strip only raises
        // EVT_TABSTRIP_ACTIVATE and the host calls Activate(id, false) once
        // its own switch (snapshot save / load) succeeded. Project tabs.
        bool                host_confirms_activation = false;
    };

    TabStrip(wxWindow *parent, const Options &options);
    ~TabStrip() override;

    const Options &GetOptions() const { return m_options; }

    // --- model access ---------------------------------------------------------
    MD3::Tabs::Model &      GetModel() { return m_model; }
    const MD3::Tabs::Model &GetModel() const { return m_model; }

    // --- tabs -----------------------------------------------------------------
    // Returns the model index. Persists.
    int  AddTab(const std::string &id, const wxString &title, const std::string &payload = std::string(),
                bool activate = false);
    void RemoveTab(const std::string &id);
    void SetTitle(const std::string &id, const wxString &title);
    void SetDirty(const std::string &id, bool dirty);
    // Make a tab current. Reveals it when it sits in a collapsed group or was
    // hidden from the strip; `emit` sends EVT_TABSTRIP_ACTIVATE.
    void Activate(const std::string &id, bool emit = true);
    std::string ActiveId() const { return m_model.active(); }
    int         ActiveIndex() const { return m_model.active_index(); }
    int         Count() const { return m_model.size(); }

    void SetPinned(const std::string &id, bool pinned);
    void SetHidden(const std::string &id, bool hidden);
    void MoveTab(int from, int to);

    // --- groups ---------------------------------------------------------------
    int  CreateGroup(const wxString &name, const wxColour &color);
    void AssignGroup(const std::string &id, int group_id); // -1 = ungroup
    void RenameGroup(int group_id, const wxString &name);
    void SetGroupColor(int group_id, const wxColour &color);
    void SetGroupCollapsed(int group_id, bool collapsed);
    void RemoveGroup(int group_id);

    // --- dock edge ------------------------------------------------------------
    MD3::Tabs::DockEdge GetDockEdge() const { return m_model.edge(); }
    void                SetDockEdge(MD3::Tabs::DockEdge edge);
    bool                IsVertical() const { return MD3::Tabs::is_vertical(m_model.edge()); }

    // --- persistence ----------------------------------------------------------
    // Apply the saved layout (order / pins / hidden / groups / edge) to tabs
    // already added in code. Call once after the surface built its tabs.
    void LoadLayout();
    // Rebuild every tab from the saved layout (surfaces whose tabs live only
    // in the layout, e.g. project tabs). Returns the number of tabs restored.
    int  LoadTabsFromLayout();
    void SaveLayout();

    // --- discovery ------------------------------------------------------------
    void OpenStripSearch();
    void OpenGroupSearch(int group_id);
    void OpenGroupsSearch();
    // Every strip the app owns (project tabs + settings strips + ...).
    static void OpenMasterSearch(wxWindow *owner);
    static const std::vector<TabStrip *> &Registry();
    // Activate a hit found by any search: reveals a tab inside a collapsed
    // group without touching the collapsed preference.
    void ActivateHit(const MD3::Tabs::SearchHit &hit);

    // --- bulk close -----------------------------------------------------------
    void OpenBulkClose(bool not_containing);

    // --- context menus --------------------------------------------------------
    void ShowTabMenu(const std::string &id, const wxPoint &screen_pos);
    void ShowGroupMenu(int group_id, const wxPoint &screen_pos);
    void ShowStripMenu(const wxPoint &screen_pos);

    // --- appearance editor hook -----------------------------------------------
    // Set once at startup by the appearance-editor lane:
    //   hook(anchor_window, element_id)
    // element_id is "tabstrip:<surface>", "tab:<surface>:<id>" or
    // "group:<surface>:<group id>". No-op until the hook is installed.
    using AppearanceHook = std::function<void(wxWindow *anchor, const std::string &element_id)>;
    static void SetAppearanceEditorHook(AppearanceHook hook);
    static bool HasAppearanceEditorHook();
    static void RequestAppearanceEditor(wxWindow *anchor, const std::string &element_id);

    // --- DPI / theme ----------------------------------------------------------
    void Rescale();

    // Keyboard focus lives on the strip itself (roving focus over tabs).
    bool AcceptsFocus() const override { return true; }
    bool AcceptsFocusFromKeyboard() const override { return IsShown() && IsThisEnabled(); }

    // Main-axis extent of one tab, DIP (44 for a vertical strip; horizontal
    // tabs measure their label between the min / max widths below).
    static constexpr int kTabHeight   = 44;
    static constexpr int kTabMinWidth = 96;
    static constexpr int kTabMaxWidth = 240;
    static constexpr int kRailWidth   = 230; // vertical strip width (settings nav width)
    static constexpr int kBarHeight   = 52;  // horizontal strip height

private:
    friend class TabStripButton;
    friend class TabStripGroupHeader;

    // Activation request from the user (press, Enter, overflow pick, search
    // hit): immediate, or deferred to the host per Options.
    void RequestActivate(const std::string &id);

    // Called by the buttons.
    void OnTabPressed(const std::string &id);
    void OnTabCloseClicked(const std::string &id);
    void OnTabDragEnd(const std::string &id, const wxPoint &screen);
    void OnTabContext(const std::string &id, const wxPoint &screen, bool shift);
    void OnGroupHeaderPressed(int group_id);

    // Layout / paint.
    void Relayout();
    void SyncButtons(); // one button per model tab, in model order
    void OnPaint(wxPaintEvent &evt);
    void OnSize(wxSizeEvent &evt);
    void OnKeyDown(wxKeyEvent &evt);
    void OnFocus(wxFocusEvent &evt);
    void OnContextMenu(wxContextMenuEvent &evt);
    void OnRightUp(wxMouseEvent &evt);
    void OpenOverflowMenu();
    void RestyleAll();
    void Changed(); // persist + EVT_TABSTRIP_CHANGED + relayout

    // Focus helpers (displayed positions).
    int  FocusedModelIndex() const;
    void SetFocusedModelIndex(int index);
    void CloseOrHide(const std::string &id);

    // Dialog helpers.
    bool PromptGroupName(wxString &name, const wxString &title);
    bool PromptGroupColor(wxColour &color);
    void MoveIntoGroupPicker(const std::string &tab_id);
    void CloseMany(const std::vector<std::string> &ids);

    MD3::Tabs::Matcher MakeMatcher(::SearchField *field) const;

    Options                              m_options;
    MD3::Tabs::Model                     m_model;
    std::vector<TabStripButton *>        m_buttons;           // parallel to m_model.tabs()
    std::vector<TabStripGroupHeader *>   m_headers;           // one per group (created lazily)
    Button *                             m_overflow_btn = nullptr;
    Button *                             m_add_btn      = nullptr;
    Button *                             m_search_btn   = nullptr;
    std::vector<int>                     m_overflowed;        // model indices in the overflow menu
    int                                  m_focus_index = -1;  // model index carrying keyboard focus
    bool                                 m_loading     = false;

#if wxUSE_ACCESSIBILITY
    friend class TabStripAccessible;
#endif
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_TabStrip_hpp_
