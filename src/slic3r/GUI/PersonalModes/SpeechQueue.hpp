#pragma once

#include <chrono>
#include <deque>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Slic3r::GUI::PersonalModes {
enum class SpeechLanguage { English, Cantonese };
struct SpeechTrack { std::wstring text; SpeechLanguage language = SpeechLanguage::English; };
// `track` names the independent narration track (printer, app, accessibility).
// Replacement and cooldown bookkeeping are keyed by track and category, so an
// event on one track never replaces or throttles an event on another.
// `cooldown_exempt` is for events a transition detector has already de-duplicated.
struct SpeechEvent { std::vector<SpeechTrack> tracks; std::string category; bool explicit_consent = false;
                     std::string track; bool cooldown_exempt = false; };
inline std::string speech_key(const SpeechEvent& e) { return e.track + "/" + e.category; }

// Completion-driven state machine. An event's tracks are never interleaved.
class SpeechQueue {
public:
    using Clock = std::chrono::steady_clock;
    bool enqueue(SpeechEvent event, Clock::time_point now = Clock::now()) {
        if (event.tracks.empty() || event.tracks.size() > 2) return false;
        for (const auto& track : event.tracks) if (track.text.empty() || track.text.size() > 32768) return false;
        if (event.category.size() > 64) return false;
        if (event.track.size() > 32) return false;
        if (event.category != "error" && !event.explicit_consent && !event.cooldown_exempt) {
            auto it = m_last.find(speech_key(event));
            if (it != m_last.end() && now - it->second < std::chrono::seconds(20)) return false;
        }
        // Errors are distinct factual events, never silently replaced.
        if (event.category != "error") for (auto& queued : m_pending)
            if (speech_key(queued) == speech_key(event) && queued.explicit_consent == event.explicit_consent) {
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
            m_last[speech_key(*m_current)] = now;
            if (m_last.size() > 128) m_last.erase(m_last.begin());
        }
        if (!enabled && !m_current->explicit_consent) { m_current.reset(); return {}; }
        auto result = m_current->tracks[m_track++];
        if (m_track == m_current->tracks.size()) m_current.reset();
        m_in_flight = true;
        return result;
    }
    void cancel() { m_pending.clear(); m_current.reset(); m_in_flight = false; }
    // Drops pending ordinary events but keeps queued errors (Focus Assist, full screen).
    void drop_non_errors() {
        for (auto it = m_pending.begin(); it != m_pending.end();) it = it->category == "error" ? std::next(it) : m_pending.erase(it);
        if (m_current && m_current->category != "error") m_current.reset();
    }
    std::size_t pending() const { return m_pending.size(); }
    bool busy() const { return m_in_flight; }
private:
    std::deque<SpeechEvent> m_pending;
    std::optional<SpeechEvent> m_current;
    std::map<std::string, Clock::time_point> m_last;
    std::size_t m_track = 0;
    bool m_in_flight = false;
};
}
