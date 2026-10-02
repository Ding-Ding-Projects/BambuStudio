#ifndef slic3r_GUI_CanvasMenuSearchModel_hpp_
#define slic3r_GUI_CanvasMenuSearchModel_hpp_

#include "Widgets/RegexBuilderBridgeState.hpp"
#include "Widgets/BoundedRegex.hpp"
#include <chrono>
#include <algorithm>
#include <string>
#include <vector>

namespace Slic3r::GUI {

inline std::string canvas_menu_search_key(const RegexBuilderValues& values,
                                          const std::vector<std::string>& items)
{
    std::string key = std::to_string(values.pattern.size()) + ":" + values.pattern;
    key += values.regex_enabled ? "R" : "L";
    key += values.case_sensitive ? "C" : "c";
    key += values.whole_word ? "W" : "w";
    key += values.multiline ? "M" : "m";
    for (const auto& item : items) key += std::to_string(item.size()) + ":" + item;
    return key;
}

// UI-independent state shared by the canvas renderer and hosted regression tests.
// A result is usable only for the exact query, flags and ordered candidate list.
struct CanvasMenuSearchModel
{
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    static constexpr unsigned max_attempts = 3;
    std::vector<bool> visible;
    std::string signature;
    std::string requested_signature;
    std::string evaluated_signature;
    bool unavailable = false;
    unsigned attempts = 0;
    bool retry_waiting = false;
    TimePoint retry_at{};

    bool change_input(const std::string& key, size_t count)
    {
        if (key == signature) return false;
        signature = key;
        evaluated_signature.clear();
        visible.assign(count, true);
        unavailable = false;
        attempts = 0;
        retry_waiting = false;
        retry_at = {};
        return true;
    }

    bool accept_result(const std::string& key, std::vector<bool> mask,
                       BoundedRegex::Status status, TimePoint now = Clock::now())
    {
        if (key != signature) return false;
        const bool complete = status == BoundedRegex::Status::Valid ||
            status == BoundedRegex::Status::Match || status == BoundedRegex::Status::NoMatch;
        const bool wrong_size = mask.size() != visible.size();
        unavailable = !complete || wrong_size;
        if (unavailable) std::fill(visible.begin(), visible.end(), true);
        else visible = std::move(mask);
        evaluated_signature = signature;
        // Readiness/contention is transient. Invalid patterns, complexity,
        // timeout budgets, protocol errors and malformed masks are not retried.
        retry_waiting = !wrong_size && status == BoundedRegex::Status::WorkerUnavailable &&
                        attempts < max_attempts;
        if (retry_waiting)
            retry_at = now + std::chrono::milliseconds(attempts <= 1 ? 250 : 1000);
        return true;
    }

    bool needs_result(TimePoint now = Clock::now()) const
    {
        return attempts < max_attempts &&
            (evaluated_signature != signature || (retry_waiting && now >= retry_at));
    }

    bool begin_request(TimePoint now = Clock::now())
    {
        if (!needs_result(now)) return false;
        requested_signature = signature;
        retry_waiting = false;
        ++attempts;
        return true;
    }

    int retry_delay_ms(TimePoint now = Clock::now()) const
    {
        if (!retry_waiting) return 0;
        // Round up to avoid repeated zero-delay frames just before the deadline.
        const auto us = std::chrono::duration_cast<std::chrono::microseconds>(retry_at - now).count();
        return static_cast<int>(std::max<std::int64_t>(1, (us + 999) / 1000));
    }

};

} // namespace Slic3r::GUI
#endif
