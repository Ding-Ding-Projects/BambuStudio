#ifndef slic3r_GUI_HomeAssistantSettingsSync_hpp_
#define slic3r_GUI_HomeAssistantSettingsSync_hpp_

// Settings sync to Home Assistant (issue #16): the runtime half. When the
// opt-in `ha_settings_sync` switch in the Smart home dialog is on, the app's
// user settings (the `app` section of BambuStudio.conf, minus the deny-list
// in HomeAssistantSettingsSyncModel.hpp) are published as Home Assistant
// state entities through the existing Home Assistant client:
//
//   * on every AppConfig save, debounced 5 s so a burst of edits is one push;
//   * on a 15 minute interval, so a restarted Home Assistant gets them back;
//   * on demand from the dialog's "Sync now" action.
//
// Failures are non-blocking (a warning toast with a Retry action, once per
// failure streak) and never touch the settings themselves. Tokens, URLs,
// paths and credentials never leave the machine: the deny-list runs on every
// key and every value, and the transport refuses a payload that fails it.

#include <functional>
#include <string>

namespace Slic3r { namespace GUI { namespace HomeAssistant { namespace SettingsSync {

enum class State { Off, Idle, Syncing, Synced, Failed };

struct Status
{
    State       state = State::Off;
    std::string last_synced_at; // local ISO-8601, empty until the first success
    std::string last_error;     // plain words, empty when the last push succeeded
    int         entities_ok    = 0;
    int         entities_total = 0;
};

// Chain onto the AppConfig save observer and start the interval timer when
// enabled. Main thread, after the config is loaded. Idempotent.
void install();
void shutdown();

bool enabled();
// Persist the switch; turning it on pushes immediately.
void set_enabled(bool on);
// Push now (dialog action or toast Retry). No-op while a push is in flight.
void sync_now();

Status status();
// The dialog subscribes so its status line follows every state change.
void set_status_listener(std::function<void()> listener);

} } } } // namespace Slic3r::GUI::HomeAssistant::SettingsSync

#endif // slic3r_GUI_HomeAssistantSettingsSync_hpp_
