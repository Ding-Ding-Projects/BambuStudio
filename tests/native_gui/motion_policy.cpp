#include "slic3r/GUI/Widgets/MD3MotionPolicy.hpp"
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
    std::cout << "Motion policy checks passed: " << checks << "\n";
    return 0;
}
