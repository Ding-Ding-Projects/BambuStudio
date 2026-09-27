#ifndef slic3r_GUI_SuperConfirmGate_hpp_
#define slic3r_GUI_SuperConfirmGate_hpp_

#include <array>
#include <functional>
#include <vector>

#include <wx/dialog.h>
#include <wx/weakref.h>

#include "MD3Motion.hpp"
#include "SuperConfirmState.hpp"

class Button;
class Label;
class SlideToConfirm;
class wxPanel;
class wxEventLoopBase;

namespace Slic3r { namespace GUI {

// Destructive-action super confirmation, in the app's own wx UI.
//
// A borderless Material surface anchored beside the control that asked for
// the destructive action (or centred on the parent as a modal fallback when
// there is no anchor). The caller supplies the exact action and the names of
// the affected items; the gate states both plainly, then demands:
//
//   1. both key switches turned (two independent focusable toggles, each
//      with its own accessible name, operated by click or Space/Enter);
//   2. the full-range SlideToConfirm completed - it is only enabled once
//      both keys are on, and a key turned back off drops it to rest.
//
// A hazard-striped charge bar animates with the slide (jumping under
// reduced motion) and a distinct check-burst plays once the slide lands.
// "Emergency exit" and Escape cancel at any moment before authorization
// and stop nothing else. Focus returns to the originating control on both
// cancel and completion. The action callback fires only when the pure
// state machine (SuperConfirmState.hpp) says both keys and the full slide
// have been completed - never from a click, a key press, or a close.
class SuperConfirmGate final : public wxDialog
{
public:
    struct Spec
    {
        // Imperative action label, e.g. "Delete preset". Shown as the title
        // and used for the slider instruction ("Slide to delete preset").
        wxString action;
        // The exact consequence in plain words: what is destroyed and that it
        // cannot be undone. Never softened by language mode or funny level.
        wxString consequence;
        // Names of the affected items. Long lists are truncated visually but
        // the total count is always stated.
        std::vector<wxString> affected;
        // Override the stated count when `affected` is only a sample (-1 =
        // use affected.size()).
        int affected_count { -1 };
        // Optional label shown on the slider once the slide completes.
        wxString completed_label;
    };

    // Blocking form: opens the gate, runs a nested event loop, and returns
    // true only when the user completed both keys and the full slide. With a
    // null anchor the gate runs as a centred modal dialog instead.
    static bool Run(wxWindow *anchor, const Spec &spec);

    // Callback form: shows the gate non-modally and invokes `on_confirm`
    // exactly once on authorization, or `on_cancel` (if given) on any other
    // way out. The gate destroys itself afterwards.
    static void Show(wxWindow *anchor, const Spec &spec,
                     std::function<void()> on_confirm,
                     std::function<void()> on_cancel = nullptr);

    // Read-only view of the arming state (for diagnostics / tests).
    const SuperConfirm::State &state() const { return m_state; }

private:
    SuperConfirmGate(wxWindow *anchor, wxWindow *parent, const Spec &spec);
    ~SuperConfirmGate() override;

    void build(const Spec &spec);
    void place_beside_anchor();
    void refresh_stage();
    void on_key_toggled(int index);
    void on_slide_progress(double fraction);
    void on_slide_complete();
    void finish(bool authorized);
    void restore_focus();
    wxString stage_text() const;

    SuperConfirm::State m_state;
    std::function<void()> m_on_confirm;
    std::function<void()> m_on_cancel;
    bool m_finished { false };
    bool m_nested_loop { false };
    wxEventLoopBase *m_loop { nullptr }; // nested loop owned by Run() while it blocks

    wxWeakRef<wxWindow> m_anchor;
    wxWeakRef<wxWindow> m_return_focus;

    std::array<wxWindow *, 2> m_keys { { nullptr, nullptr } };
    SlideToConfirm *m_slider { nullptr };
    wxPanel        *m_charge { nullptr };   // progress / completion canvas
    Label          *m_stage { nullptr };
    Button         *m_exit { nullptr };

    double m_charge_level { 0.0 };   // 0..1 ritual progress being drawn
    double m_burst { 0.0 };          // 0..1 completion burst radius
    double m_pulse { 0.0 };          // 0..1 hazard-stripe phase
    MD3::Motion::Anim m_charge_anim;
    MD3::Motion::Anim m_burst_anim;
    MD3::Motion::Anim m_pulse_anim;
};

} } // namespace Slic3r::GUI

#endif // slic3r_GUI_SuperConfirmGate_hpp_
