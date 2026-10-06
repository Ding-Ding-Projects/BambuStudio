#pragma once

namespace Slic3r::GUI::PrepareInspectorLayout {

// Inputs are measured device pixels. The density minimum is a floor, never a
// cap on a translated label, a larger font, or an existing control's best size.
constexpr int row_height(int density_minimum, int text_height, int control_height, int vertical_padding)
{
    const int content = text_height > control_height ? text_height : control_height;
    const int padded = content + 2 * vertical_padding;
    return density_minimum > padded ? density_minimum : padded;
}

} // namespace Slic3r::GUI::PrepareInspectorLayout
