#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "slic3r/GUI/PrintWorkflowState.hpp"

using namespace Slic3r::GUI::PrintWorkflowState;

TEST_CASE("Saved printer mapping applies only to a fresh project with inherited plates")
{
    // Reopening with the same saved choice is allowed after a fresh reset.
    REQUIRE(may_apply_saved_mapping(true, true, true));
    REQUIRE(may_apply_saved_mapping(true, true, true));
    // Importing a project or editing one plate takes precedence over the saved choice.
    REQUIRE_FALSE(may_apply_saved_mapping(false, true, true));
    REQUIRE_FALSE(may_apply_saved_mapping(true, false, true));
    REQUIRE_FALSE(may_apply_saved_mapping(true, true, false));
}

TEST_CASE("Slice continuation is bound to its generation and printable plate")
{
    REQUIRE(may_open_print_setup(7, 7, true, true, true));
    REQUIRE_FALSE(may_open_print_setup(0, 0, true, true, true));
    REQUIRE_FALSE(may_open_print_setup(7, 8, true, true, true));
    REQUIRE_FALSE(may_open_print_setup(7, 7, false, true, true));
    REQUIRE_FALSE(may_open_print_setup(7, 7, true, false, true));
    REQUIRE_FALSE(may_open_print_setup(7, 7, true, true, false));
}

TEST_CASE("Combined slice actions consume exactly once and keep send separate from print")
{
    int plate = 0;
    for (auto action : {SliceOutputAction::Print, SliceOutputAction::Send}) {
        PendingSliceOutput pending;
        pending.arm(action, &plate, 2, 11);
        pending.native_generation = 25;
        REQUIRE(pending.consume(11, 25, &plate, 2, &plate, true, true) == action);
        REQUIRE(pending.action == SliceOutputAction::None);
        REQUIRE(pending.consume(11, 25, &plate, 2, &plate, true, true) == SliceOutputAction::None);
    }
}

TEST_CASE("A queued old cancellation cannot consume a new slice continuation")
{
    int plate = 0;
    PendingSliceOutput pending;
    pending.arm(SliceOutputAction::Send, &plate, 0, 9);
    pending.native_generation = 30;
    // The production completion handler must reject before stopping the worker
    // or consuming pending state. This sequence models that event dispatch.
    const auto dispatch = [&](std::uint64_t event, bool succeeded) {
        if (!is_current_slice_event(event, 30)) return SliceOutputAction::None;
        return pending.consume(9, 30, &plate, 0, &plate, succeeded, true);
    };
    REQUIRE(dispatch(29, false) == SliceOutputAction::None);
    REQUIRE(pending.action == SliceOutputAction::Send);
    REQUIRE(dispatch(30, true) == SliceOutputAction::Send);
}

TEST_CASE("Cancelled failed changed and replaced slices never open an output dialog")
{
    int plate = 0, replacement = 0;
    const auto armed = [&] {
        PendingSliceOutput pending;
        pending.arm(SliceOutputAction::Print, &plate, 1, 17);
        pending.native_generation = 42;
        return pending;
    };
    auto pending = armed();
    pending.clear(); // Explicit cancellation, project replacement or shutdown.
    REQUIRE(pending.consume(17, 42, &plate, 1, &plate, true, true) == SliceOutputAction::None);
    pending = armed();
    REQUIRE(pending.consume(17, 42, &plate, 1, &plate, false, true) == SliceOutputAction::None);
    pending = armed();
    REQUIRE(pending.consume(17, 43, &plate, 1, &plate, true, true) == SliceOutputAction::None);
    pending = armed();
    REQUIRE(pending.consume(18, 42, &plate, 1, &plate, true, true) == SliceOutputAction::None);
    pending = armed();
    REQUIRE(pending.consume(17, 42, &replacement, 1, &replacement, true, true) == SliceOutputAction::None);
    pending = armed();
    REQUIRE(pending.consume(17, 42, &plate, 0, &plate, true, true) == SliceOutputAction::None);
    pending = armed();
    REQUIRE(pending.consume(17, 42, &plate, 1, &plate, true, false) == SliceOutputAction::None);
    pending = armed();
    pending.native_generation = 0; // Validation refused before a native run existed.
    REQUIRE(pending.consume(17, 0, &plate, 1, &plate, true, true) == SliceOutputAction::None);
}
