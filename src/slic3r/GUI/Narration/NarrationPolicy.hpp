#pragma once
// Pure narration policy: master and per-track switches, the app's quiet hours,
// the Windows notification state (Focus Assist, full screen, presentation),
// and screen-reader coexistence.
#include "NarrationEvents.hpp"

#include <optional>
#include <string>

namespace Slic3r::GUI::Narration {

// AppConfig keys. The master key already exists (the narrator is off by default).
inline constexpr const char* kKeyMaster          = "narrator_enabled";
inline constexpr const char* kKeyQuietToggle     = "narrator_quiet";
inline constexpr const char* kKeyQuietHours      = "narrator_quiet_hours_enabled";
inline constexpr const char* kKeyQuietStart      = "narrator_quiet_hours_start";
inline constexpr const char* kKeyQuietEnd        = "narrator_quiet_hours_end";
inline constexpr const char* kKeyRespectFocus    = "narrator_respect_focus_assist";

inline const char* track_key(Track track)
{
    return track == Track::Printer ? "narrator_track_printer" : track == Track::App ? "narrator_track_app" : "narrator_track_accessibility";
}

// Tracks default to on once the master switch is on, so enabling the narrator
// that already exists keeps working without new choices.
inline bool parse_bool(const std::string& stored, bool fallback)
{
    if (stored == "true" || stored == "1") return true;
    if (stored == "false" || stored == "0") return false;
    return fallback;
}

// "HH:MM" -> minute of day. Anything else is rejected, never guessed.
inline std::optional<int> parse_clock(const std::string& text)
{
    if (text.size() != 5 || text[2] != ':') return std::nullopt;
    for (int i : {0, 1, 3, 4}) if (text[i] < '0' || text[i] > '9') return std::nullopt;
    const int h = (text[0] - '0') * 10 + (text[1] - '0');
    const int m = (text[3] - '0') * 10 + (text[4] - '0');
    if (h > 23 || m > 59) return std::nullopt;
    return h * 60 + m;
}

struct QuietHours {
    bool enabled = false;
    int start_minute = 22 * 60;
    int end_minute = 7 * 60;
    // The window may wrap midnight (22:00-07:00). Equal bounds mean "never".
    bool contains(int minute_of_day) const
    {
        if (!enabled || start_minute == end_minute) return false;
        if (start_minute < end_minute) return minute_of_day >= start_minute && minute_of_day < end_minute;
        return minute_of_day >= start_minute || minute_of_day < end_minute;
    }
};

// Mirrors SHQueryUserNotificationState. QuietTime is Focus Assist / quiet hours.
enum class NotificationState { Unknown, AcceptsNotifications, Busy, FullScreen, Presentation, QuietTime };

struct EnvironmentInput {
    bool manual_quiet = false;           // the user's own "Quiet narration" switch
    bool screen_reader = false;
    QuietHours quiet_hours;
    int minute_of_day = 0;
    bool respect_focus_assist = true;
    NotificationState notification = NotificationState::Unknown;
};

// Held: nothing speaks and queued speech is cancelled. ErrorsOnly: errors still
// name the failure, everything else is dropped. Open: normal.
enum class EnvironmentState { Open, ErrorsOnly, Held };

inline EnvironmentState environment_state(const EnvironmentInput& in)
{
    if (in.manual_quiet || in.screen_reader) return EnvironmentState::Held;
    if (in.quiet_hours.contains(in.minute_of_day)) return EnvironmentState::Held;
    if (in.respect_focus_assist && (in.notification == NotificationState::QuietTime || in.notification == NotificationState::Busy ||
                                    in.notification == NotificationState::Presentation || in.notification == NotificationState::FullScreen))
        return EnvironmentState::ErrorsOnly;
    return EnvironmentState::Open;
}

struct AdmitInput {
    bool master = false;
    bool track_enabled = true;
    EventKind kind = EventKind::AppError;
    bool explicit_consent = false;       // a flow with its own consent control (preview, scanner)
    EnvironmentState environment = EnvironmentState::Open;
};

// Explicit-consent flows bypass the master switch, the track switch and the
// ErrorsOnly state, but never a Held environment (screen reader, manual quiet, quiet hours).
inline bool admit(const AdmitInput& in)
{
    if (in.environment == EnvironmentState::Held) return false;
    if (in.explicit_consent) return true;
    if (!in.master || !in.track_enabled) return false;
    if (in.environment == EnvironmentState::ErrorsOnly && !is_error(in.kind)) return false;
    return true;
}

} // namespace Slic3r::GUI::Narration
