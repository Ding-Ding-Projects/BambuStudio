#pragma once

namespace Slic3r::GUI::PrintSetupLayout {

// All inputs are device pixels. Reserve measured chrome before sizing the
// scrollable body, including when the display is smaller than the chrome.
constexpr int bounded_body_height(int content, int preferred, int work_area, int chrome, int margin)
{
    const long long available = static_cast<long long>(work_area > 0 ? work_area : 0)
        - (chrome > 0 ? chrome : 0) - 2LL * (margin > 0 ? margin : 0);
    if (available <= 0 || content <= 0 || preferred <= 0) return 0;
    const int wanted = content < preferred ? content : preferred;
    return available < wanted ? static_cast<int>(available) : wanted;
}

} // namespace Slic3r::GUI::PrintSetupLayout
