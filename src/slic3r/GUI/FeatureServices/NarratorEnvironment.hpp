#pragma once
#include "../TtsNarrator.hpp"
#include "../Narration/NarrationPolicy.hpp"
#include <wx/timer.h>
#include <ctime>
#include <functional>
#include <string>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace Slic3r::GUI::FeatureServices {
// Once a second, gathers everything that should quiet the narrator and hands the
// pure policy verdict (Narration::environment_state) to the narrator:
//   * the user's own Quiet narration switch,
//   * an active screen reader (SPI_GETSCREENREADER),
//   * the app's own quiet hours (narrator_quiet_hours_*),
//   * Windows Focus Assist / quiet time, full-screen and presentation states
//     (SHQueryUserNotificationState), when narrator_respect_focus_assist is on.
class NarratorEnvironment final : public wxEvtHandler {
public:
    using ConfigReader = std::function<std::string(const char*)>;

    NarratorEnvironment(std::function<bool()> quiet, ConfigReader config)
        : m_quiet(std::move(quiet)), m_config(std::move(config)), m_timer(this)
    {
        Bind(wxEVT_TIMER, [this](wxTimerEvent&) { refresh(); });
        refresh();
        m_timer.Start(1000);
    }
    ~NarratorEnvironment() override { m_timer.Stop(); }
private:
    static Narration::NotificationState query_notification_state()
    {
#ifdef _WIN32
        QUERY_USER_NOTIFICATION_STATE state{};
        if (FAILED(::SHQueryUserNotificationState(&state))) return Narration::NotificationState::Unknown;
        switch (state) {
        case QUNS_NOT_PRESENT:
        case QUNS_BUSY:                    return Narration::NotificationState::Busy;
        case QUNS_RUNNING_D3D_FULL_SCREEN: return Narration::NotificationState::FullScreen;
        case QUNS_PRESENTATION_MODE:       return Narration::NotificationState::Presentation;
        case QUNS_ACCEPTS_NOTIFICATIONS:
        case QUNS_APP:                     return Narration::NotificationState::AcceptsNotifications;
        case QUNS_QUIET_TIME:              return Narration::NotificationState::QuietTime;
        default:                           return Narration::NotificationState::Unknown;
        }
#else
        return Narration::NotificationState::Unknown;
#endif
    }

    void refresh()
    {
        bool screen_reader = false;
#ifdef _WIN32
        BOOL active = FALSE;
        if (::SystemParametersInfoW(SPI_GETSCREENREADER, 0, &active, 0)) screen_reader = active != FALSE;
#endif
        Narration::EnvironmentInput input;
        input.manual_quiet = m_quiet && m_quiet();
        input.screen_reader = screen_reader;
        if (m_config) {
            input.quiet_hours.enabled = Narration::parse_bool(m_config(Narration::kKeyQuietHours), false);
            if (const auto start = Narration::parse_clock(m_config(Narration::kKeyQuietStart))) input.quiet_hours.start_minute = *start;
            if (const auto end = Narration::parse_clock(m_config(Narration::kKeyQuietEnd))) input.quiet_hours.end_minute = *end;
            input.respect_focus_assist = Narration::parse_bool(m_config(Narration::kKeyRespectFocus), true);
        }
        const std::time_t now = std::time(nullptr);
        std::tm local{};
#ifdef _WIN32
        ::localtime_s(&local, &now);
#else
        ::localtime_r(&now, &local);
#endif
        input.minute_of_day = local.tm_hour * 60 + local.tm_min;
        input.notification = input.respect_focus_assist ? query_notification_state() : Narration::NotificationState::Unknown;

        const auto state = Narration::environment_state(input);
        TtsNarrator::set_quiet(state == Narration::EnvironmentState::Held, false);
        TtsNarrator::set_environment_state(state);
    }
    std::function<bool()> m_quiet;
    ConfigReader m_config;
    wxTimer m_timer;
};
}
