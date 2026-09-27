#include "AxisCtrlButton.hpp"
#include "../I18N.hpp"
#include "Label.hpp"
#include "MaterialIcon.hpp"
#include "StateColor.hpp"
#include "libslic3r/libslic3r.h"

#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#if wxUSE_ACCESSIBILITY
#include <wx/access.h>
#endif

namespace {

#if wxUSE_ACCESSIBILITY
class AxisCtrlButtonAccessible final : public wxWindowAccessible
{
public:
    explicit AxisCtrlButtonAccessible(AxisCtrlButton *button)
        : wxWindowAccessible(button), m_button(button)
    {
    }

    wxAccStatus GetChildCount(int *child_count) override
    {
        if (!child_count)
            return wxACC_NOT_IMPLEMENTED;
        *child_count = 5;
        return wxACC_OK;
    }

    wxAccStatus GetChild(int child_id, wxAccessible **child) override
    {
        if (!child || child_id < 1 || child_id > 5)
            return wxACC_NOT_IMPLEMENTED;
        *child = nullptr;
        return wxACC_OK;
    }

    wxAccStatus HitTest(const wxPoint& point, int *child_id, wxAccessible **child) override
    {
        if (!child_id || !child)
            return wxACC_NOT_IMPLEMENTED;
        *child_id = m_button->AccessibilityChildFromScreenPoint(point);
        *child = nullptr;
        return wxACC_OK;
    }

    wxAccStatus GetLocation(wxRect& location, int child_id) override
    {
        if (child_id < wxACC_SELF || child_id > 5)
            return wxACC_NOT_IMPLEMENTED;
        location = m_button->AccessibilityLocationForChild(child_id);
        return location.IsEmpty() ? wxACC_NOT_IMPLEMENTED : wxACC_OK;
    }

    wxAccStatus GetName(int child_id, wxString *name) override
    {
        if (!name || child_id < wxACC_SELF || child_id > 5)
            return wxACC_NOT_IMPLEMENTED;
        if (child_id == wxACC_SELF) {
            *name = m_button->GetName();
            if (name->IsEmpty() || *name == wxASCII_STR(wxPanelNameStr))
                *name = _L("XY axis controls");
        } else {
            *name = m_button->AccessibilityNameForChild(child_id);
        }
        return name->IsEmpty() ? wxACC_NOT_IMPLEMENTED : wxACC_OK;
    }

    wxAccStatus GetRole(int child_id, wxAccRole *role) override
    {
        if (!role || child_id < wxACC_SELF || child_id > 5)
            return wxACC_NOT_IMPLEMENTED;
        *role = child_id == wxACC_SELF ? wxROLE_SYSTEM_GROUPING : wxROLE_SYSTEM_PUSHBUTTON;
        return wxACC_OK;
    }

    wxAccStatus GetState(int child_id, long *state) override
    {
        if (!state || child_id < wxACC_SELF || child_id > 5)
            return wxACC_NOT_IMPLEMENTED;
        *state = 0;
        if (m_button->AcceptsFocusFromKeyboard())
            *state |= wxACC_STATE_SYSTEM_FOCUSABLE;
        if (m_button->HasFocus() &&
            ((child_id == wxACC_SELF && !m_button->AccessibilityHasCurrentChild()) ||
             m_button->AccessibilityChildIsCurrent(child_id)))
            *state |= wxACC_STATE_SYSTEM_FOCUSED;
        if (m_button->AccessibilityChildIsPressed(child_id))
            *state |= wxACC_STATE_SYSTEM_PRESSED;
        if (!m_button->IsEnabled())
            *state |= wxACC_STATE_SYSTEM_UNAVAILABLE;
        if (!m_button->IsShown())
            *state |= wxACC_STATE_SYSTEM_INVISIBLE;
        return wxACC_OK;
    }

    wxAccStatus GetFocus(int *child_id, wxAccessible **child) override
    {
        if (!child_id || !child)
            return wxACC_NOT_IMPLEMENTED;
        *child_id = 0;
        *child = nullptr;
        if (!m_button->HasFocus())
            return wxACC_OK;
        if (m_button->AccessibilityHasCurrentChild()) {
            *child_id = m_button->AccessibilityCurrentChildId();
        } else {
            *child = this;
        }
        return wxACC_OK;
    }

    wxAccStatus GetDefaultAction(int child_id, wxString *action_name) override
    {
        if (!action_name || child_id < 1 || child_id > 5)
            return wxACC_NOT_IMPLEMENTED;
        *action_name = _L("Press");
        return wxACC_OK;
    }

    wxAccStatus DoDefaultAction(int child_id) override
    {
        return m_button->AccessibilityActivate(child_id) ? wxACC_OK : wxACC_FAIL;
    }

private:
    AxisCtrlButton *m_button;
};
#endif

} // namespace

// MD3 Device-scheme tokens for the XY jog grid (Device.jsx Move control). The grid
// is used only in the Device/Monitor control column, so its tiles, accent and home
// resolve against the Device teal scheme + neutral surfaces at paint time (live
// theme-correct in light and dark). The migration retires the circular dial's
// arc-drawn rings and Grey/BrandGreen literals for the kit's 3x3 arrow grid while
// preserving the SetInt(position) event contract on_axis_ctrl_xy decodes.
static wxColour axis_tile_col()       { return StateColor::semantic(MD3::Role::SurfaceContainerHighest); }
static wxColour axis_tile_hover_col() { return StateColor::semantic(MD3::Role::SurfaceContainerHigh); }
static wxColour axis_press_col()      { return StateColor::semantic(MD3::Role::PrimaryContainer, MD3::ColorScheme::Device); }
static wxColour axis_accent_col()     { return StateColor::semantic(MD3::Role::Primary, MD3::ColorScheme::Device); }
static wxColour axis_home_col()       { return StateColor::semantic(MD3::Role::SecondaryContainer, MD3::ColorScheme::Device); }
static wxColour axis_on_home_col()    { return StateColor::semantic(MD3::Role::OnSecondaryContainer, MD3::ColorScheme::Device); }

BEGIN_EVENT_TABLE(AxisCtrlButton, wxPanel)
EVT_LEFT_DOWN(AxisCtrlButton::mouseDown)
EVT_LEFT_UP(AxisCtrlButton::mouseReleased)
EVT_MOTION(AxisCtrlButton::mouseMoving)
EVT_LEAVE_WINDOW(AxisCtrlButton::mouseLeave)
EVT_KEY_DOWN(AxisCtrlButton::keyDown)
EVT_PAINT(AxisCtrlButton::paintEvent)
END_EVENT_TABLE()

#define TILE_GAP        FromDIP(6)
#define TILE_RADIUS     FromDIP(8)

AxisCtrlButton::AxisCtrlButton(wxWindow *parent, ScalableBitmap &icon, long stlye)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, stlye | wxWANTS_CHARS)
    , current_cell(CELL_NONE)
    , text_color(std::make_pair(StateColor::semantic(MD3::Role::Outline), (int) StateColor::Disabled), std::make_pair(StateColor::semantic(MD3::Role::OnSurface), (int) StateColor::Normal))
    , state_handler(this)
{
    m_icon = icon;
    wxWindow::SetBackgroundColour(parent->GetBackgroundColour());

    border_color.append(axis_accent_col(), StateColor::Hovered);
    border_color.append(axis_accent_col(), StateColor::Normal);

    background_color.append(axis_tile_col(), StateColor::Disabled);
    background_color.append(axis_tile_col(), StateColor::Normal);

    inner_background_color.append(axis_tile_col(), StateColor::Normal);

    state_handler.attach({ &border_color, &background_color });
    state_handler.update_binds();

#if wxUSE_ACCESSIBILITY
    SetAccessible(new AxisCtrlButtonAccessible(this));
#endif
    Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent &event) {
        Refresh(false);
#if wxUSE_ACCESSIBILITY
        const int child_id = AccessibilityCurrentChildId();
        wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_FOCUS, this, wxOBJID_CLIENT,
                                  child_id == 0 ? wxACC_SELF : child_id);
#endif
        event.Skip();
    });
    Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &event) {
        Refresh(false);
        event.Skip();
    });
}

void AxisCtrlButton::updateParams() {}

void AxisCtrlButton::SetMinSize(const wxSize& size)
{
    if (size.GetWidth() > 0 && size.GetHeight() > 0) {
        minSize = size;
    } else if (size.GetWidth() > 0) {
        minSize.x = size.x;
    } else if (size.GetHeight() > 0) {
        minSize.y = size.y;
    } else {
        minSize = wxSize(168, 168);
    }
    wxWindow::SetMinSize(minSize);
    center = wxPoint(minSize.x / 2, minSize.y / 2);
}

void AxisCtrlButton::SetTextColor(StateColor const &color)
{
    text_color = color;
    state_handler.update_binds();
    Refresh();
}

void AxisCtrlButton::SetBorderColor(StateColor const& color)
{
    border_color = color;
    state_handler.update_binds();
    Refresh();
}

void AxisCtrlButton::SetBackgroundColor(StateColor const& color)
{
    background_color = color;
    state_handler.update_binds();
    Refresh();
}

void AxisCtrlButton::SetInnerBackgroundColor(StateColor const& color)
{
    inner_background_color = color;
    state_handler.update_binds();
    Refresh();
}

void AxisCtrlButton::SetBitmap(ScalableBitmap &bmp)
{
    if (&bmp  && (& bmp.bmp()) && (bmp.bmp().IsOk())) {
        m_icon = bmp;
    }
}

void AxisCtrlButton::SetStep(int mm)
{
    const int step = (mm == 1) ? 1 : 10;
    if (m_step == step)
        return;
    m_step = step;
#if wxUSE_ACCESSIBILITY
    for (int child_id = 1; child_id <= 5; ++child_id)
        wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_NAMECHANGE, this, wxOBJID_CLIENT, child_id);
#endif
}

wxString AxisCtrlButton::AccessibilityNameForChild(int child_id) const
{
    switch (child_id) {
    case 1: return wxString::Format(_L("Move Y up %d mm"), m_step);
    case 2: return wxString::Format(_L("Move X left %d mm"), m_step);
    case 3: return _L("Home XY axes");
    case 4: return wxString::Format(_L("Move X right %d mm"), m_step);
    case 5: return wxString::Format(_L("Move Y down %d mm"), m_step);
    default: return wxString();
    }
}

bool AxisCtrlButton::AccessibilityActivate(int child_id)
{
    if (!IsEnabled() || !IsShown() || child_id < 1 || child_id > 5)
        return false;
    current_cell = static_cast<unsigned char>(child_id - 1);
    Refresh(false);
    sendButtonEvent();
    return true;
}

bool AxisCtrlButton::AccessibilityHasCurrentChild() const
{
    return current_cell >= CELL_UP && current_cell <= CELL_DOWN;
}

int AxisCtrlButton::AccessibilityCurrentChildId() const
{
    return AccessibilityHasCurrentChild() ? static_cast<int>(current_cell) + 1 : 0;
}

bool AxisCtrlButton::AccessibilityChildIsCurrent(int child_id) const
{
    return child_id >= 1 && child_id <= 5 && current_cell == child_id - 1;
}

bool AxisCtrlButton::AccessibilityChildIsPressed(int child_id) const
{
    return pressedDown && AccessibilityChildIsCurrent(child_id);
}

wxRect AxisCtrlButton::AccessibilityLocationForChild(int child_id) const
{
    wxRect location = child_id == 0 ? GetClientRect() : cellRect(child_id - 1);
    if (location.IsEmpty())
        return location;
    location.SetPosition(ClientToScreen(location.GetPosition()));
    return location;
}

int AxisCtrlButton::AccessibilityChildFromScreenPoint(const wxPoint& point) const
{
    const int cell = cellFromPoint(ScreenToClient(point));
    return cell >= CELL_UP && cell <= CELL_DOWN ? cell + 1 : 0;
}

void AxisCtrlButton::Rescale() {
    Refresh();
}

void AxisCtrlButton::gridMetrics(int &tile, int &gap, int &ox, int &oy) const
{
    wxSize sz = GetSize();
    gap       = TILE_GAP;
    int avail = std::min(sz.x, sz.y) - 2 * gap;
    tile      = std::max(FromDIP(28), avail / 3);
    int grid  = 3 * tile + 2 * gap;
    ox        = (sz.x - grid) / 2;
    oy        = (sz.y - grid) / 2;
}

wxRect AxisCtrlButton::cellRect(int cell) const
{
    int tile, gap, ox, oy;
    gridMetrics(tile, gap, ox, oy);
    int col = 1, row = 1;
    switch (cell) {
    case CELL_UP:    col = 1; row = 0; break;
    case CELL_LEFT:  col = 0; row = 1; break;
    case CELL_HOME:  col = 1; row = 1; break;
    case CELL_RIGHT: col = 2; row = 1; break;
    case CELL_DOWN:  col = 1; row = 2; break;
    default:         return wxRect();
    }
    return wxRect(ox + col * (tile + gap), oy + row * (tile + gap), tile, tile);
}

int AxisCtrlButton::cellFromPoint(const wxPoint& p) const
{
    for (int c = CELL_UP; c <= CELL_DOWN; ++c) {
        if (cellRect(c).Contains(p)) return c;
    }
    return CELL_NONE;
}

int AxisCtrlButton::positionForCell(int cell) const
{
    switch (cell) {
    case CELL_UP:    return m_step == 10 ? 0 : 4; // Y+
    case CELL_LEFT:  return m_step == 10 ? 1 : 5; // X-
    case CELL_DOWN:  return m_step == 10 ? 2 : 6; // Y-
    case CELL_RIGHT: return m_step == 10 ? 3 : 7; // X+
    case CELL_HOME:  return 8;                     // auto-home
    default:         return -1;
    }
}

void AxisCtrlButton::paintEvent(wxPaintEvent& evt)
{
    wxPaintDC dc(this);
    wxGCDC gcdc(dc);
    render(gcdc);
}

void AxisCtrlButton::render(wxDC& dc)
{
    wxGraphicsContext* gc = dc.GetGraphicsContext();
    if (!gc) return;

    const bool   enabled = IsEnabled();
    const int    st      = enabled ? 0 : (int) StateColor::Disabled;
    const double dpi     = GetDPIScaleFactor() > 0.0 ? GetDPIScaleFactor() : 1.0;

    int tile, gap, ox, oy;
    gridMetrics(tile, gap, ox, oy);

    struct CellSpec { int cell; uint32_t glyph; const wchar_t *fallback; };
    const CellSpec specs[] = {
        {CELL_UP,    MaterialIcon::ArrowUp,    L"Y"},
        {CELL_LEFT,  MaterialIcon::ArrowLeft,  L"-X"},
        {CELL_HOME,  MaterialIcon::Home,       L""},
        {CELL_RIGHT, MaterialIcon::ArrowRight, L"X"},
        {CELL_DOWN,  MaterialIcon::ArrowDown,  L"-Y"},
    };

    const int glyph_px = std::max(1, (int) (tile * 0.5 / dpi + 0.5));

    for (const auto &s : specs) {
        wxRect     r      = cellRect(s.cell);
        const bool isHome = (s.cell == CELL_HOME);
        const bool active = enabled && (current_cell == s.cell);

        wxColour fill;
        if (!enabled)
            fill = isHome ? axis_home_col() : axis_tile_col();
        else if (active && pressedDown)
            fill = axis_press_col();
        else if (active)
            fill = isHome ? axis_home_col() : axis_tile_hover_col();
        else
            fill = isHome ? axis_home_col() : axis_tile_col();

        gc->SetBrush(wxBrush(fill));
        if (active)
            gc->SetPen(wxPen(border_color.colorForStates(state_handler.states() | StateColor::Hovered),
                               HasFocus() ? std::max(FromDIP(2), 1) : 2));
        else
            gc->SetPen(*wxTRANSPARENT_PEN);
        gc->DrawRoundedRectangle(r.x, r.y, r.width, r.height, TILE_RADIUS);

        // Glyph colour: on-secondary-container for home; the SetTextColor role
        // (OnSurface / Outline-when-disabled) for the arrows.
        wxColour glyph_col = isHome ? (enabled ? axis_on_home_col() : text_color.colorForStates(StateColor::Disabled))
                                    : text_color.colorForStates(st);

        if (MaterialIcon::available()) {
            // The variable icon face must not reach GDI+ as a font (heap
            // corruption); composite a plain-GDI raster. glyph_px is already
            // in this context's device coordinate space.
            const wxBitmap gb = MaterialIcon::bitmapPx(s.glyph, glyph_px, glyph_col);
            gc->DrawBitmap(gb, r.x + (r.width - gb.GetWidth()) / 2, r.y + (r.height - gb.GetHeight()) / 2,
                           gb.GetWidth(), gb.GetHeight());
        } else if (isHome && m_icon.bmp().IsOk()) {
            gc->DrawBitmap(m_icon.bmp(), r.x + (r.width - m_icon.GetBmpWidth()) / 2,
                           r.y + (r.height - m_icon.GetBmpHeight()) / 2, m_icon.GetBmpWidth(), m_icon.GetBmpHeight());
        } else if (!isHome) {
            gc->SetFont(enabled ? Label::Head_12 : Label::Body_12, glyph_col);
            wxDouble gw = 0, gh = 0;
            gc->GetTextExtent(s.fallback, &gw, &gh);
            gc->DrawText(s.fallback, r.x + (r.width - gw) / 2, r.y + (r.height - gh) / 2);
        }
    }

    if (HasFocus() && !AccessibilityHasCurrentChild()) {
        const wxRect grid_rect(ox, oy, 3 * tile + 2 * gap, 3 * tile + 2 * gap);
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        gc->SetPen(wxPen(axis_accent_col(), std::max(FromDIP(2), 1)));
        gc->DrawRoundedRectangle(grid_rect.x, grid_rect.y, grid_rect.width, grid_rect.height, TILE_RADIUS);
    }
}

void AxisCtrlButton::mouseDown(wxMouseEvent& event)
{
    event.Skip();
    pressedDown  = true;
    current_cell = cellFromPoint(event.GetPosition());
    SetFocus();
    CaptureMouse();
    Refresh();
#if wxUSE_ACCESSIBILITY
    const int child_id = AccessibilityCurrentChildId();
    if (child_id != 0)
        wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_STATECHANGE, this, wxOBJID_CLIENT, child_id);
#endif
}

void AxisCtrlButton::mouseReleased(wxMouseEvent& event)
{
    event.Skip();
    if (pressedDown) {
        const int child_id = AccessibilityCurrentChildId();
        pressedDown = false;
        if (HasCapture()) ReleaseMouse();
        if (wxRect({0, 0}, GetSize()).Contains(event.GetPosition()))
            sendButtonEvent();
        Refresh();
#if wxUSE_ACCESSIBILITY
        if (child_id != 0)
            wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_STATECHANGE, this, wxOBJID_CLIENT, child_id);
#endif
    }
}

void AxisCtrlButton::mouseMoving(wxMouseEvent& event)
{
    if (pressedDown) return;
    unsigned char cell = (unsigned char) cellFromPoint(event.GetPosition());
    if (cell != current_cell) {
        current_cell = cell;
        Refresh();
#if wxUSE_ACCESSIBILITY
        if (HasFocus()) {
            const int child_id = AccessibilityCurrentChildId();
            wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_FOCUS, this, wxOBJID_CLIENT,
                                      child_id == 0 ? wxACC_SELF : child_id);
        }
#endif
    }
}

void AxisCtrlButton::mouseLeave(wxMouseEvent& event)
{
    event.Skip();
    if (!pressedDown && current_cell != CELL_NONE) {
        current_cell = CELL_NONE;
        Refresh();
#if wxUSE_ACCESSIBILITY
        if (HasFocus())
            wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_FOCUS, this, wxOBJID_CLIENT, wxACC_SELF);
#endif
    }
}

void AxisCtrlButton::keyDown(wxKeyEvent& event)
{
    if (!IsEnabled() || !IsShown()) { event.Skip(); return; }
    int cell = CELL_NONE;
    switch (event.GetKeyCode()) {
    case WXK_UP:    cell = CELL_UP; break;
    case WXK_LEFT:  cell = CELL_LEFT; break;
    case WXK_RIGHT: cell = CELL_RIGHT; break;
    case WXK_DOWN:  cell = CELL_DOWN; break;
    case WXK_HOME:  cell = CELL_HOME; break;
    default:        event.Skip(); return;
    }
    current_cell = (unsigned char) cell;
    Refresh();
#if wxUSE_ACCESSIBILITY
    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_FOCUS, this, wxOBJID_CLIENT, cell + 1);
#endif
    sendButtonEvent();
}

#ifdef __WIN32__
WXLRESULT AxisCtrlButton::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    if (nMsg == WM_GETDLGCODE)
        return DLGC_WANTARROWS;
    return wxWindow::MSWWindowProc(nMsg, wParam, lParam);
}
#endif

void AxisCtrlButton::sendButtonEvent()
{
    int position = positionForCell(current_cell);
    if (position < 0) return;

    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED, GetId());
    event.SetEventObject(this);
    event.SetInt(position);
    GetEventHandler()->ProcessEvent(event);
}
