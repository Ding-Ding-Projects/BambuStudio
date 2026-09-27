#ifndef slic3r_GUI_PrinterWatch_hpp_
#define slic3r_GUI_PrinterWatch_hpp_

namespace Slic3r { namespace GUI { namespace PrinterWatch {

// AI printer watch: while the app is running (the user need not be actively
// watching), periodically capture a frame of the printer's live camera view
// and ask a LOCAL model to summarize it. A normal frame becomes a quiet info
// toast; a frame that suggests a failure (spaghetti, detachment, blobs)
// becomes a persistent warning toast describing what likely happened and how
// to fix it.
//
// Strictly local and opt-in:
//   * OFF by default (`printer_watch_enabled`); frames never leave the
//     machine — the backend is the Ollama HTTP API on localhost
//     (`printer_watch_endpoint`, default http://127.0.0.1:11434) with a
//     vision-capable model tag (`printer_watch_model`, default qwen2.5vl —
//     Gemma 3 also works; text-only tags such as gpt-oss cannot read frames
//     and will say so in their reply).
//   * `printer_watch_interval` minutes between checks (default 5, min 1).
//   * The frame source is the Device page's live-view window; when no stream
//     is rendering (blank/black frame) the tick is skipped silently.
//
// install() arms the timer; it re-reads the config every tick, so toggling
// the feature in Preferences applies without a restart.
void install();

// Run one check immediately (used by the debug/manual path and tests).
void check_now();

} } } // namespace Slic3r::GUI::PrinterWatch

#endif // slic3r_GUI_PrinterWatch_hpp_
