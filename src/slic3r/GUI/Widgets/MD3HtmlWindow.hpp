#ifndef slic3r_GUI_MD3HtmlWindow_hpp_
#define slic3r_GUI_MD3HtmlWindow_hpp_

#include <wx/html/htmlwin.h>

#include "MD3ScrollBars.hpp"

// A wxHtmlWindow whose scrollbars are the kit scrollbar (MD3ScrollBars)
// instead of the Windows ones. wxHtmlWindow is a wxScrolledWindow, so this is
// MD3ScrolledWindow's arrangement: wx keeps laying out the page and doing all
// of the scrolling (the wheel, the keyboard, selection drags, SetPage), and
// only where the bar is drawn changes.
//
// Use it wherever a wxHtmlWindow was used: the constructor takes the same
// arguments, and a wxHtmlWindow pointer can hold it.
class MD3HtmlWindow : public wxHtmlWindow
{
public:
    MD3HtmlWindow(wxWindow        *parent,
                  wxWindowID       id    = wxID_ANY,
                  const wxPoint   &pos   = wxDefaultPosition,
                  const wxSize    &size  = wxDefaultSize,
                  long             style = wxHW_DEFAULT_STYLE,
                  const wxString  &name  = wxT("htmlWindow"));
    ~MD3HtmlWindow() override;

    // Whether the kit bar for orient (wxVERTICAL or wxHORIZONTAL) is shown.
    bool IsBarShown(int orient) const { return m_bars.IsShown(orient); }

#ifdef __WXMSW__
    void SetScrollbar(int orient, int pos, int thumbVisible, int range, bool refresh = true) override;
    void SetScrollPos(int orient, int pos, bool refresh = true) override;
    int  GetScrollPos(int orient) const override;
    int  GetScrollThumb(int orient) const override;
    int  GetScrollRange(int orient) const override;
    bool ScrollLines(int lines) override;
    bool ScrollPages(int pages) override;

    WXDWORD   MSWGetStyle(long flags, WXDWORD *exstyle = nullptr) const override;
    WXLRESULT MSWWindowProc(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam) override;
#endif

private:
    MD3ScrollBars m_bars { this };
};

#endif // slic3r_GUI_MD3HtmlWindow_hpp_
