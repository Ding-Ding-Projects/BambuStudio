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

        const bool added = wxFont::AddPrivateFont(font_path);
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

wxSize measure(wxDC &dc, uint32_t cp, int px)
{
    const wxFont saved = dc.GetFont();
    dc.SetFont(font(px));
    const wxSize sz = dc.GetTextExtent(text(cp));
    dc.SetFont(saved);
    return sz;
}

void draw(wxDC &dc, uint32_t cp, int px, const wxColour &colour, const wxPoint &topLeft)
{
    const wxFont   savedFont = dc.GetFont();
    const wxColour savedFg   = dc.GetTextForeground();
    dc.SetFont(font(px));
    dc.SetTextForeground(colour);
    dc.DrawText(text(cp), topLeft);
    dc.SetFont(savedFont);
    dc.SetTextForeground(savedFg);
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

    const wxFont f = font(px);

    // Logical extent, measured through a scratch memory DC at base (96) DPI.
    wxSize logical;
    {
        wxBitmap   probe(1, 1);
        wxMemoryDC mdc(probe);
        mdc.SetFont(f);
        logical = mdc.GetTextExtent(text(cp));
        mdc.SelectObject(wxNullBitmap);
    }
    if (logical.x < 1)
        logical.x = std::max(1, px);
    if (logical.y < 1)
        logical.y = std::max(1, px);

    const int dev_w = std::max(1, static_cast<int>(std::ceil(logical.x * scale)));
    const int dev_h = std::max(1, static_cast<int>(std::ceil(logical.y * scale)));

    // Transparent 32-bpp canvas, then antialiased glyph via wxGraphicsContext
    // (matches the AA / alpha of the existing ScalableBitmap SVG icons). Mirrors
    // the transparent-chip idiom used elsewhere in the GUI.
    wxBitmap bmp(dev_w, dev_h);
#if defined(__WXMSW__) || defined(__WXOSX__)
    bmp.UseAlpha();
#endif
    {
        wxMemoryDC dc(bmp);
        dc.SetBackground(*wxTRANSPARENT_BRUSH);
        dc.Clear();

        wxGraphicsContext *gc = wxGraphicsContext::Create(dc);
        if (gc) {
            gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
            gc->Scale(scale, scale);
            gc->SetFont(f, colour);
            gc->DrawText(text(cp), 0, 0);
            delete gc; // flush the graphics context before the bitmap is read
        }
        dc.SelectObject(wxNullBitmap);
    }

#if wxCHECK_VERSION(3, 1, 6)
    bmp.SetScaleFactor(scale); // lay out at logical px on HiDPI
#endif
    return bmp;
}

} // namespace MaterialIcon
