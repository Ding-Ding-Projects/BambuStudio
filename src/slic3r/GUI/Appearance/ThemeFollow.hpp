#ifndef slic3r_GUI_Appearance_ThemeFollow_hpp_
#define slic3r_GUI_Appearance_ThemeFollow_hpp_

// Light, Dark or Follow Windows (Preferences, Appearance; Help > Set up Bambu
// Studio, Look). dark_color_mode keeps holding the look in effect ("1" dark),
// so everything that reads it keeps working; theme_follow_windows ("1") says
// Windows decides it, at startup and whenever Windows switches between light
// and dark apps. The rules are here, without wxWidgets
// (tests/theme_follow_test.cpp); ThemeFollowWatcher reads Windows and applies.

#include <optional>
#include <string>
#include <string_view>

namespace Slic3r::GUI::ThemeFollow {

enum class Choice { Light, Dark, FollowWindows };

constexpr const char *kFollowKey = "theme_follow_windows";

Choice choice_from(std::string_view dark_color_mode, std::string_view follow);

struct Stored
{
    std::string dark_color_mode; // "0" or "1"
    std::string follow;          // "0" or "1"
};
// What to write for a choice. Following uses what Windows prefers now; when
// Windows does not say (another system, or no such setting), the look in
// effect stays and following still starts.
Stored store(Choice choice, std::optional<bool> windows_dark, std::string_view current_dark);

// Windows' AppsUseLightTheme value: 0 means apps are dark.
std::optional<bool> windows_dark_from(std::optional<long> apps_use_light_theme);

// The look to switch to now, or nothing: only while following, only when
// Windows says, and only when it differs from the look in effect.
std::optional<bool> follow_change(std::string_view dark_color_mode, std::string_view follow, std::optional<bool> windows_dark);

} // namespace Slic3r::GUI::ThemeFollow

#endif // slic3r_GUI_Appearance_ThemeFollow_hpp_
