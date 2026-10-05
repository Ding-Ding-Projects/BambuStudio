#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <string>
#include <utility>

namespace Slic3r::GUI::PersonalModes {

// The owner persists Settings and connects these callbacks to the active workspace.
// No callback is an authorization to hide, close, or mutate the user's work.
struct AttentionSettings {
    bool focus = false;
    bool low_stimulation = false;
    bool time_awareness = false;
    bool one_thing = false;
    bool momentum = false;
    std::string next_action;
    int idle_minutes = 20;
    int snooze_minutes = 30;
};

struct AttentionCallbacks {
    std::function<void(bool)> focus;
    std::function<void(bool)> low_stimulation;
    std::function<void(bool, std::chrono::seconds, std::chrono::seconds)> elapsed;
    std::function<void(bool, const std::string&)> next_action;
    std::function<void(bool, std::chrono::seconds)> momentum;
    std::function<bool(const AttentionSettings&)> persist;
};

class AttentionModes {
public:
    using Clock = std::chrono::steady_clock;
    using Time = Clock::time_point;
    explicit AttentionModes(AttentionCallbacks callbacks, AttentionSettings settings = {}, Time now = Clock::now())
        : m_callbacks(std::move(callbacks)), m_settings(normalize(std::move(settings))),
          m_started(now), m_last_activity(now), m_snoozed_until(now) { publish(now); }

    const AttentionSettings& settings() const { return m_settings; }
    bool configure(AttentionSettings settings, Time now = Clock::now()) {
        settings = normalize(std::move(settings));
        if (m_callbacks.persist && !m_callbacks.persist(settings)) return false;
        m_settings = std::move(settings);
        if (!m_settings.momentum) m_prompted = false;
        publish(now);
        return true;
    }
    void platform_reduced_motion(bool active, Time now = Clock::now()) {
        m_platform_reduced_motion = active;
        publish(now);
    }
    bool suppress_nonessential_notifications() const { return m_settings.low_stimulation; }
    bool reduce_motion() const { return m_settings.low_stimulation || m_platform_reduced_motion; }
    void activity(Time now = Clock::now()) {
        m_last_activity = now;
        m_prompted = false;
        if (m_callbacks.momentum) m_callbacks.momentum(false, std::chrono::seconds(0));
    }
    void dismiss_momentum(Time now = Clock::now()) {
        m_snoozed_until = now + std::chrono::minutes(m_settings.snooze_minutes);
        m_prompted = false;
        if (m_callbacks.momentum) m_callbacks.momentum(false, idle(now));
    }
    void tick(Time now = Clock::now()) {
        if (m_callbacks.elapsed)
            m_callbacks.elapsed(m_settings.time_awareness, duration(now, m_started), idle(now));
        if (m_settings.momentum && !m_prompted && now >= m_snoozed_until &&
            idle(now) >= std::chrono::minutes(m_settings.idle_minutes)) {
            m_prompted = true;
            if (m_callbacks.momentum) m_callbacks.momentum(true, idle(now));
        }
    }
private:
    static AttentionSettings normalize(AttentionSettings value) {
        value.idle_minutes = std::clamp(value.idle_minutes, 1, 240);
        value.snooze_minutes = std::clamp(value.snooze_minutes, 1, 240);
        // Reject oversized actions at the UI boundary; keep this service bounded too.
        if (value.next_action.size() > 4096) value.next_action.clear();
        return value;
    }
    static std::chrono::seconds duration(Time now, Time then) {
        return now < then ? std::chrono::seconds(0) : std::chrono::duration_cast<std::chrono::seconds>(now - then);
    }
    std::chrono::seconds idle(Time now) const { return duration(now, m_last_activity); }
    void publish(Time now) {
        if (m_callbacks.focus) m_callbacks.focus(m_settings.focus);
        if (m_callbacks.low_stimulation) m_callbacks.low_stimulation(reduce_motion());
        if (m_callbacks.next_action) m_callbacks.next_action(m_settings.one_thing, m_settings.next_action);
        if (!m_settings.momentum && m_callbacks.momentum) m_callbacks.momentum(false, idle(now));
        tick(now);
    }
    AttentionCallbacks m_callbacks;
    AttentionSettings m_settings;
    Time m_started, m_last_activity, m_snoozed_until;
    bool m_platform_reduced_motion = false;
    bool m_prompted = false;
};
}
