#include "MD3ScrolledWindow.hpp"

#include <cstdlib>
#include <algorithm>

MD3ScrolledWindow::MD3ScrolledWindow() = default;

MD3ScrolledWindow::MD3ScrolledWindow(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size, long style, const wxString &name)
{
    // Created here rather than through the base constructor, so that Create()
    // already runs with this class's MSWGetStyle() and MSWWindowProc().
    Create(parent, id, pos, size, style, name);
}

MD3ScrolledWindow::~MD3ScrolledWindow()
{
#ifdef __WXMSW__
    m_bars.Abandon();
#endif
}

bool MD3ScrolledWindow::Create(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size, long style, const wxString &name)
{
    if (!wxScrolledWindow::Create(parent, id, pos, size, style, name)) return false;
    Bind(wxEVT_MOUSEWHEEL, &MD3ScrolledWindow::OnMouseWheel, this);
    Bind(wxEVT_CHAR, &MD3ScrolledWindow::OnChar, this);
    return true;
}

void MD3ScrolledWindow::OnMouseWheel(wxMouseEvent &event)
{
    if (m_reveal_owner && !m_reveal_owner->IsBeingDeleted()) {
        // A zero-range nested scroll helper otherwise consumes the wheel.
        // Preserve delta, axis and modifiers and deliver once to the owner.
        wxMouseEvent forwarded(event);
        forwarded.SetEventObject(m_reveal_owner.get());
        m_reveal_owner->GetEventHandler()->ProcessEvent(forwarded);
        return;
    }
    event.Skip();
}

void MD3ScrolledWindow::OnChar(wxKeyEvent &event)
{
    // Only background navigation belongs to the outer viewport. A focused
    // text field or category control keeps its own editing/navigation keys.
    if (m_reveal_owner && !m_reveal_owner->IsBeingDeleted() &&
        wxWindow::FindFocus() == this && !event.HasAnyModifiers()) {
        switch (event.GetKeyCode()) {
        case WXK_PAGEUP: case WXK_PAGEDOWN: case WXK_HOME: case WXK_END:
        case WXK_UP: case WXK_DOWN: case WXK_LEFT: case WXK_RIGHT: {
            wxKeyEvent forwarded(event);
            forwarded.SetEventObject(m_reveal_owner.get());
            m_reveal_owner->GetEventHandler()->ProcessEvent(forwarded);
            return;
        }
        default: break;
        }
    }
    event.Skip();
}

void MD3ScrolledWindow::SetRevealOwner(MD3ScrolledWindow *owner)
{
    if (owner == this || (owner && !owner->IsDescendant(this))) return;
    m_reveal_owner = owner;
    if (owner) {
        Scroll(0, 0);
        SetScrollRate(0, 0);
        EnableScrolling(false, false);
        EnableKeyboardScrolling(false);
        ShowScrollbars(wxSHOW_SB_NEVER, wxSHOW_SB_NEVER);
    }
}

void MD3ScrolledWindow::RevealChild(wxWindow *child)
{
    if (!child || child == this || !IsDescendant(child)) return;
    if (m_reveal_owner) {
        m_reveal_owner->RevealChild(child);
        return;
    }
    const wxRect viewport(GetClientRect());
    const wxRect target(ScreenToClient(child->GetScreenPosition()), child->GetSize());
    int sx, sy;
    GetScrollPixelsPerUnit(&sx, &sy);
    const wxPoint start = GetViewStart();
    auto reveal = [](int position, int extent, int visible, int unit, int start) {
        if (unit <= 0 || visible <= 0) return start;
        // Treat axes independently: a wide group must not suppress vertical reveal.
        // Oversized targets expose their leading edge instead of being ignored.
        int delta = position < 0 ? position :
            (extent > visible ? position : std::max(0, position + extent - visible));
        const int pixels = std::max(0, start * unit + delta);
        return (pixels + (delta > 0 ? unit - 1 : 0)) / unit;
    };
    Scroll(reveal(target.x, target.width, viewport.width, sx, start.x),
           reveal(target.y, target.height, viewport.height, sy, start.y));
}

bool MD3ScrolledWindow::ShouldScrollToChildOnFocus(wxWindow *child)
{
    RevealChild(child);
    return false;
}

#ifdef __WXMSW__

// Never forwarded to wxWindowMSW: that would hand the bar to Windows, which
// would draw it.
void MD3ScrolledWindow::SetScrollbar(int orient, int pos, int thumbVisible, int range, bool refresh)
{
    m_bars.SetScrollbar(orient, pos, thumbVisible, range, refresh);
}

void MD3ScrolledWindow::SetScrollPos(int orient, int pos, bool refresh) { m_bars.SetScrollPos(orient, pos, refresh); }

int MD3ScrolledWindow::GetScrollPos(int orient) const { return m_bars.GetScrollPos(orient); }

int MD3ScrolledWindow::GetScrollThumb(int orient) const { return m_bars.GetScrollThumb(orient); }

int MD3ScrolledWindow::GetScrollRange(int orient) const { return m_bars.GetScrollRange(orient); }

bool MD3ScrolledWindow::ScrollLines(int lines)
{
    // wxWindowMSW::ScrollLines() reads the native position back to see whether
    // anything moved, and there is no native bar: go through the scroll helper.
    const int before = GetViewStart().y;
    const wxEventType type = lines > 0 ? wxEVT_SCROLLWIN_LINEDOWN : wxEVT_SCROLLWIN_LINEUP;
    for (int i = std::abs(lines); i > 0; --i)
        m_bars.SendScroll(wxVERTICAL, type);
    return GetViewStart().y != before;
}

bool MD3ScrolledWindow::ScrollPages(int pages)
{
    const int before = GetViewStart().y;
    const wxEventType type = pages > 0 ? wxEVT_SCROLLWIN_PAGEDOWN : wxEVT_SCROLLWIN_PAGEUP;
    for (int i = std::abs(pages); i > 0; --i)
        m_bars.SendScroll(wxVERTICAL, type);
    return GetViewStart().y != before;
}

WXDWORD MD3ScrolledWindow::MSWGetStyle(long flags, WXDWORD *exstyle) const
{
    return MD3ScrollBars::WithoutNativeBars(wxScrolledWindow::MSWGetStyle(flags, exstyle));
}

WXLRESULT MD3ScrolledWindow::MSWWindowProc(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam)
{
    WXLRESULT result = 0;
    if (m_bars.Before(msg, wParam, lParam, result))
        return result;
    result = wxScrolledWindow::MSWWindowProc(msg, wParam, lParam);
    m_bars.After(msg, wParam, lParam, result);
    return result;
}

#endif // __WXMSW__
