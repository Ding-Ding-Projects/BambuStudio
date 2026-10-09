#include "LogoRender.hpp"

#include <wx/icon.h>
#include <wx/image.h>

#include <algorithm>
#include <cmath>

namespace Slic3r { namespace GUI { namespace AppLogoUI {

wxBitmap bitmap(const AppLogo::Settings& settings, unsigned pixels)
{
    if (!pixels) return wxBitmap();
    const auto rendered = AppLogo::render_selected(settings, std::min(pixels, AppLogo::max_output_px));
    if (!rendered.valid()) return wxBitmap();
    wxImage image(int(rendered.width), int(rendered.height), false);
    image.InitAlpha();
    auto* rgb = image.GetData();
    auto* alpha = image.GetAlpha();
    for (size_t i = 0; i < rendered.pixels.size(); ++i) {
        const auto& p = rendered.pixels[i];
        rgb[i * 3] = p.r; rgb[i * 3 + 1] = p.g; rgb[i * 3 + 2] = p.b; alpha[i] = p.a;
    }
    // Past the generation bound the bounded output is resampled up rather than
    // generated larger, so a high-DPI surface never lifts the limit.
    if (rendered.width < pixels)
        image.Rescale(int(pixels), int(pixels), wxIMAGE_QUALITY_BICUBIC);
    return wxBitmap(image);
}

wxBitmap scaled_bitmap(const AppLogo::Settings& settings, unsigned logical, double scale)
{
    const AppLogo::RenderPlan plan = AppLogo::plan(logical, scale);
    wxBitmap result = bitmap(settings, plan.draw_px);
#if wxCHECK_VERSION(3, 1, 6)
    if (result.IsOk() && logical)
        result.SetScaleFactor(double(plan.draw_px) / double(logical));
#endif
    return result;
}

wxIconBundle icon_bundle(const AppLogo::Settings& settings)
{
    wxIconBundle bundle;
    for (const unsigned size : AppLogo::window_icon_sizes) {
        const wxBitmap rendered = bitmap(settings, size);
        if (!rendered.IsOk()) return wxIconBundle();
        wxIcon icon;
        icon.CopyFromBitmap(rendered);
        if (!icon.IsOk()) return wxIconBundle();
        bundle.AddIcon(icon);
    }
    return bundle;
}

wxBitmap overlay(const wxBitmap& base, const AppLogo::Settings& settings, int left, int top, unsigned side)
{
    if (!base.IsOk() || !side) return base;
    const wxBitmap chosen = bitmap(settings, side);
    if (!chosen.IsOk()) return base;
    wxImage target = base.ConvertToImage();
    const wxImage logo = chosen.ConvertToImage();
    if (!target.IsOk() || !logo.IsOk() || logo.GetWidth() != int(side) || logo.GetHeight() != int(side)) return base;
    const bool target_alpha = target.HasAlpha();
    const bool logo_alpha = logo.HasAlpha();
    const double extent = double(side);
    const double radius = std::max(2.0, extent / 6.0);
    // Coverage of the rounded tile at a pixel centre, antialiased over one pixel.
    auto tile_cover = [&](double x, double y) {
        const double dx = std::max({radius - x, 0.0, x - (extent - radius)});
        const double dy = std::max({radius - y, 0.0, y - (extent - radius)});
        return std::clamp(radius + 0.5 - std::sqrt(dx * dx + dy * dy), 0.0, 1.0);
    };
    // Source-over in straight (non-premultiplied) alpha.
    auto over = [](double (&dst)[4], const double (&src)[4]) {
        const double a = src[3] + dst[3] * (1.0 - src[3]);
        for (int c = 0; c < 3; ++c)
            dst[c] = a > 0.0 ? (src[c] * src[3] + dst[c] * dst[3] * (1.0 - src[3])) / a : 0.0;
        dst[3] = a;
    };
    for (int y = 0; y < int(side); ++y) {
        const int ty = top + y;
        if (ty < 0 || ty >= target.GetHeight()) continue;
        for (int x = 0; x < int(side); ++x) {
            const int tx = left + x;
            if (tx < 0 || tx >= target.GetWidth()) continue;
            double pixel[4] = {double(target.GetRed(tx, ty)), double(target.GetGreen(tx, ty)), double(target.GetBlue(tx, ty)),
                               target_alpha ? target.GetAlpha(tx, ty) / 255.0 : 1.0};
            if (settings.transparent) {
                const double white[4] = {255.0, 255.0, 255.0, tile_cover(x + 0.5, y + 0.5)};
                over(pixel, white);
            }
            const double mark[4] = {double(logo.GetRed(x, y)), double(logo.GetGreen(x, y)), double(logo.GetBlue(x, y)),
                                    logo_alpha ? logo.GetAlpha(x, y) / 255.0 : 1.0};
            over(pixel, mark);
            target.SetRGB(tx, ty, (unsigned char) std::lround(pixel[0]), (unsigned char) std::lround(pixel[1]),
                          (unsigned char) std::lround(pixel[2]));
            if (target_alpha) target.SetAlpha(tx, ty, (unsigned char) std::lround(pixel[3] * 255.0));
        }
    }
    return wxBitmap(target);
}

} } }
