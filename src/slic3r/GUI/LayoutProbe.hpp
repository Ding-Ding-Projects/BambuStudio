#ifndef slic3r_GUI_LayoutProbe_hpp_
#define slic3r_GUI_LayoutProbe_hpp_

// Runtime layout probe: an off-by-default walker that measures every top-level
// window's child tree and every wxBoxSizer row, and writes one JSON object per
// window (NDJSON) so a headless capture run can detect clipping mechanically.
//
// Why a probe rather than a screenshot: an over-subscribed wxBoxSizer row does
// NOT overflow. It pays proportion-0 items in full and starves the items after
// them to zero width, so the starved control simply vanishes while every child
// still reports a rectangle inside its parent (HANDOFF.md §3.3 and §6.9 record
// two primary controls lost this way). The only way to see that is to sum the
// children's minimum sizes against the row's allocation, which is what this
// does. It also reports labels whose text extent exceeds their client width
// without an ellipsize style, and windows whose rect leaves the parent client.
//
// Activation: set BAMBU_LAYOUT_PROBE before launch.
//   BAMBU_LAYOUT_PROBE=1            write <data_dir>/log/layout-probe-<pid>-<n>.jsonl
//   BAMBU_LAYOUT_PROBE=<dir>        write <dir>/layout-probe-<pid>-<n>.jsonl
//   BAMBU_LAYOUT_PROBE_TAG=<text>   free-form tuple label copied into the header record
// A dump runs once after the main frame is first shown, and again whenever the
// process receives WM_COPYDATA with dwData == 2 and payload
// L"layout-probe[ <path>]", which is how the driver asks for a dump after it
// has opened a dialog. When the variable is unset the cost is one env read.

#include <string>

class wxWindow;

namespace Slic3r { namespace GUI { namespace LayoutProbe {

// True when BAMBU_LAYOUT_PROBE is set to a non-empty value.
bool enabled();

// Arm the one-shot dump that fires after `frame` is shown and idle. No-op
// when the probe is disabled.
void install(wxWindow *frame);

// Walk every top-level window now and write the report. `reason` lands in the
// header record. An empty `out_path` uses the default location. Returns the
// path written, or an empty string on failure (the failure is logged).
std::string dump(const std::string &reason, const std::string &out_path = std::string());

// Handle a WM_COPYDATA command payload. Returns true when the payload was a
// probe command (whether or not the dump succeeded).
bool handle_command(const std::wstring &payload);

}}} // namespace Slic3r::GUI::LayoutProbe

#endif // slic3r_GUI_LayoutProbe_hpp_
