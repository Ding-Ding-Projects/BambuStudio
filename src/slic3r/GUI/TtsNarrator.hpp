#ifndef slic3r_GUI_TtsNarrator_hpp_
#define slic3r_GUI_TtsNarrator_hpp_

#include <wx/string.h>
#include "PersonalModes/SapiVoice.hpp"
#include "Narration/NarrationEvents.hpp"
#include "Narration/NarrationPolicy.hpp"
#include "Narration/VoiceCatalog.hpp"
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
//   * local output uses the selected SAPI voice; the separately configured
//     external mirror retains its existing fire-and-forget delivery contract.
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
enum class ExternalMirrorStatus { Unconfigured, PlaybackCompletionUnavailable };
ExternalMirrorStatus external_mirror_status();
void set_quiet(bool quiet, bool screen_reader_active);
// Environment verdict from NarratorEnvironment (quiet hours, Focus Assist, ...):
// Held cancels everything, ErrorsOnly keeps only queued errors.
void set_environment_state(Narration::EnvironmentState state);

// GUI-thread only. Maps an event summary to the three funny-level tones, applies
// the master/track/quiet policy, de-duplicates and enqueues on the event's own
// track. The utterance never contains a path, account name or token.
void narrate(const Narration::Event& event);
// Speaks a short sample in one language with the voice currently selected for it.
// Explicit user action: bypasses the master and track switches, not a held environment.
void preview(bool cantonese);
// Every voice the platform lists, including OneCore voices that cannot be spoken yet.
std::vector<Narration::CatalogVoice> catalog();

// Deprecated single-language producers; new code should call narrate() or
// say_tracks(). Both now travel on the Accessibility track.
void say(const wxString &line, const std::string &category = "state");

// Queue regardless of narrator_enabled, for flows with their
// own explicit consent control (e.g. the filament scanner's TTS checkbox).
void say_now(const wxString &line);

} } } // namespace Slic3r::GUI::TtsNarrator

#endif // slic3r_GUI_TtsNarrator_hpp_
