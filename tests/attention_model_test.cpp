// Standalone assertions for src/slic3r/GUI/AttentionModes/AttentionModel.cpp:
// every mode off by default and independent, saving and reading, the next
// action kept to one short line, the elapsed times, and Momentum speaking once
// per untouched stretch and never while "Not now" lasts. Compiled and run by
// tests/attention_model.test.mjs.
#include "src/slic3r/GUI/AttentionModes/AttentionModel.cpp"

#include <cstdio>
#include <map>

using namespace Slic3r::GUI::AttentionModes;

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } \
    } while (0)

int main()
{
    std::map<std::string, std::string> cfg;
    const Getter get = [&cfg](const std::string &k) { const auto it = cfg.find(k); return it == cfg.end() ? std::string() : it->second; };
    const Setter set = [&cfg](const std::string &k, const std::string &v) { cfg[k] = v; };

    // Off by default, whatever junk is stored.
    Settings s = read(get);
    CHECK(!s.focus && !s.low_stimulation && !s.time_awareness && !s.one_thing && !s.momentum);
    CHECK(s.momentum_after == 30 && s.not_now_for == 60 && s.next_action.empty() && s.snoozed_until == 0);
    cfg[kFocus] = "yes"; cfg[kMomentumAfter] = "45"; cfg[kNotNowFor] = "-1"; cfg[kSnoozedUntil] = "abc";
    s = read(get);
    CHECK(!s.focus && s.momentum_after == 30 && s.not_now_for == 60 && s.snoozed_until == 0);

    // Independent: one on leaves the others off; saved and read back exactly.
    Settings t;
    t.time_awareness = true;
    t.momentum       = true;
    t.momentum_after = 15;
    t.not_now_for    = 120;
    t.next_action    = "  Check the brim\non plate 2  ";
    write(t, set);
    s = read(get);
    CHECK(s.time_awareness && s.momentum && !s.focus && !s.low_stimulation && !s.one_thing);
    CHECK(s.momentum_after == 15 && s.not_now_for == 120 && s.next_action == "Check the brim on plate 2");

    // One line of at most 120 characters, never splitting a character.
    std::string long_text;
    for (int i = 0; i < 130; ++i) long_text += "\xE5\xA2\xA8"; // 墨
    CHECK(clean_next_action(long_text).size() == 360);
    CHECK(clean_next_action(" \t \n").empty());

    // Elapsed times.
    const std::int64_t t0 = 1762581600;
    Clock              c{t0, 0, 0};
    Elapsed            e = elapsed(c, t0 + 4320);
    CHECK(e.open == 4320 && !e.changed && e.since_change == 4320);
    CHECK(split(4320).hours == 1 && split(4320).minutes == 12 && split(59).minutes == 0);
    c.last_change = t0 + 4000;
    e             = elapsed(c, t0 + 4320);
    CHECK(e.changed && e.since_change == 320);

    // Momentum: off says nothing; on speaks once per untouched stretch.
    Settings m;
    CHECK(!momentum_due(m, c, t0 + 99999));
    m.momentum       = true;
    m.momentum_after = 30;
    CHECK(!momentum_due(m, c, t0 + 4000 + 29 * 60));
    CHECK(momentum_due(m, c, t0 + 4000 + 30 * 60));
    prompted(c, t0 + 4000 + 30 * 60);
    CHECK(!momentum_due(m, c, t0 + 4000 + 90 * 60)); // once for this stretch
    c.last_change = t0 + 9000;                        // a change starts a new stretch
    CHECK(momentum_due(m, c, t0 + 9000 + 30 * 60));
    // Not now lasts the stated period.
    m.not_now_for = 60;
    const std::int64_t until = snooze(m, t0 + 9000);
    CHECK(until == t0 + 9000 + 3600 && !momentum_due(m, c, t0 + 9000 + 59 * 60));
    CHECK(momentum_due(m, c, t0 + 9000 + 61 * 60));

    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("attention model: all checks passed\n");
    return 0;
}
