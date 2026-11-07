#ifndef slic3r_GUI_PreferencesGeometry_hpp_
#define slic3r_GUI_PreferencesGeometry_hpp_

#include <algorithm>
#include <charconv>
#include <string>

namespace Slic3r { namespace GUI { namespace PreferencesGeometry {

struct Size { int width; int height; };
struct Rect { int x; int y; int width; int height; };

inline int restored_dimension(const std::string &text, int fallback)
{
    int value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    // Bound before DIP conversion, including corrupt or manually edited config.
    return parsed.ec == std::errc() && parsed.ptr == text.data() + text.size() && value > 0 && value <= 16384
        ? value : fallback;
}

inline Size minimum(Size preferred, Size available)
{
    return {std::min(preferred.width, std::max(1, available.width)),
            std::min(preferred.height, std::max(1, available.height))};
}

inline Rect bounded(Rect requested, Rect work, Size preferred_minimum)
{
    const Size low = minimum(preferred_minimum, {work.width, work.height});
    const int width = std::clamp(requested.width, low.width, std::max(1, work.width));
    const int height = std::clamp(requested.height, low.height, std::max(1, work.height));
    return {std::clamp(requested.x, work.x, work.x + std::max(1, work.width) - width),
            std::clamp(requested.y, work.y, work.y + std::max(1, work.height) - height), width, height};
}

}}}
#endif
