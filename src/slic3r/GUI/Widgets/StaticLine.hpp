#ifndef slic3r_GUI_StaticLine_hpp_
#define slic3r_GUI_StaticLine_hpp_

#include "../wxExtensions.hpp"
#include "wx/window.h"

class StaticLine : public wxWindow
{
public:
    StaticLine(wxWindow *parent, bool vertical = false, const wxString &label = {}, const wxString &icon = {});

public:
    void SetLabel(const wxString& label) override;

    void SetIcon(const wxString& icon);

    void SetLineColour(wxColour color);
    
    void Rescale();

private:
    wxColour       lineColor;
    bool vertical;
    ScalableBitmap icon;

    // Bilingual mode bookkeeping: the tooltip the caller actually asked for,
    // and the Cantonese note (if any) currently folded on top of it.
    wxString bilingual_base_tooltip;
    wxString bilingual_note;

private:
    void paintEvent(wxPaintEvent& evt);

    void messureSize();

    void render(wxDC &dc);

    // Bilingual mode needs to fold "廣東話：..." into whatever tooltip the
    // caller sets, without losing it on the next repaint -- so this virtual
    // hook (which SetToolTip() itself funnels through) captures the caller's
    // text and re-merges it with the current note (see render()).
    void DoSetToolTipText(wxString const &tip) override;

    DECLARE_EVENT_TABLE()
};

#endif // !slic3r_GUI_StaticLine_hpp_
