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
