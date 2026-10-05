#ifndef slic3r_GUI_MD3ScrolledWindow_hpp_
#define slic3r_GUI_MD3ScrolledWindow_hpp_

#include <wx/scrolwin.h>
#include <wx/weakref.h>

#include "MD3ScrollBars.hpp"

// A wxScrolledWindow whose scrollbars are the kit scrollbar (MD3ScrollBars)
// instead of the Windows ones. wx's scroll helper keeps doing all of the work
// (position, page, range, the wheel, the keyboard, Scroll() and scrolling a
// focused child into view); only where its result is drawn changes.
//
// Use it wherever a wxScrolledWindow was used: the constructor and Create()
// take the same arguments, and a wxScrolledWindow pointer can hold it.
class MD3ScrolledWindow : public wxScrolledWindow
{
public:
    MD3ScrolledWindow();
    MD3ScrolledWindow(wxWindow        *parent,
                      wxWindowID       id    = wxID_ANY,
                      const wxPoint   &pos   = wxDefaultPosition,
                      const wxSize    &size  = wxDefaultSize,
                      long             style = wxScrolledWindowStyle,
                      const wxString  &name  = wxASCII_STR(wxPanelNameStr));
    ~MD3ScrolledWindow() override;

    bool Create(wxWindow        *parent,
                wxWindowID       id    = wxID_ANY,
                const wxPoint   &pos   = wxDefaultPosition,
                const wxSize    &size  = wxDefaultSize,
                long             style = wxScrolledWindowStyle,
                const wxString  &name  = wxASCII_STR(wxPanelNameStr));

    // Thickness of a kit scrollbar strip in device pixels at ref's DPI, for
    // code that sizes a scrolled window to leave room for its bar.
    static int BarThickness(const wxWindow *ref) { return MD3ScrollBars::Thickness(ref); }

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

    void SetRevealOwner(MD3ScrolledWindow *owner);
    void RevealChild(wxWindow *child);
    bool ShouldScrollToChildOnFocus(wxWindow *child) override;

private:
    void OnMouseWheel(wxMouseEvent &event);
    wxWeakRef<MD3ScrolledWindow> m_reveal_owner;
    MD3ScrollBars m_bars { this };
};

#endif // slic3r_GUI_MD3ScrolledWindow_hpp_
