#include "AxisCtrlButton.hpp"
#include "Label.hpp"
#include "MaterialIcon.hpp"
#include "StateColor.hpp"
#include "libslic3r/libslic3r.h"

#include <wx/dcclient.h>
#include <wx/dcgraph.h>

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
    m_step = (mm == 1) ? 1 : 10;
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
            gc->SetPen(wxPen(border_color.colorForStates(state_handler.states() | StateColor::Hovered), 2));
        else
            gc->SetPen(*wxTRANSPARENT_PEN);
        gc->DrawRoundedRectangle(r.x, r.y, r.width, r.height, TILE_RADIUS);

        // Glyph colour: on-secondary-container for home; the SetTextColor role
        // (OnSurface / Outline-when-disabled) for the arrows.
        wxColour glyph_col = isHome ? (enabled ? axis_on_home_col() : text_color.colorForStates(StateColor::Disabled))
                                    : text_color.colorForStates(st);

        if (MaterialIcon::available()) {
            gc->SetFont(MaterialIcon::font(glyph_px), glyph_col);
            wxDouble gw = 0, gh = 0;
            gc->GetTextExtent(MaterialIcon::text(s.glyph), &gw, &gh);
            gc->DrawText(MaterialIcon::text(s.glyph), r.x + (r.width - gw) / 2, r.y + (r.height - gh) / 2);
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
}

void AxisCtrlButton::mouseDown(wxMouseEvent& event)
{
    event.Skip();
    pressedDown  = true;
    current_cell = cellFromPoint(event.GetPosition());
    SetFocus();
    CaptureMouse();
    Refresh();
}

void AxisCtrlButton::mouseReleased(wxMouseEvent& event)
{
    event.Skip();
    if (pressedDown) {
        pressedDown = false;
        if (HasCapture()) ReleaseMouse();
        if (wxRect({0, 0}, GetSize()).Contains(event.GetPosition()))
            sendButtonEvent();
        Refresh();
    }
}

void AxisCtrlButton::mouseMoving(wxMouseEvent& event)
{
    if (pressedDown) return;
    unsigned char cell = (unsigned char) cellFromPoint(event.GetPosition());
    if (cell != current_cell) {
        current_cell = cell;
        Refresh();
    }
}

void AxisCtrlButton::mouseLeave(wxMouseEvent& event)
{
    event.Skip();
    if (!pressedDown && current_cell != CELL_NONE) {
        current_cell = CELL_NONE;
        Refresh();
    }
}

void AxisCtrlButton::keyDown(wxKeyEvent& event)
{
    if (!IsEnabled()) { event.Skip(); return; }
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
    sendButtonEvent();
}

void AxisCtrlButton::sendButtonEvent()
{
    int position = positionForCell(current_cell);
    if (position < 0) return;

    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED, GetId());
    event.SetEventObject(this);
    event.SetInt(position);
    GetEventHandler()->ProcessEvent(event);
}
