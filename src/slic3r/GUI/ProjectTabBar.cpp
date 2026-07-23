#include "ProjectTabBar.hpp"

#include "GUI_App.hpp"
#include "I18N.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/MaterialIcon.hpp"
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/StateColor.hpp"

#include "libslic3r/AppConfig.hpp"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <wx/dcbuffer.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/utils.h> // wxGetMousePosition

namespace Slic3r { namespace GUI {

wxDEFINE_EVENT(EVT_PROJECT_TAB_SWITCH, wxCommandEvent);
wxDEFINE_EVENT(EVT_PROJECT_TAB_CLOSE,  wxCommandEvent);
wxDEFINE_EVENT(EVT_PROJECT_TAB_NEW,    wxCommandEvent);

namespace {

// All values are logical/design px; FromDIP() handles the monitor-DPI scale.
constexpr int bar_height        = MD3::Metrics::navigation_bar_height; // 52
constexpr int bar_bottom_space  = 6;   // strip below the tabs for the indicators
constexpr int tab_h_padding     = 12;  // per-side content padding
constexpr int tab_min_width     = 96;
constexpr int tab_max_width     = 240;
constexpr int chip_size         = 10;  // group-colour chip edge
constexpr int chip_gap          = 6;
constexpr int dot_size          = 6;   // dirty dot diameter
constexpr int content_gap       = 6;
constexpr int close_container   = 22;  // close IconButton edge
constexpr int close_glyph_px    = 16;
constexpr int add_container     = 30;  // trailing "+" IconButton edge
constexpr int add_glyph_px      = 20;
constexpr int group_indicator_h = 4;
constexpr int active_indicator_h = 3;
constexpr int active_indicator_inset = 12;
constexpr int drag_threshold    = 4;   // px of travel before a press becomes a drag

// Display title from an on-disk project path: filename without directory or
// extension. Used by LoadFromConfig; MainFrame later refreshes the live title.
wxString TitleFromPath(const std::string &path)
{
    if (path.empty())
        return _L("Untitled");
    const size_t s   = path.find_last_of("/\\");
    std::string base = s == std::string::npos ? path : path.substr(s + 1);
    const size_t dot = base.find_last_of('.');
    if (dot != std::string::npos && dot > 0)
        base = base.substr(0, dot);
    return wxString::FromUTF8(base);
}

} // namespace

// ---------------------------------------------------------------------------
// ProjectTabButton: one composite tab = [group chip?][title][dirty dot?][close]
// ---------------------------------------------------------------------------
class ProjectTabButton : public wxWindow
{
public:
    ProjectTabButton(ProjectTabBar *bar, wxWindow *parent);

    void SetTitle(const wxString &t);
    void SetActive(bool a);
    void SetDirty(bool d);
    void SetGrouped(const wxColour &color); // invalid colour => ungrouped
    void SetIndex(int i) { m_index = i; }
    int  Index() const { return m_index; }
    void Restyle(); // re-fetch fonts/colours (theme + DPI)

private:
    void OnPaint(wxPaintEvent &);
    void OnSize(wxSizeEvent &);
    void OnLeftDown(wxMouseEvent &);
    void OnMotion(wxMouseEvent &);
    void OnLeftUp(wxMouseEvent &);
    void OnCaptureLost(wxMouseCaptureLostEvent &);
    void OnEnter(wxMouseEvent &);
    void OnLeave(wxMouseEvent &);
    void DoLayout();
    void UpdateColors();

    ProjectTabBar *m_bar   = nullptr;
    Label         *m_title = nullptr;
    Button        *m_close = nullptr;

    int      m_index   = -1;
    bool     m_active  = false;
    bool     m_dirty   = false;
    bool     m_hover   = false;
    bool     m_grouped = false;
    wxColour m_group_color;

    // Press / drag tracking (screen-x based so a child under the cursor does not
    // confuse the delta).
    bool m_pressed        = false;
    bool m_dragging       = false;
    int  m_press_screen_x = 0;
};

ProjectTabButton::ProjectTabButton(ProjectTabBar *bar, wxWindow *parent)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
    , m_bar(bar)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));

    // Title in Label::Body_14 so it tracks Label::rebuild_fonts(); ellipsized
    // when the tab is crowded, and forwarding its clicks to the tab body so the
    // whole cell is one hit target.
    m_title = new Label(this, Label::Body_14, wxEmptyString,
                        wxST_ELLIPSIZE_END | wxST_NO_AUTORESIZE | LB_PROPAGATE_MOUSE_EVENT);

    // Borderless close IconButton (glyph), with a text fallback when the
    // Material Symbols face is unavailable so the affordance is never blank.
    m_close = new Button(this, "", "", 0, 0, wxID_ANY);
    if (MaterialIcon::available()) {
        m_close->SetIconButton(Button::IconShape::Circle, close_container);
        m_close->SetGlyph(MaterialIcon::Close, close_glyph_px);
    } else {
        m_close->SetLabel(wxString(wxUniChar(0x2715)));
        m_close->SetMinSize(wxSize(FromDIP(close_container), FromDIP(close_container)));
        m_close->SetCornerRadius(FromDIP(close_container) / 2.0);
    }
    m_close->SetToolTip(_L("Close tab"));
    m_close->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { m_bar->EmitClose(m_index); });

    Bind(wxEVT_PAINT, &ProjectTabButton::OnPaint, this);
    Bind(wxEVT_SIZE, &ProjectTabButton::OnSize, this);
    Bind(wxEVT_LEFT_DOWN, &ProjectTabButton::OnLeftDown, this);
    Bind(wxEVT_MOTION, &ProjectTabButton::OnMotion, this);
    Bind(wxEVT_LEFT_UP, &ProjectTabButton::OnLeftUp, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &ProjectTabButton::OnCaptureLost, this);
    Bind(wxEVT_ENTER_WINDOW, &ProjectTabButton::OnEnter, this);
    Bind(wxEVT_LEAVE_WINDOW, &ProjectTabButton::OnLeave, this);

    UpdateColors();
}

void ProjectTabButton::SetTitle(const wxString &t)
{
    if (m_title)
        m_title->SetLabel(t);
    SetToolTip(t);
    UpdateColors(); // re-apply fg/weight (SetLabel may reset the static text state)
    DoLayout();
}

void ProjectTabButton::SetActive(bool a)
{
    if (m_active == a)
        return;
    m_active = a;
    UpdateColors();
}

void ProjectTabButton::SetDirty(bool d)
{
    if (m_dirty == d)
        return;
    m_dirty = d;
    DoLayout();
    Refresh(false);
}

void ProjectTabButton::SetGrouped(const wxColour &color)
{
    m_grouped     = color.IsOk();
    m_group_color = color;
    DoLayout();
    Refresh(false);
}

void ProjectTabButton::Restyle()
{
    if (m_close)
        m_close->Rescale();
    UpdateColors();
    DoLayout();
}

void ProjectTabButton::UpdateColors()
{
    const wxColour surface = StateColor::semantic(MD3::Role::Surface);
    const wxColour hoverBg = StateColor::semantic(MD3::Role::SurfaceContainerLow);
    const wxColour fill    = m_hover ? hoverBg : surface;
    // Tab-bar chrome accent is pinned to Brand (above the per-workspace scheme),
    // exactly like ButtonsListCtrl.
    const wxColour active   = StateColor::semantic(MD3::Role::Primary, MD3::ColorScheme::Brand);
    const wxColour inactive = StateColor::semantic(MD3::Role::OnSurfaceVariant);

    SetBackgroundColour(fill);
    if (m_title) {
        m_title->SetBackgroundColour(fill);
        m_title->SetForegroundColour(m_active ? active : inactive);
        wxFont f = Label::Body_14;
        if (m_active) {
            f.SetWeight(wxFONTWEIGHT_SEMIBOLD);
            f.SetNumericWeight(600);
        }
        m_title->SetFont(f);
    }
    if (m_close) {
        // Keep the ghost close target's rest fill matched to the current tab
        // fill (so it reads transparent); hover still lifts to a wash.
        StateColor closeBg(
            std::pair{StateColor::semantic(MD3::Role::SurfaceContainerHigh), (int) StateColor::Hovered},
            std::pair{fill, (int) StateColor::Normal});
        m_close->SetBackgroundColor(closeBg);
    }
    Refresh(false);
}

void ProjectTabButton::DoLayout()
{
    const wxSize sz = GetClientSize();
    if (sz.x <= 0 || sz.y <= 0 || !m_close || !m_title)
        return;

    const int pad   = FromDIP(tab_h_padding);
    const int gap   = FromDIP(content_gap);
    const int chipW = m_grouped ? FromDIP(chip_size) : 0;
    const int chipG = m_grouped ? FromDIP(chip_gap) : 0;
    const int dotW  = m_dirty ? FromDIP(dot_size) : 0;
    const int dotG  = m_dirty ? FromDIP(content_gap) : 0;

    const wxSize closeSz = m_close->GetMinSize();
    const int    closeW  = closeSz.x > 0 ? closeSz.x : FromDIP(close_container);
    const int    closeH  = closeSz.y > 0 ? closeSz.y : FromDIP(close_container);
    const int    closeX  = sz.x - pad - closeW;
    m_close->SetSize(closeX, (sz.y - closeH) / 2, closeW, closeH);

    const int titleX = pad + chipW + chipG;
    const int titleR = closeX - gap - dotW - dotG;
    int       titleW = titleR - titleX;
    if (titleW < 0)
        titleW = 0;
    const int titleH = m_title->GetBestSize().y;
    m_title->SetSize(titleX, (sz.y - titleH) / 2, titleW, titleH);
}

void ProjectTabButton::OnSize(wxSizeEvent &e)
{
    DoLayout();
    e.Skip();
}

void ProjectTabButton::OnPaint(wxPaintEvent &)
{
    wxAutoBufferedPaintDC dc(this);
    const wxSize sz = GetClientSize();

    const wxColour surface = StateColor::semantic(MD3::Role::Surface);
    const wxColour hoverBg = StateColor::semantic(MD3::Role::SurfaceContainerLow);
    const wxColour fill    = m_hover ? hoverBg : surface;

    dc.SetBackground(wxBrush(fill));
    dc.Clear();

    // Group-colour chip at the leading edge.
    if (m_grouped && m_group_color.IsOk()) {
        const int c = FromDIP(chip_size);
        const int x = FromDIP(tab_h_padding);
        const int y = (sz.y - c) / 2;
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(m_group_color));
        dc.DrawRoundedRectangle(x, y, c, c, FromDIP(3));
    }

    // Dirty dot just left of the close button.
    if (m_dirty) {
        const wxSize closeSz = m_close ? m_close->GetMinSize() : wxSize(0, 0);
        const int    closeW  = closeSz.x > 0 ? closeSz.x : FromDIP(close_container);
        const int    d       = FromDIP(dot_size);
        const int    cx      = sz.x - FromDIP(tab_h_padding) - closeW - FromDIP(content_gap) - d / 2;
        const int    cy      = sz.y / 2;
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::OnSurfaceVariant)));
        dc.DrawCircle(cx, cy, d / 2);
    }
}

void ProjectTabButton::OnLeftDown(wxMouseEvent &e)
{
    m_pressed        = true;
    m_dragging       = false;
    m_press_screen_x = wxGetMousePosition().x;
    if (!HasCapture())
        CaptureMouse();
    // Do not Skip: the tab owns the gesture from here.
}

void ProjectTabButton::OnMotion(wxMouseEvent &e)
{
    if (m_pressed && !m_dragging) {
        if (std::abs(wxGetMousePosition().x - m_press_screen_x) > FromDIP(drag_threshold))
            m_dragging = true;
    }
    e.Skip();
}

void ProjectTabButton::OnLeftUp(wxMouseEvent &e)
{
    const bool was_pressed = m_pressed;
    const bool was_drag    = m_dragging;
    m_pressed  = false;
    m_dragging = false;
    if (HasCapture())
        ReleaseMouse();
    if (was_pressed && m_bar) {
        if (was_drag)
            m_bar->OnTabDragEnd(m_index, wxGetMousePosition().x);
        else
            m_bar->OnTabPressed(m_index);
    }
}

void ProjectTabButton::OnCaptureLost(wxMouseCaptureLostEvent &)
{
    m_pressed  = false;
    m_dragging = false;
}

void ProjectTabButton::OnEnter(wxMouseEvent &e)
{
    if (!m_hover) {
        m_hover = true;
        UpdateColors();
    }
    e.Skip();
}

void ProjectTabButton::OnLeave(wxMouseEvent &e)
{
    // Ignore leaves that merely crossed into our own child (title / close):
    // only clear hover when the pointer truly left the tab rect.
    const wxPoint p = ScreenToClient(wxGetMousePosition());
    if (wxRect(GetClientSize()).Contains(p)) {
        e.Skip();
        return;
    }
    if (m_hover) {
        m_hover = false;
        UpdateColors();
    }
    e.Skip();
}

// ---------------------------------------------------------------------------
// ProjectTabBar
// ---------------------------------------------------------------------------
ProjectTabBar::ProjectTabBar(wxWindow *parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxTAB_TRAVERSAL)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));

    const int h = FromDIP(bar_height);
    SetMinSize({-1, h});
    SetMaxSize({-1, h});

    m_root_sizer = new wxBoxSizer(wxHORIZONTAL);
    SetSizer(m_root_sizer);

    m_buttons_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_root_sizer->Add(m_buttons_sizer, 1, wxALIGN_TOP);

    m_actions_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_root_sizer->Add(m_actions_sizer, 0, wxALIGN_CENTER_VERTICAL);

    // Trailing "+" new-tab action.
    m_add_btn = new Button(this, "", "", 0, 0, wxID_ANY);
    if (MaterialIcon::available()) {
        m_add_btn->SetIconButton(Button::IconShape::Circle, add_container);
        m_add_btn->SetGlyph(MaterialIcon::Add, add_glyph_px);
    } else {
        m_add_btn->SetLabel("+");
        m_add_btn->SetMinSize(wxSize(FromDIP(add_container), FromDIP(add_container)));
        m_add_btn->SetCornerRadius(FromDIP(add_container) / 2.0);
    }
    m_add_btn->SetToolTip(_L("New project tab"));
    m_add_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        wxCommandEvent evt(EVT_PROJECT_TAB_NEW);
        evt.SetEventObject(this);
        wxPostEvent(this, evt);
    });
    m_actions_sizer->Add(m_add_btn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(content_gap));

    Bind(wxEVT_PAINT, &ProjectTabBar::OnPaint, this);
    Bind(wxEVT_SYS_COLOUR_CHANGED, [this](wxSysColourChangedEvent &e) {
        SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
        RestyleAll();
        e.Skip();
    });
}

ProjectTabBar::~ProjectTabBar() = default;

int ProjectTabBar::AddTab(const std::string &file_path, const wxString &title, bool activate)
{
    ProjectTab tab;
    tab.file_path = file_path;
    tab.title     = title;
    m_tabs.push_back(tab);

    ProjectTabButton *btn = new ProjectTabButton(this, this);
    btn->SetTitle(title);
    m_buttons.push_back(btn);

    const int index = int(m_tabs.size()) - 1;
    btn->SetIndex(index);

    Relayout();
    if (activate)
        SetActive(index);
    return index;
}

void ProjectTabBar::CloseTab(int i)
{
    if (i < 0 || i >= int(m_tabs.size()))
        return;

    ProjectTabButton *btn = m_buttons[i];
    m_buttons.erase(m_buttons.begin() + i);
    m_tabs.erase(m_tabs.begin() + i);
    m_buttons_sizer->Detach(btn);
    btn->Destroy();

    if (m_active == i)
        m_active = -1;
    else if (m_active > i)
        --m_active;
    if (m_active < 0 && !m_tabs.empty())
        m_active = std::min(i, int(m_tabs.size()) - 1);

    Relayout();
    if (m_active >= 0)
        SetActive(m_active);
}

void ProjectTabBar::SetActive(int i)
{
    m_active = (i >= 0 && i < int(m_buttons.size())) ? i : -1;
    for (int k = 0; k < int(m_buttons.size()); ++k)
        m_buttons[k]->SetActive(k == m_active);
    Refresh(false);
}

int  ProjectTabBar::GetActive() const { return m_active; }
int  ProjectTabBar::Count() const { return int(m_tabs.size()); }
ProjectTab &ProjectTabBar::TabAt(int i) { return m_tabs[i]; }

void ProjectTabBar::SetActiveDirty(bool dirty)
{
    if (m_active < 0 || m_active >= int(m_tabs.size()))
        return;
    m_tabs[m_active].dirty = dirty;
    m_buttons[m_active]->SetDirty(dirty);
}

void ProjectTabBar::SetActiveTitle(const wxString &t)
{
    if (m_active < 0 || m_active >= int(m_tabs.size()))
        return;
    m_tabs[m_active].title = t;
    m_buttons[m_active]->SetTitle(t);
}

void ProjectTabBar::Reorder(int from, int to)
{
    const int n = int(m_tabs.size());
    if (from < 0 || from >= n)
        return;
    if (to < 0)
        to = 0;
    if (to > n - 1)
        to = n - 1;
    if (from == to)
        return;

    ProjectTabButton *activeBtn =
        (m_active >= 0 && m_active < n) ? m_buttons[m_active] : nullptr;

    ProjectTab        tab = m_tabs[from];
    ProjectTabButton *btn = m_buttons[from];
    m_tabs.erase(m_tabs.begin() + from);
    m_buttons.erase(m_buttons.begin() + from);
    m_tabs.insert(m_tabs.begin() + to, tab);
    m_buttons.insert(m_buttons.begin() + to, btn);

    m_active = activeBtn ? IndexOfTab(activeBtn) : -1;
    Relayout();
    SetActive(m_active);
}

int ProjectTabBar::CreateGroup(const wxString &name, const wxColour &color)
{
    TabGroup g;
    g.id    = m_next_group_id++;
    g.name  = name;
    g.color = color;
    m_groups.push_back(g);
    return g.id;
}

void ProjectTabBar::AssignGroup(int tab_i, int group_id)
{
    if (tab_i < 0 || tab_i >= int(m_tabs.size()))
        return;
    m_tabs[tab_i].group_id = group_id;
    m_buttons[tab_i]->SetGrouped(GroupColor(group_id));
    Refresh(false); // repaint the contiguous-group underline
}

void ProjectTabBar::SaveToConfig()
{
    AppConfig *cfg = wxGetApp().app_config;
    if (!cfg)
        return;

    std::map<std::string, std::string> tabs;
    for (int k = 0; k < int(m_tabs.size()); ++k)
        tabs[std::to_string(k)] = m_tabs[k].file_path + "|" + std::to_string(m_tabs[k].group_id);
    cfg->set_section("project_tabs", tabs);

    std::map<std::string, std::string> groups;
    for (const auto &g : m_groups)
        groups[std::to_string(g.id)] =
            std::string(g.name.ToUTF8()) + "|" +
            std::string(g.color.GetAsString(wxC2S_HTML_SYNTAX).ToUTF8());
    cfg->set_section("tab_groups", groups);
}

void ProjectTabBar::LoadFromConfig()
{
    AppConfig *cfg = wxGetApp().app_config;
    if (!cfg)
        return;

    for (auto *btn : m_buttons) {
        m_buttons_sizer->Detach(btn);
        btn->Destroy();
    }
    m_buttons.clear();
    m_tabs.clear();
    m_groups.clear();
    m_active        = -1;
    m_next_group_id = 1;

    // Groups first so tab chips resolve their colour on creation.
    if (cfg->has_section("tab_groups")) {
        const auto &sec = cfg->get_section("tab_groups");
        for (const auto &kv : sec) {
            TabGroup g;
            try {
                g.id = std::stoi(kv.first);
            } catch (...) {
                continue;
            }
            const std::string &v   = kv.second;
            const size_t       bar = v.rfind('|');
            const std::string  name = bar == std::string::npos ? v : v.substr(0, bar);
            const std::string  hex  = bar == std::string::npos ? std::string() : v.substr(bar + 1);
            g.name  = wxString::FromUTF8(name);
            g.color = wxColour(wxString::FromUTF8(hex));
            m_groups.push_back(g);
            if (g.id >= m_next_group_id)
                m_next_group_id = g.id + 1;
        }
    }

    // Tabs, restored in their persisted integer-index order.
    if (cfg->has_section("project_tabs")) {
        const auto &sec = cfg->get_section("project_tabs");
        std::vector<std::pair<int, std::string>> ordered;
        for (const auto &kv : sec) {
            int idx;
            try {
                idx = std::stoi(kv.first);
            } catch (...) {
                continue;
            }
            ordered.emplace_back(idx, kv.second);
        }
        std::sort(ordered.begin(), ordered.end(),
                  [](const std::pair<int, std::string> &a, const std::pair<int, std::string> &b) {
                      return a.first < b.first;
                  });
        for (const auto &e : ordered) {
            const std::string &v   = e.second;
            const size_t       bar = v.rfind('|');
            const std::string  path = bar == std::string::npos ? v : v.substr(0, bar);
            int                group_id = -1;
            if (bar != std::string::npos) {
                try {
                    group_id = std::stoi(v.substr(bar + 1));
                } catch (...) {
                }
            }
            const int i = AddTab(path, TitleFromPath(path), /*activate*/ false);
            AssignGroup(i, group_id);
        }
    }

    if (!m_tabs.empty())
        SetActive(0); // visual default; MainFrame loads the project on activation
    Relayout();
}

void ProjectTabBar::Rescale()
{
    const int h = FromDIP(bar_height);
    SetMinSize({-1, h});
    SetMaxSize({-1, h});
    if (m_add_btn)
        m_add_btn->Rescale();
    for (auto *btn : m_buttons)
        btn->Restyle();
    Relayout();
}

// --- internal helpers ------------------------------------------------------

void ProjectTabBar::EmitSwitch(int index)
{
    wxCommandEvent evt(EVT_PROJECT_TAB_SWITCH);
    evt.SetEventObject(this);
    evt.SetInt(index);
    wxPostEvent(this, evt);
}

void ProjectTabBar::EmitClose(int index)
{
    wxCommandEvent evt(EVT_PROJECT_TAB_CLOSE);
    evt.SetEventObject(this);
    evt.SetInt(index);
    wxPostEvent(this, evt);
}

void ProjectTabBar::OnTabPressed(int index) { EmitSwitch(index); }

void ProjectTabBar::OnTabDragEnd(int from, int screen_x)
{
    const int n = int(m_buttons.size());
    if (n <= 1)
        return;

    const int localx = ScreenToClient(wxPoint(screen_x, 0)).x;
    int       slot   = 0; // insertion slot in [0..n]
    for (int k = 0; k < n; ++k) {
        const wxRect r = m_buttons[k]->GetRect();
        if (localx > r.x + r.width / 2)
            ++slot;
    }
    // Convert the slot to a final index for the dragged element: its own current
    // cell contributes one slot to its left, so shift down when moving right.
    int target = (slot > from) ? slot - 1 : slot;
    if (target < 0)
        target = 0;
    if (target > n - 1)
        target = n - 1;
    Reorder(from, target);
}

void ProjectTabBar::OnPaint(wxPaintEvent &)
{
    wxAutoBufferedPaintDC dc(this);
    const wxSize sz = GetClientSize();

    dc.SetBackground(wxBrush(StateColor::semantic(MD3::Role::Surface)));
    dc.Clear();

    // Quiet boundary between the tab strip and the workspace below.
    const int divider = std::max(1, FromDIP(1));
    dc.SetPen(wxPen(StateColor::semantic(MD3::Role::OutlineVariant), divider));
    dc.DrawLine(0, sz.y - divider, sz.x, sz.y - divider);

    if (m_buttons.empty())
        return;

    // Contiguous same-group underline (the group indicator). Drawn at double
    // height with a full radius, flush to the bottom, so the clipped-away lower
    // half leaves crisp square bottom corners and rounded visible top corners.
    const int gh = FromDIP(group_indicator_h);
    dc.SetPen(*wxTRANSPARENT_PEN);
    int run_start = 0;
    while (run_start < int(m_tabs.size())) {
        const int gid     = m_tabs[run_start].group_id;
        int       run_end = run_start;
        while (run_end + 1 < int(m_tabs.size()) && m_tabs[run_end + 1].group_id == gid)
            ++run_end;
        if (gid >= 0) {
            const wxColour c = GroupColor(gid);
            if (c.IsOk()) {
                const wxRect a = m_buttons[run_start]->GetRect();
                const wxRect b = m_buttons[run_end]->GetRect();
                const int    x = a.x;
                const int    w = (b.x + b.width) - a.x;
                dc.SetBrush(wxBrush(c));
                dc.DrawRoundedRectangle(x, sz.y - gh, std::max(1, w), gh * 2, gh);
            }
        }
        run_start = run_end + 1;
    }

    // Active-tab underline (chrome accent pinned to Brand), over the group run.
    if (m_active >= 0 && m_active < int(m_buttons.size())) {
        const wxRect r     = m_buttons[m_active]->GetRect();
        const int    inset = FromDIP(active_indicator_inset);
        const int    ih    = FromDIP(active_indicator_h);
        const int    w     = std::max(1, r.width - 2 * inset);
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::Primary, MD3::ColorScheme::Brand)));
        dc.DrawRoundedRectangle(r.x + inset, sz.y - ih, w, ih * 2, ih);
    }
}

void ProjectTabBar::Relayout()
{
    m_buttons_sizer->Clear(false); // detach only; the windows are reused
    const int tabH = FromDIP(bar_height - bar_bottom_space);
    const int minW = FromDIP(tab_min_width);
    const int maxW = FromDIP(tab_max_width);
    for (int k = 0; k < int(m_buttons.size()); ++k) {
        ProjectTabButton *btn = m_buttons[k];
        btn->SetIndex(k);
        btn->SetMinSize({minW, tabH});
        btn->SetMaxSize({maxW, tabH});
        m_buttons_sizer->Add(btn, wxSizerFlags(1).Align(wxALIGN_TOP));
    }
    m_root_sizer->Layout();
    Refresh(false);
}

void ProjectTabBar::RestyleAll()
{
    for (auto *btn : m_buttons)
        btn->Restyle();
    Refresh(false);
}

wxColour ProjectTabBar::GroupColor(int group_id) const
{
    if (group_id < 0)
        return wxColour();
    for (const auto &g : m_groups)
        if (g.id == group_id)
            return g.color;
    return wxColour();
}

int ProjectTabBar::IndexOfTab(const ProjectTabButton *btn) const
{
    for (int k = 0; k < int(m_buttons.size()); ++k)
        if (m_buttons[k] == btn)
            return k;
    return -1;
}

}} // namespace Slic3r::GUI
