#include "ThemeFollow.hpp"

namespace Slic3r::GUI::ThemeFollow {

Choice choice_from(std::string_view dark_color_mode, std::string_view follow)
{
    if (follow == "1") return Choice::FollowWindows;
    return dark_color_mode == "1" ? Choice::Dark : Choice::Light;
}

Stored store(Choice choice, std::optional<bool> windows_dark, std::string_view current_dark)
{
    switch (choice) {
    case Choice::Light: return {"0", "0"};
    case Choice::Dark: return {"1", "0"};
    case Choice::FollowWindows: break;
    }
    if (!windows_dark) return {current_dark == "1" ? "1" : "0", "1"};
    return {*windows_dark ? "1" : "0", "1"};
}

std::optional<bool> windows_dark_from(std::optional<long> apps_use_light_theme)
{
    if (!apps_use_light_theme) return std::nullopt;
    return *apps_use_light_theme == 0;
}

std::optional<bool> follow_change(std::string_view dark_color_mode, std::string_view follow, std::optional<bool> windows_dark)
{
#ifdef SLIC3R_THEME_FOLLOW_NEGATIVE_FIXTURE
    // Test-only mutation (tests/theme_follow.test.mjs): the suite must fail.
    (void) follow;
#else
    if (follow != "1") return std::nullopt;
#endif
    if (!windows_dark) return std::nullopt;
    const bool dark_now = dark_color_mode == "1";
    if (*windows_dark == dark_now) return std::nullopt;
    return *windows_dark;
}

} // namespace Slic3r::GUI::ThemeFollow
