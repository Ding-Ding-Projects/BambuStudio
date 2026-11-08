#include "LiveSeries.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <tuple>

namespace Slic3r { namespace GUI { namespace LiveCharts {

bool Series::add(double t, double value)
{
    if (!std::isfinite(t) || !std::isfinite(value)) return false;
    if (!m_readings.empty() && t <= m_readings.back().t) return false;
    m_readings.push_back(Reading{t, value});
    while (m_readings.size() > m_capacity) m_readings.pop_front();
    return true;
}

std::optional<Reading> Series::latest() const
{
    if (m_readings.empty()) return std::nullopt;
    return m_readings.back();
}

std::vector<Reading> Series::window(double now, double seconds) const
{
    std::vector<Reading> out;
    const double         from = now - seconds;
    auto it = std::lower_bound(m_readings.begin(), m_readings.end(), from, [](const Reading &r, double t) { return r.t < t; });
    for (; it != m_readings.end() && it->t <= now; ++it) out.push_back(*it);
    return out;
}

std::vector<std::vector<Reading>> segments(const std::vector<Reading> &readings, double gap_seconds)
{
    std::vector<std::vector<Reading>> out;
    for (const Reading &r : readings) {
#ifndef SLIC3R_LIVE_SERIES_NEGATIVE_FIXTURE
        if (out.empty() || r.t - out.back().back().t > gap_seconds) out.emplace_back();
#else
        (void) gap_seconds;
        if (out.empty()) out.emplace_back();
#endif
        out.back().push_back(r);
    }
    return out;
}

TrendInfo trend(const std::vector<Reading> &readings, double target)
{
    TrendInfo info;
    info.target = target;
    if (readings.empty()) return info;
    const Reading &last = readings.back();
    // The reading about a minute before the last one (or the first we have).
    const Reading *base = &readings.front();
    for (const Reading &r : readings)
        if (last.t - r.t <= 60.0) {
            base = &r;
            break;
        }
    info.delta = last.value - base->value;
    info.over  = last.t - base->t;
    if (target > 0 && std::fabs(last.value - target) <= std::max(1.5, target * 0.01)) info.trend = Trend::Holding;
    else if (info.over <= 0) info.trend = Trend::Unknown;
    else if (info.delta > 0.5) info.trend = Trend::Rising;
    else if (info.delta < -0.5) info.trend = Trend::Falling;
    else info.trend = Trend::Flat;
    return info;
}

std::vector<Candle> candles(const std::vector<Reading> &readings, double start, double width, double now)
{
    std::vector<Candle> out;
    if (width <= 0) return out;
    for (const Reading &r : readings) {
        if (r.t < start) continue;
        const double slot = start + std::floor((r.t - start) / width) * width;
        if (out.empty() || out.back().start != slot) {
            Candle c;
            c.start = slot;
            c.end   = slot + width;
            c.open = c.high = c.low = c.close = r.value;
            out.push_back(c);
        }
        Candle &c = out.back();
        c.high    = std::max(c.high, r.value);
        c.low     = std::min(c.low, r.value);
        c.close   = r.value;
        ++c.count;
    }
    if (!out.empty() && now < out.back().end) out.back().forming = true;
    return out;
}

Feed feed_state(bool online, const std::optional<Reading> &last, double now, double stall_after)
{
    if (!online) return Feed::Offline;
    if (!last) return Feed::Waiting;
    return now - last->t > stall_after ? Feed::Stalled : Feed::Live;
}

int nearest(const std::vector<Reading> &readings, double t)
{
    if (readings.empty()) return -1;
    auto it = std::lower_bound(readings.begin(), readings.end(), t, [](const Reading &r, double x) { return r.t < x; });
    if (it == readings.end()) return int(readings.size()) - 1;
    if (it == readings.begin()) return 0;
    const auto before = std::prev(it);
    return int((t - before->t <= it->t - t ? before : it) - readings.begin());
}

Range value_range(const std::vector<std::vector<Reading>> &series, const std::vector<double> &targets)
{
    bool   any = false;
    double low = 0, high = 0;
    const auto take = [&](double v) {
        if (!any) {
            low = high = v;
            any = true;
        } else {
            low  = std::min(low, v);
            high = std::max(high, v);
        }
    };
    for (const auto &s : series)
        for (const Reading &r : s) take(r.value);
    for (double t : targets)
        if (t > 0) take(t);
    if (!any) return Range{0, 1};
    const double span = std::max(high - low, 2.0);
    const double pad  = span * 0.12;
    return Range{low - pad, high + pad};
}

Range bounded(Range range, double floor, double ceiling)
{
    if (!(ceiling > floor)) return range;
    if (range.high - range.low >= ceiling - floor) return Range{floor, ceiling};
    if (range.low < floor) {
        range.high += floor - range.low;
        range.low = floor;
    }
    if (range.high > ceiling) {
        range.low -= range.high - ceiling;
        range.high = ceiling;
    }
    return range;
}

MomentumInfo momentum(const std::vector<Reading> &readings, double span, double threshold)
{
    MomentumInfo info;
    if (readings.size() < 2) return info;
    const double newest = readings.back().t;
    std::size_t  first  = readings.size() - 1;
    while (first > 0 && newest - readings[first - 1].t <= span) --first;
    const std::size_t count   = readings.size() - first;
    const double      covered = newest - readings[first].t;
    if (count < 2 || covered < 2.0) return info;
    double mean_t = 0, mean_v = 0;
    for (std::size_t i = first; i < readings.size(); ++i) {
        mean_t += readings[i].t - newest;
        mean_v += readings[i].value;
    }
    mean_t /= double(count);
    mean_v /= double(count);
    double num = 0, den = 0;
    for (std::size_t i = first; i < readings.size(); ++i) {
        const double dt = (readings[i].t - newest) - mean_t;
        num += dt * (readings[i].value - mean_v);
        den += dt * dt;
    }
    if (den <= 0) return info;
    info.rate           = num / den;
    info.span           = covered;
    const double moved  = info.rate * covered;
    info.direction      = moved > threshold ? Momentum::Up : moved < -threshold ? Momentum::Down : Momentum::Steady;
    return info;
}

double arrow_angle(double rate, double pixels_per_second, double pixels_per_unit, double max_degrees)
{
    constexpr double pi = 3.14159265358979323846;
    if (!(pixels_per_second > 0) || !std::isfinite(rate) || !std::isfinite(pixels_per_unit)) return 0;
    const double limit = std::clamp(max_degrees, 0.0, 89.0) * pi / 180.0;
    return std::clamp(std::atan2(-rate * pixels_per_unit, pixels_per_second), -limit, limit);
}

void HighWatch::prime(const std::vector<Reading> &readings)
{
    for (const Reading &r : readings)
        if (!m_high || r.value > *m_high) m_high = r.value;
    if (m_high && (!m_mark || *m_mark < *m_high)) m_mark = m_high;
}

bool HighWatch::see(const Reading &reading)
{
    if (!std::isfinite(reading.value)) return false;
    if (!m_high) {
        m_high = reading.value;
        m_mark = reading.value;
        return false;
    }
    if (!(reading.value > *m_high)) return false;
    m_high = reading.value;
    if (m_mark && reading.value < *m_mark + m_margin) return false;
#ifndef SLIC3R_LIVE_SERIES_HIGHS_NEGATIVE_FIXTURE
    if (m_burst_yet && reading.t - m_last_burst < m_cooldown) return false;
#endif
    m_mark       = reading.value;
    m_last_burst = reading.t;
    m_burst_yet  = true;
    return true;
}

void HighWatch::reset()
{
    m_high.reset();
    m_mark.reset();
    m_last_burst = 0;
    m_burst_yet  = false;
}

namespace {
// A small linear congruential generator, the same on every system, so the
// same seed always gives the same burst.
struct Lcg
{
    std::uint32_t state;
    double        next()
    {
        state = state * 1664525u + 1013904223u;
        return double(state >> 8) / double(1u << 24);
    }
};
} // namespace

void Particles::burst(double x, double y, int count, std::uint32_t seed, double scale)
{
    constexpr double pi = 3.14159265358979323846;
    m_scale             = scale > 0 ? scale : 1;
    Lcg rng{seed ? seed : 1u};
    for (int i = 0; i < count; ++i) {
        // Within 60 degrees either side of straight up.
        const double angle = -pi / 2 + (rng.next() - 0.5) * (pi * 2 / 3);
        const double speed = (70 + rng.next() * 110) * m_scale;
        Particle     p;
        p.x    = x;
        p.y    = y;
        p.vx   = std::cos(angle) * speed;
        p.vy   = std::sin(angle) * speed;
        p.life = 0.6 + rng.next() * 0.5;
        p.size = (1.5 + rng.next() * 1.5) * m_scale;
        m_particles.push_back(p);
    }
    if (m_particles.size() > kParticleLimit)
        m_particles.erase(m_particles.begin(), m_particles.begin() + std::ptrdiff_t(m_particles.size() - kParticleLimit));
}

void Particles::step(double dt)
{
    if (!(dt > 0)) return;
    const double keep    = std::exp(-2.2 * dt);
    const double gravity = 260 * m_scale;
    for (Particle &p : m_particles) {
        p.age += dt;
        p.vx *= keep;
        p.vy = p.vy * keep + gravity * dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
    }
    m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(), [](const Particle &p) { return p.age >= p.life; }), m_particles.end());
}

double Particles::fade(const Particle &particle) { return std::clamp(1.0 - particle.age / std::max(1e-9, particle.life), 0.0, 1.0); }

ClockFace clock_face(int hour, int minute, int second, bool twelve_hour, bool with_seconds)
{
    ClockFace face;
    face.twelve_hour = twelve_hour;
    hour             = ((hour % 24) + 24) % 24;
    char text[32];
    if (twelve_hour) {
        face.pm = hour >= 12;
#ifdef SLIC3R_LIVE_SERIES_CLOCK_NEGATIVE_FIXTURE
        const int shown = hour % 12; // Test-only mutation: midnight and noon read 0.
#else
        const int shown = hour % 12 == 0 ? 12 : hour % 12;
#endif
        if (with_seconds) std::snprintf(text, sizeof(text), "%d:%02d:%02d", shown, minute, second);
        else std::snprintf(text, sizeof(text), "%d:%02d", shown, minute);
    } else {
        if (with_seconds) std::snprintf(text, sizeof(text), "%02d:%02d:%02d", hour, minute, second);
        else std::snprintf(text, sizeof(text), "%02d:%02d", hour, minute);
    }
    face.digits = text;
    return face;
}

bool twelve_hour_format(std::string_view windows_format)
{
    bool quoted = false;
    for (const char c : windows_format) {
        if (c == '\'') {
            quoted = !quoted;
            continue;
        }
        if (quoted) continue;
        if (c == 'h') return true;
        if (c == 'H') return false;
    }
    return false;
}

ChartOptions options_from(std::string_view saved_reference, std::string_view saved_momentum, std::string_view saved_particles, std::string_view saved_clock,
                          bool windows_twelve_hour)
{
    ChartOptions options;
    options.reference    = saved_reference != "0";
    options.momentum     = saved_momentum != "0";
    options.particles    = saved_particles == "1";
    options.clock_chosen = saved_clock == "12" || saved_clock == "24";
    options.twelve_hour  = options.clock_chosen ? saved_clock == "12" : windows_twelve_hour;
    return options;
}

std::string to_csv(const std::map<std::string, std::vector<Reading>> &series)
{
    std::vector<std::tuple<double, std::string, double>> rows;
    for (const auto &[name, readings] : series)
        for (const Reading &r : readings) rows.emplace_back(r.t, name, r.value);
    std::stable_sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) { return std::get<0>(a) < std::get<0>(b); });
    std::string out = "time,series,value\n";
    char        line[160];
    for (const auto &[t, name, value] : rows) {
        const std::time_t whole = std::time_t(std::floor(t));
        std::tm           utc{};
#ifdef _WIN32
        gmtime_s(&utc, &whole);
#else
        gmtime_r(&whole, &utc);
#endif
        const int ms = int(std::lround((t - std::floor(t)) * 1000.0)) % 1000;
        std::snprintf(line, sizeof(line), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ,", utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour,
                      utc.tm_min, utc.tm_sec, ms);
        out += line;
        out += name;
        std::snprintf(line, sizeof(line), ",%.1f\n", value);
        out += line;
    }
    return out;
}

}}} // namespace Slic3r::GUI::LiveCharts
