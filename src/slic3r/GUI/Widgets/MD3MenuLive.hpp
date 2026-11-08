#ifndef slic3r_GUI_MD3MenuLive_hpp_
#define slic3r_GUI_MD3MenuLive_hpp_

// Live controls inside a Material menu row, without wxWidgets: a switch, a
// choice, a slider or a stepper that changes a value in place while the menu
// stays open. The menu (MD3Menu.cpp) draws them and routes the mouse and the
// keys here; these rules decide what a click, Left, Right or Space does to the
// value and how the value reads aloud. Tested on its own by
// tests/md3_menu_live_test.cpp.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace MD3 { namespace Menu {

struct LiveControl
{
    enum class Kind { Switch, Choice, Slider, Stepper };
    Kind                      kind = Kind::Switch;
    std::vector<std::string>  choices;  // Choice: the labels, UTF-8
    double                    min  = 0; // Slider, Stepper
    double                    max  = 1;
    double                    step = 1;
    int                       decimals = 0; // digits shown after the point
    std::string               unit;         // shown after the number, UTF-8 ("%", " mm")
    std::function<double()>   get;          // Switch 0 or 1, Choice the index, else the value
    std::function<void(double)> set;        // applied at once; the menu stays open

    double value() const { return get ? get() : 0.0; }
};

// Keeps a value inside the control's range and on its step grid.
inline double clamp_to(const LiveControl &c, double v)
{
    switch (c.kind) {
    case LiveControl::Kind::Switch: return v >= 0.5 ? 1.0 : 0.0;
    case LiveControl::Kind::Choice: {
        if (c.choices.empty()) return 0.0;
        const double last = double(c.choices.size() - 1);
        return std::clamp(std::round(v), 0.0, last);
    }
    case LiveControl::Kind::Slider:
    case LiveControl::Kind::Stepper: {
        const double lo = std::min(c.min, c.max), hi = std::max(c.min, c.max);
#ifndef SLIC3R_MENU_LIVE_NEGATIVE_FIXTURE
        v = std::clamp(v, lo, hi);
#else
        (void) hi;
#endif
        if (c.step > 0) {
            v = lo + std::round((v - lo) / c.step) * c.step;
#ifndef SLIC3R_MENU_LIVE_NEGATIVE_FIXTURE
            v = std::clamp(v, lo, hi);
#endif
        }
        // Drop the floating-point dust a step grid leaves behind (0.30000000000000004).
        const double scale = std::pow(10.0, std::clamp(c.decimals, 0, 6) + 3);
        return std::round(v * scale) / scale;
    }
    }
    return v;
}

// One step: direction is -1 or +1. A switch flips whichever way; a choice
// moves one place and stops at either end; a slider or stepper moves one step.
inline double nudge(const LiveControl &c, double value, int direction)
{
    if (direction == 0) return clamp_to(c, value);
    switch (c.kind) {
    case LiveControl::Kind::Switch: return value >= 0.5 ? 0.0 : 1.0;
    case LiveControl::Kind::Choice: return clamp_to(c, value + (direction > 0 ? 1 : -1));
    case LiveControl::Kind::Slider:
    case LiveControl::Kind::Stepper: return clamp_to(c, value + (direction > 0 ? 1 : -1) * (c.step > 0 ? c.step : 1));
    }
    return value;
}

// Slider: the value at a fraction (0 at the left end, 1 at the right) of the track, and back.
inline double value_at(const LiveControl &c, double fraction)
{
    fraction = std::clamp(fraction, 0.0, 1.0);
    return clamp_to(c, c.min + (c.max - c.min) * fraction);
}

inline double fraction_of(const LiveControl &c, double value)
{
    if (c.max == c.min) return 0.0;
    return std::clamp((value - c.min) / (c.max - c.min), 0.0, 1.0);
}

// The value as the row shows it and a screen reader reads it. The caller
// passes the translated words for a switch.
inline std::string value_text(const LiveControl &c, double value, const std::string &on, const std::string &off)
{
    switch (c.kind) {
    case LiveControl::Kind::Switch: return value >= 0.5 ? on : off;
    case LiveControl::Kind::Choice: {
        const double v = clamp_to(c, value);
        return c.choices.empty() ? std::string() : c.choices[std::size_t(v)];
    }
    case LiveControl::Kind::Slider:
    case LiveControl::Kind::Stepper: {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.*f", std::clamp(c.decimals, 0, 6), clamp_to(c, value));
        std::string text = buf;
        if (text == "-0") text = "0";
        return text + c.unit;
    }
    }
    return std::string();
}

// A step for a decimal setting that moves its last meaningful digit:
// 0.2 -> 0.01, 1.75 -> 0.1, 15 -> 1, 200 -> 10. Whole-number settings step by 1.
inline double step_for(double value, bool integer)
{
    if (integer) return 1.0;
    const double magnitude = std::fabs(value);
    if (magnitude < 1e-9) return 0.1;
    double step = std::pow(10.0, std::floor(std::log10(magnitude)) - 1.0);
    return std::clamp(step, 0.001, 100.0);
}

// Digits after the point that a step needs: 0.01 -> 2, 1 -> 0.
inline int decimals_for(double step)
{
    int d = 0;
    while (d < 6 && std::fabs(step * std::pow(10.0, d) - std::round(step * std::pow(10.0, d))) > 1e-6) ++d;
    return d;
}

}} // namespace MD3::Menu

#endif // slic3r_GUI_MD3MenuLive_hpp_
