#include "libslic3r/AppLogo/Logo.hpp"
#include <cstdlib>
#include <iostream>

using namespace Slic3r::AppLogo;
unsigned assertions = 0;
void require(bool condition, const char* message) {
    ++assertions;
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

int main()
{
    Settings settings;
    for (unsigned p = 0; p < 3; ++p) {
        require(preset(p).valid(), "preset dimensions");
        settings.preset = p;
        for (unsigned size : {16, 24, 32, 64, 256}) {
            const auto image = render(preset(p), settings, size);
            require(image.valid() && image.width == size && image.height == size, "derivative dimensions");
            bool visible = false;
            for (const auto& pixel : image.pixels) visible |= pixel.a != 0;
            require(visible, "visible preset derivative");
        }
    }
    require(!preset(3).valid(), "invalid preset");
    require(!render(preset(0), settings, 257).valid(), "output limit");
    require(!render({}, settings, 16).valid(), "invalid source");
    settings = Settings{};
    settings.inset = 0;
    Image source {4, 2, {{255,0,0,255},{255,0,0,255},{0,0,255,255},{0,0,255,255},
                        {255,0,0,255},{255,0,0,255},{0,0,255,255},{0,0,255,255}}};
    auto image = render(source, settings, 4);
    require(image.pixels[0].a == 0 && image.pixels[4].r == 255 && image.pixels[7].b == 255,
        "contain preserves complete image and letterboxes");
    settings.fit = Fit::Cover; settings.focal_x = 0;
    image = render(source, settings, 4);
    require(image.pixels.front().r == 255 && image.pixels.back().r == 255, "left focal crop");
    settings.focal_x = 100; image = render(source, settings, 4);
    require(image.pixels.front().b == 255 && image.pixels.back().b == 255, "right focal crop");
    settings.fit = Fit::Contain; settings.transparent = false; settings.background = {1,2,3,255};
    image = render(source, settings, 4);
    require(image.pixels[0].r == 1 && image.pixels[0].a == 255, "opaque background");
    source.pixels[0] = {255, 0, 0, 128}; image = render(source, settings, 4);
    require(image.pixels[4].r == 128 && image.pixels[4].a == 255, "alpha compositing");
    settings.inset = 25; image = render(source, settings, 4);
    require(image.pixels[0].r == 1 && image.pixels[0].g == 2, "safe area inset");
    Settings restored;
    require(deserialize(serialize(settings), restored) && serialize(restored) == serialize(settings), "settings round trip");
    const auto before = serialize(restored);
    for (const auto& invalid : {"", "2 0 0 50 50 8 1 255 255 255", "1 3 0 50 50 8 1 255 255 255",
        "1 0 2 50 50 8 1 255 255 255", "1 0 0 101 50 8 1 255 255 255", "1 0 0 50 50 26 1 255 255 255",
        "1 0 0 50 50 8 2 255 255 255", "1 0 0 50 50 8 1 256 255 255", "1 0 0 50 50 8 1 255 255 255 junk",
        "1 -1 0 50 50 8 1 255 255 255", "1 99999999999999999999999999 0 50 50 8 1 255 255 255"}) {
        require(!deserialize(invalid, restored), "invalid settings rejected");
        require(serialize(restored) == before, "settings rollback");
    }
    std::vector<uint8_t> bmp(70, 0);
    auto put = [&](size_t offset, uint32_t n) { for (unsigned i = 0; i < 4; ++i) bmp[offset + i] = uint8_t(n >> (8 * i)); };
    bmp[0] = 'B'; bmp[1] = 'M'; put(2, 70); put(10, 54); put(14, 40); put(18, 2); put(22, 2);
    bmp[26] = 1; bmp[28] = 24; put(34, 16); bmp[56] = 255; bmp[62] = 255;
    Image decoded; std::string error;
    require(decode_bmp(bmp, decoded, error), "bounded BMP decode");
    require(decoded.pixels[0].b == 255 && decoded.pixels[2].r == 255, "BMP rows and channels");
    const auto valid = bmp;
    for (const size_t offset : {size_t(0), size_t(2), size_t(10), size_t(14), size_t(26), size_t(28), size_t(30), size_t(34), size_t(46)}) {
        bmp = valid; bmp[offset] ^= 1;
        require(!decode_bmp(bmp, decoded, error), "signature or header corruption rejected");
        require(decoded.width == 2 && decoded.pixels[0].b == 255, "failed decode rollback");
    }
    bmp = valid; put(18, 0xffffffff);
    require(!decode_bmp(bmp, decoded, error), "oversized dimensions rejected");
    bmp = valid; bmp.pop_back();
    require(!decode_bmp(bmp, decoded, error), "truncated bitmap rejected");
    bmp.resize(4 * 1024 * 1024 + 1);
    require(!decode_bmp(bmp, decoded, error), "input size bound");
    std::cout << assertions << " assertions passed\n";
}
