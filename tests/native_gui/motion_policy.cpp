#include "slic3r/GUI/Widgets/MD3MotionPolicy.hpp"
#include "slic3r/GUI/Widgets/StateMotionMath.hpp"
#include <iostream>

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
    std::cout << "Motion policy checks passed: " << checks << "\n";
    return 0;
}
