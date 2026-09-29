#include "MD3ScrollBars.hpp"

#include "StateColor.hpp"

#include <wx/bitmap.h>
#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include <wx/dcmemory.h>
#include <wx/settings.h>

#include <algorithm>
#include <cmath>

#ifdef __WXMSW__
#include <windows.h>
#endif

namespace {
constexpr int kBarDip = 10; // strip thickness (base.css ::-webkit-scrollbar width)
#ifdef __WXMSW__
constexpr int      kInsetDip      = 2;      // transparent border around the thumb (base.css border:2px)
constexpr int      kMinThumbDip   = 20;     // shortest thumb, so a long page still has something to grab
constexpr UINT_PTR kRepeatTimer   = 0x4D33; // paging repeat while the track is held
constexpr UINT     kRepeatDelayMs = 350;    // before the first repeat, as a native bar waits
constexpr UINT     kRepeatMs      = 50;

wxPoint lparam_point(WXLPARAM lp)
{
    // Mouse coordinates are signed 16-bit values (negative on a monitor to the
    // left of or above the primary one).
    return wxPoint(static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)));
}

bool high_contrast()
{
    HIGHCONTRASTW hc {};
    hc.cbSize = sizeof(hc);
    return ::SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0) && (hc.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

// The colour the client area is erased with: the nearest window, starting at
// w, that has a background colour of its own.
wxColour effective_background(const wxWindow *w)
{
    for (const wxWindow *cur = w; cur != nullptr; cur = cur->GetParent()) {
        if (cur->UseBgCol())
            return cur->GetBackgroundColour();
        if (cur->IsTopLevel())
            break;
    }
    return w->GetBackgroundColour();
}
#endif // __WXMSW__
} // namespace

MD3ScrollBars::MD3ScrollBars(wxWindow *owner) : m_owner(owner) {}

MD3ScrollBars::~MD3ScrollBars() = default;

int MD3ScrollBars::Thickness(const wxWindow *ref)
{
    return ref != nullptr ? ref->FromDIP(kBarDip) : kBarDip;
}

void MD3ScrollBars::SendScroll(int orient, wxEventType type, int pos)
{
    if (m_handler) {
        m_handler(orient, type, pos);
        return;
    }
    wxScrollWinEvent event(type, pos, orient);
    event.SetEventObject(m_owner);
    m_owner->HandleWindowEvent(event);
}

#ifndef __WXMSW__

bool MD3ScrollBars::IsShown(int orient) const
{
    return m_owner->HasScrollbar(orient);
}

#else // __WXMSW__

bool MD3ScrollBars::IsShown(int orient) const
{
    const Bar &b = bar(orient);
    // wx passes a range of -1 for wxSHOW_SB_ALWAYS with nothing to scroll: an
    // empty track, as a disabled native bar would show.
    if (b.range == -1)
        return true;
    if (b.range > 0 && b.range > b.thumb)
        return true;
    return m_owner->HasFlag(wxALWAYS_SHOW_SB) && m_owner->HasFlag(orient == wxHORIZONTAL ? wxHSCROLL : wxVSCROLL);
}

void MD3ScrollBars::SetScrollbar(int orient, int pos, int thumb, int range, bool refresh)
{
    const bool was_shown = IsShown(orient);
    Bar &b = bar(orient);
    b.thumb = thumb;
    b.range = range;
    b.pos   = std::max(0, std::min(pos, std::max(0, range - thumb)));
    if (IsShown(orient) != was_shown)
        frame_changed(); // reserve or give back the strip; Windows sends WM_SIZE as it does for a native bar
    else if (refresh)
        paint();
}

void MD3ScrollBars::SetScrollPos(int orient, int pos, bool refresh)
{
    Bar &b = bar(orient);
    b.pos = std::max(0, std::min(pos, std::max(0, b.range - b.thumb)));
    if (refresh)
        paint();
}

int MD3ScrollBars::GetScrollPos(int orient) const { return bar(orient).pos; }

int MD3ScrollBars::GetScrollThumb(int orient) const { return bar(orient).thumb; }

int MD3ScrollBars::GetScrollRange(int orient) const { return std::max(0, bar(orient).range); }

WXDWORD MD3ScrollBars::WithoutNativeBars(WXDWORD style)
{
    // The wx flags keep wxHSCROLL/wxVSCROLL, which is what the scroll helper
    // reads; Windows gets no scrollbar style, so it never shows a bar.
    return style & ~static_cast<WXDWORD>(WS_HSCROLL | WS_VSCROLL);
}

MD3ScrollBars::Strips MD3ScrollBars::strips() const
{
    Strips s;
    HWND hwnd = (HWND) m_owner->GetHWND();
    if (hwnd == nullptr)
        return s;
    RECT wr, cr;
    ::GetWindowRect(hwnd, &wr);
    ::GetClientRect(hwnd, &cr);
    POINT origin { 0, 0 };
    ::ClientToScreen(hwnd, &origin);
    const int cx = origin.x - wr.left;
    const int cy = origin.y - wr.top;
    const int cw = cr.right - cr.left;
    const int ch = cr.bottom - cr.top;
    if (m_reserved_v > 0)
        s.v = wxRect(cx + cw, cy, m_reserved_v, ch);
    if (m_reserved_h > 0)
        s.h = wxRect(cx, cy + ch, cw, m_reserved_h);
    if (m_reserved_v > 0 && m_reserved_h > 0)
        s.corner = wxRect(cx + cw, cy + ch, m_reserved_v, m_reserved_h);
    return s;
}

wxRect MD3ScrollBars::thumb_rect(int orient, const wxRect &strip) const
{
    const Bar &b = bar(orient);
    if (strip.IsEmpty() || b.thumb <= 0 || b.range <= b.thumb)
        return wxRect();
    wxRect track = strip;
    track.Deflate(m_owner->FromDIP(kInsetDip));
    const bool vertical = orient == wxVERTICAL;
    const int  length   = vertical ? track.height : track.width;
    if (length <= 0 || track.IsEmpty())
        return wxRect();
    int thumb_len = static_cast<int>(static_cast<long long>(length) * b.thumb / b.range);
    thumb_len     = std::min(length, std::max(thumb_len, m_owner->FromDIP(kMinThumbDip)));
    const int free    = length - thumb_len;
    const int max_pos = b.range - b.thumb;
    const int offset  = static_cast<int>(static_cast<long long>(free) * std::max(0, std::min(b.pos, max_pos)) / max_pos);
    return vertical ? wxRect(track.x, track.y + offset, track.width, thumb_len)
                    : wxRect(track.x + offset, track.y, thumb_len, track.height);
}

wxPoint MD3ScrollBars::screen_to_window(const wxPoint &screen) const
{
    RECT wr { 0, 0, 0, 0 };
    if (HWND hwnd = (HWND) m_owner->GetHWND())
        ::GetWindowRect(hwnd, &wr);
    return wxPoint(screen.x - wr.left, screen.y - wr.top);
}

int MD3ScrollBars::strip_at(const wxPoint &window_pt) const
{
    const Strips s = strips();
    if (s.v.Contains(window_pt))
        return wxVERTICAL;
    if (s.h.Contains(window_pt))
        return wxHORIZONTAL;
    return 0;
}

int MD3ScrollBars::thumb_at(const wxPoint &window_pt) const
{
    const Strips s = strips();
    if (s.v.Contains(window_pt) && thumb_rect(wxVERTICAL, s.v).Contains(window_pt))
        return wxVERTICAL;
    if (s.h.Contains(window_pt) && thumb_rect(wxHORIZONTAL, s.h).Contains(window_pt))
        return wxHORIZONTAL;
    return 0;
}

bool MD3ScrollBars::in_strips(const wxPoint &window_pt) const
{
    const Strips s = strips();
    return s.v.Contains(window_pt) || s.h.Contains(window_pt) || s.corner.Contains(window_pt);
}

void MD3ScrollBars::paint_strip(wxDC &dc, int orient, const wxRect &strip, const wxColour &track)
{
    if (strip.IsEmpty())
        return;
    // Drawn off screen and copied in one go, so the thumb never flickers
    // while the wheel or a drag moves it.
    wxBitmap   buffer(strip.width, strip.height);
    wxMemoryDC mem(buffer);
    mem.SetPen(*wxTRANSPARENT_PEN);
    mem.SetBrush(wxBrush(track));
    mem.DrawRectangle(0, 0, strip.width, strip.height);
    wxRect thumb = thumb_rect(orient, strip);
    if (!thumb.IsEmpty()) {
        thumb.Offset(-strip.x, -strip.y);
        const bool active = m_hover == orient || m_drag == orient;
        wxColour   colour;
        if (high_contrast())
            colour = wxSystemSettings::GetColour(active ? wxSYS_COLOUR_HIGHLIGHT : wxSYS_COLOUR_WINDOWTEXT);
        else
            colour = StateColor::semantic(active ? MD3::Role::Outline : MD3::Role::OutlineVariant);
        wxGCDC gc(mem);
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(colour));
        gc.DrawRoundedRectangle(thumb, std::min(thumb.width, thumb.height) / 2.0);
    }
    mem.SelectObject(wxNullBitmap);
    dc.DrawBitmap(buffer, strip.x, strip.y);
}

void MD3ScrollBars::paint()
{
    if (m_reserved_v <= 0 && m_reserved_h <= 0)
        return;
    HWND hwnd = (HWND) m_owner->GetHWND();
    if (hwnd == nullptr || !::IsWindowVisible(hwnd))
        return;
    const Strips   s     = strips();
    const wxColour track = high_contrast() ? wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW) : effective_background(m_owner);
    wxWindowDC     dc(m_owner); // whole-window DC: the strips are non-client
    paint_strip(dc, wxVERTICAL, s.v, track);
    paint_strip(dc, wxHORIZONTAL, s.h, track);
    if (!s.corner.IsEmpty()) {
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(track));
        dc.DrawRectangle(s.corner);
    }
}

void MD3ScrollBars::frame_changed()
{
    // Makes Windows ask WM_NCCALCSIZE again, which is where the strips are
    // reserved; a changed client area then arrives as WM_SIZE, exactly as
    // when Windows shows or hides a scrollbar itself.
    if (HWND hwnd = (HWND) m_owner->GetHWND())
        ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                       SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

void MD3ScrollBars::press(int orient, const wxPoint &window_pt)
{
    const Strips s     = strips();
    const wxRect strip = orient == wxVERTICAL ? s.v : s.h;
    const wxRect thumb = thumb_rect(orient, strip);
    if (thumb.IsEmpty())
        return; // an empty track: nothing to scroll
    HWND       hwnd     = (HWND) m_owner->GetHWND();
    const bool vertical = orient == wxVERTICAL;
    const int  at       = vertical ? window_pt.y : window_pt.x;
    const int  start    = vertical ? thumb.y : thumb.x;
    const int  end      = start + (vertical ? thumb.height : thumb.width);
    if (at >= start && at < end) {
        m_drag        = orient;
        m_drag_origin = at;
        m_drag_pos    = bar(orient).pos;
    } else {
        m_page     = orient;
        m_page_dir = at < start ? -1 : 1;
        page_once();
        ::SetTimer(hwnd, kRepeatTimer, kRepeatDelayMs, nullptr);
    }
    ::SetCapture(hwnd);
    paint();
}

void MD3ScrollBars::drag_to(const wxPoint &window_pt)
{
    const int    orient = m_drag;
    const Bar   &b      = bar(orient);
    const Strips s      = strips();
    const wxRect strip  = orient == wxVERTICAL ? s.v : s.h;
    const wxRect thumb  = thumb_rect(orient, strip);
    if (thumb.IsEmpty())
        return;
    const bool vertical = orient == wxVERTICAL;
    const int  track    = (vertical ? strip.height : strip.width) - 2 * m_owner->FromDIP(kInsetDip);
    const int  free     = track - (vertical ? thumb.height : thumb.width);
    const int  max_pos  = b.range - b.thumb;
    if (free <= 0 || max_pos <= 0)
        return;
    const int delta = (vertical ? window_pt.y : window_pt.x) - m_drag_origin;
    int pos = m_drag_pos + static_cast<int>(std::lround(static_cast<double>(delta) * max_pos / free));
    pos     = std::max(0, std::min(pos, max_pos));
    if (pos != b.pos)
        SendScroll(orient, wxEVT_SCROLLWIN_THUMBTRACK, pos);
}

void MD3ScrollBars::page_once()
{
    const int orient = m_page;
    if (orient == 0)
        return;
    POINT cursor { 0, 0 };
    ::GetCursorPos(&cursor);
    const wxPoint p     = screen_to_window(wxPoint(cursor.x, cursor.y));
    const Strips  s     = strips();
    const wxRect  thumb = thumb_rect(orient, orient == wxVERTICAL ? s.v : s.h);
    if (thumb.IsEmpty())
        return;
    const bool vertical = orient == wxVERTICAL;
    const int  at       = vertical ? p.y : p.x;
    const int  start    = vertical ? thumb.y : thumb.x;
    const int  end      = start + (vertical ? thumb.height : thumb.width);
    // Stop once the thumb reaches the pointer, as a native bar does.
    if (m_page_dir < 0 ? at >= start : at < end)
        return;
    SendScroll(orient, m_page_dir < 0 ? wxEVT_SCROLLWIN_PAGEUP : wxEVT_SCROLLWIN_PAGEDOWN);
}

void MD3ScrollBars::release(bool release_capture, bool notify)
{
    const int dragged = m_drag;
    if (dragged == 0 && m_page == 0)
        return;
    HWND hwnd = (HWND) m_owner->GetHWND();
    if (hwnd != nullptr)
        ::KillTimer(hwnd, kRepeatTimer);
    m_drag = 0;
    m_page = 0;
    if (release_capture && hwnd != nullptr && ::GetCapture() == hwnd) {
        m_releasing = true;
        ::ReleaseCapture();
        m_releasing = false;
    }
    if (!notify || m_owner->IsBeingDeleted())
        return;
    if (dragged != 0)
        SendScroll(dragged, wxEVT_SCROLLWIN_THUMBRELEASE, bar(dragged).pos);
    paint();
}

void MD3ScrollBars::Abandon() { release(true, false); }

void MD3ScrollBars::set_hover(int orient)
{
    if (m_hover == orient)
        return;
    m_hover = orient;
    paint();
}

bool MD3ScrollBars::Before(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam, WXLRESULT &result)
{
    HWND hwnd = (HWND) m_owner->GetHWND();
    switch (msg) {
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK: {
        const wxPoint p = screen_to_window(lparam_point(lParam));
        if (const int orient = strip_at(p)) {
            press(orient, p);
            result = 0;
            return true;
        }
        if (in_strips(p)) {
            result = 0; // the corner square
            return true;
        }
        return false;
    }
    case WM_NCMOUSEMOVE:
        set_hover(thumb_at(screen_to_window(lparam_point(lParam))));
        if (!m_tracking && hwnd != nullptr) {
            TRACKMOUSEEVENT tme {};
            tme.cbSize    = sizeof(tme);
            tme.dwFlags   = TME_LEAVE | TME_NONCLIENT;
            tme.hwndTrack = hwnd;
            m_tracking    = ::TrackMouseEvent(&tme) != FALSE;
        }
        return false;
    case WM_NCMOUSELEAVE:
        m_tracking = false;
        set_hover(0);
        return false;
    case WM_MOUSEMOVE:
        if (m_drag != 0) {
            POINT pt { static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)) };
            ::ClientToScreen(hwnd, &pt);
            drag_to(screen_to_window(wxPoint(pt.x, pt.y)));
            result = 0;
            return true;
        }
        if (m_page != 0) {
            result = 0;
            return true;
        }
        return false;
    case WM_LBUTTONUP:
        if (m_drag != 0 || m_page != 0) {
            release(true, true);
            result = 0;
            return true;
        }
        return false;
    case WM_CAPTURECHANGED:
        if (m_releasing) {
            result = 0;
            return true;
        }
        if (m_drag != 0 || m_page != 0) {
            release(false, true);
            result = 0;
            return true;
        }
        return false;
    case WM_TIMER:
        if (wParam != kRepeatTimer)
            return false;
        if (m_page != 0) {
            ::SetTimer(hwnd, kRepeatTimer, kRepeatMs, nullptr); // from the first delay to the repeat rate
            page_once();
        } else {
            ::KillTimer(hwnd, kRepeatTimer);
        }
        result = 0;
        return true;
    default:
        return false;
    }
}

void MD3ScrollBars::After(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam, WXLRESULT &result)
{
    switch (msg) {
    case WM_NCCALCSIZE: {
        RECT *rc = wParam ? &reinterpret_cast<NCCALCSIZE_PARAMS *>(lParam)->rgrc[0] : reinterpret_cast<RECT *>(lParam);
        const int thickness = Thickness(m_owner);
        m_reserved_v = IsShown(wxVERTICAL) ? std::max(0, std::min<int>(thickness, rc->right - rc->left)) : 0;
        m_reserved_h = IsShown(wxHORIZONTAL) ? std::max(0, std::min<int>(thickness, rc->bottom - rc->top)) : 0;
        rc->right -= m_reserved_v;
        rc->bottom -= m_reserved_h;
        break;
    }
    case WM_NCPAINT:
        paint();
        break;
    case WM_PAINT:
        // A theme or background change repaints the client area only; bring
        // the strips along so they never keep the old colours.
        paint();
        break;
    case WM_NCHITTEST:
        if (in_strips(screen_to_window(lparam_point(lParam))))
            result = HTBORDER; // non-client, with the arrow cursor and nothing else attached
        break;
    default:
        break;
    }
}

#endif // __WXMSW__
