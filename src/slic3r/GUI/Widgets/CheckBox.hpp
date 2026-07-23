#ifndef slic3r_GUI_CheckBox_hpp_
#define slic3r_GUI_CheckBox_hpp_

#include "../wxExtensions.hpp"
#include "MD3Tokens.hpp"

#include <wx/tglbtn.h>

// MD3 checkbox. The nine baked 18px PNGs (check_on/half/off x normal/disabled/
// focused) are gone: the glyph is drawn live at 20px through a wxGraphicsContext
// so it recolors with the theme and the active ColorScheme (Preview-purple /
// Device-teal). Unchecked = a rounded square outline (OnSurfaceVariant); checked
// = a filled square (Primary) + the Material Symbols Check glyph (OnPrimary);
// indeterminate = the filled square + a horizontal bar.
class CheckBox : public wxBitmapToggleButton
{
public:
	CheckBox(wxWindow * parent, int id = wxID_ANY);

public:
	void SetValue(bool value) override;

	void SetHalfChecked(bool value = true);

	// Recolor the checked/indeterminate fill to a workspace accent (Preview /
	// Device). Neutral roles (the unchecked outline) never carry a scheme.
	void SetColorScheme(MD3::ColorScheme scheme);

	void Rescale();

	// Draw a single checkbox glyph state to a DPI-correct, antialiased,
	// transparent bitmap at logical size `px` (device size = px * scale), so any
	// custom-painted row can reuse the exact CheckBox anatomy (unchecked =
	// OnSurfaceVariant rounded-square outline; checked/indeterminate = a filled
	// Primary/scheme square + Check glyph or bar) instead of hand-painting raster
	// checkbox bitmaps. Used internally by renderBitmap() and by e.g.
	// MultiMachinePage's DevicePickItem list-row checkbox.
	static wxBitmap RenderGlyphBitmap(int px, double scale, bool checked, bool half, bool disabled,
	                                   MD3::ColorScheme scheme = MD3::ColorScheme::Brand);

#ifdef __WXOSX__
    virtual bool Enable(bool enable = true) wxOVERRIDE;
#endif

protected:
#ifdef __WXMSW__
    virtual State GetNormalState() const wxOVERRIDE;
#endif

#ifdef __WXOSX__
    virtual wxBitmap DoGetBitmap(State which) const wxOVERRIDE;

    void updateBitmap(wxEvent & evt);

    bool m_disable = false;
    bool m_hover = false;
    bool m_focus = false;
#endif

private:
	void update();

	// Draw a single state to a DPI-correct, antialiased, transparent bitmap: the
	// 20px glyph centered inside the 44px hit-target canvas, with an optional
	// distinct keyboard-focus ring (used only for the MSW SetBitmapFocus bitmap).
	wxBitmap renderBitmap(bool checked, bool half, bool disabled, bool focus = false) const;

	// Device-pixel side of the 44px logical hit-target window at the current DPI.
	// Kept in sync with renderBitmap() so the button reserves exactly the drawn
	// (hit-target) size; the visible glyph stays 20px, centered inside it.
	int deviceSide() const;

private:
    MD3::ColorScheme m_scheme = MD3::ColorScheme::Brand;
    bool m_half_checked = false;
};

#endif // !slic3r_GUI_CheckBox_hpp_
