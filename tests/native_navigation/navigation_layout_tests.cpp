#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "slic3r/GUI/NotebookNavigation.hpp"

#include <set>

using namespace NotebookNavigation;

TEST_CASE("Workflow projection preserves every original page ID")
{
    REQUIRE(order(9, {{1, 2, 8, 3}}) == std::vector<int>{1, 2, 8, 3, 0, 4, 5, 6, 7});
    REQUIRE(order(4, {{-1, 2, 2, 99}}) == std::vector<int>{2, 0, 1, 3});
    REQUIRE(order(0, {{1, 2, 8, 3}}).empty());
}

TEST_CASE("Insert and remove keep workflow roles attached to the same pages")
{
    Workflow workflow{{1, 2, 8, 3}};
    inserted(workflow, 2);
    REQUIRE(workflow == Workflow{{1, 3, 9, 4}});
    removed(workflow, 2);
    REQUIRE(workflow == Workflow{{1, 2, 8, 3}});
    removed(workflow, 3);
    REQUIRE(workflow == Workflow{{1, 2, 7, -1}});
    inserted(workflow, 0);
    REQUIRE(workflow == Workflow{{2, 3, 8, -1}});
}

TEST_CASE("Full labels use measured widths and overflow appears only when needed")
{
    const std::vector<Item> items{{1, 100, true}, {2, 140, true}, {3, 80, false}};
    auto layout = fit(items, 328, 40, 4);
    REQUIRE(layout.overflow.empty());
    REQUIRE(layout.used == 328);
    REQUIRE(layout.visible[1].x == 104);
    REQUIRE(layout.visible[1].width == 140);
    layout = fit(items, 327, 40, 4);
    REQUIRE(layout.overflow == std::vector<int>{3});
    REQUIRE(layout.overflow_x == 248);
    REQUIRE(layout.used == 288);
}

TEST_CASE("A selected auxiliary destination remains visible without changing visual order")
{
    const std::vector<Item> items{{1, 100, true}, {2, 140, true}, {8, 160, true}, {3, 80, true}, {0, 100, false}};
    const auto layout = fit(items, 260, 40, 4, 0);
    REQUIRE(layout.visible.size() == 2);
    REQUIRE(layout.visible[0].id == 1);
    REQUIRE(layout.visible[1].id == 0);
    REQUIRE(layout.visible[1].width == 100);
    REQUIRE(layout.overflow == std::vector<int>{2, 8, 3});
}

TEST_CASE("A label wider than the viewport stays wholly in the searchable overflow")
{
    const auto layout = fit({{1, 2000, true}, {2, 80, true}}, 200, 40, 4, 1);
    REQUIRE(layout.overflow == std::vector<int>{1});
    REQUIRE(layout.visible.size() == 1);
    REQUIRE(layout.visible[0].id == 2);
    REQUIRE(layout.visible[0].width == 80);
}

TEST_CASE("Every destination remains exactly once and every visible rectangle is bounded")
{
    const std::vector<Item> items{{1, 120, true}, {2, 173, true}, {8, 110, true}, {3, 136, true}, {0, 92, false}, {4, 184, false}, {9, 135, false}};
    for (int scale : {100, 125, 150, 200}) {
        auto scaled = items;
        for (auto &item : scaled) item.width = item.width * scale / 100;
        for (int available = 0; available <= 2400; available += 7) {
            for (int selected : {-1, 0, 1, 3, 8, 9}) {
                const auto layout = fit(scaled, available, 40 * scale / 100, 4 * scale / 100, selected);
                REQUIRE(layout.used <= available);
                std::set<int> ids;
                int right = 0;
                for (const auto &position : layout.visible) {
                    REQUIRE(ids.insert(position.id).second);
                    REQUIRE(position.x >= right);
                    REQUIRE(position.x + position.width <= available);
                    const auto original = std::find_if(scaled.begin(), scaled.end(), [&](const Item &item) { return item.id == position.id; });
                    REQUIRE(original != scaled.end());
                    REQUIRE(position.width == original->width);
                    right = position.x + position.width;
                }
                for (int id : layout.overflow) REQUIRE(ids.insert(id).second);
                REQUIRE(ids.size() == items.size());
                if (!layout.overflow.empty()) {
                    REQUIRE(layout.overflow_x >= right);
                    REQUIRE(layout.overflow_x + layout.overflow_width <= available);
                    const auto active = std::find_if(scaled.begin(), scaled.end(), [selected](const Item &item) { return item.id == selected; });
                    if (active != scaled.end() && active->width <= available - 44 * scale / 100)
                        REQUIRE(std::find(layout.overflow.begin(), layout.overflow.end(), selected) == layout.overflow.end());
                }
            }
        }
    }
}
