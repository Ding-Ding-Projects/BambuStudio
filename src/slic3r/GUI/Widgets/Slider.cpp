#include "Slider.hpp"

#include "StateColor.hpp"
#include "StaticBox.hpp"

#include <algorithm>
#include <cmath>

#include <wx/dcclient.h>
#include <wx/dcgraph.h>

// Logical (DIP) anatomy — mirrors selection/Slider.prompt.md. Everything is
// resolved through FromDIP() at paint / hit-test time (never cached in device
// px) so the slider stays crisp and correct across DPI / density changes.
namespace {
constexpr int kTrack        = 4;   // track thickness
constexpr int kThumbMax     = 16;  // thumb diameter ceiling
constexpr int kThumbMin     = 10;  // thumb diameter floor
constexpr int kHaloPad      = 4;   // focus/drag halo ring around the thumb
constexpr int kHBestW       = 160; // horizontal best width
constexpr int kHBestH       = 20;  // horizontal best height (room for the thumb)
constexpr int kVBestW       = 12;  // vertical best width (12x300 variant)
constexpr int kVBestH       = 300; // vertical best height

inline double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
} // namespace

Slider::Slider() {}

Slider::Slider(wxWindow *parent, int value, int minValue, int maxValue, bool vertical, const wxPoint &pos, const wxSize &size)
{
    Create(parent, value, minValue, maxValue, vertical, pos, size);
}

bool Slider::Create(wxWindow *parent, int value, int minValue, int maxValue, bool vertical, const wxPoint &pos, const wxSize &size)
{
    m_min      = minValue;
    m_max      = maxValue;
    m_vertical = vertical;
    m_value    = clampValue(value);

    if (!wxWindow::Create(parent, wxID_ANY, pos, size, wxFULL_REPAINT_ON_RESIZE))
        return false;

    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(StaticBox::GetParentBackgroundColor(parent));

    Bind(wxEVT_PAINT, &Slider::paintEvent, this);
    Bind(wxEVT_LEFT_DOWN, &Slider::onMouseDown, this);
    Bind(wxEVT_LEFT_UP, &Slider::onMouseUp, this);
    Bind(wxEVT_MOTION, &Slider::onMotion, this);
    Bind(wxEVT_MOUSEWHEEL, &Slider::onWheel, this);
    Bind(wxEVT_KEY_DOWN, &Slider::onKey, this);
    Bind(wxEVT_SET_FOCUS, &Slider::onFocus, this);
    Bind(wxEVT_KILL_FOCUS, &Slider::onFocus, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent &) { m_dragging = false; });
    Bind(wxEVT_SIZE, [this](wxSizeEvent &e) {
        Refresh(false);
        e.Skip();
    });

    const wxSize best = DoGetBestSize();
    SetMinSize(best);
    if (size == wxDefaultSize)
        SetInitialSize(best);
    return true;
}

void Slider::SetRange(int minValue, int maxValue)
{
    m_min   = minValue;
    m_max   = maxValue;
    m_value = clampValue(m_value);
    Refresh(false);
}

void Slider::SetValue(int value) { setValueInternal(value, false); }

void Slider::SetVertical(bool vertical)
{
    if (m_vertical == vertical)
        return;
    m_vertical = vertical;
    InvalidateBestSize();
    SetMinSize(DoGetBestSize());
    Refresh(false);
}

void Slider::SetColorScheme(MD3::ColorScheme scheme)
{
    m_scheme = scheme;
    Refresh(false);
}

void Slider::Rescale()
{
    InvalidateBestSize();
    Refresh(false);
}

wxSize Slider::DoGetBestSize() const
{
    return m_vertical ? wxSize(FromDIP(kVBestW), FromDIP(kVBestH)) : wxSize(FromDIP(kHBestW), FromDIP(kHBestH));
}

int Slider::clampValue(int value) const
{
    if (m_max <= m_min)
        return m_min;
    return std::min(m_max, std::max(m_min, value));
}

int Slider::thumbDiameter() const
{
    const wxSize sz    = GetSize();
    int          cross = m_vertical ? sz.x : sz.y;
    if (cross <= 0)
        cross = m_vertical ? FromDIP(kVBestW) : FromDIP(kHBestH);
    int d = std::min(cross, FromDIP(kThumbMax));
    if (d < FromDIP(kThumbMin))
        d = std::min(FromDIP(kThumbMin), cross);
    return std::max(1, d);
}

int Slider::trackThickness() const
{
    int t = FromDIP(kTrack);
    int d = thumbDiameter();
    if (t > d - FromDIP(2))
        t = d - FromDIP(2);
    return std::max(1, t);
}

int Slider::valueFromPoint(const wxPoint &p) const
{
    const wxSize sz = GetSize();
    const double td = thumbDiameter();
    double       r;
    if (m_vertical) {
        const double yBottom = sz.y - td / 2.0; // min
        const double yTop    = td / 2.0;        // max
        r = (yBottom == yTop) ? 0.0 : (p.y - yBottom) / (yTop - yBottom);
    } else {
        const double x0 = td / 2.0;
        const double x1 = sz.x - td / 2.0;
        r = (x1 == x0) ? 0.0 : (p.x - x0) / (x1 - x0);
    }
    r = clamp01(r);
    return m_min + (int) std::lround(r * (m_max - m_min));
}

void Slider::setValueInternal(int value, bool notify)
{
    const int c = clampValue(value);
    if (c == m_value)
        return;
    m_value = c;
    Refresh(false);
    if (notify && m_on_change)
        m_on_change(c);
}

void Slider::onMouseDown(wxMouseEvent &evt)
{
    SetFocus();
    m_dragging = true;
    if (!HasCapture())
        CaptureMouse();
    setValueInternal(valueFromPoint(evt.GetPosition()), true);
    evt.Skip();
}

void Slider::onMouseUp(wxMouseEvent &evt)
{
    if (m_dragging) {
        m_dragging = false;
        if (HasCapture())
            ReleaseMouse();
    }
    evt.Skip();
}

void Slider::onMotion(wxMouseEvent &evt)
{
    if (m_dragging && evt.Dragging() && evt.LeftIsDown())
        setValueInternal(valueFromPoint(evt.GetPosition()), true);
    evt.Skip();
}

void Slider::onWheel(wxMouseEvent &evt)
{
    const int step = std::max(1, (m_max - m_min) / 20);
    const int dir  = evt.GetWheelRotation() > 0 ? 1 : -1;
    setValueInternal(m_value + dir * step, true);
}

void Slider::onKey(wxKeyEvent &evt)
{
    const int page = std::max(1, (m_max - m_min) / 10);
    switch (evt.GetKeyCode()) {
    case WXK_LEFT:
    case WXK_DOWN: setValueInternal(m_value - 1, true); break;
    case WXK_RIGHT:
    case WXK_UP: setValueInternal(m_value + 1, true); break;
    case WXK_PAGEDOWN: setValueInternal(m_value - page, true); break;
    case WXK_PAGEUP: setValueInternal(m_value + page, true); break;
    case WXK_HOME: setValueInternal(m_min, true); break;
    case WXK_END: setValueInternal(m_max, true); break;
    default: evt.Skip(); return;
    }
}

void Slider::onFocus(wxFocusEvent &evt)
{
    m_focused = (evt.GetEventType() == wxEVT_SET_FOCUS);
    Refresh(false);
    evt.Skip();
}

void Slider::paintEvent(wxPaintEvent &)
{
    wxPaintDC pdc(this);
    pdc.SetBackground(wxBrush(GetBackgroundColour()));
    pdc.Clear();
    // Wrap the concrete wxPaintDC in a wxGCDC so the thin track and round thumb
    // draw anti-aliased on every backend.
    wxGCDC gdc(pdc);
    render(gdc);
}

void Slider::render(wxDC &dc)
{
    const wxSize sz = GetSize();
    if (sz.x <= 0 || sz.y <= 0)
        return;

    const bool     dark     = StateColor::isDarkMode();
    const wxColour active   = MD3::resolve(MD3::Role::Primary, dark, m_scheme);
    const wxColour inactive = MD3::resolve(MD3::Role::OutlineVariant, dark);

    const double td    = thumbDiameter();
    const int    tt    = trackThickness();
    const double rr    = tt / 2.0;
    const double range = (m_max > m_min) ? double(m_max - m_min) : 1.0;
    const double r     = clamp01((m_value - m_min) / range);

    dc.SetPen(*wxTRANSPARENT_PEN);

    double cx, cy;
    if (m_vertical) {
        const double yBottom = sz.y - td / 2.0;
        const double yTop    = td / 2.0;
        cx                   = sz.x / 2.0;
        cy                   = yBottom + r * (yTop - yBottom);
        const int left       = (int) std::lround(cx - tt / 2.0);
        // Inactive: full travel span.
        if (yBottom > yTop) {
            dc.SetBrush(wxBrush(inactive));
            dc.DrawRoundedRectangle(wxRect(left, (int) std::lround(yTop), tt, (int) std::lround(yBottom - yTop)), rr);
        }
        // Active: from the thumb down to the minimum (bottom).
        if (yBottom > cy) {
            dc.SetBrush(wxBrush(active));
            dc.DrawRoundedRectangle(wxRect(left, (int) std::lround(cy), tt, (int) std::lround(yBottom - cy)), rr);
        }
    } else {
        const double x0  = td / 2.0;
        const double x1  = sz.x - td / 2.0;
        cy               = sz.y / 2.0;
        cx               = x0 + r * (x1 - x0);
        const int top    = (int) std::lround(cy - tt / 2.0);
        // Inactive: full travel span.
        if (x1 > x0) {
            dc.SetBrush(wxBrush(inactive));
            dc.DrawRoundedRectangle(wxRect((int) std::lround(x0), top, (int) std::lround(x1 - x0), tt), rr);
        }
        // Active: from the minimum (left) to the thumb.
        if (cx > x0) {
            dc.SetBrush(wxBrush(active));
            dc.DrawRoundedRectangle(wxRect((int) std::lround(x0), top, (int) std::lround(cx - x0), tt), rr);
        }
    }

    // Focus / drag halo.
    if (m_focused || m_dragging) {
        dc.SetBrush(wxBrush(wxColour(active.Red(), active.Green(), active.Blue(), 40)));
        dc.DrawCircle((int) std::lround(cx), (int) std::lround(cy), (int) (td / 2.0) + FromDIP(kHaloPad));
    }

    // Primary circular thumb.
    dc.SetBrush(wxBrush(active));
    dc.DrawCircle((int) std::lround(cx), (int) std::lround(cy), (int) (td / 2.0));
}
