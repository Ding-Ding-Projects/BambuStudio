#ifndef slic3r_GUI_Drawing_DrawingModel_hpp_
#define slic3r_GUI_Drawing_DrawingModel_hpp_

// The drawing board, without wxWidgets: seven pens and an eraser, how each
// pen turns a pointer's path into a stroke (speed, pressure, direction,
// smoothing, Shift's eight directions), an eraser that removes only what it
// touches and splits a stroke in two, undo and redo, versions kept on this
// computer, and saving as JSON and exporting as SVG. Tested on its own by
// tests/drawing_model_test.cpp.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Slic3r { namespace GUI { namespace Drawing {

enum class Pen { Pencil = 0, Pen, Fineliner, Marker, Highlighter, Brush, Fountain, Eraser, Count };

enum class Width { Speed, Constant, Direction };

struct PenInfo
{
    Pen         pen;
    const char *id;
    const char *title; // untranslated
    char        key;   // '1'..'7', 'E'
    Width       width;
    float       default_width;   // px
    float       default_opacity; // 0..1
    bool        square;          // flat ends (Marker, Highlighter)
};
const std::vector<PenInfo> &pens();
const PenInfo              &info(Pen pen);
const PenInfo              *find_pen(std::string_view id);

struct Point
{
    float  x = 0, y = 0;
    float  width = 1; // resolved for this point
    double t = 0;     // seconds
};

struct Stroke
{
    Pen                pen     = Pen::Pen;
    std::uint32_t      color   = 0x000000; // 0xRRGGBB
    float              opacity = 1;
    std::vector<Point> points;
};

struct Document
{
    int                 width = 1000, height = 660;
    std::vector<Stroke> strokes;
};

// Turns pointer samples into a stroke for one pen.
class StrokeBuilder
{
public:
    void   start(Pen pen, std::uint32_t color, float width, float opacity, float smoothing, float x, float y, double t, float pressure = 1);
    // lock: keep the stroke on the nearest of eight directions from where it started.
    void   add(float x, float y, double t, float pressure = 1, bool lock = false);
    bool   active() const { return m_active; }
    Stroke finish();
    const Stroke &stroke() const { return m_stroke; }

private:
    float  width_at(float x, float y, double t, float pressure) const;
    bool   m_active = false;
    Stroke m_stroke;
    float  m_base = 4, m_smoothing = 0;
    float  m_start_x = 0, m_start_y = 0, m_last_x = 0, m_last_y = 0;
    double m_last_t = 0;
};

// Snaps (dx, dy) to the nearest of eight directions, keeping its length along that direction.
void lock_direction(float &dx, float &dy);

// Removes the parts of strokes within radius of (x, y); a stroke cut in the
// middle becomes two. Returns true when anything was removed.
bool erase(std::vector<Stroke> &strokes, float x, float y, float radius);

class History
{
public:
    explicit History(std::size_t limit = 200) : m_limit(limit) {}
    void reset(const std::vector<Stroke> &state);
    void push(const std::vector<Stroke> &state); // after a change
    bool can_undo() const { return m_index > 0; }
    bool can_redo() const { return m_index + 1 < m_states.size(); }
    const std::vector<Stroke> &undo();
    const std::vector<Stroke> &redo();
    const std::vector<Stroke> &current() const { return m_states[m_index]; }

private:
    std::size_t                      m_limit;
    std::vector<std::vector<Stroke>> m_states{std::vector<Stroke>{}};
    std::size_t                      m_index = 0;
};

struct Version
{
    std::int64_t        time = 0;
    std::string         label;
    std::vector<Stroke> strokes;
};

std::string to_json(const Document &doc, const std::vector<Version> &versions);
bool        from_json(std::string_view json, Document &doc, std::vector<Version> &versions);
// Exactly what the board shows, erasing included; background_png is a base64 PNG or empty.
std::string to_svg(const Document &doc, float scale, const std::string &background_png_base64);

std::string   hex_color(std::uint32_t rgb);       // "#RRGGBB"
bool          parse_hex(std::string_view text, std::uint32_t &rgb);

}}} // namespace Slic3r::GUI::Drawing

#endif // slic3r_GUI_Drawing_DrawingModel_hpp_
