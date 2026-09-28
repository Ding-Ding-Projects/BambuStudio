
#ifndef _WX_ERRORMSGSTATTEXT_H_
#define _WX_ERRORMSGSTATTEXT_H_

#include <wx/panel.h>
#include "wx/stattext.h"

class WXDLLIMPEXP_CORE ErrorMsgStaticText : public wxPanel
{
public:
    wxString m_msg;
    ErrorMsgStaticText();
    ErrorMsgStaticText(wxWindow *parent,
                 wxWindowID id = wxID_ANY,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxSize(0,0));

    void paintEvent(wxPaintEvent &evt);

    // A real override of wxWindow::SetLabel (the previous by-value overload
    // only hid it, so a caller holding this as a wxWindow* skipped m_msg
    // entirely) that also repaints, matching every other kit SetLabel.
    void SetLabel(const wxString &msg) override;

protected:
    // Bilingual mode needs to fold "廣東話：..." into whatever tooltip the
    // caller sets, without losing it on the next repaint -- so this virtual
    // hook (which SetToolTip() itself funnels through) captures the caller's
    // text and re-merges it with the current note (see paintEvent).
    void DoSetToolTipText(wxString const &tip) override;

private:
    // Bilingual mode bookkeeping: the tooltip the caller actually asked for,
    // and the Cantonese note (if any) currently folded on top of it.
    wxString m_bilingual_base_tooltip;
    wxString m_bilingual_note;
};
#endif