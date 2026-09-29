#ifndef slic3r_GUI_StaticGroup_hpp_
#define slic3r_GUI_StaticGroup_hpp_

#include "../wxExtensions.hpp"

#include <wx/statbox.h>

class StaticGroup : public wxStaticBox
{
public:
    StaticGroup(wxWindow *parent, wxWindowID id);

public:
    void ShowBadge(bool show);
    void SetBorderColor(const wxColour &color);
    bool Show(bool show=true) override;
private:
#ifdef __WXMSW__
    void OnPaint(wxPaintEvent &evt);
    void PaintForeground(wxDC &dc, const struct tagRECT &rc) override;
#endif

private:
#ifdef __WXMSW__
    ScalableBitmap badge;
#endif
#ifdef __WXOSX__
    ScalableButton * badge { nullptr };
#endif
    wxColour       borderColor_;
};

// The Material group box, a drop-in for wxStaticBox wherever a wxStaticBoxSizer
// groups controls: a 1 px OutlineVariant outline with small rounded corners and
// the title in the kit's small title face, instead of the Windows group frame and
// caption in the system font. Everything wxStaticBoxSizer relies on is the
// native box's, so the controls inside lay out exactly as before.
class MD3GroupBox : public wxStaticBox
{
public:
    MD3GroupBox(wxWindow *parent, const wxString &label = wxEmptyString);

private:
#ifdef __WXMSW__
    void OnPaint(wxPaintEvent &evt);
    void PaintForeground(wxDC &dc, const struct tagRECT &rc) override;
#endif
};

#endif // !slic3r_GUI_StaticGroup_hpp_
