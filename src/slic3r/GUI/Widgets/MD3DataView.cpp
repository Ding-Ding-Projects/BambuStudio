#include "MD3DataView.hpp"

#ifdef __WXMSW__
#include <windows.h>
#endif

// ---------------------------------------------------------------------------
// MD3DataViewCtrl

MD3DataViewCtrl::MD3DataViewCtrl() = default;

MD3DataViewCtrl::MD3DataViewCtrl(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size, long style,
                                 const wxValidator &validator, const wxString &name)
{
    // Created here rather than through the base constructor, so that Create()
    // already runs with this class's MSWGetStyle() and MSWWindowProc().
    Create(parent, id, pos, size, style, validator, name);
}

MD3DataViewCtrl::~MD3DataViewCtrl()
{
#ifdef __WXMSW__
    m_bars.Abandon();
#endif
}

#ifdef __WXMSW__
// Never forwarded to wxWindowMSW: that would hand the bar to Windows, which
// would draw it.
void MD3DataViewCtrl::SetScrollbar(int orient, int pos, int thumbVisible, int range, bool refresh)
{
    m_bars.SetScrollbar(orient, pos, thumbVisible, range, refresh);
}

void MD3DataViewCtrl::SetScrollPos(int orient, int pos, bool refresh) { m_bars.SetScrollPos(orient, pos, refresh); }

int MD3DataViewCtrl::GetScrollPos(int orient) const { return m_bars.GetScrollPos(orient); }

int MD3DataViewCtrl::GetScrollThumb(int orient) const { return m_bars.GetScrollThumb(orient); }

int MD3DataViewCtrl::GetScrollRange(int orient) const { return m_bars.GetScrollRange(orient); }

WXDWORD MD3DataViewCtrl::MSWGetStyle(long flags, WXDWORD *exstyle) const
{
    return MD3ScrollBars::WithoutNativeBars(wxDataViewCtrl::MSWGetStyle(flags, exstyle));
}

WXLRESULT MD3DataViewCtrl::MSWWindowProc(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam)
{
    WXLRESULT result = 0;
    if (m_bars.Before(msg, wParam, lParam, result))
        return result;
    // wxDataViewCtrl's own window procedure is private. All it adds to its
    // base's is DLGC_WANTARROWS, so the arrow keys move the selection; do the
    // same here.
    result = wxDataViewCtrlBase::MSWWindowProc(msg, wParam, lParam);
    if (msg == WM_GETDLGCODE)
        result |= DLGC_WANTARROWS;
    m_bars.After(msg, wParam, lParam, result);
    return result;
}
#endif // __WXMSW__

// ---------------------------------------------------------------------------
// MD3DataViewListCtrl

MD3DataViewListCtrl::MD3DataViewListCtrl() = default;

MD3DataViewListCtrl::MD3DataViewListCtrl(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size, long style,
                                         const wxValidator &validator)
{
    Create(parent, id, pos, size, style, validator);
}

MD3DataViewListCtrl::~MD3DataViewListCtrl()
{
#ifdef __WXMSW__
    m_bars.Abandon();
#endif
}

#ifdef __WXMSW__
void MD3DataViewListCtrl::SetScrollbar(int orient, int pos, int thumbVisible, int range, bool refresh)
{
    m_bars.SetScrollbar(orient, pos, thumbVisible, range, refresh);
}

void MD3DataViewListCtrl::SetScrollPos(int orient, int pos, bool refresh) { m_bars.SetScrollPos(orient, pos, refresh); }

int MD3DataViewListCtrl::GetScrollPos(int orient) const { return m_bars.GetScrollPos(orient); }

int MD3DataViewListCtrl::GetScrollThumb(int orient) const { return m_bars.GetScrollThumb(orient); }

int MD3DataViewListCtrl::GetScrollRange(int orient) const { return m_bars.GetScrollRange(orient); }

WXDWORD MD3DataViewListCtrl::MSWGetStyle(long flags, WXDWORD *exstyle) const
{
    return MD3ScrollBars::WithoutNativeBars(wxDataViewListCtrl::MSWGetStyle(flags, exstyle));
}

WXLRESULT MD3DataViewListCtrl::MSWWindowProc(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam)
{
    WXLRESULT result = 0;
    if (m_bars.Before(msg, wParam, lParam, result))
        return result;
    result = wxDataViewCtrlBase::MSWWindowProc(msg, wParam, lParam);
    if (msg == WM_GETDLGCODE)
        result |= DLGC_WANTARROWS;
    m_bars.After(msg, wParam, lParam, result);
    return result;
}
#endif // __WXMSW__
