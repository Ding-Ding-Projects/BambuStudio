#ifndef slic3r_GUI_MD3ColorPicker_hpp_
#define slic3r_GUI_MD3ColorPicker_hpp_

#include <wx/dialog.h>
#include <wx/colour.h>

class wxPanel;
class wxTextCtrl;
class Label;

// Material Design 3 colour picker: a continuous (infinite) picker styled with
// the MD3 tokens instead of the native common dialog.
//
//   * a saturation/value field for the current hue (every colour reachable);
//   * a continuous hue strip;
//   * a Material tonal ladder of the current pick (11 tones, 5..95) as
//     one-click quick picks — a fresh ladder for every hue, so the set of
//     ladders is as infinite as the hue wheel;
//   * a live preview chip and a #RRGGBB hex field, kept in two-way sync.
//
// ShowModal() returns wxID_OK with GetColour() holding the pick.
class MD3ColorPickerDialog : public wxDialog
{
public:
    MD3ColorPickerDialog(wxWindow *parent, const wxColour &initial);

    wxColour GetColour() const { return m_colour; }

private:
    void set_from_hsv(double h, double s, double v, bool update_hex = true);
    void set_colour(const wxColour &c, bool update_hex = true);
    void rebuild_bitmaps();
    void sync_hex();

    wxColour m_colour;
    double   m_h { 140.0 }, m_s { 0.8 }, m_v { 0.45 };

    wxPanel    *m_sv_field { nullptr };
    wxPanel    *m_hue_strip { nullptr };
    wxPanel    *m_tone_row { nullptr };
    wxPanel    *m_preview { nullptr };
    wxTextCtrl *m_hex { nullptr };
    Label      *m_tone_caption { nullptr };
    Label      *m_translate { nullptr };
};

#endif // slic3r_GUI_MD3ColorPicker_hpp_
