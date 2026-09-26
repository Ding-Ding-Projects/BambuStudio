#include "../../src/slic3r/GUI/Widgets/FanMotionMath.hpp"

#include <cassert>

using namespace Slic3r::GUI::FanMotionMath;

int main()
{
    assert(percent_from_pwm(-1) == 0);
    assert(percent_from_pwm(0) == 0);
    assert(percent_from_pwm(127) == 50);
    assert(percent_from_pwm(255) == 100);
    assert(percent_from_pwm(300) == 100);

    assert(!telemetry_matches_command(60, -1));
    assert(!telemetry_matches_command(59, 70));
    assert(telemetry_matches_command(69, 70));
    assert(telemetry_matches_command(70, 70));

    assert(ease_track(0.0, 100) > 0.0);
    assert(ease_track(0.0, 100) < 100.0);
    assert(ease_track(99.6, 100) == 100.0);
    assert(ease_track(80.0, 20) < 80.0); // a fast reversal retargets immediately

    assert(angle_step(0, false) == 0.0);
    assert(angle_step(0, true) == 0.0);
    assert(angle_step(80, false) > angle_step(20, false));
    assert(angle_step(80, false) != angle_step(80, true));
}
