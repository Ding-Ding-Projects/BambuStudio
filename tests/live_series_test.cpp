// Standalone assertions for src/slic3r/GUI/LiveCharts/LiveSeries.cpp: readings
// the printer sent (refused when not finite or out of order), the shown
// window, gaps that stay gaps, the trend in words, one-minute candles, the
// feed state, the crosshair's nearest reading, the value range and the CSV.
// Compiled and run by tests/live_series.test.mjs.
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

    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("live series: all checks passed\n");
    return 0;
}
