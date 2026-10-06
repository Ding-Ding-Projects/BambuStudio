#include <algorithm>
#include <iostream>
#include <vector>
#include "atlas_shell_layout.inc"

int main()
{
    int checks = 0, failures = 0;
    auto verify = [&](bool value) { ++checks; if (!value) ++failures; };
    auto empty = atlasTabOverflow({}, {}, 0, 36, 4);
    verify(empty.visible.empty() && empty.hidden.empty() && !empty.needs_button);
    auto all = atlasTabOverflow({100, 100}, {false, true}, 204, 36, 4);
    verify(all.visible == std::vector<int>({0, 1}) && !all.needs_button);
    auto priority = atlasTabOverflow({100, 100, 100}, {false, false, true}, 244, 36, 4);
    verify(priority.visible == std::vector<int>({0, 2}) && priority.hidden == std::vector<int>({1}));
    auto large_pin = atlasTabOverflow({1200, 100}, {true, false}, 240, 36, 4);
    verify(large_pin.visible == std::vector<int>({1}) && large_pin.hidden == std::vector<int>({0}));
    auto tiny = atlasTabOverflow({100, 100}, {true, true}, 1, 36, 4);
    verify(tiny.visible.empty() && tiny.hidden == std::vector<int>({0, 1}) && tiny.needs_button);

    // The input indices are stable identities. Visible and hidden partitions never
    // reorder the model, lose an oversized pinned tab, or exceed the reserved budget.
    for (double scale : {1.0, 1.25, 1.5, 2.0}) {
        auto px = [scale](int dip) { return int(dip * scale + 0.5); };
        const std::vector<int> widths{px(96), px(184), px(720), px(128)};
        for (int mask = 0; mask < 16; ++mask) {
            std::vector<bool> pins;
            for (int i = 0; i < 4; ++i) pins.push_back((mask & (1 << i)) != 0);
            const auto original = pins;
            for (int budget = 0; budget <= px(1200); budget += 5) {
                const auto result = atlasTabOverflow(widths, pins, budget, px(36), px(4));
                verify(pins == original);
                verify(std::is_sorted(result.visible.begin(), result.visible.end()));
                verify(std::is_sorted(result.hidden.begin(), result.hidden.end()));
                std::vector<int> partition = result.visible;
                partition.insert(partition.end(), result.hidden.begin(), result.hidden.end());
                std::sort(partition.begin(), partition.end());
                verify(partition == std::vector<int>({0, 1, 2, 3}));
                int used = 0;
                for (int index : result.visible) used += widths[index];
                if (!result.visible.empty()) used += (int(result.visible.size()) - 1) * px(4);
                const int available = result.needs_button ? std::max(0, budget - px(36) - px(4)) : budget;
                verify(used <= available);
                verify(result.needs_button == !result.hidden.empty());
            }
        }
    }

    // Project identity never consumes space already assigned to caption commands.
    for (int width = 0; width <= 2400; ++width) {
        const int text = atlasTitleTextBudget(width, 640, 48, 240);
        verify(text >= 0 && text <= 240);
        verify(text <= std::max(0, width - 640 - 48));
    }
    verify(atlasTitleTextBudget(1000, 640, 48, 240) == 240);
    verify(atlasTitleTextBudget(660, 640, 48, 240) == 0);
    std::cout << "7 shell-layout cases; " << checks << " assertions; " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
