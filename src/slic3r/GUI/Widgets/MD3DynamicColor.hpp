#ifndef slic3r_GUI_MD3DynamicColor_hpp_
#define slic3r_GUI_MD3DynamicColor_hpp_

// Material Design 3 dynamic colour: a complete colour scheme derived from one
// seed colour.
//
// This is a C++ port of the parts of Google's material-color-utilities
// (Apache License 2.0, https://github.com/material-foundation/material-color-utilities)
// that a Tonal Spot scheme needs: the HCT colour space (CAM16 hue and chroma
// with CIELAB L* tone), the HCT solver that turns a hue, chroma and tone back
// into sRGB, tonal palettes, and the 2021 Material colour specification's role
// tones at standard contrast. Tonal Spot is the default Material You scheme.
//
// The code has no wxWidgets dependency so it can be unit tested on any host;
// MD3Tokens.hpp adapts the result to wxColour.
//
// Every role is checked byte for byte against the reference library in
// tests/md3_tokens/ (see md3_dynamic_color_reference.json and its generator).

#include <array>
#include <cstddef>
#include <cstdint>

namespace MD3 { namespace Dynamic {

using Argb = std::uint32_t; // 0xAARRGGBB, alpha always 0xFF

// Hue in degrees [0, 360), CAM16 chroma (>= 0), tone = CIELAB L* [0, 100].
struct Hct
{
    double hue    { 0.0 };
    double chroma { 0.0 };
    double tone   { 0.0 };
};

Hct  hct_from_argb(Argb argb);
// The closest in-gamut sRGB colour with the requested hue and tone and the
// requested chroma, or the highest chroma available at that hue and tone.
Argb argb_from_hct(double hue, double chroma, double tone);
double lstar_from_argb(Argb argb);

inline Argb argb_from_rgb(int r, int g, int b)
{
    return 0xFF000000u | (Argb(r & 0xFF) << 16) | (Argb(g & 0xFF) << 8) | Argb(b & 0xFF);
}
inline int red_of(Argb argb)   { return int((argb >> 16) & 0xFF); }
inline int green_of(Argb argb) { return int((argb >> 8) & 0xFF); }
inline int blue_of(Argb argb)  { return int(argb & 0xFF); }

// A hue and chroma held constant while tone varies from 0 (black) to 100
// (white).
class TonalPalette
{
public:
    TonalPalette() = default;
    TonalPalette(double hue, double chroma) : m_hue(hue), m_chroma(chroma) {}

    double hue() const { return m_hue; }
    double chroma() const { return m_chroma; }
    Argb   tone(double t) const;

private:
    double m_hue    { 0.0 };
    double m_chroma { 0.0 };
};

// Every colour role of the Material 3 scheme (2021 specification).
enum class SchemeRole : std::size_t
{
    Primary,
    OnPrimary,
    PrimaryContainer,
    OnPrimaryContainer,
    InversePrimary,
    Secondary,
    OnSecondary,
    SecondaryContainer,
    OnSecondaryContainer,
    Tertiary,
    OnTertiary,
    TertiaryContainer,
    OnTertiaryContainer,
    Error,
    OnError,
    ErrorContainer,
    OnErrorContainer,
    Background,
    OnBackground,
    Surface,
    SurfaceDim,
    SurfaceBright,
    SurfaceContainerLowest,
    SurfaceContainerLow,
    SurfaceContainer,
    SurfaceContainerHigh,
    SurfaceContainerHighest,
    OnSurface,
    SurfaceVariant,
    OnSurfaceVariant,
    InverseSurface,
    InverseOnSurface,
    Outline,
    OutlineVariant,
    Shadow,
    Scrim,
    SurfaceTint,
    PrimaryFixed,
    PrimaryFixedDim,
    OnPrimaryFixed,
    OnPrimaryFixedVariant,
    SecondaryFixed,
    SecondaryFixedDim,
    OnSecondaryFixed,
    OnSecondaryFixedVariant,
    TertiaryFixed,
    TertiaryFixedDim,
    OnTertiaryFixed,
    OnTertiaryFixedVariant,
    Count
};

constexpr std::size_t kSchemeRoleCount = static_cast<std::size_t>(SchemeRole::Count);

// Stable lower-camel-case role name, matching the reference library and the
// --md-sys-color-* custom property suffixes ("primaryContainer").
const char *role_name(SchemeRole role);

struct Palettes
{
    TonalPalette primary;
    TonalPalette secondary;
    TonalPalette tertiary;
    TonalPalette neutral;
    TonalPalette neutral_variant;
    TonalPalette error;
};

// Tonal Spot palettes for a seed: primary keeps the seed hue at chroma 36,
// secondary the same hue at 16, tertiary the hue rotated by 60 degrees at 24,
// neutrals the seed hue at 6 and 8, error hue 25 at chroma 84.
Palettes tonal_spot_palettes(Argb seed);

struct Scheme
{
    Argb seed  { 0xFF000000u };
    bool dark  { false };
    std::array<Argb, kSchemeRoleCount> colors {};

    Argb get(SchemeRole role) const { return colors[static_cast<std::size_t>(role)]; }
};

// The Tonal Spot scheme for a seed at standard contrast.
Scheme tonal_spot(Argb seed, bool dark);

}} // namespace MD3::Dynamic

#endif // slic3r_GUI_MD3DynamicColor_hpp_
