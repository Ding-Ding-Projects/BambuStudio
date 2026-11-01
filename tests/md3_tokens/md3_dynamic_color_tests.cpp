#include <catch_main.hpp>

#include "slic3r/GUI/Widgets/MD3DynamicColor.hpp"

#include <cmath>
#include <cstdio>
#include <string>

namespace {
#include "md3_dynamic_color_reference.inc"

std::string hex(MD3::Dynamic::Argb argb)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "#%06x", unsigned(argb & 0xFFFFFFu));
    return buf;
}
} // namespace

using namespace MD3::Dynamic;

TEST_CASE("Role names follow the reference library order", "[MD3DynamicColor]")
{
    REQUIRE(sizeof(kRefRoleNames) / sizeof(kRefRoleNames[0]) == kSchemeRoleCount);
    for (std::size_t i = 0; i < kSchemeRoleCount; ++i)
        CHECK(std::string(role_name(static_cast<SchemeRole>(i))) == kRefRoleNames[i]);
}

TEST_CASE("HCT of a seed matches the reference", "[MD3DynamicColor]")
{
    for (const RefHct &ref : kRefHct) {
        const Hct h = hct_from_argb(ref.seed);
        INFO("seed " << hex(ref.seed));
        // Achromatic colours have an arbitrary hue; only compare it with chroma.
        if (ref.chroma > 1e-3)
            CHECK(std::fabs(h.hue - ref.hue) < 1e-4);
        CHECK(std::fabs(h.chroma - ref.chroma) < 1e-4);
        CHECK(std::fabs(h.tone - ref.tone) < 1e-4);
    }
}

TEST_CASE("HCT solver matches the reference byte for byte", "[MD3DynamicColor]")
{
    std::size_t mismatches = 0;
    for (const RefSolve &ref : kRefSolve) {
        const Argb got = argb_from_hct(ref.hue, ref.chroma, ref.tone);
        if (got != ref.argb) {
            ++mismatches;
            UNSCOPED_INFO("h=" << ref.hue << " c=" << ref.chroma << " t=" << ref.tone << " want " << hex(ref.argb)
                               << " got " << hex(got));
        }
    }
    CHECK(mismatches == 0);
}

TEST_CASE("Tonal Spot scheme matches the reference for every role", "[MD3DynamicColor]")
{
    std::size_t mismatches = 0;
    for (const RefScheme &ref : kRefSchemes) {
        const Scheme s = tonal_spot(ref.seed, ref.dark);
        for (std::size_t i = 0; i < kSchemeRoleCount; ++i) {
            if (s.colors[i] != ref.colors[i]) {
                ++mismatches;
                UNSCOPED_INFO("seed " << hex(ref.seed) << (ref.dark ? " dark " : " light ") << kRefRoleNames[i]
                                      << " want " << hex(ref.colors[i]) << " got " << hex(s.colors[i]));
            }
        }
    }
    CHECK(mismatches == 0);
}

TEST_CASE("Solved colours keep the requested tone", "[MD3DynamicColor]")
{
    for (double hue = 0; hue < 360; hue += 30)
        for (double tone = 10; tone <= 90; tone += 10) {
            const Argb c = argb_from_hct(hue, 36.0, tone);
            INFO("hue " << hue << " tone " << tone);
            CHECK(std::fabs(lstar_from_argb(c) - tone) < 0.6);
        }
}

TEST_CASE("Text roles keep WCAG contrast on their containers", "[MD3DynamicColor]")
{
    auto luminance = [](Argb c) {
        auto lin = [](int v) {
            const double s = v / 255.0;
            return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * lin(red_of(c)) + 0.7152 * lin(green_of(c)) + 0.0722 * lin(blue_of(c));
    };
    auto ratio = [&](Argb a, Argb b) {
        double la = luminance(a), lb = luminance(b);
        if (la < lb)
            std::swap(la, lb);
        return (la + 0.05) / (lb + 0.05);
    };
    const std::pair<SchemeRole, SchemeRole> pairs[] = {
        { SchemeRole::OnPrimary, SchemeRole::Primary },
        { SchemeRole::OnPrimaryContainer, SchemeRole::PrimaryContainer },
        { SchemeRole::OnSecondaryContainer, SchemeRole::SecondaryContainer },
        { SchemeRole::OnSurface, SchemeRole::Surface },
        { SchemeRole::OnSurface, SchemeRole::SurfaceContainerHighest },
        { SchemeRole::OnSurfaceVariant, SchemeRole::SurfaceContainerHighest },
        { SchemeRole::OnError, SchemeRole::Error },
        { SchemeRole::OnErrorContainer, SchemeRole::ErrorContainer },
        { SchemeRole::InverseOnSurface, SchemeRole::InverseSurface },
    };
    for (const RefScheme &ref : kRefSchemes) {
        const Scheme s = tonal_spot(ref.seed, ref.dark);
        for (const auto &p : pairs) {
            INFO("seed " << hex(ref.seed) << (ref.dark ? " dark " : " light ") << role_name(p.first) << " on "
                         << role_name(p.second));
            CHECK(ratio(s.get(p.first), s.get(p.second)) >= 4.5);
        }
    }
}
