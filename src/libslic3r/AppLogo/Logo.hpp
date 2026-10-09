#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Slic3r { namespace AppLogo {

struct Pixel { uint8_t r = 0, g = 0, b = 0, a = 0; };
struct Image {
    unsigned width = 0, height = 0;
    std::vector<Pixel> pixels;
    bool valid() const { return width && height && width <= 2048 && height <= 2048 &&
        uint64_t(width) * height <= 1048576 && pixels.size() == size_t(width) * height; }
};
enum class Fit { Contain, Cover };

// AppConfig key holding serialize(settings). Empty or missing selects the
// shipped mark. The logo is presentation only: package identity, application
// ids, executables, installer, update feed, data folder and the installed
// operating-system icon never read this key.
constexpr const char *config_key = "app_logo";
constexpr unsigned preset_count = 3;
constexpr unsigned max_output_px = 256;

// Shipped: every surface keeps drawing its own shipped asset. Preset: the
// surfaces draw the generated preset rendered with the settings below.
enum class Source { Shipped, Preset };

struct Settings {
    Source source = Source::Shipped;
    unsigned preset = 0;
    Fit fit = Fit::Contain;
    unsigned focal_x = 50, focal_y = 50, inset = 8;
    bool transparent = true;
    Pixel background {255, 255, 255, 255};
    bool valid() const { return (source == Source::Shipped || source == Source::Preset) && preset < preset_count &&
        (fit == Fit::Contain || fit == Fit::Cover) &&
        focal_x <= 100 && focal_y <= 100 && inset <= 25 && background.a == 255; }
    bool shipped() const { return source == Source::Shipped; }
};

inline bool same(const Settings& a, const Settings& b)
{
    return a.source == b.source && a.preset == b.preset && a.fit == b.fit && a.focal_x == b.focal_x &&
        a.focal_y == b.focal_y && a.inset == b.inset && a.transparent == b.transparent &&
        a.background.r == b.background.r && a.background.g == b.background.g &&
        a.background.b == b.background.b && a.background.a == b.background.a;
}

// This is a bounded decoder primitive, not an isolation boundary. Do not connect
// an untrusted file picker until the host supplies a verified isolated worker.
inline bool decode_bmp(const std::vector<uint8_t>& data, Image& destination, std::string& error)
{
    auto reject = [&](const char* reason) { error = reason; return false; };
    if (data.size() < 54 || data.size() > 4 * 1024 * 1024)
        return reject("Bitmap size is outside the supported bounds.");
    auto u16 = [&](size_t p) { return unsigned(data[p]) | (unsigned(data[p + 1]) << 8); };
    auto u32 = [&](size_t p) { return uint32_t(data[p]) | (uint32_t(data[p + 1]) << 8) |
        (uint32_t(data[p + 2]) << 16) | (uint32_t(data[p + 3]) << 24); };
    if (data[0] != 'B' || data[1] != 'M' || u32(2) != data.size() || u32(10) != 54 ||
        u32(14) != 40 || u16(26) != 1 || u16(28) != 24 || u32(30) != 0 ||
        u32(46) != 0 || u32(50) != 0)
        return reject("Only exact, uncompressed 24-bit BMP files are supported.");
    const uint32_t w = u32(18), h = u32(22);
    if (!w || !h || w > 2048 || h > 2048 || uint64_t(w) * h > 1048576)
        return reject("Bitmap dimensions exceed the supported bounds.");
    const size_t stride = (size_t(w) * 3 + 3) & ~size_t(3);
    const size_t bytes = stride * h;
    if (data.size() != 54 + bytes || (u32(34) != 0 && u32(34) != bytes))
        return reject("Bitmap pixel data is truncated or has unexpected trailing data.");
    Image result {w, h, std::vector<Pixel>(size_t(w) * h)};
    for (unsigned y = 0; y < h; ++y)
        for (unsigned x = 0; x < w; ++x) {
            const size_t p = 54 + (h - 1 - y) * stride + x * 3;
            result.pixels[y * w + x] = {data[p + 2], data[p + 1], data[p], 255};
        }
    destination = std::move(result);
    error.clear();
    return true;
}

// Three original printer-themed marks, generated from deterministic geometry.
inline Image preset(unsigned index)
{
    if (index > 2) return {};
    const std::array<Pixel, 3> colors {{{0, 142, 93, 255}, {44, 91, 172, 255}, {180, 89, 23, 255}}};
    Image result {96, 80, std::vector<Pixel>(96 * 80)};
    for (unsigned y = 0; y < 80; ++y)
        for (unsigned x = 0; x < 96; ++x) {
            const bool frame = x >= 10 && x < 86 && y >= 5 && y < 74 &&
                (x < 17 || x >= 79 || y < 12 || y >= 67);
            const bool bed = x >= 22 && x < 74 && y >= 58 && y < 63;
            const bool gantry = y >= 21 && y < 27 && x >= 17 && x < 79;
            const bool nozzle = x >= 44 && x < 53 && y >= 27 && y < 36;
            const unsigned layer_width = index == 0 ? 25 : index == 1 ? 17 + (y % 8) : 10 + (y > 40 ? y - 40 : 0);
            const bool layers = y >= 39 && y < 56 && y % 4 < 2 && x >= 48 - layer_width && x < 48 + layer_width;
            if (frame || bed || gantry || nozzle || layers) result.pixels[y * 96 + x] = colors[index];
        }
    return result;
}

inline Image render(const Image& source, const Settings& settings, unsigned size)
{
    if (!source.valid() || !settings.valid() || !size || size > max_output_px) return {};
    const Pixel background = settings.transparent ? Pixel{} : settings.background;
    Image result {size, size, std::vector<Pixel>(size_t(size) * size, background)};
    const double inset = size * settings.inset / 100.0;
    const double available = size - inset * 2;
    const double scale = settings.fit == Fit::Contain ?
        std::min(available / source.width, available / source.height) :
        std::max(available / source.width, available / source.height);
    const double left = inset + (available - source.width * scale) * settings.focal_x / 100.0;
    const double top = inset + (available - source.height * scale) * settings.focal_y / 100.0;
    for (unsigned y = 0; y < size; ++y)
        for (unsigned x = 0; x < size; ++x) {
            if (x + .5 < inset || y + .5 < inset || x + .5 >= size - inset || y + .5 >= size - inset) continue;
            const int sx = int(std::floor((x + .5 - left) / scale));
            const int sy = int(std::floor((y + .5 - top) / scale));
            if (sx < 0 || sy < 0 || sx >= int(source.width) || sy >= int(source.height)) continue;
            Pixel p = source.pixels[size_t(sy) * source.width + sx];
            if (!settings.transparent) {
                auto blend = [&](unsigned a, unsigned b) { return uint8_t((a * p.a + b * (255 - p.a) + 127) / 255); };
                p = {blend(p.r, background.r), blend(p.g, background.g), blend(p.b, background.b), 255};
            }
            result.pixels[y * size + x] = p;
        }
    return result;
}

// The picker catalogue: the shipped mark first, then every generated preset.
// Display names are translated by the GUI; the ids are stable and searchable.
struct SourceEntry { Source source; unsigned preset; const char* id; };
inline const std::array<SourceEntry, 1 + preset_count>& sources()
{
    static const std::array<SourceEntry, 1 + preset_count> list {{
        {Source::Shipped, 0, "shipped"},
        {Source::Preset, 0, "green-printer"},
        {Source::Preset, 1, "blue-layers"},
        {Source::Preset, 2, "amber-nozzle"},
    }};
    return list;
}

inline int source_index(const Settings& s)
{
    if (!s.valid()) return -1;
    return s.shipped() ? 0 : int(1 + s.preset);
}

// Selects catalogue entry `index` and keeps every other presentation choice.
// An out-of-range index returns the settings unchanged.
inline Settings with_source(Settings s, size_t index)
{
    if (index >= sources().size()) return s;
    s.source = sources()[index].source;
    s.preset = sources()[index].preset;
    return s;
}

// The generated image a selection renders from. The shipped mark has none:
// each surface keeps drawing its own shipped asset.
inline Image source_image(const Settings& s)
{
    return s.valid() && s.source == Source::Preset ? preset(s.preset) : Image{};
}

inline Image render_selected(const Settings& s, unsigned size)
{
    return render(source_image(s), s, size);
}

// Where the selected logo is drawn, at its logical size. Each consumer asks
// plan() for the device size, so a high-DPI display never exceeds the bound.
enum class Target { TitleBar, WindowIcon, About, StartupScreen };
struct TargetSpec { Target target; const char* id; unsigned logical_px; };
inline const std::array<TargetSpec, 4>& chrome_targets()
{
    static const std::array<TargetSpec, 4> list {{
        {Target::TitleBar, "title-bar", 26},
        {Target::WindowIcon, "window-icon", 32},
        {Target::About, "about", 64},
        {Target::StartupScreen, "startup-screen", 122},
    }};
    return list;
}

inline unsigned logical_px(Target target)
{
    for (const auto& spec : chrome_targets())
        if (spec.target == target) return spec.logical_px;
    return 0;
}

// Window and taskbar icon sizes, each rendered from the source rather than
// resampled from another size.
constexpr std::array<unsigned, 9> window_icon_sizes {{16, 20, 24, 32, 40, 48, 64, 128, 256}};

// render_px is what render() generates (never above max_output_px); draw_px
// is the device size the surface occupies. When draw_px is larger the caller
// resamples the generated image up, so a bound is never bypassed.
struct RenderPlan { unsigned render_px = 0, draw_px = 0; };
inline RenderPlan plan(unsigned logical, double scale)
{
    if (!logical) return {};
    if (!(scale > 0.0) || !std::isfinite(scale)) scale = 1.0;
    scale = std::min(scale, 8.0);
    const unsigned draw = std::max(1u, unsigned(std::ceil(logical * scale - 1e-9)));
    return {std::min(draw, max_output_px), draw};
}

// Only neutral numeric presentation settings are serialized. Source bytes and
// filenames must never enter settings export, history or telemetry.
// Version 2 adds the source; version 1 (preset only) still reads as a preset.
inline std::string serialize(const Settings& s)
{
    if (!s.valid()) return {};
    return "2 " + std::to_string(int(s.source)) + " " + std::to_string(s.preset) + " " + std::to_string(int(s.fit)) + " " +
        std::to_string(s.focal_x) + " " + std::to_string(s.focal_y) + " " + std::to_string(s.inset) + " " +
        std::to_string(s.transparent ? 1 : 0) + " " + std::to_string(s.background.r) + " " +
        std::to_string(s.background.g) + " " + std::to_string(s.background.b);
}

inline bool deserialize(const std::string& text, Settings& destination)
{
    if (text.empty() || text.size() > 64) return false;
    std::array<unsigned, 11> values {};
    size_t count = 0, pos = 0;
    for (;;) {
        if (count == values.size() || pos == text.size() || text[pos] < '0' || text[pos] > '9') return false;
        unsigned value = 0;
        while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
            value = value * 10 + unsigned(text[pos++] - '0');
            if (value > 255) return false;
        }
        values[count++] = value;
        if (pos == text.size()) break;
        if (text[pos++] != ' ') return false;
    }
    const unsigned version = values[0];
    if (!((version == 1 && count == 10) || (version == 2 && count == 11))) return false;
    const size_t o = version == 2 ? 1 : 0;
    if (version == 2 && values[1] > 1) return false;
    if (values[2 + o] > 1 || values[6 + o] > 1) return false;
    Settings s;
    s.source = version == 2 ? Source(values[1]) : Source::Preset;
    s.preset = values[1 + o]; s.fit = Fit(values[2 + o]); s.focal_x = values[3 + o]; s.focal_y = values[4 + o];
    s.inset = values[5 + o]; s.transparent = values[6 + o] != 0;
    s.background = {uint8_t(values[7 + o]), uint8_t(values[8 + o]), uint8_t(values[9 + o]), 255};
    if (!s.valid()) return false;
    destination = s;
    return true;
}

// The value to persist: empty for the shipped defaults, so a reset clears the
// key, otherwise serialize(). Invalid settings are refused and `out` is kept.
inline bool stored_value(const Settings& s, std::string& out)
{
    if (!s.valid()) return false;
    out = same(s, Settings{}) ? std::string() : serialize(s);
    return true;
}

// Default: nothing stored. Stored: a valid value. Invalid: a value that does
// not parse; the shipped mark is shown and the value is left for the user to
// replace or reset, never silently rewritten.
enum class Provenance { Default, Stored, Invalid };
struct Resolved { Settings settings; Provenance provenance = Provenance::Default; };
inline Resolved resolve(const std::string& stored)
{
    Resolved result;
    if (stored.empty()) return result;
    if (deserialize(stored, result.settings)) result.provenance = Provenance::Stored;
    else { result.settings = Settings{}; result.provenance = Provenance::Invalid; }
    return result;
}

} }
