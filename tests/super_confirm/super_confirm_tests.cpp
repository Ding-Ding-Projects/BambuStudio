#include <catch_main.hpp>

#include "slic3r/GUI/Widgets/SuperConfirmState.hpp"

using Slic3r::GUI::SuperConfirm::Phase;
using Slic3r::GUI::SuperConfirm::State;

namespace {
State armed_state()
{
    State s;
    s.toggle_key(0);
    s.toggle_key(1);
    return s;
}
} // namespace

TEST_CASE("Untouched gate cannot fire and keeps the slider disabled", "[SuperConfirm]")
{
    State s;
    REQUIRE(s.phase() == Phase::Untouched);
    REQUIRE_FALSE(s.slider_enabled());
    REQUIRE_FALSE(s.may_fire());
    REQUIRE(s.ritual_progress() == 0.0);
    // A slide attempt on a disabled slider is refused and leaves no trace.
    REQUIRE_FALSE(s.set_slider(100));
    REQUIRE(s.slider_percent == 0);
    REQUIRE_FALSE(s.may_fire());
}

TEST_CASE("One key alone never enables the slider", "[SuperConfirm]")
{
    for (int key = 0; key < 2; ++key) {
        State s;
        REQUIRE(s.toggle_key(key));
        REQUIRE(s.phase() == Phase::OneKey);
        REQUIRE_FALSE(s.slider_enabled());
        REQUIRE_FALSE(s.set_slider(100));
        REQUIRE_FALSE(s.may_fire());
        REQUIRE(s.ritual_progress() == 0.25);
    }
}

TEST_CASE("Both keys arm the slider but do not fire on their own", "[SuperConfirm]")
{
    State s = armed_state();
    REQUIRE(s.phase() == Phase::Armed);
    REQUIRE(s.slider_enabled());
    REQUIRE_FALSE(s.authorized);
    REQUIRE_FALSE(s.may_fire());
    REQUIRE(s.ritual_progress() == 0.5);
}

TEST_CASE("A partial slide is only sliding and snaps back on release", "[SuperConfirm]")
{
    State s = armed_state();
    REQUIRE(s.set_slider(60));
    REQUIRE(s.phase() == Phase::Sliding);
    REQUIRE(s.slider_percent == 60);
    REQUIRE_FALSE(s.authorized);
    REQUIRE_FALSE(s.may_fire());
    REQUIRE(s.ritual_progress() == 0.8);
    s.release_slider();
    REQUIRE(s.slider_percent == 0);
    REQUIRE(s.phase() == Phase::Armed);
    REQUIRE_FALSE(s.may_fire());
}

TEST_CASE("A full slide with both keys on authorizes exactly once", "[SuperConfirm]")
{
    State s = armed_state();
    REQUIRE(s.set_slider(100));
    REQUIRE(s.phase() == Phase::Authorized);
    REQUIRE(s.authorized);
    REQUIRE(s.may_fire());
    REQUIRE(s.ritual_progress() == 1.0);
    // The latch is spent: further slider input is refused, keys are frozen.
    REQUIRE_FALSE(s.slider_enabled());
    REQUIRE_FALSE(s.set_slider(0));
    REQUIRE(s.slider_percent == 100);
    REQUIRE(s.toggle_key(0));
    REQUIRE(s.key_a);
    REQUIRE(s.may_fire());
}

TEST_CASE("Slider values are clamped to the 0..100 range", "[SuperConfirm]")
{
    State s = armed_state();
    REQUIRE(s.set_slider(-40));
    REQUIRE(s.slider_percent == 0);
    REQUIRE(s.phase() == Phase::Armed);
    REQUIRE(s.set_slider(400));
    REQUIRE(s.slider_percent == 100);
    REQUIRE(s.may_fire());
}

TEST_CASE("Turning a key off mid-slide disarms and resets the slider", "[SuperConfirm]")
{
    State s = armed_state();
    REQUIRE(s.set_slider(90));
    REQUIRE_FALSE(s.toggle_key(1));
    REQUIRE(s.phase() == Phase::OneKey);
    REQUIRE(s.slider_percent == 0);
    REQUIRE_FALSE(s.slider_enabled());
    REQUIRE_FALSE(s.set_slider(100));
    REQUIRE_FALSE(s.may_fire());
    // Re-arming starts the slide from rest again.
    REQUIRE(s.toggle_key(1));
    REQUIRE(s.phase() == Phase::Armed);
    REQUIRE(s.slider_percent == 0);
}

TEST_CASE("Cancel (emergency exit) closes the gate permanently", "[SuperConfirm]")
{
    SECTION("from untouched")
    {
        State s;
        s.cancel();
        REQUIRE(s.phase() == Phase::Cancelled);
        REQUIRE_FALSE(s.may_fire());
        REQUIRE_FALSE(s.slider_enabled());
    }
    SECTION("from armed with a partial slide")
    {
        State s = armed_state();
        REQUIRE(s.set_slider(70));
        s.cancel();
        REQUIRE(s.phase() == Phase::Cancelled);
        REQUIRE_FALSE(s.may_fire());
        // Nothing after a cancel can reopen the gate.
        REQUIRE_FALSE(s.set_slider(100));
        s.toggle_key(0);
        s.toggle_key(0);
        REQUIRE_FALSE(s.slider_enabled());
        REQUIRE_FALSE(s.may_fire());
        REQUIRE(s.phase() == Phase::Cancelled);
    }
}

TEST_CASE("Escape behaves as cancel before authorization and is inert after", "[SuperConfirm]")
{
    State before = armed_state();
    REQUIRE(before.set_slider(99));
    before.cancel(); // the widget routes Escape here
    REQUIRE(before.phase() == Phase::Cancelled);
    REQUIRE_FALSE(before.may_fire());

    State after = armed_state();
    REQUIRE(after.set_slider(100));
    after.cancel(); // a late Escape must not un-authorize a spent gate
    REQUIRE(after.phase() == Phase::Authorized);
    REQUIRE_FALSE(after.cancelled);
    REQUIRE(after.may_fire());
}

TEST_CASE("Completion requires both keys AND a full slide, in any order of keys", "[SuperConfirm]")
{
    // key b first, then key a, then slide
    State s;
    s.toggle_key(1);
    REQUIRE_FALSE(s.set_slider(100));
    s.toggle_key(0);
    REQUIRE_FALSE(s.may_fire());
    REQUIRE(s.set_slider(50));
    REQUIRE_FALSE(s.may_fire());
    REQUIRE(s.set_slider(100));
    REQUIRE(s.may_fire());

    // Direct field tampering that skips the slide still fails may_fire().
    State forged;
    forged.authorized = true;
    REQUIRE_FALSE(forged.may_fire());
    forged.key_a = forged.key_b = true;
    REQUIRE_FALSE(forged.may_fire());
    forged.slider_percent = 100;
    REQUIRE(forged.may_fire());
    forged.cancelled = true;
    REQUIRE_FALSE(forged.may_fire());
}
