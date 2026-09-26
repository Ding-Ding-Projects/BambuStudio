#ifndef slic3r_GUI_FANMOTIONMATH_hpp_
#define slic3r_GUI_FANMOTIONMATH_hpp_

#include <algorithm>
#include <cmath>

namespace Slic3r { namespace GUI { namespace FanMotionMath {

inline int percent_from_pwm(int pwm)
{
    return (std::clamp(pwm, 0, 255) * 100 + 127) / 255;
}

inline bool telemetry_matches_command(int telemetry_percent, int command_percent)
{
    return command_percent >= 0 && std::abs(telemetry_percent - command_percent) <= 1;
}

inline double ease_track(double displayed, int target)
{
    const double delta = target - displayed;
    return std::abs(delta) < 0.5 ? target : displayed + delta * 0.22;
}

inline double angle_step(int telemetry_percent, bool auxiliary)
{
    return telemetry_percent <= 0 ? 0.0 :
        (auxiliary ? 0.035 : 0.045) + telemetry_percent * (auxiliary ? 0.004 : 0.006);
}

}}} // namespace Slic3r::GUI::FanMotionMath

#endif
