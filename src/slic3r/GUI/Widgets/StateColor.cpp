#include "StateColor.hpp"
#include <wx/gdicmn.h>

static bool gDarkMode = false;

static bool operator<(wxColour const &l, wxColour const &r) { return l.GetRGBA() < r.GetRGBA(); }

// IDEMPOTENCY INVARIANT: no VALUE on the dark side of this table may equal any
// KEY on the light side. darkModeColorFor() is applied both at paint time
// (StateColor::colorForStates) and by GUI_App::UpdateDarkUI over window fg/bg
// colours it may visit repeatedly, so the mapping must be a fixed point on its
// own output — otherwise a second pass corrupts an already-dark colour (the
// old "#e8e7ee -> #2f3036" collapse that made dark-mode text near-invisible).
// New neutral-role pairs and historical raw RGB aliases share one destination
// per semantic role. Arbitrary colours that are not known keys remain untouched.
static std::map<wxColour, wxColour> gDarkColors{
    {ThemeColor::BrandGreen,  "#8bd89b"},/*green*/
    {ThemeColor::BrandGreenPressed, "#7ac98a"},
    {ThemeColor::BrandGreenHovered, "#9ee0ad"},
    // {"#1F8EEA", "#2778D2"},/*blue*/ -- dead, only used by disabled Notebook.cpp:80 OnPaint
    {ThemeColor::Warning,     "#ffb77c"},
    {ThemeColor::Danger,      "#ffb4ab"},/*red*/
    {ThemeColor::Link,        "#479EF5"},/*blue*/
    {ThemeColor::TextPrimary, MD3::Dark::onSurface},
    {ThemeColor::TextSecondary, MD3::Dark::onSurfaceVariant},
    {ThemeColor::TextMuted,     "#a8a9b3"},
    // Disabled text retains the existing explicit tone; normal supporting text
    // uses the semantic OnSurfaceVariant role.
    // The previous #6a6b73 sat at ~1.7:1 on SurfaceContainerHigh — unreadable
    // disabled labels on the dark Slice/Print pills and input fields.
    {ThemeColor::TextDisabled,  "#8a8b94"},
    {ThemeColor::White,       MD3::Dark::scLowest},
    {ThemeColor::Grey200,     MD3::Dark::scLow},
    {ThemeColor::Grey250,     MD3::Dark::sc},
    {ThemeColor::Grey300,     MD3::Dark::scHigh},/*gray -> */
    {ThemeColor::Grey350,     MD3::Dark::scHighest},
    {ThemeColor::Grey400,     MD3::Dark::outlineVariant},
    {ThemeColor::Grey450,     MD3::Dark::outline},
    {"#2C2C2E", MD3::Dark::onSurface},/*black*/
    {"#E5E7EB", MD3::Dark::scHighest},/*gray200 -> gray800*/
    {"#6B6B6B", "#a8a9b3"},/*gray -> */
    {"#ACACAC", MD3::Dark::outline},/*gray -> */
    {"#3B4446", MD3::Dark::scHigh},
    {"#CECECE", MD3::Dark::outlineVariant},
    {"#DBFDD5", "#095228"},
    {"#000000", MD3::Dark::onSurface},
    {"#F4F4F4", MD3::Dark::scLow},
    {"#F7F7F7", MD3::Dark::scLow},
    {"#DBDBDB", MD3::Dark::outlineVariant},
    {ThemeColor::LightGreen,  "#095228"},
    {"#EDFAF2", "#095228"},
    {"#323A3C", MD3::Dark::onSurface},
    {"#6B6B6A", "#a8a9b3"},
    {"#303A3C", MD3::Dark::onSurface},
    {"#FEFFFF", MD3::Dark::surface},
    {"#363636", MD3::Dark::onSurface},
    {"#F0F0F1", MD3::Dark::sc},
    {"#9E9E9E", MD3::Dark::outline},
    {"#D7E8DE", "#2b3a2f"},
    {"#2B3436", MD3::Dark::onSurfaceVariant},
    {"#ABABAB", MD3::Dark::outline},
    {"#D9D9D9", MD3::Dark::scHighest},
    {"#EBF9F0", "#095228"},
    {"#DBFDE7", "#095228"},
    // MD3 neutral surface roles. Construction-time semantic() snapshots of the
    // light surfaces (MainFrame's notebook plate, the Monitor/Project/Calibration
    // page backgrounds, HMSPanel, SideTools) are taken once and never re-resolved,
    // so without these pairs a runtime theme switch leaves a near-white plate on an
    // otherwise dark shell until restart.
    // SurfaceBright has a distinct light key, preserving its brighter role
    // through the same compatibility route instead of collapsing into Surface.
    // ErrorContainer is deliberately NOT paired: Dark::onErrorContainer aliases
    // Light::errorContainer (#ffdad6). Mapping errorContainer alone would recolour
    // the plate to #93000a while its #410002 text stayed put (~1.3:1, unreadable),
    // and adding the reciprocal onErrorContainer pair to fix that would put #ffdad6
    // on both sides of the table — the idempotency violation described above. It
    // requires a separate change to the error palette before either pair is safe.
    {MD3::Light::surface,       MD3::Dark::surface},
    {MD3::Light::surfaceDim,    MD3::Dark::surfaceDim},
    {MD3::Light::surfaceBright, MD3::Dark::surfaceBright},
    // Legacy raw colour snapshots retain dark-mode compatibility. Named aliases
    // above now resolve the current defaults in both themes. No saved style is
    // rewritten, and values outside this known compatibility set pass through.
    {"#faf8fd", MD3::Dark::surface},
    {"#dad9e0", MD3::Dark::surfaceDim},
    {"#1a1b1f", MD3::Dark::onSurface},
    {"#44464e", MD3::Dark::onSurfaceVariant},
    {"#f4f2f9", MD3::Dark::scLow},
    {"#eeedf3", MD3::Dark::sc},
    {"#e8e7ee", MD3::Dark::scHigh},
    {"#e2e1e9", MD3::Dark::scHighest},
    {"#c5c6d0", MD3::Dark::outlineVariant},
    {"#75777f", MD3::Dark::outline},
    // MD3 brand container-green tokens. Construction-time semantic() snapshots of
    // the tonal greens capture the light value; these pairs live-remap them when
    // the app toggles to dark mode (mirrors the resolve() dark tones exactly).
    {MD3::Light::primaryContainer,     MD3::Dark::primaryContainer},     /*#a6f4b8 -> #095228*/
    {MD3::Light::secondaryContainer,   MD3::Dark::secondaryContainer},   /*#d7e8d9 -> #2b3a2f*/
    {MD3::Light::onPrimaryContainer,   MD3::Dark::onPrimaryContainer},   /*#00210c -> #a7f5b9*/
    {MD3::Light::onSecondaryContainer, MD3::Dark::onSecondaryContainer}, /*#0e1f13 -> #cfe9d3*/
    // Device-scheme teal accent tokens. Construction-time
    // semantic(role, ColorScheme::Device) snapshots (AMS Load/Unload buttons,
    // ConnectPrinter, UpgradePanel, StatusPanel, ...) capture the light tones;
    // these light->dark pairs live-remap them when the app toggles to dark mode,
    // mirroring the 3-arg resolve() Device dark tones exactly. OnPrimary is
    // intentionally omitted (its light tone is #ffffff == ThemeColor::White,
    // already mapped above), as is the SecondaryContainer pair
    // (Device::secondaryContainerLight shares #cce8e3 with
    // Device::onSecondaryContainerDark, so mapping it would corrupt snapshots
    // taken while dark mode is active).
    {MD3::Device::primaryLight,            MD3::Device::primaryDark},            /*#0f766e -> #5eead4*/
    {MD3::Device::primaryContainerLight,   MD3::Device::primaryContainerDark},   /*#9cf2e7 -> #005047*/
    {MD3::Device::onPrimaryContainerLight, MD3::Device::onPrimaryContainerDark}  /*#00201d -> #83f5e3*/
    //{"#F0F0F0", "#4C4C54"},
};

void StateColor::SetDarkMode(bool dark) { gDarkMode = dark; }

bool StateColor::isDarkMode() { return gDarkMode; }

wxColour StateColor::semantic(MD3::Role role) { return MD3::resolve(role, gDarkMode); }

wxColour StateColor::semantic(MD3::Role role, MD3::ColorScheme scheme) { return MD3::resolve(role, gDarkMode, scheme); }

wxColour StateColor::scrim() { return MD3::scrim(gDarkMode); }

wxColour StateColor::shadowTint() { return MD3::shadowTint(gDarkMode); }

inline wxColour darkModeColorFor2(wxColour const &color)
{
    if (!gDarkMode)
        return color;
    auto iter = gDarkColors.find(color);
    if (iter != gDarkColors.end()) return iter->second;
    return color;
}

std::map<wxColour, wxColour> revert(std::map<wxColour, wxColour> const & map)
{
    std::map<wxColour, wxColour> map2;
    for (auto &p : map) map2.emplace(p.second, p.first);
    return map2;
}

wxColour StateColor::lightModeColorFor(wxColour const &color)
{
    static std::map<wxColour, wxColour> gLightColors = [] {
        auto result = revert(gDarkColors);
        // Several historical light values share one dark neutral. Prefer the
        // current role on return to light mode, not the first numeric map key.
        // Accent, error and arbitrary/custom colour handling stays unchanged.
        for (const auto role : {MD3::Role::Surface, MD3::Role::SurfaceDim, MD3::Role::SurfaceBright,
                MD3::Role::SurfaceContainerLowest, MD3::Role::SurfaceContainerLow,
                MD3::Role::SurfaceContainer, MD3::Role::SurfaceContainerHigh,
                MD3::Role::SurfaceContainerHighest, MD3::Role::OnSurface,
                MD3::Role::OnSurfaceVariant, MD3::Role::Outline, MD3::Role::OutlineVariant})
            result[MD3::resolve(role, true)] = MD3::resolve(role, false);
        return result;
    }();
    auto iter = gLightColors.find(color);
    if (iter != gLightColors.end()) return iter->second;
    return color;
}

wxColour StateColor::darkModeColorFor(wxColour const &color) { return darkModeColorFor2(color); }

StateColor::StateColor(wxColour const &color) { append(color, 0); }

StateColor::StateColor(wxString const &color) { append(color, 0); }

StateColor::StateColor(unsigned long color) { append(color, 0); }

void StateColor::append(wxColour const & color, int states)
{
    statesList_.push_back(states);
    colors_.push_back(color);
}

void StateColor::append(wxString const & color, int states)
{
    wxColour c1(color);
    append(c1, states);
}

void StateColor::append(unsigned long color, int states)
{
    if ((color & 0xff000000) == 0)
        color |= 0xff000000;
    wxColour cl; cl.SetRGBA(color & 0xff00ff00 | ((color & 0xff) << 16) | ((color >> 16) & 0xff));
    append(cl, states);
}

void StateColor::clear()
{
    statesList_.clear();
    colors_.clear();
}

int StateColor::states() const
{
    int states = 0;
    for (auto s : statesList_) states |= s;
    states = (states & 0xffff) | (states >> 16);
    if (takeFocusedAsHovered_ && (states & Hovered))
        states |= Focused;
    return states;
}

wxColour StateColor::defaultColor() {
    return colorForStates(0);
}

wxColour StateColor::colorForStates(int states) const
{
    bool focused = takeFocusedAsHovered_ && (states & Focused);
    for (int i = 0; i < statesList_.size(); ++i) {
        int s = statesList_[i];
        int on = s & 0xffff;
        int off = s >> 16;
        if ((on & states) == on && (off & ~states) == off) {
            return darkModeColorFor2(colors_[i]);
        }
        if (focused && (on & Hovered)) {
            on |= Focused;
            on &= ~Hovered;
            if ((on & states) == on && (off & ~states) == off) {
                return darkModeColorFor2(colors_[i]);
            }
        }
    }
    return wxColour(0, 0, 0, 0);
}

wxColour StateColor::colorForStatesNoDark(int states)
{
    bool focused = takeFocusedAsHovered_ && (states & Focused);
    for (int i = 0; i < statesList_.size(); ++i) {
        int s = statesList_[i];
        int on = s & 0xffff;
        int off = s >> 16;
        if ((on & states) == on && (off & ~states) == off) {
            return colors_[i];
        }
        if (focused && (on & Hovered)) {
            on |= Focused;
            on &= ~Hovered;
            if ((on & states) == on && (off & ~states) == off) {
                return colors_[i];
            }
        }
    }
    return wxColour(0, 0, 0, 0);
}

int StateColor::colorIndexForStates(int states)
{
    for (int i = 0; i < statesList_.size(); ++i) {
        int s   = statesList_[i];
        int on  = s & 0xffff;
        int off = s >> 16;
        if ((on & states) == on && (off & ~states) == off) { return i; }
    }
    return -1;
}

bool StateColor::setColorForStates(wxColour const &color, int states)
{
    for (int i = 0; i < statesList_.size(); ++i) {
        if (statesList_[i] == states) {
            colors_[i] = color;
            return true;
        }
    }
    return false;
}

void StateColor::setTakeFocusedAsHovered(bool set) { takeFocusedAsHovered_ = set; }

StateColor StateColor::createButtonStyleGray()
{
    return StateColor(std::pair<wxColour, int>(ThemeColor::Grey300, StateColor::Pressed),
        std::pair<wxColour, int>(ThemeColor::Grey200, StateColor::Focused),
        std::pair<wxColour, int>(ThemeColor::Grey200, StateColor::Hovered),
        std::pair<wxColour, int>(ThemeColor::White, StateColor::Normal));
}
