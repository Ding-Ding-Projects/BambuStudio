#ifndef slic3r_GUI_Appearance_ThemeFollowWatcher_hpp_
#define slic3r_GUI_Appearance_ThemeFollowWatcher_hpp_

// Follow Windows in the running app: reads whether Windows wants dark apps,
// sets the look before the first window at startup, then looks again every
// three seconds while following is on and switches the whole app the moment
// Windows switches. The rules live in ThemeFollow.

#include <optional>

namespace Slic3r::GUI::ThemeFollow {

// Whether Windows wants dark apps (AppsUseLightTheme); nothing on other systems.
std::optional<bool> windows_prefers_dark();
// At startup, before the look is first applied: writes dark_color_mode from
// Windows while following. Nothing on screen yet, so nothing to repaint.
void apply_at_startup();
// Starts the watcher (once); it does nothing while following is off.
void start_watching();
void stop_watching();
// Switches the running app to dark or light, as the theme row does.
void apply_now(bool dark);

} // namespace Slic3r::GUI::ThemeFollow

#endif // slic3r_GUI_Appearance_ThemeFollowWatcher_hpp_
