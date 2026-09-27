#pragma once

#include <cstdint>

namespace Slic3r::GUI::PrintWorkflowState {

inline bool may_apply_saved_mapping(bool fresh_project, bool all_plates_inherit, bool saved_mode_is_auto)
{
    return fresh_project && all_plates_inherit && saved_mode_is_auto;
}

inline bool may_open_print_setup(std::uint64_t requested_generation, std::uint64_t current_generation,
                                bool same_plate, bool slice_succeeded, bool printable)
{
    return requested_generation != 0 && requested_generation == current_generation && same_plate &&
           slice_succeeded && printable;
}

} // namespace Slic3r::GUI::PrintWorkflowState
