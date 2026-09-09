#include "TabStrip.hpp"

#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "Button.hpp"
#include "Label.hpp"
#include "MD3ColorPicker.hpp"
#include "MD3Menu.hpp"
#include "MD3Tokens.hpp"
#include "MaterialIcon.hpp"
#include "SearchField.hpp"
#include "StateColor.hpp"
#include "TabStripDialogs.hpp"

#include "libslic3r/AppConfig.hpp"

#include <algorithm>
#include <cstdlib>

#include <wx/control.h>
#include <wx/dcbuffer.h>
#include <wx/dcgraph.h>
#include <wx/menu.h>
#include <wx/settings.h>
#include <wx/utils.h>

namespace Slic3r { namespace GUI {

wxDEFINE_EVENT(EVT_TABSTRIP_ACTIVATE, wxCommandEvent);
wxDEFINE_EVENT(EVT_TABSTRIP_CLOSE_REQUEST, wxCommandEvent);
wxDEFINE_EVENT(EVT_TABSTRIP_NEW, wxCommandEvent);
wxDEFINE_EVENT(EVT_TABSTRIP_CHANGED, wxCommandEvent);
wxDEFINE_EVENT(EVT_TABSTRIP_DOCK_CHANGED, wxCommandEvent);

using MD3::Tabs::DockEdge;

namespace {

// Logical / design px; FromDIP() applies the monitor scale.
constexpr int tab_h_padding        = 12;
constexpr int rail_h_padding       = 10;
constexpr int rail_v_padding       = 10;
constexpr int item_gap_vertical    = 2;
constexpr int item_gap_horizontal  = 0;
constexpr int chip_size            = 10;
constexpr int chip_gap             = 6;
constexpr int dot_size             = 6;
constexpr int content_gap          = 6;
constexpr int close_container      = 22;
constexpr int close_glyph_px       = 16;
constexpr int action_container     = 30;
constexpr int action_glyph_px      = 20;
constexpr int group_header_extent  = 28; // vertical: row height; horizontal: pill height
constexpr int group_indicator      = 4;
constexpr int active_indicator_h   = 3;
constexpr int active_indicator_inset = 12;
constexpr int drag_threshold       = 4;
constexpr int bar_bottom_space     = 6;

const char *kConfigSection = "tab_strips";

// Menu command ids, local to this widget.
enum MenuId
{
    ID_PIN = wxID_HIGHEST + 2300,
    ID_UNPIN,
    ID_MOVE_INTO_GROUP,
    ID_REMOVE_FROM_GROUP,
    ID_NEW_GROUP_FROM_TAB,
    ID_CLOSE_TAB,
    ID_CLOSE_CONTAINING,
    ID_CLOSE_NOT_CONTAINING,
    ID_EDIT_TAB_APPEARANCE,
    ID_EDIT_GROUP_APPEARANCE,
    ID_EDIT_STRIP_APPEARANCE,
    ID_SEARCH_STRIP,
    ID_SEARCH_GROUPS,
    ID_SEARCH_MASTER,
    ID_SEARCH_IN_GROUP,
    ID_SHOW_OVERFLOW,
    ID_DOCK_LEFT,
    ID_DOCK_RIGHT,
    ID_DOCK_TOP,
    ID_DOCK_BOTTOM,
    ID_GROUP_RENAME,
    ID_GROUP_COLOR,
    ID_GROUP_COLLAPSE,
    ID_GROUP_EXPAND,
    ID_GROUP_REMOVE,
    ID_NEW_TAB,
    ID_RESTORE_FIRST = wxID_HIGHEST + 2400, // + n: restore hidden tab n / show overflowed tab n
    ID_RESTORE_LAST  = wxID_HIGHEST + 2999,
};

// New-group palette (MD3 tonal accents), cycled by creation order.
const wxColour kGroupPalette[] = {
    wxColour(0x1C, 0xA7, 0x52), wxColour(0x1E, 0x88, 0xE5), wxColour(0xF4, 0x51, 0x1E), wxColour(0x8E, 0x24, 0xAA),
    wxColour(0xFB, 0x8C, 0x00), wxColour(0x00, 0x89, 0x7B), wxColour(0xE5, 0x39, 0x35), wxColour(0x3F, 0x51, 0xB5),
};

TabStrip::AppearanceHook &appearance_hook()
{
    static TabStrip::AppearanceHook hook;
    return hook;
}

std::vector<TabStrip *> &registry()
{
    static std::vector<TabStrip *> strips;
    return strips;
}

wxString shortcut(const wxString &label, const char *keys) { return label + "\t" + wxString(keys); }

} // namespace

// ---------------------------------------------------------------------------
// TabStripButton: one tab = [group chip?][pin?][title][dirty dot?][close]
// ---------------------------------------------------------------------------
class TabStripButton : public wxWindow
{
public:
    TabStripButton(TabStrip *strip, const std::string &id);

    const std::string &Id() const { return m_id; }
    void SetTitle(const wxString &t);
    void SetActive(bool a);
    void SetDirty(bool d);
    void SetPinned(bool p);
    void SetGrouped(const wxColour &color);
    void Restyle();
    // Preferred main-axis extent for the current orientation (device px).
    int  PreferredExtent(bool vertical);

    bool AcceptsFocus() const override { return false; }
    bool AcceptsFocusFromKeyboard() const override { return false; }

private:
    void OnPaint(wxPaintEvent &);
    void OnSize(wxSizeEvent &);
    void OnLeftDown(wxMouseEvent &);
    void OnMotion(wxMouseEvent &);
    void OnLeftUp(wxMouseEvent &);
    void OnRightUp(wxMouseEvent &);
    void OnCaptureLost(wxMouseCaptureLostEvent &);
    void OnEnter(wxMouseEvent &);
    void OnLeave(wxMouseEvent &);
    void DoLayout();
    void UpdateColors();
    bool Vertical() const { return m_strip->IsVertical(); }
    bool Focused() const;

    TabStrip *  m_strip = nullptr;
    std::string m_id;
    wxString    m_title;
    Button *    m_close = nullptr;

    bool     m_active  = false;
    bool     m_dirty   = false;
    bool     m_pinned  = false;
    bool     m_hover   = false;
    bool     m_grouped = false;
    wxColour m_group_color;

    bool    m_pressed  = false;
    bool    m_dragging = false;
    wxPoint m_press_screen;
};

TabStripButton::TabStripButton(TabStrip *strip, const std::string &id)
    : wxWindow(strip, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE), m_strip(strip), m_id(id)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif
    SetBackgroundStyle(wxBG_STYLE_PAINT);

    m_close = new Button(this, "", "", 0, 0, wxID_ANY);
    if (MaterialIcon::available()) {
        m_close->SetIconButton(Button::IconShape::Circle, close_container);
        m_close->SetGlyph(MaterialIcon::Close, close_glyph_px);
    } else {
        m_close->SetLabel(wxString(wxUniChar(0x2715)));
        m_close->SetMinSize(wxSize(FromDIP(close_container), FromDIP(close_container)));
        m_close->SetCornerRadius(FromDIP(close_container) / 2.0);
    }
    const wxString close_name = m_strip->GetOptions().close_mode == TabStrip::CloseMode::Hide ? _L("Hide tab from strip") : _L("Close tab");
    m_close->SetToolTip(close_name);
    m_close->SetName(close_name);
    m_close->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { m_strip->OnTabCloseClicked(m_id); });
    m_close->Show(m_strip->GetOptions().allow_close);

    Bind(wxEVT_PAINT, &TabStripButton::OnPaint, this);
    Bind(wxEVT_SIZE, &TabStripButton::OnSize, this);
    Bind(wxEVT_LEFT_DOWN, &TabStripButton::OnLeftDown, this);
    Bind(wxEVT_MOTION, &TabStripButton::OnMotion, this);
    Bind(wxEVT_LEFT_UP, &TabStripButton::OnLeftUp, this);
    Bind(wxEVT_RIGHT_UP, &TabStripButton::OnRightUp, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &TabStripButton::OnCaptureLost, this);
    Bind(wxEVT_ENTER_WINDOW, &TabStripButton::OnEnter, this);
    Bind(wxEVT_LEAVE_WINDOW, &TabStripButton::OnLeave, this);
    UpdateColors();
}

bool TabStripButton::Focused() const
{
    const int idx = m_strip->GetModel().index_of(m_id);
    return m_strip->HasFocus() && idx >= 0 && idx == m_strip->m_focus_index;
}

void TabStripButton::SetTitle(const wxString &t)
{
    m_title = t;
    SetToolTip(t);
    SetName(t);
    DoLayout();
    Refresh(false);
}

void TabStripButton::SetActive(bool a)
{
    if (m_active == a)
        return;
    m_active = a;
    UpdateColors();
}

void TabStripButton::SetDirty(bool d)
{
    if (m_dirty == d)
        return;
    m_dirty = d;
    DoLayout();
    Refresh(false);
}

void TabStripButton::SetPinned(bool p)
{
    if (m_pinned == p)
        return;
    m_pinned = p;
    // Pinned tabs keep no close affordance: they are protected from single and
    // bulk closes alike until unpinned.
    m_close->Show(m_strip->GetOptions().allow_close && !m_pinned);
    DoLayout();
    Refresh(false);
}

void TabStripButton::SetGrouped(const wxColour &color)
{
    m_grouped     = color.IsOk();
    m_group_color = color;
    DoLayout();
    Refresh(false);
}

void TabStripButton::Restyle()
{
    if (m_close)
        m_close->Rescale();
    UpdateColors();
    DoLayout();
}

int TabStripButton::PreferredExtent(bool vertical)
{
    if (vertical)
        return FromDIP(TabStrip::kTabHeight);
    wxClientDC dc(this);
    dc.SetFont(::Label::Body_14);
    wxCoord tw = 0, th = 0;
    dc.GetTextExtent(m_title, &tw, &th);
    int w = 2 * FromDIP(tab_h_padding) + tw + FromDIP(content_gap) + FromDIP(close_container);
    if (m_grouped)
        w += FromDIP(chip_size) + FromDIP(chip_gap);
    if (m_dirty)
        w += FromDIP(dot_size) + FromDIP(content_gap);
    return std::max(FromDIP(TabStrip::kTabMinWidth), std::min(FromDIP(TabStrip::kTabMaxWidth), w));
}

void TabStripButton::UpdateColors()
{
    const bool     vertical = Vertical();
    const wxColour base     = vertical ? StateColor::semantic(MD3::Role::SurfaceContainerLow)
                                       : StateColor::semantic(MD3::Role::Surface);
    SetBackgroundColour(base);
    if (m_close) {
        wxColour fill = base;
        if (vertical && m_active)
            fill = StateColor::semantic(MD3::Role::SecondaryContainer);
        else if (m_hover)
            fill = StateColor::semantic(vertical ? MD3::Role::SurfaceContainerHigh : MD3::Role::SurfaceContainerLow);
        StateColor closeBg(std::pair{StateColor::semantic(MD3::Role::SurfaceContainerHighest), (int) StateColor::Hovered},
                           std::pair{fill, (int) StateColor::Normal});
        m_close->SetBackgroundColor(closeBg);
    }
    Refresh(false);
}

void TabStripButton::DoLayout()
{
    const wxSize sz = GetClientSize();
    if (sz.x <= 0 || sz.y <= 0 || !m_close)
        return;
    const int    pad     = FromDIP(Vertical() ? 14 : tab_h_padding);
    const wxSize closeSz = m_close->GetMinSize();
    const int    closeW  = closeSz.x > 0 ? closeSz.x : FromDIP(close_container);
    const int    closeH  = closeSz.y > 0 ? closeSz.y : FromDIP(close_container);
    m_close->SetSize(sz.x - pad + FromDIP(4) - closeW, (sz.y - closeH) / 2, closeW, closeH);
}

void TabStripButton::OnSize(wxSizeEvent &e)
{
    DoLayout();
    e.Skip();
}

void TabStripButton::OnPaint(wxPaintEvent &)
{
    wxAutoBufferedPaintDC pdc(this);
    const wxSize          sz       = GetClientSize();
    const bool            vertical = Vertical();

    const wxColour base = vertical ? StateColor::semantic(MD3::Role::SurfaceContainerLow)
                                   : StateColor::semantic(MD3::Role::Surface);
    pdc.SetBackground(wxBrush(base));
    pdc.Clear();
#ifdef __WXMSW__
    wxGCDC dc(pdc);
#else
    wxDC &dc = pdc;
#endif

    wxColour fg;
    if (vertical) {
        // NavItem pill: selected -> SecondaryContainer, hover -> SurfaceContainerHigh.
        wxColour pill;
        bool     draw_pill = false;
        if (m_active) { pill = StateColor::semantic(MD3::Role::SecondaryContainer); draw_pill = true; }
        else if (m_hover) { pill = StateColor::semantic(MD3::Role::SurfaceContainerHigh); draw_pill = true; }
        if (draw_pill) {
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(pill));
            dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, sz.y / 2.0);
        }
        fg = m_active ? StateColor::semantic(MD3::Role::OnSecondaryContainer) : StateColor::semantic(MD3::Role::OnSurfaceVariant);
        dc.SetFont(m_active ? ::Label::Head_13 : ::Label::Body_13);
    } else {
        if (m_hover) {
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::SurfaceContainerLow)));
            dc.DrawRectangle(0, 0, sz.x, sz.y);
        }
        fg = m_active ? StateColor::semantic(MD3::Role::Primary, MD3::ColorScheme::Brand)
                      : StateColor::semantic(MD3::Role::OnSurfaceVariant);
        wxFont f = ::Label::Body_14;
        if (m_active) {
            f.SetWeight(wxFONTWEIGHT_SEMIBOLD);
            f.SetNumericWeight(600);
        }
        dc.SetFont(f);
    }

    const int pad = FromDIP(vertical ? 14 : tab_h_padding);
    const int gap = FromDIP(content_gap);
    int       x   = pad;

    // Group-colour chip.
    if (m_grouped && m_group_color.IsOk()) {
        const int c = FromDIP(chip_size);
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(m_group_color));
        dc.DrawRoundedRectangle(x, (sz.y - c) / 2, c, c, FromDIP(3));
        x += c + FromDIP(chip_gap);
    }
    // Pin marker: a small filled disc with a stem, in the label colour.
    if (m_pinned) {
        const int r  = FromDIP(3);
        const int cy = sz.y / 2 - FromDIP(2);
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(fg));
        dc.DrawCircle(x + r, cy, r);
        dc.SetPen(wxPen(fg, std::max(1, FromDIP(1))));
        dc.DrawLine(x + r, cy + r, x + r, cy + r + FromDIP(4));
        x += 2 * r + FromDIP(chip_gap);
    }

    // Trailing region: close (when shown) and dirty dot.
    int right = sz.x - pad;
    if (m_close && m_close->IsShown())
        right -= (m_close->GetMinSize().x > 0 ? m_close->GetMinSize().x : FromDIP(close_container)) - FromDIP(4) + gap;
    if (m_dirty) {
        const int d = FromDIP(dot_size);
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::OnSurfaceVariant)));
        dc.DrawCircle(right - d / 2, sz.y / 2, d / 2);
        right -= d + gap;
    }

    // Title, ellipsized to the remaining room.
    const int avail = std::max(0, right - x);
    wxCoord   tw = 0, th = 0;
    dc.GetTextExtent(m_title, &tw, &th);
    const wxString shown = tw > avail ? wxControl::Ellipsize(m_title, dc, wxELLIPSIZE_END, avail) : m_title;
    dc.SetTextForeground(fg);
    dc.DrawText(shown, x, (sz.y - th) / 2);

    // Keyboard focus ring (roving focus from the strip).
    if (Focused()) {
        const int inset = FromDIP(2);
        const int pw    = std::max(1, FromDIP(2));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(StateColor::semantic(MD3::Role::Primary), pw));
        const double radius = vertical ? (sz.y - 2 * inset) / 2.0 : FromDIP(6);
        dc.DrawRoundedRectangle(inset, inset, sz.x - 2 * inset, sz.y - 2 * inset, radius);
    }
}

void TabStripButton::OnLeftDown(wxMouseEvent &)
{
    m_pressed      = true;
    m_dragging     = false;
    m_press_screen = wxGetMousePosition();
    if (!HasCapture())
        CaptureMouse();
}

void TabStripButton::OnMotion(wxMouseEvent &e)
{
    if (m_pressed && !m_dragging) {
        const wxPoint p = wxGetMousePosition();
        const int     d = Vertical() ? std::abs(p.y - m_press_screen.y) : std::abs(p.x - m_press_screen.x);
        if (d > FromDIP(drag_threshold))
            m_dragging = true;
    }
    e.Skip();
}

void TabStripButton::OnLeftUp(wxMouseEvent &)
{
    const bool was_pressed = m_pressed;
    const bool was_drag    = m_dragging;
    m_pressed  = false;
    m_dragging = false;
    if (HasCapture())
        ReleaseMouse();
    if (!was_pressed)
        return;
    if (was_drag)
        m_strip->OnTabDragEnd(m_id, wxGetMousePosition());
    else
        m_strip->OnTabPressed(m_id);
}

void TabStripButton::OnRightUp(wxMouseEvent &e)
{
    m_strip->OnTabContext(m_id, ClientToScreen(e.GetPosition()), e.ShiftDown());
}

void TabStripButton::OnCaptureLost(wxMouseCaptureLostEvent &)
{
    m_pressed  = false;
    m_dragging = false;
}

void TabStripButton::OnEnter(wxMouseEvent &e)
{
    if (!m_hover) {
        m_hover = true;
        SetCursor(wxCURSOR_HAND);
        UpdateColors();
    }
    e.Skip();
}

void TabStripButton::OnLeave(wxMouseEvent &e)
{
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
// TabStripGroupHeader: [chip] name [chevron]; click toggles collapse.
// ---------------------------------------------------------------------------
class TabStripGroupHeader : public wxWindow
{
public:
    TabStripGroupHeader(TabStrip *strip, int group_id)
        : wxWindow(strip, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE), m_strip(strip), m_group_id(group_id)
    {
#ifdef __WINDOWS__
        SetDoubleBuffered(true);
#endif
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &TabStripGroupHeader::OnPaint, this);
        Bind(wxEVT_LEFT_UP, [this](wxMouseEvent &) { m_strip->OnGroupHeaderPressed(m_group_id); });
        Bind(wxEVT_RIGHT_UP, [this](wxMouseEvent &e) {
            if (e.ShiftDown())
                TabStrip::RequestAppearanceEditor(this, "group:" + m_strip->GetOptions().surface_key + ":" + std::to_string(m_group_id));
            else
                m_strip->ShowGroupMenu(m_group_id, ClientToScreen(e.GetPosition()));
        });
        Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent &e) { m_hover = true; SetCursor(wxCURSOR_HAND); Refresh(false); e.Skip(); });
        Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent &e) { m_hover = false; Refresh(false); e.Skip(); });
    }

    int  GroupId() const { return m_group_id; }
    void Update()
    {
        if (const MD3::Tabs::Group *g = m_strip->GetModel().group(m_group_id)) {
            const wxString name = g->name + (g->collapsed ? " (" + _L("collapsed") + ")" : wxString());
            SetName(wxString::Format(_L("Group %s"), name));
            SetToolTip(g->collapsed ? _L("Expand group") : _L("Collapse group"));
        }
        Refresh(false);
    }
    int PreferredExtent(bool vertical)
    {
        if (vertical)
            return FromDIP(group_header_extent);
        const MD3::Tabs::Group *g = m_strip->GetModel().group(m_group_id);
        wxClientDC              dc(this);
        dc.SetFont(::Label::Head_12);
        wxCoord tw = 0, th = 0;
        dc.GetTextExtent(g ? g->name : wxString(), &tw, &th);
        return std::min(FromDIP(160), tw + FromDIP(chip_size) + FromDIP(chip_gap) + FromDIP(18) + 2 * FromDIP(10));
    }
    bool AcceptsFocus() const override { return false; }

private:
    void OnPaint(wxPaintEvent &)
    {
        wxAutoBufferedPaintDC pdc(this);
        const wxSize          sz       = GetClientSize();
        const bool            vertical = m_strip->IsVertical();
        const wxColour base = vertical ? StateColor::semantic(MD3::Role::SurfaceContainerLow) : StateColor::semantic(MD3::Role::Surface);
        pdc.SetBackground(wxBrush(base));
        pdc.Clear();
#ifdef __WXMSW__
        wxGCDC dc(pdc);
#else
        wxDC &dc = pdc;
#endif
        const MD3::Tabs::Group *g = m_strip->GetModel().group(m_group_id);
        if (!g)
            return;
        if (m_hover) {
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::SurfaceContainerHigh)));
            dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, FromDIP(8));
        }
        const int pad = FromDIP(10);
        int       x   = pad;
        const int c   = FromDIP(chip_size);
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(g->color.IsOk() ? g->color : StateColor::semantic(MD3::Role::Outline)));
        dc.DrawRoundedRectangle(x, (sz.y - c) / 2, c, c, FromDIP(3));
        x += c + FromDIP(chip_gap);

        const wxColour fg = StateColor::semantic(MD3::Role::OnSurfaceVariant);
        dc.SetFont(::Label::Head_12);
        dc.SetTextForeground(fg);
        const int chevron = FromDIP(18);
        wxCoord   tw = 0, th = 0;
        dc.GetTextExtent(g->name, &tw, &th);
        const int avail = std::max(0, sz.x - x - pad - chevron - FromDIP(4));
        const wxString shown = tw > avail ? wxControl::Ellipsize(g->name, dc, wxELLIPSIZE_END, avail) : g->name;
        dc.DrawText(shown, x, (sz.y - th) / 2);
        if (MaterialIcon::available())
            MaterialIcon::drawCentered(dc, g->collapsed ? MaterialIcon::ChevronRight : MaterialIcon::ExpandMore, chevron, fg,
                                       wxRect(sz.x - pad - chevron, 0, chevron, sz.y));
    }

    TabStrip *m_strip = nullptr;
    int       m_group_id = -1;
    bool      m_hover = false;
};

// ---------------------------------------------------------------------------
// Accessibility: the strip is a tablist whose children are the displayed tabs.
// ---------------------------------------------------------------------------
#if wxUSE_ACCESSIBILITY
class TabStripAccessible final : public wxWindowAccessible
{
public:
    explicit TabStripAccessible(TabStrip *strip) : wxWindowAccessible(strip), m_strip(strip) {}

    wxAccStatus GetChildCount(int *count) override
    {
        if (!count) return wxACC_FAIL;
        *count = int(m_strip->GetModel().displayed_indices().size());
        return wxACC_OK;
    }
    wxAccStatus GetChild(int child_id, wxAccessible **child) override
    {
        if (!child) return wxACC_FAIL;
        if (child_id == wxACC_SELF) { *child = this; return wxACC_OK; }
        if (model_index(child_id) < 0) return wxACC_FAIL;
        *child = nullptr;
        return wxACC_OK;
    }
    wxAccStatus GetRole(int child_id, wxAccRole *role) override
    {
        if (!role) return wxACC_FAIL;
        *role = child_id == wxACC_SELF ? wxROLE_SYSTEM_PAGETABLIST : wxROLE_SYSTEM_PAGETAB;
        return wxACC_OK;
    }
    wxAccStatus GetName(int child_id, wxString *name) override
    {
        if (!name) return wxACC_FAIL;
        if (child_id == wxACC_SELF) { *name = m_strip->GetOptions().strip_name; return wxACC_OK; }
        const int i = model_index(child_id);
        if (i < 0) return wxACC_FAIL;
        const MD3::Tabs::Tab &t = m_strip->GetModel().at(i);
        *name = t.title;
        if (t.pinned) *name << ", " << _L("pinned");
        if (const MD3::Tabs::Group *g = m_strip->GetModel().group(t.group_id))
            *name << ", " << wxString::Format(_L("group %s"), g->name);
        return wxACC_OK;
    }
    // Orientation semantics (aria-orientation): says which arrow keys move.
    wxAccStatus GetDescription(int child_id, wxString *description) override
    {
        if (!description) return wxACC_FAIL;
        if (child_id != wxACC_SELF) return wxACC_NOT_IMPLEMENTED;
        *description = m_strip->IsVertical() ? _L("Vertical tab list. Use Up and Down to move between tabs.")
                                             : _L("Horizontal tab list. Use Left and Right to move between tabs.");
        return wxACC_OK;
    }
    wxAccStatus GetState(int child_id, long *state) override
    {
        if (!state) return wxACC_FAIL;
        *state = 0;
        if (child_id == wxACC_SELF) {
            *state |= wxACC_STATE_SYSTEM_FOCUSABLE;
            if (m_strip->HasFocus()) *state |= wxACC_STATE_SYSTEM_FOCUSED;
            return wxACC_OK;
        }
        const int i = model_index(child_id);
        if (i < 0) return wxACC_FAIL;
        *state |= wxACC_STATE_SYSTEM_FOCUSABLE | wxACC_STATE_SYSTEM_SELECTABLE;
        if (i == m_strip->GetModel().active_index()) *state |= wxACC_STATE_SYSTEM_SELECTED;
        if (m_strip->HasFocus() && i == m_strip->m_focus_index) *state |= wxACC_STATE_SYSTEM_FOCUSED;
        return wxACC_OK;
    }
    wxAccStatus GetLocation(wxRect &rect, int element_id) override
    {
        if (element_id == wxACC_SELF) return wxWindowAccessible::GetLocation(rect, element_id);
        const int i = model_index(element_id);
        if (i < 0 || i >= int(m_strip->m_buttons.size())) return wxACC_FAIL;
        rect = m_strip->m_buttons[i]->GetScreenRect();
        return wxACC_OK;
    }
    wxAccStatus GetDefaultAction(int child_id, wxString *action) override
    {
        if (!action) return wxACC_FAIL;
        if (child_id == wxACC_SELF) return wxACC_NOT_IMPLEMENTED;
        *action = _L("Switch");
        return wxACC_OK;
    }
    wxAccStatus DoDefaultAction(int child_id) override
    {
        const int i = model_index(child_id);
        if (i < 0) return wxACC_FAIL;
        m_strip->RequestActivate(m_strip->GetModel().at(i).id);
        return wxACC_OK;
    }
    wxAccStatus GetFocus(int *child_id, wxAccessible **child) override
    {
        if (!child_id || !child) return wxACC_FAIL;
        *child = nullptr;
        *child_id = wxACC_SELF;
        const std::vector<int> disp = m_strip->GetModel().displayed_indices();
        for (int k = 0; k < int(disp.size()); ++k)
            if (disp[k] == m_strip->m_focus_index) { *child_id = k + 1; break; }
        return wxACC_OK;
    }

private:
    int model_index(int child_id) const
    {
        const std::vector<int> disp = m_strip->GetModel().displayed_indices();
        if (child_id < 1 || child_id > int(disp.size())) return -1;
        return disp[child_id - 1];
    }
    TabStrip *m_strip;
};
#endif

// ---------------------------------------------------------------------------
// TabStrip
// ---------------------------------------------------------------------------
TabStrip::TabStrip(wxWindow *parent, const Options &options)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxWANTS_CHARS), m_options(options)
{
    m_model.set_edge(options.default_edge);
    SetName(options.strip_name.IsEmpty() ? _L("Tabs") : options.strip_name);
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif
    SetBackgroundStyle(wxBG_STYLE_PAINT);

    auto make_action = [this](uint32_t glyph, const wxString &fallback, const wxString &name) {
        Button *b = new Button(this, "", "", 0, 0, wxID_ANY);
        if (MaterialIcon::available()) {
            b->SetIconButton(Button::IconShape::Circle, action_container);
            b->SetGlyph(glyph, action_glyph_px);
        } else {
            b->SetLabel(fallback);
            b->SetMinSize(wxSize(FromDIP(action_container), FromDIP(action_container)));
            b->SetCornerRadius(FromDIP(action_container) / 2.0);
        }
        b->SetToolTip(name);
        b->SetName(name);
        return b;
    };
    m_overflow_btn = make_action(MaterialIcon::MoreHoriz, wxString::FromUTF8("\xE2\x80\xA6"), _L("More tabs"));
    m_overflow_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { OpenOverflowMenu(); });
    m_overflow_btn->Hide();
    m_search_btn = make_action(MaterialIcon::Search, "?", _L("Search tabs"));
    m_search_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { OpenStripSearch(); });
    if (options.show_new_button) {
        m_add_btn = make_action(MaterialIcon::Add, "+", _L("New tab"));
        m_add_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            wxCommandEvent evt(EVT_TABSTRIP_NEW);
            evt.SetEventObject(this);
            wxPostEvent(this, evt);
        });
    }

    Bind(wxEVT_PAINT, &TabStrip::OnPaint, this);
    Bind(wxEVT_SIZE, &TabStrip::OnSize, this);
    Bind(wxEVT_KEY_DOWN, &TabStrip::OnKeyDown, this);
    Bind(wxEVT_SET_FOCUS, &TabStrip::OnFocus, this);
    Bind(wxEVT_KILL_FOCUS, &TabStrip::OnFocus, this);
    Bind(wxEVT_CONTEXT_MENU, &TabStrip::OnContextMenu, this);
    Bind(wxEVT_RIGHT_UP, &TabStrip::OnRightUp, this);
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) { SetFocus(); e.Skip(); });
    Bind(wxEVT_SYS_COLOUR_CHANGED, [this](wxSysColourChangedEvent &e) { RestyleAll(); e.Skip(); });

#if wxUSE_ACCESSIBILITY
    SetAccessible(new TabStripAccessible(this));
#endif
    registry().push_back(this);
    Relayout();
}

TabStrip::~TabStrip()
{
    auto &r = registry();
    r.erase(std::remove(r.begin(), r.end(), this), r.end());
}

// --- statics -----------------------------------------------------------------
const std::vector<TabStrip *> &TabStrip::Registry() { return registry(); }
void TabStrip::SetAppearanceEditorHook(AppearanceHook hook) { appearance_hook() = std::move(hook); }
bool TabStrip::HasAppearanceEditorHook() { return bool(appearance_hook()); }
void TabStrip::RequestAppearanceEditor(wxWindow *anchor, const std::string &element_id)
{
    if (appearance_hook())
        appearance_hook()(anchor, element_id);
}

// --- tabs --------------------------------------------------------------------
int TabStrip::AddTab(const std::string &id, const wxString &title, const std::string &payload, bool activate)
{
    MD3::Tabs::Tab t;
    t.id      = id;
    t.title   = title;
    t.payload = payload;
    const int index = m_model.add(t);
    SyncButtons();
    if (activate)
        m_model.set_active(id);
    Changed();
    return index;
}

void TabStrip::RemoveTab(const std::string &id)
{
    if (!m_model.remove(id))
        return;
    SyncButtons();
    Changed();
}

void TabStrip::SetTitle(const std::string &id, const wxString &title)
{
    if (!m_model.set_title(id, title))
        return;
    const int i = m_model.index_of(id);
    if (i >= 0 && i < int(m_buttons.size()))
        m_buttons[i]->SetTitle(title);
    Relayout();
    SaveLayout();
}

void TabStrip::SetDirty(const std::string &id, bool dirty)
{
    if (!m_model.set_dirty(id, dirty))
        return;
    const int i = m_model.index_of(id);
    if (i >= 0 && i < int(m_buttons.size()))
        m_buttons[i]->SetDirty(dirty);
    Relayout();
}

void TabStrip::Activate(const std::string &id, bool emit)
{
    const MD3::Tabs::Tab *t = m_model.find(id);
    if (!t)
        return;
    bool changed = false;
    if (t->hidden) {
        m_model.set_hidden(id, false);
        changed = true;
    }
    if (!m_model.is_displayed(*t))
        m_model.reveal(id); // collapsed group: show it without expanding
    m_model.set_active(id);
    m_focus_index = m_model.index_of(id);
    for (int k = 0; k < int(m_buttons.size()); ++k)
        m_buttons[k]->SetActive(k == m_focus_index);
    if (changed)
        Changed();
    else {
        Relayout();
        SaveLayout();
    }
    if (emit) {
        wxCommandEvent evt(EVT_TABSTRIP_ACTIVATE);
        evt.SetEventObject(this);
        evt.SetString(wxString::FromUTF8(id));
        evt.SetInt(m_model.index_of(id));
        wxPostEvent(this, evt);
    }
}

void TabStrip::SetPinned(const std::string &id, bool pinned)
{
    if (!m_model.set_pinned(id, pinned))
        return;
    SyncButtons();
    Changed();
}

void TabStrip::SetHidden(const std::string &id, bool hidden)
{
    if (!m_model.set_hidden(id, hidden))
        return;
    if (!hidden)
        m_model.set_active(id);
    SyncButtons();
    Changed();
}

void TabStrip::MoveTab(int from, int to)
{
    if (!m_model.move(from, to))
        return;
    SyncButtons();
    Changed();
}

// --- groups ------------------------------------------------------------------
int TabStrip::CreateGroup(const wxString &name, const wxColour &color)
{
    const int id = m_model.create_group(name, color);
    Changed();
    return id;
}

void TabStrip::AssignGroup(const std::string &id, int group_id)
{
    if (!m_model.assign_group(id, group_id))
        return;
    SyncButtons();
    Changed();
}

void TabStrip::RenameGroup(int group_id, const wxString &name)
{
    if (m_model.rename_group(group_id, name))
        Changed();
}

void TabStrip::SetGroupColor(int group_id, const wxColour &color)
{
    if (m_model.set_group_color(group_id, color)) {
        SyncButtons();
        Changed();
    }
}

void TabStrip::SetGroupCollapsed(int group_id, bool collapsed)
{
    if (m_model.set_group_collapsed(group_id, collapsed)) {
        m_model.clear_reveal();
        Changed();
    }
}

void TabStrip::RemoveGroup(int group_id)
{
    if (m_model.remove_group(group_id)) {
        SyncButtons();
        Changed();
    }
}

// --- dock edge ---------------------------------------------------------------
void TabStrip::SetDockEdge(DockEdge edge)
{
    if (m_model.edge() == edge)
        return;
    m_model.set_edge(edge);
    if (m_overflow_btn && MaterialIcon::available())
        m_overflow_btn->SetGlyph(IsVertical() ? MaterialIcon::MoreVert : MaterialIcon::MoreHoriz, action_glyph_px);
    RestyleAll();
    Changed();
    wxCommandEvent evt(EVT_TABSTRIP_DOCK_CHANGED);
    evt.SetEventObject(this);
    evt.SetInt(int(edge));
    wxPostEvent(this, evt);
}

// --- persistence -------------------------------------------------------------
void TabStrip::LoadLayout()
{
    AppConfig *cfg = wxGetApp().app_config;
    if (!cfg || m_options.surface_key.empty())
        return;
    const std::string text = cfg->get(kConfigSection, m_options.surface_key);
    if (text.empty())
        return;
    MD3::Tabs::Model saved;
    if (!saved.from_json(text))
        return;
    m_loading = true;
    const DockEdge before = m_model.edge();
    m_model.adopt_layout(saved);
    m_loading = false;
    SyncButtons();
    if (m_model.edge() != before) {
        if (m_overflow_btn && MaterialIcon::available())
            m_overflow_btn->SetGlyph(IsVertical() ? MaterialIcon::MoreVert : MaterialIcon::MoreHoriz, action_glyph_px);
        RestyleAll();
        wxCommandEvent evt(EVT_TABSTRIP_DOCK_CHANGED);
        evt.SetEventObject(this);
        evt.SetInt(int(m_model.edge()));
        wxPostEvent(this, evt);
    }
    Relayout();
}

int TabStrip::LoadTabsFromLayout()
{
    AppConfig *cfg = wxGetApp().app_config;
    if (!cfg || m_options.surface_key.empty())
        return 0;
    const std::string text = cfg->get(kConfigSection, m_options.surface_key);
    MD3::Tabs::Model  saved;
    if (text.empty() || !saved.from_json(text))
        return 0;
    const DockEdge before = m_model.edge();
    m_model               = saved;
    SyncButtons();
    if (m_model.edge() != before) {
        if (m_overflow_btn && MaterialIcon::available())
            m_overflow_btn->SetGlyph(IsVertical() ? MaterialIcon::MoreVert : MaterialIcon::MoreHoriz, action_glyph_px);
        RestyleAll();
        wxCommandEvent evt(EVT_TABSTRIP_DOCK_CHANGED);
        evt.SetEventObject(this);
        evt.SetInt(int(m_model.edge()));
        wxPostEvent(this, evt);
    }
    Relayout();
    return m_model.size();
}

void TabStrip::SaveLayout()
{
    if (m_loading)
        return;
    AppConfig *cfg = wxGetApp().app_config;
    if (!cfg || m_options.surface_key.empty())
        return;
    cfg->set(kConfigSection, m_options.surface_key, m_model.to_json());
}

void TabStrip::Changed()
{
    SyncButtons();
    Relayout();
    SaveLayout();
    if (m_loading)
        return;
    wxCommandEvent evt(EVT_TABSTRIP_CHANGED);
    evt.SetEventObject(this);
    wxPostEvent(this, evt);
}

// --- discovery ---------------------------------------------------------------
MD3::Tabs::Matcher TabStrip::MakeMatcher(::SearchField *field) const { return MatcherForField(field); }

void TabStrip::RequestActivate(const std::string &id)
{
    const MD3::Tabs::Tab *t = m_model.find(id);
    if (!t)
        return;
    if (!m_options.host_confirms_activation) {
        Activate(id);
        return;
    }
    // Deferred: make the tab visible now so the host's switch lands on a
    // displayed tab, but leave the active id to the host.
    if (t->hidden)
        m_model.set_hidden(id, false);
    if (!m_model.is_displayed(*t))
        m_model.reveal(id);
    m_focus_index = m_model.index_of(id);
    SyncButtons();
    Relayout();
    wxCommandEvent evt(EVT_TABSTRIP_ACTIVATE);
    evt.SetEventObject(this);
    evt.SetString(wxString::FromUTF8(id));
    evt.SetInt(m_model.index_of(id));
    wxPostEvent(this, evt);
}

void TabStrip::ActivateHit(const MD3::Tabs::SearchHit &hit)
{
    if (hit.is_group) {
        // Reveal the group: expand nothing, just go to its first member.
        const std::vector<std::string> members = m_model.members(hit.group_id);
        if (!members.empty())
            RequestActivate(members.front());
        return;
    }
    RequestActivate(hit.tab_id);
    SetFocus();
}

void TabStrip::OpenStripSearch()
{
    TabSearchDialog dlg(wxGetTopLevelParent(this), m_search_btn ? static_cast<wxWindow *>(m_search_btn) : this,
                        TabSearchDialog::Scope::Strip, {this});
    if (dlg.ShowModal() == wxID_OK && dlg.ChosenStrip())
        dlg.ChosenStrip()->ActivateHit(dlg.ChosenHit());
}

void TabStrip::OpenGroupSearch(int group_id)
{
    TabSearchDialog dlg(wxGetTopLevelParent(this), this, TabSearchDialog::Scope::Group, {this}, group_id);
    if (dlg.ShowModal() == wxID_OK && dlg.ChosenStrip())
        dlg.ChosenStrip()->ActivateHit(dlg.ChosenHit());
}

void TabStrip::OpenGroupsSearch()
{
    TabSearchDialog dlg(wxGetTopLevelParent(this), this, TabSearchDialog::Scope::Groups, {this});
    if (dlg.ShowModal() == wxID_OK && dlg.ChosenStrip())
        dlg.ChosenStrip()->ActivateHit(dlg.ChosenHit());
}

void TabStrip::OpenMasterSearch(wxWindow *owner)
{
    std::vector<TabStrip *> strips = registry();
    TabSearchDialog dlg(owner ? wxGetTopLevelParent(owner) : nullptr, owner, TabSearchDialog::Scope::Master, strips);
    if (dlg.ShowModal() == wxID_OK && dlg.ChosenStrip())
        dlg.ChosenStrip()->ActivateHit(dlg.ChosenHit());
}

// --- bulk close --------------------------------------------------------------
void TabStrip::OpenBulkClose(bool not_containing)
{
    const wxString verb = m_options.close_mode == CloseMode::Hide ? _L("Hide tabs") : _L("Close tabs");
    BulkCloseDialog dlg(wxGetTopLevelParent(this), this, m_model, not_containing, verb);
    if (dlg.ShowModal() != wxID_OK)
        return;
    CloseMany(dlg.Ids());
}

void TabStrip::CloseMany(const std::vector<std::string> &ids)
{
    for (const std::string &id : ids)
        CloseOrHide(id);
}

void TabStrip::CloseOrHide(const std::string &id)
{
    const MD3::Tabs::Tab *t = m_model.find(id);
    if (!t)
        return;
    if (m_options.close_mode == CloseMode::Hide) {
        SetHidden(id, true);
        if (!m_model.active().empty() && m_model.active() != id) {
            wxCommandEvent evt(EVT_TABSTRIP_ACTIVATE);
            evt.SetEventObject(this);
            evt.SetString(wxString::FromUTF8(m_model.active()));
            evt.SetInt(m_model.active_index());
            wxPostEvent(this, evt);
        }
        return;
    }
    wxCommandEvent evt(EVT_TABSTRIP_CLOSE_REQUEST);
    evt.SetEventObject(this);
    evt.SetString(wxString::FromUTF8(id));
    evt.SetInt(m_model.index_of(id));
    wxPostEvent(this, evt);
}

// --- button callbacks --------------------------------------------------------
void TabStrip::OnTabPressed(const std::string &id)
{
    SetFocus();
    m_focus_index = m_model.index_of(id);
    if (id != m_model.active())
        RequestActivate(id);
    else
        Refresh(false);
}

void TabStrip::OnTabCloseClicked(const std::string &id) { CloseOrHide(id); }

void TabStrip::OnTabDragEnd(const std::string &id, const wxPoint &screen)
{
    const int from = m_model.index_of(id);
    if (from < 0)
        return;
    const wxPoint          local = ScreenToClient(screen);
    const std::vector<int> disp  = m_model.displayed_indices();
    // Insertion slot among displayed tabs, along the main axis.
    int slot = 0;
    for (int k = 0; k < int(disp.size()); ++k) {
        const wxRect r = m_buttons[disp[k]]->GetRect();
        const bool   after = IsVertical() ? local.y > r.y + r.height / 2 : local.x > r.x + r.width / 2;
        if (after)
            ++slot;
    }
    // Map the slot back to a model index.
    int target;
    if (slot >= int(disp.size()))
        target = m_model.size() - 1;
    else
        target = disp[slot];
    if (target > from)
        --target;
    MoveTab(from, target);
    m_focus_index = m_model.index_of(id);
}

void TabStrip::OnTabContext(const std::string &id, const wxPoint &screen, bool shift)
{
    SetFocus();
    m_focus_index = m_model.index_of(id);
    if (shift) {
        const int i = m_model.index_of(id);
        RequestAppearanceEditor(i >= 0 ? static_cast<wxWindow *>(m_buttons[i]) : this,
                                "tab:" + m_options.surface_key + ":" + id);
        return;
    }
    ShowTabMenu(id, screen);
}

void TabStrip::OnGroupHeaderPressed(int group_id)
{
    SetFocus();
    const MD3::Tabs::Group *g = m_model.group(group_id);
    if (g)
        SetGroupCollapsed(group_id, !g->collapsed);
}

// --- focus / keyboard --------------------------------------------------------
int TabStrip::FocusedModelIndex() const
{
    if (m_focus_index >= 0 && m_focus_index < m_model.size() && m_model.is_displayed(m_model.at(m_focus_index)))
        return m_focus_index;
    return m_model.active_index();
}

void TabStrip::SetFocusedModelIndex(int index)
{
    m_focus_index = index;
    Refresh(false);
    for (auto *b : m_buttons)
        b->Refresh(false);
#if wxUSE_ACCESSIBILITY
    const std::vector<int> disp = m_model.displayed_indices();
    for (int k = 0; k < int(disp.size()); ++k)
        if (disp[k] == index)
            wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_FOCUS, this, wxOBJID_CLIENT, k + 1);
#endif
}

void TabStrip::OnFocus(wxFocusEvent &e)
{
    if (e.GetEventType() == wxEVT_SET_FOCUS && FocusedModelIndex() >= 0)
        m_focus_index = FocusedModelIndex();
    Refresh(false);
    for (auto *b : m_buttons)
        b->Refresh(false);
    e.Skip();
}

void TabStrip::OnKeyDown(wxKeyEvent &e)
{
    const int  key   = e.GetKeyCode();
    const bool ctrl  = e.ControlDown();
    const bool shift = e.ShiftDown();
    const int  cur   = FocusedModelIndex();

    // Ctrl+Shift+Arrow along the axis: reorder the focused tab.
    if (ctrl && shift) {
        const int step = MD3::Tabs::arrow_step(m_model.edge(), key);
        if (step != 0 && cur >= 0) {
            const int target = MD3::Tabs::step_displayed(m_model, cur, step);
            if (target >= 0 && target != cur) {
                const std::string id = m_model.at(cur).id;
                MoveTab(cur, target);
                SetFocusedModelIndex(m_model.index_of(id));
            }
            return;
        }
        if (key == 'K') { OpenStripSearch(); return; }
        if (key == 'E' && cur >= 0) {
            RequestAppearanceEditor(m_buttons[cur], "tab:" + m_options.surface_key + ":" + m_model.at(cur).id);
            return;
        }
    }
    if (ctrl && !shift) {
        if (key == 'W' && cur >= 0 && m_options.allow_close && !m_model.at(cur).pinned) { CloseOrHide(m_model.at(cur).id); return; }
        if (key == 'P' && cur >= 0) { SetPinned(m_model.at(cur).id, !m_model.at(cur).pinned); return; }
    }
    const int step = MD3::Tabs::arrow_step(m_model.edge(), key);
    if (step != 0) {
        const int target = MD3::Tabs::step_displayed(m_model, cur, step);
        if (target >= 0)
            SetFocusedModelIndex(target);
        return;
    }
    if (key == WXK_HOME || key == WXK_END) {
        const std::vector<int> disp = m_model.displayed_indices();
        if (!disp.empty())
            SetFocusedModelIndex(key == WXK_HOME ? disp.front() : disp.back());
        return;
    }
    if (key == WXK_RETURN || key == WXK_NUMPAD_ENTER || key == WXK_SPACE) {
        if (cur >= 0)
            RequestActivate(m_model.at(cur).id);
        return;
    }
    if (key == WXK_DELETE && cur >= 0 && m_options.allow_close && !m_model.at(cur).pinned) {
        CloseOrHide(m_model.at(cur).id);
        return;
    }
    if (key == WXK_MENU || (shift && key == WXK_F10)) {
        if (cur >= 0) {
            const wxRect r = m_buttons[cur]->GetScreenRect();
            ShowTabMenu(m_model.at(cur).id, wxPoint(r.x + r.width / 2, r.y + r.height / 2));
        } else {
            const wxRect r = GetScreenRect();
            ShowStripMenu(wxPoint(r.x + FromDIP(8), r.y + FromDIP(8)));
        }
        return;
    }
    e.Skip();
}

void TabStrip::OnContextMenu(wxContextMenuEvent &e)
{
    wxPoint pos = e.GetPosition();
    if (pos == wxDefaultPosition) {
        const wxRect r = GetScreenRect();
        pos            = wxPoint(r.x + FromDIP(8), r.y + FromDIP(8));
    }
    ShowStripMenu(pos);
}

void TabStrip::OnRightUp(wxMouseEvent &e)
{
    if (e.ShiftDown()) {
        RequestAppearanceEditor(this, "tabstrip:" + m_options.surface_key);
        return;
    }
    ShowStripMenu(ClientToScreen(e.GetPosition()));
}

// --- menus -------------------------------------------------------------------
namespace {
void append_dock_submenu(wxMenu &menu, DockEdge current)
{
    wxMenu *dock = new wxMenu();
    dock->AppendRadioItem(ID_DOCK_LEFT, _L("Left"));
    dock->AppendRadioItem(ID_DOCK_RIGHT, _L("Right"));
    dock->AppendRadioItem(ID_DOCK_TOP, _L("Top"));
    dock->AppendRadioItem(ID_DOCK_BOTTOM, _L("Bottom"));
    dock->Check(ID_DOCK_LEFT, current == DockEdge::Left);
    dock->Check(ID_DOCK_RIGHT, current == DockEdge::Right);
    dock->Check(ID_DOCK_TOP, current == DockEdge::Top);
    dock->Check(ID_DOCK_BOTTOM, current == DockEdge::Bottom);
    menu.AppendSubMenu(dock, _L("Dock tab strip"));
}

void append_strip_items(wxMenu &menu, DockEdge current, bool has_new)
{
    menu.Append(ID_SEARCH_STRIP, shortcut(_L("Search tabs..."), "Ctrl+Shift+K"));
    menu.Append(ID_SEARCH_GROUPS, _L("Search tab groups..."));
    menu.Append(ID_SEARCH_MASTER, _L("Search all tabs..."));
    menu.AppendSeparator();
    menu.Append(ID_CLOSE_CONTAINING, _L("Close tabs containing text..."));
    menu.Append(ID_CLOSE_NOT_CONTAINING, _L("Close tabs not containing text..."));
    menu.Append(ID_SHOW_OVERFLOW, _L("Show more tabs..."));
    if (has_new)
        menu.Append(ID_NEW_TAB, _L("New tab"));
    menu.AppendSeparator();
    append_dock_submenu(menu, current);
    menu.Append(ID_EDIT_STRIP_APPEARANCE, _L("Edit tab strip appearance..."));
}
} // namespace

void TabStrip::ShowTabMenu(const std::string &id, const wxPoint &screen_pos)
{
    const MD3::Tabs::Tab *t = m_model.find(id);
    if (!t)
        return;
    const bool hide_mode = m_options.close_mode == CloseMode::Hide;
    wxMenu     menu;
    if (t->pinned)
        menu.Append(ID_UNPIN, shortcut(_L("Unpin tab"), "Ctrl+P"));
    else
        menu.Append(ID_PIN, shortcut(_L("Pin tab"), "Ctrl+P"));
    menu.Append(ID_MOVE_INTO_GROUP, _L("Move into group..."));
    if (t->group_id >= 0)
        menu.Append(ID_REMOVE_FROM_GROUP, _L("Remove from group"));
    menu.Append(ID_NEW_GROUP_FROM_TAB, _L("New group with this tab..."));
    if (t->group_id >= 0)
        menu.Append(ID_SEARCH_IN_GROUP, _L("Search tabs in this group..."));
    menu.AppendSeparator();
    if (m_options.allow_close) {
        wxMenuItem *close = menu.Append(ID_CLOSE_TAB, shortcut(hide_mode ? _L("Hide tab from strip") : _L("Close tab"), "Ctrl+W"));
        close->Enable(!t->pinned);
        if (t->pinned)
            close->SetHelp(_L("Pinned tabs are protected. Unpin the tab to close it."));
    }
    menu.AppendSeparator();
    menu.Append(ID_EDIT_TAB_APPEARANCE, shortcut(_L("Edit tab appearance..."), "Ctrl+Shift+E"));
    if (t->group_id >= 0)
        menu.Append(ID_EDIT_GROUP_APPEARANCE, _L("Edit group appearance..."));
    menu.AppendSeparator();
    append_strip_items(menu, m_model.edge(), m_options.show_new_button);

    const int   sel = MD3::PopupMenuSelection(this, menu, screen_pos);
    const int   i   = m_model.index_of(id);
    wxWindow *  anchor = i >= 0 && i < int(m_buttons.size()) ? static_cast<wxWindow *>(m_buttons[i]) : this;
    switch (sel) {
    case ID_PIN: SetPinned(id, true); break;
    case ID_UNPIN: SetPinned(id, false); break;
    case ID_MOVE_INTO_GROUP: MoveIntoGroupPicker(id); break;
    case ID_REMOVE_FROM_GROUP: AssignGroup(id, -1); break;
    case ID_NEW_GROUP_FROM_TAB: {
        wxString name = t->title;
        if (PromptGroupName(name, _L("New group"))) {
            const wxColour c   = kGroupPalette[m_model.groups().size() % (sizeof(kGroupPalette) / sizeof(kGroupPalette[0]))];
            const int      gid = CreateGroup(name, c);
            AssignGroup(id, gid);
        }
        break;
    }
    case ID_SEARCH_IN_GROUP: OpenGroupSearch(t->group_id); break;
    case ID_CLOSE_TAB: CloseOrHide(id); break;
    case ID_EDIT_TAB_APPEARANCE: RequestAppearanceEditor(anchor, "tab:" + m_options.surface_key + ":" + id); break;
    case ID_EDIT_GROUP_APPEARANCE:
        RequestAppearanceEditor(anchor, "group:" + m_options.surface_key + ":" + std::to_string(t->group_id));
        break;
    case ID_SEARCH_STRIP: OpenStripSearch(); break;
    case ID_SEARCH_GROUPS: OpenGroupsSearch(); break;
    case ID_SEARCH_MASTER: OpenMasterSearch(this); break;
    case ID_CLOSE_CONTAINING: OpenBulkClose(false); break;
    case ID_CLOSE_NOT_CONTAINING: OpenBulkClose(true); break;
    case ID_SHOW_OVERFLOW: OpenOverflowMenu(); break;
    case ID_NEW_TAB: {
        wxCommandEvent evt(EVT_TABSTRIP_NEW);
        evt.SetEventObject(this);
        wxPostEvent(this, evt);
        break;
    }
    case ID_DOCK_LEFT: SetDockEdge(DockEdge::Left); break;
    case ID_DOCK_RIGHT: SetDockEdge(DockEdge::Right); break;
    case ID_DOCK_TOP: SetDockEdge(DockEdge::Top); break;
    case ID_DOCK_BOTTOM: SetDockEdge(DockEdge::Bottom); break;
    case ID_EDIT_STRIP_APPEARANCE: RequestAppearanceEditor(this, "tabstrip:" + m_options.surface_key); break;
    default: break;
    }
}

void TabStrip::ShowGroupMenu(int group_id, const wxPoint &screen_pos)
{
    const MD3::Tabs::Group *g = m_model.group(group_id);
    if (!g)
        return;
    wxMenu menu;
    menu.Append(g->collapsed ? ID_GROUP_EXPAND : ID_GROUP_COLLAPSE, g->collapsed ? _L("Expand group") : _L("Collapse group"));
    menu.Append(ID_SEARCH_IN_GROUP, _L("Search tabs in this group..."));
    menu.AppendSeparator();
    menu.Append(ID_GROUP_RENAME, _L("Rename group..."));
    menu.Append(ID_GROUP_COLOR, _L("Change group colour..."));
    menu.Append(ID_GROUP_REMOVE, _L("Remove group (keep tabs)"));
    menu.AppendSeparator();
    menu.Append(ID_EDIT_GROUP_APPEARANCE, _L("Edit group appearance..."));
    menu.AppendSeparator();
    append_strip_items(menu, m_model.edge(), m_options.show_new_button);

    wxWindow *anchor = this;
    for (auto *h : m_headers)
        if (h->GroupId() == group_id)
            anchor = h;

    const int sel = MD3::PopupMenuSelection(this, menu, screen_pos);
    switch (sel) {
    case ID_GROUP_EXPAND: SetGroupCollapsed(group_id, false); break;
    case ID_GROUP_COLLAPSE: SetGroupCollapsed(group_id, true); break;
    case ID_SEARCH_IN_GROUP: OpenGroupSearch(group_id); break;
    case ID_GROUP_RENAME: {
        wxString name = g->name;
        if (PromptGroupName(name, _L("Rename group")))
            RenameGroup(group_id, name);
        break;
    }
    case ID_GROUP_COLOR: {
        wxColour c = g->color;
        if (PromptGroupColor(c))
            SetGroupColor(group_id, c);
        break;
    }
    case ID_GROUP_REMOVE: RemoveGroup(group_id); break;
    case ID_EDIT_GROUP_APPEARANCE:
        RequestAppearanceEditor(anchor, "group:" + m_options.surface_key + ":" + std::to_string(group_id));
        break;
    case ID_SEARCH_STRIP: OpenStripSearch(); break;
    case ID_SEARCH_GROUPS: OpenGroupsSearch(); break;
    case ID_SEARCH_MASTER: OpenMasterSearch(this); break;
    case ID_CLOSE_CONTAINING: OpenBulkClose(false); break;
    case ID_CLOSE_NOT_CONTAINING: OpenBulkClose(true); break;
    case ID_SHOW_OVERFLOW: OpenOverflowMenu(); break;
    case ID_DOCK_LEFT: SetDockEdge(DockEdge::Left); break;
    case ID_DOCK_RIGHT: SetDockEdge(DockEdge::Right); break;
    case ID_DOCK_TOP: SetDockEdge(DockEdge::Top); break;
    case ID_DOCK_BOTTOM: SetDockEdge(DockEdge::Bottom); break;
    case ID_EDIT_STRIP_APPEARANCE: RequestAppearanceEditor(this, "tabstrip:" + m_options.surface_key); break;
    default: break;
    }
}

void TabStrip::ShowStripMenu(const wxPoint &screen_pos)
{
    wxMenu menu;
    append_strip_items(menu, m_model.edge(), m_options.show_new_button);
    const int sel = MD3::PopupMenuSelection(this, menu, screen_pos);
    switch (sel) {
    case ID_SEARCH_STRIP: OpenStripSearch(); break;
    case ID_SEARCH_GROUPS: OpenGroupsSearch(); break;
    case ID_SEARCH_MASTER: OpenMasterSearch(this); break;
    case ID_CLOSE_CONTAINING: OpenBulkClose(false); break;
    case ID_CLOSE_NOT_CONTAINING: OpenBulkClose(true); break;
    case ID_SHOW_OVERFLOW: OpenOverflowMenu(); break;
    case ID_NEW_TAB: {
        wxCommandEvent evt(EVT_TABSTRIP_NEW);
        evt.SetEventObject(this);
        wxPostEvent(this, evt);
        break;
    }
    case ID_DOCK_LEFT: SetDockEdge(DockEdge::Left); break;
    case ID_DOCK_RIGHT: SetDockEdge(DockEdge::Right); break;
    case ID_DOCK_TOP: SetDockEdge(DockEdge::Top); break;
    case ID_DOCK_BOTTOM: SetDockEdge(DockEdge::Bottom); break;
    case ID_EDIT_STRIP_APPEARANCE: RequestAppearanceEditor(this, "tabstrip:" + m_options.surface_key); break;
    default: break;
    }
}

// The overflow surface: tabs that did not fit, then (hide-on-close surfaces)
// the tabs hidden from the strip so they can be restored.
void TabStrip::OpenOverflowMenu()
{
    wxMenu                   menu;
    std::vector<std::string> targets;
    auto add_target = [&](const MD3::Tabs::Tab &t, const wxString &suffix) {
        const int id = ID_RESTORE_FIRST + int(targets.size());
        if (id > ID_RESTORE_LAST)
            return;
        wxString label = t.title;
        if (t.pinned)
            label << "  [" << _L("pinned") << "]";
        if (const MD3::Tabs::Group *g = m_model.group(t.group_id))
            label << wxString::FromUTF8("  \xC2\xB7  ") << g->name;
        label << suffix;
        menu.Append(id, label);
        targets.push_back(t.id);
    };
    for (int i : m_overflowed)
        if (i >= 0 && i < m_model.size())
            add_target(m_model.at(i), wxString());
    const std::vector<int> hidden = m_model.hidden_indices();
    if (!hidden.empty()) {
        if (!targets.empty())
            menu.AppendSeparator();
        for (int i : hidden)
            add_target(m_model.at(i), "  (" + _L("hidden, click to restore") + ")");
    }
    if (targets.empty()) {
        wxMenuItem *none = menu.Append(wxID_ANY, _L("Every tab fits in the strip"));
        none->Enable(false);
    }
    wxRect anchor = m_overflow_btn && m_overflow_btn->IsShown() ? m_overflow_btn->GetScreenRect() : GetScreenRect();
    const int sel = MD3::PopupMenuSelection(this, menu, wxPoint(anchor.x, anchor.GetBottom() + 1));
    if (sel >= ID_RESTORE_FIRST && sel < ID_RESTORE_FIRST + int(targets.size()))
        RequestActivate(targets[sel - ID_RESTORE_FIRST]);
}

// --- dialogs -----------------------------------------------------------------
bool TabStrip::PromptGroupName(wxString &name, const wxString &title)
{
    GroupNameDialog dlg(wxGetTopLevelParent(this), title, name);
    if (dlg.ShowModal() != wxID_OK)
        return false;
    const wxString v = dlg.GetValue();
    if (v.IsEmpty())
        return false;
    name = v;
    return true;
}

bool TabStrip::PromptGroupColor(wxColour &color)
{
    MD3ColorPickerDialog dlg(wxGetTopLevelParent(this), color.IsOk() ? color : kGroupPalette[0]);
    if (dlg.ShowModal() != wxID_OK)
        return false;
    color = dlg.GetColour();
    return color.IsOk();
}

void TabStrip::MoveIntoGroupPicker(const std::string &tab_id)
{
    const int i      = m_model.index_of(tab_id);
    wxWindow *anchor = i >= 0 && i < int(m_buttons.size()) ? static_cast<wxWindow *>(m_buttons[i]) : this;
    MoveToGroupDialog dlg(wxGetTopLevelParent(this), anchor, m_model, tab_id);
    if (dlg.ShowModal() != wxID_OK)
        return;
    if (dlg.CreateRequested()) {
        wxString name;
        if (const MD3::Tabs::Tab *t = m_model.find(tab_id))
            name = t->title;
        if (PromptGroupName(name, _L("New group"))) {
            const wxColour c   = kGroupPalette[m_model.groups().size() % (sizeof(kGroupPalette) / sizeof(kGroupPalette[0]))];
            const int      gid = CreateGroup(name, c);
            AssignGroup(tab_id, gid);
        }
        return;
    }
    if (dlg.SelectedGroup() >= 0)
        AssignGroup(tab_id, dlg.SelectedGroup()); // a collapsed target stays collapsed
    SetFocus();
}

// --- layout / paint ----------------------------------------------------------
void TabStrip::SyncButtons()
{
    // Reuse buttons by id; create missing ones; destroy orphans.
    std::vector<TabStripButton *> next;
    next.reserve(m_model.size());
    for (const MD3::Tabs::Tab &t : m_model.tabs()) {
        TabStripButton *found = nullptr;
        for (auto *b : m_buttons)
            if (b && b->Id() == t.id) { found = b; break; }
        if (!found)
            found = new TabStripButton(this, t.id);
        next.push_back(found);
    }
    for (auto *b : m_buttons) {
        bool keep = false;
        for (auto *n : next)
            if (n == b) { keep = true; break; }
        if (!keep)
            b->Destroy();
    }
    m_buttons = std::move(next);
    for (int k = 0; k < m_model.size(); ++k) {
        const MD3::Tabs::Tab &t = m_model.at(k);
        TabStripButton *      b = m_buttons[k];
        b->SetTitle(t.title);
        b->SetDirty(t.dirty);
        b->SetPinned(t.pinned);
        b->SetActive(t.id == m_model.active());
        const MD3::Tabs::Group *g = m_model.group(t.group_id);
        b->SetGrouped(g ? g->color : wxColour());
    }
    // Group headers: one per group that still exists.
    std::vector<TabStripGroupHeader *> headers;
    for (const MD3::Tabs::Group &g : m_model.groups()) {
        TabStripGroupHeader *found = nullptr;
        for (auto *h : m_headers)
            if (h && h->GroupId() == g.id) { found = h; break; }
        if (!found)
            found = new TabStripGroupHeader(this, g.id);
        found->Update();
        headers.push_back(found);
    }
    for (auto *h : m_headers) {
        bool keep = false;
        for (auto *n : headers)
            if (n == h) { keep = true; break; }
        if (!keep)
            h->Destroy();
    }
    m_headers = std::move(headers);
    if (m_focus_index >= m_model.size())
        m_focus_index = m_model.active_index();
}

void TabStrip::Relayout()
{
    const bool vertical = IsVertical();
    if (vertical) {
        SetMinSize(wxSize(FromDIP(kRailWidth), -1));
        SetMaxSize(wxSize(FromDIP(kRailWidth), -1));
    } else {
        SetMinSize(wxSize(-1, FromDIP(kBarHeight)));
        SetMaxSize(wxSize(-1, FromDIP(kBarHeight)));
    }
    const wxSize sz = GetClientSize();
    if (sz.x <= 0 || sz.y <= 0)
        return;

    // Item list along the main axis: group headers precede each group run.
    struct Item
    {
        wxWindow *window = nullptr;
        int       model_index = -1; // -1 for a header
        int       extent = 0;
        bool      pinned = false;
    };
    std::vector<Item>      items;
    const std::vector<int> disp = m_model.displayed_indices();
    int                    prev_group = -2;
    for (int i : disp) {
        const MD3::Tabs::Tab &t = m_model.at(i);
        if (t.group_id >= 0 && t.group_id != prev_group) {
            for (auto *h : m_headers)
                if (h->GroupId() == t.group_id) {
                    Item hi;
                    hi.window = h;
                    hi.extent = h->PreferredExtent(vertical);
                    hi.pinned = t.pinned;
                    items.push_back(hi);
                }
        }
        prev_group = t.group_id;
        Item it;
        it.window      = m_buttons[i];
        it.model_index = i;
        it.extent      = m_buttons[i]->PreferredExtent(vertical);
        it.pinned      = t.pinned;
        items.push_back(it);
    }
    // Every non-displayed tab and unused header is hidden.
    for (int k = 0; k < int(m_buttons.size()); ++k)
        if (std::find(disp.begin(), disp.end(), k) == disp.end())
            m_buttons[k]->Hide();
    for (auto *h : m_headers) {
        bool used = false;
        for (const Item &it : items)
            if (it.window == h) used = true;
        if (!used)
            h->Hide();
    }

    // Trailing actions along the main axis.
    const int padMain  = FromDIP(vertical ? rail_v_padding : 0);
    const int padCross = FromDIP(vertical ? rail_h_padding : 0);
    const int gap      = FromDIP(vertical ? item_gap_vertical : item_gap_horizontal);
    const int actionPx = FromDIP(action_container);
    const int actionGap = FromDIP(content_gap);
    int       actions_extent = 0;
    std::vector<Button *> actions;
    actions.push_back(m_search_btn);
    if (m_add_btn)
        actions.push_back(m_add_btn);
    for (auto *a : actions)
        actions_extent += actionPx + actionGap;

    const int total    = vertical ? sz.y : sz.x;
    const int available = std::max(0, total - 2 * padMain - actions_extent - (vertical ? 0 : FromDIP(bar_bottom_space)));

    std::vector<int>  extents, pinned;
    std::vector<bool> pinned_b;
    for (const Item &it : items) {
        extents.push_back(it.extent);
        pinned_b.push_back(it.pinned);
    }
    const MD3::Tabs::OverflowResult ov = MD3::Tabs::compute_overflow(extents, pinned_b, available, actionPx + actionGap, gap);
    m_overflowed.clear();
    for (int pos : ov.hidden) {
        items[pos].window->Hide();
        if (items[pos].model_index >= 0)
            m_overflowed.push_back(items[pos].model_index);
    }

    // Place visible items.
    int cursor = padMain;
    const int crossExtent = vertical ? sz.x - 2 * padCross : sz.y - FromDIP(bar_bottom_space);
    for (int pos : ov.visible) {
        const Item &it = items[pos];
        if (vertical)
            it.window->SetSize(padCross, cursor, crossExtent, it.extent);
        else
            it.window->SetSize(cursor, 0, it.extent, crossExtent);
        it.window->Show();
        cursor += it.extent + gap;
    }
    // Overflow button right after the last visible item; trailing actions at
    // the far end.
    if (ov.needs_button) {
        if (vertical)
            m_overflow_btn->SetSize(padCross + (crossExtent - actionPx) / 2, cursor, actionPx, actionPx);
        else
            m_overflow_btn->SetSize(cursor, (crossExtent - actionPx) / 2, actionPx, actionPx);
        m_overflow_btn->Show();
        m_overflow_btn->SetToolTip(wxString::Format(_L("More tabs (%d hidden)"), int(ov.hidden.size())));
    } else {
        m_overflow_btn->Hide();
    }
    int end = total - padMain;
    for (auto it = actions.rbegin(); it != actions.rend(); ++it) {
        Button *a = *it;
        end -= actionPx;
        if (vertical)
            a->SetSize(padCross + (crossExtent - actionPx) / 2, end, actionPx, actionPx);
        else
            a->SetSize(end, (crossExtent - actionPx) / 2, actionPx, actionPx);
        a->Show();
        end -= actionGap;
    }
    Refresh(false);
}

void TabStrip::OnSize(wxSizeEvent &e)
{
    Relayout();
    e.Skip();
}

void TabStrip::OnPaint(wxPaintEvent &)
{
    wxAutoBufferedPaintDC dc(this);
    const wxSize          sz       = GetClientSize();
    const bool            vertical = IsVertical();
    dc.SetBackground(wxBrush(vertical ? StateColor::semantic(MD3::Role::SurfaceContainerLow) : StateColor::semantic(MD3::Role::Surface)));
    dc.Clear();

    // Divider on the edge that faces the content.
    const int divider = std::max(1, FromDIP(1));
    dc.SetPen(wxPen(StateColor::semantic(MD3::Role::OutlineVariant), divider));
    switch (m_model.edge()) {
    case DockEdge::Left: dc.DrawLine(sz.x - divider, 0, sz.x - divider, sz.y); break;
    case DockEdge::Right: dc.DrawLine(0, 0, 0, sz.y); break;
    case DockEdge::Top: dc.DrawLine(0, sz.y - divider, sz.x, sz.y - divider); break;
    case DockEdge::Bottom: dc.DrawLine(0, 0, sz.x, 0); break;
    }
    if (m_buttons.empty())
        return;

    const std::vector<int> disp = m_model.displayed_indices();
    dc.SetPen(*wxTRANSPARENT_PEN);
    const int gh = FromDIP(group_indicator);
    // Contiguous same-group runs: underline (horizontal) or side bar (vertical).
    int run_start = 0;
    while (run_start < int(disp.size())) {
        const int gid     = m_model.at(disp[run_start]).group_id;
        int       run_end = run_start;
        while (run_end + 1 < int(disp.size()) && m_model.at(disp[run_end + 1]).group_id == gid)
            ++run_end;
        const MD3::Tabs::Group *g = m_model.group(gid);
        if (g && g->color.IsOk() && m_buttons[disp[run_start]]->IsShown() && m_buttons[disp[run_end]]->IsShown()) {
            const wxRect a = m_buttons[disp[run_start]]->GetRect();
            const wxRect b = m_buttons[disp[run_end]]->GetRect();
            dc.SetBrush(wxBrush(g->color));
            if (vertical) {
                const int x = m_model.edge() == DockEdge::Left ? FromDIP(2) : sz.x - FromDIP(2) - gh;
                dc.DrawRoundedRectangle(x, a.y, gh, (b.y + b.height) - a.y, gh / 2.0);
            } else {
                dc.DrawRoundedRectangle(a.x, sz.y - gh, std::max(1, (b.x + b.width) - a.x), gh * 2, gh);
            }
        }
        run_start = run_end + 1;
    }
    // Horizontal active underline (the vertical strip paints its pill instead).
    if (!vertical) {
        const int ai = m_model.active_index();
        if (ai >= 0 && ai < int(m_buttons.size()) && m_buttons[ai]->IsShown()) {
            const wxRect r     = m_buttons[ai]->GetRect();
            const int    inset = FromDIP(active_indicator_inset);
            const int    ih    = FromDIP(active_indicator_h);
            dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::Primary, MD3::ColorScheme::Brand)));
            dc.DrawRoundedRectangle(r.x + inset, sz.y - ih, std::max(1, r.width - 2 * inset), ih * 2, ih);
        }
    }
}

void TabStrip::RestyleAll()
{
    for (auto *b : m_buttons)
        b->Restyle();
    for (auto *h : m_headers)
        h->Update();
    Relayout();
}

void TabStrip::Rescale()
{
    if (m_overflow_btn) m_overflow_btn->Rescale();
    if (m_search_btn) m_search_btn->Rescale();
    if (m_add_btn) m_add_btn->Rescale();
    RestyleAll();
}

}} // namespace Slic3r::GUI
