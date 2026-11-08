#include "AttentionModel.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace Slic3r { namespace GUI { namespace AttentionModes {

namespace {

bool flag(const Getter &get, const char *key) { return get(key) == "true"; }

int choice(const Getter &get, const char *key, const std::vector<int> &choices, int fallback)
{
    const std::string text = get(key);
    if (text.empty() || text.size() > 6 || !std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c); }))
        return fallback;
    const int value = std::atoi(text.c_str());
    return std::find(choices.begin(), choices.end(), value) != choices.end() ? value : fallback;
}

} // namespace

const std::vector<int> &momentum_choices()
{
    static const std::vector<int> c = {15, 30, 60, 90};
    return c;
}

const std::vector<int> &not_now_choices()
{
    static const std::vector<int> c = {30, 60, 120, 240};
    return c;
}

Settings read(const Getter &get)
{
    Settings s;
    s.focus           = flag(get, kFocus);
    s.low_stimulation = flag(get, kLowStimulation);
    s.time_awareness  = flag(get, kTimeAwareness);
    s.one_thing       = flag(get, kOneThing);
    s.momentum        = flag(get, kMomentum);
    s.momentum_after  = choice(get, kMomentumAfter, momentum_choices(), 30);
    s.not_now_for     = choice(get, kNotNowFor, not_now_choices(), 60);
    s.next_action     = clean_next_action(get(kNextAction));
    const std::string until = get(kSnoozedUntil);
    if (!until.empty() && until.size() < 16 && std::all_of(until.begin(), until.end(), [](unsigned char c) { return std::isdigit(c); }))
        s.snoozed_until = std::strtoll(until.c_str(), nullptr, 10);
    return s;
}

void write(const Settings &s, const Setter &set)
{
    const auto b = [](bool v) { return std::string(v ? "true" : "false"); };
    set(kFocus, b(s.focus));
    set(kLowStimulation, b(s.low_stimulation));
    set(kTimeAwareness, b(s.time_awareness));
    set(kOneThing, b(s.one_thing));
    set(kMomentum, b(s.momentum));
    set(kMomentumAfter, std::to_string(s.momentum_after));
    set(kNotNowFor, std::to_string(s.not_now_for));
    set(kNextAction, clean_next_action(s.next_action));
    set(kSnoozedUntil, std::to_string(s.snoozed_until));
}

std::string clean_next_action(std::string_view text)
{
    std::string out;
    for (char c : text) out.push_back(c == '\r' || c == '\n' || c == '\t' ? ' ' : c);
    const auto first = out.find_first_not_of(' ');
    if (first == std::string::npos) return {};
    out = out.substr(first, out.find_last_not_of(' ') - first + 1);
    // At most 120 characters, never cutting a UTF-8 sequence.
    std::size_t chars = 0, i = 0;
    for (; i < out.size(); ++i) {
        if ((static_cast<unsigned char>(out[i]) & 0xC0) != 0x80) {
            if (chars == 120) break;
            ++chars;
        }
    }
    return out.substr(0, i);
}

Elapsed elapsed(const Clock &clock, std::int64_t now)
{
    Elapsed e;
    e.open         = std::max<std::int64_t>(0, now - clock.session_start);
    e.changed      = clock.last_change > 0;
    e.since_change = std::max<std::int64_t>(0, now - (e.changed ? clock.last_change : clock.session_start));
    return e;
}

Parts split(std::int64_t seconds)
{
    const std::int64_t minutes = std::max<std::int64_t>(0, seconds) / 60;
    return Parts{int(minutes / 60), int(minutes % 60)};
}

bool momentum_due(const Settings &settings, const Clock &clock, std::int64_t now)
{
    if (!settings.momentum || now < settings.snoozed_until) return false;
    const std::int64_t since = clock.last_change > 0 ? clock.last_change : clock.session_start;
#ifndef SLIC3R_ATTENTION_MODEL_NEGATIVE_FIXTURE
    if (clock.last_prompt >= since) return false; // already said once for this stretch
#endif
    return now - since >= std::int64_t(settings.momentum_after) * 60;
}

void prompted(Clock &clock, std::int64_t now) { clock.last_prompt = now; }

std::int64_t snooze(Settings &settings, std::int64_t now)
{
    settings.snoozed_until = now + std::int64_t(settings.not_now_for) * 60;
    return settings.snoozed_until;
}

}}} // namespace Slic3r::GUI::AttentionModes
