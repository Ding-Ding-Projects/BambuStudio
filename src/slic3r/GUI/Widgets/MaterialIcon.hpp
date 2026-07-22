#ifndef slic3r_GUI_MaterialIcon_hpp_
#define slic3r_GUI_MaterialIcon_hpp_

#include <cstdint>

#include <wx/bitmap.h>
#include <wx/colour.h>
#include <wx/dc.h>
#include <wx/font.h>
#include <wx/gdicmn.h> // wxPoint / wxRect / wxSize
#include <wx/string.h>

#include "MD3Tokens.hpp"

class wxWindow;

// Material Symbols Outlined icon-font helper.
//
// The MD3 design kit renders every UI glyph from the "Material Symbols
// Outlined" variable font (see MD3::Type::font_icon). This helper is the single
// place that (a) registers the bundled TTF as a private font and (b) turns a
// glyph codepoint into drawable text / a wxFont / a DPI-correct bitmap.
//
// Codepoints, NOT ligatures. The app paints with wxDC::DrawText, which on MSW
// goes through GDI ExtTextOut; GDI does not apply OpenType GSUB ligature
// substitution, so drawing the ligature string ("videocam") would render literal
// letters or tofu. A single PUA codepoint maps straight through the font cmap to
// the glyph on every wx backend (GDI, GDI+/Direct2D, Core Text, Pango/Cairo).
// Every value below was verified against the shipped MaterialSymbolsOutlined.ttf
// cmap; because the exact TTF is vendored, the cmap is frozen and cannot drift
// across releases unless the font file is intentionally replaced (re-verify then).
//
// Only the font's default instance is renderable through wxFont/GDI (Outlined,
// wght 400, FILL 0, opsz 24); fvar axes cannot be selected. Express active /
// emphasis state through colour, never the FILL axis.
namespace MaterialIcon {

// Private-Use-Area codepoints (Unicode scalar values), verified against the
// bundled font cmap. Keep as uint32_t scalar values, not hand-encoded UTF-8.
enum Glyph : uint32_t {
    Videocam             = 0xE04B,
    Fullscreen           = 0xE5D0,
    FullscreenExit       = 0xE5D1,
    Settings             = 0xE8B8,
    SdCard               = 0xE623,
    Timelapse            = 0xE422,
    RadioButtonChecked   = 0xE837,
    RadioButtonUnchecked = 0xE836,
    Home                 = 0xE88A,
    ArrowUp              = 0xE316,
    ArrowDown            = 0xE313,
    ArrowLeft            = 0xE314,
    ArrowRight           = 0xE315,
    Close                = 0xE5CD,
    Check                = 0xE5CA,
    Add                  = 0xE145,
    Remove               = 0xE15B,
    Search               = 0xE8B6,
    MoreVert             = 0xE5D4,
    MoreHoriz            = 0xE5D3,
    ExpandMore           = 0xE5CF,
    ExpandLess           = 0xE5CE,
    ChevronLeft          = 0xE5CB,
    ChevronRight         = 0xE5CC,
    PlayArrow            = 0xE037,
    Pause                = 0xE034,
    Stop                 = 0xE047,
    Refresh              = 0xE5D5,
};

// True when the Material Symbols face is registered and resolvable, so callers
// can degrade gracefully (fall back to their SVG/bitmap) when the TTF is
// missing. Triggers the one-time private-font registration on first use.
bool available();

// The single glyph for a Unicode scalar codepoint, as a wxString. Encoding is
// centralized here so call sites stay readable (MaterialIcon::text(Home)).
wxString text(uint32_t cp);

// A wxFont for the Material Symbols Outlined face sized at a logical px value.
// The design-px -> wx point-size conversion mirrors Label::sysFont so icon sizes
// stay consistent with the type scale. Self-registers the private font (safety
// net for the initSysFont(load_font_resource=false) paths).
wxFont font(int px);

// Text extent of a glyph at the given logical px, measured through dc.
wxSize measure(wxDC &dc, uint32_t cp, int px);

// Draw a glyph with its top-left at topLeft, in colour. Saves/restores the DC
// font and text foreground.
void draw(wxDC &dc, uint32_t cp, int px, const wxColour &colour, const wxPoint &topLeft);

// Draw a glyph centered within rect.
void drawCentered(wxDC &dc, uint32_t cp, int px, const wxColour &colour, const wxRect &rect);

// Render a glyph to a transparent, antialiased, DPI-correct wxBitmap. dpiRef
// supplies the content-scale factor (may be null -> 1.0). The returned bitmap
// carries its scale factor so it lays out at logical px on HiDPI.
wxBitmap bitmap(wxWindow *dpiRef, uint32_t cp, int px, const wxColour &colour);

} // namespace MaterialIcon

#endif // !slic3r_GUI_MaterialIcon_hpp_
