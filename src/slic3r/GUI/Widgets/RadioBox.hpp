#ifndef slic3r_GUI_RADIOBOX_hpp_
#define slic3r_GUI_RADIOBOX_hpp_

#include "../wxExtensions.hpp"
#include "MD3Tokens.hpp"

#include <wx/tglbtn.h>

namespace Slic3r {
namespace GUI {

// MD3 radio button. The baked radio_on/off/ban 18px PNGs are gone: the glyph is
// drawn live from the Material Symbols RadioButtonChecked / RadioButtonUnchecked
// codepoints so it recolors with the theme and the active ColorScheme. Selected
// = Primary, unselected = OnSurfaceVariant, disabled = dimmed OnSurfaceVariant
// (the ban asset is dropped).
class RadioBox : public wxBitmapToggleButton
{
public:
    RadioBox(wxWindow *parent);

public:
    void SetValue(bool value) override;
	bool GetValue();

	// Recolor the selected dot to a workspace accent (Preview / Device).
	void SetColorScheme(MD3::ColorScheme scheme);

    void Rescale();
    bool Disable() {
        bool result = wxBitmapToggleButton::Disable();
        update();
        return result;
    }
    bool Enable() {
        bool result = wxBitmapToggleButton::Enable();
        update();
        return result;
    }

private:
    void update();

	// Draw the current state to a DPI-correct, antialiased, transparent bitmap.
	wxBitmap renderBitmap(bool selected, bool disabled) const;

	int deviceSide() const;

private:
    MD3::ColorScheme m_scheme = MD3::ColorScheme::Brand;
};

}}



#endif // !slic3r_GUI_CheckBox_hpp_
