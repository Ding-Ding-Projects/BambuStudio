#pragma once
#include <algorithm>
#include <cmath>

namespace MD3::Motion {
inline double interpolate(double from, double to, double progress)
{
    return from + (to - from) * std::clamp(progress, 0.0, 1.0);
}
inline unsigned char color_channel(unsigned char from, unsigned char to, double progress)
{
    return static_cast<unsigned char>(std::lround(interpolate(from, to, progress)));
}
}
