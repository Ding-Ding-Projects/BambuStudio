#pragma once
// Local, read-only printer status API: service entry points (docs/features/device/status-api.md).
// Every function is called on the GUI thread. The pure rules live in StatusApiCore.hpp.

#include <string>

#include <wx/string.h>

namespace Slic3r { namespace GUI { namespace StatusApi {

enum class State { Off, Listening, NoToken, PortInUse };

// Reads the switch, the port and the token from the application settings and starts, restarts or
// stops the loopback listener. Safe to call at any time.
void apply_settings();
// GUI_App::post_init: starts the listener when the switch is on.
void start_after_startup();
// Stops the listener and the snapshot timer before the windows are destroyed.
void shutdown();

State current_state();
int   current_port();

// Makes a new random token (32 bytes from the OpenSSL generator, as 64 hexadecimal characters),
// stores it in the application settings and returns it. The old token stops working at once.
std::string regenerate_token();
// The stored token, generating one when none is stored. Never written to a log.
std::string ensure_token();

wxString state_text(State state);

}}} // namespace Slic3r::GUI::StatusApi
