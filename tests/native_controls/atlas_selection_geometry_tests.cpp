#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include "atlas_selection_geometry.inc"

// Minimal window contract for the extracted production event/layout methods.
// This exercises their allocation lifecycle without claiming a wxWidgets run.
struct wxSize { int x = 0, y = 0; };
struct wxSizeEvent { bool skipped = false; void Skip() { skipped = true; } };
struct Child {
    wxSize position, size;
    void SetSize(wxSize value) { size = value; }
    void SetPosition(wxSize value) { position = value; }
};
namespace MD3 { namespace Metrics {
    bool compact = false;
    bool isCompact() { return compact; }
} }
class SpinInput {
public:
    double scale = 1;
    bool m_layout_children = false;
    wxSize allocated, m_editor_best, labelSize;
    Child entry, increment, decrement;
    Child *text_ctrl = &entry, *button_inc = &increment, *button_dec = &decrement;
    wxSize GetClientSize() { return allocated; }
    int FromDIP(int dip) { return int(std::lround(dip * scale)); }
    void layoutChildren();
    void onSize(wxSizeEvent &event);
};
#include "atlas_selection_lifecycle.inc"

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
    // Start at a requested size, then let a sizer allocate the content minimum
    // (and an explicitly forced smaller size). Width and height both change.
    for (double scale : {1.0, 1.25, 1.5, 2.0}) for (bool compact : {false, true})
    for (int requested_width : {96, 110}) {
        SpinInput control;
        control.scale = scale;
        MD3::Metrics::compact = compact;
        control.m_editor_best = {control.FromDIP(10), control.FromDIP(18)};
        const int pad = control.FromDIP(compact ? 3 : 4);
        const auto minimum = atlasSpinLayout(0, 0, control.m_editor_best.y,
            control.m_editor_best.x, 0, 0, pad, control.FromDIP(compact ? 18 : 20));
        control.allocated = {control.FromDIP(requested_width), control.FromDIP(50)};
        wxSizeEvent initial;
        control.onSize(initial);
        const int old_right = control.entry.position.x + control.entry.size.x;
        const int old_bottom = control.decrement.position.y + control.decrement.size.y;
        verify(old_right > minimum.minimum_width);
        verify(old_bottom > minimum.height);
        // Explicit constructor/SetSize dimensions remain minimums; a caller
        // with unspecified dimensions publishes only the measured content floor.
        const wxSize explicit_minimum{std::max(control.FromDIP(requested_width), minimum.minimum_width),
                                      std::max(control.FromDIP(50), minimum.height)};
        for (wxSize allocation : {explicit_minimum, wxSize{minimum.minimum_width, minimum.height},
             wxSize{control.FromDIP(32), control.FromDIP(12)}, wxSize{0, 0},
             wxSize{control.FromDIP(180), control.FromDIP(64)}}) {
            control.allocated = allocation;
            wxSizeEvent resized;
            control.onSize(resized);
            verify(resized.skipped);
            verify(!control.m_layout_children);
            for (const auto *child : {&control.entry, &control.increment, &control.decrement}) {
                verify(child->position.x >= 0 && child->position.y >= 0);
                verify(child->size.x >= 0 && child->size.y >= 0);
                verify(child->position.x + child->size.x <= allocation.x);
                verify(child->position.y + child->size.y <= allocation.y);
            }
        }
    }
    std::cout << "Numeric geometry: " << assertions << " assertions passed\n";
}
