#ifndef slic3r_GUI_WorkflowPrintState_hpp_
#define slic3r_GUI_WorkflowPrintState_hpp_

namespace Slic3r { namespace GUI { namespace WorkflowPrint {

enum class Action { Prepare, Preview, Monitor, Output, OutputOptions, Slice, SliceAndPrint, SliceAndSend };
enum class Status { Empty, Slicing, NeedsSlice, Ready, Unavailable };

// A projection of MainFrame's existing authorities, not a second readiness policy.
struct Availability {
    bool has_content = false;
    bool slicing = false;
    bool slice_ready = false;
    bool slice_enabled = false;
    bool output_enabled = false;
    bool combined_enabled = false;

    Status status() const {
        if (slicing) return Status::Slicing;
        if (!has_content) return Status::Empty;
        if (!slice_ready) return Status::NeedsSlice;
        return output_enabled ? Status::Ready : Status::Unavailable;
    }

    bool allows(Action action) const {
        switch (action) {
        case Action::Output: return !slicing && output_enabled;
        case Action::Slice: return !slicing && slice_enabled;
        case Action::SliceAndPrint:
        case Action::SliceAndSend: return !slicing && combined_enabled;
        case Action::Prepare:
        case Action::Preview:
        case Action::Monitor:
        case Action::OutputOptions: return true;
        }
        return false;
    }
};

// Called only by a deliberate action. Readiness is refreshed synchronously at
// activation, so an enabled button from a previous plate cannot submit stale work.
template<class ReadCurrent, class RunAction>
bool dispatch(Action action, ReadCurrent read_current, RunAction run_action)
{
    if (!read_current().allows(action)) return false;
    run_action(action);
    return true;
}

}}} // namespace Slic3r::GUI::WorkflowPrint
#endif
