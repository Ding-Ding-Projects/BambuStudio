#ifndef slic3r_GUI_LiveCharts_LiveSeries_hpp_
#define slic3r_GUI_LiveCharts_LiveSeries_hpp_

// Live charts on the Device page, without wxWidgets: the readings a printer
// sent this session (never a point it did not send), the time window shown,
// gaps that stay gaps, the trend in words, one-minute candles, the state of
// the feed (waiting, live, stalled, offline), the crosshair's nearest reading,
// the CSV of what is shown, the momentum arrow, new highs and their particles,
// the 12-hour or 24-hour clock and the saved chart options. Tested on its own
// by tests/live_series_test.cpp.

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
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
// Keeps a range inside [floor, ceiling] without making it narrower: a
// percentage is never drawn below 0 or above 100.
Range bounded(Range range, double floor, double ceiling);

// The momentum arrow at the newest point: which way the last seconds went.
enum class Momentum { Unknown, Up, Down, Steady };
struct MomentumInfo
{
    Momentum direction = Momentum::Unknown;
    double   rate      = 0; // units a second (least squares)
    double   span      = 0; // seconds the readings used cover
};
// The least-squares slope over the readings no more than `span` seconds older
// than the newest one. Unknown with fewer than two such readings or when they
// cover less than two seconds; Up or Down when the slope moves the value by
// more than `threshold` over the seconds covered; Steady otherwise.
MomentumInfo momentum(const std::vector<Reading> &readings, double span = 10, double threshold = 0.3);
// The arrow's angle on screen in radians: 0 points right and a negative angle
// points up (screen y grows downwards). Steeper than max_degrees is drawn at
// max_degrees so the arrow never points straight up or down.
double arrow_angle(double rate, double pixels_per_second, double pixels_per_unit, double max_degrees = 60);

// New highs, for the particles. A burst needs a reading above every earlier
// one, at least `margin` above the level of the last burst (or of the first
// reading), and `cooldown` seconds since the last burst. The first reading
// only sets the high, and so do the readings a chart already showed (prime).
class HighWatch
{
public:
    explicit HighWatch(double margin = 0.5, double cooldown = 5) : m_margin(margin), m_cooldown(cooldown) {}
    void                  prime(const std::vector<Reading> &readings);
    bool                  see(const Reading &reading);
    std::optional<double> high() const { return m_high; }
    void                  reset();

private:
    double                m_margin, m_cooldown;
    std::optional<double> m_high;
    std::optional<double> m_mark; // the level of the last burst
    double                m_last_burst = 0;
    bool                  m_burst_yet  = false;
};

// The particles of a burst, in pixels and seconds. The same seed gives the
// same burst, so a burst can be tested; at most kParticleLimit live at once.
struct Particle
{
    double x = 0, y = 0;   // pixels
    double vx = 0, vy = 0; // pixels a second; negative vy flies up
    double age = 0, life = 1;
    double size = 2;
};
constexpr std::size_t kParticleLimit = 160;
class Particles
{
public:
    // `count` particles from (x, y), fanning upwards; `scale` is the DPI scale.
    void                         burst(double x, double y, int count, std::uint32_t seed, double scale = 1);
    // Moves every particle on by dt seconds: drag slows it, gravity pulls it
    // down, and one older than its life is gone.
    void                         step(double dt);
    const std::vector<Particle> &live() const { return m_particles; }
    bool                         empty() const { return m_particles.empty(); }
    void                         clear() { m_particles.clear(); }
    // 1 for a new particle, 0 at the end of its life.
    static double                fade(const Particle &particle);

private:
    std::vector<Particle> m_particles;
    double                m_scale = 1;
};

// The clock on the time axis, under the crosshair and in the stalled note. A
// 12-hour clock shows 12 for the hours 0 and 12, no leading zero, and says AM
// or PM (pm; the caller adds the translated words). A 24-hour clock pads the
// hour to two digits.
struct ClockFace
{
    std::string digits;
    bool        twelve_hour = false;
    bool        pm          = false;
};
ClockFace clock_face(int hour, int minute, int second, bool twelve_hour, bool with_seconds);
// Windows' short time format (LOCALE_STIMEFORMAT, such as "h:mm tt" or
// "HH:mm"): true when its hours are on a 12-hour clock, that is an "h" outside
// quoted text.
bool twelve_hour_format(std::string_view windows_format);

// The chart options, saved in the app settings. The switches are on unless
// saved as "0", except particles, off unless saved as "1". The clock is "12"
// or "24"; until one is chosen it follows Windows' time format.
constexpr const char *kReferenceKey = "live_charts_reference";
constexpr const char *kMomentumKey  = "live_charts_momentum";
constexpr const char *kParticlesKey = "live_charts_particles";
constexpr const char *kClockKey     = "live_charts_clock";
struct ChartOptions
{
    bool reference    = true;
    bool momentum     = true;
    bool particles    = false;
    bool twelve_hour  = false;
    bool clock_chosen = false;
};
ChartOptions options_from(std::string_view saved_reference, std::string_view saved_momentum, std::string_view saved_particles, std::string_view saved_clock,
                          bool windows_twelve_hour);

// time,series,value rows in UTC ISO 8601, one per reading, sorted by time.
std::string to_csv(const std::map<std::string, std::vector<Reading>> &series);

}}} // namespace Slic3r::GUI::LiveCharts

#endif // slic3r_GUI_LiveCharts_LiveSeries_hpp_
