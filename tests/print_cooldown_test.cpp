// Standalone assertions for src/slic3r/GUI/PrintCooldown/PrintCooldown.hpp: the
// status and temperature rules, the fan G-code and the session that decides
// when the fans go on and off. Build without NDEBUG (this file undefines it),
// for example:
//   g++ -std=c++17 -Wall -Wextra -Werror tests/print_cooldown_test.cpp -o pct && ./pct
// tests/print_cooldown.test.mjs compiles and runs it.
#undef NDEBUG
#include "../src/slic3r/GUI/PrintCooldown/PrintCooldown.hpp"
#include <cassert>
#include <iostream>
#include <string>

using namespace Slic3r::GUI::PrintCooldown;
static int assertions = 0;
#define CHECK(expr) do { assert(expr); ++assertions; } while (0)

int main()
{
    // Status rules.
    for (const char *status : {"RUNNING", "PAUSE", "PREPARE", "SLICING"}) { CHECK(is_printing(status)); CHECK(!has_ended(status)); }
    for (const char *status : {"FINISH", "FAILED"}) { CHECK(has_ended(status)); CHECK(!is_printing(status)); }
    CHECK(!is_printing("IDLE") && !has_ended("IDLE"));

    // Temperature rule: the bed always counts, the chamber only when reported.
    CHECK(cool_enough(40.0f, 40.0f));
    CHECK(!cool_enough(40.5f, 30.0f));
    CHECK(!cool_enough(30.0f, 41.0f));
    CHECK(cool_enough(30.0f, 0.0f));
    CHECK(cool_enough(30.0f, -1.0f));

    // G-code: all three fans, full or off.
    CHECK(fans_gcode(true) == "M106 P1 S255\nM106 P2 S255\nM106 P3 S255\n");
    CHECK(fans_gcode(false) == "M106 P1 S0\nM106 P2 S0\nM106 P3 S0\n");
    CHECK(kTimeoutMs == 300000);

    { // A print ends hot, the fans run, the printer cools.
        Session s;
        CHECK(s.observe("a", "FINISH", 60, 45, 0, true) == Action::None); // finished before the app looked
        CHECK(s.observe("a", "RUNNING", 60, 45, 1000, true) == Action::None);
        CHECK(s.observe("a", "FINISH", 60, 45, 2000, true) == Action::FansOn);
        CHECK(s.phase() == Phase::Cooling);
        CHECK(s.remaining_ms(2000) == kTimeoutMs);
        CHECK(s.observe("a", "FINISH", 50, 45, 60000, true) == Action::None);
        CHECK(s.remaining_ms(62000) == kTimeoutMs - 60000);
        CHECK(s.observe("a", "FINISH", 39, 41, 90000, true) == Action::None); // chamber still warm
        CHECK(s.observe("a", "FINISH", 39, 40, 91000, true) == Action::FansOff);
        CHECK(s.phase() == Phase::Cooled);
        CHECK(s.remaining_ms(91000) == 0);
        CHECK(s.observe("a", "FINISH", 39, 40, 92000, true) == Action::None); // sent once
    }
    { // The five minute limit.
        Session s;
        s.observe("a", "RUNNING", 100, 0, 0, true);
        CHECK(s.observe("a", "FAILED", 100, 0, 1000, true) == Action::FansOn);
        CHECK(s.observe("a", "FAILED", 90, 0, 1000 + kTimeoutMs - 1, true) == Action::None);
        CHECK(s.observe("a", "FAILED", 90, 0, 1000 + kTimeoutMs, true) == Action::FansOff);
        CHECK(s.phase() == Phase::TimedOut);
        CHECK(s.observe("a", "FAILED", 90, 0, 2000 + kTimeoutMs, true) == Action::None);
    }
    { // Already cool at the end: no fans.
        Session s;
        s.observe("a", "RUNNING", 35, 0, 0, true);
        CHECK(s.observe("a", "FINISH", 35, 0, 1000, true) == Action::None);
        CHECK(s.phase() == Phase::Cooled);
    }
    { // Off: nothing starts, and turning it on later does not start a stale cool-down.
        Session s;
        s.observe("a", "RUNNING", 60, 0, 0, false);
        CHECK(s.observe("a", "FINISH", 60, 0, 1000, false) == Action::None);
        CHECK(s.observe("a", "FINISH", 60, 0, 2000, true) == Action::None);
        CHECK(s.phase() == Phase::Idle);
    }
    { // Turned off while cooling: the fans stop.
        Session s;
        s.observe("a", "RUNNING", 60, 0, 0, true);
        CHECK(s.observe("a", "FINISH", 60, 0, 1000, true) == Action::FansOn);
        CHECK(s.observe("a", "FINISH", 60, 0, 2000, false) == Action::FansOff);
        CHECK(s.phase() == Phase::Idle);
    }
    { // A new print during the cool-down owns the fans: nothing is sent.
        Session s;
        s.observe("a", "RUNNING", 60, 0, 0, true);
        CHECK(s.observe("a", "FINISH", 60, 0, 1000, true) == Action::FansOn);
        CHECK(s.observe("a", "PREPARE", 60, 0, 2000, true) == Action::None);
        CHECK(s.phase() == Phase::Idle);
        CHECK(s.observe("a", "FINISH", 60, 0, 3000, true) == Action::FansOn); // and its own end cools again
    }
    { // Switching printers starts over and sends nothing to either.
        Session s;
        s.observe("a", "RUNNING", 60, 0, 0, true);
        CHECK(s.observe("b", "FINISH", 60, 0, 1000, true) == Action::None);
        s.observe("b", "RUNNING", 60, 0, 2000, true);
        CHECK(s.observe("b", "FINISH", 60, 0, 3000, true) == Action::FansOn);
        CHECK(s.observe("a", "FINISH", 60, 0, 4000, true) == Action::None);
        CHECK(s.phase() == Phase::Idle);
    }

    std::cout << "print_cooldown_test: " << assertions << " assertions passed\n";
    return 0;
}
