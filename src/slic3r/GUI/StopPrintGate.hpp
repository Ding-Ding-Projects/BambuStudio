#ifndef slic3r_GUI_StopPrintGate_hpp_
#define slic3r_GUI_StopPrintGate_hpp_

#include <array>
#include <functional>

#include <wx/dialog.h>

class Button;
class Label;
class SlideToConfirm;
class wxPanel;

namespace Slic3r::GUI {

// Launch-console style interlock for stopping a print. The stages must be
// completed in order and every stage is labelled, so the flow is theatrical
// but never unclear about what it does:
//   1. turn BOTH key switches (click rotates the painted key);
//   2. press each of the three arming buttons twice (live countdown on the
//      button face);
//   3. complete the slide-to-confirm;
//   4. the safety cover flips open and reveals the actual STOP button.
// Only the revealed STOP button fires the callback. Closing the dialog in
// any other way stops nothing.
class StopPrintGateDialog final : public wxDialog
{
public:
    explicit StopPrintGateDialog(wxWindow *parent);

    void SetOnStopConfirmed(std::function<void()> cb) { m_on_stop = std::move(cb); }

private:
    void update_stage();

    std::function<void()> m_on_stop;
    std::array<bool, 2> m_keys { { false, false } };
    std::array<int, 3>  m_presses { { 0, 0, 0 } };
    bool m_slid { false };
    bool m_cover_open { false };

    std::array<wxPanel *, 2> m_key_switches { { nullptr, nullptr } };
    std::array<Button *, 3>  m_arm_buttons { { nullptr, nullptr, nullptr } };
    SlideToConfirm *m_slider { nullptr };
    wxPanel        *m_cover { nullptr };
    Button         *m_stop { nullptr };
    Label          *m_stage_label { nullptr };
};

} // namespace Slic3r::GUI

#endif // slic3r_GUI_StopPrintGate_hpp_
