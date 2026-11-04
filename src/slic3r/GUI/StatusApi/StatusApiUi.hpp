#pragma once
// Local status API, the rows of its Preferences section (docs/features/device/status-api.md).

class wxWindow;

namespace Slic3r { namespace GUI { namespace StatusApi {

// One sentence that says what the API offers and that it is read-only and local.
wxWindow *create_intro_row(wxWindow *parent);
// The listener state, refreshed whenever the switch, the port or the token changes.
wxWindow *create_status_row(wxWindow *parent);
// The bearer token: masked, with Show, Copy and Regenerate.
wxWindow *create_token_row(wxWindow *parent);

}}} // namespace Slic3r::GUI::StatusApi
