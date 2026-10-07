#include <algorithm>
#include <cmath>
#include <iostream>
#include "atlas_listbox_geometry.inc"

int main()
{
    int assertions = 0, failures = 0;
    auto check = [&](bool ok) { ++assertions; if (!ok) ++failures; };
    // Normal text and checkbox origins retain the existing click geometry.
    auto plain = atlasListGeometry(300, 40, 4, 2, 12, 0);
    auto checked = atlasListGeometry(300, 40, 4, 2, 12, 28);
    check(plain.text_x == 16 && plain.text_width == 268);
    check(checked.text_x == 44 && checked.text_width == 240);
    check(checked.pane_y == 2 && checked.pane_height == 36);
    // Multiline/localized or customized larger text grows only the row floor.
    check(atlasListRowHeight(40, 19, 20, 2, 6) == 40);
    check(atlasListRowHeight(32, 18, 20, 2, 4) == 32);
    check(atlasListRowHeight(40, 72, 20, 2, 6) == 88);
    check(atlasListRowHeight(32, 8, 20, 2, 4) == 32);
    // Both densities and four display scales keep text allocation in its pane.
    for (double scale : {1.0, 1.25, 1.5, 2.0}) {
        auto px = [scale](int dip) { return int(std::lround(dip * scale)); };
        for (int row : {32, 40}) for (int width = 0; width <= 500; ++width) for (bool checks : {false, true}) {
            auto g = atlasListGeometry(width, px(row), px(4), px(2), px(12), checks ? px(28) : 0);
            check(g.pane_x >= 0 && g.pane_width >= 0 && g.pane_x + g.pane_width <= width);
            check(g.text_width >= 0 && g.text_x >= g.pane_x && g.text_x + g.text_width <= g.pane_x + g.pane_width);
            check(g.pane_y >= 0 && g.pane_height >= 0 && g.pane_y + g.pane_height <= px(row));
        }
    }
    // Degenerate and negative dimensions never reach ellipsizing as negative widths.
    auto empty = atlasListGeometry(-1, -1, 4, 2, 12, 28);
    check(empty.text_width == 0 && empty.pane_width == 0 && empty.pane_height == 0);
    for (int width = 0; width < 32; ++width) {
        auto g = atlasListGeometry(width, 1, 4, 2, 12, 28);
        check(g.text_width == 0 && g.text_x <= width);
    }
    std::cout << "4 list geometry cases; " << assertions << " assertions; " << failures << " failures\n";
    return failures ? 1 : 0;
}
