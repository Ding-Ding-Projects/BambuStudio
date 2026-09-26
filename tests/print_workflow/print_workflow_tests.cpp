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
