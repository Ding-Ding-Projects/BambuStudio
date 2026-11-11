#ifndef slic3r_GUI_DoorChimeService_hpp_
#define slic3r_GUI_DoorChimeService_hpp_

// Plays the door chimes on the selected printer. install() starts a timer that
// watches the door reading of the selected printer and sends the open or close
// chime chosen in the Door chime card when the reading changes; preview() sends
// one chime straight away for the card's Preview buttons.

#include "DoorChime.hpp"

#include <string>

namespace Slic3r { namespace GUI { namespace DoorChime {

void install();

bool        enabled();
void        set_enabled(bool enabled);
std::string chosen_id(bool open);
void        set_chosen_id(bool open, const std::string &id);

// Why a chime can or cannot play on the selected printer right now.
enum class Readiness { Ready, NoPrinter, NoDoorSensor, Printing };
Readiness readiness();

// Sends the chime to the selected printer. Returns the readiness that decided
// it; the chime was sent only for Ready (a silent chime sends nothing).
Readiness preview(bool open, const std::string &id);

}}} // namespace Slic3r::GUI::DoorChime

#endif // slic3r_GUI_DoorChimeService_hpp_
