#pragma once

#include <chrono>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Slic3r::GUI::PersonalModes {
enum class SpeechLanguage { English, Cantonese };
struct SpeechTrack { std::wstring text; SpeechLanguage language = SpeechLanguage::English; };
struct SpeechEvent { std::vector<SpeechTrack> tracks; std::string category; bool explicit_consent = false; };

// Completion-driven state machine. An event's tracks are never interleaved.
class SpeechQueue {
public:
    using Clock = std::chrono::steady_clock;
    bool enqueue(SpeechEvent event, Clock::time_point now = Clock::now()) {
        if (event.tracks.empty() || event.tracks.size() > 2) return false;
        for (const auto& track : event.tracks) if (track.text.empty() || track.text.size() > 32768) return false;
        if (event.category.size() > 64) return false;
        if (event.category != "error" && !event.explicit_consent) {
            auto it = m_last.find(event.category);
            if (it != m_last.end() && now - it->second < std::chrono::seconds(20)) return false;
        }
        // Errors are distinct factual events, never silently replaced.
        if (event.category != "error") for (auto& queued : m_pending)
            if (queued.category == event.category && queued.explicit_consent == event.explicit_consent) {
                queued = std::move(event); return true;
            }
        // Bound ordinary pending work; errors receive priority in the available queue.
        if (m_pending.size() >= 64) {
            if (event.category != "error") return false;
            auto ordinary = m_pending.begin();
            while (ordinary != m_pending.end() && ordinary->category == "error") ++ordinary;
            if (ordinary == m_pending.end()) return false;
            m_pending.erase(ordinary);
        }
        if (event.category == "error") {
            auto position = m_pending.begin();
            while (position != m_pending.end() && position->category == "error") ++position;
            m_pending.insert(position, std::move(event));
        } else m_pending.push_back(std::move(event));
        return true;
    }
    std::optional<SpeechTrack> next(bool backend_complete, bool enabled, bool quiet,
                                    Clock::time_point now = Clock::now()) {
        if (m_in_flight && !backend_complete) return {};
        m_in_flight = false;
        if (quiet) return {};
        if (!m_current) {
            while (!m_pending.empty() && !enabled && !m_pending.front().explicit_consent) m_pending.pop_front();
            if (m_pending.empty()) return {};
            m_current = std::move(m_pending.front()); m_pending.pop_front(); m_track = 0;
            m_last[m_current->category] = now;
            if (m_last.size() > 128) m_last.erase(m_last.begin());
        }
        if (!enabled && !m_current->explicit_consent) { m_current.reset(); return {}; }
        auto result = m_current->tracks[m_track++];
        if (m_track == m_current->tracks.size()) m_current.reset();
        m_in_flight = true;
        return result;
    }
    void cancel() { m_pending.clear(); m_current.reset(); m_in_flight = false; }
    bool busy() const { return m_in_flight; }
private:
    std::deque<SpeechEvent> m_pending;
    std::optional<SpeechEvent> m_current;
    std::map<std::string, Clock::time_point> m_last;
    std::size_t m_track = 0;
    bool m_in_flight = false;
};
}
