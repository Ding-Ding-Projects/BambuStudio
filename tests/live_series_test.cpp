// Standalone assertions for src/slic3r/GUI/LiveCharts/LiveSeries.cpp: readings
// the printer sent (refused when not finite or out of order), the shown
// window, gaps that stay gaps, the trend in words, one-minute candles, the
// feed state, the crosshair's nearest reading, the value range and the CSV,
// then the momentum arrow, new highs and their particles, the 12-hour clock
// and the saved chart options. Compiled and run by tests/live_series.test.mjs.
#include "src/slic3r/GUI/LiveCharts/LiveSeries.cpp"

#include <cmath>
#include <cstdio>
#include <limits>

using namespace Slic3r::GUI::LiveCharts;

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } \
    } while (0)

int main()
{
    const double t0 = 1762581600; // 2025-11-08T06:00:00Z
    Series       s(5);
    CHECK(s.add(t0, 20.0) && s.add(t0 + 1, 21.0));
    CHECK(!s.add(t0 + 1, 22.0));                                       // not later
    CHECK(!s.add(t0 + 2, std::numeric_limits<double>::quiet_NaN()));   // not finite
    for (int i = 2; i < 8; ++i) s.add(t0 + i, 20.0 + i);
    CHECK(s.readings().size() == 5 && s.readings().front().t == t0 + 3); // capacity keeps the newest
    CHECK(s.latest() && s.latest()->value == 27.0);
    CHECK(s.window(t0 + 7, 2).size() == 3);

    // A gap stays a gap.
    std::vector<Reading> r = {{t0, 1}, {t0 + 1, 2}, {t0 + 2, 3}, {t0 + 30, 4}, {t0 + 31, 5}};
    const auto parts = segments(r, 5.0);
    CHECK(parts.size() == 2 && parts[0].size() == 3 && parts[1].size() == 2);

    // Trend: holding near a target, rising, falling, flat.
    std::vector<Reading> heat;
    for (int i = 0; i <= 120; ++i) heat.push_back({t0 + i, 150.0 + i * 0.5});
    TrendInfo up = trend(heat, 220.0);
    CHECK(up.trend == Trend::Rising && std::fabs(up.delta - 30.0) < 1e-9 && up.over == 60.0);
    std::vector<Reading> hold = {{t0, 219.8}, {t0 + 30, 220.4}, {t0 + 42, 219.6}};
    CHECK(trend(hold, 220.0).trend == Trend::Holding);
    CHECK(trend(hold, 0.0).trend == Trend::Flat);
    std::vector<Reading> cool = {{t0, 60.0}, {t0 + 40, 55.0}};
    CHECK(trend(cool, 0.0).trend == Trend::Falling);
    CHECK(trend({}, 0.0).trend == Trend::Unknown);

    // Candles: first, highest, lowest, last per minute; empty minutes have none; the last is forming.
    std::vector<Reading> c = {{t0 + 5, 30}, {t0 + 20, 34}, {t0 + 50, 29}, {t0 + 59, 31}, {t0 + 185, 40}, {t0 + 190, 38}};
    const auto k = candles(c, t0, 60, t0 + 200);
    CHECK(k.size() == 2);
    CHECK(k[0].open == 30 && k[0].high == 34 && k[0].low == 29 && k[0].close == 31 && k[0].count == 4 && !k[0].forming);
    CHECK(k[1].start == t0 + 180 && k[1].forming && k[1].close == 38);

    // Feed state.
    CHECK(feed_state(false, Reading{t0, 1}, t0) == Feed::Offline);
    CHECK(feed_state(true, std::nullopt, t0) == Feed::Waiting);
    CHECK(feed_state(true, Reading{t0, 1}, t0 + 3) == Feed::Live);
    CHECK(feed_state(true, Reading{t0, 1}, t0 + 14) == Feed::Stalled);

    // Crosshair.
    CHECK(nearest(r, t0 + 1.4) == 1 && nearest(r, t0 + 1.6) == 2 && nearest(r, t0 - 9) == 0 && nearest(r, t0 + 99) == 4);
    CHECK(nearest({}, t0) == -1);

    // Range includes targets and pads a flat line.
    const Range flat = value_range({{{t0, 65.0}, {t0 + 1, 65.0}}}, {65.0});
    CHECK(flat.low < 65.0 && flat.high > 65.0);
    const Range with_target = value_range({{{t0, 150.0}}}, {220.0, 0.0});
    CHECK(with_target.high > 220.0 && with_target.low < 150.0);

    // CSV in time order, UTC.
    const std::string csv = to_csv({{"Nozzle", {{t0 + 1.25, 218.6}}}, {"Bed", {{t0, 65.0}}}});
    CHECK(csv == "time,series,value\n2025-11-08T06:00:00.000Z,Bed,65.0\n2025-11-08T06:00:01.250Z,Nozzle,218.6\n");

    // Bounded: a percentage stays inside 0..100 and the range keeps its width.
    const Range low_fan = bounded(Range{-0.24, 0.24}, 0, 100);
    CHECK(low_fan.low == 0 && std::fabs(low_fan.high - 0.48) < 1e-9);
    const Range high_fan = bounded(Range{98, 104}, 0, 100);
    CHECK(high_fan.high == 100 && high_fan.low == 94);
    const Range wide = bounded(Range{-20, 130}, 0, 100);
    CHECK(wide.low == 0 && wide.high == 100);
    CHECK(bounded(Range{10, 20}, 0, 100).low == 10 && bounded(Range{10, 20}, 0, 100).high == 20);

    // Momentum: the least-squares slope over the last ten seconds.
    std::vector<Reading> climb;
    for (int i = 0; i <= 30; ++i) climb.push_back({t0 + i, 100.0 + (i >= 20 ? (i - 20) * 0.2 : 0.0)});
    const MomentumInfo rising = momentum(climb);
    CHECK(rising.direction == Momentum::Up && std::fabs(rising.rate - 0.2) < 1e-9 && rising.span == 10);
    CHECK(momentum({{t0, 50}, {t0 + 5, 49}, {t0 + 10, 48}}).direction == Momentum::Down);
    CHECK(momentum({{t0, 65.0}, {t0 + 2, 65.1}, {t0 + 4, 64.9}, {t0 + 6, 65.0}}).direction == Momentum::Steady);
    CHECK(momentum({{t0, 1}, {t0 + 1, 9}}).direction == Momentum::Unknown);  // under two seconds
    CHECK(momentum({{t0, 1}}).direction == Momentum::Unknown);
    CHECK(momentum({{t0, 1}, {t0 + 30, 9}}).direction == Momentum::Unknown); // the older reading is outside the span

    // The arrow points up for a rise (screen y grows downwards), never steeper than 60 degrees.
    const double pi = 3.14159265358979323846;
    CHECK(arrow_angle(0, 2, 3) == 0);
    CHECK(std::fabs(arrow_angle(1, 10, 10) + pi / 4) < 1e-9);
    CHECK(std::fabs(arrow_angle(-1, 10, 10) - pi / 4) < 1e-9);
    CHECK(std::fabs(arrow_angle(-100, 1, 10) - 60 * pi / 180) < 1e-9);
    CHECK(arrow_angle(1, 0, 10) == 0);

    // New highs: not the first reading, not within the cooldown, not without the margin.
    HighWatch watch(0.5, 5);
    CHECK(!watch.see({t0, 20}));
    CHECK(watch.see({t0 + 1, 25}));
    CHECK(!watch.see({t0 + 2, 30}));   // a new high within the cooldown
    CHECK(watch.see({t0 + 7, 30.2}));  // above the last burst's level by the margin, after the cooldown
    CHECK(!watch.see({t0 + 20, 30.4})); // a new high, but not by the margin
    CHECK(!watch.see({t0 + 30, 29}));  // not a high
    CHECK(watch.high() && *watch.high() == 30.4);
    HighWatch primed;
    primed.prime({{t0, 40}, {t0 + 1, 50}, {t0 + 2, 45}});
    CHECK(!primed.see({t0 + 3, 50.3}));
    CHECK(primed.see({t0 + 4, 51}));
    primed.reset();
    CHECK(!primed.high());

    // Particles: the same seed gives the same burst; they fly up, fall back and go.
    Particles fx;
    fx.burst(100, 100, 24, 7, 1.0);
    CHECK(fx.live().size() == 24);
    bool upwards = true;
    for (const Particle &p : fx.live()) upwards = upwards && p.vy < 0 && p.x == 100 && p.y == 100 && p.life <= 1.1;
    CHECK(upwards);
    Particles twin;
    twin.burst(100, 100, 24, 7, 1.0);
    CHECK(twin.live().size() == 24 && twin.live()[5].vx == fx.live()[5].vx && twin.live()[5].life == fx.live()[5].life);
    const double vy_before = fx.live().front().vy;
    fx.step(0.1);
    CHECK(!fx.empty() && fx.live().front().y < 100 && fx.live().front().vy > vy_before);
    CHECK(Particles::fade(fx.live().front()) < 1.0 && Particles::fade(fx.live().front()) > 0.0);
    fx.step(2.0);
    CHECK(fx.empty());
    for (int i = 0; i < 10; ++i) fx.burst(0, 0, 24, std::uint32_t(i + 1));
    CHECK(fx.live().size() == kParticleLimit);

    // The clock: 12 for midnight and noon on a 12-hour clock, two-digit hours on a 24-hour clock.
    CHECK(clock_face(0, 5, 9, true, true).digits == "12:05:09" && !clock_face(0, 5, 9, true, true).pm);
    CHECK(clock_face(12, 0, 0, true, false).digits == "12:00" && clock_face(12, 0, 0, true, false).pm);
    CHECK(clock_face(13, 5, 0, true, false).digits == "1:05" && clock_face(13, 5, 0, true, false).pm);
    CHECK(clock_face(23, 59, 59, true, true).digits == "11:59:59");
    CHECK(clock_face(9, 5, 0, false, false).digits == "09:05" && !clock_face(9, 5, 0, false, false).twelve_hour);
    CHECK(clock_face(0, 43, 18, false, true).digits == "00:43:18");
    CHECK(twelve_hour_format("h:mm:ss tt") && twelve_hour_format("tt h:mm") && twelve_hour_format("hh:mm tt"));
    CHECK(!twelve_hour_format("HH:mm:ss") && !twelve_hour_format("HH 'h' mm") && !twelve_hour_format(""));

    // Saved options: the switches' defaults, and the clock follows Windows until chosen.
    const ChartOptions fresh = options_from("", "", "", "", true);
    CHECK(fresh.reference && fresh.momentum && !fresh.particles && fresh.twelve_hour && !fresh.clock_chosen);
    const ChartOptions saved = options_from("0", "0", "1", "24", true);
    CHECK(!saved.reference && !saved.momentum && saved.particles && !saved.twelve_hour && saved.clock_chosen);
    CHECK(options_from("1", "1", "0", "12", false).twelve_hour);
    CHECK(!options_from("", "", "yes", "", false).particles);

    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("live series: all checks passed\n");
    return 0;
}
