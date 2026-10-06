#ifndef slic3r_GUI_NotebookNavigation_hpp_
#define slic3r_GUI_NotebookNavigation_hpp_

#include <algorithm>
#include <array>
#include <vector>

namespace NotebookNavigation {

// These are positions in the book, never a reordered copy of its page array.
using Workflow = std::array<int, 4>;

inline void inserted(Workflow &workflow, int at)
{
    for (int &page : workflow)
        if (page >= at) ++page;
}

inline void removed(Workflow &workflow, int at)
{
    for (int &page : workflow) {
        if (page == at) page = -1;
        else if (page > at) --page;
    }
}

inline std::vector<int> order(int count, const Workflow &workflow)
{
    std::vector<int> result;
    for (const int page : workflow)
        if (page >= 0 && page < count && std::find(result.begin(), result.end(), page) == result.end())
            result.push_back(page);
    for (int page = 0; page < count; ++page)
        if (std::find(result.begin(), result.end(), page) == result.end())
            result.push_back(page);
    return result;
}

struct Item {
    int id;
    int width;
    bool workflow;
};

struct Placement {
    int id;
    int x;
    int width;
    bool workflow;
};

struct Layout {
    std::vector<Placement> visible;
    std::vector<int> overflow;
    int overflow_x = 0;
    int overflow_width = 0;
    int used = 0;
};

// Preserve full measured widths. Selection gets first refusal on the available
// space, then the remaining destinations retain their visual order. If even the
// selected label cannot fit it stays explicitly checked in the overflow menu.
inline Layout fit(const std::vector<Item> &items, int available, int overflow_width, int gap, int selected = -1)
{
    Layout result;
    available = std::max(0, available);
    gap = std::max(0, gap);
    for (const auto &item : items)
        result.used += std::max(0, item.width) + (result.used ? gap : 0);
    const bool overflows = result.used > available;
    result.used = 0;
    overflow_width = std::min(available, std::max(0, overflow_width));
    const int budget = overflows ? std::max(0, available - overflow_width - gap) : available;
    const auto active = std::find_if(items.begin(), items.end(), [selected](const Item &item) { return item.id == selected; });
    const int reserve = overflows && active != items.end() && active->width <= budget
        ? std::max(0, active->width) : 0;
    int reserved = reserve;
    for (const auto &item : items) {
        const int width = std::max(0, item.width);
        if (item.id == selected) reserved = 0;
        const int x = result.used + (result.visible.empty() ? 0 : gap);
        const int remaining_reserve = reserved ? reserved + gap : 0;
        if (width <= budget - x - remaining_reserve) {
            result.visible.push_back({item.id, x, width, item.workflow});
            result.used = x + width;
        } else {
            result.overflow.push_back(item.id);
        }
    }
    if (!result.overflow.empty()) {
        result.overflow_x = result.used + (result.visible.empty() ? 0 : gap);
        result.overflow_width = overflow_width;
        result.used = result.overflow_x + overflow_width;
    }
    return result;
}

} // namespace NotebookNavigation
#endif
