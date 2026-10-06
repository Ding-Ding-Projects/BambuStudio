#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include "atlas_selection_geometry.inc"

int main()
{
    unsigned assertions = 0;
    const auto verify = [&](bool condition) { ++assertions; assert(condition); };
    for (double scale : {1.0, 1.25, 1.5, 2.0}) {
        const auto px = [scale](int dip) { return int(std::lround(dip * scale)); };
        for (bool compact : {false, true}) for (int label : {0, 12, 120, 420})
        for (int width : {0, 45, 60, 110, 200, 800}) for (int height : {0, 24, 32, 50}) {
            const int pad = px(compact ? 3 : 4), digit = px(10), text = px(18);
            const auto g = atlasSpinLayout(px(width), px(height), text, digit, px(label),
                label ? px(16) : 0, pad, px(compact ? 18 : 20));
            verify(g.width >= g.minimum_width);
            verify(g.entry_width >= digit);
            verify(g.entry_x >= pad + g.step_width + pad);
            verify(g.entry_x + g.entry_width <= g.label_x);
            verify(g.label_x + px(label) + pad == g.width);
            verify(2 * g.step_height <= g.height - 2 * pad);
            verify(g.height >= text + 2 * pad);
            // A longer unit followed by a shorter one may release minimum width.
            const auto small = atlasSpinLayout(g.width, g.height, text, digit, 0, 0, pad, g.step_width);
            verify(small.minimum_width <= g.minimum_width);
        }
    }
    std::cout << "Numeric geometry: " << assertions << " assertions passed\n";
}
