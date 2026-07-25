#ifndef slic3r_GUI_TtsNarrator_hpp_
#define slic3r_GUI_TtsNarrator_hpp_

#include <wx/string.h>

namespace Slic3r { namespace GUI { namespace TtsNarrator {

// Spoken narrator for app/printer events, per the narrator rules:
//   * OFF by default (`narrator_enabled`); the user opts in (Preferences ▸
//     Other, or the Smart home dialog);
//   * one utterance at a time through a serialized queue — a superseded
//     queued line of the same category is REPLACED, never stacked;
//   * per-category cooldown (default 20s) keeps narration infrequent;
//   * error lines are always plain and accurate and are never suppressed by
//     the cooldown;
//   * output goes to the local voice (Windows SAPI) and, when configured, to
//     Home Assistant media players (`ha_speakers`) via tts.speak.
//
// install() also arms the printer watch: the selected machine's print stage
// and error code are polled and state CHANGES are narrated ("Printing
// started", "Print finished", "Printer error: ..."), errors verbatim.
void install();

// Queue a line under a category ("state", "error", "scan", ...). Categories
// other than "error" respect the cooldown; "error" always speaks.
void say(const wxString &line, const std::string &category = "state");

// Speak immediately regardless of narrator_enabled — for flows with their
// own explicit consent control (e.g. the filament scanner's TTS checkbox).
void say_now(const wxString &line);

} } } // namespace Slic3r::GUI::TtsNarrator

#endif // slic3r_GUI_TtsNarrator_hpp_
