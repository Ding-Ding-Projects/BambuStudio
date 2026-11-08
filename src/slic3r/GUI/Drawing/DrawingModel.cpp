#include "DrawingModel.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Slic3r { namespace GUI { namespace Drawing {

using json = nlohmann::json;

namespace {
constexpr float kPi = 3.14159265358979f;
}

const std::vector<PenInfo> &pens()
{
    static const std::vector<PenInfo> all = {
        {Pen::Pencil, "pencil", "Pencil", '1', Width::Speed, 2.0f, 0.9f, false},
        {Pen::Pen, "pen", "Pen", '2', Width::Speed, 3.0f, 1.0f, false},
        {Pen::Fineliner, "fineliner", "Fineliner", '3', Width::Constant, 2.0f, 1.0f, false},
        {Pen::Marker, "marker", "Marker", '4', Width::Constant, 8.0f, 1.0f, true},
        {Pen::Highlighter, "highlighter", "Highlighter", '5', Width::Constant, 18.0f, 0.35f, true},
        {Pen::Brush, "brush", "Brush", '6', Width::Speed, 12.0f, 0.9f, false},
        {Pen::Fountain, "fountain", "Fountain pen", '7', Width::Direction, 5.0f, 1.0f, false},
        {Pen::Eraser, "eraser", "Eraser", 'E', Width::Constant, 16.0f, 1.0f, false},
    };
    return all;
}

const PenInfo &info(Pen pen) { return pens()[std::size_t(std::min(int(pen), int(Pen::Eraser)))]; }

const PenInfo *find_pen(std::string_view id)
{
    for (const PenInfo &p : pens())
        if (id == p.id) return &p;
    return nullptr;
}

// ---------------------------------------------------------------- strokes

void lock_direction(float &dx, float &dy)
{
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0) return;
    const float angle   = std::atan2(dy, dx);
    const float step    = kPi / 4.0f;
    const float snapped = std::round(angle / step) * step;
    const float along   = dx * std::cos(snapped) + dy * std::sin(snapped);
    dx                  = along * std::cos(snapped);
    dy                  = along * std::sin(snapped);
}

void StrokeBuilder::start(Pen pen, std::uint32_t color, float width, float opacity, float smoothing, float x, float y, double t, float pressure)
{
    m_active         = true;
    m_stroke         = Stroke{};
    m_stroke.pen     = pen;
    m_stroke.color   = color & 0xFFFFFF;
    m_stroke.opacity = std::clamp(opacity, 0.05f, 1.0f);
    m_base           = std::clamp(width, 0.5f, 120.0f);
    m_smoothing      = std::clamp(smoothing, 0.0f, 0.9f);
    m_start_x = m_last_x = x;
    m_start_y = m_last_y = y;
    m_last_t             = t;
    m_stroke.points.push_back(Point{x, y, width_at(x, y, t, pressure), t});
}

float StrokeBuilder::width_at(float x, float y, double t, float pressure) const
{
    const PenInfo &p = info(m_stroke.pen);
    pressure         = std::clamp(pressure, 0.1f, 1.0f);
    switch (p.width) {
    case Width::Constant: return m_base;
    case Width::Direction: {
        // A nib held at 45 degrees: wide across it, thin along it.
        const float dx = x - m_last_x, dy = y - m_last_y;
        if (dx == 0 && dy == 0) return m_base * pressure;
        const float angle = std::atan2(dy, dx);
        return m_base * (0.3f + 0.7f * std::fabs(std::sin(angle - kPi / 4.0f))) * pressure;
    }
    case Width::Speed: {
        const double dt    = std::max(1e-3, t - m_last_t);
        const float  dist  = std::sqrt((x - m_last_x) * (x - m_last_x) + (y - m_last_y) * (y - m_last_y));
        const float  speed = float(dist / dt); // px per second
        const float  thin  = std::clamp(1.15f - speed / 2000.0f, 0.45f, 1.15f);
        return m_base * thin * pressure;
    }
    }
    return m_base;
}

void StrokeBuilder::add(float x, float y, double t, float pressure, bool lock)
{
    if (!m_active) return;
    if (lock) {
        float dx = x - m_start_x, dy = y - m_start_y;
        lock_direction(dx, dy);
        x = m_start_x + dx;
        y = m_start_y + dy;
    } else if (m_smoothing > 0) {
        x = m_last_x + (x - m_last_x) * (1.0f - m_smoothing);
        y = m_last_y + (y - m_last_y) * (1.0f - m_smoothing);
    }
    if (x == m_last_x && y == m_last_y) return;
    Point p{x, y, width_at(x, y, t, pressure), t};
    // A locked stroke is a straight line: keep only where it starts and ends.
    if (lock && m_stroke.points.size() >= 2) m_stroke.points.resize(1);
    m_stroke.points.push_back(p);
    m_last_x = x;
    m_last_y = y;
    m_last_t = t;
}

Stroke StrokeBuilder::finish()
{
    m_active = false;
    return std::move(m_stroke);
}

bool erase(std::vector<Stroke> &strokes, float x, float y, float radius)
{
    bool                any = false;
    std::vector<Stroke> out;
    out.reserve(strokes.size());
    const float r2 = radius * radius;
    for (Stroke &s : strokes) {
        Stroke piece  = s;
        piece.points.clear();
        bool touched = false;
        for (const Point &p : s.points) {
            const float reach = radius + p.width / 2.0f;
            const bool  hit   = (p.x - x) * (p.x - x) + (p.y - y) * (p.y - y) <= std::max(r2, reach * reach);
            if (hit) {
                touched = true;
#ifndef SLIC3R_DRAWING_MODEL_NEGATIVE_FIXTURE
                // Only what the eraser touches goes: the stroke before it stays as its own stroke.
                if (!piece.points.empty()) out.push_back(piece);
                piece.points.clear();
#else
                piece.points.clear();
                out.clear();
#endif
                continue;
            }
            piece.points.push_back(p);
        }
        if (!piece.points.empty()) out.push_back(piece);
        any = any || touched;
    }
    if (any) strokes = std::move(out);
    return any;
}

// ---------------------------------------------------------------- history

void History::reset(const std::vector<Stroke> &state)
{
    m_states = {state};
    m_index  = 0;
}

void History::push(const std::vector<Stroke> &state)
{
    m_states.resize(m_index + 1);
    m_states.push_back(state);
    // limit counts the changes that can be undone, so one more state than that is kept.
    if (m_states.size() > m_limit + 1) m_states.erase(m_states.begin());
    m_index = m_states.size() - 1;
}

const std::vector<Stroke> &History::undo()
{
    if (m_index > 0) --m_index;
    return m_states[m_index];
}

const std::vector<Stroke> &History::redo()
{
    if (m_index + 1 < m_states.size()) ++m_index;
    return m_states[m_index];
}

// ---------------------------------------------------------------- colours

std::string hex_color(std::uint32_t rgb)
{
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%06X", unsigned(rgb & 0xFFFFFF));
    return buf;
}

bool parse_hex(std::string_view text, std::uint32_t &rgb)
{
    if (!text.empty() && text.front() == '#') text.remove_prefix(1);
    if (text.size() != 6) return false;
    std::uint32_t v = 0;
    for (char c : text) {
        v <<= 4;
        if (c >= '0' && c <= '9') v |= std::uint32_t(c - '0');
        else if (c >= 'a' && c <= 'f') v |= std::uint32_t(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= std::uint32_t(c - 'A' + 10);
        else return false;
    }
    rgb = v;
    return true;
}

// ---------------------------------------------------------------- saving

namespace {

json strokes_json(const std::vector<Stroke> &strokes)
{
    json list = json::array();
    for (const Stroke &s : strokes) {
        json points = json::array();
        for (const Point &p : s.points)
            points.push_back(json::array({std::round(p.x * 10) / 10, std::round(p.y * 10) / 10, std::round(p.width * 10) / 10, std::round(p.t * 1000) / 1000}));
        list.push_back(json{{"pen", info(s.pen).id}, {"color", hex_color(s.color)}, {"opacity", s.opacity}, {"points", points}});
    }
    return list;
}

bool strokes_from(const json &list, std::vector<Stroke> &out)
{
    if (!list.is_array()) return false;
    out.clear();
    for (const json &j : list) {
        if (!j.is_object() || !j.contains("pen") || !j["pen"].is_string() || !j.contains("points") || !j["points"].is_array()) continue;
        const PenInfo *pen = find_pen(j["pen"].get<std::string>());
        if (!pen || pen->pen == Pen::Eraser) continue;
        Stroke s;
        s.pen = pen->pen;
        if (j.contains("color") && j["color"].is_string()) parse_hex(j["color"].get<std::string>(), s.color);
        if (j.contains("opacity") && j["opacity"].is_number()) s.opacity = std::clamp(j["opacity"].get<float>(), 0.05f, 1.0f);
        for (const json &p : j["points"]) {
            if (!p.is_array() || p.size() < 3 || !p[0].is_number() || !p[1].is_number() || !p[2].is_number()) continue;
            Point pt{p[0].get<float>(), p[1].get<float>(), std::clamp(p[2].get<float>(), 0.1f, 200.0f), p.size() > 3 && p[3].is_number() ? p[3].get<double>() : 0.0};
            if (!std::isfinite(pt.x) || !std::isfinite(pt.y)) continue;
            s.points.push_back(pt);
        }
        if (!s.points.empty()) out.push_back(std::move(s));
    }
    return true;
}

} // namespace

std::string to_json(const Document &doc, const std::vector<Version> &versions)
{
    json v = json::array();
    for (const Version &ver : versions) v.push_back(json{{"time", ver.time}, {"label", ver.label}, {"strokes", strokes_json(ver.strokes)}});
    return json{{"version", 1}, {"width", doc.width}, {"height", doc.height}, {"strokes", strokes_json(doc.strokes)}, {"versions", v}}.dump();
}

bool from_json(std::string_view text, Document &doc, std::vector<Version> &versions)
{
    const json j = json::parse(text.begin(), text.end(), nullptr, false);
    if (j.is_discarded() || !j.is_object()) return false;
    Document d;
    if (j.contains("width") && j["width"].is_number_integer()) d.width = std::clamp(j["width"].get<int>(), 16, 8192);
    if (j.contains("height") && j["height"].is_number_integer()) d.height = std::clamp(j["height"].get<int>(), 16, 8192);
    if (!j.contains("strokes") || !strokes_from(j["strokes"], d.strokes)) return false;
    std::vector<Version> vs;
    if (j.contains("versions") && j["versions"].is_array())
        for (const json &v : j["versions"]) {
            Version ver;
            if (!v.is_object() || !v.contains("strokes") || !strokes_from(v["strokes"], ver.strokes)) continue;
            if (v.contains("time") && v["time"].is_number_integer()) ver.time = v["time"].get<std::int64_t>();
            if (v.contains("label") && v["label"].is_string()) ver.label = v["label"].get<std::string>().substr(0, 120);
            vs.push_back(std::move(ver));
        }
    doc      = std::move(d);
    versions = std::move(vs);
    return true;
}

std::string to_svg(const Document &doc, float scale, const std::string &background_png_base64)
{
    scale = std::clamp(scale, 0.25f, 4.0f);
    char        buf[256];
    std::string out;
    std::snprintf(buf, sizeof(buf), "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\">\n",
                  int(std::lround(doc.width * scale)), int(std::lround(doc.height * scale)), doc.width, doc.height);
    out += buf;
    if (!background_png_base64.empty()) {
        std::snprintf(buf, sizeof(buf), "<image x=\"0\" y=\"0\" width=\"%d\" height=\"%d\" href=\"data:image/png;base64,", doc.width, doc.height);
        out += buf;
        out += background_png_base64;
        out += "\"/>\n";
    }
    for (const Stroke &s : doc.strokes) {
        const std::string color = hex_color(s.color);
        const char       *cap   = info(s.pen).square ? "square" : "round";
        if (s.points.size() == 1) {
            const Point &p = s.points.front();
            std::snprintf(buf, sizeof(buf), "<circle cx=\"%.1f\" cy=\"%.1f\" r=\"%.2f\" fill=\"%s\" fill-opacity=\"%.2f\"/>\n", p.x, p.y, p.width / 2,
                          color.c_str(), s.opacity);
            out += buf;
            continue;
        }
        // The group fades as one, so overlapping segments of a see-through pen never darken.
        std::snprintf(buf, sizeof(buf), "<g opacity=\"%.2f\" stroke=\"%s\" stroke-linecap=\"%s\" fill=\"none\">\n", s.opacity, color.c_str(), cap);
        out += buf;
        for (std::size_t i = 1; i < s.points.size(); ++i) {
            const Point &a = s.points[i - 1], &b = s.points[i];
            std::snprintf(buf, sizeof(buf), "<line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke-width=\"%.2f\"/>\n", a.x, a.y, b.x, b.y,
                          (a.width + b.width) / 2);
            out += buf;
        }
        out += "</g>\n";
    }
    out += "</svg>\n";
    return out;
}

}}} // namespace Slic3r::GUI::Drawing
