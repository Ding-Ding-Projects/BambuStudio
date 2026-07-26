#include "PrinterWatch.hpp"

#include "GUI_App.hpp"
#include "I18N.hpp"
#include "MainFrame.hpp"
#include "Monitor.hpp"
#include "NotificationManager.hpp"
#include "Plater.hpp"
#include "StatusPanel.hpp"
#include "slic3r/Utils/Http.hpp"

#include "libslic3r/AppConfig.hpp"

#include "nlohmann/json.hpp"

#include <boost/log/trivial.hpp>

#include <atomic>
#include <string>
#include <thread>

#include <wx/base64.h>
#include <wx/image.h>
#include <wx/mstream.h>
#include <wx/timer.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Slic3r { namespace GUI { namespace PrinterWatch {

namespace {

std::atomic<bool> s_request_in_flight { false };

int interval_minutes()
{
    long minutes = 5;
    const std::string value = wxGetApp().app_config->get("printer_watch_interval");
    if (!value.empty())
        wxString(value).ToLong(&minutes);
    return std::max(1, static_cast<int>(minutes));
}

bool enabled()
{
    return wxGetApp().app_config->get("printer_watch_enabled") == "true";
}

std::string model_tag()
{
    std::string model = wxGetApp().app_config->get("printer_watch_model");
    return model.empty() ? "qwen2.5vl" : model;
}

std::string endpoint()
{
    std::string url = wxGetApp().app_config->get("printer_watch_endpoint");
    return url.empty() ? "http://127.0.0.1:11434" : url;
}

// Capture the live-view window into an image; empty result when the window
// is missing, hidden, or rendering nothing (uniform frame).
wxImage capture_live_view()
{
    MainFrame *frame = wxGetApp().mainframe;
    if (frame == nullptr || frame->m_monitor == nullptr)
        return {};
    StatusPanel *status = frame->m_monitor->get_status_panel();
    if (status == nullptr)
        return {};
    wxWindow *media = status->get_media_ctrl();
    if (media == nullptr || !media->IsShownOnScreen())
        return {};
    const wxSize size = media->GetClientSize();
    if (size.x < 64 || size.y < 64)
        return {};
#ifdef _WIN32
    wxBitmap bitmap(size.x, size.y);
    {
        wxMemoryDC mdc(bitmap);
        HDC hdc = (HDC) mdc.GetHDC();
        if (!::PrintWindow((HWND) media->GetHWND(), hdc, 2 /*PW_RENDERFULLCONTENT*/))
            return {};
    }
    wxImage image = bitmap.ConvertToImage();
    // Uniform frame == no stream rendering: sample a sparse grid.
    const unsigned char r0 = image.GetRed(0, 0), g0 = image.GetGreen(0, 0), b0 = image.GetBlue(0, 0);
    bool uniform = true;
    for (int y = 0; y < size.y && uniform; y += std::max(1, size.y / 8))
        for (int x = 0; x < size.x && uniform; x += std::max(1, size.x / 8))
            if (std::abs(image.GetRed(x, y) - r0) > 8 || std::abs(image.GetGreen(x, y) - g0) > 8 ||
                std::abs(image.GetBlue(x, y) - b0) > 8)
                uniform = false;
    if (uniform)
        return {};
    // Bound the upload: the model does not need more than ~768px.
    if (size.x > 768)
        image.Rescale(768, size.y * 768 / size.x, wxIMAGE_QUALITY_HIGH);
    return image;
#else
    return {};
#endif
}

std::string to_jpeg_base64(const wxImage &image)
{
    wxMemoryOutputStream stream;
    wxImage copy = image;
    if (!copy.SaveFile(stream, wxBITMAP_TYPE_JPEG))
        return {};
    const size_t length = stream.GetSize();
    std::string buffer(length, '\0');
    stream.CopyTo(buffer.data(), length);
    return wxBase64Encode(buffer.data(), length).ToStdString();
}

void notify(const wxString &text, bool problem)
{
    Plater *plater = wxGetApp().plater();
    if (plater == nullptr || plater->get_notification_manager() == nullptr)
        return;
    plater->get_notification_manager()->push_notification(
        NotificationType::CustomNotification,
        problem ? NotificationManager::NotificationLevel::WarningNotificationLevel
                : NotificationManager::NotificationLevel::RegularNotificationLevel,
        // TRN: prefix of the AI printer-watch toast; %s is the model's summary.
        into_u8(wxString::Format(_L("Printer watch: %s"), text)));
}

void run_check(std::string image_base64)
{
    // Worker thread: one bounded local HTTP call, result marshalled back.
    const std::string prompt =
        "You are watching a 3D printer camera frame. Reply with EXACTLY two lines. "
        "Line 1: OK or PROBLEM. Line 2: one or two short sentences summarizing what you see; "
        "if PROBLEM, say what likely went wrong (spaghetti, detachment, blob, stringing...) "
        "and one concrete fix suggestion.";
    nlohmann::json body = {
        {"model", model_tag()},
        {"prompt", prompt},
        {"images", {image_base64}},
        {"stream", false},
    };
    auto http = Http::post(endpoint() + "/api/generate");
    http.header("Content-Type", "application/json")
        .set_post_body(body.dump())
        .timeout_max(120)
        .on_complete([](std::string response_body, unsigned) {
            std::string reply;
            try {
                reply = nlohmann::json::parse(response_body).value("response", "");
            } catch (...) {}
            s_request_in_flight = false;
            if (reply.empty())
                return;
            const bool problem = reply.rfind("PROBLEM", 0) == 0;
            std::string summary = reply;
            if (const auto newline = summary.find('\n'); newline != std::string::npos)
                summary = summary.substr(newline + 1);
            while (!summary.empty() && (summary.front() == '\n' || summary.front() == ' '))
                summary.erase(summary.begin());
            wxTheApp->CallAfter([summary, problem]() {
                notify(wxString::FromUTF8(summary), problem);
            });
        })
        .on_error([](std::string, std::string error, unsigned) {
            s_request_in_flight = false;
            // Quietly log-only: a stopped Ollama must not nag every interval.
            BOOST_LOG_TRIVIAL(info) << "PrinterWatch: local model unavailable: " << error;
        })
        .perform_sync();
}

class WatchTimer : public wxTimer
{
public:
    void Notify() override
    {
        if (enabled())
            check_now();
        StartOnce(interval_minutes() * 60 * 1000); // re-arm with the current setting
    }
};

WatchTimer *watch_timer()
{
    static WatchTimer *s_timer = new WatchTimer(); // app-lifetime
    return s_timer;
}

} // namespace

void check_now()
{
    if (s_request_in_flight.exchange(true))
        return; // one outstanding request at a time
    const wxImage frame = capture_live_view();
    if (!frame.IsOk()) {
        s_request_in_flight = false;
        return; // no live view rendering -> nothing to summarize
    }
    std::string image_base64 = to_jpeg_base64(frame);
    if (image_base64.empty()) {
        s_request_in_flight = false;
        return;
    }
    std::thread([payload = std::move(image_base64)]() { run_check(payload); }).detach();
}

void install()
{
    watch_timer()->StartOnce(interval_minutes() * 60 * 1000);
}

} } } // namespace Slic3r::GUI::PrinterWatch
