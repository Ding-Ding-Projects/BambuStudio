#ifndef slic3r_GUI_MD3DialogChrome_hpp_
#define slic3r_GUI_MD3DialogChrome_hpp_

#include <wx/panel.h>
#include "MD3Motion.hpp"

class wxDialog;
class Label;

// One explicitly owned top-level entrance. Hide/reopen replaces the previous
// run; no close action or input state waits for the visual transition.
class MD3TransientEntrance : public wxEvtHandler
{
public:
    ~MD3TransientEntrance() override;
    void Show(wxWindow *owner, int duration_ms);
    void Stop();
private:
    void Begin(int duration_ms);
    void Restore();
    MD3::Motion::Anim m_anim;
    wxWeakRef<wxWindow> m_owner;
    void *m_native_handle = nullptr;
    uint64_t m_generation = 0;
};

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

    // Request rounded corners. Caption instances own show/reopen motion.
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

    // Make every caption of `dialog` show the dialog's current window title.
    // A dialog that calls SetTitle() after adoption changed the window text
    // only; the strip kept its first title. A caption adopted with no title
    // follows the title by itself, on idle; call this when the new title must
    // show in the same paint as the change.
    static void SyncTitle(wxDialog *dialog);

    // Caption strip height in device pixels, for owner-drawn dialogs that
    // paint content at absolute client coordinates.
    static int Height(wxWindow *ref);

private:
    void OnPaintClose(wxPaintEvent &event);
    void FollowDialogTitle();

    void OnDialogShow(wxShowEvent &event);

    wxDialog *m_dialog { nullptr };
    Label    *m_title { nullptr };
    wxPanel  *m_close { nullptr };
    bool      m_close_hover { false };
    // The title the strip was last given; compared instead of the label text,
    // which the bilingual decorator may pair.
    wxString  m_applied_title;
    // True for a caption created from the dialog's own title (Adopt with no
    // literal): it keeps following that title.
    bool      m_follow_title { false };

    MD3TransientEntrance m_entrance;
};

#endif // slic3r_GUI_MD3DialogChrome_hpp_
