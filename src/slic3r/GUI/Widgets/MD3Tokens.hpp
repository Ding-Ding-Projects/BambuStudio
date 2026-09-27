#ifndef slic3r_GUI_MD3Tokens_hpp_
#define slic3r_GUI_MD3Tokens_hpp_

#include <wx/colour.h>

#include <algorithm>
#include <cmath>

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
    OnError,
    OnErrorContainer,
    InversePrimary,
};

// Workspaces share the same neutral surfaces, type and error roles, while
// their primary accents communicate the current task context.  Keeping the
// schemes here prevents Preview/Device controls from falling back to legacy
// brand-green literals in individual wxWidgets and ImGui implementations.
enum class ColorScheme
{
    Brand,
    Preview,
    Device,
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
inline const wxColour onError{"#ffffff"};
inline const wxColour onErrorContainer{"#410002"};
inline const wxColour inversePrimary{"#8bd89b"};

// Overlay tints carry alpha over black. Scrim dims content behind modal
// surfaces; shadow tints the elevation drop-shadows (see Metrics::elev*).
inline const wxColour scrim{0, 0, 0, 82};  // rgba(0,0,0,.32)
inline const wxColour shadow{0, 0, 0, 41}; // rgba(0,0,0,.16)

} // namespace Light

namespace Dark {

// HEX-ALIAS INVARIANT (dark-mode double-remap fix): no Dark:: tone below may
// reuse a hex that is a light-side KEY in StateColor.cpp's gDarkColors table.
// StateColor::darkModeColorFor() (applied by StateColor::colorForStates at
// every paint AND by GUI_App::UpdateDarkUI on window fg/bg it revisits) maps
// light keys to dark values; when a dark tone aliases a light key, a second
// pass corrupts already-correct dark colours. That is exactly how OnSurface
// text (#e8e7ee, aliasing light Grey300) collapsed to #2f3036 — near-invisible
// dark-on-dark labels/fields all over dark mode. Three tones are therefore
// nudged by one RGB step off the kit values (imperceptible, but hex-distinct):
//   onSurface          #e8e7ee -> #e9e8ef (aliased ThemeColor::Grey300)
//   onPrimaryContainer #a6f4b8 -> #a7f5b9 (aliased Light::primaryContainer)
//   inversePrimary     #146c2e -> #156d2f (aliased ThemeColor::BrandGreen)
inline const wxColour surface{"#1b1c21"};
inline const wxColour surfaceDim{"#161619"};
inline const wxColour surfaceBright{"#3b3c43"};
inline const wxColour scLowest{"#131317"};
inline const wxColour scLow{"#202127"};
inline const wxColour sc{"#25262b"};
inline const wxColour scHigh{"#2f3036"};
inline const wxColour scHighest{"#393a41"};
inline const wxColour onSurface{"#e9e8ef"};
inline const wxColour onSurfaceVariant{"#cdced8"};
inline const wxColour outline{"#94959f"};
inline const wxColour outlineVariant{"#4a4c54"};
inline const wxColour primary{"#8bd89b"};
inline const wxColour onPrimary{"#00391a"};
inline const wxColour primaryContainer{"#095228"};
inline const wxColour onPrimaryContainer{"#a7f5b9"};
inline const wxColour secondaryContainer{"#2b3a2f"};
inline const wxColour onSecondaryContainer{"#cfe9d3"};
inline const wxColour error{"#ffb4ab"};
inline const wxColour errorContainer{"#93000a"};
inline const wxColour inverseSurface{"#e3e2e9"};
inline const wxColour inverseOn{"#2f3036"};
inline const wxColour onError{"#690005"};
inline const wxColour onErrorContainer{"#ffdad6"};
inline const wxColour inversePrimary{"#156d2f"};

// Overlay tints carry alpha over black. Dark theme deepens both tints.
inline const wxColour scrim{0, 0, 0, 153}; // rgba(0,0,0,.6)
inline const wxColour shadow{0, 0, 0, 128}; // rgba(0,0,0,.5)

} // namespace Dark

namespace Brand {

// #146c2e is the approved Brand/green seed. The Brand accent role tones live
// inline in Light::/Dark:: (resolved when scheme == ColorScheme::Brand); this
// constant exists only for symmetric scheme metadata with Preview/Device.
inline const wxColour seed{"#146c2e"};

} // namespace Brand

namespace Preview {

// #7c5cff is the approved Preview seed. The dark role is lifted for contrast
// against the shared dark surfaces, as prescribed by Material color roles.
inline const wxColour seed{"#7c5cff"};
inline const wxColour primaryLight{"#7050e8"};
inline const wxColour onPrimaryLight{"#ffffff"};
inline const wxColour primaryContainerLight{"#e8ddff"};
inline const wxColour onPrimaryContainerLight{"#23005c"};
inline const wxColour secondaryContainerLight{"#e5dff3"};
inline const wxColour onSecondaryContainerLight{"#211a2d"};
inline const wxColour primaryDark{"#ad98ff"};
inline const wxColour onPrimaryDark{"#2b006d"};
inline const wxColour primaryContainerDark{"#563bc2"};
inline const wxColour onPrimaryContainerDark{"#e8ddff"};
inline const wxColour secondaryContainerDark{"#494253"};
inline const wxColour onSecondaryContainerDark{"#e8dff5"};

} // namespace Preview

namespace Device {

// #14b8a6 is the approved Device seed. Accessible role tones are used for
// text-bearing controls in light and dark mode.
inline const wxColour seed{"#14b8a6"};
inline const wxColour primaryLight{"#0f766e"};
inline const wxColour onPrimaryLight{"#ffffff"};
inline const wxColour primaryContainerLight{"#9cf2e7"};
inline const wxColour onPrimaryContainerLight{"#00201d"};
inline const wxColour secondaryContainerLight{"#cce8e3"};
inline const wxColour onSecondaryContainerLight{"#08201d"};
inline const wxColour primaryDark{"#5eead4"};
inline const wxColour onPrimaryDark{"#003731"};
inline const wxColour primaryContainerDark{"#005047"};
inline const wxColour onPrimaryContainerDark{"#83f5e3"};
inline const wxColour secondaryContainerDark{"#304a46"};
inline const wxColour onSecondaryContainerDark{"#cce8e3"};

} // namespace Device

namespace detail {

// Runtime accent override. setAccentSeed() (bottom of this header) fills these
// six accent-role tones for light and dark from a user-chosen Appearance seed;
// while active, resolve(role, dark) returns them in place of the built-in Brand
// accent, so every StateColor::semantic() consumer re-themes on its next
// repaint/rebuild. The Preview/Device contextual schemes resolve their own
// accents (in the 3-arg resolve below) and are intentionally left untouched.
// Passing the Brand seed clears the override, restoring the hand-tuned tones.
struct AccentSlots
{
    wxColour primary;
    wxColour onPrimary;
    wxColour primaryContainer;
    wxColour onPrimaryContainer;
    wxColour secondaryContainer;
    wxColour onSecondaryContainer;
};

struct AccentOverrideState
{
    bool        active = false;
    AccentSlots light;
    AccentSlots dark;
};

inline AccentOverrideState &accentState()
{
    static AccentOverrideState state;
    return state;
}

// Non-null only for the six accent roles while an override is active; returns a
// pointer into the process-lifetime accentState() storage (semantic() copies the
// value immediately, mirroring how resolve() hands back the static role tones).
inline const wxColour *accentOverrideColour(Role role, bool dark)
{
    const AccentOverrideState &s = accentState();
    if (!s.active)
        return nullptr;
    const AccentSlots &a = dark ? s.dark : s.light;
    switch (role) {
    case Role::Primary: return &a.primary;
    case Role::OnPrimary: return &a.onPrimary;
    case Role::PrimaryContainer: return &a.primaryContainer;
    case Role::OnPrimaryContainer: return &a.onPrimaryContainer;
    case Role::SecondaryContainer: return &a.secondaryContainer;
    case Role::OnSecondaryContainer: return &a.onSecondaryContainer;
    default: return nullptr;
    }
}

} // namespace detail

// Resolve by semantic role instead of by light-mode RGB value. Several MD3
// roles deliberately share a light value but diverge in dark mode (for
// example surface and surfaceBright), so a colour-to-colour lookup cannot
// preserve their meaning.
inline const wxColour &resolve(Role role, bool dark)
{
    // A user-chosen Appearance accent (setAccentSeed) recolours the six accent
    // roles for the default (Brand) scheme; non-accent roles fall through.
    if (const wxColour *accent = detail::accentOverrideColour(role, dark))
        return *accent;

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
        case Role::OnError: return Dark::onError;
        case Role::OnErrorContainer: return Dark::onErrorContainer;
        case Role::InversePrimary: return Dark::inversePrimary;
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
    case Role::OnError: return Light::onError;
    case Role::OnErrorContainer: return Light::onErrorContainer;
    case Role::InversePrimary: return Light::inversePrimary;
    }

    return Light::surface;
}

inline const wxColour &resolve(Role role, bool dark, ColorScheme scheme)
{
    if (scheme == ColorScheme::Brand)
        return resolve(role, dark);

    if (scheme == ColorScheme::Preview) {
        if (dark) {
            switch (role) {
            case Role::Primary: return Preview::primaryDark;
            case Role::OnPrimary: return Preview::onPrimaryDark;
            case Role::PrimaryContainer: return Preview::primaryContainerDark;
            case Role::OnPrimaryContainer: return Preview::onPrimaryContainerDark;
            case Role::SecondaryContainer: return Preview::secondaryContainerDark;
            case Role::OnSecondaryContainer: return Preview::onSecondaryContainerDark;
            default: return resolve(role, true);
            }
        }
        switch (role) {
        case Role::Primary: return Preview::primaryLight;
        case Role::OnPrimary: return Preview::onPrimaryLight;
        case Role::PrimaryContainer: return Preview::primaryContainerLight;
        case Role::OnPrimaryContainer: return Preview::onPrimaryContainerLight;
        case Role::SecondaryContainer: return Preview::secondaryContainerLight;
        case Role::OnSecondaryContainer: return Preview::onSecondaryContainerLight;
        default: return resolve(role, false);
        }
    }

    if (dark) {
        switch (role) {
        case Role::Primary: return Device::primaryDark;
        case Role::OnPrimary: return Device::onPrimaryDark;
        case Role::PrimaryContainer: return Device::primaryContainerDark;
        case Role::OnPrimaryContainer: return Device::onPrimaryContainerDark;
        case Role::SecondaryContainer: return Device::secondaryContainerDark;
        case Role::OnSecondaryContainer: return Device::onSecondaryContainerDark;
        default: return resolve(role, true);
        }
    }
    switch (role) {
    case Role::Primary: return Device::primaryLight;
    case Role::OnPrimary: return Device::onPrimaryLight;
    case Role::PrimaryContainer: return Device::primaryContainerLight;
    case Role::OnPrimaryContainer: return Device::onPrimaryContainerLight;
    case Role::SecondaryContainer: return Device::secondaryContainerLight;
    case Role::OnSecondaryContainer: return Device::onSecondaryContainerLight;
    default: return resolve(role, false);
    }
}

// Theme-aware overlay tints. scrim() dims content behind modal surfaces;
// shadowTint() colours every elevation drop-shadow (see Metrics::elev*).
inline const wxColour &scrim(bool dark) { return dark ? Dark::scrim : Light::scrim; }
inline const wxColour &shadowTint(bool dark) { return dark ? Dark::shadow : Light::shadow; }

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

// A single soft drop-shadow: offset-x and spread are always 0, and the colour
// is the theme shadow tint (see shadowTint()). Only dy (offset-y) and blur
// radius vary across the five-step elevation ladder.
struct Elevation
{
    int dy;
    int blur;
};

namespace Metrics {

inline constexpr DensityMetrics comfortable{12, 16, 40, 14, 60, 344, 16, 10};
inline constexpr DensityMetrics compact{7, 10, 32, 13, 50, 312, 12, 8};

// Runtime density selection (Appearance > Density). The comfortable/compact
// presets above are unchanged and remain valid to read directly at any time;
// these accessors add a process-wide "which preset is active" state that
// consumers can consult (via active()) so radii/heights/paddings track the
// user's choice instead of being hardcoded. Set from Preferences on change and
// at construction (see setDensity in Preferences.cpp). Fixed panel widths such
// as settings_nav_width stay density-independent by design.
enum class Density
{
    Comfortable,
    Compact,
};

inline Density &density_state()
{
    static Density d = Density::Comfortable;
    return d;
}

inline void    setDensity(Density d) { density_state() = d; }
inline Density density()             { return density_state(); }
inline bool    isCompact()           { return density_state() == Density::Compact; }

// The density preset currently selected at runtime. Consumers that want to
// follow the Appearance > Density choice read active().<field> in place of a
// hardcoded comfortable/compact reference.
inline const DensityMetrics &active() { return isCompact() ? compact : comfortable; }

inline constexpr int top_bar_height         = 46;
inline constexpr int navigation_bar_height  = 52;
inline constexpr int prepare_actions_height = 66;
inline constexpr int preview_timeline_height = 58;

// Elevation ladder — offset-y + blur radius; colour = theme shadow tint.
// Shadow is reserved for floating chrome; layered surfaces use container
// steps instead.
inline constexpr Elevation elev1{1, 2};   // logo tile
inline constexpr Elevation elev2{2, 6};   // CTAs, chips
inline constexpr Elevation elev3{3, 10};  // floating toolbars
inline constexpr Elevation elev4{8, 28};  // popover, snackbar
inline constexpr Elevation elev5{16, 44}; // dialogs

// Fixed panel / dialog widths and centered-content bounds (density-independent).
inline constexpr int settings_nav_width   = 230;
inline constexpr int history_drawer_width = 410;
inline constexpr int popover_width        = 300;
inline constexpr int dialog_width_s       = 460; // Add-filament tier
inline constexpr int dialog_width_m       = 520;
inline constexpr int dialog_width_l       = 580; // Export/Send tier
inline constexpr int content_max_min      = 1080;
inline constexpr int content_max_max      = 1300;
inline constexpr int tab_active_indicator = 3; // tab-bar active underline

// Fixed shape radii (density-independent). 12/8 also appear via the compact
// density branch, but rail/snackbar (12) and tiny controls (8) need them at
// any density. Standalone buttons and chips use a pill radius = height / 2,
// computed at the call site.
inline constexpr int radius_dialog    = 28; // dialogs, hero cards
inline constexpr int radius_home      = 20; // home cards
inline constexpr int radius_icon_tile = 14; // icon tiles
inline constexpr int radius_rail      = 12; // rail buttons, snackbars
inline constexpr int radius_tiny      = 8;  // tiny controls

// Pill (stadium) radius for the fully-rounded controls the kit calls pills:
// standalone buttons, chips, switches, the search field, and nav items. Their
// corner radius is always half the control's height, not a fixed token, so it
// is computed at the call site. Pass the CURRENT, DPI-scaled height at
// paint/layout time (e.g. FromDIP(height)); a value cached at construction goes
// stale on a monitor-DPI or density change. Button::applyMD3Style() re-derives
// this on Rescale(), and the switch/thumb paths recompute it every paint.
// Segmented controls are NOT pills — they keep the fixed radius_rail / radius_tiny.
inline constexpr int pill_radius(int height) { return height / 2; }

} // namespace Metrics

// Prepare / Preview / Device 3D viewport only — never UI chrome. Axis colours
// tint the X/Y/Z gizmo and labels; live is the pulsing record/stream dot. All
// four are theme-independent (defined once in the design kit :root).
namespace Viewport {

inline const wxColour axisX{"#ea4335"};
inline const wxColour axisY{"#34a853"};
inline const wxColour axisZ{"#4c8bf5"};
inline const wxColour live{"#ff4747"};

} // namespace Viewport

// Type scale — px size + Roboto weight. Half-pixel sizes are intentional; keep
// the float source of truth and round only at the wx font layer. The 11px/600
// uppercase label with +0.6px tracking is the strongest recurring signature.
struct TypeStyle
{
    float size;
    int   weight;
};

namespace Type {

inline constexpr TypeStyle headline{23.0f, 700};      // hero greeting ("Welcome back")
inline constexpr TypeStyle page_title{20.0f, 700};    // page titles
inline constexpr TypeStyle dialog_title{18.0f, 600};  // dialog titles
inline constexpr TypeStyle section_title{16.0f, 700}; // content section titles (600-700)
inline constexpr TypeStyle card_title{15.0f, 600};    // card titles
inline constexpr TypeStyle body{14.0f, 400};          // base body; 13 compact via DensityMetrics
inline constexpr TypeStyle body_s{13.5f, 500};        // row primary text, buttons
inline constexpr TypeStyle body_xs{12.5f, 400};       // secondary rows, controls
inline constexpr TypeStyle caption{11.5f, 400};       // metadata
inline constexpr TypeStyle label{11.0f, 600};         // uppercase section labels, badges
inline constexpr TypeStyle micro{10.5f, 400};         // field captions

inline constexpr float label_tracking = 0.6f; // px letter-spacing on the uppercased label

inline constexpr const char *font_family = "Roboto";
inline constexpr const char *font_mono   = "Roboto Mono";               // numeric / technical values
inline constexpr const char *font_icon   = "Material Symbols Outlined"; // weight 400, FILL 1 when active

// Runtime UI font scale (Appearance > Font size) — the type analogue of the
// Metrics density state. Preferences reads the "ui_font_scale" AppConfig key and
// installs the multiplier here via setUiFontScale(); the font factory multiplies
// every computed point size by uiFontScale() when it (re)builds the
// Head_/Body_/Mono_ fonts (see Label::rebuild_fonts / Label::sysFont). Clamped to
// a legible 0.8..1.4 band. Like density, fonts are rebuilt once on change rather
// than rescaled per paint, so this is only read at font-build time.
inline double &ui_font_scale_state()
{
    static double s = 1.0;
    return s;
}

inline void setUiFontScale(double scale)
{
    if (scale < 0.8) scale = 0.8;
    if (scale > 1.4) scale = 1.4;
    ui_font_scale_state() = scale;
}

inline double uiFontScale() { return ui_font_scale_state(); }

} // namespace Type

// Live accent generator — ports accentVars() from the design kit's
// seed-algorithm. Produces the six accent role tones for a user-picked seed.
// The static Brand/Preview/Device sets above are hand-tuned and intentionally
// differ from this algorithm's output; this path is for the custom seed only.
struct AccentRoles
{
    wxColour primary;
    wxColour onPrimary;
    wxColour primaryContainer;
    wxColour onPrimaryContainer;
    wxColour secondaryContainer;
    wxColour onSecondaryContainer;
};

namespace detail {

// sRGB -> HSL. Hue in [0,360), saturation and lightness in [0,100]. Mirrors
// hexToHsl() in guidelines/seed-algorithm.html.
inline void rgbToHsl(const wxColour &c, double &h, double &s, double &l)
{
    const double r  = c.Red() / 255.0;
    const double g  = c.Green() / 255.0;
    const double b  = c.Blue() / 255.0;
    const double mx = std::max(r, std::max(g, b));
    const double mn = std::min(r, std::min(g, b));
    l = (mx + mn) / 2.0;
    if (mx == mn) {
        h = 0.0;
        s = 0.0;
    } else {
        const double d = mx - mn;
        s = l > 0.5 ? d / (2.0 - mx - mn) : d / (mx + mn);
        if (mx == r)
            h = (g - b) / d + (g < b ? 6.0 : 0.0);
        else if (mx == g)
            h = (b - r) / d + 2.0;
        else
            h = (r - g) / d + 4.0;
        h /= 6.0;
    }
    h *= 360.0;
    s *= 100.0;
    l *= 100.0;
}

// HSL (hue [0,360), saturation/lightness [0,100]) -> sRGB wxColour.
inline wxColour hslToRgb(double h, double s, double l)
{
    s /= 100.0;
    l /= 100.0;
    const double c  = (1.0 - std::fabs(2.0 * l - 1.0)) * s;
    const double hp = h / 60.0;
    const double x  = c * (1.0 - std::fabs(std::fmod(hp, 2.0) - 1.0));
    double r1 = 0.0, g1 = 0.0, b1 = 0.0;
    if (hp < 1.0) {
        r1 = c;
        g1 = x;
    } else if (hp < 2.0) {
        r1 = x;
        g1 = c;
    } else if (hp < 3.0) {
        g1 = c;
        b1 = x;
    } else if (hp < 4.0) {
        g1 = x;
        b1 = c;
    } else if (hp < 5.0) {
        r1 = x;
        b1 = c;
    } else {
        r1 = c;
        b1 = x;
    }
    const double m   = l - c / 2.0;
    auto         to8 = [](double v) {
        long n = std::lround(v * 255.0);
        if (n < 0)
            n = 0;
        if (n > 255)
            n = 255;
        return static_cast<unsigned char>(n);
    };
    return wxColour(to8(r1 + m), to8(g1 + m), to8(b1 + m));
}

} // namespace detail

// Regenerate the six accent roles from a seed colour. Saturation is clamped to
// [32,92]; hue is preserved; the seed's own lightness is discarded (each role
// derives its own tone). Container saturations round-half-up on s * multiplier.
inline AccentRoles accentFromSeed(const wxColour &seed, bool dark)
{
    double h = 0.0, s = 0.0, l = 0.0;
    detail::rgbToHsl(seed, h, s, l);
    s = std::max(32.0, std::min(92.0, s));

    AccentRoles roles;
    if (dark) {
        roles.primary              = detail::hslToRgb(h, s, 76.0);
        roles.onPrimary            = detail::hslToRgb(h, s, 16.0);
        roles.primaryContainer     = detail::hslToRgb(h, static_cast<double>(std::lround(s * 0.9)), 28.0);
        roles.onPrimaryContainer   = detail::hslToRgb(h, s, 90.0);
        roles.secondaryContainer   = detail::hslToRgb(h, static_cast<double>(std::lround(s * 0.35)), 26.0);
        roles.onSecondaryContainer = detail::hslToRgb(h, s, 88.0);
    } else {
        roles.primary              = detail::hslToRgb(h, s, 36.0);
        roles.onPrimary            = wxColour(255, 255, 255);
        roles.primaryContainer     = detail::hslToRgb(h, static_cast<double>(std::lround(s * 0.7)), 88.0);
        roles.onPrimaryContainer   = detail::hslToRgb(h, s, 12.0);
        roles.secondaryContainer   = detail::hslToRgb(h, static_cast<double>(std::lround(s * 0.45)), 90.0);
        roles.onSecondaryContainer = detail::hslToRgb(h, s, 20.0);
    }
    return roles;
}

// Apply an Appearance accent swatch (seed colour) to the default (Brand) colour
// scheme. Recomputes the six accent roles for both light and dark via
// accentFromSeed and installs them as the runtime override consulted by
// resolve(role, dark), so every StateColor::semantic() consumer picks up the new
// accent on its next repaint/rebuild. Passing the Brand seed (#146c2e) clears
// the override, restoring the hand-tuned Brand tones exactly. The Preview/Device
// contextual schemes resolve their own accents and are unaffected.
inline void setAccentSeed(const wxColour &seed)
{
    detail::AccentOverrideState &s = detail::accentState();

    const bool is_brand = seed.Red() == Brand::seed.Red() &&
                          seed.Green() == Brand::seed.Green() &&
                          seed.Blue() == Brand::seed.Blue();
    if (is_brand) {
        s.active = false;
        return;
    }

    const AccentRoles light = accentFromSeed(seed, false);
    const AccentRoles dark  = accentFromSeed(seed, true);
    s.light = {light.primary, light.onPrimary, light.primaryContainer,
               light.onPrimaryContainer, light.secondaryContainer, light.onSecondaryContainer};
    s.dark  = {dark.primary, dark.onPrimary, dark.primaryContainer,
               dark.onPrimaryContainer, dark.secondaryContainer, dark.onSecondaryContainer};
    s.active = true;
}

inline void clearAccentSeed() { detail::accentState().active = false; }
inline bool hasAccentSeed()   { return detail::accentState().active; }

} // namespace MD3

#endif // slic3r_GUI_MD3Tokens_hpp_
