#ifndef slic3r_GUI_SuperConfirmState_hpp_
#define slic3r_GUI_SuperConfirmState_hpp_

#include <algorithm>

// Pure, toolkit-free state machine behind the destructive-action super
// confirmation gate (Widgets/SuperConfirmGate). Keeping the arming rules here
// lets tests/super_confirm exercise every path without wxWidgets:
//
//   * two independent keys must BOTH be on before the slider is enabled;
//   * the slider only counts when it reaches 100 percent while both keys are
//     still on; a partial slide never arms anything;
//   * authorization is a one-shot latch that cancel/escape cannot reach once
//     it has been granted, and that can never be reached after a cancel.
//
// The widget owns the animation and focus behaviour; this struct owns the
// single question "may the destructive action fire?".
namespace Slic3r { namespace GUI { namespace SuperConfirm {

enum class Phase {
    Untouched,   // nothing turned, slider disabled
    OneKey,      // exactly one key on, slider still disabled
    Armed,       // both keys on, slider enabled and at rest
    Sliding,     // both keys on, slider partially travelled
    Authorized,  // both keys on and the slide completed: the action may fire
    Cancelled,   // emergency exit / Escape / dismissed: the action never fires
};

struct State
{
    bool key_a { false };
    bool key_b { false };
    int  slider_percent { 0 }; // 0..100
    bool authorized { false };
    bool cancelled { false };

    bool both_keys() const { return key_a && key_b; }

    // The slider is operable only while both keys are on and the gate is
    // neither authorized nor cancelled.
    bool slider_enabled() const { return both_keys() && !authorized && !cancelled; }

    // Toggle key 0 or 1. Turning a key off while the slider is partially
    // travelled drops the slider back to rest: an arming gesture cannot
    // survive the disarming of the thing that enabled it. Returns the new
    // key state. Ignored after authorization or cancel.
    bool toggle_key(int index)
    {
        if (authorized || cancelled)
            return index == 0 ? key_a : key_b;
        bool &key = index == 0 ? key_a : key_b;
        key = !key;
        if (!both_keys())
            slider_percent = 0;
        return key;
    }

    // Move the slider to `percent` (clamped 0..100). Returns false when the
    // slider is not enabled, in which case the position is unchanged. A value
    // of 100 authorizes the gate; anything less is only "sliding".
    bool set_slider(int percent)
    {
        if (!slider_enabled())
            return false;
        slider_percent = std::clamp(percent, 0, 100);
        if (slider_percent >= 100)
            authorized = true;
        return true;
    }

    // Release the slider without reaching the end: snap back to rest.
    void release_slider()
    {
        if (!authorized)
            slider_percent = 0;
    }

    // Emergency exit or Escape. Once authorized the gate is spent and a late
    // cancel changes nothing; before that it permanently closes the gate.
    void cancel()
    {
        if (!authorized)
            cancelled = true;
    }

    // The one predicate the widget consults before invoking the caller's
    // destructive callback.
    bool may_fire() const { return authorized && both_keys() && slider_percent >= 100 && !cancelled; }

    Phase phase() const
    {
        if (cancelled)
            return Phase::Cancelled;
        if (authorized)
            return Phase::Authorized;
        if (!both_keys())
            return (key_a || key_b) ? Phase::OneKey : Phase::Untouched;
        return slider_percent > 0 ? Phase::Sliding : Phase::Armed;
    }

    // Progress 0..1 through the whole ritual, for the dramatic-but-honest
    // progress animation: each key is worth a quarter, the slide the rest.
    double ritual_progress() const
    {
        double p = 0.0;
        if (key_a) p += 0.25;
        if (key_b) p += 0.25;
        if (both_keys()) p += 0.5 * (slider_percent / 100.0);
        return std::clamp(p, 0.0, 1.0);
    }
};

} } } // namespace Slic3r::GUI::SuperConfirm

#endif // slic3r_GUI_SuperConfirmState_hpp_
