#include <algorithm>
#include <cassert>
#include <iostream>
#include "atlas_field_layout.inc"

int main()
{
    unsigned checks = 0;
    auto verify = [&](bool value) { ++checks; assert(value); };
    // A bare editable field uses the available width without an implicit label column.
    auto plain = atlasFieldLayout(200, 8, 8, 0, 0, 0, 0, 48, true, false);
    verify(plain.entry_x == 8 && plain.entry_width == 184 && plain.minimum_width == 64);

    // A label/support stack reserves its widest line once, on either side of the editor.
    for (bool left : {false, true}) {
        auto row = atlasFieldLayout(400, 8, 8, 24, 100, 20, 30, 48, true, left);
        verify(row.minimum_width == 262);
        verify(row.entry_width == 186);
        verify(row.prefix_x + 20 + 8 == row.entry_x);
        verify(row.entry_x + row.entry_width + 8 == row.unit_x);
        if (left) verify(row.label_x + 100 + 8 == row.prefix_x);
        else verify(row.unit_x + 30 + 8 == row.label_x);
    }

    // Read-only choices reserve no phantom native editor or numeric sample.
    auto choice = atlasFieldLayout(280, 8, 8, 24, 180, 0, 0, 48, false, true);
    verify(choice.minimum_width == 220 && choice.entry_width == 0 && choice.label_x == 32);

    // Below minimum width, never pass wxWidgets' -1 "default size" sentinel to the editor.
    for (int width = 0; width < 200; ++width) {
        const auto tiny = atlasFieldLayout(width, 8, 8, 24, 200, 20, 30, 48, true, true);
        verify(tiny.entry_width == 0);
    }

    // Measurements arrive in pixels, once. Cover both densities and the supported scales,
    // short and long bilingual-width labels, prefixes, units, and both label placements.
    for (double scale : {1.0, 1.25, 1.5, 2.0}) {
        const auto px = [scale](int dip) { return static_cast<int>(dip * scale + 0.5); };
        for (int padding : {5, 8}) for (int label : {0, 42, 180, 420})
        for (int prefix : {0, 24}) for (int unit : {0, 58}) for (bool left : {false, true}) {
            const auto required = atlasFieldLayout(0, px(padding), px(8), px(24),
                px(label), px(prefix), px(unit), px(48), true, left);
            const auto fits = atlasFieldLayout(required.minimum_width, px(padding), px(8), px(24),
                px(label), px(prefix), px(unit), px(48), true, left);
            verify(fits.entry_width == px(48));
            verify(fits.entry_x >= px(padding));
            verify(fits.unit_x + px(unit) <= fits.minimum_width - px(padding));
            if (label) verify(fits.label_x + px(label) <= fits.minimum_width - px(padding));
            const auto again = atlasFieldLayout(required.minimum_width, px(padding), px(8), px(24),
                px(label), px(prefix), px(unit), px(48), true, left);
            verify(again.minimum_width == fits.minimum_width && again.entry_x == fits.entry_x);
        }
    }
    std::cout << "5 field-layout cases passed; " << checks << " assertions\n";
}
