#ifndef slic3r_GUI_MD3Grid_hpp_
#define slic3r_GUI_MD3Grid_hpp_

#include <wx/grid.h>

#include "MD3ScrollBars.hpp"

// A wxGrid whose scrollbars are the kit scrollbar (MD3ScrollBars) instead of
// the Windows ones. wxGrid is a wxScrolledCanvas, so this is MD3ScrolledWindow's
// arrangement: wx keeps computing the rows and columns that are visible and
// keeps doing all of the scrolling (the wheel, the keyboard, dragging a
// selection past the edge, Scroll(), MakeCellVisible()); only where its bars
// are drawn changes. The row and column labels, the cells and the editors are
// child windows laid out in the client area, which shrinks by the width of a
// kit strip where a Windows bar used to be.
//
// Use it wherever a wxGrid was used: the constructor and Create() take the same
// arguments, and a wxGrid pointer can hold it. The window is created from this
// class's own constructor body, so a subclass passes the usual arguments to the
// MD3Grid constructor in its initialiser list.
class MD3Grid : public wxGrid
{
public:
    MD3Grid();
    MD3Grid(wxWindow        *parent,
            wxWindowID       id    = wxID_ANY,
            const wxPoint   &pos   = wxDefaultPosition,
            const wxSize    &size  = wxDefaultSize,
            long             style = wxWANTS_CHARS,
            const wxString  &name  = wxASCII_STR(wxGridNameStr));
    ~MD3Grid() override;

    bool Create(wxWindow        *parent,
                wxWindowID       id    = wxID_ANY,
                const wxPoint   &pos   = wxDefaultPosition,
                const wxSize    &size  = wxDefaultSize,
                long             style = wxWANTS_CHARS,
                const wxString  &name  = wxASCII_STR(wxGridNameStr));

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

#endif // slic3r_GUI_MD3Grid_hpp_
