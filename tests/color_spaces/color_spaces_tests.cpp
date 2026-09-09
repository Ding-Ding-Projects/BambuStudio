#include <catch_main.hpp>

#include "slic3r/GUI/Widgets/ColorSpaces.hpp"

#include <cmath>
#include <string>

using namespace MD3::Color;

namespace {

struct Ref { const char *label; Rgb rgb; };

// White, black, mid grey and the three sRGB primaries: every one must survive
// each round trip within 8-bit rounding.
const Ref kRefs[] = {
    { "white",    { 1.0, 1.0, 1.0 } },
    { "black",    { 0.0, 0.0, 0.0 } },
    { "mid grey", { 128 / 255.0, 128 / 255.0, 128 / 255.0 } },
    { "red",      { 1.0, 0.0, 0.0 } },
    { "green",    { 0.0, 1.0, 0.0 } },
    { "blue",     { 0.0, 0.0, 1.0 } },
    { "material green", { 20 / 255.0, 108 / 255.0, 46 / 255.0 } },
};

bool same_byte(const Rgb &a, const Rgb &b)
{
    return to_byte(a.r) == to_byte(b.r) && to_byte(a.g) == to_byte(b.g) && to_byte(a.b) == to_byte(b.b);
}

bool close(const Rgb &a, const Rgb &b, double tol)
{
    return std::fabs(a.r - b.r) < tol && std::fabs(a.g - b.g) < tol && std::fabs(a.b - b.b) < tol;
}

} // namespace

TEST_CASE("sRGB transfer function is its own inverse", "[ColorSpaces]")
{
    for (double v = 0.0; v <= 1.0; v += 0.05)
        REQUIRE(std::fabs(linear_to_srgb(srgb_to_linear(v)) - v) < 1e-9);
}

TEST_CASE("Reference colours round-trip through every space", "[ColorSpaces]")
{
    for (const Ref &ref : kRefs) {
        INFO(ref.label);
        REQUIRE(close(hsv_to_rgb(rgb_to_hsv(ref.rgb)), ref.rgb, 1e-9));
        REQUIRE(close(hsl_to_rgb(rgb_to_hsl(ref.rgb)), ref.rgb, 1e-9));
        REQUIRE(close(hwb_to_rgb(rgb_to_hwb(ref.rgb)), ref.rgb, 1e-9));
        REQUIRE(close(cmyk_to_rgb(rgb_to_cmyk(ref.rgb)), ref.rgb, 1e-9));

        const Clipped xyz = xyz_to_rgb(rgb_to_xyz(ref.rgb));
        REQUIRE_FALSE(xyz.clipped);
        REQUIRE(close(xyz.rgb, ref.rgb, 5e-5));
        REQUIRE(same_byte(xyz.rgb, ref.rgb));

        const Clipped lab = lab_to_rgb(rgb_to_lab(ref.rgb));
        REQUIRE_FALSE(lab.clipped);
        REQUIRE(close(lab.rgb, ref.rgb, 5e-5));
        REQUIRE(same_byte(lab.rgb, ref.rgb));

        const Clipped lch = lch_to_rgb(rgb_to_lch(ref.rgb));
        REQUIRE_FALSE(lch.clipped);
        REQUIRE(close(lch.rgb, ref.rgb, 5e-5));
        REQUIRE(same_byte(lch.rgb, ref.rgb));

        const Clipped ok = oklab_to_rgb(rgb_to_oklab(ref.rgb));
        REQUIRE_FALSE(ok.clipped);
        REQUIRE(close(ok.rgb, ref.rgb, 1e-5));

        const Clipped okc = oklch_to_rgb(rgb_to_oklch(ref.rgb));
        REQUIRE_FALSE(okc.clipped);
        REQUIRE(close(okc.rgb, ref.rgb, 1e-5));
        REQUIRE(same_byte(okc.rgb, ref.rgb));
    }
}

TEST_CASE("Known landmark values", "[ColorSpaces]")
{
    // sRGB white is D65 white in XYZ and L=100 in both Lab and OKLab.
    const Xyz w = rgb_to_xyz({ 1, 1, 1 });
    REQUIRE(std::fabs(w.x - kWhiteX) < 1e-6);
    REQUIRE(std::fabs(w.y - 1.0) < 1e-6);
    REQUIRE(std::fabs(w.z - kWhiteZ) < 1e-6);
    REQUIRE(std::fabs(rgb_to_lab({ 1, 1, 1 }).l - 100.0) < 1e-4);
    REQUIRE(std::fabs(rgb_to_oklab({ 1, 1, 1 }).l - 1.0) < 1e-4);
    REQUIRE(std::fabs(rgb_to_oklab({ 1, 1, 1 }).a) < 1e-4);
    REQUIRE(std::fabs(rgb_to_oklab({ 1, 1, 1 }).b) < 1e-4);

    // Pure red: hue 0 in HSL/HSV/HWB, hue ~29 in OKLCH, ~40 in LCH.
    REQUIRE(rgb_to_hsv({ 1, 0, 0 }).h == 0.0);
    REQUIRE(rgb_to_hsl({ 1, 0, 0 }).s == 1.0);
    REQUIRE(std::fabs(rgb_to_oklch({ 1, 0, 0 }).h - 29.23) < 0.1);
    REQUIRE(std::fabs(rgb_to_lch({ 1, 0, 0 }).h - 40.0) < 0.5);

    const Cmyk k = rgb_to_cmyk({ 0, 0, 0 });
    REQUIRE(k.k == 1.0);
    REQUIRE(k.c == 0.0);
}

TEST_CASE("Out-of-gamut OKLCH is flagged clipped", "[ColorSpaces]")
{
    // A vivid P3-ish green: well outside sRGB.
    const Clipped c = oklch_to_rgb({ 0.85, 0.30, 145.0 });
    REQUIRE(c.clipped);
    REQUIRE(c.rgb.r >= 0.0);
    REQUIRE(c.rgb.g <= 1.0);
    // The clamped colour is still a valid sRGB triple.
    REQUIRE(c.rgb.g > 0.9);

    // An in-gamut OKLCH is not flagged.
    REQUIRE_FALSE(oklch_to_rgb({ 0.7, 0.1, 200.0 }).clipped);
}

TEST_CASE("Named colours look up both ways", "[ColorSpaces]")
{
    REQUIRE(name_of({ 1, 1, 1 }).value() == "white");
    REQUIRE(name_of({ 0, 0, 0 }).value() == "black");
    REQUIRE(name_of({ 102 / 255.0, 51 / 255.0, 153 / 255.0 }).value() == "rebeccapurple");
    REQUIRE_FALSE(name_of({ 20 / 255.0, 108 / 255.0, 46 / 255.0 }).has_value());

    const Rgb navy = from_name("Navy").value();
    REQUIRE(to_byte(navy.r) == 0);
    REQUIRE(to_byte(navy.b) == 128);
    REQUIRE_FALSE(from_name("notacolour").has_value());
}

TEST_CASE("Contrast ratio", "[ColorSpaces]")
{
    REQUIRE(std::fabs(contrast_ratio({ 0, 0, 0 }, { 1, 1, 1 }) - 21.0) < 1e-9);
    REQUIRE(std::fabs(contrast_ratio({ 1, 1, 1 }, { 0, 0, 0 }) - 21.0) < 1e-9);
    REQUIRE(std::fabs(contrast_ratio({ 1, 1, 1 }, { 1, 1, 1 }) - 1.0) < 1e-9);
    // Alpha composites toward the background, lowering contrast.
    const Rgb half = composite_over({ 0, 0, 0, 0.5 }, { 1, 1, 1 });
    REQUIRE(contrast_ratio(half, { 1, 1, 1 }) < 21.0);
    REQUIRE(contrast_ratio(half, { 1, 1, 1 }) > 1.0);
}

TEST_CASE("Formatting", "[ColorSpaces]")
{
    REQUIRE(format_hex({ 1, 0, 0 }) == "#FF0000");
    REQUIRE(format_hex8({ 1, 0, 0, 0.5 }) == "#FF000080");
    REQUIRE(format_rgb({ 1, 0, 0 }) == "rgb(255, 0, 0)");
    REQUIRE(format_rgba({ 1, 0, 0, 0.5 }) == "rgba(255, 0, 0, 0.5)");
    REQUIRE(format_hsl(rgb_to_hsl({ 1, 0, 0 })) == "hsl(0, 100%, 50%)");
    REQUIRE(format_hsv(rgb_to_hsv({ 0, 1, 0 })) == "hsv(120, 100%, 100%)");
    REQUIRE(format_hwb(rgb_to_hwb({ 0, 0, 1 })) == "hwb(240 0% 0%)");
    REQUIRE(format_cmyk(rgb_to_cmyk({ 0, 1, 1 })) == "cmyk(100%, 0%, 0%, 0%)");
    REQUIRE(format_oklch(rgb_to_oklch({ 1, 1, 1 })).rfind("oklch(1 0 ", 0) == 0);

    const auto all = translate_all({ 1, 1, 1, 1 });
    REQUIRE(all.size() == 15);
    REQUIRE(std::string(all.front().space) == "Name");
    REQUIRE(all.front().text == "white");
    REQUIRE(translate_all({ 20 / 255.0, 108 / 255.0, 46 / 255.0, 1 }).size() == 14);
}

TEST_CASE("Parsing every notation", "[ColorSpaces]")
{
    auto bytes = [](const Parsed &p) {
        return std::to_string(to_byte(p.rgba.r)) + "," + std::to_string(to_byte(p.rgba.g)) + "," +
               std::to_string(to_byte(p.rgba.b)) + "," + std::to_string(to_byte(p.rgba.a));
    };

    REQUIRE(bytes(parse("#ff0000").value()) == "255,0,0,255");
    REQUIRE(bytes(parse("#F00").value()) == "255,0,0,255");
    REQUIRE(bytes(parse("#ff000080").value()) == "255,0,0,128");
    REQUIRE(bytes(parse("ff0000").value()) == "255,0,0,255");
    REQUIRE(parse("#ff0000").value().space == "hex");

    REQUIRE(bytes(parse("rgb(255, 0, 0)").value()) == "255,0,0,255");
    REQUIRE(bytes(parse("rgb(100% 0% 0% / 50%)").value()) == "255,0,0,128");
    REQUIRE(bytes(parse("rgba(0,0,255,0.5)").value()) == "0,0,255,128");

    REQUIRE(bytes(parse("hsl(120, 100%, 50%)").value()) == "0,255,0,255");
    REQUIRE(bytes(parse("hsla(120 100% 50% / 0.25)").value()) == "0,255,0,64");
    REQUIRE(bytes(parse("hsv(240, 100%, 100%)").value()) == "0,0,255,255");
    REQUIRE(bytes(parse("hsb(240 100 100)").value()) == "0,0,255,255");
    REQUIRE(bytes(parse("hwb(0 0% 0%)").value()) == "255,0,0,255");
    REQUIRE(bytes(parse("hwb(0 50% 50%)").value()) == "128,128,128,255");

    REQUIRE(bytes(parse("lab(100 0 0)").value()) == "255,255,255,255");
    REQUIRE(bytes(parse("lch(0 0 0)").value()) == "0,0,0,255");
    REQUIRE(bytes(parse("oklab(1 0 0)").value()) == "255,255,255,255");
    REQUIRE(bytes(parse("oklch(0.5 0 0)").value()) == "99,99,99,255");
    REQUIRE(bytes(parse("xyz-d65(0.95047 1 1.08883)").value()) == "255,255,255,255");

    const Parsed okl = parse("oklch(70% 0.1 200)").value();
    REQUIRE(okl.space == "oklch");
    REQUIRE_FALSE(okl.clipped);
    // Same colour written back reproduces the same bytes.
    const Rgb back = oklch_to_rgb({ 0.7, 0.1, 200.0 }).rgb;
    REQUIRE(same_byte({ okl.rgba.r, okl.rgba.g, okl.rgba.b }, back));

    REQUIRE(bytes(parse("cmyk(0%, 100%, 100%, 0%)").value()) == "255,0,0,255");
    REQUIRE(bytes(parse("cmyk(0 1 1 0)").value()) == "255,0,0,255");
    REQUIRE(bytes(parse("device-cmyk(0 0 0 100%)").value()) == "0,0,0,255");

    REQUIRE(bytes(parse("Rebeccapurple").value()) == "102,51,153,255");
    REQUIRE(parse("white").value().space == "name");

    // Out-of-gamut input parses, but is flagged.
    const Parsed wide = parse("oklch(0.85 0.3 145)").value();
    REQUIRE(wide.clipped);

    // Garbage is refused, not guessed.
    REQUIRE_FALSE(parse("").has_value());
    REQUIRE_FALSE(parse("#12345").has_value());
    REQUIRE_FALSE(parse("rgb(1,2)").has_value());
    REQUIRE_FALSE(parse("rgb(a,b,c)").has_value());
    REQUIRE_FALSE(parse("plaid(1 2 3)").has_value());
    REQUIRE_FALSE(parse("oklch(0.5 0.1").has_value());
}
