// Standalone assertions for src/slic3r/GUI/Widgets/MD3MenuLive.hpp: what a
// click, Left, Right or Space does to a switch, a choice, a slider and a
// stepper inside a menu row, how each value reads aloud, and the step chosen
// for a decimal setting. Compiled and run by tests/md3_menu_live.test.mjs.
#include "src/slic3r/GUI/Widgets/MD3MenuLive.hpp"

#include <cmath>
#include <cstdio>

using namespace MD3::Menu;

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } \
    } while (0)

static bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

int main()
{
    // A switch flips either way and reads On or Off.
    LiveControl sw;
    sw.kind = LiveControl::Kind::Switch;
    CHECK(nudge(sw, 0, +1) == 1 && nudge(sw, 1, +1) == 0 && nudge(sw, 1, -1) == 0);
    CHECK(value_text(sw, 1, "On", "Off") == "On" && value_text(sw, 0, "On", "Off") == "Off");
    CHECK(clamp_to(sw, 0.7) == 1 && clamp_to(sw, 0.2) == 0);

    // A choice moves one place and stops at both ends.
    LiveControl choice;
    choice.kind    = LiveControl::Kind::Choice;
    choice.choices = {"Perspective", "Orthographic"};
    CHECK(nudge(choice, 0, +1) == 1 && nudge(choice, 1, +1) == 1 && nudge(choice, 0, -1) == 0);
    CHECK(value_text(choice, 1, "", "") == "Orthographic" && value_text(choice, 7, "", "") == "Orthographic");
    LiveControl empty_choice;
    empty_choice.kind = LiveControl::Kind::Choice;
    CHECK(clamp_to(empty_choice, 3) == 0 && value_text(empty_choice, 0, "", "").empty());

    // A stepper moves one step and never leaves its range.
    LiveControl loops;
    loops.kind = LiveControl::Kind::Stepper;
    loops.min = 0, loops.max = 5, loops.step = 1;
    CHECK(nudge(loops, 3, +1) == 4 && nudge(loops, 5, +1) == 5 && nudge(loops, 0, -1) == 0);
    CHECK(clamp_to(loops, 9) == 5 && clamp_to(loops, -2) == 0);
    CHECK(value_text(loops, 3, "", "") == "3");

    // Decimal steps stay on the grid without floating-point dust.
    LiveControl height;
    height.kind = LiveControl::Kind::Stepper;
    height.min = 0.08, height.max = 0.6, height.step = 0.01, height.decimals = 2, height.unit = " mm";
    double h = 0.2;
    for (int i = 0; i < 10; ++i) h = nudge(height, h, +1);
    CHECK(near(h, 0.3));
    CHECK(value_text(height, h, "", "") == "0.30 mm");
    CHECK(near(clamp_to(height, 0.123), 0.12));

    // A slider maps the track to its range and back, snapped to its step.
    LiveControl opacity;
    opacity.kind = LiveControl::Kind::Slider;
    opacity.min = 0, opacity.max = 100, opacity.step = 5, opacity.unit = " %";
    CHECK(value_at(opacity, 0.8) == 80 && value_at(opacity, 0.83) == 85 && value_at(opacity, 1.4) == 100 && value_at(opacity, -1) == 0);
    CHECK(near(fraction_of(opacity, 25), 0.25) && fraction_of(opacity, 400) == 1);
    CHECK(value_text(opacity, 80, "", "") == "80 %");
    CHECK(nudge(opacity, 80, -1) == 75);

    // A step that moves the last meaningful digit.
    CHECK(near(step_for(0.2, false), 0.01) && near(step_for(1.75, false), 0.1) && near(step_for(15, false), 1));
    CHECK(near(step_for(200, false), 10) && near(step_for(0, false), 0.1) && step_for(37, true) == 1);
    CHECK(decimals_for(0.01) == 2 && decimals_for(1) == 0 && decimals_for(0.5) == 1);

    // Reading the value goes through the getter; no getter reads zero.
    double stored = 3;
    LiveControl bound;
    bound.kind = LiveControl::Kind::Stepper;
    bound.max  = 10;
    bound.get  = [&stored]() { return stored; };
    bound.set  = [&stored](double v) { stored = v; };
    bound.set(nudge(bound, bound.value(), +1));
    CHECK(stored == 4 && LiveControl{}.value() == 0);

    if (failures) {
        std::printf("menu live controls: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("menu live controls: all checks passed\n");
    return 0;
}
