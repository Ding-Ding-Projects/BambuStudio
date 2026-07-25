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

#include <wx/timer.h>

#ifdef _WIN32
// SAPI is used through IDispatch late binding ("SAPI.SpVoice") instead of
// <sapi.h>: that header's unqualified `byte` is ambiguous against std::byte
// under this project's PCH, and late binding needs no import libraries.
#include <windows.h>
#include <oleauto.h>
#endif

namespace Slic3r { namespace GUI { namespace TtsNarrator {

namespace {

constexpr int kCooldownSec = 20;
constexpr int kPollMs      = 3000;

struct QueuedLine
{
    wxString    line;
    std::string category;
};

std::deque<QueuedLine> s_queue;
std::map<std::string, std::chrono::steady_clock::time_point> s_last_spoken;
bool s_speaking = false;

#ifdef _WIN32
IDispatch *voice()
{
    static IDispatch *s_voice = []() -> IDispatch * {
        ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        CLSID clsid;
        if (FAILED(::CLSIDFromProgID(L"SAPI.SpVoice", &clsid)))
            return nullptr;
        IDispatch *disp = nullptr;
        if (FAILED(::CoCreateInstance(clsid, nullptr, CLSCTX_ALL, IID_IDispatch, (void **) &disp)))
            return nullptr;
        return disp;
    }();
    return s_voice;
}
#endif

bool narrator_enabled()
{
    return wxGetApp().app_config->get("narrator_enabled") == "true";
}

void speak_local(const wxString &line)
{
#ifdef _WIN32
    IDispatch *v = voice();
    if (v == nullptr)
        return;
    DISPID dispid = 0;
    OLECHAR *name = const_cast<OLECHAR *>(L"Speak");
    if (FAILED(v->GetIDsOfNames(IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &dispid)))
        return;
    // Flags 1|2 = SVSFlagsAsync | SVSFPurgeBeforeSpeak: one utterance at a
    // time, a new line replaces a still-playing superseded one.
    VARIANT args[2];
    ::VariantInit(&args[0]);
    ::VariantInit(&args[1]);
    args[1].vt      = VT_BSTR;
    args[1].bstrVal = ::SysAllocString(line.wc_str());
    args[0].vt      = VT_I4;
    args[0].lVal    = 1 | 2;
    DISPPARAMS params { args, nullptr, 2, 0 };
    v->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, nullptr, nullptr, nullptr);
    ::SysFreeString(args[1].bstrVal);
#else
    (void) line;
#endif
}

void pump_queue()
{
    if (s_queue.empty())
        return;
    QueuedLine next = std::move(s_queue.front());
    s_queue.pop_front();
    s_last_spoken[next.category] = std::chrono::steady_clock::now();
    speak_local(next.line);
    // Optional network speakers via Home Assistant (fire-and-forget).
    HomeAssistant::speak_on_speakers(next.line);
}

// --- printer state watch ----------------------------------------------------

class NarratorTimer : public wxTimer
{
public:
    void Notify() override
    {
        pump_queue();
        if (!narrator_enabled())
            return;
        DeviceManager *manager = wxGetApp().getDeviceManager();
        MachineObject *obj = manager != nullptr ? manager->get_selected_machine() : nullptr;
        if (obj == nullptr)
            return;
        const std::string status = obj->print_status;
        if (!m_last_status.empty() && status != m_last_status) {
            // TRN: TTS lines for printer state changes.
            if (status == "RUNNING")      say(_L("Printing started."), "state");
            else if (status == "FINISH") {
                say(_L("Print finished."), "state");
                if (wxGetApp().app_config->get("ha_flash_on_finish") == "true")
                    HomeAssistant::flash_lights(0, 200, 80); // green pulse
            }
            else if (status == "PAUSE")   say(_L("Print paused."), "state");
            else if (status == "FAILED") {
                say(_L("Print failed."), "error");
                if (wxGetApp().app_config->get("ha_flash_on_error") == "true")
                    HomeAssistant::flash_lights(230, 30, 30); // red flash
            }
        }
        m_last_status = status;
        const int error = obj->print_error;
        if (error != 0 && error != m_last_error) {
            // TRN: TTS line for a printer error; %06X is the hexadecimal error code.
            say(wxString::Format(_L("Printer error %06X. Check the device screen for details."),
                                 (unsigned) error), "error");
            if (wxGetApp().app_config->get("ha_flash_on_error") == "true")
                HomeAssistant::flash_lights(230, 30, 30);
        }
        m_last_error = error;
    }

private:
    std::string m_last_status;
    int         m_last_error { 0 };
};

NarratorTimer *timer()
{
    static NarratorTimer *s_timer = new NarratorTimer(); // app-lifetime
    return s_timer;
}

} // namespace

void say(const wxString &line, const std::string &category)
{
    if (!narrator_enabled())
        return;
    const bool is_error = category == "error";
    if (!is_error) {
        const auto it = s_last_spoken.find(category);
        if (it != s_last_spoken.end() &&
            std::chrono::steady_clock::now() - it->second < std::chrono::seconds(kCooldownSec))
            return; // cooldown: narration stays infrequent
    }
    // Replace a superseded queued line of the same category rather than stack.
    for (auto &queued : s_queue)
        if (queued.category == category) {
            queued.line = line;
            return;
        }
    s_queue.push_back({line, category});
    if (is_error)
        pump_queue(); // error narration is never delayed
}

void say_now(const wxString &line)
{
    speak_local(line);
    HomeAssistant::speak_on_speakers(line);
}

void install()
{
    timer()->Start(kPollMs);
}

} } } // namespace Slic3r::GUI::TtsNarrator
