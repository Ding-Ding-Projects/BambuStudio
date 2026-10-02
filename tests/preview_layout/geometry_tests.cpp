#include "slic3r/GUI/PreviewLayout.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace Slic3r::GUI::PreviewLayout;
int main()
{
    int checks = 0;
    auto require = [&](bool condition, const char* message) {
        ++checks;
        if (!condition) throw std::runtime_error(message);
    };
    for (float scale : {1.0f, 1.25f, 1.5f, 2.0f}) {
        for (float width : {320.0f, 640.0f, 1000.0f, 1600.0f}) {
            auto column = notification_column(width, scale, 560.0f * scale, 360.0f * scale);
            require(column.width > 0.0f, "Notification width must remain positive");
            require(column.right >= 0.0f, "Notification right edge must remain on canvas");
            require(column.width + column.right <= width + 0.01f, "Notification left edge must remain on canvas");
        }
    }
    auto beside = notification_column(800.0f, 1.0f, 560.0f, 350.0f);
    require(std::abs(beside.right - 350.0f) < 0.01f, "Preserve the dock margin when a readable narrower card fits");
    require(std::abs(beside.width - 434.0f) < 0.01f, "Use the space beside the dock");
    auto first = stack_item(900.0f, 80.0f, 80.0f, 500.0f);
    require(!first.deferred && first.height == 420.0f, "Tall first notification gets a bounded scrolling viewport");
    auto second = stack_item(100.0f, 490.0f, 80.0f, 500.0f);
    require(second.deferred, "Defer a whole card that cannot fit in the remaining stack");
    auto next_page = stack_item(100.0f, 80.0f, 80.0f, 500.0f);
    require(!next_page.deferred, "A deferred card fits at the start of the next page");
    require(tips_height(300.0f, 0.0f, 58.0f, 20.0f, 1.0f) == 206.0f, "Tips give height back on a short canvas");
    require(tips_height(1200.0f, 0.0f, 116.0f, 40.0f, 2.0f) == 760.0f, "Tips retain their normal maximum on a tall canvas");
    require(tips_height(20.0f, 0.0f, 116.0f, 40.0f, 2.0f) == 1.0f, "Tips never request a negative child size");
    require(inline_link_fits(320.0f, 140.0f, 160.0f, 8.0f), "A short title and link share a row");
    require(!inline_link_fits(320.0f, 200.0f, 180.0f, 8.0f), "Long translated title and link need separate rows");
    std::cout << checks << " preview layout checks passed\n";
}
