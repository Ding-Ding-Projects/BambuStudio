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

PersonalModes::SapiVoice& voice() {
    if (!s_voice) s_voice = std::make_unique<PersonalModes::SapiVoice>();
    return *s_voice;
}

bool narrator_enabled() {
    return wxGetApp().app_config->get("narrator_enabled") == "true";
}

void say_event(const wxString& source, const std::string& category) {
    const auto tracks = I18N::language_mode_service().narration(source);
    say_tracks(tracks.primary, tracks.secondary, category);
}

int number(const char* key) {
    try { return std::clamp(std::stoi(wxGetApp().app_config->get(key)), -10, 10); }
    catch (...) { return 0; }
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
    const bool completed = !s_backend_inflight || voice().complete();
    if (!completed) return;
    s_backend_inflight = false;
    const auto next = s_queue.next(true, narrator_enabled(), false);
    if (!next) return;
    const bool cantonese = next->language == PersonalModes::SpeechLanguage::Cantonese;
    const auto selected = wxString::FromUTF8(wxGetApp().app_config->get(cantonese ? "narrator_voice_yue" : "narrator_voice_en")).ToStdWstring();
    const auto resolved = voice().resolve(s_voices, cantonese, selected);
    if (!resolved.available) { s_delivery_failed = true; return; }
    s_backend_inflight = voice().speak(next->text, resolved.effective_id,
        number(cantonese ? "narrator_rate_yue" : "narrator_rate_en"), number(cantonese ? "narrator_pitch_yue" : "narrator_pitch_en"));
    s_delivery_failed = !s_backend_inflight;
    // Remote media playback has no completion acknowledgment. It must not be
    // mirrored here, because that would invalidate serialized speech delivery.
}
// --- printer state watch ----------------------------------------------------

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
        const std::string status = obj->print_status;
        if (!m_last_status.empty() && status != m_last_status) {
            // TRN: TTS lines for printer state changes.
            if (status == "RUNNING")      say_event("Printing started.", "state");
            else if (status == "FINISH") {
                say_event("Print finished.", "state");
                if (wxGetApp().app_config->get("ha_flash_on_finish") == "true")
                    HomeAssistant::flash_lights(0, 200, 80); // green pulse
            }
            else if (status == "PAUSE")   say_event("Print paused.", "state");
            else if (status == "FAILED") {
                say_event("Print failed.", "error");
                if (wxGetApp().app_config->get("ha_flash_on_error") == "true")
                    HomeAssistant::flash_lights(230, 30, 30); // red flash
            }
        }
        m_last_status = status;
        const int error = obj->print_error;
        if (error != 0 && error != m_last_error) {
            // TRN: TTS line for a printer error; %06X is the hexadecimal error code.
            auto tracks = I18N::language_mode_service().narration("Printer error %06X. Check the device screen for details.");
            say_tracks(wxString::Format(tracks.primary, (unsigned)error),
                tracks.secondary.empty() ? wxString() : wxString::Format(tracks.secondary, (unsigned)error), "error");
            if (wxGetApp().app_config->get("ha_flash_on_error") == "true")
                HomeAssistant::flash_lights(230, 30, 30);
        }
        m_last_error = error;
    }

private:
    unsigned m_ticks = 0;
    std::string m_last_status;
    int         m_last_error { 0 };
};

NarratorTimer *timer()
{
    static NarratorTimer *s_timer = new NarratorTimer(); // app-lifetime
    return s_timer;
}

} // namespace

void say_tracks(const wxString& english, const wxString& cantonese, const std::string& category, bool consent)
{
    if (!consent && !narrator_enabled()) return;
    PersonalModes::SpeechEvent event;
    event.category = category;
    event.explicit_consent = consent;
    auto language = wxGetApp().app_config->get("narrator_language");
    if (PersonalModes::school_presentation_suppressed.load()) language = "en";
    if (language != "yue_HK" && !english.empty())
        event.tracks.push_back({english.ToStdWstring(), PersonalModes::SpeechLanguage::English});
    if ((language == "yue_HK" || language == "both") && !cantonese.empty())
        event.tracks.push_back({cantonese.ToStdWstring(), PersonalModes::SpeechLanguage::Cantonese});
    if (!s_queue.enqueue(std::move(event))) s_delivery_failed = true;
    pump_queue();
}

void say(const wxString& line, const std::string& category)
{
    // Legacy callers provide one already-localized line. The owning producer
    // should use say_tracks for language-independent event narration.
    PersonalModes::SpeechEvent event;
    if (!narrator_enabled()) return;
    event.category = category;
    const auto language = wxGetApp().app_config->get("language");
    event.tracks.push_back({line.ToStdWstring(), language == "yue_HK" && !PersonalModes::school_presentation_suppressed.load()
        ? PersonalModes::SpeechLanguage::Cantonese : PersonalModes::SpeechLanguage::English});
    if (!s_queue.enqueue(std::move(event))) s_delivery_failed = true;
    pump_queue();
}

void say_now(const wxString& line)
{
    PersonalModes::SpeechEvent event;
    event.category = "preview";
    event.explicit_consent = true;
    const auto language = wxGetApp().app_config->get("language");
    event.tracks.push_back({line.ToStdWstring(), language == "yue_HK" && !PersonalModes::school_presentation_suppressed.load()
        ? PersonalModes::SpeechLanguage::Cantonese : PersonalModes::SpeechLanguage::English});
    if (!s_queue.enqueue(std::move(event))) s_delivery_failed = true;
    pump_queue();
}

std::vector<PersonalModes::VoiceInfo> voices()
{
    s_voices = voice().enumerate();
    return s_voices;
}

PersonalModes::VoiceStatus voice_status(bool cantonese)
{
    const auto key = cantonese ? "narrator_voice_yue" : "narrator_voice_en";
    return voice().resolve(s_voices, cantonese, wxString::FromUTF8(wxGetApp().app_config->get(key)).ToStdWstring());
}

bool delivery_failed() { return s_delivery_failed; }
void set_quiet(bool quiet, bool screen_reader_active) { s_quiet = quiet; s_screen_reader = screen_reader_active; pump_queue(); }
void install() { s_voices = voice().enumerate(); timer()->Start(kPollMs); }
void shutdown() { timer()->Stop(); s_queue.cancel(); if (s_voice) s_voice->stop(); s_voice.reset(); s_backend_inflight = false; }
} } } // namespace Slic3r::GUI::TtsNarrator
