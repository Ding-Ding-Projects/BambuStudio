#pragma once
// Pure printer state-transition detection and event de-duplication.
//
// The narrator polls the selected printer on a timer. Narration must fire on a
// TRANSITION, never on every status tick, and a repeated identical event inside
// a short window must be dropped.
#include "NarrationEvents.hpp"

#include <chrono>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace Slic3r::GUI::Narration {

struct PrinterSnapshot {
    std::string machine;      // opaque identity used only to notice a switch of printer; never spoken
    std::string status;       // RUNNING, PAUSE, FINISH, FAILED, IDLE, ...
    int stage = -1;           // mc_print_stage; -1 when unknown
    int error = 0;            // print_error; 0 when none
    // Live door sensor: 1 open, 0 closed, -1 when the printer has no door sensor or
    // has not reported the bit (see DeviceLive::door_open_reading). The reading comes
    // from a community-documented status bit, so the feature is labelled beta.
    int door_open = -1;
};

// Community-documented values, not yet confirmed on hardware by this project:
// stage 6 is "paused due to filament runout"; error 0x0300400C is "task cancelled";
// error module byte 0x07 is the AMS.
inline constexpr int kStageFilamentRunout = 6;
inline constexpr int kErrorTaskCancelled  = 0x0300400C;
inline constexpr int kAmsErrorModule      = 0x07;

class PrinterTransitionDetector {
public:
    void reset() { m_seeded = false; }

    std::vector<Event> observe(const PrinterSnapshot& now)
    {
        std::vector<Event> out;
        if (!m_seeded || now.machine != m_last.machine) {
            // First sight of a printer only records its state, so opening the
            // app next to an old finished job or a standing error stays silent.
            m_last = now; m_seeded = true;
            m_runout_reported = now.stage == kStageFilamentRunout;
            return out;
        }
        const std::string& was = m_last.status;
        const std::string& is  = now.status;

        const bool new_error = now.error != 0 && now.error != m_last.error;
        const bool cancelled = new_error && now.error == kErrorTaskCancelled;

        if (now.stage == kStageFilamentRunout && m_last.stage != kStageFilamentRunout && !m_runout_reported) {
            out.push_back({EventKind::FilamentRunout, 0});
            m_runout_reported = true;
        }
        if (now.stage != kStageFilamentRunout && is != "PAUSE") m_runout_reported = false;

        if (is != was) {
            const bool was_active = was == "RUNNING" || was == "PAUSE";
            if (is == "RUNNING") {
                if (was == "PAUSE") out.push_back({EventKind::PrintResumed, 0});
                else if (!was.empty()) out.push_back({EventKind::PrintStarted, 0});
            } else if (is == "PAUSE" && was == "RUNNING") {
                if (!m_runout_reported) out.push_back({EventKind::PrintPaused, 0});
            } else if (is == "FINISH" && was_active) {
                out.push_back({EventKind::PrintFinished, 0});
            } else if (is == "FAILED" && was_active) {
                if (cancelled) out.push_back({EventKind::PrintCancelled, 0});
                else if (!new_error) out.push_back({EventKind::PrintFailed, 0});
                // a failure that arrives with a fresh error code is spoken once, as that error
            }
        }
        if (new_error && !cancelled) {
            if (((now.error >> 24) & 0xff) == kAmsErrorModule) out.push_back({EventKind::AmsError, 0});
            else out.push_back({EventKind::PrinterError, now.error});
        }
        const bool door_known = now.door_open == 0 || now.door_open == 1;
        const bool door_was_known = m_last.door_open == 0 || m_last.door_open == 1;
        if (door_known && door_was_known && now.door_open != m_last.door_open)
            out.push_back({now.door_open == 1 ? EventKind::DoorOpened : EventKind::DoorClosed, 0});
        m_last = now;
        return out;
    }

private:
    PrinterSnapshot m_last;
    bool m_seeded = false;
    bool m_runout_reported = false;
};

// Drops an identical event repeated within `window` (default 30 s). Time is
// injected so the behaviour is testable; the map is bounded.
class EventDeduper {
public:
    using Clock = std::chrono::steady_clock;
    explicit EventDeduper(std::chrono::seconds window = std::chrono::seconds(30)) : m_window(window) {}

    bool admit(const Event& event, Clock::time_point now = Clock::now())
    {
        const auto key = std::make_pair(static_cast<int>(event.kind), event.code);
        auto it = m_seen.find(key);
        if (it != m_seen.end() && now - it->second < m_window) return false;
        m_seen[key] = now;
        if (m_seen.size() > 64) m_seen.erase(m_seen.begin());
        return true;
    }

private:
    std::chrono::seconds m_window;
    std::map<std::pair<int, int>, Clock::time_point> m_seen;
};

} // namespace Slic3r::GUI::Narration
