#include "slic3r/GUI/Widgets/MD3MotionPolicy.hpp"
#include "slic3r/GUI/Widgets/StateMotionMath.hpp"
#include <iostream>
#include <array>
#include "slic3r/GUI/Widgets/MenuMotionPaint.hpp"

int main()
{
    using namespace MD3::Motion;
    int checks = 0;
    auto check = [&checks](bool value) { ++checks; return value; };
    for (const auto setting : {"system", "", "unknown", "full"}) {
        if (!check(!reduce_motion(setting, false)) || !check(reduce_motion(setting, true))) return 1;
    }
    if (!check(reduce_motion("reduced", false)) || !check(reduce_motion("reduced", true))) return 1;
    if (!check(owner_action(true, false, false, false) == OwnerAction::Cancel) ||
        !check(owner_action(true, false, false, true) == OwnerAction::Cancel) ||
        !check(owner_action(true, true, false, false) == OwnerAction::Settle) ||
        !check(owner_action(true, true, true, true) == OwnerAction::Settle) ||
        !check(owner_action(true, true, true, false) == OwnerAction::Animate) ||
        !check(owner_action(false, false, false, false) == OwnerAction::Animate) ||
        !check(owner_action(false, false, false, true) == OwnerAction::Settle)) return 1;
    if (!check(interpolate(10, 30, 0) == 10) ||
        !check(interpolate(10, 30, 1) == 30) ||
        !check(interpolate(10, 30, -1) == 10) ||
        !check(interpolate(10, 30, 2) == 30) ||
        !check(interpolate(interpolate(0, 100, .25), 0, .5) == 12.5) ||
        !check(color_channel(255, 0, .5) == 128) ||
        !check(color_channel(0, 255, .5) == 128) ||
        !check(color_channel(255, 0, 1) == 0) ||
        !check(color_channel(0, 255, 1) == 255)) return 1;
    using RGB = std::array<unsigned char, 3>;
    const RGB foreground{25, 28, 32}, secondary{68, 70, 78}, surface{238, 237, 243};
    auto luminance = [](const RGB &rgb) {
        double channels[3];
        for (int i = 0; i < 3; ++i) {
            const double c = rgb[i] / 255.0;
            channels[i] = c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4);
        }
        return .2126 * channels[0] + .7152 * channels[1] + .0722 * channels[2];
    };
    auto contrast = [&](const RGB &a, const RGB &b) {
        const double x = luminance(a), y = luminance(b);
        return (std::max(x, y) + .05) / (std::min(x, y) + .05);
    };
    for (double progress : {0.0, .25, .5, 1.0}) for (bool reduced : {false, true}) {
        const auto paint = menu_filter_paint(foreground, secondary, progress, reduced);
        if (!check(paint.foreground == foreground) || !check(paint.secondary == secondary) ||
            !check(contrast(paint.secondary, surface) >= 4.5) ||
            !check(reduced ? paint.edge_opacity == 0 : paint.edge_opacity >= 0 && paint.edge_opacity <= .16)) return 1;
    }
    // Reject the former first-frame text blend using the same light caption
    // colors. This negative would pass a mere nonzero-opacity assertion.
    RGB former_blend;
    for (int i = 0; i < 3; ++i) former_blend[i] = static_cast<unsigned char>(std::lround(secondary[i] * .6 + surface[i] * .4));
    if (!check(contrast(former_blend, surface) < 4.5)) return 1;
    std::cout << "Motion policy checks passed: " << checks << "\n";
    return 0;
}
