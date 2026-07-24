#ifndef slic3r_GUI_SlideToConfirm_hpp_
#define slic3r_GUI_SlideToConfirm_hpp_

#include <functional>

#include <wx/window.h>

#include "MD3Tokens.hpp"

// Material Design 3 "slide to confirm" gate for destructive or
// secret-revealing actions (config export / import). The user drags the
// knob across the track (or walks it with the arrow keys); only a
// completed slide arms the guarded action, so a stray click can never
// trigger it.
//
// Anatomy: a 44px stadium track (SurfaceContainerHighest fill, 1px Outline
// border; ErrorContainer tints while sliding when `danger` styling is on),
// a centred instruction label, and a circular Primary knob carrying a
// double-chevron glyph. Completing the slide fills the track with
// PrimaryContainer, docks the knob right, swaps the label for the
// confirmed text and fires the callback once.
//
// Keyboard: the control is focusable; Right/Up advances, Left/Down
// retreats, End completes, Home resets. This keeps the gate reachable
// without a pointer while preserving the deliberate multi-step gesture.
class SlideToConfirm : public wxWindow
{
public:
    SlideToConfirm(wxWindow *parent,
                   const wxString &instruction,
                   const wxString &confirmed_label);

    // Fired exactly once each time the slide completes.
    void SetOnConfirm(std::function<void()> cb) { m_on_confirm = std::move(cb); }

    bool IsConfirmed() const { return m_confirmed; }
    // Return to the unarmed state (e.g. after the guarded action ran or failed).
    void Reset();

    // Error-container styling for secret-revealing gates.
    void SetDangerStyle(bool on) { m_danger = on; Refresh(); }

    void Rescale();

protected:
    void OnPaint(wxPaintEvent &event);
    void OnMouse(wxMouseEvent &event);
    void OnKey(wxKeyEvent &event);
    void OnFocus(wxFocusEvent &event);
    wxSize DoGetBestSize() const override;

private:
    int  trackWidth() const;
    int  knobDiameter() const;
    int  maxTravel() const;
    void complete();

    wxString m_instruction;
    wxString m_confirmed_label;
    std::function<void()> m_on_confirm;

    int  m_pos { 0 };          // knob travel in px from the left dock
    bool m_dragging { false };
    int  m_drag_grab_dx { 0 };
    bool m_confirmed { false };
    bool m_danger { true };
};

#endif // slic3r_GUI_SlideToConfirm_hpp_
