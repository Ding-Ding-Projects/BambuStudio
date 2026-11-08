#ifndef slic3r_GUI_AttentionModes_AttentionModel_hpp_
#define slic3r_GUI_AttentionModes_AttentionModel_hpp_

// Attention modes, without wxWidgets: five interface accommodations, each off
// by default and independent of the others (Focus, Low stimulation, Time
// awareness, One thing at a time, Momentum), how they are saved, the elapsed
// times the strip states, and when Momentum may say that nothing has changed:
// once per untouched stretch, never while "Not now" lasts. Tested on its own
// by tests/attention_model_test.cpp.

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Slic3r { namespace GUI { namespace AttentionModes {

inline constexpr const char *kFocus          = "attention_focus";
inline constexpr const char *kLowStimulation = "attention_low_stimulation";
inline constexpr const char *kTimeAwareness  = "attention_time_awareness";
inline constexpr const char *kOneThing       = "attention_one_thing";
inline constexpr const char *kMomentum       = "attention_momentum";
inline constexpr const char *kMomentumAfter  = "attention_momentum_minutes";
inline constexpr const char *kNotNowFor      = "attention_not_now_minutes";
inline constexpr const char *kNextAction     = "attention_next_action";
inline constexpr const char *kSnoozedUntil   = "attention_snoozed_until";

const std::vector<int> &momentum_choices(); // minutes: 15, 30, 60, 90
const std::vector<int> &not_now_choices();  // minutes: 30, 60, 120, 240

struct Settings
{
    bool         focus           = false;
    bool         low_stimulation = false;
    bool         time_awareness  = false;
    bool         one_thing       = false;
    bool         momentum        = false;
    int          momentum_after  = 30; // minutes untouched before Momentum speaks
    int          not_now_for     = 60; // minutes "Not now" lasts
    std::string  next_action;
    std::int64_t snoozed_until = 0;
};

using Getter = std::function<std::string(const std::string &key)>;
using Setter = std::function<void(const std::string &key, const std::string &value)>;
// Anything unknown or out of range reads as the default: every mode off.
Settings read(const Getter &get);
void     write(const Settings &settings, const Setter &set);

// One line, trimmed, at most 120 characters.
std::string clean_next_action(std::string_view text);

struct Clock
{
    std::int64_t session_start = 0;
    std::int64_t last_change   = 0; // 0: nothing changed yet this session
    std::int64_t last_prompt   = 0;
};

struct Elapsed
{
    std::int64_t open         = 0; // seconds the session has been open
    std::int64_t since_change = 0; // seconds since the last change, or since opening
    bool         changed      = false;
};
Elapsed elapsed(const Clock &clock, std::int64_t now);

struct Parts
{
    int hours = 0, minutes = 0;
};
Parts split(std::int64_t seconds); // whole minutes, rounded down

// Momentum speaks once per untouched stretch of momentum_after minutes, never while snoozed.
bool momentum_due(const Settings &settings, const Clock &clock, std::int64_t now);
void prompted(Clock &clock, std::int64_t now);
// "Not now": quiet for not_now_for minutes from now.
std::int64_t snooze(Settings &settings, std::int64_t now);

}}} // namespace Slic3r::GUI::AttentionModes

#endif // slic3r_GUI_AttentionModes_AttentionModel_hpp_
