#include "MaterialIcon.hpp"

#include <algorithm>
#include <cmath>
#include <mutex>

#include <wx/dcmemory.h>
#include <wx/filefn.h>
#include <wx/graphics.h>
#include <wx/window.h>

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include "libslic3r/Utils.hpp" // Slic3r::resources_dir()

#ifdef __WXMSW__
#include <windows.h>
#endif

namespace MaterialIcon {

namespace {

std::once_flag g_register_once;
bool           g_available = false;

wxString face() { return wxString::FromUTF8(MD3::Type::font_icon); }

// Register the bundled Material Symbols TTF as a private font exactly once. This
// mirrors the AddPrivateFont call in Label::initSysFont and acts as a safety net
// for the GUI_App re-init paths that call initSysFont(load_font_resource=false)
// and therefore never register icon resources. Registering the same TTF twice is
// harmless; call_once keeps it to a single attempt. Must run on the GUI thread
// (all callers are paint-time), never crashes when the file is absent.
void ensureRegistered()
{
    std::call_once(g_register_once, [] {
        const std::string &resource_path = Slic3r::resources_dir();
        const wxString      font_path     = wxString::FromUTF8(resource_path + "/fonts/MaterialSymbolsOutlined.ttf");

        if (!wxFileExists(font_path)) {
            BOOST_LOG_TRIVIAL(warning)
                << "MaterialIcon: MaterialSymbolsOutlined.ttf not found; icon glyphs unavailable";
            g_available = false;
            return;
        }

#ifdef __WXMSW__
        // Session-visible registration (no FR_PRIVATE): GDI+ cannot see private
        // faces and poisons its font-family cache when one reaches a graphics
        // context — the PageHeap-verified startup heap corruption. Mirrors the
        // SessionFontRegistrar in Label.cpp; this fallback face is removed with
        // the process (single face, registered at most once via call_once).
        const bool added = ::AddFontResourceExW(font_path.ToStdWstring().c_str(), 0, nullptr) > 0;
#else
        const bool added = wxFont::AddPrivateFont(font_path);
#endif
        BOOST_LOG_TRIVIAL(info) << boost::format("add font of MaterialSymbolsOutlined returns %1%") % added;

        // AddPrivateFont returns false when the face is already registered (e.g.
        // Label::initSysFont ran first), so capability is probed by whether the
        // face name actually resolves rather than by the add result.
        wxFont probe(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, face());
        g_available = probe.SetFaceName(face()) && probe.IsOk();
        if (!g_available)
            BOOST_LOG_TRIVIAL(warning) << "MaterialIcon: face '" << MD3::Type::font_icon
                                       << "' did not resolve after registration";
    });
}

} // namespace

bool available()
{
    ensureRegistered();
    return g_available;
}

wxString text(uint32_t cp) { return wxString(wxUniChar(cp)); }

wxFont font(int px)
{
    ensureRegistered();

    double pt = static_cast<double>(px);
#ifndef __APPLE__
    pt = pt * 4.0 / 5.0; // design px -> wx point size (matches Label::sysFont)
#endif
    if (pt < 1.0)
        pt = 1.0;
    const int initial = static_cast<int>(pt);

    wxFont f(initial, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, face());
    f.SetFaceName(face());
    f.SetFractionalPointSize(pt);
    return f;
}

wxSize measure(wxDC &, uint32_t cp, int px)
{
    // Deliberately measured through a private plain-GDI probe DC rather than
    // the caller's DC: a wxGCDC caller would otherwise hand the variable icon
    // face to GDI+ just to build the measuring font (heap corruption). The
    // extent of a fixed-size glyph face is DC-independent.
    wxBitmap   probe(1, 1);
    wxMemoryDC mdc(probe);
    mdc.SetFont(font(px));
    const wxSize sz = mdc.GetTextExtent(text(cp));
    mdc.SelectObject(wxNullBitmap);
    return sz;
}

// Plain-GDI glyph rasterizer shared by every public entry point. The icon face
// is a VARIABLE TTF (fvar/gvar) registered privately; rendering it through
// GDI+ (wxGraphicsContext / wxGCDC text) intermittently corrupts the process
// heap — the startup-crash dump bottomed out in exactly that path. GDI renders
// the variable font's default instance correctly, so the glyph is drawn
// black-on-white with GDI and converted to a colour+alpha image.
static wxImage glyph_image(uint32_t cp, int dev_px, const wxColour &colour)
{
    const wxFont fdev = font(std::max(1, dev_px));
    wxSize       dev;
    {
        wxBitmap   probe(1, 1);
        wxMemoryDC mdc(probe);
        mdc.SetFont(fdev);
        dev = mdc.GetTextExtent(text(cp));
        mdc.SelectObject(wxNullBitmap);
    }
    if (dev.x < 1) dev.x = std::max(1, dev_px);
    if (dev.y < 1) dev.y = std::max(1, dev_px);

    wxBitmap mono(dev.x, dev.y, 24);
    {
        wxMemoryDC mdc(mono);
        mdc.SetBackground(*wxWHITE_BRUSH);
        mdc.Clear();
        mdc.SetFont(fdev);
        mdc.SetTextForeground(*wxBLACK);
        mdc.SetBackgroundMode(wxTRANSPARENT);
        mdc.DrawText(text(cp), 0, 0);
        mdc.SelectObject(wxNullBitmap);
    }

    wxImage cov = mono.ConvertToImage();
    wxImage out(dev.x, dev.y);
    out.InitAlpha();
    const unsigned char r = colour.Red(), g = colour.Green(), b = colour.Blue();
    unsigned char *src = cov.GetData();
    unsigned char *dst = out.GetData();
    unsigned char *al  = out.GetAlpha();
    const size_t   n   = static_cast<size_t>(dev.x) * static_cast<size_t>(dev.y);
    for (size_t i = 0; i < n; ++i) {
        const unsigned int lum = (src[3 * i] + src[3 * i + 1] + src[3 * i + 2]) / 3;
        dst[3 * i]     = r;
        dst[3 * i + 1] = g;
        dst[3 * i + 2] = b;
        al[i]          = static_cast<unsigned char>(255u - lum);
    }
    return out;
}

void draw(wxDC &dc, uint32_t cp, int px, const wxColour &colour, const wxPoint &topLeft)
{
    // Composite a pre-rasterized glyph instead of DrawText: callers pass plain
    // DCs and wxGCDCs alike, and the variable icon face must never reach GDI+.
    dc.DrawBitmap(wxBitmap(glyph_image(cp, px, colour), 32), topLeft, true);
}

wxBitmap bitmapPx(uint32_t cp, int px, const wxColour &colour, double scale)
{
    ensureRegistered();
    const int dev_px = std::max(1, static_cast<int>(std::lround(px * (scale > 0.0 ? scale : 1.0))));
    return wxBitmap(glyph_image(cp, dev_px, colour), 32);
}

void drawCentered(wxDC &dc, uint32_t cp, int px, const wxColour &colour, const wxRect &rect)
{
    const wxSize  sz = measure(dc, cp, px);
    const wxPoint topLeft(rect.x + (rect.width - sz.x) / 2, rect.y + (rect.height - sz.y) / 2);
    draw(dc, cp, px, colour, topLeft);
}

wxBitmap bitmap(wxWindow *dpiRef, uint32_t cp, int px, const wxColour &colour)
{
    ensureRegistered();

    double scale = (dpiRef && dpiRef->GetDPIScaleFactor() > 0.0) ? dpiRef->GetDPIScaleFactor() : 1.0;

    const int dev_px = std::max(1, static_cast<int>(std::lround(px * scale)));
    wxBitmap  bmp(glyph_image(cp, dev_px, colour), 32);
#if wxCHECK_VERSION(3, 1, 6)
    bmp.SetScaleFactor(scale); // lay out at logical px on HiDPI
#endif
    return bmp;
}

} // namespace MaterialIcon
