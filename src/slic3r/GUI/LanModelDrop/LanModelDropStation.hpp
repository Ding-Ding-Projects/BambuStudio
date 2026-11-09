#pragma once
// LAN model drop, station side (docs/features/application-integration/lan-model-drop.md).
//
// While the Preferences switch is on, a worker thread polls the drop container on the local network
// downloads each received model into the data folder, checks its size,
// SHA-256 and type, and hands it to the GUI thread, which shows a non-blocking notification with
// Open and Discard. Nothing is opened, sliced or printed automatically. Every function below is
// called on the GUI thread.

#include "LanModelDropModel.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <wx/string.h>

class wxWindow;

namespace Slic3r { namespace GUI { namespace LanModelDrop {

// Application settings keys.
constexpr const char *kEnabledConfigKey    = "lan_drop_enabled";
constexpr const char *kAddressConfigKey    = "lan_drop_address";
// The LAN address the user picked for the invite link when this computer has several.
constexpr const char *kInviteIpv4ConfigKey = "lan_drop_invite_ipv4";

// What the Preferences section, the invite dialog and the title-bar indicator show.
struct View
{
    bool          enabled = false;
    bool          direct_send = false;
    LinkState     state   = LinkState::Off;
    std::string   address;       // the configured address, normalized when it parses
    bool          address_valid = true;
    std::string   station_name;
    std::string   public_url;    // the container's DROP_PUBLIC_URL, normalized; may be empty
    bool          busy_test  = false; // a Test connection is in flight
    bool          tested     = false; // a Test connection finished since the view was opened
    LinkState     test_state = LinkState::Off;
    int           waiting    = 0;     // received files waiting for Open or Discard
    std::uint64_t queued     = 0;     // files the container still holds
    InviteBase    invite;             // base of the invite link and the LAN choices
    std::string   link;               // sender address, empty until connected
};

// Starts polling after startup when the switch is on (GUI_App::post_init). Removes received files
// that were never opened in an earlier session; the container still holds them, so they arrive again.
void start_after_startup();
// Re-reads the switch and the address, and starts or stops polling.
void apply_settings();
// Stops the worker before the windows are destroyed: sets the cancel flag, wakes it, aborts any
// transfer in flight and joins it. Callbacks still queued for the GUI thread are dropped.
void shutdown();
// After the main window was rebuilt (language change): notifications of files still waiting are
// shown again in the new window.
void after_gui_rebuild();

const View &view();
// Listeners are called on the GUI thread whenever the view may have changed.
int  add_listener(std::function<void()> listener);
void remove_listener(int id);

void test_connection();
// Shows the notifications of every file still waiting for Open or Discard again.
void show_waiting_again();
// Re-reads this computer's network adapters for the invite link.
void refresh_lan_addresses();
// The LAN address the invite link uses when there are several.
void choose_invite_ipv4(const std::string &ipv4);

// File > LAN model drop..., the palette and the indicator land here: Preferences on the section.
void open_preferences_section();
// File > Invite someone to send a model..., the palette and the indicator: the invite dialog.
void open_invite_dialog(wxWindow *parent);

// Translated texts shared by every surface.
wxString state_text(LinkState state);
wxString status_text(const View &view);
wxString invite_source_text(const View &view);

}}} // namespace Slic3r::GUI::LanModelDrop
