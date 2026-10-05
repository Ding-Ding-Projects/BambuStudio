#pragma once
#include <algorithm>

namespace MD3::Motion {
template<class Color> struct MenuFilterPaint {
    Color foreground;
    Color secondary;
    double edge_opacity;
};

// Filter feedback belongs to decoration. Content colors, including disabled
// and selected variants already resolved by the caller, must remain exact.
template<class Color>
MenuFilterPaint<Color> menu_filter_paint(Color foreground, Color secondary,
                                         double progress, bool reduced)
{
    return {foreground, secondary, reduced ? 0.0 : 0.16 * (1.0 - std::clamp(progress, 0.0, 1.0))};
}
}
