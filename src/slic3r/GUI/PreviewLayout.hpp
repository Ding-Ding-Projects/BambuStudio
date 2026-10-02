#ifndef slic3r_PreviewLayout_hpp_
#define slic3r_PreviewLayout_hpp_

#include <algorithm>

namespace Slic3r { namespace GUI { namespace PreviewLayout {
struct Column { float width; float right; };
inline Column notification_column(float canvas_width, float scale, float desired, float right)
{
    const float margin = std::min(16.0f * scale, std::max(0.0f, canvas_width / 8.0f));
    const float beside_width = canvas_width - std::max(right, margin) - margin;
    const float preferred = beside_width >= std::min(280.0f * scale, desired)
        ? std::min(desired, beside_width) : desired;
    const float width = std::max(1.0f, std::min(preferred, canvas_width - 2.0f * margin));
    return {width, std::clamp(right, margin, std::max(margin, canvas_width - width - margin))};
}
struct StackItem { float height; bool deferred; };
inline StackItem stack_item(float wanted, float initial, float bottom, float top)
{
    const float height = std::clamp(wanted, 1.0f, std::max(1.0f, top - bottom));
    return {height, initial + height > top + 0.5f};
}
inline float tips_height(float canvas_height, float initial, float header, float padding, float scale)
{
    return std::clamp(canvas_height - initial - header - padding - 16.0f * scale,
                      1.0f, std::max(1.0f, 380.0f * scale));
}
inline bool inline_link_fits(float width, float title, float link, float spacing)
{
    return title + link + spacing <= width;
}
}}}
#endif
