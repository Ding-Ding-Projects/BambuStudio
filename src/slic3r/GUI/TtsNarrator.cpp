#include "TtsNarrator.hpp"

#include "GUI_App.hpp"
#include "HomeAssistant.hpp"
#include "I18N.hpp"
#include "DeviceManager.hpp"
#include "DeviceCore/DevManager.h"

#include "libslic3r/AppConfig.hpp"

#include <chrono>
#include <deque>
#include <map>
#include <string>
#include <memory>

#include <wx/timer.h>
#include "PersonalModes/SpeechQueue.hpp"
#include "PersonalModes/SapiVoice.hpp"
#include "PersonalModes/SchoolMode.hpp"
#include "Narration/NarrationTransitions.hpp"
#include "Narration/OneCoreVoices.hpp"
#include "LanguageMode.hpp"

#ifdef _WIN32
// SAPI is used through IDispatch late binding ("SAPI.SpVoice") instead of
// <sapi.h>: that header's unqualified `byte` is ambiguous against std::byte
// under this project's PCH, and late binding needs no import libraries.
#include <windows.h>
#include <oleauto.h>
#endif

namespace Slic3r { namespace GUI { namespace TtsNarrator {

namespace {

constexpr int kPollMs = 100;
PersonalModes::SpeechQueue s_queue;
std::unique_ptr<PersonalModes::SapiVoice> s_voice;
std::vector<PersonalModes::VoiceInfo> s_voices;
bool s_backend_inflight = false;
bool s_quiet = false;
bool s_screen_reader = false;
bool s_delivery_failed = false;
bool s_last_suppressed = true;
bool s_errors_only = false;
Narration::PrinterTransitionDetector s_detector;
Narration::EventDeduper s_deduper;
std::vector<Narration::CatalogVoice> s_catalog;

PersonalModes::SapiVoice& voice() {
    if (!s_voice) s_voice = std::make_unique<PersonalModes::SapiVoice>();
    return *s_voice;
}

bool narrator_enabled() {
    return wxGetApp().app_config->get("narrator_enabled") == "true";
}

int number(const char* key) {
    try { return std::clamp(std::stoi(wxGetApp().app_config->get(key)), -10, 10); }
    catch (...) { return 0; }
}

void pump_queue();

bool config_bool(const char* key, bool fallback) {
    return Narration::parse_bool(wxGetApp().app_config->get(key), fallback);
}

// Shared enqueue path. `track` keeps replacement and cooldown independent.
void enqueue_tracks(const wxString& english, const wxString& cantonese, const std::string& category,
                    Narration::Track track, bool consent, bool cooldown_exempt) {
    PersonalModes::SpeechEvent event;
    event.category = category;
    event.explicit_consent = consent;
    event.track = Narration::track_name(track);
    event.cooldown_exempt = cooldown_exempt;
    auto language = wxGetApp().app_config->get("narrator_language");
    if (PersonalModes::school_presentation_suppressed.load()) language = "en";
    if (language != "yue_HK" && !english.empty())
        event.tracks.push_back({english.ToStdWstring(), PersonalModes::SpeechLanguage::English});
    if ((language == "yue_HK" || language == "both") && !cantonese.empty())
        event.tracks.push_back({cantonese.ToStdWstring(), PersonalModes::SpeechLanguage::Cantonese});
    if (event.tracks.empty()) return;
    if (!s_queue.enqueue(std::move(event))) s_delivery_failed = true;
    pump_queue();
}

void pump_queue() {
    const bool suppressed = PersonalModes::school_presentation_suppressed.load();
    if (suppressed && !s_last_suppressed) {
        s_queue.cancel();
        if (s_backend_inflight) voice().stop();
        s_backend_inflight = false;
    }
    s_last_suppressed = suppressed;
    if (s_quiet || s_screen_reader) {
        s_queue.cancel();
        if (s_backend_inflight) voice().stop();
        s_backend_inflight = false;
        return;
    }
    if (s_errors_only) s_queue.drop_non_errors();
    const bool completed = !s_backend_inflight || voice().complete();
    if (!completed) return;
    s_backend_inflight = false;
    const auto next = s_queue.next(true, narrator_enabled(), false);
    if (!next) return;
    // Preserve the user's separately configured external announcement mirror.
    // Its service request acknowledges dispatch, not remote playback completion.
    // Tracks contain source/localized copy, never the private display transform.
    HomeAssistant::speak_on_speakers(wxString(next->text));
    const bool cantonese = next->language == PersonalModes::SpeechLanguage::Cantonese;
    const auto selected = wxString::FromUTF8(wxGetApp().app_config->get(cantonese ? "narrator_voice_yue" : "narrator_voice_en")).ToStdWstring();
    const auto resolved = voice().resolve(s_voices, cantonese, selected);
    if (!resolved.available) { s_delivery_failed = true; return; }
    s_backend_inflight = voice().speak(next->text, resolved.effective_id,
        number(cantonese ? "narrator_rate_yue" : "narrator_rate_en"), number(cantonese ? "narrator_pitch_yue" : "narrator_pitch_en"));
    s_delivery_failed = !s_backend_inflight;
}
// --- printer state watch ----------------------------------------------------

Narration::EnvironmentState environment_now() {
    if (s_quiet || s_screen_reader) return Narration::EnvironmentState::Held;
    return s_errors_only ? Narration::EnvironmentState::ErrorsOnly : Narration::EnvironmentState::Open;
}

class NarratorTimer : public wxTimer
{
public:
    void Notify() override
    {
        pump_queue();
        if (++m_ticks % 30 != 0) return;
        s_voices = voice().enumerate();
        if (!narrator_enabled())
            return;
        DeviceManager *manager = wxGetApp().getDeviceManager();
        MachineObject *obj = manager != nullptr ? manager->get_selected_machine() : nullptr;
        if (obj == nullptr)
            return;
        // The detector fires on transitions only; the machine id is compared, never spoken.
        Narration::PrinterSnapshot snapshot;
        snapshot.machine = obj->get_dev_id();
        snapshot.status  = obj->print_status;
        snapshot.stage   = obj->mc_print_stage;
        snapshot.error   = obj->print_error;
        for (const auto& event : s_detector.observe(snapshot)) {
            narrate(event);
            if (event.kind == Narration::EventKind::PrintFinished && wxGetApp().app_config->get("ha_flash_on_finish") == "true")
                HomeAssistant::flash_lights(0, 200, 80); // green pulse
            if ((event.kind == Narration::EventKind::PrintFailed || event.kind == Narration::EventKind::PrinterError ||
                 event.kind == Narration::EventKind::AmsError) && wxGetApp().app_config->get("ha_flash_on_error") == "true")
                HomeAssistant::flash_lights(230, 30, 30); // red flash
        }
    }

private:
    unsigned m_ticks = 0;
};

NarratorTimer *timer()
{
    static NarratorTimer *s_timer = new NarratorTimer(); // app-lifetime
    return s_timer;
}

} // namespace

void narrate(const Narration::Event& event)
{
    using namespace Narration;
    const bool consent = event.kind == EventKind::VoicePreview;
    AdmitInput admit_in;
    admit_in.master           = narrator_enabled();
    admit_in.track_enabled    = config_bool(track_key(track_of(event.kind)), true);
    admit_in.kind             = event.kind;
    admit_in.explicit_consent = consent;
    admit_in.environment      = environment_now();
    if (!admit(admit_in)) return;
    if (!consent && !s_deduper.admit(event)) return;
    auto& service = I18N::language_mode_service();
    const auto msgid_for = [&](I18N::FunnyLanguage language) {
        return wxString::FromUTF8(utterance_msgid(event.kind, tone_for_level(service.funny_level(language))));
    };
    wxString english = service.narration(msgid_for(I18N::FunnyLanguage::English)).primary;
    wxString cantonese = service.narration(msgid_for(I18N::FunnyLanguage::Cantonese)).secondary;
    if (has_code(event.kind)) {
        english = wxString::Format(english, static_cast<unsigned>(event.code));
        if (!cantonese.empty()) cantonese = wxString::Format(cantonese, static_cast<unsigned>(event.code));
    }
    // Transitions are already de-duplicated, so the legacy per-category cooldown is skipped.
    enqueue_tracks(english, cantonese, category_of(event.kind), track_of(event.kind), consent, true);
}

void preview(bool cantonese)
{
    using namespace Narration;
    AdmitInput admit_in;
    admit_in.kind             = EventKind::VoicePreview;
    admit_in.explicit_consent = true;
    admit_in.environment      = environment_now();
    if (!admit(admit_in)) return;
    const wxString msgid = wxString::FromUTF8(utterance_msgid(EventKind::VoicePreview, Tone::Serious));
    const auto tracks = I18N::language_mode_service().narration(msgid);
    const wxString text = cantonese ? tracks.secondary : tracks.primary;
    if (text.empty()) { s_delivery_failed = true; return; } // no Cantonese wording available
    PersonalModes::SpeechEvent event;
    event.category = "preview";
    event.explicit_consent = true;
    event.track = track_name(Track::Accessibility);
    event.cooldown_exempt = true;
    event.tracks.push_back({text.ToStdWstring(), cantonese ? PersonalModes::SpeechLanguage::Cantonese : PersonalModes::SpeechLanguage::English});
    if (!s_queue.enqueue(std::move(event))) s_delivery_failed = true;
    pump_queue();
}

void say_tracks(const wxString& english, const wxString& cantonese, const std::string& category, bool consent)
{
    if (!consent && (!narrator_enabled() || !config_bool(Narration::track_key(Narration::Track::Accessibility), true))) return;
    // Producers that supply their own tracks travel on the accessibility track,
    // so they never replace or throttle printer or application events.
    enqueue_tracks(english, cantonese, category, Narration::Track::Accessibility, consent, false);
}

void say(const wxString& line, const std::string& category)
{
    // Deprecated: one already-localized line. Prefer narrate() or say_tracks().
    // The line follows the interface language, as before, but now rides the
    // accessibility track instead of sharing printer categories.
    if (!narrator_enabled()) return;
    const bool cantonese = wxGetApp().app_config->get("language") == "yue_HK" && !PersonalModes::school_presentation_suppressed.load();
    say_tracks(cantonese ? wxString() : line, cantonese ? line : wxString(), category, false);
}

void say_now(const wxString& line)
{
    // Deprecated: explicit-consent single line (filament scanner used it before
    // migrating to say_tracks). Kept for source compatibility.
    const bool cantonese = wxGetApp().app_config->get("language") == "yue_HK" && !PersonalModes::school_presentation_suppressed.load();
    say_tracks(cantonese ? wxString() : line, cantonese ? line : wxString(), "preview", true);
}

std::vector<PersonalModes::VoiceInfo> voices()
{
    s_voices = voice().enumerate();
    return s_voices;
}

std::vector<Narration::CatalogVoice> catalog()
{
    std::vector<Narration::CatalogVoice> sapi;
    for (const auto& v : voices()) {
        Narration::CatalogVoice c; c.id = v.id; c.name = v.name; c.english = v.english; c.cantonese = v.cantonese;
        sapi.push_back(std::move(c));
    }
    s_catalog = Narration::merge(sapi, Narration::enumerate_onecore_voices());
    return s_catalog;
}

PersonalModes::VoiceStatus voice_status(bool cantonese)
{
    const auto key = cantonese ? "narrator_voice_yue" : "narrator_voice_en";
    return voice().resolve(s_voices, cantonese, wxString::FromUTF8(wxGetApp().app_config->get(key)).ToStdWstring());
}

bool delivery_failed() { return s_delivery_failed; }
ExternalMirrorStatus external_mirror_status()
{
    return HomeAssistant::configured() && !wxGetApp().app_config->get("ha_speakers").empty()
        ? ExternalMirrorStatus::PlaybackCompletionUnavailable : ExternalMirrorStatus::Unconfigured;
}
void set_quiet(bool quiet, bool screen_reader_active) { s_quiet = quiet; s_screen_reader = screen_reader_active; pump_queue(); }
void set_environment_state(Narration::EnvironmentState state)
{
    s_errors_only = state == Narration::EnvironmentState::ErrorsOnly;
    pump_queue();
}
void install() { s_voices = voice().enumerate(); timer()->Start(kPollMs); }
void shutdown() { timer()->Stop(); s_queue.cancel(); if (s_voice) s_voice->stop(); s_voice.reset(); s_backend_inflight = false; }
} } } // namespace Slic3r::GUI::TtsNarrator
