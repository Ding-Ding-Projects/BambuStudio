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
struct Settings {
    unsigned preset = 0;
    Fit fit = Fit::Contain;
    unsigned focal_x = 50, focal_y = 50, inset = 8;
    bool transparent = true;
    Pixel background {255, 255, 255, 255};
    bool valid() const { return preset < 3 && (fit == Fit::Contain || fit == Fit::Cover) &&
        focal_x <= 100 && focal_y <= 100 && inset <= 25 && background.a == 255; }
};

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
    if (!source.valid() || !settings.valid() || !size || size > 256) return {};
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

// Only neutral numeric presentation settings are serialized. Source bytes and
// filenames must never enter settings export, history or telemetry.
inline std::string serialize(const Settings& s)
{
    if (!s.valid()) return {};
    return "1 " + std::to_string(s.preset) + " " + std::to_string(int(s.fit)) + " " +
        std::to_string(s.focal_x) + " " + std::to_string(s.focal_y) + " " + std::to_string(s.inset) + " " +
        std::to_string(s.transparent ? 1 : 0) + " " + std::to_string(s.background.r) + " " +
        std::to_string(s.background.g) + " " + std::to_string(s.background.b);
}

inline bool deserialize(const std::string& text, Settings& destination)
{
    if (text.empty() || text.size() > 64) return false;
    std::array<unsigned, 10> values {};
    size_t pos = 0;
    for (size_t n = 0; n < values.size(); ++n) {
        if (pos == text.size() || text[pos] < '0' || text[pos] > '9') return false;
        while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
            values[n] = values[n] * 10 + unsigned(text[pos++] - '0');
            if (values[n] > 255) return false;
        }
        if (n + 1 < values.size() && (pos == text.size() || text[pos++] != ' ')) return false;
    }
    if (pos != text.size() || values[0] != 1 || values[2] > 1 || values[6] > 1) return false;
    Settings s;
    s.preset = values[1]; s.fit = Fit(values[2]); s.focal_x = values[3]; s.focal_y = values[4];
    s.inset = values[5]; s.transparent = values[6] != 0;
    s.background = {uint8_t(values[7]), uint8_t(values[8]), uint8_t(values[9]), 255};
    if (!s.valid()) return false;
    destination = s;
    return true;
}

} }
