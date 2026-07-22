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
    // --- Workspace tabs, title-bar chips, nav ---
    ViewInAr                = 0xE9FE,
    Layers                  = 0xE53B,
    Cast                    = 0xE307,
    Devices                 = 0xE1B1,
    FolderOpen              = 0xE2C8,
    Build                   = 0xE869,
    Palette                 = 0xE3B7,
    Print                   = 0xE555,
    Tune                    = 0xE429,
    AccountTree             = 0xE97A,
    DeployedCode            = 0xF720,
    GridView                = 0xE9B0,
    Sync                    = 0xE627,
    History                 = 0xE28E,
    Send                    = 0xE163,
    // --- Title-bar action + window controls ---
    Save                    = 0xE161,
    Undo                    = 0xE166,
    Redo                    = 0xE15A,
    Publish                 = 0xE255,
    Minimize                = 0xE931,
    CropSquare              = 0xE3C1,
    FilterNone              = 0xE3E0,
    // --- Fields / selection / dialog + toolbar actions ---
    CheckBox                = 0xE834,
    CheckBoxOutlineBlank    = 0xE835,
    Edit                    = 0xE150,
    Delete                  = 0xE872,
    ContentCopy             = 0xE14D,
    ContentCut              = 0xE14E,
    Lock                    = 0xE88D,
    LockOpen                = 0xE898,
    Star                    = 0xE838,
    Done                    = 0xE876,
    TaskAlt                 = 0xE2E6,
    SwapHoriz               = 0xE8D4,
    Map                     = 0xE55B,
    Info                    = 0xE88E,
    Help                    = 0xE887,
    Warning                 = 0xE002,
    Error                   = 0xE000,
    ArrowBack               = 0xE5C4,
    ArrowForward            = 0xE5C8,
    OpenInNew               = 0xE895,
    DryCleaning             = 0xEA58,
    // --- Object outliner / tree ---
    Hardware                = 0xEA59,
    Grain                   = 0xE3EA,
    VerticalAlignBottom     = 0xE258,
    Sort                    = 0xE164,
    FiberManualRecord       = 0xE061,
    // --- Device / monitor control glyphs ---
    Lightbulb               = 0xE0F0,
    ModeFan                 = 0xF168,
    Speed                   = 0xE9E4,
    PhotoCamera             = 0xE3B0,
    Schedule                = 0xE192,
    Payments                = 0xEF63,
    Lan                     = 0xEB2F,
    Scale                   = 0xEB5F,
    Thermostat              = 0xF076,
    Inventory2              = 0xE1A1,
    ControlCamera           = 0xE074,
    ModeHeat                = 0xF16A,
    HomeWork                = 0xEA09,
    Circle                  = 0xEF4A,
    // --- Connectivity / signal strength ---
    SignalWifi4Bar          = 0xE1D8,
    NetworkWifi3Bar         = 0xEBE1,
    NetworkWifi2Bar         = 0xEBD6,
    SignalWifiStatusbarNull = 0xF067,
    // --- Preview overlay (ImGui atlas consumers) ---
    SkipPrevious            = 0xE045,
    SkipNext                = 0xE044,
    Route                   = 0xEACD,
    LineStartCircle         = 0xF816,
    UTurnLeft               = 0xEBA1,
    WaterDrop               = 0xE798,
    Insights                = 0xF092,
    Stack                   = 0xF609,
    // --- Gizmo rail + viewport toolbar ---
    OpenWith                = 0xE89F,
    RotateRight             = 0xE41A,
    OpenInFull              = 0xF1CE,
    AlignHorizontalLeft     = 0xE00D,
    Straighten              = 0xE41C,
    Transform               = 0xE428,
    CallSplit               = 0xE0B6,
    AutoAwesomeMosaic       = 0xE660,
    CenterFocusStrong       = 0xE3B4,
    FilterCenterFocus       = 0xE3DC,
    Carpenter               = 0xF1F8,
    Foundation              = 0xF200,
    Brush                   = 0xE3AE,
    MatchCase               = 0xF6F1,
    TextFields              = 0xE262,
    ShapeLine               = 0xF8D3,
    JoinInner               = 0xEAF4,
    Padding                 = 0xE9C8,
    Compress                = 0xE94D,
    Timeline                = 0xE922,
    // --- Aliases for names absent from the vendored font (nearest verified glyph) ---
    Download                = 0xE171, // 'file_download' is not in this font; 'download' is the Material Symbols name
    SettingsBackupRestore   = 0xE8BA, // 'restore' is not in this font (use for undo-to-system-value / restore defaults)
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
