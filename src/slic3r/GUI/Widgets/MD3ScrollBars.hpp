#ifndef slic3r_GUI_MD3ScrollBars_hpp_
#define slic3r_GUI_MD3ScrollBars_hpp_

#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/window.h>

#include <functional>
#include <utility>

// The kit scrollbar (ui-md3 design-system/tokens/base.css) for a window that
// would otherwise show the Windows ones: a 10px strip with no track fill,
// holding a fully rounded thumb inset 2px, OutlineVariant at rest and Outline
// while hovered or dragged. With Windows high contrast on, the strip uses the
// system window and text colours instead.
//
// The owning window keeps its wx scroll helper (wxScrolledWindow's or
// wxVListBox's), which still computes the position, the page and the range and
// still handles the wheel, the keyboard and programmatic scrolling. The owner
// overrides the native scrollbar calls and forwards them here, so the helper's
// result is recorded instead of being handed to Windows:
//
//     void SetScrollbar(int o, int pos, int thumb, int range, bool refresh) override
//         { m_bars.SetScrollbar(o, pos, thumb, range, refresh); }
//     (SetScrollPos, GetScrollPos, GetScrollThumb, GetScrollRange likewise)
//     WXDWORD MSWGetStyle(long flags, WXDWORD *ex) const override
//         { return MD3ScrollBars::WithoutNativeBars(Base::MSWGetStyle(flags, ex)); }
//     WXLRESULT MSWWindowProc(WXUINT m, WXWPARAM w, WXLPARAM l) override
//     {
//         WXLRESULT result = 0;
//         if (m_bars.Before(m, w, l, result)) return result;
//         result = Base::MSWWindowProc(m, w, l);
//         m_bars.After(m, w, l, result);
//         return result;
//     }
//
// The owner must create its window from its own constructor body or Create(),
// not through a base-class constructor, so that MSWGetStyle() above already
// applies: Windows then never gets WS_HSCROLL/WS_VSCROLL and never draws a bar.
// Each kit bar that is needed reserves its strip in the non-client area, where
// a native bar sits, so the client area and every layout in it shrink as they
// did for a native bar (by 10px instead of 17px). Pressing the thumb drags it;
// pressing the track pages towards the pointer and repeats while held.
class MD3ScrollBars
{
public:
    explicit MD3ScrollBars(wxWindow *owner);
    ~MD3ScrollBars();

    MD3ScrollBars(const MD3ScrollBars &)            = delete;
    MD3ScrollBars &operator=(const MD3ScrollBars &) = delete;

    // Thickness of a strip in device pixels at ref's DPI, for code that sizes
    // a scrolled window to leave room for its bar.
    static int Thickness(const wxWindow *ref);

    // Whether the bar for orient (wxVERTICAL or wxHORIZONTAL) is shown.
    bool IsShown(int orient) const;

    // The same event a native bar sends, so the scroll helper moves the
    // content and every wxEVT_SCROLLWIN_* handler still hears about it.
    void SendScroll(int orient, wxEventType type, int pos = 0);

    // For an owner without a wx scroll helper (a multi-line text box scrolls
    // its Windows edit control itself): the strip's drags and pages go here,
    // as the same wxEVT_SCROLLWIN_* type and position, instead of as an event.
    using ScrollHandler = std::function<void(int orient, wxEventType type, int pos)>;
    void SetScrollHandler(ScrollHandler handler) { m_handler = std::move(handler); }

#ifdef __WXMSW__
    void SetScrollbar(int orient, int pos, int thumb, int range, bool refresh);
    void SetScrollPos(int orient, int pos, bool refresh);
    int  GetScrollPos(int orient) const;
    int  GetScrollThumb(int orient) const;
    int  GetScrollRange(int orient) const;

    static WXDWORD WithoutNativeBars(WXDWORD style);

    // Before the owner's window procedure: true when the message is answered
    // here (result holds the answer and the owner returns it at once).
    bool Before(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam, WXLRESULT &result);
    // After the owner's window procedure ran: reserve the strips, paint them,
    // claim them in hit testing.
    void After(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam, WXLRESULT &result);

    // Ends a drag or a held page without notifying anybody: for destructors.
    void Abandon();

private:
    struct Bar
    {
        int pos   { 0 };
        int thumb { 0 };
        int range { 0 };
    };
    struct Strips
    {
        wxRect v;      // right-hand strip, window coordinates
        wxRect h;      // bottom strip
        wxRect corner; // the square where both meet
    };

    Bar       &bar(int orient) { return orient == wxHORIZONTAL ? m_hbar : m_vbar; }
    const Bar &bar(int orient) const { return orient == wxHORIZONTAL ? m_hbar : m_vbar; }

    Strips  strips() const;
    wxRect  thumb_rect(int orient, const wxRect &strip) const;
    wxPoint screen_to_window(const wxPoint &screen) const;
    int     strip_at(const wxPoint &window_pt) const; // wxVERTICAL, wxHORIZONTAL, or 0
    int     thumb_at(const wxPoint &window_pt) const; // the bar whose thumb is under the point, or 0
    bool    in_strips(const wxPoint &window_pt) const;

    void paint();
    void paint_strip(wxDC &dc, int orient, const wxRect &strip, const wxColour &track);
    void frame_changed();
    void press(int orient, const wxPoint &window_pt);
    void drag_to(const wxPoint &window_pt);
    void page_once();
    void release(bool release_capture, bool notify);
    void set_hover(int orient);

    wxWindow     *m_owner;
    ScrollHandler m_handler;
    Bar       m_vbar;
    Bar       m_hbar;
    int       m_reserved_v { 0 };   // strip widths reserved by the last WM_NCCALCSIZE
    int       m_reserved_h { 0 };
    int       m_hover { 0 };        // bar whose thumb is under the pointer
    bool      m_tracking { false }; // TrackMouseEvent(TME_NONCLIENT) requested
    int       m_drag { 0 };         // bar whose thumb is being dragged
    int       m_drag_origin { 0 };  // pointer position along the bar when the drag began
    int       m_drag_pos { 0 };     // scroll position when the drag began
    int       m_page { 0 };         // bar being paged by holding the track
    int       m_page_dir { 0 };     // -1 towards the start, +1 towards the end
    bool      m_releasing { false };
#else
private:
    wxWindow     *m_owner;
    ScrollHandler m_handler;
#endif // __WXMSW__
};

#endif // slic3r_GUI_MD3ScrollBars_hpp_
