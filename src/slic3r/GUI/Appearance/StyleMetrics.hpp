#ifndef slic3r_GUI_Appearance_StyleMetrics_hpp_
#define slic3r_GUI_Appearance_StyleMetrics_hpp_

#include <algorithm>
#include <cmath>

namespace Slic3r { namespace GUI {

// Values are logical pixels. Conversion to device pixels belongs to the widget.
struct StyleMetrics {
    double radius = 0;
    double border_width = 0;
    double padding = 0;
    double margin = 0;
    double letter_spacing = 0;
    double line_height = 1;

    static double bounded(double value, double fallback, double low, double high)
    {
        return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
    }

    StyleMetrics sanitized() const
    {
        return {bounded(radius, 0, 0, 64), bounded(border_width, 0, 0, 12),
                bounded(padding, 0, 0, 64), bounded(margin, 0, 0, 64),
                bounded(letter_spacing, 0, -4, 20), bounded(line_height, 1, 0.8, 3)};
    }

    // Rounded outlines must never extend beyond the owning box.
    double radius_for(double width, double height) const
    {
        return std::min(sanitized().radius, std::max(0.0, std::min(width, height) / 2));
    }
    double inset_for(double width, double height) const
    {
        const auto s = sanitized();
        return std::min(s.padding + s.border_width, std::max(0.0, std::min(width, height) / 2));
    }
    double line_advance(double font_height) const
    {
        return std::max(0.0, font_height) * sanitized().line_height;
    }
};

}} // namespace Slic3r::GUI
#endif
