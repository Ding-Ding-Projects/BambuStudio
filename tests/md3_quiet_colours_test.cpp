// Standalone assertions for the quieter colours of Low stimulation in
// src/slic3r/GUI/Widgets/MD3Tokens.hpp, compiled against a stand-in
// <wx/colour.h> (tests/stubs): off, every role is its usual tone; on, the
// accent roles of every scheme keep their lightness and lose most of their
// colour, a chosen accent included, while surfaces, text and every error role
// stay exactly as they were. Built and run by tests/md3_quiet_colours.test.mjs,
// which also builds it with SLIC3R_MD3_QUIET_NEGATIVE_FIXTURE: in that build
// the error colour is quieted too, and the checks below must fail.
#include "src/slic3r/GUI/Widgets/MD3Tokens.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

static int g_failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                           \
        }                                                                           \
    } while (0)

static double luminance(const wxColour &c) { return 0.2126 * c.Red() + 0.7152 * c.Green() + 0.0722 * c.Blue(); }
static int    spread(const wxColour &c) { return std::max({c.Red(), c.Green(), c.Blue()}) - std::min({c.Red(), c.Green(), c.Blue()}); }

int main()
{
    using MD3::Role;
    const Role accents[] = {Role::Primary, Role::OnPrimary, Role::PrimaryContainer, Role::OnPrimaryContainer,
                            Role::SecondaryContainer, Role::OnSecondaryContainer, Role::InversePrimary};
    const Role kept[] = {Role::Surface, Role::SurfaceContainer, Role::OnSurface, Role::OnSurfaceVariant, Role::Outline,
                         Role::Error, Role::ErrorContainer, Role::OnError, Role::OnErrorContainer};
    const MD3::ColorScheme schemes[] = {MD3::ColorScheme::Brand, MD3::ColorScheme::Preview, MD3::ColorScheme::Device};

    MD3::setQuietColours(false);
    CHECK(!MD3::quietColours());
    for (bool dark : {false, true})
        for (Role r : accents) CHECK(MD3::resolve(r, dark) == MD3::resolve_tone(r, dark));

    MD3::setQuietColours(true);
    CHECK(MD3::quietColours());
    for (MD3::ColorScheme scheme : schemes)
        for (bool dark : {false, true}) {
            for (Role r : accents) {
                const wxColour usual = MD3::resolve_scheme_tone(r, dark, scheme);
                const wxColour quiet = MD3::resolve(r, dark, scheme);
                CHECK(std::fabs(luminance(quiet) - luminance(usual)) <= 1.5); // as readable as before
                CHECK(spread(quiet) <= spread(usual));                        // less colourful
                if (spread(usual) > 40) CHECK(spread(quiet) < spread(usual) / 2);
            }
            for (Role r : kept) CHECK(MD3::resolve(r, dark, scheme) == MD3::resolve_scheme_tone(r, dark, scheme));
        }
    // A chosen accent is quieted too, and comes back whole when Low stimulation ends.
    MD3::setAccentSeed(wxColour("#d81b60"));
    const wxColour pink_quiet = MD3::resolve(Role::Primary, false);
    CHECK(spread(pink_quiet) < spread(MD3::resolve_tone(Role::Primary, false)) / 2);
    MD3::setQuietColours(false);
    CHECK(MD3::resolve(Role::Primary, false) == MD3::resolve_tone(Role::Primary, false));
    MD3::clearAccentSeed();

    if (g_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("md3 quiet colours: all checks passed\n");
    return 0;
}
