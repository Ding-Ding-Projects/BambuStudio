#ifndef slic3r_GUI_LiveCharts_LiveSeries_hpp_
#define slic3r_GUI_LiveCharts_LiveSeries_hpp_

// Live charts on the Device page, without wxWidgets: the readings a printer
// sent this session (never a point it did not send), the time window shown,
// gaps that stay gaps, the trend in words, one-minute candles, the state of
// the feed (waiting, live, stalled, offline), the crosshair's nearest reading
// and the CSV of what is shown. Tested on its own by tests/live_series_test.cpp.

#include <cstddef>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace LiveCharts {

struct Reading
{
    double t     = 0; // seconds since 1970-01-01 UTC, as the reading arrived
    double value = 0;
};

class Series
{
public:
    explicit Series(std::size_t capacity = 14400) : m_capacity(capacity) {}
    // Ignores a value that is not finite and a reading that is not later than the last one.
    bool                        add(double t, double value);
    const std::deque<Reading>  &readings() const { return m_readings; }
    std::optional<Reading>      latest() const;
    // The readings with t in [now - seconds, now].
    std::vector<Reading>        window(double now, double seconds) const;
    void                        clear() { m_readings.clear(); }

private:
    std::size_t         m_capacity;
    std::deque<Reading> m_readings;
};

// Consecutive readings further apart than gap_seconds belong to different segments:
// a line is never drawn across a gap.
std::vector<std::vector<Reading>> segments(const std::vector<Reading> &readings, double gap_seconds);

enum class Trend { Unknown, Holding, Rising, Falling, Flat };
struct TrendInfo
{
    Trend  trend        = Trend::Unknown;
    double delta        = 0; // value change over `over` seconds
    double over         = 0;
    double target       = 0;
};
// The trend over about the last minute. Holding: within max(1.5, 1 %) of a
// target above zero. Otherwise Rising or Falling by more than half a unit, or Flat.
TrendInfo trend(const std::vector<Reading> &readings, double target);

struct Candle
{
    double start = 0, end = 0;
    double open = 0, high = 0, low = 0, close = 0;
    int    count   = 0;
    bool   forming = false; // the last candle, still collecting readings
};
// One candle per `width` seconds from `start`; a period without readings has no candle.
std::vector<Candle> candles(const std::vector<Reading> &readings, double start, double width, double now);

enum class Feed { Waiting, Live, Stalled, Offline };
// Offline when the printer is not connected; Waiting before the first reading;
// Stalled when no reading came for stall_after seconds.
Feed feed_state(bool online, const std::optional<Reading> &last, double now, double stall_after = 10);

// The reading nearest to t (by time); -1 when there is none.
int nearest(const std::vector<Reading> &readings, double t);

// Minimum and maximum over the readings and the targets above zero, padded so
// a flat line is not drawn on the edge.
struct Range
{
    double low = 0, high = 1;
};
Range value_range(const std::vector<std::vector<Reading>> &series, const std::vector<double> &targets);

// time,series,value rows in UTC ISO 8601, one per reading, sorted by time.
std::string to_csv(const std::map<std::string, std::vector<Reading>> &series);

}}} // namespace Slic3r::GUI::LiveCharts

#endif // slic3r_GUI_LiveCharts_LiveSeries_hpp_
