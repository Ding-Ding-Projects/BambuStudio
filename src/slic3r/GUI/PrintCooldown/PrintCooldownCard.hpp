#ifndef slic3r_GUI_PrintCooldownCard_hpp_
#define slic3r_GUI_PrintCooldownCard_hpp_

// The Cool down card on the Device page: a switch for cooling the printer down
// after a print, and a line saying what is happening now (waiting for a print
// to end, fans running with the time left and the temperatures, cooled, or
// stopped at the five minute limit).

#include "../Widgets/StaticBox.hpp"

#include <wx/timer.h>

class Label;
class SwitchButton;

namespace Slic3r { namespace GUI {

class PrintCooldownCard : public StaticBox
{
public:
    explicit PrintCooldownCard(wxWindow *parent);

private:
    void refresh_state();

    SwitchButton *m_switch = nullptr;
    Label        *m_note   = nullptr;
    wxTimer       m_refresh;
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_PrintCooldownCard_hpp_
