#ifndef slic3r_GUI_MD3Tokens_hpp_
#define slic3r_GUI_MD3Tokens_hpp_

#include <wx/colour.h>

namespace MD3 {

enum class Role
{
    Surface,
    SurfaceDim,
    SurfaceBright,
    SurfaceContainerLowest,
    SurfaceContainerLow,
    SurfaceContainer,
    SurfaceContainerHigh,
    SurfaceContainerHighest,
    OnSurface,
    OnSurfaceVariant,
    Outline,
    OutlineVariant,
    Primary,
    OnPrimary,
    PrimaryContainer,
    OnPrimaryContainer,
    SecondaryContainer,
    OnSecondaryContainer,
    Error,
    ErrorContainer,
    InverseSurface,
    InverseOn,
};

namespace Light {

inline const wxColour surface{"#faf8fd"};
inline const wxColour surfaceDim{"#dad9e0"};
inline const wxColour surfaceBright{"#faf8fd"};
inline const wxColour scLowest{"#ffffff"};
inline const wxColour scLow{"#f4f2f9"};
inline const wxColour sc{"#eeedf3"};
inline const wxColour scHigh{"#e8e7ee"};
inline const wxColour scHighest{"#e2e1e9"};
inline const wxColour onSurface{"#1a1b1f"};
inline const wxColour onSurfaceVariant{"#44464e"};
inline const wxColour outline{"#75777f"};
inline const wxColour outlineVariant{"#c5c6d0"};
inline const wxColour primary{"#146c2e"};
inline const wxColour onPrimary{"#ffffff"};
inline const wxColour primaryContainer{"#a6f4b8"};
inline const wxColour onPrimaryContainer{"#00210c"};
inline const wxColour secondaryContainer{"#d7e8d9"};
inline const wxColour onSecondaryContainer{"#0e1f13"};
inline const wxColour error{"#ba1a1a"};
inline const wxColour errorContainer{"#ffdad6"};
inline const wxColour inverseSurface{"#2f3036"};
inline const wxColour inverseOn{"#f1f0f7"};

} // namespace Light

namespace Dark {

inline const wxColour surface{"#1b1c21"};
inline const wxColour surfaceDim{"#161619"};
inline const wxColour surfaceBright{"#3b3c43"};
inline const wxColour scLowest{"#131317"};
inline const wxColour scLow{"#202127"};
inline const wxColour sc{"#25262b"};
inline const wxColour scHigh{"#2f3036"};
inline const wxColour scHighest{"#393a41"};
inline const wxColour onSurface{"#e8e7ee"};
inline const wxColour onSurfaceVariant{"#cdced8"};
inline const wxColour outline{"#94959f"};
inline const wxColour outlineVariant{"#4a4c54"};
inline const wxColour primary{"#8bd89b"};
inline const wxColour onPrimary{"#00391a"};
inline const wxColour primaryContainer{"#095228"};
inline const wxColour onPrimaryContainer{"#a6f4b8"};
inline const wxColour secondaryContainer{"#2b3a2f"};
inline const wxColour onSecondaryContainer{"#cfe9d3"};
inline const wxColour error{"#ffb4ab"};
inline const wxColour errorContainer{"#93000a"};
inline const wxColour inverseSurface{"#e3e2e9"};
inline const wxColour inverseOn{"#2f3036"};

} // namespace Dark

// Resolve by semantic role instead of by light-mode RGB value. Several MD3
// roles deliberately share a light value but diverge in dark mode (for
// example surface and surfaceBright), so a colour-to-colour lookup cannot
// preserve their meaning.
inline const wxColour &resolve(Role role, bool dark)
{
    if (dark) {
        switch (role) {
        case Role::Surface: return Dark::surface;
        case Role::SurfaceDim: return Dark::surfaceDim;
        case Role::SurfaceBright: return Dark::surfaceBright;
        case Role::SurfaceContainerLowest: return Dark::scLowest;
        case Role::SurfaceContainerLow: return Dark::scLow;
        case Role::SurfaceContainer: return Dark::sc;
        case Role::SurfaceContainerHigh: return Dark::scHigh;
        case Role::SurfaceContainerHighest: return Dark::scHighest;
        case Role::OnSurface: return Dark::onSurface;
        case Role::OnSurfaceVariant: return Dark::onSurfaceVariant;
        case Role::Outline: return Dark::outline;
        case Role::OutlineVariant: return Dark::outlineVariant;
        case Role::Primary: return Dark::primary;
        case Role::OnPrimary: return Dark::onPrimary;
        case Role::PrimaryContainer: return Dark::primaryContainer;
        case Role::OnPrimaryContainer: return Dark::onPrimaryContainer;
        case Role::SecondaryContainer: return Dark::secondaryContainer;
        case Role::OnSecondaryContainer: return Dark::onSecondaryContainer;
        case Role::Error: return Dark::error;
        case Role::ErrorContainer: return Dark::errorContainer;
        case Role::InverseSurface: return Dark::inverseSurface;
        case Role::InverseOn: return Dark::inverseOn;
        }
    }

    switch (role) {
    case Role::Surface: return Light::surface;
    case Role::SurfaceDim: return Light::surfaceDim;
    case Role::SurfaceBright: return Light::surfaceBright;
    case Role::SurfaceContainerLowest: return Light::scLowest;
    case Role::SurfaceContainerLow: return Light::scLow;
    case Role::SurfaceContainer: return Light::sc;
    case Role::SurfaceContainerHigh: return Light::scHigh;
    case Role::SurfaceContainerHighest: return Light::scHighest;
    case Role::OnSurface: return Light::onSurface;
    case Role::OnSurfaceVariant: return Light::onSurfaceVariant;
    case Role::Outline: return Light::outline;
    case Role::OutlineVariant: return Light::outlineVariant;
    case Role::Primary: return Light::primary;
    case Role::OnPrimary: return Light::onPrimary;
    case Role::PrimaryContainer: return Light::primaryContainer;
    case Role::OnPrimaryContainer: return Light::onPrimaryContainer;
    case Role::SecondaryContainer: return Light::secondaryContainer;
    case Role::OnSecondaryContainer: return Light::onSecondaryContainer;
    case Role::Error: return Light::error;
    case Role::ErrorContainer: return Light::errorContainer;
    case Role::InverseSurface: return Light::inverseSurface;
    case Role::InverseOn: return Light::inverseOn;
    }

    return Light::surface;
}

struct DensityMetrics
{
    int gap;
    int padding;
    int row_height;
    int font_size;
    int rail_width;
    int sidebar_width;
    int radius;
    int small_radius;
};

namespace Metrics {

inline constexpr DensityMetrics comfortable{12, 16, 40, 14, 60, 344, 16, 10};
inline constexpr DensityMetrics compact{7, 10, 32, 13, 50, 312, 12, 8};

inline constexpr int top_bar_height         = 46;
inline constexpr int navigation_bar_height  = 52;
inline constexpr int prepare_actions_height = 66;
inline constexpr int preview_timeline_height = 58;

} // namespace Metrics

} // namespace MD3

#endif // slic3r_GUI_MD3Tokens_hpp_
