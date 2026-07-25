#ifndef slic3r_GUI_MD3DialogChrome_hpp_
#define slic3r_GUI_MD3DialogChrome_hpp_

#include <wx/panel.h>

class wxDialog;
class Label;

// Material Design 3 dialog caption, replacing the native Windows title bar.
//
// Owned dialogs create themselves borderless (wxBORDER_NONE, no
// wxDEFAULT_DIALOG_STYLE caption) and add this 44px strip as the first item
// of their root sizer:
//   * Head_14 title (mnemonic-safe via SetLabelText), announced to screen
//     readers through the panel name;
//   * a trailing close button with the Material `close` glyph and a >=44px
//     hit target, keyboard reachable (Tab + Enter/Space), closing with
//     wxID_CANCEL exactly like the native X;
//   * the whole strip is a drag region (WM_NCLBUTTONDOWN/HTCAPTION on MSW);
//   * DWM rounded corners are requested for the borderless frame, and the
//     dialog fades in through MD3::Motion (jump under reduced motion).
// Esc keeps closing through the dialog's normal wxID_CANCEL routing.
class MD3DialogCaption : public wxPanel
{
public:
    MD3DialogCaption(wxDialog *dialog, const wxString &title);

    // One-call adoption: request rounded corners + play the entrance fade.
    // Call after the dialog's sizer is set (typically right before Show).
    static void FinishChrome(wxDialog *dialog);

    // Full one-call adoption for dialogs that cannot change base class:
    // strips the native caption styles in place, wraps the dialog's existing
    // root sizer under a caption strip, restores the content's client height,
    // and finishes the chrome. Call as the LAST layout act of the ctor (after
    // Fit/SetSizeHints; move CenterOnParent after it when present). An empty
    // title falls back to the dialog's window title. wxRESIZE_BORDER, when
    // present, is preserved so edge-resizing keeps working.
    static void Adopt(wxDialog *dialog, const wxString &title = wxString());

private:
    void OnPaintClose(wxPaintEvent &event);

    wxDialog *m_dialog { nullptr };
    Label    *m_title { nullptr };
    wxPanel  *m_close { nullptr };
    bool      m_close_hover { false };
};

#endif // slic3r_GUI_MD3DialogChrome_hpp_
