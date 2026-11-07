#pragma once

#include "Narration/NarrationPolicy.hpp"
#include <deque>
#include <optional>

namespace Slic3r::GUI::ActionSounds {
inline constexpr const char* enabled_key = "action_sounds_enabled";
enum class Cue { VocabularyApplied, Rejected, WordingCleared };

struct Preferences {
    bool enabled = true;
    bool quiet = false;
    Narration::QuietHours hours;
    bool respect_focus = true;
};
inline bool admitted(const Preferences& prefs, int minute, bool os_quiet)
{
    return prefs.enabled && !prefs.quiet && !prefs.hours.contains(minute) &&
           !(prefs.respect_focus && os_quiet);
}

// Bounded, ordered requests. A busy audio backend never changes an action result.
// The owner serializes access and cancels queued requests when settings change.
class CueQueue {
public:
    static constexpr std::size_t capacity = 32;
    bool append(Cue cue) {
        if (m_pending.size() == capacity) return false;
        m_pending.push_back(cue);
        return true;
    }
    std::optional<Cue> take() {
        if (m_pending.empty()) return std::nullopt;
        const Cue cue = m_pending.front();
        m_pending.pop_front();
        return cue;
    }
    void cancel() { m_pending.clear(); }
private:
    std::deque<Cue> m_pending;
};

// GUI-thread entrypoints. No filenames, imported text or private payloads cross
// into the playback worker. Playback errors never escape into an action handler.
void play(Cue cue) noexcept;
void settings_changed() noexcept;
void cancel() noexcept;
} // namespace Slic3r::GUI::ActionSounds
