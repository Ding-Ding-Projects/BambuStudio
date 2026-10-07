#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include <algorithm>
#include <cmath>

// Minimal RGB value adapter for compiling the exact extracted production paint
// calculation without starting wxWidgets. This is not a native widget test.
struct wxColour {
    unsigned char r, g, b, a;
    wxColour(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}
    unsigned char Red() const { return r; }
    unsigned char Green() const { return g; }
    unsigned char Blue() const { return b; }
    unsigned char Alpha() const { return a; }
};

// Produced verbatim from Button.cpp by atlas_control_anatomy.test.mjs --extract.
#include "atlas_button_state_functions.inc"

TEST_CASE("Default semantic pairs retain readable text through hover and press")
{
    const wxColour pairs[][2] = {
        {{0x14,0x6c,0x2e}, {255,255,255}}, {{0x8b,0xd8,0x9b}, {0,0x39,0x1a}},
        {{0xd7,0xe8,0xd9}, {0x0e,0x1f,0x13}}, {{0x2b,0x3a,0x2f}, {0xcf,0xe9,0xd3}},
        {{0x70,0x50,0xe8}, {255,255,255}}, {{0xad,0x98,0xff}, {0x2b,0,0x6d}},
        {{0xe5,0xdf,0xf3}, {0x21,0x1a,0x2d}}, {{0x49,0x42,0x53}, {0xe8,0xdf,0xf5}},
        {{0x0f,0x76,0x6e}, {255,255,255}}, {{0x5e,0xea,0xd4}, {0,0x37,0x31}},
        {{0xcc,0xe8,0xe3}, {8,0x20,0x1d}}, {{0x30,0x4a,0x46}, {0xcc,0xe8,0xe3}},
        {{0xba,0x1a,0x1a}, {255,255,255}}, {{0xff,0xb4,0xab}, {0x69,0,5}}
    };
    for (const auto &pair : pairs)
        for (double opacity : {0.08, 0.12}) {
            const auto result = buttonStateLayer(pair[0], pair[1], opacity);
            REQUIRE(contrastRatio(result, pair[1]) >= 4.5 - 1e-9);
        }
}

TEST_CASE("Custom pairs never lose existing low contrast or drop below the readable threshold")
{
    for (int r = 0; r <= 255; r += 51)
        for (int g = 0; g <= 255; g += 51)
            for (int b = 0; b <= 255; b += 51)
                for (int fg = 0; fg <= 255; fg += 51)
                    for (double opacity : {0.08, 0.12}) {
                        const wxColour base(static_cast<unsigned char>(r), static_cast<unsigned char>(g), static_cast<unsigned char>(b), 173);
                        const wxColour text(static_cast<unsigned char>(fg), static_cast<unsigned char>(255-fg), static_cast<unsigned char>(fg));
                        const auto result = buttonStateLayer(base, text, opacity);
                        REQUIRE(result.Alpha() == 173);
                        REQUIRE(contrastRatio(result, text) + 1e-9 >= std::min(4.5, contrastRatio(base, text)));
                    }
}

TEST_CASE("Pressed treatment differs from hover where the contrast budget permits")
{
    const wxColour surface(0xd7, 0xe8, 0xd9), text(0x0e, 0x1f, 0x13);
    const auto hover = buttonStateLayer(surface, text, 0.08);
    const auto press = buttonStateLayer(surface, text, 0.12);
    REQUIRE(press.Red() < hover.Red());
    REQUIRE(press.Green() < hover.Green());
    REQUIRE(press.Blue() < hover.Blue());
}
