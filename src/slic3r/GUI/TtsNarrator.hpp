#ifndef slic3r_GUI_TtsNarrator_hpp_
#define slic3r_GUI_TtsNarrator_hpp_

#include <wx/string.h>
#include "PersonalModes/SapiVoice.hpp"
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace TtsNarrator {

// Spoken narrator for app/printer events, per the narrator rules:
//   * OFF by default (`narrator_enabled`); the user opts in (Preferences ▸
//     Other, or the Smart home dialog);
//   * one utterance at a time through a serialized queue — a superseded
//     queued line of the same category is REPLACED, never stacked;
//   * per-category cooldown (default 20s) keeps narration infrequent;
//   * error lines preserve facts and bypass cooldown;
//   * output uses the selected local SAPI voice; remote media output requires
//     a separate completion-aware transport and is not mirrored here.
//
// install() also arms the printer watch: the selected machine's print stage
// and error code are polled and state CHANGES are narrated ("Printing
// started", "Print finished", "Printer error: ..."), errors verbatim.
void install();
void shutdown();
void say_tracks(const wxString& english, const wxString& cantonese, const std::string& category = "state", bool explicit_consent = false);
std::vector<PersonalModes::VoiceInfo> voices();
PersonalModes::VoiceStatus voice_status(bool cantonese);
bool delivery_failed();
void set_quiet(bool quiet, bool screen_reader_active);

// Queue a line under a category ("state", "error", "scan", ...). Categories
// other than "error" respect the cooldown; "error" always speaks.
void say(const wxString &line, const std::string &category = "state");

// Queue regardless of narrator_enabled, for flows with their
// own explicit consent control (e.g. the filament scanner's TTS checkbox).
void say_now(const wxString &line);

} } } // namespace Slic3r::GUI::TtsNarrator

#endif // slic3r_GUI_TtsNarrator_hpp_
