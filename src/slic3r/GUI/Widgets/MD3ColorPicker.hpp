#ifndef slic3r_GUI_MD3ColorPicker_hpp_
#define slic3r_GUI_MD3ColorPicker_hpp_

#include <wx/dialog.h>
#include <wx/colour.h>

#include <vector>

class wxPanel;
class wxTextCtrl;
class Label;
class Slider;
class Button;

// Material Design 3 colour picker: a continuous (infinite) picker styled with
// the MD3 tokens instead of the native common dialog.
//
//   * a saturation/value field for the current hue (every colour reachable);
//   * a continuous hue strip;
//   * a Material tonal ladder of the current pick (11 tones, 5..95) as
//     one-click quick picks — a fresh ladder for every hue, so the set of
//     ladders is as infinite as the hue wheel;
//   * an alpha slider (0..100 %), carried in GetColour().Alpha();
//   * a live preview chip and a #RRGGBB hex field, kept in two-way sync;
//   * a Translations column: the same colour written as a CSS name (when it
//     has one), HEX, HEX8, RGB, RGBA, HSL, HSLA, HSV, HWB, CIE XYZ (D65),
//     CIELAB, LCH, OKLab, OKLCH and naive CMYK, each in a read-only field
//     with its own Copy button;
//   * an "Enter any format" field that parses any of those notations (plus
//     CSS colour names) and drives the picker, with an inline non-blocking
//     status line naming the active colour space and warning before an
//     out-of-sRGB value is shown clipped;
//   * a WCAG contrast readout of the pick against a foreground/background
//     pair the caller passes in (default: the MD3 OnSurface / Surface roles).
//
// ShowModal() returns wxID_OK with GetColour() holding the pick.
class MD3ColorPickerDialog : public wxDialog
{
public:
    // The pair the contrast readout is measured against. `foreground` is the
    // text that would sit on the picked colour; `background` is the surface
    // the picked colour would be drawn on. Either may be wxNullColour to
    // skip that half of the readout.
    struct ContrastContext {
        wxColour foreground;
        wxColour background;
    };

    // Default context: MD3 OnSurface text over the MD3 Surface role.
    static ContrastContext defaultContrastContext();

    MD3ColorPickerDialog(wxWindow *parent, const wxColour &initial);
    MD3ColorPickerDialog(wxWindow *parent, const wxColour &initial, const ContrastContext &contrast);

    // The pick, alpha included (255 when the slider was left at 100 %).
    wxColour GetColour() const { return m_colour; }

private:
    enum class Source { Picker, HexField, AnyFormatField };

    void build(wxWindow *parent, const wxColour &initial);
    void set_from_hsv(double h, double s, double v, Source source = Source::Picker);
    void set_colour(const wxColour &c, Source source = Source::Picker);
    void set_alpha(int percent, Source source = Source::Picker);
    void refresh_all(Source source);
    void refresh_translations();
    void refresh_contrast();
    void set_status(const wxString &text, bool warning);
    void on_any_format_edited();
    void copy_to_clipboard(const wxString &text);
    void rebuild_bitmaps();
    void sync_hex();

    wxColour m_colour;
    double   m_h { 140.0 }, m_s { 0.8 }, m_v { 0.45 };
    int      m_alpha_percent { 100 };
    ContrastContext m_contrast;

    wxPanel    *m_sv_field { nullptr };
    wxPanel    *m_hue_strip { nullptr };
    wxPanel    *m_tone_row { nullptr };
    wxPanel    *m_preview { nullptr };
    wxTextCtrl *m_hex { nullptr };
    Label      *m_tone_caption { nullptr };
    Slider     *m_alpha { nullptr };
    Label      *m_alpha_value { nullptr };
    wxTextCtrl *m_any_format { nullptr };
    Label      *m_status { nullptr };
    Label      *m_gamut { nullptr };
    Label      *m_contrast_line { nullptr };

    struct TranslationRow {
        const char *space;   // "HEX", "OKLCH", ... (identifier, not translated)
        Label      *caption;
        wxTextCtrl *value;
        Button     *copy;
    };
    std::vector<TranslationRow> m_rows;
    bool m_syncing { false };
};

#endif // slic3r_GUI_MD3ColorPicker_hpp_
