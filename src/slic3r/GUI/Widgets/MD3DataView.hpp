#ifndef slic3r_GUI_MD3DataView_hpp_
#define slic3r_GUI_MD3DataView_hpp_

#include <wx/dataview.h>

#include "MD3ScrollBars.hpp"

// wxDataViewCtrl and wxDataViewListCtrl with the kit scrollbar (MD3ScrollBars)
// instead of the Windows one. The table's own scroll helper still scrolls the
// rows and the header; only where its bars are drawn changes. Tables take the
// Material table style from md3_style_data_view() as before.
//
// Each creates its window from its own constructor body, so a subclass passes
// the usual arguments to the MD3 constructor in its initialiser list.
class MD3DataViewCtrl : public wxDataViewCtrl
{
public:
    MD3DataViewCtrl();
    MD3DataViewCtrl(wxWindow           *parent,
                    wxWindowID          id,
                    const wxPoint      &pos       = wxDefaultPosition,
                    const wxSize       &size      = wxDefaultSize,
                    long                style     = 0,
                    const wxValidator  &validator = wxDefaultValidator,
                    const wxString     &name      = wxASCII_STR(wxDataViewCtrlNameStr));
    ~MD3DataViewCtrl() override;

    // Whether the kit bar for orient (wxVERTICAL or wxHORIZONTAL) is shown.
    bool IsBarShown(int orient) const { return m_bars.IsShown(orient); }

#ifdef __WXMSW__
    void      SetScrollbar(int orient, int pos, int thumbVisible, int range, bool refresh = true) override;
    void      SetScrollPos(int orient, int pos, bool refresh = true) override;
    int       GetScrollPos(int orient) const override;
    int       GetScrollThumb(int orient) const override;
    int       GetScrollRange(int orient) const override;
    WXDWORD   MSWGetStyle(long flags, WXDWORD *exstyle = nullptr) const override;
    WXLRESULT MSWWindowProc(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam) override;
#endif

private:
    MD3ScrollBars m_bars { this };
};

class MD3DataViewListCtrl : public wxDataViewListCtrl
{
public:
    MD3DataViewListCtrl();
    MD3DataViewListCtrl(wxWindow          *parent,
                        wxWindowID         id,
                        const wxPoint     &pos       = wxDefaultPosition,
                        const wxSize      &size      = wxDefaultSize,
                        long               style     = wxDV_ROW_LINES,
                        const wxValidator &validator = wxDefaultValidator);
    ~MD3DataViewListCtrl() override;

    // Whether the kit bar for orient (wxVERTICAL or wxHORIZONTAL) is shown.
    bool IsBarShown(int orient) const { return m_bars.IsShown(orient); }

#ifdef __WXMSW__
    void      SetScrollbar(int orient, int pos, int thumbVisible, int range, bool refresh = true) override;
    void      SetScrollPos(int orient, int pos, bool refresh = true) override;
    int       GetScrollPos(int orient) const override;
    int       GetScrollThumb(int orient) const override;
    int       GetScrollRange(int orient) const override;
    WXDWORD   MSWGetStyle(long flags, WXDWORD *exstyle = nullptr) const override;
    WXLRESULT MSWWindowProc(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam) override;
#endif

private:
    MD3ScrollBars m_bars { this };
};

#endif // slic3r_GUI_MD3DataView_hpp_
