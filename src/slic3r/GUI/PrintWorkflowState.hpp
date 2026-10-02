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

enum class SliceOutputAction { None, Print, Send };

// Shared by the native event consumer and the focused workflow tests.
inline bool is_current_slice_event(std::uint64_t event_generation, std::uint64_t current_generation)
{
    return event_generation == current_generation;
}

struct PendingSliceOutput {
    SliceOutputAction action{SliceOutputAction::None};
    const void* plate{nullptr};
    int plate_index{-1};
    std::uint64_t request_generation{0};
    std::uint64_t native_generation{0};

    void clear() { *this = {}; }
    void arm(SliceOutputAction requested_action, const void* requested_plate, int index, std::uint64_t generation)
    {
        clear();
        action = requested_action;
        plate = requested_plate;
        plate_index = index;
        request_generation = generation;
    }
    SliceOutputAction consume(std::uint64_t request, std::uint64_t native, const void* selected_plate,
                              int selected_index, const void* processed_plate, bool success, bool printable)
    {
        const bool allowed = action != SliceOutputAction::None && native_generation != 0 &&
            is_current_slice_event(native_generation, native) &&
            may_open_print_setup(request_generation, request,
                plate != nullptr && plate == selected_plate && plate == processed_plate && plate_index == selected_index,
                success, printable);
        const auto result = allowed ? action : SliceOutputAction::None;
        clear(); // Consume before opening a modal dialog or entering any nested event loop.
        return result;
    }
};

} // namespace Slic3r::GUI::PrintWorkflowState
