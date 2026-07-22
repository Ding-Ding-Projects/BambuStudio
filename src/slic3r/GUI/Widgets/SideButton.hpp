#ifndef slic3r_GUI_SideButton_hpp_
#define slic3r_GUI_SideButton_hpp_

#include <cstdint>

#include <wx/stattext.h>
#include <wx/vlbox.h>
#include <wx/combo.h>
#include "../wxExtensions.hpp"
#include "StateHandler.hpp"


class SideButton : public wxWindow
{
public:

    enum EHorizontalOrientation : unsigned char
    {
        HO_Left,
        HO_Center,
        HO_Right,
        Num_Horizontal_Orientations
    };

    SideButton(wxWindow* parent, wxString text, wxString icon = "", long style = 0, int iconSize = 0);

    void SetCornerRadius(double radius);

    //BBS set enable array
    void SetCornerEnable(const std::vector<bool>& enable);

    void SetTextLayout(EHorizontalOrientation orient, int margin = 15);

    void SetLayoutStyle(int style);

    void SetLabel(const wxString& label);

    bool SetForegroundColour(wxColour const & colour) override;

    bool SetBackgroundColour(wxColour const & color) override;

    bool SetBottomColour(wxColour const &color);

    void SetMinSize(const wxSize& size) override;
    
    void SetBorderColor(StateColor const & color);

    void SetForegroundColor(StateColor const &color);

    void SetBackgroundColor(StateColor const &color);

    bool Enable(bool enable = true);

    void Rescale();

    void SetExtraSize(const wxSize& size);

    void SetIconOffset(const int offset);

    // Optional leading Material Symbols glyph, drawn before the label through the
    // shared MaterialIcon helper. Additive: the existing raster-icon and
    // text-only paths are unchanged, and the glyph is suppressed (a graceful
    // fallback to the label alone) whenever MaterialIcon::available() is false.
    // px<=0 derives a default size; codepoint 0 clears the glyph.
    void SetLeadingGlyph(uint32_t codepoint, int px = 0);

private:
    wxSize textSize;
    wxSize minSize;
    ScalableBitmap icon;
    double radius;
    wxSize extra_size;
    int icon_offset;
    uint32_t leading_glyph_cp = 0;
    int      leading_glyph_px = 0;
    std::vector<bool> radius_enable;

    StateHandler    state_handler;
    StateColor      text_color;
    StateColor      border_color;
    StateColor      background_color;
    wxColour        bottom_color;

    bool pressedDown = false;
    int  layout_style = 0;

    EHorizontalOrientation text_orientation;
    int text_margin;


    void paintEvent(wxPaintEvent& evt);

    void dorender(wxDC& dc, wxDC& text_dc);

    // Design-px size of the leading glyph (explicit SetLeadingGlyph px, else a
    // default that balances the Body_14 label).
    int leadingGlyphPx() const;

    void messureSize();

    void mouseDown(wxMouseEvent& event);
    void mouseReleased(wxMouseEvent& event);

    void sendButtonEvent();

	DECLARE_EVENT_TABLE()
};
#endif // !slic3r_GUI_Button_hpp_
