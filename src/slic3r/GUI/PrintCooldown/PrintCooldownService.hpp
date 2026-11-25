#ifndef slic3r_GUI_PrintCooldownService_hpp_
#define slic3r_GUI_PrintCooldownService_hpp_

// Runs the cool-down on the selected printer. install() starts a timer that
// watches the print status of the selected printer; when a print ends it turns
// every fan to full, then turns them off once the bed and chamber are cool
// enough or after five minutes. The Cool down card reads state() to say what
// is happening.

#include "PrintCooldown.hpp"

namespace Slic3r { namespace GUI { namespace PrintCooldown {

void install();

bool enabled();
void set_enabled(bool enabled);

struct State
{
    bool  connected    = false; // a printer is selected and connected
    bool  blocked      = false; // fan control is locked for this printer
    Phase phase        = Phase::Idle;
    int   remaining_s  = 0;     // seconds left before the timeout while cooling
    float bed_c        = 0.0f;
    float chamber_c    = 0.0f;  // zero or less: no chamber reading
};

State state();

}}} // namespace Slic3r::GUI::PrintCooldown

#endif // slic3r_GUI_PrintCooldownService_hpp_
