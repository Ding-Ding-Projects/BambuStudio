#ifndef slic3r_GUI_ColorSpaces_hpp_
#define slic3r_GUI_ColorSpaces_hpp_

// Header-only colour maths for the Material colour picker's translator.
//
// Everything here is a pure function over small value structs; the only wx
// dependency is the pair of wxColour adapters at the bottom, compiled only
// when wx/colour.h has already been included by the translation unit.
//
// Conventions
//   * sRGB components are 0..1 doubles (`Rgb`), alpha 0..1, and the 8-bit
//     forms are produced only by the formatters.
//   * Hue is degrees 0..360, saturation/lightness/value/whiteness/blackness
//     are 0..1, CIELAB L is 0..100, OKLab L is 0..1, chroma is unbounded.
//   * CIE XYZ uses the D65 white point (0.95047, 1.00000, 1.08883), the same
//     reference the sRGB matrix is defined against, so no adaptation step.
//   * CMYK is the naive formula (K = 1 - max(R,G,B)); it is a display
//     convenience, not a press profile, and the docs say so.
//
// Round-trip precision
//   Every sRGB <-> space <-> sRGB round trip in this file reproduces the
//   8-bit input exactly after rounding (|delta| < 0.5/255) for in-gamut
//   colours; the double precision paths hold to ~5e-5 for XYZ/Lab/LCH (the
//   published 7-digit sRGB matrices are not exact inverses), ~1e-5 for
//   OKLab/OKLCH, ~1e-9 for HSL/HSV/HWB/CMYK, and
//   ~1e-4 in hue degrees near the achromatic axis (hue is undefined at zero
//   chroma and is reported as 0). Tests in tests/color_spaces assert both.
//
// Gamut
//   `to_srgb_clipped()` converts any space back to sRGB and reports whether
//   any channel had to be clamped into 0..1, so a caller can warn before
//   showing the clipped value.

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace MD3 { namespace Color {

struct Rgb   { double r { 0 }, g { 0 }, b { 0 }; };            // sRGB 0..1, gamma-encoded
struct Rgba  { double r { 0 }, g { 0 }, b { 0 }, a { 1 }; };
struct Hsl   { double h { 0 }, s { 0 }, l { 0 }; };
struct Hsv   { double h { 0 }, s { 0 }, v { 0 }; };
struct Hwb   { double h { 0 }, w { 0 }, b { 0 }; };
struct Xyz   { double x { 0 }, y { 0 }, z { 0 }; };            // D65, Y of white = 1
struct Lab   { double l { 0 }, a { 0 }, b { 0 }; };            // CIELAB, L 0..100
struct Lch   { double l { 0 }, c { 0 }, h { 0 }; };            // CIELCH
struct OkLab { double l { 0 }, a { 0 }, b { 0 }; };            // L 0..1
struct OkLch { double l { 0 }, c { 0 }, h { 0 }; };
struct Cmyk  { double c { 0 }, m { 0 }, y { 0 }, k { 0 }; };

struct Clipped {
    Rgb  rgb;
    bool clipped { false };
};

// ---------------------------------------------------------------- helpers

inline double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

inline double wrap_hue(double h)
{
    h = std::fmod(h, 360.0);
    if (h < 0.0) h += 360.0;
    return h;
}

inline int to_byte(double v) { return int(std::lround(clamp01(v) * 255.0)); }

constexpr double kEps = 1e-9;
constexpr double kPi  = 3.14159265358979323846;

inline double deg(double rad) { return rad * 180.0 / kPi; }
inline double rad(double deg) { return deg * kPi / 180.0; }

// ------------------------------------------------------- sRGB <-> linear

inline double srgb_to_linear(double c)
{
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

inline double linear_to_srgb(double c)
{
    return c <= 0.0031308 ? c * 12.92 : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
}

inline Rgb to_linear(const Rgb &c) { return { srgb_to_linear(c.r), srgb_to_linear(c.g), srgb_to_linear(c.b) }; }
inline Rgb to_srgb(const Rgb &lin) { return { linear_to_srgb(lin.r), linear_to_srgb(lin.g), linear_to_srgb(lin.b) }; }

// ------------------------------------------------------------ HSL / HSV / HWB

inline Hsv rgb_to_hsv(const Rgb &c)
{
    const double mx = std::max({ c.r, c.g, c.b }), mn = std::min({ c.r, c.g, c.b }), d = mx - mn;
    Hsv out;
    out.v = mx;
    out.s = mx <= kEps ? 0.0 : d / mx;
    if (d <= kEps)        out.h = 0.0;
    else if (mx == c.r)   out.h = 60.0 * std::fmod((c.g - c.b) / d, 6.0);
    else if (mx == c.g)   out.h = 60.0 * ((c.b - c.r) / d + 2.0);
    else                  out.h = 60.0 * ((c.r - c.g) / d + 4.0);
    out.h = wrap_hue(out.h);
    return out;
}

inline Rgb hsv_to_rgb(const Hsv &in)
{
    const double s = clamp01(in.s), v = clamp01(in.v);
    const double c  = v * s;
    const double hp = wrap_hue(in.h) / 60.0;
    const double x  = c * (1.0 - std::fabs(std::fmod(hp, 2.0) - 1.0));
    double r = 0, g = 0, b = 0;
    switch (int(hp) % 6) {
    case 0: r = c; g = x; break;
    case 1: r = x; g = c; break;
    case 2: g = c; b = x; break;
    case 3: g = x; b = c; break;
    case 4: r = x; b = c; break;
    default: r = c; b = x; break;
    }
    const double m = v - c;
    return { r + m, g + m, b + m };
}

inline Hsl rgb_to_hsl(const Rgb &c)
{
    const double mx = std::max({ c.r, c.g, c.b }), mn = std::min({ c.r, c.g, c.b }), d = mx - mn;
    Hsl out;
    out.l = (mx + mn) / 2.0;
    out.h = rgb_to_hsv(c).h;
    out.s = (d <= kEps || out.l <= kEps || out.l >= 1.0 - kEps) ? 0.0 : d / (1.0 - std::fabs(2.0 * out.l - 1.0));
    return out;
}

inline Rgb hsl_to_rgb(const Hsl &in)
{
    const double s = clamp01(in.s), l = clamp01(in.l);
    const double v = l + s * std::min(l, 1.0 - l);
    const double sv = v <= kEps ? 0.0 : 2.0 * (1.0 - l / v);
    return hsv_to_rgb({ in.h, sv, v });
}

inline Hwb rgb_to_hwb(const Rgb &c)
{
    const Hsv hsv = rgb_to_hsv(c);
    return { hsv.h, (1.0 - hsv.s) * hsv.v, 1.0 - hsv.v };
}

inline Rgb hwb_to_rgb(const Hwb &in)
{
    double w = clamp01(in.w), b = clamp01(in.b);
    if (w + b >= 1.0) { // achromatic: grey level w/(w+b)
        const double g = w / (w + b);
        return { g, g, g };
    }
    const double v = 1.0 - b;
    const double s = 1.0 - w / v;
    return hsv_to_rgb({ in.h, s, v });
}

// ------------------------------------------------------------- CIE XYZ (D65)

constexpr double kWhiteX = 0.95047, kWhiteY = 1.0, kWhiteZ = 1.08883;

inline Xyz rgb_to_xyz(const Rgb &c)
{
    const Rgb l = to_linear(c);
    return {
        0.4124564 * l.r + 0.3575761 * l.g + 0.1804375 * l.b,
        0.2126729 * l.r + 0.7151522 * l.g + 0.0721750 * l.b,
        0.0193339 * l.r + 0.1191920 * l.g + 0.9503041 * l.b,
    };
}

inline Rgb xyz_to_linear_rgb(const Xyz &c)
{
    return {
         3.2404542 * c.x - 1.5371385 * c.y - 0.4985314 * c.z,
        -0.9692660 * c.x + 1.8760108 * c.y + 0.0415560 * c.z,
         0.0556434 * c.x - 0.2040259 * c.y + 1.0572252 * c.z,
    };
}

// Linear -> gamma-encoded sRGB with an out-of-gamut report. The gamma curve
// is only defined on 0..1, so clamping happens in linear space.
inline Clipped linear_to_srgb_clipped(const Rgb &lin)
{
    constexpr double tol = 1e-6; // float noise from the matrices is not a gamut violation
    Clipped out;
    out.clipped = lin.r < -tol || lin.r > 1.0 + tol || lin.g < -tol || lin.g > 1.0 + tol ||
                  lin.b < -tol || lin.b > 1.0 + tol;
    out.rgb = to_srgb({ clamp01(lin.r), clamp01(lin.g), clamp01(lin.b) });
    return out;
}

inline Clipped xyz_to_rgb(const Xyz &c) { return linear_to_srgb_clipped(xyz_to_linear_rgb(c)); }

// ----------------------------------------------------------------- CIELAB

inline Lab xyz_to_lab(const Xyz &c)
{
    auto f = [](double t) {
        constexpr double d = 6.0 / 29.0;
        return t > d * d * d ? std::cbrt(t) : t / (3.0 * d * d) + 4.0 / 29.0;
    };
    const double fx = f(c.x / kWhiteX), fy = f(c.y / kWhiteY), fz = f(c.z / kWhiteZ);
    return { 116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz) };
}

inline Xyz lab_to_xyz(const Lab &c)
{
    auto finv = [](double t) {
        constexpr double d = 6.0 / 29.0;
        return t > d ? t * t * t : 3.0 * d * d * (t - 4.0 / 29.0);
    };
    const double fy = (c.l + 16.0) / 116.0;
    const double fx = fy + c.a / 500.0;
    const double fz = fy - c.b / 200.0;
    return { kWhiteX * finv(fx), kWhiteY * finv(fy), kWhiteZ * finv(fz) };
}

inline Lab     rgb_to_lab(const Rgb &c) { return xyz_to_lab(rgb_to_xyz(c)); }
inline Clipped lab_to_rgb(const Lab &c) { return xyz_to_rgb(lab_to_xyz(c)); }

inline Lch lab_to_lch(const Lab &c)
{
    const double chroma = std::hypot(c.a, c.b);
    return { c.l, chroma, chroma <= 1e-7 ? 0.0 : wrap_hue(deg(std::atan2(c.b, c.a))) };
}

inline Lab lch_to_lab(const Lch &c) { return { c.l, c.c * std::cos(rad(c.h)), c.c * std::sin(rad(c.h)) }; }

inline Lch     rgb_to_lch(const Rgb &c) { return lab_to_lch(rgb_to_lab(c)); }
inline Clipped lch_to_rgb(const Lch &c) { return lab_to_rgb(lch_to_lab(c)); }

// ------------------------------------------------------------------ OKLab

inline OkLab linear_rgb_to_oklab(const Rgb &l)
{
    const double lm = 0.4122214708 * l.r + 0.5363325363 * l.g + 0.0514459929 * l.b;
    const double mm = 0.2119034982 * l.r + 0.6806995451 * l.g + 0.1073969566 * l.b;
    const double sm = 0.0883024619 * l.r + 0.2817188376 * l.g + 0.6299787005 * l.b;
    const double l_ = std::cbrt(lm), m_ = std::cbrt(mm), s_ = std::cbrt(sm);
    return {
        0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_,
        1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_,
        0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_,
    };
}

inline Rgb oklab_to_linear_rgb(const OkLab &c)
{
    const double l_ = c.l + 0.3963377774 * c.a + 0.2158037573 * c.b;
    const double m_ = c.l - 0.1055613458 * c.a - 0.0638541728 * c.b;
    const double s_ = c.l - 0.0894841775 * c.a - 1.2914855480 * c.b;
    const double l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
    return {
         4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
        -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
        -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s,
    };
}

inline OkLab   rgb_to_oklab(const Rgb &c)   { return linear_rgb_to_oklab(to_linear(c)); }
inline Clipped oklab_to_rgb(const OkLab &c) { return linear_to_srgb_clipped(oklab_to_linear_rgb(c)); }

inline OkLch oklab_to_oklch(const OkLab &c)
{
    const double chroma = std::hypot(c.a, c.b);
    return { c.l, chroma, chroma <= 1e-7 ? 0.0 : wrap_hue(deg(std::atan2(c.b, c.a))) };
}

inline OkLab oklch_to_oklab(const OkLch &c) { return { c.l, c.c * std::cos(rad(c.h)), c.c * std::sin(rad(c.h)) }; }

inline OkLch   rgb_to_oklch(const Rgb &c)   { return oklab_to_oklch(rgb_to_oklab(c)); }
inline Clipped oklch_to_rgb(const OkLch &c) { return oklab_to_rgb(oklch_to_oklab(c)); }

// ------------------------------------------------------------- CMYK (naive)

inline Cmyk rgb_to_cmyk(const Rgb &c)
{
    const double k = 1.0 - std::max({ c.r, c.g, c.b });
    if (k >= 1.0 - kEps) return { 0, 0, 0, 1 };
    return { (1.0 - c.r - k) / (1.0 - k), (1.0 - c.g - k) / (1.0 - k), (1.0 - c.b - k) / (1.0 - k), k };
}

inline Rgb cmyk_to_rgb(const Cmyk &c)
{
    const double k = clamp01(c.k);
    return { (1.0 - clamp01(c.c)) * (1.0 - k), (1.0 - clamp01(c.m)) * (1.0 - k), (1.0 - clamp01(c.y)) * (1.0 - k) };
}

// -------------------------------------------------------------- named colours

struct NamedColor { const char *name; std::uint8_t r, g, b; };

// The CSS Color Level 4 named colours (148 entries, both grey spellings).
inline const std::vector<NamedColor> &css_named_colors()
{
    static const std::vector<NamedColor> table = {
        {"aliceblue",240,248,255},{"antiquewhite",250,235,215},{"aqua",0,255,255},{"aquamarine",127,255,212},
        {"azure",240,255,255},{"beige",245,245,220},{"bisque",255,228,196},{"black",0,0,0},
        {"blanchedalmond",255,235,205},{"blue",0,0,255},{"blueviolet",138,43,226},{"brown",165,42,42},
        {"burlywood",222,184,135},{"cadetblue",95,158,160},{"chartreuse",127,255,0},{"chocolate",210,105,30},
        {"coral",255,127,80},{"cornflowerblue",100,149,237},{"cornsilk",255,248,220},{"crimson",220,20,60},
        {"cyan",0,255,255},{"darkblue",0,0,139},{"darkcyan",0,139,139},{"darkgoldenrod",184,134,11},
        {"darkgray",169,169,169},{"darkgreen",0,100,0},{"darkgrey",169,169,169},{"darkkhaki",189,183,107},
        {"darkmagenta",139,0,139},{"darkolivegreen",85,107,47},{"darkorange",255,140,0},{"darkorchid",153,50,204},
        {"darkred",139,0,0},{"darksalmon",233,150,122},{"darkseagreen",143,188,143},{"darkslateblue",72,61,139},
        {"darkslategray",47,79,79},{"darkslategrey",47,79,79},{"darkturquoise",0,206,209},{"darkviolet",148,0,211},
        {"deeppink",255,20,147},{"deepskyblue",0,191,255},{"dimgray",105,105,105},{"dimgrey",105,105,105},
        {"dodgerblue",30,144,255},{"firebrick",178,34,34},{"floralwhite",255,250,240},{"forestgreen",34,139,34},
        {"fuchsia",255,0,255},{"gainsboro",220,220,220},{"ghostwhite",248,248,255},{"gold",255,215,0},
        {"goldenrod",218,165,32},{"gray",128,128,128},{"green",0,128,0},{"greenyellow",173,255,47},
        {"grey",128,128,128},{"honeydew",240,255,240},{"hotpink",255,105,180},{"indianred",205,92,92},
        {"indigo",75,0,130},{"ivory",255,255,240},{"khaki",240,230,140},{"lavender",230,230,250},
        {"lavenderblush",255,240,245},{"lawngreen",124,252,0},{"lemonchiffon",255,250,205},{"lightblue",173,216,230},
        {"lightcoral",240,128,128},{"lightcyan",224,255,255},{"lightgoldenrodyellow",250,250,210},{"lightgray",211,211,211},
        {"lightgreen",144,238,144},{"lightgrey",211,211,211},{"lightpink",255,182,193},{"lightsalmon",255,160,122},
        {"lightseagreen",32,178,170},{"lightskyblue",135,206,250},{"lightslategray",119,136,153},{"lightslategrey",119,136,153},
        {"lightsteelblue",176,196,222},{"lightyellow",255,255,224},{"lime",0,255,0},{"limegreen",50,205,50},
        {"linen",250,240,230},{"magenta",255,0,255},{"maroon",128,0,0},{"mediumaquamarine",102,205,170},
        {"mediumblue",0,0,205},{"mediumorchid",186,85,211},{"mediumpurple",147,112,219},{"mediumseagreen",60,179,113},
        {"mediumslateblue",123,104,238},{"mediumspringgreen",0,250,154},{"mediumturquoise",72,209,204},{"mediumvioletred",199,21,133},
        {"midnightblue",25,25,112},{"mintcream",245,255,250},{"mistyrose",255,228,225},{"moccasin",255,228,181},
        {"navajowhite",255,222,173},{"navy",0,0,128},{"oldlace",253,245,230},{"olive",128,128,0},
        {"olivedrab",107,142,35},{"orange",255,165,0},{"orangered",255,69,0},{"orchid",218,112,214},
        {"palegoldenrod",238,232,170},{"palegreen",152,251,152},{"paleturquoise",175,238,238},{"palevioletred",219,112,147},
        {"papayawhip",255,239,213},{"peachpuff",255,218,185},{"peru",205,133,63},{"pink",255,192,203},
        {"plum",221,160,221},{"powderblue",176,224,230},{"purple",128,0,128},{"rebeccapurple",102,51,153},
        {"red",255,0,0},{"rosybrown",188,143,143},{"royalblue",65,105,225},{"saddlebrown",139,69,19},
        {"salmon",250,128,114},{"sandybrown",244,164,96},{"seagreen",46,139,87},{"seashell",255,245,238},
        {"sienna",160,82,45},{"silver",192,192,192},{"skyblue",135,206,235},{"slateblue",106,90,205},
        {"slategray",112,128,144},{"slategrey",112,128,144},{"snow",255,250,250},{"springgreen",0,255,127},
        {"steelblue",70,130,180},{"tan",210,180,140},{"teal",0,128,128},{"thistle",216,191,216},
        {"tomato",255,99,71},{"turquoise",64,224,208},{"violet",238,130,238},{"wheat",245,222,179},
        {"white",255,255,255},{"whitesmoke",245,245,245},{"yellow",255,255,0},{"yellowgreen",154,205,50},
    };
    return table;
}

inline std::string lower_ascii(std::string_view s)
{
    std::string out(s);
    for (char &c : out) c = char(std::tolower((unsigned char) c));
    return out;
}

// Exact 8-bit match -> CSS name (first spelling in table order, so "gray"
// before "grey"), or nullopt when the colour has no name.
inline std::optional<std::string> name_of(const Rgb &c)
{
    const int r = to_byte(c.r), g = to_byte(c.g), b = to_byte(c.b);
    for (const NamedColor &n : css_named_colors())
        if (n.r == r && n.g == g && n.b == b) return std::string(n.name);
    return std::nullopt;
}

// Case-insensitive name -> colour, nullopt when unknown.
inline std::optional<Rgb> from_name(std::string_view name)
{
    const std::string key = lower_ascii(name);
    for (const NamedColor &n : css_named_colors())
        if (key == n.name) return Rgb { n.r / 255.0, n.g / 255.0, n.b / 255.0 };
    return std::nullopt;
}

// ------------------------------------------------------------------ contrast

// WCAG 2.x relative luminance of an sRGB colour.
inline double relative_luminance(const Rgb &c)
{
    const Rgb l = to_linear(c);
    return 0.2126 * l.r + 0.7152 * l.g + 0.0722 * l.b;
}

// WCAG contrast ratio, 1..21, order-independent.
inline double contrast_ratio(const Rgb &a, const Rgb &b)
{
    const double la = relative_luminance(a), lb = relative_luminance(b);
    const double hi = std::max(la, lb), lo = std::min(la, lb);
    return (hi + 0.05) / (lo + 0.05);
}

// Composite `fg` with alpha over an opaque `bg` (the colour the eye sees).
inline Rgb composite_over(const Rgba &fg, const Rgb &bg)
{
    const double a = clamp01(fg.a);
    return { fg.r * a + bg.r * (1 - a), fg.g * a + bg.g * (1 - a), fg.b * a + bg.b * (1 - a) };
}

// ---------------------------------------------------------------- formatting

namespace detail {

// Trim "1.500" -> "1.5", "2.000" -> "2".
inline std::string trim_num(double v, int decimals)
{
    if (std::fabs(v) < 0.5 * std::pow(10.0, -decimals)) v = 0.0; // avoid "-0"
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, v);
    std::string s(buf);
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s.empty() ? "0" : s;
}

inline std::string pct(double v01, int decimals = 1) { return trim_num(clamp01(v01) * 100.0, decimals) + "%"; }

} // namespace detail

inline std::string format_hex(const Rgb &c)
{
    char b[16];
    std::snprintf(b, sizeof b, "#%02X%02X%02X", to_byte(c.r), to_byte(c.g), to_byte(c.b));
    return b;
}

inline std::string format_hex8(const Rgba &c)
{
    char b[16];
    std::snprintf(b, sizeof b, "#%02X%02X%02X%02X", to_byte(c.r), to_byte(c.g), to_byte(c.b), to_byte(c.a));
    return b;
}

inline std::string format_rgb(const Rgb &c)
{
    char b[48];
    std::snprintf(b, sizeof b, "rgb(%d, %d, %d)", to_byte(c.r), to_byte(c.g), to_byte(c.b));
    return b;
}

inline std::string format_rgba(const Rgba &c)
{
    char b[64];
    std::snprintf(b, sizeof b, "rgba(%d, %d, %d, %s)", to_byte(c.r), to_byte(c.g), to_byte(c.b),
                  detail::trim_num(clamp01(c.a), 3).c_str());
    return b;
}

inline std::string format_hsl(const Hsl &c)
{
    return "hsl(" + detail::trim_num(c.h, 1) + ", " + detail::pct(c.s) + ", " + detail::pct(c.l) + ")";
}

inline std::string format_hsla(const Hsl &c, double a)
{
    return "hsla(" + detail::trim_num(c.h, 1) + ", " + detail::pct(c.s) + ", " + detail::pct(c.l) + ", " +
           detail::trim_num(clamp01(a), 3) + ")";
}

inline std::string format_hsv(const Hsv &c)
{
    return "hsv(" + detail::trim_num(c.h, 1) + ", " + detail::pct(c.s) + ", " + detail::pct(c.v) + ")";
}

inline std::string format_hwb(const Hwb &c)
{
    return "hwb(" + detail::trim_num(c.h, 1) + " " + detail::pct(c.w) + " " + detail::pct(c.b) + ")";
}

inline std::string format_xyz(const Xyz &c)
{
    return "xyz-d65(" + detail::trim_num(c.x, 4) + " " + detail::trim_num(c.y, 4) + " " + detail::trim_num(c.z, 4) + ")";
}

inline std::string format_lab(const Lab &c)
{
    return "lab(" + detail::trim_num(c.l, 2) + " " + detail::trim_num(c.a, 2) + " " + detail::trim_num(c.b, 2) + ")";
}

inline std::string format_lch(const Lch &c)
{
    return "lch(" + detail::trim_num(c.l, 2) + " " + detail::trim_num(c.c, 2) + " " + detail::trim_num(c.h, 1) + ")";
}

inline std::string format_oklab(const OkLab &c)
{
    return "oklab(" + detail::trim_num(c.l, 4) + " " + detail::trim_num(c.a, 4) + " " + detail::trim_num(c.b, 4) + ")";
}

inline std::string format_oklch(const OkLch &c)
{
    return "oklch(" + detail::trim_num(c.l, 4) + " " + detail::trim_num(c.c, 4) + " " + detail::trim_num(c.h, 1) + ")";
}

inline std::string format_cmyk(const Cmyk &c)
{
    return "cmyk(" + detail::pct(c.c, 0) + ", " + detail::pct(c.m, 0) + ", " + detail::pct(c.y, 0) + ", " + detail::pct(c.k, 0) + ")";
}

// ------------------------------------------------------------------ parsing

// Result of parsing any supported notation. `space` names the notation the
// text was written in ("hex", "rgb", "hsl", "hsv", "hwb", "xyz", "lab",
// "lch", "oklab", "oklch", "cmyk", "name"); `clipped` is set when the input
// described a colour outside sRGB and `rgba` holds the clamped value.
struct Parsed {
    Rgba        rgba;
    std::string space;
    bool        clipped { false };
};

namespace detail {

inline std::string_view trim(std::string_view s)
{
    while (!s.empty() && std::isspace((unsigned char) s.front())) s.remove_prefix(1);
    while (!s.empty() && std::isspace((unsigned char) s.back())) s.remove_suffix(1);
    return s;
}

// A numeric token with its unit suffix ("%", "deg", "" or "none").
struct Token { double value { 0 }; bool percent { false }; bool none { false }; };

// Tokenize the inside of "fn( ... )": commas, slashes and whitespace all
// separate; "/" is kept as its own marker so alpha can be told apart.
inline bool tokenize(std::string_view body, std::vector<Token> &out, int &alpha_index)
{
    alpha_index = -1;
    std::string cur;
    auto flush = [&]() -> bool {
        if (cur.empty()) return true;
        Token t;
        if (cur == "none") { t.none = true; }
        else {
            char *end = nullptr;
            t.value = std::strtod(cur.c_str(), &end);
            if (end == cur.c_str()) return false;
            std::string_view unit(end);
            if (unit == "%") t.percent = true;
            else if (unit == "deg" || unit.empty()) {}
            else if (unit == "turn") t.value *= 360.0;
            else if (unit == "rad") t.value = deg(t.value);
            else if (unit == "grad") t.value *= 0.9;
            else return false;
        }
        out.push_back(t);
        cur.clear();
        return true;
    };
    for (char ch : body) {
        if (ch == ',' || std::isspace((unsigned char) ch)) { if (!flush()) return false; }
        else if (ch == '/') { if (!flush()) return false; alpha_index = int(out.size()); }
        else cur += ch;
    }
    if (!flush()) return false;
    if (alpha_index >= 0 && alpha_index != int(out.size()) - 1) return false;
    return true;
}

inline double chan(const Token &t, double pct_scale, double plain_scale = 1.0)
{
    if (t.none) return 0.0;
    return t.percent ? t.value / 100.0 * pct_scale : t.value * plain_scale;
}

inline double alpha_of(const std::vector<Token> &tk, size_t n_channels)
{
    if (tk.size() <= n_channels) return 1.0;
    const Token &t = tk[n_channels];
    return clamp01(t.none ? 1.0 : (t.percent ? t.value / 100.0 : t.value));
}

inline int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

} // namespace detail

// Parse "#rgb", "#rgba", "#rrggbb", "#rrggbbaa" (leading '#' optional).
inline std::optional<Parsed> parse_hex(std::string_view s)
{
    s = detail::trim(s);
    if (!s.empty() && s.front() == '#') s.remove_prefix(1);
    if (s.size() != 3 && s.size() != 4 && s.size() != 6 && s.size() != 8) return std::nullopt;
    int v[8];
    for (size_t i = 0; i < s.size(); ++i) {
        v[i] = detail::hexval(s[i]);
        if (v[i] < 0) return std::nullopt;
    }
    Parsed p; p.space = "hex";
    if (s.size() <= 4) {
        p.rgba.r = v[0] * 17 / 255.0; p.rgba.g = v[1] * 17 / 255.0; p.rgba.b = v[2] * 17 / 255.0;
        p.rgba.a = s.size() == 4 ? v[3] * 17 / 255.0 : 1.0;
    } else {
        p.rgba.r = (v[0] * 16 + v[1]) / 255.0; p.rgba.g = (v[2] * 16 + v[3]) / 255.0; p.rgba.b = (v[4] * 16 + v[5]) / 255.0;
        p.rgba.a = s.size() == 8 ? (v[6] * 16 + v[7]) / 255.0 : 1.0;
    }
    return p;
}

// Parse any supported notation. Accepts CSS-like functional forms with either
// comma or space separators and an optional "/ alpha" or trailing alpha
// argument, hex, and CSS colour names. Returns nullopt for anything else.
inline std::optional<Parsed> parse(std::string_view text)
{
    using namespace detail;
    std::string_view s = trim(text);
    if (s.empty()) return std::nullopt;
    if (s.front() == '#') return parse_hex(s);

    const size_t open = s.find('(');
    if (open == std::string_view::npos) {
        if (auto named = from_name(s)) {
            Parsed p; p.space = "name"; p.rgba = { named->r, named->g, named->b, 1.0 };
            return p;
        }
        return parse_hex(s); // bare "rrggbb"
    }
    if (s.back() != ')') return std::nullopt;
    const std::string fn = lower_ascii(trim(s.substr(0, open)));
    std::vector<Token> tk; int alpha_index = -1;
    if (!tokenize(s.substr(open + 1, s.size() - open - 2), tk, alpha_index)) return std::nullopt;

    auto finish = [&](const Clipped &c, size_t n, const char *space) {
        Parsed p; p.space = space; p.clipped = c.clipped;
        p.rgba = { clamp01(c.rgb.r), clamp01(c.rgb.g), clamp01(c.rgb.b), alpha_of(tk, n) };
        return p;
    };
    auto plain = [&](const Rgb &c, size_t n, const char *space) {
        Clipped cl; cl.rgb = c;
        cl.clipped = c.r < -1e-9 || c.r > 1 + 1e-9 || c.g < -1e-9 || c.g > 1 + 1e-9 || c.b < -1e-9 || c.b > 1 + 1e-9;
        return finish(cl, n, space);
    };

    if (fn == "rgb" || fn == "rgba") {
        if (tk.size() < 3 || tk.size() > 4) return std::nullopt;
        return plain({ chan(tk[0], 1.0, 1 / 255.0), chan(tk[1], 1.0, 1 / 255.0), chan(tk[2], 1.0, 1 / 255.0) }, 3, "rgb");
    }
    if (fn == "hsl" || fn == "hsla") {
        if (tk.size() < 3 || tk.size() > 4) return std::nullopt;
        return plain(hsl_to_rgb({ chan(tk[0], 360.0), chan(tk[1], 1.0, tk[1].percent ? 1.0 : 0.01), chan(tk[2], 1.0, tk[2].percent ? 1.0 : 0.01) }), 3, "hsl");
    }
    if (fn == "hsv" || fn == "hsb" || fn == "hsva") {
        if (tk.size() < 3 || tk.size() > 4) return std::nullopt;
        return plain(hsv_to_rgb({ chan(tk[0], 360.0), chan(tk[1], 1.0, tk[1].percent ? 1.0 : 0.01), chan(tk[2], 1.0, tk[2].percent ? 1.0 : 0.01) }), 3, "hsv");
    }
    if (fn == "hwb") {
        if (tk.size() < 3 || tk.size() > 4) return std::nullopt;
        return plain(hwb_to_rgb({ chan(tk[0], 360.0), chan(tk[1], 1.0, tk[1].percent ? 1.0 : 0.01), chan(tk[2], 1.0, tk[2].percent ? 1.0 : 0.01) }), 3, "hwb");
    }
    if (fn == "xyz" || fn == "xyz-d65" || fn == "color-xyz") {
        if (tk.size() < 3 || tk.size() > 4) return std::nullopt;
        return finish(xyz_to_rgb({ chan(tk[0], 1.0), chan(tk[1], 1.0), chan(tk[2], 1.0) }), 3, "xyz");
    }
    if (fn == "lab") {
        if (tk.size() < 3 || tk.size() > 4) return std::nullopt;
        return finish(lab_to_rgb({ chan(tk[0], 100.0), chan(tk[1], 125.0), chan(tk[2], 125.0) }), 3, "lab");
    }
    if (fn == "lch") {
        if (tk.size() < 3 || tk.size() > 4) return std::nullopt;
        return finish(lch_to_rgb({ chan(tk[0], 100.0), chan(tk[1], 150.0), chan(tk[2], 360.0) }), 3, "lch");
    }
    if (fn == "oklab") {
        if (tk.size() < 3 || tk.size() > 4) return std::nullopt;
        return finish(oklab_to_rgb({ chan(tk[0], 1.0), chan(tk[1], 0.4), chan(tk[2], 0.4) }), 3, "oklab");
    }
    if (fn == "oklch") {
        if (tk.size() < 3 || tk.size() > 4) return std::nullopt;
        return finish(oklch_to_rgb({ chan(tk[0], 1.0), chan(tk[1], 0.4), chan(tk[2], 360.0) }), 3, "oklch");
    }
    if (fn == "cmyk" || fn == "device-cmyk") {
        if (tk.size() < 4 || tk.size() > 5) return std::nullopt;
        auto ch = [&](const Token &t) { return chan(t, 1.0, t.percent ? 1.0 : (t.value > 1.0 ? 0.01 : 1.0)); };
        return plain(cmyk_to_rgb({ ch(tk[0]), ch(tk[1]), ch(tk[2]), ch(tk[3]) }), 4, "cmyk");
    }
    return std::nullopt;
}

// ------------------------------------------------------ everything at once

// One row of the translator: which notation, and the colour written in it.
struct Translation { const char *space; std::string text; };

// Every representation of `c` the picker shows, in display order. Alpha is
// carried by the HEX8 / RGBA / HSLA rows; the rest describe the opaque colour.
inline std::vector<Translation> translate_all(const Rgba &c)
{
    const Rgb rgb { c.r, c.g, c.b };
    std::vector<Translation> out;
    if (auto n = name_of(rgb)) out.push_back({ "Name", *n });
    out.push_back({ "HEX",   format_hex(rgb) });
    out.push_back({ "HEX8",  format_hex8(c) });
    out.push_back({ "RGB",   format_rgb(rgb) });
    out.push_back({ "RGBA",  format_rgba(c) });
    const Hsl hsl = rgb_to_hsl(rgb);
    out.push_back({ "HSL",   format_hsl(hsl) });
    out.push_back({ "HSLA",  format_hsla(hsl, c.a) });
    out.push_back({ "HSV",   format_hsv(rgb_to_hsv(rgb)) });
    out.push_back({ "HWB",   format_hwb(rgb_to_hwb(rgb)) });
    const Xyz xyz = rgb_to_xyz(rgb);
    out.push_back({ "XYZ",   format_xyz(xyz) });
    const Lab lab = xyz_to_lab(xyz);
    out.push_back({ "Lab",   format_lab(lab) });
    out.push_back({ "LCH",   format_lch(lab_to_lch(lab)) });
    const OkLab ok = rgb_to_oklab(rgb);
    out.push_back({ "OKLab", format_oklab(ok) });
    out.push_back({ "OKLCH", format_oklch(oklab_to_oklch(ok)) });
    out.push_back({ "CMYK",  format_cmyk(rgb_to_cmyk(rgb)) });
    return out;
}

// ------------------------------------------------------------ wx adapters

#ifdef _WX_COLOUR_H_BASE_
inline Rgba from_wx(const wxColour &c)
{
    if (!c.IsOk()) return {};
    return { c.Red() / 255.0, c.Green() / 255.0, c.Blue() / 255.0, c.Alpha() / 255.0 };
}

inline wxColour to_wx(const Rgba &c)
{
    return wxColour((unsigned char) to_byte(c.r), (unsigned char) to_byte(c.g), (unsigned char) to_byte(c.b),
                    (unsigned char) to_byte(c.a));
}
#endif

} } // namespace MD3::Color

#endif // slic3r_GUI_ColorSpaces_hpp_
