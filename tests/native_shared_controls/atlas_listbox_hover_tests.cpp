#include <array>
#include <functional>
#include <iostream>
#include <set>
#include <string>
#include <vector>
#include "../../src/slic3r/GUI/Widgets/MD3MotionPolicy.hpp"

// Non-window adapters record invalidated rows. The transition/event bodies and
// hover paint expression below are extracted verbatim from production source.
using wxString = std::string;
struct wxMouseEvent { int row; int GetY() const { return row; } void Skip() {} };
namespace MD3 { namespace Motion {
bool reduce = false, hidden = false;
bool reduced() { return reduce; }
constexpr int short2 = 100;
double easeStandard(double t) { return t; }
struct Anim {
    std::function<void(double)> tick;
    void Stop() { tick = nullptr; } // production Stop cancels without a final tick
    void Play(int, std::function<void(double)> callback, std::nullptr_t, double(*)(double), void*) {
        tick = callback;
        if (owner_action(true, true, !hidden, reduce) == OwnerAction::Settle) { auto last = tick; Stop(); last(1.0); }
        else tick(0.0);
    }
    void advance(double value) {
        if (!tick) return;
        if (owner_action(true, true, !hidden, reduce) == OwnerAction::Settle) value = 1.0;
        auto callback = tick;
        if (value == 1.0) Stop();
        callback(value);
    }
};
}}
struct ListBox {
    std::vector<wxString> m_rows {"A", "B", "C"};
    int m_hover = -1, m_previous_hover = -1;
    double m_hover_progress = 1.0;
    MD3::Motion::Anim m_hover_motion;
    std::set<size_t> dirty;
    std::array<double, 3> painted {0, 0, 0};
    int VirtualHitTest(int row) const { return row; }
    void RefreshRow(size_t row) { dirty.insert(row); }
    void SetToolTip(const wxString&) {}
    bool IsEnabled() const { return true; }
    void onMotion(wxMouseEvent&);
    void onLeave(wxMouseEvent&);
    void animateHover(int);
    double hoverPaint(size_t) const;
    void move(int row) { wxMouseEvent event {row}; onMotion(event); }
    void leave() { wxMouseEvent event {-1}; onLeave(event); }
    void flush() { for (auto row : dirty) painted.at(row) = hoverPaint(row); dirty.clear(); }
};
#include "atlas_listbox_hover.inc"

int main()
{
    int assertions = 0, failures = 0;
    auto check = [&](bool ok) { ++assertions; if (!ok) ++failures; };
    // Each mode covers both interruption paths. A cached painted surface changes
    // only when the actual production bodies invalidate that row.
    for (int mode = 0; mode < 3; ++mode) for (bool leave : {false, true}) {
        MD3::Motion::reduce = false; MD3::Motion::hidden = false;
        ListBox list;
        list.move(0); list.m_hover_motion.advance(1.0); list.flush();
        check(list.painted[0] == 1.0);
        list.move(1); list.m_hover_motion.advance(0.25); list.flush();
        check(list.painted[0] > 0.0 && list.painted[1] > 0.0);
        MD3::Motion::reduce = mode == 1; MD3::Motion::hidden = mode == 2;
        if (leave) list.leave(); else list.move(2);
        check(list.dirty.count(0) == 1);
        list.flush();
        check(list.painted[0] == 0.0);
        list.m_hover_motion.advance(1.0); list.flush();
        check(list.painted[1] == 0.0);
        check(list.painted[2] == (leave ? 0.0 : 1.0));
        check(list.m_hover == (leave ? -1 : 2));
        check(!list.m_hover_motion.tick);
    }
    std::cout << "6 interrupted-hover cases; " << assertions << " assertions; " << failures << " failures\n";
    return failures ? 1 : 0;
}
