#include "MD3ScrolledWindow.hpp"

#include <cstdlib>

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
    return wxScrolledWindow::Create(parent, id, pos, size, style, name);
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
